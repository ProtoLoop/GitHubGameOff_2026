#pragma once

#include "FileFetcher.h"
#include "Result.h"

#include <cstddef>
#include <memory>

struct SDL_Texture;
struct SDL_Renderer;

struct DestroyTexture
{
    void operator()(SDL_Texture* texture) const;
};

using TexturePtr = std::unique_ptr<SDL_Texture, DestroyTexture>;

class TextureLoader
{
public:
    static Result<TextureLoader> Load(
        const FilePath& path, FileFetcher& fileFetcher, SDL_Renderer* renderer);

    bool IsPending() const;

    Result<TexturePtr> Take();

private:
    TextureLoader(const FilePath& path, FetchRequestPtr fetchRequest, SDL_Renderer* renderer)
        : m_Path(path),
          m_FetchRequest(std::move(fetchRequest)),
          m_Renderer(renderer)
    {
    }

    FilePath m_Path;
    FetchRequestPtr m_FetchRequest;
    SDL_Renderer* m_Renderer{ nullptr };

    bool m_Taken{ false };
};