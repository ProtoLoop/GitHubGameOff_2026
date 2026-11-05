#include "SpriteLib.h"

#include <type_traits>

namespace
{
BoundedVector<TexturePath>
CollectPaths(const std::span<const SpriteSheetDef> defs)
{
    BoundedVector<TexturePath> paths(defs.size());
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
            const Result<> result =
            std::visit(
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

                        GG_CHECKB(frameWidth >= 1 && frameHeight >= 1, "Frame dimensions must be at least 1");

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

BoundedVector<SpriteLib::SpriteEntry>
CollectSpriteEntries(const std::span<const SpriteSheetDef> defs)
{
    const size_t spriteCount = CountSprites(defs);
    BoundedVector<SpriteLib::SpriteEntry> entries(spriteCount);

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

Result<SpriteLib>
SpriteLib::Create(const std::span<const SpriteSheetDef> defs, SDL_Renderer* renderer)
{
    auto rectResult = CollectRects(defs);
    GG_CHECK(rectResult, "Failed to collect sprite rects");

    BoundedVector<SpriteEntry> entries = CollectSpriteEntries(defs);

    BoundedVector<Sprite::NameType> names = CollectSpriteNames(defs);

    BoundedVector<TexturePath> paths = CollectPaths(defs);

    TextureLoader textureLoader(std::move(paths), renderer);
    GG_CHECK(textureLoader.Start(), "Failed to start texture loader");

    return SpriteLib(defs.size(),
        std::move(*rectResult),
        std::move(entries),
        std::move(names),
        std::move(textureLoader));
}

SpriteLib::SpriteLib(const size_t sheetCount,
        BoundedVector<SDL_FRect>&& spriteRects,
        BoundedVector<SpriteEntry>&& spriteEntries,
        BoundedVector<Sprite::NameType>&& spriteNames,
        TextureLoader&& textureLoader)
    : m_SpriteRects(std::move(spriteRects)),
      m_SpriteEntries(std::move(spriteEntries)),
      m_Sprites(m_SpriteEntries.size()),
      m_Textures(sheetCount),
      m_SpriteNames(std::move(spriteNames))
{
    GG_ASSERT(textureLoader.IsPending(), "Texture loader must be pending");

    m_TextureLoader.emplace(std::move(textureLoader));

    m_Status = Status::Loading;
}

void
SpriteLib::Update()
{
    if(!GG_VERIFY(Status::None != m_Status, "SpriteLib has not been loaded"))
    {
        return;
    }

    if(Status::Loading != m_Status)
    {
        GG_WARN("Update() called but while not in loading stage");
        return;
    }

    GG_ASSERT(m_TextureLoader, "Texture loader must be initialized");

    m_TextureLoader->Update();

    if(m_TextureLoader->IsPending())
    {
        return;
    }

    m_Status = Status::Failed;

    auto textureLibResult = m_TextureLoader->Take();

    // Free the texture loader as we no longer need it after taking the textures.
    m_TextureLoader.reset();

    if(!textureLibResult)
    {
        GG_ERROR("Failed to take texture library result");
        return;
    }

    for(TexturePtr& texture : textureLibResult->GetTextures())
    {
        m_Textures.push_back(std::move(texture));
    }

    const std::span<const SDL_FRect> spriteRects = m_SpriteRects;

    m_Status = Status::Failed;

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

    m_Status = Status::Loaded;
}

bool
SpriteLib::IsPending() const
{
    return m_TextureLoader && m_TextureLoader->IsPending();
}

bool
SpriteLib::IsLoaded() const
{
    return m_Status == Status::Loaded;
}

Result<const Sprite*>
SpriteLib::GetSprite(const std::string_view name) const
{
    return GetSprite(Sprite::NameType{ name });
}

Result<const Sprite*>
SpriteLib::GetSprite(const Sprite::NameType& name) const
{
    GG_CHECKB(m_Status == Status::Loaded, "SpriteLib is not loaded");
    GG_ASSERT(m_Sprites.size() == m_SpriteNames.size(),
        "Mismatch between sprites and sprite names");

    for(size_t i = 0; i < m_SpriteNames.size(); ++i)
    {
        if(name == *m_SpriteNames.TryGet(i))
        {
            return m_Sprites.TryGet(i);
        }
    }

    GG_ERROR("Sprite not found: {}", name);
    
    return Result<>::Fail();
}
