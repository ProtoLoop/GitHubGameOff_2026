#include "SpriteLib.h"

#include "Defer.h"
#include "FileFetcher.h"
#include "Shortcuts.h"
#include "WebGpu.h"

namespace
{

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
        const Rect& sheetRectDef = sheetDef.SheetRect;
        const Dimension2d& spriteDimsDef = sheetDef.SpriteDims;
        const SpriteSheet::NameType& name = sheetDef.Name;

        GG_CHECKB(!atlasPath.empty(), "Sprite sheet path cannot be empty");
        GG_CHECKB(spriteDimsDef.w > 0 && spriteDimsDef.h > 0, "Sprite dimensions cannot be empty");
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

        const SpriteSheet sheet(name, scast<uint32_t>(atlasIndex), sheetRectDef, spriteDimsDef);

        GG_CHECKB(sheets.push_back(sheet));
    }
    return sheets;
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

    auto texLoaderResult = TextureLoader::Load(paths, fileFetcher, webGpu, "SpriteLib");
    GG_CHECK(texLoaderResult, "Failed to load textures");

    return SpriteLibLoader(std::move(*sheetResult), std::move(*texLoaderResult));
}

SpriteLibLoader::SpriteLibLoader(BoundedVector<SpriteSheet>&& spriteSheets,
    TextureLoader&& textureLoader)
    : m_TextureLoader(std::move(textureLoader)),
      m_SpriteSheets(std::move(spriteSheets)),
      m_Status(Status::Loading)
{
}

Result<>
SpriteLibLoader::Update()
{
    switch(m_Status)
    {
        case Status::None:
            GG_CHECKB(false, "SpriteLib has not been loaded");
            break;
        case Status::Loading:
            if(!m_TextureLoader.IsPending())
            {
                m_Status = Status::Failed;

                auto cleanup = GG_DEFER
                {
                    // Free resources.
                    auto byeLoades = std::move(m_TextureLoader);
                };

                auto texResult = m_TextureLoader.Take();
                GG_CHECKB(texResult, "Failed to create texture array");

                GG_CHECK(FixupSpriteSheets(*texResult), "Failed to validate sprite sheets");

                m_Texture = std::move(*texResult);

                m_Status = Status::Loaded;
            }
            break;
        case Status::Loaded:
        case Status::Failed:
        case Status::Taken:
            break;
    }

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

    return SpriteLib(m_Texture, std::move(m_SpriteSheets));
}

// private:

Result<>
SpriteLibLoader::FixupSpriteSheets(const wgpu::Texture texture)
{
    const uint32_t texWidth = texture.GetWidth();
    const uint32_t texHeight = texture.GetHeight();
    const uint32_t layerCount = texture.GetDepthOrArrayLayers();

    // Validate sprite sheets
    for(SpriteSheet& sheet : m_SpriteSheets)
    {
        const SpriteSheet::NameType& name = sheet.GetName();
        const Rect& sheetRect = sheet.m_SheetRect;

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
            const Rect newRect{ .x = 0, .y = 0, .w = texWidth, .h = texHeight };

            // Call the ctor again so rows/cols are recalculated based on the new sheet rect.
            sheet = SpriteSheet(sheet.GetName(), sheet.m_AtlasIndex, newRect, sheet.m_SpriteDims);
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
            texWidth,
            texHeight);

        // Verify the sprite dimensions are positive.
        GG_CHECKB(sheet.m_SpriteDims.w > 0 && sheet.m_SpriteDims.h > 0,
            "Invalid sprite dimensions in sheet: {} - {}x{}",
            name,
            sheet.m_SpriteDims.w,
            sheet.m_SpriteDims.h);

        // Verify the sprite dimensions fit within the sheet rect.
        GG_CHECKB(sheet.m_SpriteDims.w <= sheet.m_SheetRect.w
                && sheet.m_SpriteDims.h <= sheet.m_SheetRect.h,
            "Sprite dimensions exceed sheet rect: {} - {}x{} - {}x{}",
            name,
            sheet.m_SpriteDims.w,
            sheet.m_SpriteDims.h,
            sheet.m_SheetRect.w,
            sheet.m_SheetRect.h);
    }

    return Result<>::Success();
}