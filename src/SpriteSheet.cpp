#include "SpriteSheet.h"

SpriteSheet::SpriteSheet(const NameType& name,
    const uint32_t atlasIndex,
    const Rect& rect,
    const Dimension2d& spriteDims)
    : m_Name(name),
      m_AtlasIndex(atlasIndex),
      m_SheetRect(rect),
      m_SpriteDims(spriteDims),
      m_RowCount(m_SpriteDims.h ? m_SheetRect.h / m_SpriteDims.h : 0),
      m_ColCount(m_SpriteDims.w ? m_SheetRect.w / m_SpriteDims.w : 0)
{
}

Result<Rect>
SpriteSheet::GetFrameRect(const uint32_t row, const uint32_t col) const
{
    GG_CHECKB(row < m_RowCount, "Row index out of bounds");
    GG_CHECKB(col < m_ColCount, "Column index out of bounds");

    Rect rect{};
    rect.x = m_SheetRect.x + (col * m_SpriteDims.w);
    rect.y = m_SheetRect.y + (row * m_SpriteDims.h);
    rect.w = m_SpriteDims.w;
    rect.h = m_SpriteDims.h;
    return rect;
}