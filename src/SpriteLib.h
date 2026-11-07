#pragma once

#include "BoundedVector.h"
#include "Result.h"
#include "Sprite.h"
#include "TextureLoader.h"

#include <string_view>
#include <variant>
#include <vector>

struct RectDef
{
    uint32_t X;
    uint32_t Y;
    uint32_t Width;
    uint32_t Height;
};

/// Defines a sequence of sprite frames by specifying
/// the rectangle enclosing all frames and the number of frames.
/// Frames move horizontally, left to right, within the rectangle.
/// The height of each frame is the height of the rectangle.
/// The width of each frame is the width of the rectangle divided by the number of frames.
struct SpriteFrameDef
{
    /// The rectangle enclosing all frames of this sprite.
    RectDef Rect;
    /// The number of frames in this sprite.
    uint32_t Count;
};

struct SpriteFrameListDef
{
    std::vector<RectDef> Rects;
};

struct SpriteDef
{
    Sprite::NameType Name;
    std::variant<SpriteFrameListDef, SpriteFrameDef> Frames;
};

struct SpriteSheetDef
{
    FilePath Path;
    std::vector<SpriteDef> Sprites;
};

class SpriteLib
{
public:

    SpriteLib() = default;

    Result<const Sprite*> GetSprite(const std::string_view name) const;
    Result<const Sprite*> GetSprite(const Sprite::NameType& name) const;

private:

    friend class SpriteLibLoader;

    SpriteLib(BoundedVector<TexturePtr>&& textures,
        BoundedVector<SDL_FRect>&& spriteRects,
        BoundedVector<Sprite::NameType>&& spriteNames,
        BoundedVector<Sprite>&& sprites);

    BoundedVector<TexturePtr> m_Textures{0};
    BoundedVector<SDL_FRect> m_SpriteRects{0};
    BoundedVector<Sprite::NameType> m_SpriteNames{0};
    BoundedVector<Sprite> m_Sprites{0};
};

class SpriteLibLoader
{
public:

    struct SpriteEntry
    {
        uint32_t SheetIndex{ 0 };
        uint32_t FrameIndex{ 0 };
        uint32_t FrameCount{ 0 };
    };

    static Result<SpriteLibLoader> Load(const std::span<const SpriteSheetDef> defs,
        FileFetcher& fileFetcher,
        SDL_Renderer* renderer);

    void Update();

    bool IsPending() const;

    bool IsLoaded() const;

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

    SpriteLibLoader(const size_t sheetCount,
        BoundedVector<SDL_FRect>&& spriteRects,
        BoundedVector<SpriteEntry>&& spriteEntries,
        BoundedVector<Sprite::NameType>&& spriteNames,
        BoundedVector<TextureLoader>&& textureLoaders);

    BoundedVector<TextureLoader> m_TextureLoaders;

    BoundedVector<SDL_FRect> m_SpriteRects;
    BoundedVector<SpriteEntry> m_SpriteEntries;
    BoundedVector<Sprite> m_Sprites;
    BoundedVector<TexturePtr> m_Textures;
    BoundedVector<Sprite::NameType> m_SpriteNames;

    Status m_Status{ Status::None };
};