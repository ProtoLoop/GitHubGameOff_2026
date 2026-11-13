#include "Renderer.h"

#include "CellMap.h"

#include <iterator>
#include <webgpu/webgpu_cpp.h>

namespace
{

struct CellMapParams
{
    float viewXY[2]{}; // Camera view position in map space
    uint32_t cellMapCellsPerSide{ 0 }; // Number of cells along each axis in the map
    uint32_t pad1{ 0 };
};

struct ShaderInfo
{
    std::string_view Label;
    const char* Code;
};

#include "Shaders/SpriteShader.wgsl"
#include "Shaders/TileMapShader.wgsl"

const ShaderInfo kSpriteShaderInfo{ .Label = "SpriteShader", .Code = kSpriteShaderCode };
const ShaderInfo kTileMapShaderInfo{ .Label = "TileMapShader", .Code = kTileMapShaderCode };

constexpr const char* kVertexEntry = "vs_main";
/// Entry point for the fragment shader when outputting to a non-sRGB framebuffer.
constexpr const char* kFragmentEntry_non_srgb = "fs_main_non_srgb";
/// Entry point for the fragment shader when outputting to an sRGB framebuffer.
constexpr const char* kFragmentEntry_srgb = "fs_main_srgb";

wgpu::ShaderModule
CreateShaderModule(const wgpu::Device& gpuDevice, const ShaderInfo& shaderInfo)
{
    const wgpu::StringView shaderCode{ shaderInfo.Code };
    const wgpu::ShaderSourceWGSL wgsl{ { .code = shaderCode } };
    const wgpu::ShaderModuleDescriptor shaderModelDesc{ .nextInChain = &wgsl,
        .label = shaderInfo.Label };

    return gpuDevice.CreateShaderModule(&shaderModelDesc);
}

Result<wgpu::RenderPipeline>
CreateRenderPipeline(const wgpu::Device& gpuDevice,
    wgpu::TextureFormat targetFormat,
    const ShaderInfo& shaderInfo,
    const wgpu::PipelineLayout& pipelineLayout,
    const std::span<const wgpu::ConstantEntry> vsConstantEntries)
{
    static constexpr wgpu::BlendState blendState //
    {
        .color =
        {
            .operation = wgpu::BlendOperation::Add,
            .srcFactor = wgpu::BlendFactor::SrcAlpha,
            .dstFactor = wgpu::BlendFactor::OneMinusSrcAlpha,
        },
        .alpha =
        {
            .operation = wgpu::BlendOperation::Add,
            .srcFactor = wgpu::BlendFactor::One,
            .dstFactor = wgpu::BlendFactor::Zero,
        },
    };

    const wgpu::ColorTargetState colorTargetState //
        {
            .format = targetFormat,
            .blend = &blendState,
            .writeMask = wgpu::ColorWriteMask::All,
        };

    const char* fragmentEntry = nullptr;

    const wgpu::ShaderModule shader = CreateShaderModule(gpuDevice, shaderInfo);
    GG_CHECK(shader, "Failed to create shader module");

    if(wgpu::TextureFormat::RGBA8UnormSrgb == targetFormat
        || wgpu::TextureFormat::BGRA8UnormSrgb == targetFormat)
    {
        fragmentEntry = kFragmentEntry_srgb;
    }
    else
    {
        fragmentEntry = kFragmentEntry_non_srgb;
    }

    const wgpu::FragmentState fragmentState //
        {
            .module = shader,
            .entryPoint = fragmentEntry,
            .targetCount = 1,
            .targets = &colorTargetState,
        };

    const wgpu::RenderPipelineDescriptor desc//
    {
        .label = shaderInfo.Label,
        .layout = pipelineLayout,
        .vertex =
        {
            .module = shader,
            .entryPoint = kVertexEntry,
            .constantCount = static_cast<uint32_t>(vsConstantEntries.size()),
            .constants = vsConstantEntries.data(),
            .bufferCount = 0,
            .buffers = nullptr,
        },
        .primitive =
        {
            .topology = wgpu::PrimitiveTopology::TriangleList,
            .stripIndexFormat = wgpu::IndexFormat::Undefined,
            .frontFace = wgpu::FrontFace::CW,
            .cullMode = wgpu::CullMode::Back,
            .unclippedDepth = false,
        },
        .depthStencil = nullptr, // No depth/stencil for this pipeline
        .multisample =
        {
            .count = 1,
            .mask = 0xFFFFFFFF,
            .alphaToCoverageEnabled = false,
        },
        .fragment = &fragmentState,
    };

    return gpuDevice.CreateRenderPipeline(&desc);
}

wgpu::RenderPassEncoder
CreateRenderPassEncoder(wgpu::CommandEncoder cmdEncoder, wgpu::Texture renderTarget)
{
    const wgpu::RenderPassColorAttachment attachment //
        {
            .view = renderTarget.CreateView(),
            .loadOp = wgpu::LoadOp::Clear,
            .storeOp = wgpu::StoreOp::Store,
            .clearValue = { .r = 0.0f, .g = 0.0f, .b = 0.0f, .a = 1.0f },
        };

    const wgpu::RenderPassDescriptor renderPassDesc //
        {
            .label = "Renderer",
            .colorAttachmentCount = 1,
            .colorAttachments = &attachment,
        };

    return cmdEncoder.BeginRenderPass(&renderPassDesc);
}

wgpu::BindGroup
CreateSpriteShaderBindGroup0(wgpu::Device gpuDevice,
    wgpu::BindGroupLayout layout,
    wgpu::Buffer spriteStateBuffer,
    wgpu::Buffer cellMapParamsBuffer)
{
    const wgpu::BindGroupEntry entries[] //
        { {
              .binding = 0,
              .buffer = spriteStateBuffer,
          },
            {
                .binding = 1,
                .buffer = cellMapParamsBuffer,
            } };

    const wgpu::BindGroupDescriptor desc //
        {
            .label = "SpriteShaderBindGroup0",
            .layout = layout,
            .entryCount = std::size(entries),
            .entries = &entries[0],
        };

    return gpuDevice.CreateBindGroup(&desc);
}

wgpu::BindGroup
CreateMapShaderBindGroup0(
    wgpu::Device gpuDevice, wgpu::BindGroupLayout layout, wgpu::Buffer cellMapParamsBuffer)
{
    const wgpu::BindGroupEntry entries[] //
        {
            {
                .binding = 0,
                .buffer = cellMapParamsBuffer,
            },
        };

    const wgpu::BindGroupDescriptor desc //
        {
            .label = "MapShaderBindGroup0",
            .layout = layout,
            .entryCount = std::size(entries),
            .entries = &entries[0],
        };

    return gpuDevice.CreateBindGroup(&desc);
}
} // namespace

Result<std::unique_ptr<Renderer>>
Renderer::Create(const Params& params, const WebGpu& webGpu)
{
    return std::unique_ptr<Renderer>(new Renderer{ params, webGpu });
}

Renderer::Renderer(const Params& params, const WebGpu& webGpu)
    : m_Params(params),
      m_WebGpu(&webGpu)
{
}

Result<wgpu::BindGroup>
Renderer::CreateSpriteTextureBindGroup(const wgpu::Texture& texture)
{
    GG_CHECK(EnsurePipelines(), "Failed to ensure pipelines");

    const wgpu::TextureViewDescriptor viewDesc //
        {
            .dimension = wgpu::TextureViewDimension::e2DArray,
        };

    const wgpu::BindGroupEntry entries[] //
        {
            {
                .binding = 0,
                .textureView = texture.CreateView(&viewDesc),
            },
            {
                .binding = 1,
                .sampler = m_WebGpu->GetDefaultSampler(),
            },
        };

    const wgpu::BindGroupDescriptor desc //
        {
            .label = "SpriteShaderBindGroup1",
            .layout = m_SpritePipeline.GetBindGroupLayout(1),
            .entryCount = std::size(entries),
            .entries = &entries[0],
        };

    return m_WebGpu->GetDevice().CreateBindGroup(&desc);
}

Result<wgpu::BindGroup>
Renderer::CreateTileSetTextureBindGroup(const wgpu::Texture& texture)
{
    GG_CHECK(EnsurePipelines(), "Failed to ensure pipelines");

    const wgpu::TextureViewDescriptor viewDesc //
        {
            .dimension = wgpu::TextureViewDimension::e2DArray,
        };

    const wgpu::BindGroupEntry entries[] //
        {
            {
                .binding = 0,
                .textureView = texture.CreateView(&viewDesc),
            },
            {
                .binding = 1,
                .sampler = m_WebGpu->GetDefaultSampler(),
            },
        };

    const wgpu::BindGroupDescriptor desc //
        {
            .label = "MapShaderBindGroup1",
            .layout = m_MapPipeline.GetBindGroupLayout(1),
            .entryCount = std::size(entries),
            .entries = &entries[0],
        };

    return m_WebGpu->GetDevice().CreateBindGroup(&desc);
}

Result<>
Renderer::Begin()
{
    GG_CHECKB(!m_CmdEncoder, "Begin() already called");

    m_CmdEncoder = m_WebGpu->GetDevice().CreateCommandEncoder();
    GG_CHECKB(m_CmdEncoder, "Failed to create command encoder");

    return Result<>::Success();
}

Result<>
Renderer::End()
{
    GG_CHECKB(m_CmdEncoder, "End() called outside of Begin()");

    const wgpu::CommandEncoder cmdEncoder = std::move(m_CmdEncoder);

    const wgpu::CommandBuffer commandBuffer = cmdEncoder.Finish();
    GG_CHECK(commandBuffer, "Failed to finish command buffer");

    m_WebGpu->GetQueue().Submit(1, &commandBuffer);

    return Result<>::Success();
}

void
Renderer::SetView(const float viewX, const float viewY)
{
    // The player is in sprite state zero.
    m_SpriteStates[0].mapXY[0] = viewX;
    m_SpriteStates[0].mapXY[1] = viewY;
}

Result<>
Renderer::Render(const CellMap& cellMap, const Sprite& sprite)
{
    GG_CHECKB(m_CmdEncoder, "Render called outside of Begin()/End()");

    auto texResult = m_WebGpu->GetSwapchainTexture();
    GG_CHECK(texResult, "Failed to acquire swapchain texture");
    const wgpu::Texture renderTarget = *texResult;

    const wgpu::RenderPassEncoder renderPass = CreateRenderPassEncoder(m_CmdEncoder, renderTarget);
    GG_CHECK(renderPass, "Failed to create render pass");

    GG_CHECK(EnsureSpriteStateBuffer(), "Failed to ensure sprite state buffer");
    GG_CHECK(EnsureCellMapParamsBuffer(cellMap), "Failed to ensure cell map params buffer");

    GG_CHECK(EnsurePipelines(), "Failed to ensure pipeline");
    GG_CHECK(EnsureBindGroups(), "Failed to ensure bind groups");

    GG_CHECK(SetViewport(renderTarget, renderPass), "Failed to set viewport");
    GG_CHECK(RenderMap(cellMap, renderPass), "Failed to render map");
    GG_CHECK(RenderSprites(sprite, renderPass), "Failed to render sprites");

    renderPass.End();

    return Result<>::Success();
}

// private:

Result<>
Renderer::RenderMap(const CellMap& cellMap, const wgpu::RenderPassEncoder& renderPass)
{
    auto bgResult = cellMap.GetTileSetBindGroup();
    GG_CHECK(bgResult, "Failed to get tile set bind group");

    renderPass.SetBindGroup(0, m_MapBindGroup0);
    renderPass.SetBindGroup(1, *bgResult);
    renderPass.SetPipeline(m_MapPipeline);

    /*// Number of rows/columns of cells visible in the current view.
    // This calculation assumes an isometric projection where the width of the view is
    // increased by twice the height.
    // This also works non-iso but likely overestimates the number of visible cells.
    const float viewWidth =
        static_cast<float>(m_LogicalDisplayWidth) / static_cast<float>(kCellPixelSize);
    const float viewHeight =
        static_cast<float>(m_LogicalDisplayHeight) / static_cast<float>(kCellPixelSize);
    const float cellsPerAxis = std::ceil(viewWidth + (2 * viewHeight)) + 1;
    const uint32_t instanceCount = static_cast<uint32_t>(cellsPerAxis * cellsPerAxis);

    // NOLINTNEXTLINE(readability-magic-numbers)
    renderPass.Draw(6, instanceCount, 0, 0);*/

    // This version just sends all the cells.
    const uint32_t instanceCount = cellMap.GetCellCount();
    // NOLINTNEXTLINE(readability-magic-numbers)
    renderPass.Draw(6, instanceCount * 3 / 2, 0, 0);

    return Result<>::Success();
}

Result<>
Renderer::RenderSprites(const Sprite& sprite, const wgpu::RenderPassEncoder& renderPass)
{
    SpriteState& spriteState = m_SpriteStates[0];
    const Rect& rect = sprite.GetRect();
    spriteState.layer = sprite.GetLayer();
    spriteState.srcRect[0] = static_cast<float>(rect.x);
    spriteState.srcRect[1] = static_cast<float>(rect.y);
    spriteState.srcRect[2] = static_cast<float>(rect.w);
    spriteState.srcRect[3] = static_cast<float>(rect.h);

    // Update the sprite state buffer with the current sprite states
    m_WebGpu->GetQueue().WriteBuffer(m_SpriteStateBuffer,
        0,
        &m_SpriteStates[0],
        sizeof(SpriteState) * std::size(m_SpriteStates));

    renderPass.SetBindGroup(0, m_SpriteBindGroup0);
    renderPass.SetBindGroup(1, sprite.GetTextureBindGroup());
    renderPass.SetPipeline(m_SpritePipeline);

    // NOLINTNEXTLINE(readability-magic-numbers)
    renderPass.Draw(6, 1, 0, 0);
    /*for(size_t instanceIndex = 0; instanceIndex < m_SpriteStates.size(); ++instanceIndex)
    {
        if(!m_Visibility[instanceIndex])
        {
            continue;
        }
        // NOLINTNEXTLINE(readability-magic-numbers)
        renderPass.Draw(6, 1, 0, static_cast<uint32_t>(instanceIndex));
    }*/

    return Result<>::Success();
}

Result<>
Renderer::SetViewport(const wgpu::Texture& renderTarget,
    const wgpu::RenderPassEncoder& renderPass) const
{
    const float ldw = static_cast<float>(m_Params.LogicalDisplayWidth);
    const float ldh = static_cast<float>(m_Params.LogicalDisplayHeight);

    const float rtW = static_cast<float>(renderTarget.GetWidth());
    const float rtH = static_cast<float>(renderTarget.GetHeight());
    const float scale = std::min(rtW / ldw, rtH / ldh);
    const float vpW = ldw * scale;
    const float vpH = ldh * scale;
    const float vpX = (rtW - vpW) / 2;
    const float vpY = (rtH - vpH) / 2;

    renderPass.SetViewport(vpX, vpY, vpW, vpH, 0.0f, 1.0f);

    return Result<>::Success();
}

Result<>
Renderer::EnsurePipelines()
{
    const wgpu::TextureFormat swapchainFormat = m_WebGpu->GetSwapchainFormat();

    const wgpu::ConstantEntry vsConstantEntries[] //
        {
            {
                .key = "logicalTargetWidth",
                .value = static_cast<double>(m_Params.LogicalDisplayWidth),
            },
            {
                .key = "logicalTargetHeight",
                .value = static_cast<double>(m_Params.LogicalDisplayHeight),
            },
            {
                .key = "cellPixelsPerSide",
                .value = static_cast<double>(m_Params.CellPixelsPerSide),
            },
            {
                .key = "sheetTilesPerSide",
                .value = static_cast<double>(m_Params.SheetTilesPerSide),
            },
        };

    if(!m_SpritePipeline || m_RenderTargetFormat != swapchainFormat)
    {
        auto pipelineResult = CreateRenderPipeline(m_WebGpu->GetDevice(),
            swapchainFormat,
            kSpriteShaderInfo,
            nullptr,//*layoutResult,
            vsConstantEntries);
        GG_CHECK(pipelineResult, "Failed to create render pipeline: {}", kSpriteShaderInfo.Label);
        m_SpritePipeline = std::move(*pipelineResult);
    }

    if(!m_MapPipeline || m_RenderTargetFormat != swapchainFormat)
    {
        auto pipelineResult = CreateRenderPipeline(m_WebGpu->GetDevice(),
            swapchainFormat,
            kTileMapShaderInfo,
            nullptr, //*layoutResult,
            vsConstantEntries);
        GG_CHECK(pipelineResult, "Failed to create render pipeline: {}", kTileMapShaderInfo.Label);
        m_MapPipeline = std::move(*pipelineResult);
    }

    m_RenderTargetFormat = swapchainFormat;

    return Result<>::Success();
}

Result<>
Renderer::EnsureSpriteStateBuffer()
{
    const size_t bufferSize = sizeof(SpriteState) * std::size(m_SpriteStates);

    if(!m_SpriteStateBuffer || m_SpriteStateBuffer.GetSize() < bufferSize)
    {
        m_SpriteStateBuffer =
            m_WebGpu->CreateStorageBuffer(static_cast<uint32_t>(bufferSize), "SpriteStateBuffer");
        GG_CHECK(m_SpriteStateBuffer, "Failed to create sprite state buffer");
    }

    return Result<>::Success();
}

Result<>
Renderer::EnsureCellMapParamsBuffer(const CellMap& cellMap)
{
    if(!m_CellMapParamsBuffer)
    {
        m_CellMapParamsBuffer =
            m_WebGpu->CreateUniformBuffer(sizeof(CellMapParams), "CellMapParams");
        GG_CHECK(m_CellMapParamsBuffer, "Failed to create cell map params buffer");
    }

    const SpriteState& spriteState = m_SpriteStates[0];

    const CellMapParams cellMapParams //
        {
            .viewXY = { spriteState.mapXY[0], spriteState.mapXY[1] },
            .cellMapCellsPerSide = cellMap.GetCellsPerSide(),
        };
    m_WebGpu->GetQueue().WriteBuffer(m_CellMapParamsBuffer,
        0,
        &cellMapParams,
        sizeof(cellMapParams));

    return Result<>::Success();
}

Result<>
Renderer::EnsureBindGroups()
{
    if(!m_SpriteBindGroup0)
    {
        m_SpriteBindGroup0 = CreateSpriteShaderBindGroup0(m_WebGpu->GetDevice(),
            m_SpritePipeline.GetBindGroupLayout(0),
            m_SpriteStateBuffer,
            m_CellMapParamsBuffer);

        GG_CHECK(m_SpriteBindGroup0, "Failed to create sprite ");
    }

    if(!m_MapBindGroup0)
    {
        m_MapBindGroup0 = CreateMapShaderBindGroup0(m_WebGpu->GetDevice(),
            m_MapPipeline.GetBindGroupLayout(0),
            m_CellMapParamsBuffer);

        GG_CHECK(m_MapBindGroup0, "Failed to create map bind group 0");
    }

    return Result<>::Success();
}