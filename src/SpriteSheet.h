#pragma once

#include "InlineString.h"
#include "Result.h"

#include <SDL3/SDL_rect.h>
#include <webgpu/webgpu_cpp.h>

class SpriteSheet
{
    static constexpr size_t kMaxNameLength = 32;

public:
    using NameType = InlineString<kMaxNameLength>;

    SpriteSheet() = delete;

    SpriteSheet(const NameType& name,
        const uint32_t atlasIndex,
        const SDL_FRect& rect,
        const uint32_t frameWidth,
        const uint32_t frameHeight);

    const NameType& GetName() const { return m_Name; }

    uint32_t GetRowCount() const { return m_RowCount; }

    uint32_t GetColCount() const { return m_ColCount; }

    Result<SDL_FRect> GetFrameRect(const uint32_t row, const uint32_t col) const;

private:
    friend class SpriteLibLoader;

    NameType m_Name;
    // Index into the webgpu texture array - passed to the shader.
    // Used by the sampler.
    uint32_t m_AtlasIndex;
    SDL_FRect m_SheetRect;
    uint32_t m_FrameWidth;
    uint32_t m_FrameHeight;
    uint32_t m_RowCount;
    uint32_t m_ColCount;
};