#include "TextureLoader.h"

#include "Defer.h"

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

BoundedVector<TextureLoadRequest>
CreateRequests(const std::span<const TexturePath> paths, SDL_Renderer* renderer)
{
    BoundedVector<TextureLoadRequest> requests(paths.size());
    for(const TexturePath& path : paths)
    {
        requests.emplace_back(path, renderer);
    }

    return requests;
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

TextureLoadRequest::~TextureLoadRequest()
{
#ifdef __EMSCRIPTEN__
    if(m_Fetch)
    {
        emscripten_fetch_t* fetch = nullptr;
        std::swap(m_Fetch, fetch);
        emscripten_fetch_close(fetch);
    }
#endif
}

Result<TexturePtr>
TextureLoadRequest::Take()
{
    GG_CHECKB(Status::Taken != m_Status, "Texture already taken: {}", m_Path);
    GG_CHECKB(Status::Succeeded == m_Status, "Texture not yet loaded: {}", m_Path);

    m_Status = Status::Taken;

#ifdef __EMSCRIPTEN__

    GG_CHECKB(m_Fetch, "Texture already taken: {}", m_Path);

    auto cleanupRequest = GG_DEFER
    {
        if(m_Fetch)
        {
            emscripten_fetch_close(m_Fetch);
            m_Fetch = nullptr;
        }
    };

    GG_CHECKB(m_Fetch->data, "Fetch data is null");
    GG_CHECKB(m_Fetch->numBytes <= std::numeric_limits<int>::max(),
        "Fetch data too large: {} - {}",
        m_Path,
        m_Fetch->numBytes);

    const void* p = m_Fetch->data;
    const stbi_uc* imageData = static_cast<const stbi_uc*>(p);
    const int numBytes = static_cast<int>(m_Fetch->numBytes);

    int x{}, y{};
    stbi_uc* image =
        stbi_load_from_memory(imageData, numBytes, &x, &y, nullptr, kNumTextureChannels);
#else

    std::string_view sv_path = m_Path;

    GG_CHECKB(!sv_path.empty(), "Texture path is empty");

    while(!sv_path.empty() && sv_path.front() == '/')
    {
        sv_path.remove_prefix(1);
    }

    const TexturePath path(sv_path);

    int x{}, y{};
    stbi_uc* image = stbi_load(path.c_str(), &x, &y, nullptr, kNumTextureChannels);

#endif // __EMSCRIPTEN__

    GG_CHECKB(image, "Failed to load image: {} - {}", m_Path, stbi_failure_reason());

    auto cleanupImage = GG_DEFER
    {
        if(image)
        {
            stbi_image_free(image);
            image = nullptr;
        }
    };

    return TextureLoader::CreateTexture(image, x, y, m_Renderer);
}

#ifdef __EMSCRIPTEN__

void
TextureLoadRequest::onSuccess(emscripten_fetch_t* fetch)
{
    TextureLoadRequest* request = static_cast<TextureLoadRequest*>(fetch->userData);
    GG_INFO("Fetch succeeded: {}", request->GetPath());

    request->m_Status = Status::Succeeded;
}

void
TextureLoadRequest::onError(emscripten_fetch_t* fetch)
{
    TextureLoadRequest* request = static_cast<TextureLoadRequest*>(fetch->userData);
    GG_INFO("Fetch failed: {} - HTTP {}", request->GetPath(), fetch->status);

    request->m_Status = Status::Failed;

    if(request->m_Fetch)
    {
        emscripten_fetch_close(request->m_Fetch);
        request->m_Fetch = nullptr;
    }
}

#endif // __EMSCRIPTEN__

TextureLoader::TextureLoader(const std::span<const TexturePath> paths, SDL_Renderer* renderer)
    : m_Requests(CreateRequests(paths, renderer)),
      m_Textures(paths.size())
{
}

Result<>
TextureLoader::Start()
{
    GG_CHECKB(m_Stage == Stage::None, "TextureLoader already started");

    m_Stage = Stage::Failed;

    GG_CHECKB(!m_Requests.empty(), "No texture load requests provided");

    for(const TextureLoadRequest& request : m_Requests)
    {
        GG_CHECKB(request.m_Status == TextureLoadRequest::Status::None,
            "Request already in progress");
    }

    for(TextureLoadRequest& request : m_Requests)
    {
        GG_VERIFY(Fetch(request), "Failed to fetch texture: {}", request.GetPath());
    }

    m_Stage = Stage::Loading;

    return Result<>::Success();
}

void
TextureLoader::Update()
{
    if(!GG_VERIFY(Stage::None != m_Stage, "TextureLoader not started"))
    {
        return;
    }

    switch(m_Stage)
    {
        case Stage::None:
            break;
        case Stage::Loading:
            m_Stage = Stage::CreateTextures;
            for(const TextureLoadRequest& request : m_Requests)
            {
                if(request.m_Status == TextureLoadRequest::Status::Pending)
                {
                    m_Stage = Stage::Loading;
                    break;
                }
            }
            break;
        case Stage::CreateTextures:
            if(GG_VERIFY(CreateTextures(), "Failed to create textures"))
            {
                m_Stage = Stage::Succeeded;
            }
            else
            {
                m_Stage = Stage::Failed;
            }

            m_Requests.clear();

            break;
        case Stage::Succeeded:
            [[fallthrough]];
        case Stage::Failed:
            [[fallthrough]];
        case Stage::Taken:
            break;
    }
}

bool
TextureLoader::IsPending() const
{
    GG_ASSERT(Stage::None != m_Stage, "TextureLoader not started");

    return Stage::Loading == m_Stage || Stage::CreateTextures == m_Stage;
}

Result<TextureLib>
TextureLoader::Take()
{
    GG_CHECKB(Stage::Taken != m_Stage, "TextureLib already taken");
    GG_CHECKB(m_Stage >= Stage::Succeeded, "TextureLoader still loading");
    GG_CHECKB(Stage::Succeeded == m_Stage, "TextureLoader did not succeed");

    m_Stage = Stage::Taken;
    return TextureLib(std::move(m_Textures));
}

// private:

Result<>
TextureLoader::Fetch(TextureLoadRequest& request)
{
    GG_CHECKB(request.m_Status == TextureLoadRequest::Status::None, "Request already started");

#ifdef __EMSCRIPTEN__
    emscripten_fetch_attr_t attr;
    emscripten_fetch_attr_init(&attr);
    std::ranges::copy("GET", std::begin(attr.requestMethod));
    attr.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY;
    attr.onsuccess = TextureLoadRequest::onSuccess;
    attr.onerror = TextureLoadRequest::onError;
    attr.userData = &request;

    request.m_Fetch = emscripten_fetch(&attr, request.m_Path.c_str());
    if(!GG_VERIFY(request.m_Fetch, "Failed to initiate fetch"))
    {
        request.m_Fetch = nullptr;
        request.m_Status = TextureLoadRequest::Status::Failed;
    }
    else
    {
        request.m_Status = TextureLoadRequest::Status::Pending;
    }
#else

    request.m_Status = TextureLoadRequest::Status::Succeeded;

#endif // __EMSCRIPTEN__

    return Result<>::Success();
}

Result<TexturePtr>
TextureLoader::CreateTexture(
    const void* data, const int width, const int height, SDL_Renderer* renderer)
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

Result<>
TextureLoader::CreateTextures()
{
    for(TextureLoadRequest& request : m_Requests)
    {
        TexturePtr texPtr;

        if(request.m_Status == TextureLoadRequest::Status::Succeeded)
        {
            auto texPtrResult = request.Take();
            if(GG_VERIFY(texPtrResult, "Failed to take texture: {}", request.GetPath()))
            {
                texPtr = std::move(*texPtrResult);
            }
        }
        m_Textures.push_back(std::move(texPtr));
    }

    return Result<>::Success();
}