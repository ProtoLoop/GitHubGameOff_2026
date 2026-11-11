#pragma once

#include "InlineString.h"
#include "Result.h"

#include <webgpu/webgpu_cpp.h>

struct Rect
{
    uint32_t x;
    uint32_t y;
    uint32_t w;
    uint32_t h;
};

struct Dimension2d
{
    uint32_t w;
    uint32_t h;
};

class SpriteSheet
{
    static constexpr size_t kMaxNameLength = 32;

public:
    using NameType = InlineString<kMaxNameLength>;

    SpriteSheet() = delete;

    SpriteSheet(const NameType& name,
        const uint32_t atlasIndex,
        const Rect& rect,
        const Dimension2d& spriteDims);

    const NameType& GetName() const { return m_Name; }

    uint32_t GetRowCount() const { return m_RowCount; }

    uint32_t GetColCount() const { return m_ColCount; }

    Result<Rect> GetFrameRect(const uint32_t row, const uint32_t col) const;

private:
    friend class SpriteLibLoader;

    NameType m_Name;
    // Index into the webgpu texture array - passed to the shader.
    // Used by the sampler.
    uint32_t m_AtlasIndex;
    Rect m_SheetRect;
    Dimension2d m_SpriteDims;
    uint32_t m_RowCount;
    uint32_t m_ColCount;
};