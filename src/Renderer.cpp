#include "Renderer.h"

#include "CellMap.h"

#include <iterator>
#include <webgpu/webgpu_cpp.h>

namespace
{

struct CellMapParams
{
    float viewXY[2]{}; // Camera view position in map space
    uint32_t pad0{ 0 };
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
        .layout = nullptr,
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
        {
            {
                .binding = 0,
                .buffer = spriteStateBuffer,
            },
            {
                .binding = 1,
                .buffer = cellMapParamsBuffer,
            }
        };

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
CreateSpriteTextureBindGroup(wgpu::Device gpuDevice,
    wgpu::BindGroupLayout layout,
    wgpu::Texture texture,
    wgpu::Sampler sampler)
{
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
                .sampler = sampler,
            },
        };

    const wgpu::BindGroupDescriptor desc //
        {
            .label = "SpriteShaderBindGroup1",
            .layout = layout,
            .entryCount = std::size(entries),
            .entries = &entries[0],
        };

    return gpuDevice.CreateBindGroup(&desc);
}

wgpu::BindGroup
CreateTileShaderBindGroup0(wgpu::Device gpuDevice,
    wgpu::BindGroupLayout layout,
    wgpu::Texture texture,
    wgpu::Sampler sampler,
    wgpu::Buffer cellMapParamsBuffer)
{
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
                .sampler = sampler,
            },
            {
                .binding = 2,
                .buffer = cellMapParamsBuffer,
            },
        };

    const wgpu::BindGroupDescriptor desc //
        {
            .label = "TileShaderBindGroup0",
            .layout = layout,
            .entryCount = std::size(entries),
            .entries = &entries[0],
        };

    return gpuDevice.CreateBindGroup(&desc);
}
} // namespace

Result<std::unique_ptr<Renderer>>
Renderer::Create(const uint32_t logicalDisplayWidth,
    const uint32_t logicalDisplayHeight,
    const WebGpu& webGpu,
    const CellMap& cellMap,
    BoundedVector<LostSoul>&& lostSouls)
{
    return std::unique_ptr<Renderer>(new Renderer{ logicalDisplayWidth,
        logicalDisplayHeight,
        webGpu,
        cellMap,
        std::move(lostSouls) });
}

Renderer::Renderer(const uint32_t logicalDisplayWidth,
    const uint32_t logicalDisplayHeight,
    const WebGpu& webGpu,
    const CellMap& cellMap,
    BoundedVector<LostSoul>&& lostSouls)
    : m_LostSouls(std::move(lostSouls)),
      m_SpriteStates(m_LostSouls.size()),
      m_Visibility(m_LostSouls.size()),
      m_LogicalDisplayWidth(logicalDisplayWidth),
      m_LogicalDisplayHeight(logicalDisplayHeight),
      m_WebGpu(&webGpu),
      m_CellMap(&cellMap)
{
}

Result<>
Renderer::BeginFrame()
{
    GG_CHECKB(!m_CmdEncoder, "BeginFrame already active");

    auto texResult = m_WebGpu->GetSwapchainTexture();
    GG_CHECK(texResult, "Failed to acquire swapchain texture");
    const wgpu::Texture renderTarget = *texResult;

    GG_CHECK(EnsurePipelines(renderTarget), "Failed to ensure pipeline");
    GG_CHECK(EnsureSpriteStateBuffer(), "Failed to ensure sprite state buffer");
    GG_CHECK(EnsureCellMapParamsBuffer(), "Failed to ensure cell map params buffer");
    GG_CHECK(EnsureBindGroups(), "Failed to ensure bind groups");

    m_CmdEncoder = m_WebGpu->GetDevice().CreateCommandEncoder();
    GG_CHECKB(m_CmdEncoder, "Failed to create command encoder");
    
    return Result<>::Success();
}

Result<>
Renderer::EndFrame()
{
    GG_CHECKB(m_CmdEncoder, "EndFrame called without an active BeginFrame");

    const wgpu::CommandEncoder cmdEncoder = std::move(m_CmdEncoder);

    const wgpu::CommandBuffer commandBuffer = cmdEncoder.Finish();
    GG_CHECK(commandBuffer, "Failed to finish command buffer");

    m_WebGpu->GetQueue().Submit(1, &commandBuffer);
    
    return Result<>::Success();
}

void
Renderer::SetView(const float viewX, const float viewY)
{
    m_ViewX = viewX;
    m_ViewY = viewY;

    // The player is in sprite state zero.
    m_SpriteStates[0].mapXY[0] = m_ViewX;
    m_SpriteStates[0].mapXY[1] = m_ViewY;
}

Result<>
Renderer::Render(const Sprite& sprite)
{
    GG_CHECKB(m_CmdEncoder, "Render called without an active BeginFrame");
    
    auto texResult = m_WebGpu->GetSwapchainTexture();
    GG_CHECK(texResult, "Failed to acquire swapchain texture");
    const wgpu::Texture renderTarget = *texResult;

    const wgpu::RenderPassEncoder renderPass = CreateRenderPassEncoder(m_CmdEncoder, renderTarget);
    GG_CHECK(renderPass, "Failed to create render pass");

    GG_CHECK(SetViewport(renderTarget, renderPass), "Failed to set viewport");
    GG_CHECK(RenderMap(renderPass), "Failed to render map");
    GG_CHECK(RenderSprites(renderPass, sprite), "Failed to render sprites");

    renderPass.End();

    return Result<>::Success();
}

// private:

Result<>
Renderer::RenderMap(const wgpu::RenderPassEncoder& renderPass)
{
    renderPass.SetBindGroup(0, m_TileBindGroup);
    renderPass.SetPipeline(m_TilePipeline);

    const uint32_t instanceCount = m_CellMap->GetGridSize() * m_CellMap->GetGridSize();

    // NOLINTNEXTLINE(readability-magic-numbers)
    renderPass.Draw(6, instanceCount * 3 / 2, 0, 0);

    return Result<>::Success();
}

Result<>
Renderer::RenderSprites(const wgpu::RenderPassEncoder& renderPass, const Sprite& sprite)
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
        m_SpriteStates.data(),
        sizeof(SpriteState) * m_SpriteStates.size());

    auto bg = CreateSpriteTextureBindGroup(m_WebGpu->GetDevice(),
        m_SpritePipeline.GetBindGroupLayout(1),
        sprite.GetTexture(),
        m_WebGpu->GetDefaultSampler());

    GG_CHECK(bg, "Failed to create bind group");

    renderPass.SetBindGroup(0, m_SpriteStatesBindGroup);
    renderPass.SetBindGroup(1, bg);
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
Renderer::SetViewport(const wgpu::Texture& renderTarget, const wgpu::RenderPassEncoder& renderPass) const
{
    const float ldw = static_cast<float>(m_LogicalDisplayWidth);
    const float ldh = static_cast<float>(m_LogicalDisplayHeight);

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
Renderer::EnsurePipelines(const wgpu::Texture& renderTarget)
{
    const wgpu::ConstantEntry vsConstantEntries[] //
        {
            { .key = "logicalTargetWidth", .value = static_cast<double>(m_LogicalDisplayWidth) },
            { .key = "logicalTargetHeight", .value = static_cast<double>(m_LogicalDisplayHeight) },
            { .key = "cellPixelSize", .value = 64.0 },
            { .key = "sheetGridSize", .value = 1.0 },
            { .key = "cellMapGridSize", .value = static_cast<double>(m_CellMap->GetGridSize()) },
        };

    if(!m_SpritePipeline || m_PipelineFormat != renderTarget.GetFormat())
    {
        auto pipelineResult = CreateRenderPipeline(m_WebGpu->GetDevice(),
            renderTarget.GetFormat(),
            kSpriteShaderInfo,
            vsConstantEntries);
        GG_CHECK(pipelineResult, "Failed to create render pipeline: {}", kSpriteShaderInfo.Label);
        m_SpritePipeline = std::move(*pipelineResult);
    }

    if(!m_TilePipeline || m_PipelineFormat != renderTarget.GetFormat())
    {
        auto pipelineResult = CreateRenderPipeline(m_WebGpu->GetDevice(),
            renderTarget.GetFormat(),
            kTileMapShaderInfo,
            vsConstantEntries);
        GG_CHECK(pipelineResult, "Failed to create render pipeline: {}", kTileMapShaderInfo.Label);
        m_TilePipeline = std::move(*pipelineResult);
    }

    m_PipelineFormat = renderTarget.GetFormat();

    return Result<>::Success();
}

Result<>
Renderer::EnsureSpriteStateBuffer()
{
    const size_t bufferSize = sizeof(SpriteState) * m_SpriteStates.size();

    if(!m_SpriteStateBuffer || m_SpriteStateBuffer.GetSize() < bufferSize)
    {
        m_SpriteStateBuffer =
            m_WebGpu->CreateStorageBuffer(static_cast<uint32_t>(bufferSize), "SpriteStateBuffer");
        GG_CHECK(m_SpriteStateBuffer, "Failed to create sprite state buffer");
    }

    return Result<>::Success();
}

Result<>
Renderer::EnsureCellMapParamsBuffer()
{
    if(!m_CellMapParamsBuffer)
    {
        m_CellMapParamsBuffer =
            m_WebGpu->CreateUniformBuffer(sizeof(CellMapParams), "CellMapParams");
        GG_CHECK(m_CellMapParamsBuffer, "Failed to create cell map params buffer");
    }

    const CellMapParams cellMapParams //
        {
            .viewXY = { m_ViewX, m_ViewY },
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
    if(!m_SpriteStatesBindGroup)
    {
        m_SpriteStatesBindGroup = CreateSpriteShaderBindGroup0(m_WebGpu->GetDevice(),
            m_SpritePipeline.GetBindGroupLayout(0),
            m_SpriteStateBuffer,
            m_CellMapParamsBuffer);

        GG_CHECK(m_SpriteStatesBindGroup, "Failed to create bind group");
    }

    if(!m_TileBindGroup)
    {
        m_TileBindGroup = CreateTileShaderBindGroup0(m_WebGpu->GetDevice(),
            m_TilePipeline.GetBindGroupLayout(0),
            m_CellMap->GetTileSetTexture(),
            m_WebGpu->GetDefaultSampler(),
            m_CellMapParamsBuffer);

        GG_CHECK(m_TileBindGroup, "Failed to create bind group");
    }

    return Result<>::Success();
}