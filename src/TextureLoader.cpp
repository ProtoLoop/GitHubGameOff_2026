#include "TextureLoader.h"

#include "Defer.h"
#include "FileFetcher.h"

#include <memory>
#include <SDL3/SDL_render.h>
#include <stb_image.h>

#ifdef __EMSCRIPTEN__
#include <emscripten/fetch.h>
#include <limits>
#include <span>
#endif

namespace
{
constexpr int kNumTextureChannels = 4;

Result<stbi_uc*>
LoadImageFromMemory(const FetchRequestPtr& rqstPtr, int* x, int* y)
{
    auto dataResult = rqstPtr->GetData();
    GG_CHECKB(dataResult, "Failed to get fetch data");
    GG_CHECKB(dataResult->size() <= std::numeric_limits<int>::max(),
        "Data size exceeds maximum allowed size");

    const void* p = dataResult->data();
    const stbi_uc* imageData = static_cast<const stbi_uc*>(p);
    const int numBytes = static_cast<int>(dataResult->size());

    stbi_uc* image =
        stbi_load_from_memory(imageData, numBytes, x, y, nullptr, kNumTextureChannels);

    GG_CHECKB(image, "Failed to load image: {}", stbi_failure_reason());

    return image;
}

Result<TexturePtr>
CreateTexture(const void* data, const int width, const int height, SDL_Renderer* renderer)
{
    SDL_Texture* texture = SDL_CreateTexture(renderer,
        SDL_PIXELFORMAT_RGBA32,
        SDL_TEXTUREACCESS_STATIC,
        width,
        height);

    GG_CHECKB(texture, "Failed to create texture: {}", SDL_GetError());

    TexturePtr texturePtr(texture);

    GG_CHECKB(SDL_UpdateTexture(texture, nullptr, data, width * kNumTextureChannels),
        "Failed to update texture: {}",
        SDL_GetError());

    return texturePtr;
}
} // namespace

void
DestroyTexture::operator()(SDL_Texture* texture) const
{
    if(texture)
    {
        SDL_DestroyTexture(texture);
    }
}

Result<TextureLoader>
TextureLoader::Load(const FilePath& path, FileFetcher& fileFetcher, SDL_Renderer* renderer)
{
    auto fetchResult = fileFetcher.Fetch(path);
    GG_CHECK(fetchResult, "Failed to fetch file: {}", path);

    return TextureLoader(path, std::move(*fetchResult), renderer);
}

bool TextureLoader::IsPending() const
{
    return m_FetchRequest && m_FetchRequest->IsPending();
}

Result<TexturePtr>
TextureLoader::Take()
{
    GG_CHECKB(!m_Taken, "Texture has already been taken");
    GG_CHECKB(m_FetchRequest, "Texture fetch request is not valid");
    GG_CHECKB(!m_FetchRequest->IsPending(), "Texture fetch request is still pending");

    m_Taken = true;

    auto fetchCleanup = GG_DEFER
    {
        // Free the fetch request when leaving this scope.
        m_FetchRequest.reset();
    };

    int x{}, y{};
    auto imageResult = LoadImageFromMemory(m_FetchRequest, &x, &y);
    GG_CHECKB(imageResult, "Failed to load image: {}", m_Path);
    stbi_uc* image = *imageResult;

    auto cleanupImage = GG_DEFER
    {
        if(image)
        {
            stbi_image_free(image);
            image = nullptr;
        }
    };

    return CreateTexture(image, x, y, m_Renderer);
}