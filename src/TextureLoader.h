#pragma once

#include "FileFetcher.h"
#include "InlineVector.h"
#include "Result.h"

#include <cstddef>
#include <span>
#include <string_view>

class WebGpu;

namespace wgpu
{
class Texture;
}

class TextureLoader
{
public:
    static constexpr size_t kMaxArrayLayers = 16;

    /// Loads an image from the given path and creates a single layer texture..
    static Result<TextureLoader> Load(const FilePath& path,
        FileFetcher& fileFetcher,
        const WebGpu& webGpu,
        const std::string_view label);

    /// Loads a collection of images from the given paths and creates a multilayer texture.
    /// Layers are in the same order as the provided paths.
    static Result<TextureLoader> Load(const std::span<const FilePath> paths,
        FileFetcher& fileFetcher,
        const WebGpu& webGpu,
        const std::string_view label);

    bool IsPending() const;

    Result<wgpu::Texture> Take();

private:
    TextureLoader(InlineVector<FilePath, kMaxArrayLayers> paths,
        InlineVector<FetchRequestPtr, kMaxArrayLayers> fetchRequests,
        const WebGpu& webGpu,
        const std::string_view label)
        : m_Paths(std::move(paths)),
          m_FetchRequests(std::move(fetchRequests)),
          m_WebGpu(&webGpu),
          m_Label(label)
    {
    }

    static constexpr size_t kMaxLabelLength = 64;

    InlineVector<FilePath, kMaxArrayLayers> m_Paths;
    InlineVector<FetchRequestPtr, kMaxArrayLayers> m_FetchRequests;
    const WebGpu* m_WebGpu;
    InlineString<kMaxLabelLength> m_Label;

    bool m_Taken{ false };
};