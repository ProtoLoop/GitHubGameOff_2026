#pragma once

#include "InlineString.h"
#include "Result.h"

#include <SDL3/SDL_rect.h>

struct SDL_Texture;

class SpriteSheet
{
    static constexpr size_t kMaxNameLength = 32;

public:
    using NameType = InlineString<kMaxNameLength>;

    SpriteSheet() = delete;

    const NameType& GetName() const { return m_Name; }

    uint32_t GetRowCount() const { return m_RowCount; }

    uint32_t GetColCount() const { return m_ColCount; }

    Result<SDL_FRect> GetFrameRect(const uint32_t row, const uint32_t col) const;

    SDL_Texture* GetTexture() const { return m_Texture; }

private:
    friend class SpriteLibLoader;

    SpriteSheet(const NameType& name,
        SDL_Texture* texture,
        const SDL_FRect& rect,
        const uint32_t frameWidth,
        const uint32_t frameHeight);

    NameType m_Name;
    SDL_Texture* m_Texture;
    SDL_FRect m_SheetRect;
    uint32_t m_FrameWidth;
    uint32_t m_FrameHeight;
    uint32_t m_RowCount;
    uint32_t m_ColCount;
};