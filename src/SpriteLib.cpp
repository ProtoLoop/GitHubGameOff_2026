#include "SpriteLib.h"

#include "Defer.h"
#include "Shortcuts.h"

#include <SDL3/SDL_render.h>

namespace
{
BoundedVector<FilePath>
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
            paths.push_back(sheet.Path);
        }
    }
    return paths;
}

Result<BoundedVector<SpriteLibLoader::SpriteSheetInfo>>
CollectSpriteSheets(const std::span<const SpriteSheetDef> defs,
    const std::span<const FilePath> paths)
{
    BoundedVector<SpriteLibLoader::SpriteSheetInfo> sheets(defs.size());

    for(const SpriteSheetDef& sheet : defs)
    {
        const FilePath& atlasPath = sheet.Path;
        const RectDef& sheetRect = sheet.SheetRect;
        const Dimension2Def& spriteDims = sheet.SpriteDims;
        const SpriteSheet::NameType& name = sheet.Name;

        GG_CHECKB(!atlasPath.empty(), "Sprite sheet path cannot be empty");
        GG_CHECKB(spriteDims.Width > 0 && spriteDims.Height > 0,
            "Sprite dimensions cannot be empty");
        GG_CHECKB(!name.empty(), "Sprite sheet name cannot be empty");

        bool atlasFound = false;
        size_t atlasIndex = 0;
        for(const FilePath& path : paths)
        {
            if(path == atlasPath)
            {
                atlasFound = true;
                break;
            }

            ++atlasIndex;
        }

        GG_CHECK(atlasFound, "Atlas not found: {}", atlasPath);

        const SpriteLibLoader::SpriteSheetInfo spriteSheetInfo //
            {
                .Name = name,
                .AtlasIndex = scast<uint32_t>(atlasIndex),
                .SheetRect //
                {
                    .x = scast<float>(sheetRect.X),
                    .y = scast<float>(sheetRect.Y),
                    .w = scast<float>(sheetRect.Width),
                    .h = scast<float>(sheetRect.Height),
                },
                .SpriteWidth = spriteDims.Width,
                .SpriteHeight = spriteDims.Height,
            };

        sheets.push_back(spriteSheetInfo);
    }
    return sheets;
}
} // namespace

SpriteLib::SpriteLib(BoundedVector<TexturePtr>&& textures,
    BoundedVector<SpriteSheet>&& spriteSheets)
    : m_Textures(std::move(textures)),
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
    const std::span<const SpriteSheetDef> defs, FileFetcher& fileFetcher, SDL_Renderer* renderer)
{
    const BoundedVector<FilePath> paths = CollectPaths(defs);

    auto sheetResult = CollectSpriteSheets(defs, paths);
    GG_CHECK(sheetResult, "Failed to collect sprite sheets");

    BoundedVector<TextureLoader> textureLoaders(paths.size());

    for(const FilePath& path : paths)
    {
        auto texLoadResult = TextureLoader::Load(path, fileFetcher, renderer);
        GG_CHECK(texLoadResult, "Failed to load texture: {}", path);

        textureLoaders.emplace_back(std::move(*texLoadResult));
    }

    return SpriteLibLoader(std::move(*sheetResult), std::move(textureLoaders));
}

SpriteLibLoader::SpriteLibLoader(BoundedVector<SpriteLibLoader::SpriteSheetInfo>&& spriteSheets,
    BoundedVector<TextureLoader>&& textureLoaders)
    : m_TextureLoaders(std::move(textureLoaders)),
      m_SpriteSheetInfos(std::move(spriteSheets)),
      m_Textures(m_SpriteSheetInfos.size()),
      m_Status(Status::Loading)
{
}

void
SpriteLibLoader::Update()
{
    if(!GG_VERIFYB(Status::None != m_Status, "SpriteLib has not been loaded"))
    {
        return;
    }

    if(Status::Loading != m_Status)
    {
        GG_WARN("Update() called while not in loading stage");
        return;
    }

    bool pending = false;
    for(const TextureLoader& loader : m_TextureLoaders)
    {
        if(loader.IsPending())
        {
            pending = true;
            break;
        }
    }

    if(pending)
    {
        return;
    }

    m_Status = Status::Failed;

    auto cleanup = GG_DEFER
    {
        // Free resources.
        auto bye = std::move(m_TextureLoaders);
    };

    auto bailOnError = GG_DEFER
    {
        // If we bail due to error then just free everything.
        auto bye = std::move(m_Textures);
    };

    for(TextureLoader& loader : m_TextureLoaders)
    {
        auto texResult = loader.Take();
        if(!texResult)
        {
            GG_ERROR("Failed to take texture result from loader");
            return;
        }

        m_Textures.push_back(std::move(*texResult));
    }

    // Validate sprite sheets
    for(SpriteLibLoader::SpriteSheetInfo& sheetInfo : m_SpriteSheetInfos)
    {
        const SpriteSheet::NameType& name = sheetInfo.Name;
        const SDL_FRect& sheetRect = sheetInfo.SheetRect;

        if(!GG_VERIFYB(!name.empty(), "Sprite sheet name cannot be empty"))
        {
            return;
        }

        // Verify the atlas index
        if(!GG_VERIFYB(sheetInfo.AtlasIndex < m_Textures.size(),
               "Invalid atlas index in sheet: {} - {}",
               name,
               sheetInfo.AtlasIndex))
        {
            return;
        }

        const TexturePtr* texture = m_Textures.TryGet(sheetInfo.AtlasIndex);
        if(!GG_VERIFYB(texture, "Failed to get texture at index: {}", sheetInfo.AtlasIndex))
        {
            return;
        }

        // If the sheet rect is all zeros, it indicates that the full texture dimensions should be
        // used.
        if(!GG_VERIFYB(
               (sheetRect.w == 0 && sheetRect.h == 0 && sheetRect.x == 0 && sheetRect.y == 0)
                   || (sheetRect.w > 0 && sheetRect.h > 0),
               "Either the sheet rect x/y/w/h must be zero or w/h must be positive: {} - {}x{}x{}x{}",
               name,
               sheetRect.x,
               sheetRect.y,
               sheetRect.w,
               sheetRect.h))
        {
            return;
        }

        const float texWidth = scast<float>(texture->get()->w);
        const float texHeight = scast<float>(texture->get()->h);

        if(0 == sheetRect.w)
        {
            sheetInfo.SheetRect.w = scast<float>(texture->get()->w);
            sheetInfo.SheetRect.h = scast<float>(texture->get()->h);
        }

        // Verify the sheet rect is within the texture bounds.
        if(!GG_VERIFYB((sheetRect.x + sheetRect.w <= texWidth)
                   && (sheetRect.y + sheetRect.h <= texHeight),
               "Sheet rect exceeds texture bounds: {} - {}x{}x{}x{} - {}x{}",
               name,
               sheetRect.x,
               sheetRect.y,
               sheetRect.w,
               sheetRect.h,
               scast<unsigned>(texWidth),
               scast<unsigned>(texHeight)))
        {
            return;
        }

        // Verify the sprite dimensions are positive.
        if(!GG_VERIFYB(sheetInfo.SpriteWidth > 0 && sheetInfo.SpriteHeight > 0,
               "Invalid sprite dimensions in sheet: {} - {}x{}",
               name,
               sheetInfo.SpriteWidth,
               sheetInfo.SpriteHeight))
        {
            return;
        }

        if(!GG_VERIFYB(scast<float>(sheetInfo.SpriteWidth) <= sheetInfo.SheetRect.w
                   && scast<float>(sheetInfo.SpriteHeight) <= sheetInfo.SheetRect.h,
               "Sprite dimensions exceed sheet rect: {} - {}x{} - {}x{}",
               name,
               sheetInfo.SpriteWidth,
               sheetInfo.SpriteHeight,
               sheetInfo.SheetRect.w,
               sheetInfo.SheetRect.h))
        {
            return;
        }
    }

    bailOnError.release();

    m_Status = Status::Loaded;
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

    BoundedVector<SpriteSheet> spriteSheets(m_SpriteSheetInfos.size());

    for(const auto& sheetInfo : m_SpriteSheetInfos)
    {
        const TexturePtr* texturePtr = m_Textures.TryGet(sheetInfo.AtlasIndex);
        GG_CHECKB(texturePtr, "Failed to get texture for atlas index {}", sheetInfo.AtlasIndex);

        SpriteSheet spriteSheet(sheetInfo.Name,
            texturePtr->get(),
            sheetInfo.SheetRect,
            sheetInfo.SpriteWidth,
            sheetInfo.SpriteHeight);
        spriteSheets.push_back(std::move(spriteSheet));
    }

    m_Status = Status::Taken;

    return SpriteLib(std::move(m_Textures), std::move(spriteSheets));
}