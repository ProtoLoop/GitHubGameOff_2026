#include "SpriteSheet.h"

#include <SDL3/SDL_render.h>

SpriteSheet::SpriteSheet(const NameType& name,
    SDL_Texture* texture,
    const SDL_FRect& rect,
    const uint32_t frameWidth,
    const uint32_t frameHeight)
    : m_Name(name),
      m_Texture(texture),
      m_SheetRect(rect),
      m_FrameWidth(frameWidth),
      m_FrameHeight(frameHeight),
      m_RowCount(m_FrameHeight
              ? static_cast<uint32_t>(m_SheetRect.h / static_cast<float>(m_FrameHeight))
              : 0),
      m_ColCount(m_FrameWidth
              ? static_cast<uint32_t>(m_SheetRect.w / static_cast<float>(m_FrameWidth))
              : 0)
{
}

Result<SDL_FRect>
SpriteSheet::GetFrameRect(const uint32_t row, const uint32_t col) const
{
    GG_CHECKB(row < m_RowCount, "Row index out of bounds");
    GG_CHECKB(col < m_ColCount, "Column index out of bounds");

    SDL_FRect rect{};
    rect.x = m_SheetRect.x + static_cast<float>(col * m_FrameWidth);
    rect.y = m_SheetRect.y + static_cast<float>(row * m_FrameHeight);
    rect.w = static_cast<float>(m_FrameWidth);
    rect.h = static_cast<float>(m_FrameHeight);
    return rect;
}