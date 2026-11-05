#pragma once

#include "BoundedVector.h"
#include "InlineString.h"
#include "Result.h"

#include <cstddef>
#include <memory>
#include <span>

struct SDL_Texture;
struct SDL_Renderer;

static constexpr size_t kMaxTexturePathLength = 256 - 1;
using TexturePath = InlineString<kMaxTexturePathLength + 1>;

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

    ~TextureLoadRequest();
    TextureLoadRequest(const TextureLoadRequest&) = delete;
    TextureLoadRequest& operator=(const TextureLoadRequest&) = delete;
    TextureLoadRequest(TextureLoadRequest&&) = delete;
    TextureLoadRequest& operator=(TextureLoadRequest&&) = delete;

    explicit TextureLoadRequest(const std::string_view path, SDL_Renderer* renderer)
        : TextureLoadRequest(TexturePath(path), renderer)
    {
    }

    explicit TextureLoadRequest(const TexturePath& path, SDL_Renderer* renderer)
        : m_Path(path)
        , m_Renderer(renderer)
    {
    }

    const TexturePath& GetPath() const { return m_Path; }
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

    TexturePath m_Path;

    Status m_Status{ Status::None };

    SDL_Renderer* m_Renderer = nullptr;

    TexturePtr m_Texture;

#ifdef __EMSCRIPTEN__
    emscripten_fetch_t* m_Fetch = nullptr;
#endif
};

class TextureLib
{
public:
    TextureLib() = delete;
    ~TextureLib() = default;
    TextureLib(const TextureLib&) = delete;
    TextureLib& operator=(const TextureLib&) = delete;
    TextureLib(TextureLib&&) = default;
    TextureLib& operator=(TextureLib&&) = default;

    explicit TextureLib(BoundedVector<TexturePtr> textures)
        : m_Textures(std::move(textures))
    {
    }

    std::span<TexturePtr> GetTextures() { return m_Textures; }
    std::span<const TexturePtr> GetTextures() const { return m_Textures; }

private:

    BoundedVector<TexturePtr> m_Textures;
};

class TextureLoader
{
public:
    TextureLoader() = delete;
    ~TextureLoader() = default;
    TextureLoader(const TextureLoader&) = delete;
    TextureLoader& operator=(const TextureLoader&) = delete;
    TextureLoader(TextureLoader&& other) = default;
    TextureLoader& operator=(TextureLoader&& other) = default;

    TextureLoader(const std::span<const TexturePath> paths, SDL_Renderer* renderer);

    Result<> Start();

    void Update();

    bool IsPending() const;

    Result<TextureLib> Take();

private:

    friend class TextureLoadRequest;

    enum class Stage
    {
        None,
        Loading,
        CreateTextures,
        Succeeded,
        Failed,
        Taken
    };

    static Result<> Fetch(TextureLoadRequest& request);

    static Result<TexturePtr> CreateTexture(
        const void* data, const int width, const int height, SDL_Renderer* renderer);

    Result<> CreateTextures();

    BoundedVector<TextureLoadRequest> m_Requests;
    BoundedVector<TexturePtr> m_Textures;

    Stage m_Stage{ Stage::None };
};