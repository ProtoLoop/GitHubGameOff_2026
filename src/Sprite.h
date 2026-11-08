#pragma once

#include "InlineString.h"
#include "Result.h"

#include <SDL3/SDL_rect.h>
#include <span>

struct SDL_Texture;

class Sprite
{
public:

    //NOLINTNEXTLINE(readability-magic-numbers)
    using NameType = InlineString<32>;

    Sprite() = delete;

    static Result<Sprite> Create(SDL_Texture* texture, const std::span<const SDL_FRect> rects);

    SDL_Texture* GetTexture() const { return m_Texture; }

    size_t GetFrameCount() const { return m_Rects.size(); }

    const SDL_FRect& GetRect(const size_t frameIndex) const
    {
        return GG_VERIFYB(frameIndex < m_Rects.size()) ? m_Rects[frameIndex] : m_Rects[0];
    }

private:

    Sprite(SDL_Texture* texture, const std::span<const SDL_FRect> rects);

    SDL_Texture* m_Texture;
    std::span<const SDL_FRect> m_Rects;
};