#include "SpriteLib.h"

#include "Defer.h"
#include "FileFetcher.h"
#include "Shortcuts.h"
#include "WebGpu.h"

#include <SDL3/SDL_render.h>
#include <stb_image.h>
#include <webgpu/webgpu_cpp.h>

namespace
{
constexpr int kNumTextureChannels = 4;

struct stbiDeleter
{
    void operator()(stbi_uc* ptr) const
    {
        if(ptr)
        {
            stbi_image_free(ptr);
        }
    }
};

using StbiImagePtr = std::unique_ptr<stbi_uc, stbiDeleter>;

struct Image
{
    Image() = delete;

    Image(StbiImagePtr data, const uint32_t width, const uint32_t height)
        : Data(std::move(data)),
          Width(width),
          Height(height)
    {
    }
    StbiImagePtr Data;
    uint32_t Width{ 0 };
    uint32_t Height{ 0 };
};

Result<BoundedVector<FilePath>>
CollectPaths(const std::span<const SpriteSheetDef> defs)
{
    BoundedVector<FilePath> paths(defs.size());
    for(const SpriteSheetDef& sheet : defs)
    {
        bool havePath = false;
        for(const FilePath& path : paths)
        {
            if(path == sheet.Path)
            {
                havePath = true;
                break;
            }
        }

        if(!havePath)
        {
            GG_CHECKB(paths.push_back(sheet.Path));
        }
    }
    return paths;
}

Result<BoundedVector<SpriteSheet>>
CollectSpriteSheets(const std::span<const SpriteSheetDef> defs,
    const std::span<const FilePath> atlasPaths)
{
    BoundedVector<SpriteSheet> sheets(defs.size());

    for(const SpriteSheetDef& sheetDef : defs)
    {
        const FilePath& atlasPath = sheetDef.Path;
        const RectDef& sheetRectDef = sheetDef.SheetRect;
        const Dimension2Def& spriteDimsDef = sheetDef.SpriteDims;
        const SpriteSheet::NameType& name = sheetDef.Name;

        GG_CHECKB(!atlasPath.empty(), "Sprite sheet path cannot be empty");
        GG_CHECKB(spriteDimsDef.Width > 0 && spriteDimsDef.Height > 0,
            "Sprite dimensions cannot be empty");
        GG_CHECKB(!name.empty(), "Sprite sheet name cannot be empty");

        for(const auto& sheet : sheets)
        {
            GG_CHECKB(sheet.GetName() != name, "Duplicate sprite sheet name: {}", name);
        }

        bool atlasFound = false;
        size_t atlasIndex = 0;
        for(const FilePath& path : atlasPaths)
        {
            if(path == atlasPath)
            {
                atlasFound = true;
                break;
            }

            ++atlasIndex;
        }

        GG_CHECK(atlasFound, "Atlas not found: {}", atlasPath);

        const SDL_FRect sheetRect //
            {
                .x = scast<float>(sheetRectDef.X),
                .y = scast<float>(sheetRectDef.Y),
                .w = scast<float>(sheetRectDef.Width),
                .h = scast<float>(sheetRectDef.Height),
            };

        const SpriteSheet sheet(name,
            scast<uint32_t>(atlasIndex),
            sheetRect,
            spriteDimsDef.Width,
            spriteDimsDef.Height);

        GG_CHECKB(sheets.push_back(sheet));
    }
    return sheets;
}

Result<Image>
LoadImageFromMemory(const FetchRequestPtr& rqstPtr)
{
    auto dataResult = rqstPtr->GetData();
    GG_CHECKB(dataResult, "Failed to get fetch data");
    GG_CHECKB(dataResult->size() <= std::numeric_limits<int>::max(),
        "Data size exceeds maximum allowed size");

    const void* p = dataResult->data();
    const stbi_uc* imageData = static_cast<const stbi_uc*>(p);
    const int numBytes = static_cast<int>(dataResult->size());

    int width = 0;
    int height = 0;
    stbi_uc* image =
        stbi_load_from_memory(imageData, numBytes, &width, &height, nullptr, kNumTextureChannels);

    GG_CHECKB(image, "Failed to load image: {}", stbi_failure_reason());

    return Image(StbiImagePtr(image), scast<uint32_t>(width), scast<uint32_t>(height));
}

Result<wgpu::Texture>
CreateTextureArray(const std::span<Image> images, const WebGpu& webGpu)
{
    uint32_t width = 0, height = 0;

    BoundedVector<std::span<const std::byte>> imageDataSpans(images.size());

    for(const Image& image : images)
    {
        GG_CHECKB(image.Width > 0 && image.Height > 0, "Invalid image dimensions");
        GG_CHECKB(width == 0 || width == image.Width, "Inconsistent image width");
        GG_CHECKB(height == 0 || height == image.Height, "Inconsistent image height");

        width = image.Width;
        height = image.Height;

        const std::span<const std::byte> imageSpan(recast<const std::byte*>(image.Data.get()),
            scast<size_t>(image.Width) * image.Height * kNumTextureChannels);

        GG_CHECKB(imageDataSpans.push_back(imageSpan));
    }

    return webGpu.CreateTextureArray(width, height, imageDataSpans, "Sprites");
}
} // namespace

SpriteLib::SpriteLib(wgpu::Texture texture, BoundedVector<SpriteSheet>&& spriteSheets)
    : m_Texture(std::move(texture)),
      m_SpriteSheets(std::move(spriteSheets))
{
}

Result<const SpriteSheet*>
SpriteLib::GetSpriteSheet(const std::string_view name) const
{
    for(const SpriteSheet& sheet : m_SpriteSheets)
    {
        if(name == sheet.GetName())
        {
            return &sheet;
        }
    }

    GG_ERROR("Sprite not found: {}", name);
    return Result<>::Fail();
}

/// SpriteLibLoader

Result<SpriteLibLoader>
SpriteLibLoader::Load(
    const std::span<const SpriteSheetDef> defs, FileFetcher& fileFetcher, const WebGpu& webGpu)
{
    auto pathResult = CollectPaths(defs);
    GG_CHECK(pathResult, "Failed to collect paths");
    const BoundedVector<FilePath> paths = std::move(*pathResult);

    auto sheetResult = CollectSpriteSheets(defs, paths);
    GG_CHECK(sheetResult, "Failed to collect sprite sheets");

    BoundedVector<FetchRequestPtr> fetchRequests(paths.size());

    for(const FilePath& path : paths)
    {
        auto fetchResult = fileFetcher.Fetch(path);
        GG_CHECK(fetchResult, "Failed to fetch file: {}", path);

        GG_CHECKB(fetchRequests.push_back(std::move(*fetchResult)));
    }

    return SpriteLibLoader(std::move(*sheetResult), std::move(fetchRequests), webGpu);
}

SpriteLibLoader::SpriteLibLoader(BoundedVector<SpriteSheet>&& spriteSheets,
    BoundedVector<FetchRequestPtr>&& textureFetchRequests,
    const WebGpu& webGpu)
    : m_TextureFetchRequests(std::move(textureFetchRequests)),
      m_SpriteSheets(std::move(spriteSheets)),
      m_WebGpu(&webGpu),
      m_Status(Status::Loading)
{
}

Result<>
SpriteLibLoader::Update()
{
    GG_CHECKB(Status::None != m_Status, "SpriteLib has not been loaded");

    if(m_Status != Status::Loading)
    {
        return Result<>::Success();
    }

    bool pending = false;
    for(const FetchRequestPtr& fetchRequest : m_TextureFetchRequests)
    {
        if(fetchRequest->IsPending())
        {
            pending = true;
            break;
        }
    }

    if(pending)
    {
        return Result<>::Success();
    }

    m_Status = Status::Failed;

    auto cleanup = GG_DEFER
    {
        // Free resources.
        auto bye = std::move(m_TextureFetchRequests);
    };

    auto bailOnError = GG_DEFER
    {
        // If we bail due to error then just free everything.
        auto bye = std::move(m_TextureArray);
    };

    const size_t textureCount = m_TextureFetchRequests.size();

    BoundedVector<Image> images(textureCount);

    for(const FetchRequestPtr& fetchRequest : m_TextureFetchRequests)
    {
        auto imageResult = LoadImageFromMemory(fetchRequest);
        GG_CHECKB(imageResult, "Failed to load image from memory");
        GG_CHECKB(images.push_back(std::move(*imageResult)));
    }

    auto texArrayResult = CreateTextureArray(images, *m_WebGpu);
    GG_CHECKB(texArrayResult, "Failed to create texture array");

    m_TextureArray = *texArrayResult;

    GG_CHECK(FixupSpriteSheets(), "Failed to validate sprite sheets");

    bailOnError.release();

    m_Status = Status::Loaded;

    return Result<>::Success();
}

bool
SpriteLibLoader::IsPending() const
{
    return m_Status == Status::Loading;
}

Result<SpriteLib>
SpriteLibLoader::Take()
{
    GG_CHECKB(Status::Failed != m_Status, "Sprite lib loading failed");
    GG_CHECKB(Status::Loading != m_Status, "Sprite lib is still loading");
    GG_CHECKB(Status::Taken != m_Status, "Sprite lib has already been taken");
    GG_CHECKB(Status::Loaded == m_Status, "Sprite lib did not load");

    m_Status = Status::Taken;

    return SpriteLib(m_TextureArray, std::move(m_SpriteSheets));
}

// private:

Result<>
SpriteLibLoader::FixupSpriteSheets()
{
    const float texWidth = scast<float>(m_TextureArray.GetWidth());
    const float texHeight = scast<float>(m_TextureArray.GetHeight());
    const uint32_t layerCount = m_TextureArray.GetDepthOrArrayLayers();

    // Validate sprite sheets
    for(SpriteSheet& sheet : m_SpriteSheets)
    {
        const SpriteSheet::NameType& name = sheet.GetName();
        const SDL_FRect& sheetRect = sheet.m_SheetRect;

        GG_CHECKB(!name.empty(), "Sprite sheet name cannot be empty");

        // Verify the atlas index
        GG_CHECKB(sheet.m_AtlasIndex < layerCount,
            "Invalid atlas index in sheet: {} - {}",
            name,
            sheet.m_AtlasIndex);

        // If the sheet rect is all zeros, it indicates that the full texture dimensions should be
        // used.
        GG_CHECKB((sheetRect.w == 0 && sheetRect.h == 0 && sheetRect.x == 0 && sheetRect.y == 0)
                || (sheetRect.w > 0 && sheetRect.h > 0),
            "Either the sheet rect x/y/w/h must be zero or w/h must be positive: {} - {}x{}x{}x{}",
            name,
            sheetRect.x,
            sheetRect.y,
            sheetRect.w,
            sheetRect.h);

        if(0 == sheetRect.w)
        {
            // Use the full texture dimensions for the sheet rect.
            const SDL_FRect newRect{ .x = 0, .y = 0, .w = texWidth, .h = texHeight };

            // Call the ctor again so rows/cols are recalculated based on the new sheet rect.
            sheet = SpriteSheet(sheet.GetName(),
                sheet.m_AtlasIndex,
                newRect,
                sheet.m_FrameWidth,
                sheet.m_FrameHeight);
        }

        // Verify the sheet rect is within the texture bounds.
        GG_CHECKB((sheetRect.x + sheetRect.w <= texWidth)
                && (sheetRect.y + sheetRect.h <= texHeight),
            "Sheet rect exceeds texture bounds: {} - {}x{}x{}x{} - {}x{}",
            name,
            sheetRect.x,
            sheetRect.y,
            sheetRect.w,
            sheetRect.h,
            scast<unsigned>(texWidth),
            scast<unsigned>(texHeight));

        // Verify the sprite dimensions are positive.
        GG_CHECKB(sheet.m_FrameWidth > 0 && sheet.m_FrameHeight > 0,
            "Invalid sprite dimensions in sheet: {} - {}x{}",
            name,
            sheet.m_FrameWidth,
            sheet.m_FrameHeight);

        // Verify the sprite dimensions fit within the sheet rect.
        GG_CHECKB(scast<float>(sheet.m_FrameWidth) <= sheet.m_SheetRect.w
                && scast<float>(sheet.m_FrameHeight) <= sheet.m_SheetRect.h,
            "Sprite dimensions exceed sheet rect: {} - {}x{} - {}x{}",
            name,
            sheet.m_FrameWidth,
            sheet.m_FrameHeight,
            sheet.m_SheetRect.w,
            sheet.m_SheetRect.h);
    }

    return Result<>::Success();
}