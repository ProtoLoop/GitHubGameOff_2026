#include "TextureLoader.h"

#include "Defer.h"
#include "FileFetcher.h"
#include "Shortcuts.h"
#include "WebGpu.h"

#include <cstdint>
#include <memory>
#include <ranges>
#include <stb_image.h>

#ifdef __EMSCRIPTEN__
#include <emscripten/fetch.h>
#include <limits>
#include <span>
#endif

namespace
{
constexpr int kNumTextureChannels = 4;

struct stbiDeleter
{
    void operator()(stbi_uc* ptr) const
    {
        if(ptr)
        {
            stbi_image_free(ptr);
        }
    }
};

using StbiImagePtr = std::unique_ptr<stbi_uc, stbiDeleter>;

Result<StbiImagePtr>
LoadImageFromMemory(const FetchRequestPtr& rqstPtr, int* x, int* y)
{
    auto dataResult = rqstPtr->GetData();
    GG_CHECKB(dataResult, "Failed to get fetch data");
    GG_CHECKB(dataResult->size() <= std::numeric_limits<int>::max(),
        "Data size exceeds maximum allowed size");

    const void* p = dataResult->data();
    const stbi_uc* imageData = static_cast<const stbi_uc*>(p);
    const int numBytes = static_cast<int>(dataResult->size());

    stbi_uc* image = stbi_load_from_memory(imageData, numBytes, x, y, nullptr, kNumTextureChannels);

    GG_CHECKB(image, "Failed to load image: {}", stbi_failure_reason());

    return StbiImagePtr(image);
}

} // namespace

Result<TextureLoader>
TextureLoader::Load(const FilePath& path,
    FileFetcher& fileFetcher,
    const WebGpu& webGpu,
    const std::string_view label)
{
    auto fetchResult = fileFetcher.Fetch(path);
    GG_CHECK(fetchResult, "Failed to fetch file: {}", path);

    InlineVector<FilePath, TextureLoader::kMaxArrayLayers> paths;
    GG_CHECKB(paths.push_back(path), "Failed to add path to InlineVector");

    InlineVector<FetchRequestPtr, TextureLoader::kMaxArrayLayers> fetchRequests;
    GG_CHECKB(fetchRequests.push_back(std::move(*fetchResult)),
        "Failed to add fetch request to InlineVector");

    return TextureLoader(std::move(paths), std::move(fetchRequests), webGpu, label);
}

Result<TextureLoader>
TextureLoader::Load(const std::span<const FilePath> paths,
    FileFetcher& fileFetcher,
    const WebGpu& webGpu,
    const std::string_view label)
{
    GG_CHECKB(!paths.empty(), "No paths provided");
    GG_CHECKB(paths.size() <= TextureLoader::kMaxArrayLayers, "Too many paths provided");
    InlineVector<FetchRequestPtr, TextureLoader::kMaxArrayLayers> fetchRequests;
    for(const FilePath& path : paths)
    {
        auto fetchResult = fileFetcher.Fetch(path);
        GG_CHECK(fetchResult, "Failed to fetch file: {}", path);
        GG_CHECKB(fetchRequests.push_back(std::move(*fetchResult)),
            "Failed to add fetch request to InlineVector");
    }

    InlineVector<FilePath, TextureLoader::kMaxArrayLayers> inlinePaths;
    for(const FilePath& path : paths)
    {
        GG_CHECKB(inlinePaths.push_back(path), "Failed to add path to InlineVector");
    }

    return TextureLoader(std::move(inlinePaths), std::move(fetchRequests), webGpu, label);
}

bool
TextureLoader::IsPending() const
{
    for(const auto& fetchRequest : m_FetchRequests)
    {
        if(fetchRequest && fetchRequest->IsPending())
        {
            return true;
        }
    }
    return false;
}

Result<wgpu::Texture>
TextureLoader::Take()
{
    GG_CHECKB(!m_Taken, "Texture has already been taken");
    GG_CHECKB(!m_FetchRequests.empty(), "Texture fetch request is not valid");
    GG_CHECKB(!IsPending(), "Texture fetch request is still pending");

    m_Taken = true;

    auto fetchCleanup = GG_DEFER
    {
        // Free the fetch request when leaving this scope.
        m_FetchRequests.clear();
    };

    uint32_t width = 0, height = 0;

    InlineVector<std::span<const std::byte>, TextureLoader::kMaxArrayLayers> imageData;

    // Used to maintain the lifetime of the loaded images while the texture is being created
    // from the borrowed image data.
    InlineVector<StbiImagePtr, TextureLoader::kMaxArrayLayers> stbiImages;

    for(const auto& [fetchRequest, path] : std::views::zip(m_FetchRequests, m_Paths))
    {
        GG_CHECKB(fetchRequest, "Texture fetch request is not valid");
        int x{}, y{};
        auto imageResult = LoadImageFromMemory(fetchRequest, &x, &y);
        GG_CHECKB(imageResult, "Failed to load image: {}", path);

        const uint32_t w = scast<uint32_t>(x);
        const uint32_t h = scast<uint32_t>(y);

        GG_CHECKB(width == 0 || (width == w && height == h),
            "Texture array layers must have the same dimensions");

        width = w;
        height = h;

        const size_t size = scast<size_t>(width) * scast<size_t>(height) * kNumTextureChannels;
        const std::byte* imageBytes = recast<const std::byte*>(imageResult->get());
        const std::span<const std::byte> imageSpan(imageBytes, size);

        // Borrow the image data...
        GG_CHECKB(imageData.push_back(imageSpan), "Failed to add image data to InlineVector");

        // ...and hold onto the stbi image to maintain its lifetime.
        GG_CHECKB(stbiImages.push_back(std::move(*imageResult)), "Failed to add stbi image to InlineVector");
    }

    return m_WebGpu->CreateTexture(width, height, imageData, m_Label);
}