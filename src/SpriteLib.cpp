#include "SpriteLib.h"

#include "Defer.h"

#include <type_traits>

namespace
{
BoundedVector<FilePath>
CollectPaths(const std::span<const SpriteSheetDef> defs)
{
    BoundedVector<FilePath> paths(defs.size());
    for(const SpriteSheetDef& sheet : defs)
    {
        paths.push_back(sheet.Path);
    }
    return paths;
}

size_t
CountSprites(const std::span<const SpriteSheetDef> defs)
{
    size_t count = 0;
    for(const SpriteSheetDef& sheet : defs)
    {
        count += sheet.Sprites.size();
    }
    return count;
}

size_t
CountSpriteRects(const std::span<const SpriteSheetDef> defs)
{
    size_t count = 0;
    for(const SpriteSheetDef& sheet : defs)
    {
        for(const SpriteDef& sprite : sheet.Sprites)
        {
            std::visit(
                [&](const auto& frame)
                {
                    using T = std::remove_cvref_t<decltype(frame)>;

                    if constexpr(std::is_same_v<T, SpriteFrameListDef>)
                    {
                        count += frame.Rects.size();
                    }
                    else if constexpr(std::is_same_v<T, SpriteFrameDef>)
                    {
                        count += frame.Count;
                    }
                    else
                    {
                        static_assert(false, "Unknown frame type");
                    }
                },
                sprite.Frames);
        }
    }
    return count;
}

Result<BoundedVector<SDL_FRect>>
CollectRects(const std::span<const SpriteSheetDef> defs)
{
    const size_t rectCount = CountSpriteRects(defs);
    BoundedVector<SDL_FRect> rects(rectCount);

    for(const SpriteSheetDef& sheet : defs)
    {
        for(const SpriteDef& sprite : sheet.Sprites)
        {
            const Result<> result = std::visit(
                [&](const auto& frame) -> Result<>
                {
                    using T = std::remove_cvref_t<decltype(frame)>;

                    if constexpr(std::is_same_v<T, SpriteFrameListDef>)
                    {
                        for(const RectDef& rect : frame.Rects)
                        {
                            rects.push_back({
                                .x = static_cast<float>(rect.X),
                                .y = static_cast<float>(rect.Y),
                                .w = static_cast<float>(rect.Width),
                                .h = static_cast<float>(rect.Height),
                            });
                        }
                    }
                    else if constexpr(std::is_same_v<T, SpriteFrameDef>)
                    {
                        GG_CHECKB(frame.Count > 0, "Frame count must be greater than 0");
                        const uint32_t frameCount = frame.Count;

                        const float frameWidth =
                            static_cast<float>(frame.Rect.Width) / static_cast<float>(frameCount);
                        const float frameHeight = static_cast<float>(frame.Rect.Height);

                        GG_CHECKB(frameWidth >= 1 && frameHeight >= 1,
                            "Frame dimensions must be at least 1");

                        float frameX = static_cast<float>(frame.Rect.X);
                        const float frameY = static_cast<float>(frame.Rect.Y);
                        for(uint32_t i = 0; i < frameCount; ++i, frameX += frameWidth)
                        {
                            rects.push_back({
                                .x = frameX,
                                .y = frameY,
                                .w = frameWidth,
                                .h = frameHeight,
                            });
                        }
                    }
                    else
                    {
                        static_assert(false, "Unknown frame type");
                    }
                    return Result<>::Success();
                },
                sprite.Frames);

            GG_CHECK(result, "Failed to collect frames for sprite: {}", sprite.Name);
        }
    }
    return rects;
}

BoundedVector<SpriteLibLoader::SpriteEntry>
CollectSpriteEntries(const std::span<const SpriteSheetDef> defs)
{
    const size_t spriteCount = CountSprites(defs);
    BoundedVector<SpriteLibLoader::SpriteEntry> entries(spriteCount);

    uint32_t sheetIndex = 0;
    uint32_t frameIndex = 0;

    for(const SpriteSheetDef& sheet : defs)
    {
        for(const SpriteDef& sprite : sheet.Sprites)
        {
            std::visit(
                [&](const auto& frame)
                {
                    using T = std::remove_cvref_t<decltype(frame)>;

                    if constexpr(std::is_same_v<T, SpriteFrameListDef>)
                    {
                        entries.push_back({ .SheetIndex = sheetIndex,
                            .FrameIndex = frameIndex,
                            .FrameCount = static_cast<uint32_t>(frame.Rects.size()) });

                        frameIndex += frame.Rects.size();
                    }
                    else if constexpr(std::is_same_v<T, SpriteFrameDef>)
                    {
                        entries.push_back({ .SheetIndex = sheetIndex,
                            .FrameIndex = frameIndex,
                            .FrameCount = frame.Count });

                        frameIndex += frame.Count;
                    }
                    else
                    {
                        static_assert(false, "Unknown frame type");
                    }
                },
                sprite.Frames);
        }

        ++sheetIndex;
    }
    return entries;
}

BoundedVector<Sprite::NameType>
CollectSpriteNames(const std::span<const SpriteSheetDef> defs)
{
    const size_t spriteCount = CountSprites(defs);
    BoundedVector<Sprite::NameType> names(spriteCount);

    for(const SpriteSheetDef& sheet : defs)
    {
        for(const SpriteDef& sprite : sheet.Sprites)
        {
            names.push_back(sprite.Name);
        }
    }
    return names;
}
} // namespace

SpriteLib::SpriteLib(BoundedVector<TexturePtr>&& textures,
    BoundedVector<SDL_FRect>&& spriteRects,
    BoundedVector<Sprite::NameType>&& spriteNames,
    BoundedVector<Sprite>&& sprites)
    : m_Textures(std::move(textures)),
      m_SpriteRects(std::move(spriteRects)),
      m_SpriteNames(std::move(spriteNames)),
      m_Sprites(std::move(sprites))
{
    GG_ASSERT(m_Sprites.size() == m_SpriteNames.size(),
        "Mismatch between sprites and sprite names");
}

Result<const Sprite*>
SpriteLib::GetSprite(const std::string_view name) const
{
    return GetSprite(Sprite::NameType{ name });
}

Result<const Sprite*>
SpriteLib::GetSprite(const Sprite::NameType& name) const
{
    for(size_t i = 0; i < m_SpriteNames.size(); ++i)
    {
        const Sprite::NameType* spriteName = m_SpriteNames.TryGet(i);
        if(spriteName && name == *spriteName)
        {
            return m_Sprites.TryGet(i);
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
    auto rectResult = CollectRects(defs);
    GG_CHECK(rectResult, "Failed to collect sprite rects");

    BoundedVector<SpriteEntry> entries = CollectSpriteEntries(defs);

    BoundedVector<Sprite::NameType> names = CollectSpriteNames(defs);

    const BoundedVector<FilePath> paths = CollectPaths(defs);

    BoundedVector<TextureLoader> textureLoaders(paths.size());

    for(const FilePath& path : paths)
    {
        auto texLoadResult = TextureLoader::Load(path, fileFetcher, renderer);
        GG_CHECK(texLoadResult, "Failed to load texture: {}", path);

        textureLoaders.emplace_back(std::move(*texLoadResult));
    }

    return SpriteLibLoader(defs.size(),
        std::move(*rectResult),
        std::move(entries),
        std::move(names),
        std::move(textureLoaders));
}

SpriteLibLoader::SpriteLibLoader(const size_t sheetCount,
    BoundedVector<SDL_FRect>&& spriteRects,
    BoundedVector<SpriteEntry>&& spriteEntries,
    BoundedVector<Sprite::NameType>&& spriteNames,
    BoundedVector<TextureLoader>&& textureLoaders)
    : m_TextureLoaders(std::move(textureLoaders)),
      m_SpriteRects(std::move(spriteRects)),
      m_SpriteEntries(std::move(spriteEntries)),
      m_Sprites(m_SpriteEntries.size()),
      m_Textures(sheetCount),
      m_SpriteNames(std::move(spriteNames)),
      m_Status(Status::Loading)
{
}

void
SpriteLibLoader::Update()
{
    if(!GG_VERIFY(Status::None != m_Status, "SpriteLib has not been loaded"))
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

    const std::span<const SDL_FRect> spriteRects = m_SpriteRects;

    for(const SpriteEntry& entry : m_SpriteEntries)
    {
        const size_t spriteIndex = m_Sprites.size();

        if(!GG_VERIFY(spriteIndex < m_SpriteNames.size(),
               "Mismatch between sprites and sprite names: {} >= {}",
               m_Sprites.size(),
               m_SpriteNames.size()))
        {
            return;
        }

        const Sprite::NameType& name = *m_SpriteNames.TryGet(spriteIndex);

        if(!GG_VERIFY(entry.SheetIndex < m_Textures.size(),
               "Invalid texture index in sprite entry: {} ({})",
               entry.SheetIndex,
               name))
        {
            return;
        }

        if(!GG_VERIFY(entry.FrameIndex + entry.FrameCount <= m_SpriteRects.size(),
               "Invalid frame span in sprite entry: {}-{} ({})",
               entry.FrameIndex,
               entry.FrameIndex + entry.FrameCount,
               name))
        {
            return;
        }

        const TexturePtr* texture = m_Textures.TryGet(entry.SheetIndex);
        if(!GG_VERIFY(texture, "Failed to get texture at index: {}", entry.SheetIndex))
        {
            return;
        }

        const std::span<const SDL_FRect> frames =
            spriteRects.subspan(entry.FrameIndex, entry.FrameCount);

        auto spriteResult = Sprite::Create(texture->get(), frames);
        if(!spriteResult)
        {
            GG_ERROR("Failed to create sprite: {}", name);
            return;
        }

        m_Sprites.push_back(std::move(*spriteResult));
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

    m_Status = Status::Taken;

    return SpriteLib(std::move(m_Textures),
        std::move(m_SpriteRects),
        std::move(m_SpriteNames),
        std::move(m_Sprites));
}