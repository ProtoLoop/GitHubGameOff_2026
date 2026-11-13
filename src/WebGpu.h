#pragma once

#include "Result.h"

#include <webgpu/webgpu_cpp.h>

struct SDL_Window;
using SDL_MetalView = void*;

class WebGpu
{
public:
    WebGpu() = default;
    ~WebGpu();
    WebGpu(const WebGpu&) = delete;
    WebGpu& operator=(const WebGpu&) = delete;
    WebGpu(WebGpu&&) = delete;
    WebGpu& operator=(WebGpu&&) = delete;

    /// Starts asynchronous initialization once. Repeated calls fail without changing state.
    Result<> Start(SDL_Window* window);

    /// Processes callbacks without waiting. Requires Start() to have succeeded.
    /// Call every frame, including after readiness. Failure is terminal.
    Result<> Update();

    bool IsReady() const;

    /// Requires IsReady(). Returned handles must be released on the main thread.
    /// Destroying this context destroys the device even if callers retain handles.
    wgpu::Device GetDevice() const;

    /// Requires IsReady(). Returned handles must be released on the main thread.
    wgpu::Queue GetQueue() const;

    /// Creates a single layer texture.
    wgpu::Texture CreateTexture(const uint32_t width,
        const uint32_t height,
        std::span<const std::byte> data,
        const std::string_view name) const;

    /// Creates multilayer texture. Each element in `data` represents a layer.
    wgpu::Texture CreateTexture(const uint32_t width,
        const uint32_t height,
        std::span<std::span<const std::byte>> data,
        const std::string_view name) const;

    wgpu::Buffer CreateStorageBuffer(const size_t size, const std::string_view name) const;

    wgpu::Buffer CreateUniformBuffer(const size_t size, const std::string_view name) const;

    wgpu::Sampler GetDefaultSampler() const;

    wgpu::TextureFormat GetSwapchainFormat() const;

    Result<wgpu::Texture> GetSwapchainTexture() const;

    Result<> Present() const;

private:
    enum class Stage
    {
        Idle,
        RequestingAdapter,
        AdapterReady,
        RequestingDevice,
        DeviceReady,
        Ready,
        Failed,
        Stopping,
    };

    static void OnAdapterRequestedCb(wgpu::RequestAdapterStatus status,
        wgpu::Adapter adapter,
        wgpu::StringView message,
        WebGpu* context);

    static void OnDeviceRequestedCb(wgpu::RequestDeviceStatus status,
        wgpu::Device device,
        wgpu::StringView message,
        WebGpu* context);

    static void OnDeviceLostCb(const wgpu::Device& device,
        wgpu::DeviceLostReason reason,
        wgpu::StringView message,
        WebGpu* context);

    /// May run spontaneously. Only logs, without accessing GPU objects or state.
    static void OnUncapturedErrorCb(
        const wgpu::Device& device, wgpu::ErrorType type, wgpu::StringView message);

    Stage m_Stage{ Stage::Idle };
    SDL_Window* m_Window{ nullptr };
    SDL_MetalView m_MetalView{ nullptr };
    wgpu::Instance m_Instance;
    wgpu::Adapter m_Adapter;
    wgpu::Device m_Device;
    wgpu::Queue m_Queue;
    wgpu::Surface m_Surface;
    wgpu::Sampler m_DefaultSampler;
    wgpu::TextureFormat m_SwapchainFormat{ wgpu::TextureFormat::Undefined };
};
