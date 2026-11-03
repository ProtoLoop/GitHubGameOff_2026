#pragma once

#include "Result.h"

#include <SDL3/SDL_rect.h>

struct SDL_Texture;

class Sprite
{
public:

    Sprite() = delete;

    static Result<Sprite> Create(SDL_Texture* texture, const SDL_FRect rect);

    SDL_Texture* GetTexture() const { return m_Texture; }

    const SDL_FRect& GetRect() const { return m_Rect; }

private:

    Sprite(SDL_Texture* texture, const SDL_FRect rect);

    SDL_Texture* m_Texture;
    SDL_FRect m_Rect;
};