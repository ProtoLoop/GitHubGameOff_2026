#pragma once

#include "BoundedVector.h"
#include "SpriteSheet.h"
#include "TextureLoader.h"

#include <string_view>

struct RectDef
{
    uint32_t X;
    uint32_t Y;
    uint32_t Width;
    uint32_t Height;
};

struct Dimension2Def
{
    uint32_t Width;
    uint32_t Height;
};

struct SpriteSheetDef
{
    FilePath Path;
    SpriteSheet::NameType Name;
    /// The rectangle enclosing the sprite sheet.
    /// Could be a subrectangle of the full atlas texture.
    /// If width/height are zero, the full texture dimensions will be used.
    RectDef SheetRect{};
    Dimension2Def SpriteDims{};
};

class SpriteLib
{
public:
    SpriteLib() = default;

    Result<const SpriteSheet*> GetSpriteSheet(const std::string_view name) const;

private:
    friend class SpriteLibLoader;

    SpriteLib(BoundedVector<TexturePtr>&& textures, BoundedVector<SpriteSheet>&& spriteSheets);

    BoundedVector<TexturePtr> m_Textures{ 0 };
    BoundedVector<SpriteSheet> m_SpriteSheets{ 0 };
};

class SpriteLibLoader
{
public:
    struct SpriteSheetInfo
    {
        SpriteSheet::NameType Name;
        uint32_t AtlasIndex{ 0 };
        SDL_FRect SheetRect{};
        uint32_t SpriteWidth{ 0 };
        uint32_t SpriteHeight{ 0 };
    };

    static Result<SpriteLibLoader> Load(const std::span<const SpriteSheetDef> defs,
        FileFetcher& fileFetcher,
        SDL_Renderer* renderer);

    void Update();

    bool IsPending() const;

    Result<SpriteLib> Take();

private:
    enum class Status
    {
        None,
        Loading,
        Loaded,
        Failed,
        Taken
    };

    SpriteLibLoader(BoundedVector<SpriteSheetInfo>&& spriteSheets,
        BoundedVector<TextureLoader>&& textureLoaders);

    BoundedVector<TextureLoader> m_TextureLoaders;

    BoundedVector<SpriteSheetInfo> m_SpriteSheetInfos;
    BoundedVector<TexturePtr> m_Textures;

    Status m_Status{ Status::None };
};