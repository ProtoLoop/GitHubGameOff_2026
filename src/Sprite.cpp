#include "Sprite.h"

#include "Result.h"

#include <SDL3/SDL_render.h>

Result<Sprite>
Sprite::Create(SDL_Texture* texture, const SDL_FRect rect)
{
    GG_CHECKB(texture, "Texture is null");
    GG_CHECKB(rect.x >= 0 && rect.y >= 0 && rect.w > 0 && rect.h > 0,
        "Invalid rectangle: {},{},{},{}",
        rect.x,
        rect.y,
        rect.w,
        rect.h);

    float w{}, h{};
    GG_CHECKB(SDL_GetTextureSize(texture, &w, &h), "Failed to query texture: {}", SDL_GetError());

    GG_CHECKB(rect.x + rect.w <= w && rect.y + rect.h <= h,
        "Rectangle exceeds texture bounds: {},{},{},{}",
        rect.x,
        rect.y,
        rect.w,
        rect.h);

    return Sprite(texture, rect);
}

Sprite::Sprite(SDL_Texture* texture, const SDL_FRect rect)
    : m_Texture(texture),
      m_Rect(rect)
{
}