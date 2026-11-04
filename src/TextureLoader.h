#pragma once

#include "InlineString.h"
#include "InlineVector.h"
#include "Result.h"

#include <memory>
#include <span>

struct SDL_Texture;
struct SDL_Renderer;

#ifdef __EMSCRIPTEN__
struct emscripten_fetch_t;
#endif

struct DestroyTexture
{
    void operator()(SDL_Texture* texture) const;
};

using TexturePtr = std::unique_ptr<SDL_Texture, DestroyTexture>;

class TextureLoadRequest
{
public:

    static constexpr size_t kMaxPathLength = 256 - 1;
    using PathType = InlineString<kMaxPathLength + 1>;

    ~TextureLoadRequest();
    TextureLoadRequest(const TextureLoadRequest&) = delete;
    TextureLoadRequest& operator=(const TextureLoadRequest&) = delete;
    TextureLoadRequest(TextureLoadRequest&&) = delete;
    TextureLoadRequest& operator=(TextureLoadRequest&&) = delete;

    explicit TextureLoadRequest(const std::string_view path)
        : TextureLoadRequest(PathType(path))
    {
    }

    explicit TextureLoadRequest(const PathType& path)
        : m_Path(path)
    {
    }

    const PathType& GetPath() const { return m_Path; }
    bool IsPending() const { return m_Status == Status::Pending; }
    bool Succeeded() const { return m_Status == Status::Succeeded; }

    Result<TexturePtr> Take();

private:
    friend class TextureLoader;

    enum class Status
    {
        None,
        Pending,
        Succeeded,
        Failed,
        Taken
    };

#ifdef __EMSCRIPTEN__

    static void onSuccess(emscripten_fetch_t* fetch);

    static void onError(emscripten_fetch_t* fetch);

#endif

    PathType m_Path;

    Status m_Status{ Status::None };

    SDL_Renderer* m_Renderer = nullptr;

    TexturePtr m_Texture;

#ifdef __EMSCRIPTEN__
    emscripten_fetch_t* m_Fetch = nullptr;
#endif
};

class TextureLoader
{
public:
    static const size_t kMaxRequests = 256;

    TextureLoader() = delete;
    ~TextureLoader() = default;
    TextureLoader(const TextureLoader&) = delete;
    TextureLoader& operator=(const TextureLoader&) = delete;
    TextureLoader(TextureLoader&& other) = default;
    TextureLoader& operator=(TextureLoader&& other) = default;

    explicit TextureLoader(const std::span<TextureLoadRequest> requests);

    Result<> Start(SDL_Renderer* renderer);

    bool IsPending() const;

private:

    InlineVector<TextureLoadRequest, kMaxRequests> m_RequestsBuffer;

    std::span<TextureLoadRequest> m_Requests;
};