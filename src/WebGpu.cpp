#include "WebGpu.h"

#include "Diag.h"
#include "Shortcuts.h"

#include <SDL3/SDL_metal.h>
#include <SDL3/SDL_video.h>
#include <string_view>
#include <utility>

namespace
{
constexpr wgpu::TextureFormat kDefaultTextureFormat = wgpu::TextureFormat::RGBA8UnormSrgb;

bool
TryGetWindowDims(SDL_Window* window, uint32_t& width, uint32_t& height)
{
    GG_ASSERT(window, "Window must not be null");

    int w{}, h{};
    SDL_GetWindowSize(window, &w, &h);
    width = scast<uint32_t>(w);
    height = scast<uint32_t>(h);
    return true;
}

const char*
GetPresentModeString(const wgpu::PresentMode presentMode)
{
    switch(presentMode)
    {
        case wgpu::PresentMode::Undefined:
            return "Undefined";
        case wgpu::PresentMode::Fifo:
            return "Fifo";
        case wgpu::PresentMode::FifoRelaxed:
            return "FifoRelaxed";
        case wgpu::PresentMode::Mailbox:
            return "Mailbox";
        case wgpu::PresentMode::Immediate:
            return "Immediate";
        default:
            return "Unknown";
    }
}

wgpu::PresentMode
ChoosePresentMode(const std::span<const wgpu::PresentMode> availableModes)
{
    // Preferred modes in order of preference.
    constexpr wgpu::PresentMode modePreference[] //
        {
            wgpu::PresentMode::Immediate,
            wgpu::PresentMode::Mailbox,
            wgpu::PresentMode::Fifo,
            wgpu::PresentMode::FifoRelaxed,
        };

    for(const wgpu::PresentMode mode : modePreference)
    {
        for(const wgpu::PresentMode availableMode : availableModes)
        {
            if(mode == availableMode)
            {
                return mode;
            }
        }
    }

    return wgpu::PresentMode::Undefined;
}

wgpu::TextureFormat
ChooseBackbufferFormat(const std::span<const wgpu::TextureFormat> availableFormats)
{
    // Preferred formats in order of preference.
    constexpr wgpu::TextureFormat preferredFormats[] = //
        {
            wgpu::TextureFormat::RGBA8UnormSrgb,
            wgpu::TextureFormat::BGRA8UnormSrgb,
            wgpu::TextureFormat::RGBA8Unorm,
            wgpu::TextureFormat::BGRA8Unorm,
        };

    wgpu::TextureFormat best = wgpu::TextureFormat::Undefined;
    size_t bestIdx = std::numeric_limits<size_t>::max();

    // Prefer BGRA8Unorm if available
    for(const wgpu::TextureFormat format : availableFormats)
    {
        for(size_t i = 0; i < std::size(preferredFormats); ++i)
        {
            if(format == preferredFormats[i] && i < bestIdx)
            {
                best = format;
                bestIdx = i;
            }
        }
    }

    if(best != wgpu::TextureFormat::Undefined)
    {
        return best;
    }

    // Fallback to first available format
    return availableFormats[0];
}

Result<wgpu::Surface>
CreateSurface(const wgpu::Instance& instance,
    [[maybe_unused]] SDL_Window* window,
    [[maybe_unused]] SDL_MetalView metalView)
{
    wgpu::SurfaceDescriptor surfaceDesc{};
    wgpu::Surface surface;

#if defined(__EMSCRIPTEN__)

    wgpu::EmscriptenSurfaceSourceCanvasHTMLSelector canvas_desc = {};
    canvas_desc.selector = "#canvas";

    surfaceDesc.nextInChain = &canvas_desc;
    surface = instance.CreateSurface(&surfaceDesc);

#elif defined(_WIN32)

    GG_DEBUG("Creating surface for Win32 HWND");

    const SDL_PropertiesID props = SDL_GetWindowProperties(window);

    void* hwnd = SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL);

    wgpu::SurfaceSourceWindowsHWND surfaceSrc{};
    surfaceSrc.hinstance = ::GetModuleHandle(NULL);
    surfaceSrc.hwnd = hwnd;
    surfaceDesc.nextInChain = &surfaceSrc;
    surface = instance.CreateSurface(&surfaceDesc);

#elif defined(__linux__)

    const char* sdlDriver = SDL_GetCurrentVideoDriver();
    GG_DEBUG("Creating surface for Linux - SDL video driver: {}",
        sdlDriver ? sdlDriver : "unknown");

    const SDL_PropertiesID props = SDL_GetWindowProperties(window);

    if(sdlDriver && strcmp(sdlDriver, "wayland") == 0)
    {
        wgpu::SurfaceSourceWaylandSurface surfaceSrc{};
        surfaceSrc.display =
            SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WAYLAND_DISPLAY_POINTER, nullptr);
        surfaceSrc.surface =
            SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WAYLAND_SURFACE_POINTER, nullptr);
        surfaceDesc.nextInChain = &surfaceSrc;
        surface = instance.CreateSurface(&surfaceDesc);
    }
    else if(sdlDriver && strcmp(sdlDriver, "x11") == 0)
    {
        wgpu::SurfaceSourceXlibWindow surfaceSrc{};
        surfaceSrc.display =
            SDL_GetPointerProperty(props, SDL_PROP_WINDOW_X11_DISPLAY_POINTER, nullptr);
        const Sint64 windowNumber =
            SDL_GetNumberProperty(props, SDL_PROP_WINDOW_X11_WINDOW_NUMBER, -1);
        surfaceSrc.window = static_cast<uint64_t>(windowNumber);
        surfaceDesc.nextInChain = &surfaceSrc;
        surface = instance.CreateSurface(&surfaceDesc);
    }
    else
    {
        GG_ERROR("Unsupported SDL video driver: {}", sdlDriver ? sdlDriver : "unknown");
    }
#elif defined(__APPLE__)

    GG_DEBUG("Creating surface for Metal");

    void* metalLayer = SDL_Metal_GetLayer(metalView);
    GG_CHECK(metalLayer, "Error getting Metal layer: {}", SDL_GetError());

    wgpu::SurfaceSourceMetalLayer surfaceSrc{};
    surfaceSrc.layer = metalLayer;
    surfaceDesc.nextInChain = &surfaceSrc;
    surface = instance.CreateSurface(&surfaceDesc);
#else
    GG_ERROR("Unsupported platform for surface creation");
#endif

    GG_CHECK(surface, "Failed to create Surface");

    return surface;
}

Result<wgpu::TextureFormat>
ConfigureSurface(const wgpu::Adapter& adapter,
    const wgpu::Device& device,
    const wgpu::Surface& surface,
    const uint32_t width,
    const uint32_t height)
{
    wgpu::SurfaceCapabilities capabilities;
    const wgpu::Status status = surface.GetCapabilities(adapter, &capabilities);
    GG_CHECK(status == wgpu::Status::Success, "surface.GetCapabilities failed");

    const std::span<const wgpu::PresentMode> presentModes(capabilities.presentModes,
        capabilities.presentModeCount);
    const std::span<const wgpu::TextureFormat> formats(capabilities.formats,
        capabilities.formatCount);

    const wgpu::PresentMode presentMode = ChoosePresentMode(presentModes);
    GG_CHECK(presentMode != wgpu::PresentMode::Undefined, "No supported present mode found");

    GG_INFO("Present mode: {}", GetPresentModeString(presentMode));

    const wgpu::TextureFormat format = ChooseBackbufferFormat(formats);
    GG_CHECK(format != wgpu::TextureFormat::Undefined, "No supported backbuffer format found");

    const wgpu::SurfaceConfiguration config //
        {
            .device = device,
            .format = format,
            .usage = wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::CopyDst,
            .width = width,
            .height = height,
            .alphaMode = wgpu::CompositeAlphaMode::Opaque,
            .presentMode = presentMode,
        };

    surface.Configure(&config);

    return format;
}

wgpu::Sampler
CreateSampler(const wgpu::Device& gpuDevice)
{
    const wgpu::SamplerDescriptor desc //
        {
            .label = "Scene::Sampler",
            .addressModeU = wgpu::AddressMode::ClampToEdge,
            .addressModeV = wgpu::AddressMode::ClampToEdge,
            .addressModeW = wgpu::AddressMode::ClampToEdge,
            .magFilter = wgpu::FilterMode::Nearest,
            .minFilter = wgpu::FilterMode::Nearest,
            .mipmapFilter = wgpu::MipmapFilterMode::Linear,
        };

    return gpuDevice.CreateSampler(&desc);
}
} // namespace

WebGpu::~WebGpu()
{
    m_Stage = Stage::Stopping;
    if(!m_Instance)
    {
        return;
    }

    // Finish already-ready requests while callback userdata is still alive. A device
    // delivered during shutdown is destroyed by its callback instead of being installed.
    m_Instance.ProcessEvents();

    if(m_MetalView)
    {
        SDL_Metal_DestroyView(m_MetalView);
        m_MetalView = nullptr;
    }

    if(m_Device)
    {
        m_Device.Destroy();
    }
    // Destroy readies the device-lost callback, including for externally retained handles.
    m_Instance.ProcessEvents();
    m_Queue = nullptr;
    m_Device = nullptr;
    m_Adapter = nullptr;
    // Browser requests can still be pending. Releasing the instance cancels their
    // non-spontaneous callbacks before this object goes away.
    m_Instance = nullptr;
    m_Surface = nullptr;
}

Result<>
WebGpu::Start(SDL_Window* window)
{
    GG_CHECK(m_Stage == Stage::Idle, "WebGPU initialization has already been started");
    m_Stage = Stage::Failed;

    m_Window = window;

#if defined(__APPLE__)
    SDL_MetalView metalView = SDL_Metal_CreateView(window);
    GG_CHECK(metalView, "Failed to create Metal view: {}", SDL_GetError());
    m_MetalView = metalView;
#endif

    m_Instance = wgpu::CreateInstance(nullptr);
    GG_CHECK(m_Instance, "Failed to create WebGPU instance");

    const wgpu::RequestAdapterOptions options{};
    m_Stage = Stage::RequestingAdapter;
    m_Instance.RequestAdapter(&options,
        wgpu::CallbackMode::AllowProcessEvents,
        OnAdapterRequestedCb,
        this);

    return Result<>::Success();
}

Result<>
WebGpu::Update()
{
    switch(m_Stage)
    {
        case Stage::Idle:
            GG_ERROR("WebGPU initialization has not been started");
            return Result<>::Fail();
        case Stage::RequestingAdapter:
            break;
        case Stage::AdapterReady:
        {
            wgpu::AdapterInfo info{};
            if(m_Adapter.GetInfo(&info) == wgpu::Status::Success)
            {
                GG_INFO("WebGPU adapter: {} / {}",
                    std::string_view(info.vendor),
                    std::string_view(info.device));
            }

            wgpu::DeviceDescriptor descriptor{};
            descriptor.SetDeviceLostCallback(wgpu::CallbackMode::AllowProcessEvents,
                OnDeviceLostCb,
                this);
            descriptor.SetUncapturedErrorCallback(OnUncapturedErrorCb);

            m_Adapter.RequestDevice(&descriptor,
                wgpu::CallbackMode::AllowProcessEvents,
                OnDeviceRequestedCb,
                this);

            m_Stage = Stage::RequestingDevice;
        }
        break;
        case Stage::RequestingDevice:
            break;
        case Stage::DeviceReady:
        {
            auto surfaceResult = CreateSurface(m_Instance, m_Window, m_MetalView);
            GG_CHECK(surfaceResult, "Failed to create WebGPU surface");
            m_Surface = *surfaceResult;

            uint32_t width{}, height{};
            GG_CHECK(TryGetWindowDims(m_Window, width, height), "Failed to get window dimensions");

            auto surfaceFormat = ConfigureSurface(m_Adapter, m_Device, m_Surface, width, height);
            GG_CHECK(surfaceFormat, "Failed to configure WebGPU surface");

            m_DefaultSampler = CreateSampler(m_Device);
            GG_CHECK(m_DefaultSampler, "Failed to create default sampler");

            m_Stage = Stage::Ready;
        }
        break;
        case Stage::Ready:
        case Stage::Failed:
        case Stage::Stopping:
            break;
    }
    if(m_Stage == Stage::Failed)
    {
        return Result<>::Fail();
    }
    m_Instance.ProcessEvents();

    if(m_Stage == Stage::Failed)
    {
        return Result<>::Fail();
    }
    return Result<>::Success();
}

bool
WebGpu::IsReady() const
{
    return m_Stage == Stage::Ready;
}

wgpu::Device
WebGpu::GetDevice() const
{
    GG_ASSERT(IsReady());
    return m_Device;
}

wgpu::Queue
WebGpu::GetQueue() const
{
    GG_ASSERT(IsReady());
    return m_Queue;
}

wgpu::Texture
WebGpu::CreateTexture(const uint32_t width,
    const uint32_t height,
    std::span<const std::byte> data,
    const std::string_view name) const
{
    GG_ASSERT(IsReady());

    GG_ASSERT(width > 0 && height > 0);
    GG_ASSERT(4uz * width * height == data.size(),
        "Texture data size does not match width * height * 4");

    const wgpu::TextureDescriptor desc //
        {
            .label = name,
            .usage = wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::CopyDst,
            .dimension = wgpu::TextureDimension::e2D,
            .size //
            {
                .width = width,
                .height = height,
                .depthOrArrayLayers = 1,
            },
            .format = kDefaultTextureFormat,
            .mipLevelCount = 1,
            .sampleCount = 1,
        };

    const wgpu::Texture texture = m_Device.CreateTexture(&desc);
    GG_ASSERT(texture);

    if(!data.empty())
    {
        const wgpu::TexelCopyTextureInfo dest //
            { .texture = texture, .mipLevel = 0, .origin = { .x = 0, .y = 0, .z = 0 } };

        const wgpu::TexelCopyBufferLayout layout //
            {
                .bytesPerRow = width * 4,
                .rowsPerImage = height,
            };

        const wgpu::Extent3D copyExtent //
            {
                .width = width,
                .height = height,
                .depthOrArrayLayers = 1,
            };

        m_Queue.WriteTexture(&dest, data.data(), data.size(), &layout, &copyExtent);
    }

    return texture;
}

wgpu::Texture
WebGpu::CreateTexture(const uint32_t width,
    const uint32_t height,
    std::span<std::span<const std::byte>> data,
    const std::string_view name) const
{
    GG_ASSERT(IsReady());

    GG_ASSERT(width > 0 && height > 0);
    GG_ASSERT(!data.empty(), "Texture array data cannot be empty");

    const wgpu::TextureDescriptor desc //
        {
            .label = name,
            .usage = wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::CopyDst,
            .dimension = wgpu::TextureDimension::e2D,
            .size //
            {
                .width = width,
                .height = height,
                .depthOrArrayLayers = scast<uint32_t>(data.size()),
            },
            .format = kDefaultTextureFormat,
            .mipLevelCount = 1,
            .sampleCount = 1,
        };

    const wgpu::Texture texture = m_Device.CreateTexture(&desc);
    GG_ASSERT(texture);

    for(size_t i = 0; i < data.size(); ++i)
    {
        GG_ASSERT(4uz * width * height == data[i].size(),
            "Texture data size does not match width * height * 4 at layer: {}", i);

        const wgpu::TexelCopyTextureInfo dest //
            { .texture = texture, .mipLevel = 0, .origin = { .x = 0, .y = 0, .z = scast<uint32_t>(i) } };

        const wgpu::TexelCopyBufferLayout layout //
            {
                .bytesPerRow = width * 4,
                .rowsPerImage = height,
            };

        const wgpu::Extent3D copyExtent //
            {
                .width = width,
                .height = height,
                .depthOrArrayLayers = 1,
            };

        m_Queue.WriteTexture(&dest, data[i].data(), data[i].size(), &layout, &copyExtent);
    }

    return texture;
}

wgpu::Buffer
WebGpu::CreateStorageBuffer(const size_t size, const std::string_view name) const
{
    GG_ASSERT(IsReady());

    GG_ASSERT(size > 0);

    const wgpu::BufferDescriptor desc //
        {
            .label = name,
            .usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Storage,
            .size = size,
        };

    return m_Device.CreateBuffer(&desc);
}

wgpu::Buffer
WebGpu::CreateUniformBuffer(const size_t size, const std::string_view name) const
{
    GG_ASSERT(IsReady());

    GG_ASSERT(size > 0);

    const wgpu::BufferDescriptor desc //
        {
            .label = name,
            .usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Uniform,
            .size = size,
        };

    return m_Device.CreateBuffer(&desc);
}

wgpu::Sampler
WebGpu::GetDefaultSampler() const
{
    GG_ASSERT(IsReady());
    return m_DefaultSampler;
}

Result<wgpu::Texture>
WebGpu::GetSwapchainTexture() const
{
    GG_CHECKB(IsReady(), "WebGPU is not ready");

    uint32_t width{}, height{};
    GG_CHECK(TryGetWindowDims(m_Window, width, height), "Failed to get window dimensions");

    wgpu::SurfaceTexture surfaceTexture;
    m_Surface.GetCurrentTexture(&surfaceTexture);

    switch(surfaceTexture.status)
    {
        case wgpu::SurfaceGetCurrentTextureStatus::SuccessSuboptimal:
            GG_WARN(
                "GetCurrentTexture() returned SuccessSuboptimal - the surface may need to be reconfigured");
            [[fallthrough]];
        case wgpu::SurfaceGetCurrentTextureStatus::SuccessOptimal:
            if(width != surfaceTexture.texture.GetWidth()
                || height != surfaceTexture.texture.GetHeight())
            {
                GG_INFO("Window size changed - reconfiguring surface...");
            }
            else
            {
                return surfaceTexture.texture;
            }
            break;
        case wgpu::SurfaceGetCurrentTextureStatus::Timeout:
            GG_WARN("Reconfiguring surface: Timeout...");
            break;
        case wgpu::SurfaceGetCurrentTextureStatus::Outdated:
            GG_WARN("Reconfiguring surface: Outdated...");
            break;
        case wgpu::SurfaceGetCurrentTextureStatus::Lost:
            GG_WARN("Reconfiguring surface: Lost...");
            break;
        case wgpu::SurfaceGetCurrentTextureStatus::Error:
            GG_ERROR("Failed to acquire current surface texture: Unknown error");
            return Result<>::Fail();
            break;
    }

    // Attempt to reconfigure the surface and acquire the texture again
    m_Surface.Unconfigure();

    auto surfaceFormat = ConfigureSurface(m_Adapter, m_Device, m_Surface, width, height);
    GG_CHECK(surfaceFormat, "Failed to configure WebGPU surface");

    // GetCurrentTexture writes directly to the provided surfaceTexture structure
    // without releasing the previous texture. This ensures we start with a clean state.
    surfaceTexture = {};

    m_Surface.GetCurrentTexture(&surfaceTexture);

    GG_CHECK(surfaceTexture.status == wgpu::SurfaceGetCurrentTextureStatus::SuccessOptimal
            || surfaceTexture.status == wgpu::SurfaceGetCurrentTextureStatus::SuccessSuboptimal,
        "Failed to acquire current surface texture after reconfiguration");

    return surfaceTexture.texture;
}

Result<>
WebGpu::Present() const
{
    GG_CHECKB(IsReady(), "WebGPU is not ready");

#if !defined(__EMSCRIPTEN__)

    const wgpu::Status status = m_Surface.Present();
    GG_CHECK(status == wgpu::Status::Success, "Failed to present current surface texture");

#endif // !defined(__EMSCRIPTEN__)

    return Result<>::Success();
}

// private:

void
WebGpu::OnAdapterRequestedCb(wgpu::RequestAdapterStatus status,
    wgpu::Adapter adapter,
    wgpu::StringView message,
    WebGpu* context)
{
    if(context->m_Stage == Stage::Stopping)
    {
        return;
    }
    if(status != wgpu::RequestAdapterStatus::Success || !adapter)
    {
        GG_ERROR("WebGPU adapter request failed: {}", std::string_view(message));
        context->m_Stage = Stage::Failed;
        return;
    }
    context->m_Adapter = std::move(adapter);
    context->m_Stage = Stage::AdapterReady;
}

void
WebGpu::OnDeviceLostCb(
    const wgpu::Device&, wgpu::DeviceLostReason reason, wgpu::StringView message, WebGpu* context)
{
    const char* reasonString = nullptr;
    switch(reason)
    {
        case wgpu::DeviceLostReason::Unknown:
            reasonString = "Unknown";
            break;
        case wgpu::DeviceLostReason::Destroyed:
            reasonString = "Destroyed";
            break;
        case wgpu::DeviceLostReason::CallbackCancelled:
            reasonString = "CallbackCancelled";
            break;
        case wgpu::DeviceLostReason::FailedCreation:
            reasonString = "FailedCreation";
            break;
    }

    GG_ERROR("WebGPU device lost ({}): {}", reasonString, std::string_view(message));

    if(context->m_Stage == Stage::Stopping)
    {
        return;
    }

    context->m_Stage = Stage::Failed;
}

void
WebGpu::OnUncapturedErrorCb(
    const wgpu::Device&, wgpu::ErrorType errorType, wgpu::StringView message)
{
    const char* errorTypeString = nullptr;
    switch(errorType)
    {
        case wgpu::ErrorType::NoError:
            errorTypeString = "NoError";
            break;
        case wgpu::ErrorType::Validation:
            errorTypeString = "Validation";
            break;
        case wgpu::ErrorType::OutOfMemory:
            errorTypeString = "OutOfMemory";
            break;
        case wgpu::ErrorType::Internal:
            errorTypeString = "Internal";
            break;
        case wgpu::ErrorType::Unknown:
            errorTypeString = "Unknown";
            break;
    }

    GG_ERROR("WebGPU error ({}): {}", errorTypeString, std::string_view(message));

    // When a debugger is attached trigger a BP so we can inspect the error
    GG_DEBUG_BREAK;
}

void
WebGpu::OnDeviceRequestedCb(wgpu::RequestDeviceStatus status,
    wgpu::Device device,
    wgpu::StringView message,
    WebGpu* context)
{
    if(context->m_Stage == Stage::Stopping || context->m_Stage == Stage::Failed)
    {
        if(device)
        {
            device.Destroy();
        }
        return;
    }
    if(status != wgpu::RequestDeviceStatus::Success || !device)
    {
        GG_ERROR("WebGPU device request failed: {}", std::string_view(message));
        context->m_Stage = Stage::Failed;
        return;
    }
    context->m_Device = std::move(device);
    context->m_Queue = context->m_Device.GetQueue();
    if(!context->m_Queue)
    {
        GG_ERROR("Failed to get WebGPU queue");
        context->m_Stage = Stage::Failed;
        return;
    }
    context->m_Stage = Stage::DeviceReady;
    GG_INFO("WebGPU device ready");
}
