#include "Scene.h"

#include "CellMap.h"
#include "SpriteLib.h"

#include <iterator>
#include <webgpu/webgpu_cpp.h>

namespace
{

struct CellMapParams
{
    float viewRow{};              // Camera view row in world space
    float viewCol{};              // Camera view column in world space
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
            .label = "Scene",
            .colorAttachmentCount = 1,
            .colorAttachments = &attachment,
        };

    return cmdEncoder.BeginRenderPass(&renderPassDesc);
}

wgpu::BindGroup
CreateSpriteShaderBindGroup(wgpu::Device gpuDevice,
    wgpu::BindGroupLayout layout,
    wgpu::Texture texture,
    wgpu::Sampler sampler,
    wgpu::Buffer spriteStateBuffer)
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
                .buffer = spriteStateBuffer,
            },
        };

    const wgpu::BindGroupDescriptor desc //
        {
            .label = "SpriteShaderBindGroup",
            .layout = layout,
            .entryCount = std::size(entries),
            .entries = &entries[0],
        };

    return gpuDevice.CreateBindGroup(&desc);
}

wgpu::BindGroup
CreateTileShaderBindGroup(wgpu::Device gpuDevice,
    wgpu::BindGroupLayout layout,
    wgpu::Texture texture,
    wgpu::Sampler sampler,
    wgpu::Buffer cellMapParamsBuffer,
    wgpu::Buffer cellOccupancyBuffer)
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
            {
                .binding = 3,
                .buffer = cellOccupancyBuffer,
            },
        };

    const wgpu::BindGroupDescriptor desc //
        {
            .label = "TileShaderBindGroup",
            .layout = layout,
            .entryCount = std::size(entries),
            .entries = &entries[0],
        };

    return gpuDevice.CreateBindGroup(&desc);
}
} // namespace

Result<Scene>
Scene::Create(const uint32_t logicalDisplayWidth,
    const uint32_t logicalDisplayHeight,
    const WebGpu& webGpu,
    const SpriteLib& spriteLib,
    const CellMap& cellMap,
    BoundedVector<LostSoul>&& lostSouls)
{
    return Scene{ logicalDisplayWidth,
        logicalDisplayHeight,
        webGpu,
        spriteLib,
        cellMap,
        std::move(lostSouls) };
}

Scene::Scene(const uint32_t logicalDisplayWidth,
    const uint32_t logicalDisplayHeight,
    const WebGpu& webGpu,
    const SpriteLib& spriteLib,
    const CellMap& cellMap,
    BoundedVector<LostSoul>&& lostSouls)
    : m_LostSouls(std::move(lostSouls)),
      m_SpriteStates(m_LostSouls.size()),
      m_Visibility(m_LostSouls.size()),
      m_LogicalDisplayWidth(logicalDisplayWidth),
      m_LogicalDisplayHeight(logicalDisplayHeight),
      m_WebGpu(&webGpu),
      m_SpriteLib(&spriteLib),
      m_CellMap(&cellMap)
{
}

void
Scene::SetFrameAndDir(uint32_t index, uint32_t frame, uint32_t dir)
{
    if(!GG_VERIFYB(index < m_SpriteStates.size(), "Invalid sprite index"))
    {
        return;
    }

    SpriteState& spriteState = m_SpriteStates[index];

    spriteState.frame = frame;
    spriteState.dir = dir;
    m_Visibility[index] = true;
}

void
Scene::SetView(const float viewRow, const float viewCol)
{
    m_ViewRow = viewRow;
    m_ViewCol = viewCol;
}

Result<>
Scene::Render()
{
    auto texResult = m_WebGpu->GetSwapchainTexture();
    GG_CHECK(texResult, "Failed to acquire swapchain texture");
    const wgpu::Texture renderTarget = *texResult;

    const wgpu::Device gpuDevice = m_WebGpu->GetDevice();
    const wgpu::CommandEncoder encoder = gpuDevice.CreateCommandEncoder();
    GG_CHECK(encoder, "Failed to create command encoder");

    GG_CHECK(EnsurePipelines(renderTarget), "Failed to ensure pipeline");
    GG_CHECK(EnsureSpriteStateBuffer(), "Failed to ensure sprite state buffer");
    GG_CHECK(EnsureCellMapParamsBuffer(), "Failed to ensure cell map params buffer");
    GG_CHECK(EnsureCellOccupancyBuffer(), "Failed to ensure cell occupancy buffer");
    GG_CHECK(EnsureBindGroups(), "Failed to ensure bind groups");

    // Update the sprite state buffer with the current sprite states
    m_WebGpu->GetQueue().WriteBuffer(m_SpriteStateBuffer,
        0,
        m_SpriteStates.data(),
        sizeof(SpriteState) * m_SpriteStates.size());

    const wgpu::RenderPassEncoder renderPass = CreateRenderPassEncoder(encoder, renderTarget);
    GG_CHECK(renderPass, "Failed to create render pass");

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

    renderPass.SetBindGroup(0, m_TileBindGroup);
    renderPass.SetPipeline(m_TilePipeline);

    const uint32_t instanceCount = m_CellMap->GetGridSize() * m_CellMap->GetGridSize();

    // NOLINTNEXTLINE(readability-magic-numbers)
    renderPass.Draw(6, instanceCount * 3 / 2, 0, 0);

    renderPass.SetBindGroup(0, m_SpriteBindGroup);
    renderPass.SetPipeline(m_SpritePipeline);

    for(size_t instanceIndex = 0; instanceIndex < m_SpriteStates.size(); ++instanceIndex)
    {
        if(!m_Visibility[instanceIndex])
        {
            continue;
        }
        // NOLINTNEXTLINE(readability-magic-numbers)
        renderPass.Draw(6, 1, 0, static_cast<uint32_t>(instanceIndex));
    }

    renderPass.End();

    const wgpu::CommandBuffer commandBuffer = encoder.Finish();
    GG_CHECK(commandBuffer, "Failed to finish command buffer");

    m_WebGpu->GetQueue().Submit(1, &commandBuffer);

    return Result<>::Success();
}

// private:

Result<>
Scene::EnsurePipelines(const wgpu::Texture renderTarget)
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
Scene::EnsureSpriteStateBuffer()
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
Scene::EnsureCellMapParamsBuffer()
{
    if(!m_CellMapParamsBuffer)
    {
        m_CellMapParamsBuffer =
            m_WebGpu->CreateUniformBuffer(sizeof(CellMapParams), "CellMapParams");
        GG_CHECK(m_CellMapParamsBuffer, "Failed to create cell map params buffer");
    }

    const CellMapParams cellMapParams //
        {
            .viewRow = m_ViewRow,
            .viewCol = m_ViewCol,
        };
    m_WebGpu->GetQueue().WriteBuffer(m_CellMapParamsBuffer,
        0,
        &cellMapParams,
        sizeof(cellMapParams));

    return Result<>::Success();
}

Result<>
Scene::EnsureCellOccupancyBuffer()
{
    if(!m_CellOccupancyBuffer)
    {
        const size_t cellCount =
            static_cast<size_t>(m_CellMap->GetGridSize()) * m_CellMap->GetGridSize();
        const size_t byteCount = (cellCount + 7) / 8;
        m_CellOccupancyBuffer = m_WebGpu->CreateStorageBuffer(byteCount, "CellOccupancy");
        GG_CHECK(m_CellOccupancyBuffer, "Failed to create cell occupancy buffer");
    }

    return Result<>::Success();
}

Result<>
Scene::EnsureBindGroups()
{
    if(!m_SpriteBindGroup)
    {
        m_SpriteBindGroup = CreateSpriteShaderBindGroup(m_WebGpu->GetDevice(),
            m_SpritePipeline.GetBindGroupLayout(0),
            m_SpriteLib->GetTexture(),
            m_WebGpu->GetDefaultSampler(),
            m_SpriteStateBuffer);

        GG_CHECK(m_SpriteBindGroup, "Failed to create bind group");
    }

    if(!m_TileBindGroup)
    {
        m_TileBindGroup = CreateTileShaderBindGroup(m_WebGpu->GetDevice(),
            m_TilePipeline.GetBindGroupLayout(0),
            m_CellMap->GetTileSetTexture(),
            m_WebGpu->GetDefaultSampler(),
            m_CellMapParamsBuffer,
            m_CellOccupancyBuffer);

        GG_CHECK(m_TileBindGroup, "Failed to create bind group");
    }

    return Result<>::Success();
}