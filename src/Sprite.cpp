#include "Sprite.h"

#include "Result.h"

#include <SDL3/SDL_render.h>

Result<Sprite>
Sprite::Create(SDL_Texture* texture, const std::span<const SDL_FRect> rects)
{
    GG_CHECKB(texture, "Texture is null");
    GG_CHECKB(!rects.empty(), "No rectangles provided");

    float w{}, h{};
    GG_CHECKB(SDL_GetTextureSize(texture, &w, &h), "Failed to query texture: {}", SDL_GetError());

    for(const SDL_FRect& rect : rects)
    {
        GG_CHECKB(rect.x >= 0 && rect.y >= 0 && rect.w > 0 && rect.h > 0,
            "Invalid rectangle: {},{},{},{}",
            rect.x,
            rect.y,
            rect.w,
            rect.h);

        GG_CHECKB(rect.x + rect.w <= w && rect.y + rect.h <= h,
            "Rectangle exceeds texture bounds: {},{},{},{}",
            rect.x,
            rect.y,
            rect.w,
            rect.h);
    }

    return Sprite(texture, rects);
}

Sprite::Sprite(SDL_Texture* texture, const std::span<const SDL_FRect> rects)
    : m_Texture(texture),
      m_Rects(rects)
{
}