#pragma once

#include "Result.h"
#include "Sprite.h"
#include "TextureLoader.h"

#include <optional>
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
    TexturePath Path;
    std::vector<SpriteDef> Sprites;
};

class SpriteLib
{
public:
    enum class Status
    {
        None,
        Loading,
        Loaded,
        Failed
    };

    struct SpriteEntry
    {
        uint32_t SheetIndex{ 0 };
        uint32_t FrameIndex{ 0 };
        uint32_t FrameCount{ 0 };
    };

    static Result<SpriteLib> Create(const std::span<const SpriteSheetDef> defs,
        SDL_Renderer* renderer);

    void Update();

    bool IsPending() const;

    bool IsLoaded() const;

    Status GetStatus() const { return m_Status; }

    Result<const Sprite*> GetSprite(const std::string_view name) const;
    Result<const Sprite*> GetSprite(const Sprite::NameType& name) const;

private:

    SpriteLib(const size_t sheetCount,
        BoundedVector<SDL_FRect>&& spriteRects,
        BoundedVector<SpriteEntry>&& spriteEntries,
        BoundedVector<Sprite::NameType>&& spriteNames,
        TextureLoader&& textureLoader);

    Status m_Status{ Status::None };

    std::optional<TextureLoader> m_TextureLoader;

    BoundedVector<SDL_FRect> m_SpriteRects;
    BoundedVector<SpriteEntry> m_SpriteEntries;
    BoundedVector<Sprite> m_Sprites;
    BoundedVector<TexturePtr> m_Textures;
    BoundedVector<Sprite::NameType> m_SpriteNames;
};