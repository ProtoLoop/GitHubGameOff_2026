#pragma once

#include "BoundedVector.h"
#include "SpriteSheet.h"
#include "TextureLoader.h"

#include <string_view>

class WebGpu;

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
    SpriteLib() = delete;

    Result<const SpriteSheet*> GetSpriteSheet(const std::string_view name) const;

    wgpu::Texture GetTexture() const { return m_Texture; }

private:
    friend class SpriteLibLoader;

    explicit SpriteLib(wgpu::Texture texture, BoundedVector<SpriteSheet>&& spriteSheets);

    wgpu::Texture m_Texture;
    BoundedVector<SpriteSheet> m_SpriteSheets{ 0 };
};

class SpriteLibLoader
{
public:
    static Result<SpriteLibLoader> Load(
        const std::span<const SpriteSheetDef> defs, FileFetcher& fileFetcher, const WebGpu& webGpu);

    Result<> Update();

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

    SpriteLibLoader(BoundedVector<SpriteSheet>&& spriteSheets, TextureLoader&& textureLoader);

    /// Validate that sprite sheet rectangles are within the bounds of the texture and have valid
    /// dimensions. For sprite sheets where rectangles were not specified, the full texture
    /// dimensions will be used.
    Result<> FixupSpriteSheets(const wgpu::Texture texture);

    TextureLoader m_TextureLoader;

    BoundedVector<SpriteSheet> m_SpriteSheets;

    wgpu::Texture m_Texture;

    Status m_Status{ Status::None };
};