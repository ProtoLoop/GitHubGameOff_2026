#include "Renderer.h"

#include "Defer.h"
#include "Level.h"
#include "WebGpu.h"

#include <glm/ext/vector_float4.hpp>
#include <glm/mat2x2.hpp>
#include <string_view>
#include <webgpu/webgpu_cpp.h>

namespace
{

struct ShaderInfo
{
    std::string_view Label;
    const char* Code;
};

#include "Shaders/RectangleShader.wgsl"
#include "Shaders/SpriteShader.wgsl"
#include "Shaders/TileMapShader.wgsl"

const ShaderInfo kSpriteShaderInfo{ .Label = "SpriteShader", .Code = kSpriteShaderCode };
const ShaderInfo kSpriteShadowShaderInfo{ .Label = "SpriteShadowShader",
    .Code = kSpriteShaderCode };
const ShaderInfo kTileMapShaderInfo{ .Label = "TileMapShader", .Code = kTileMapShaderCode };
const ShaderInfo kRectangleShaderInfo{ .Label = "RectangleShader", .Code = kRectangleShaderCode };

constexpr uint32_t kVerticesPerQuad = 6;

constexpr const char* kVertexEntry = "vs_main";
/// Entry point for the fragment shader when outputting to a non-sRGB framebuffer.
constexpr const char* kFragmentEntry_non_srgb = "fs_main_non_srgb";
/// Entry point for the fragment shader when outputting to an sRGB framebuffer.
constexpr const char* kFragmentEntry_srgb = "fs_main_srgb";

// Key used to identify the shadow pass pipeline override.
constexpr const char* kPipelineOverrideKey_shadowPass = "shadowPass";

/// 1:1 Orthographic projection matrix.
[[maybe_unused]] constexpr glm::mat2x2 kOrthographicProj //
    {
        glm::vec2(1, 0), // +X east -> right
        glm::vec2(0, 1), // +Y north -> up
    };

/// 2:1 (45 degree yaw, 30 degree pitch) isometric projection matrix.
[[maybe_unused]] constexpr glm::mat2x2 kIsometricProj //
    {
        glm::vec2(0.5, -0.25), // +X east -> right/down
        glm::vec2(0.5, 0.25),  // +Y north -> right/up
    };

/// 3/4 (0 degree yaw, 45 degree pitch) projection matrix.
[[maybe_unused]] constexpr glm::mat2x2 kThreeQuarterProj //
    {
        glm::vec2(1.0f, 0.0f),        // +X east -> right
        glm::vec2(0.0f, 0.70710678f), // +Y north -> up
    };

wgpu::ShaderModule
CreateShaderModule(const wgpu::Device& gpuDevice, const ShaderInfo& shaderInfo)
{
    const wgpu::StringView shaderCode{ shaderInfo.Code };
    const wgpu::ShaderSourceWGSL wgsl{ { .code = shaderCode } };
    const wgpu::ShaderModuleDescriptor shaderModelDesc{ .nextInChain = &wgsl,
        .label = shaderInfo.Label };

    return gpuDevice.CreateShaderModule(&shaderModelDesc);
}

Result<wgpu::BindGroupLayout>
CreateSpriteShaderBindGroupLayout0(const wgpu::Device& gpuDevice)
{
    const wgpu::BindGroupLayoutEntry entries[] = {
        {
            .binding = 0,
            .visibility = wgpu::ShaderStage::Vertex,
            .buffer = { .type = wgpu::BufferBindingType::ReadOnlyStorage },
        },
        {
            .binding = 1,
            .visibility = wgpu::ShaderStage::Vertex,
            .buffer = { .type = wgpu::BufferBindingType::Uniform },
        },
    };
    const wgpu::BindGroupLayoutDescriptor bgLayoutDesc //
        {
            .label = "SpriteShaderBindGroup0",
            .entryCount = std::size(entries),
            .entries = &entries[0],
        };

    return gpuDevice.CreateBindGroupLayout(&bgLayoutDesc);
}

Result<wgpu::BindGroupLayout>
CreateSpriteShaderBindGroupLayout1(const wgpu::Device& gpuDevice)
{
    const wgpu::BindGroupLayoutEntry entries[] = {
        {
            .binding = 0,
            .visibility = wgpu::ShaderStage::Fragment | wgpu::ShaderStage::Vertex,
            .texture = //
            {
                .sampleType = wgpu::TextureSampleType::Float,
                .viewDimension = wgpu::TextureViewDimension::e2DArray,
            },
        },
        {
            .binding = 1,
            .visibility = wgpu::ShaderStage::Fragment,
            .sampler = { .type = wgpu::SamplerBindingType::Filtering },
        },
    };
    const wgpu::BindGroupLayoutDescriptor bgLayoutDesc //
        {
            .label = "SpriteShaderBindGroup1",
            .entryCount = std::size(entries),
            .entries = &entries[0],
        };

    return gpuDevice.CreateBindGroupLayout(&bgLayoutDesc);
}

Result<wgpu::PipelineLayout>
CreateSpriteShaderRenderPipelineLayout(const wgpu::Device& gpuDevice)
{
    auto bg0LayoutResult = CreateSpriteShaderBindGroupLayout0(gpuDevice);
    GG_CHECK(bg0LayoutResult, "Failed to create bind group layout 0");
    auto bg1LayoutResult = CreateSpriteShaderBindGroupLayout1(gpuDevice);
    GG_CHECK(bg1LayoutResult, "Failed to create bind group layout 1");

    const wgpu::BindGroupLayout bindGroupLayouts[] //
        {
            *bg0LayoutResult,
            *bg1LayoutResult,
        };

    const wgpu::PipelineLayoutDescriptor desc //
        {
            .label = "RenderPipelineLayout",
            .bindGroupLayoutCount = std::size(bindGroupLayouts),
            .bindGroupLayouts = &bindGroupLayouts[0],
        };

    return gpuDevice.CreatePipelineLayout(&desc);
}

Result<wgpu::RenderPipeline>
CreateRenderPipeline(const wgpu::Device& gpuDevice,
    wgpu::TextureFormat targetFormat,
    const ShaderInfo& shaderInfo,
    const wgpu::PipelineLayout& pipelineLayout,
    const std::span<const wgpu::ConstantEntry> vsConstantEntries)
{
    wgpu::FrontFace frontFace = wgpu::FrontFace::CW;

    // Determine if this render pipeline is for the shadow pass
    // because triangle winding is reversed for the shadow pass.
    for(const wgpu::ConstantEntry& entry : vsConstantEntries)
    {
        if(std::string_view(entry.key) == kPipelineOverrideKey_shadowPass && entry.value != 0)
        {
            frontFace = wgpu::FrontFace::CCW;
            break;
        }
    }

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
            .constantCount = static_cast<uint32_t>(vsConstantEntries.size()),
            .constants = vsConstantEntries.data(),
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
            .frontFace = frontFace,
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
CreateRenderPassEncoder(const wgpu::CommandEncoder& cmdEncoder,
    const wgpu::Texture& renderTarget,
    const wgpu::TextureFormat targetFormat)
{
    const wgpu::TextureViewDescriptor viewDesc //
        {
            .format = targetFormat,
        };

    const wgpu::RenderPassColorAttachment attachment //
        {
            .view = renderTarget.CreateView(&viewDesc),
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
CreateSpriteShaderBindGroup0(const wgpu::Device& gpuDevice,
    const wgpu::BindGroupLayout& layout,
    const wgpu::Buffer& spriteStateBuffer,
    const wgpu::Buffer& cellMapParamsBuffer)
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
CreateMapShaderBindGroup0(const wgpu::Device& gpuDevice,
    const wgpu::BindGroupLayout& layout,
    const wgpu::Buffer& cellMapParamsBuffer)
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

wgpu::BindGroup
CreateRectangleShaderBindGroup0(const wgpu::Device& gpuDevice,
    const wgpu::BindGroupLayout& layout,
    const wgpu::Buffer& rectangleStateBuffer,
    const wgpu::Buffer& cellMapParamsBuffer)
{
    const wgpu::BindGroupEntry entries[] //
        { {
              .binding = 0,
              .buffer = rectangleStateBuffer,
          },
            {
                .binding = 1,
                .buffer = cellMapParamsBuffer,
            } };

    const wgpu::BindGroupDescriptor desc //
        {
            .label = "RectangleShaderBindGroup0",
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

uint32_t
Renderer::GetCellPixelsPerSide() const
{
    return m_Params.CellPixelsPerSide;
}
Renderer::ProjectionType
// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
Renderer::GetProjectionType() const
{
    return ProjectionType::ThreeQuarter;
}

const glm::mat2x2&
Renderer::GetProjectionMatrix() const
{
    switch(GetProjectionType())
    {
        case ProjectionType::Orthographic:
            return kOrthographicProj;
        case ProjectionType::Isometric:
            return kIsometricProj;
        case ProjectionType::ThreeQuarter:
            return kThreeQuarterProj;
    }
}

Result<>
Renderer::QueueSprite(const SpriteFrame& spriteFrame, const Transform2D& transform)
{
    GG_CHECKB(m_SpriteDrawQueue.size() < m_SpriteDrawQueue.capacity(),
        "Exceeded sprite draw queue");

    m_SpriteDrawQueue.push_back( //
        {
            .Transform = transform,
            .SpriteFrame = spriteFrame,
            .SortTieBreaker = m_SpriteDrawQueue.size(),
        });

    return Result<>::Ok;
}

Result<>
Renderer::QueueRectangle(
    const Rectf& rectangle, const Transform2D& transform, const glm::vec4& color)
{
    GG_CHECKB(m_RectangleDrawQueue.size() < m_RectangleDrawQueue.capacity(),
        "Exceeded rectangle draw queue");

    m_RectangleDrawQueue.push_back( //
        {
            .Transform = transform,
            .Rectangle = rectangle,
            .color = color,
        });

    return Result<>::Ok;
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

void
Renderer::SetView(const float viewX, const float viewY)
{
    m_CellMapParams.viewXY.x = viewX;
    m_CellMapParams.viewXY.y = viewY;
}

Result<>
Renderer::Render(const Level& level)
{
    auto cleanup = GG_DEFER
    {
        m_SpriteStates.clear();
        m_RectangleStates.clear();
        m_SpriteDrawQueue.clear();
        m_RectangleDrawQueue.clear();
    };

    const wgpu::CommandEncoder cmdEncoder = m_WebGpu->GetDevice().CreateCommandEncoder();
    GG_CHECKB(cmdEncoder, "Failed to create command encoder");

    auto renderTargetResult = m_WebGpu->GetSwapchainTexture();
    GG_CHECK(renderTargetResult, "Failed to acquire swapchain texture");
    const wgpu::Texture& renderTarget = *renderTargetResult;

    const wgpu::RenderPassEncoder renderPass = CreateRenderPassEncoder(cmdEncoder,
        renderTarget,
        m_WebGpu->GetSwapchainTextureViewFormat());
    GG_CHECK(renderPass, "Failed to create render pass");

    GG_CHECK(SetViewport(renderTarget, renderPass), "Failed to set viewport");

    GG_CHECK(EnsurePipelines(), "Failed to ensure pipeline");

    GG_CHECK(CommitSpriteDrawQueueToGpu(), "Failed to commit sprite draw queue to GPU");

    GG_CHECK(RenderCellMap(level.GetCellMap(), renderPass), "Failed to render map");
    GG_CHECK(RenderShadows(renderPass), "Failed to render shadows");
    GG_CHECK(RenderSprites(renderPass), "Failed to render sprites");
    GG_CHECK(RenderRectangles(renderPass), "Failed to render rectangles");

    renderPass.End();

    const wgpu::CommandBuffer commandBuffer = cmdEncoder.Finish();
    GG_CHECK(commandBuffer, "Failed to finish command buffer");

    m_WebGpu->GetQueue().Submit(1, &commandBuffer);

    return Result<>::Ok;
}

// private:

Result<>
Renderer::CommitSpriteDrawQueueToGpu()
{
    if(m_SpriteDrawQueue.empty())
    {
        return Result<>::Ok;
    }

    if(GetProjectionType() == ProjectionType::Isometric)
    {
        // Sort so that sprites render in the correct order for an isometric projection.
        std::ranges::sort(m_SpriteDrawQueue,
            [](const SpriteDrawRecord& a, const SpriteDrawRecord& b)
            {
                const float aval = a.Transform.Pos.y - a.Transform.Pos.x;
                const float bval = b.Transform.Pos.y - b.Transform.Pos.x;

                if(aval != bval)
                {
                    return aval > bval;
                }
                return std::less<>()(a.SortTieBreaker, b.SortTieBreaker);
            });
    }
    else
    {
        // Sort so that sprites render in the correct order for a top-down projection.
        std::ranges::sort(m_SpriteDrawQueue,
            [](const SpriteDrawRecord& a, const SpriteDrawRecord& b)
            {
                const float aval = a.Transform.Pos.y;
                const float bval = b.Transform.Pos.y;

                if(aval != bval)
                {
                    return aval > bval;
                }
                return std::less<>()(a.SortTieBreaker, b.SortTieBreaker);
            });
    }

    GG_ASSERT(m_SpriteStates.empty());

    // Copy sprite data to GPU
    for(const SpriteDrawRecord& rec : m_SpriteDrawQueue)
    {
        const SpriteFrame& spriteFrame = rec.SpriteFrame;
        GpuSpriteState& spriteState = m_SpriteStates.emplace_back();

        spriteState.cellXY = rec.Transform.Pos;

        spriteState.pivot = spriteFrame.GetPivot();

        spriteState.tint = spriteFrame.GetTint();

        const Rect& srcRect = spriteFrame.GetSrcRect();

        spriteState.srcRect = glm::vec4{ srcRect.Origin(), srcRect.Size() };

        spriteState.dstRect =
            glm::vec4{ -spriteFrame.GetPivot(), srcRect.Size() } * rec.Transform.Scale;
    }

    GG_CHECK(EnsureSpriteStateBuffer(sizeof(GpuSpriteState) * m_SpriteStates.size()),
        "Failed to ensure sprite state buffer");

    // Update the sprite state buffer with the current sprite states
    m_WebGpu->GetQueue().WriteBuffer(m_SpriteStateBuffer,
        0,
        m_SpriteStates.data(),
        sizeof(GpuSpriteState) * std::size(m_SpriteStates));

    return Result<>::Ok;
}

Result<>
Renderer::RenderCellMap(const CellMap& cellMap, const wgpu::RenderPassEncoder& renderPass)
{
    GG_CHECK(EnsureTileMapShaderBuffers(), "Failed to ensure cell map params buffer");
    GG_CHECK(EnsureCellMapBindGroups(), "Failed to ensure cell map bind groups");

    m_CellMapParams.proj = GetProjectionMatrix();
    m_CellMapParams.cellMapCellsPerSide = cellMap.GetCellsPerSide();

    m_WebGpu->GetQueue().WriteBuffer(m_CellMapParamsBuffer,
        0,
        &m_CellMapParams,
        sizeof(m_CellMapParams));

    renderPass.SetBindGroup(0, m_MapBindGroup0);
    renderPass.SetBindGroup(1, cellMap.GetTileSetBindGroup());
    renderPass.SetPipeline(m_MapPipeline);

    const uint32_t instanceCount = cellMap.GetCellsPerLayer();
    for(uint32_t layer = 0; layer < cellMap.GetLayerCount(); ++layer)
    {
        const uint32_t firstInstance = layer * instanceCount;
        renderPass.Draw(kVerticesPerQuad, instanceCount, 0, firstInstance);
    }

    return Result<>::Ok;
}

Result<>
Renderer::RenderShadows(const wgpu::RenderPassEncoder& renderPass)
{
    if(m_SpriteDrawQueue.empty())
    {
        return Result<>::Ok;
    }

    GG_CHECK(EnsureSpriteBindGroups(), "Failed to ensure sprite bind groups");

    renderPass.SetBindGroup(0, m_SpriteBindGroup0);
    renderPass.SetPipeline(m_SpriteShadowPipeline);

    uint32_t instanceIndex = 0;

    for(const SpriteDrawRecord& rec : m_SpriteDrawQueue)
    {
        const SpriteFrame& spriteFrame = rec.SpriteFrame;
        renderPass.SetBindGroup(1, spriteFrame.GetTextureBindGroup());
        renderPass.Draw(kVerticesPerQuad, 1, 0, instanceIndex);
        ++instanceIndex;
    }

    return Result<>::Ok;
}

Result<>
Renderer::RenderSprites(const wgpu::RenderPassEncoder& renderPass)
{
    if(m_SpriteDrawQueue.empty())
    {
        return Result<>::Ok;
    }

    GG_CHECK(EnsureSpriteBindGroups(), "Failed to ensure sprite bind groups");

    renderPass.SetBindGroup(0, m_SpriteBindGroup0);
    renderPass.SetPipeline(m_SpritePipeline);

    uint32_t instanceIndex = 0;

    for(const SpriteDrawRecord& rec : m_SpriteDrawQueue)
    {
        const SpriteFrame& spriteFrame = rec.SpriteFrame;
        renderPass.SetBindGroup(1, spriteFrame.GetTextureBindGroup());
        renderPass.Draw(kVerticesPerQuad, 1, 0, instanceIndex);
        ++instanceIndex;
    }

    return Result<>::Ok;
}

Result<>
Renderer::RenderRectangles(const wgpu::RenderPassEncoder& renderPass)
{
    if(m_RectangleDrawQueue.empty())
    {
        return Result<>::Ok;
    }

    // Copy rectangle data to GPU
    for(const RectangleDrawRecord& rec : m_RectangleDrawQueue)
    {
        const Rectf& rect = rec.Rectangle;
        GpuRectangleState& rectState = m_RectangleStates.emplace_back();

        rectState.xywh = glm::vec4(rect.x, rect.y, rect.w, rect.h);
        rectState.color = rec.color;
        rectState.cellXY = rec.Transform.Pos;
    }

    GG_CHECK(EnsureRectangleStateBuffer(sizeof(GpuRectangleState) * m_RectangleStates.size()),
        "Failed to ensure rectangle state buffer");
    GG_CHECK(EnsureRectangleBindGroups(), "Failed to ensure rectangle bind groups");

    // Update the rectangle state buffer with the current rectangle states
    m_WebGpu->GetQueue().WriteBuffer(m_RectangleStateBuffer,
        0,
        m_RectangleStates.data(),
        sizeof(GpuRectangleState) * std::size(m_RectangleStates));

    renderPass.SetBindGroup(0, m_RectangleBindGroup0);
    renderPass.SetPipeline(m_RectanglePipeline);

    const uint32_t rectangleCount = static_cast<uint32_t>(m_RectangleDrawQueue.size());
    renderPass.Draw(kVerticesPerQuad, rectangleCount, 0, 0);

    return Result<>::Ok;
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

    const float fleft = std::floor(vpX);
    const float ftop = std::floor(vpY);
    const float fright = std::ceil(vpX + vpW);
    const float fbottom = std::ceil(vpY + vpH);

    const uint32_t left = static_cast<uint32_t>(std::max(fleft, 0.0f));
    const uint32_t top = static_cast<uint32_t>(std::max(ftop, 0.0f));
    const uint32_t right = static_cast<uint32_t>(std::min(fright, rtW));
    const uint32_t bottom = static_cast<uint32_t>(std::min(fbottom, rtH));

    renderPass.SetScissorRect(left, top, right - left, bottom - top);

    return Result<>::Ok;
}

namespace
{
enum class ShadowPass : bool
{
    Disabled = false,
    Enabled = true
};

template<size_t N>
std::array<wgpu::ConstantEntry, N>
MakeConstantEntries(const std::pair<const char*, double> (&overrides)[N],
    const ShadowPass shadowPass)
{
    std::array<wgpu::ConstantEntry, N> entries{};
    for(size_t i = 0; i < N; ++i)
    {
        entries[i].key = overrides[i].first;
        entries[i].value = overrides[i].second;
    }

    wgpu::ConstantEntry* shadowPassEntry = nullptr;

    for(wgpu::ConstantEntry& entry : entries)
    {
        if(std::string_view(entry.key) == kPipelineOverrideKey_shadowPass)
        {
            shadowPassEntry = &entry;
            break;
        }
    }

    if(GG_VERIFYB(shadowPassEntry))
    {
        shadowPassEntry->value = shadowPass == ShadowPass::Enabled ? 1.0 : 0.0;
    }

    return entries;
}
} // namespace

Result<>
Renderer::EnsurePipelines()
{
    const wgpu::TextureFormat targetFormat = m_WebGpu->GetSwapchainTextureViewFormat();

    const std::pair<const char*, double> overrides[] //
        {
            { "logicalTargetWidth", static_cast<double>(m_Params.LogicalDisplayWidth) },
            { "logicalTargetHeight", static_cast<double>(m_Params.LogicalDisplayHeight) },
            { "cellPixelsPerSide", static_cast<double>(m_Params.CellPixelsPerSide) },
            { "sheetTilesPerSide", static_cast<double>(m_Params.SheetTilesPerSide) },
            { kPipelineOverrideKey_shadowPass, 0 },
        };

    if(!m_SpriteShaderPipelineLayout)
    {
        auto spriteShaderLayoutResult =
            CreateSpriteShaderRenderPipelineLayout(m_WebGpu->GetDevice());
        GG_CHECK(spriteShaderLayoutResult, "Failed to create sprite shader render pipeline layout");
        m_SpriteShaderPipelineLayout = std::move(*spriteShaderLayoutResult);
    }

    if(!m_SpritePipeline || m_RenderTargetFormat != targetFormat)
    {
        auto constantEntries = MakeConstantEntries(overrides, ShadowPass::Disabled);

        auto pipelineResult = CreateRenderPipeline(m_WebGpu->GetDevice(),
            targetFormat,
            kSpriteShaderInfo,
            m_SpriteShaderPipelineLayout,
            constantEntries);
        GG_CHECK(pipelineResult, "Failed to create render pipeline: {}", kSpriteShaderInfo.Label);
        m_SpritePipeline = std::move(*pipelineResult);
    }

    if(!m_SpriteShadowPipeline || m_RenderTargetFormat != targetFormat)
    {
        auto constantEntries = MakeConstantEntries(overrides, ShadowPass::Enabled);

        auto pipelineResult = CreateRenderPipeline(m_WebGpu->GetDevice(),
            targetFormat,
            kSpriteShadowShaderInfo,
            m_SpriteShaderPipelineLayout,
            constantEntries);
        GG_CHECK(pipelineResult,
            "Failed to create render pipeline: {}",
            kSpriteShadowShaderInfo.Label);
        m_SpriteShadowPipeline = std::move(*pipelineResult);
    }

    if(!m_MapPipeline || m_RenderTargetFormat != targetFormat)
    {
        auto constantEntries = MakeConstantEntries(overrides, ShadowPass::Disabled);

        auto pipelineResult = CreateRenderPipeline(m_WebGpu->GetDevice(),
            targetFormat,
            kTileMapShaderInfo,
            nullptr, //*layoutResult,
            constantEntries);
        GG_CHECK(pipelineResult, "Failed to create render pipeline: {}", kTileMapShaderInfo.Label);
        m_MapPipeline = std::move(*pipelineResult);
    }

    if(!m_RectanglePipeline || m_RenderTargetFormat != targetFormat)
    {
        auto constantEntries = MakeConstantEntries(overrides, ShadowPass::Disabled);

        auto pipelineResult = CreateRenderPipeline(m_WebGpu->GetDevice(),
            targetFormat,
            kRectangleShaderInfo,
            nullptr, //*layoutResult,
            constantEntries);
        GG_CHECK(pipelineResult,
            "Failed to create render pipeline: {}",
            kRectangleShaderInfo.Label);
        m_RectanglePipeline = std::move(*pipelineResult);
    }

    m_RenderTargetFormat = targetFormat;

    return Result<>::Ok;
}

Result<>
Renderer::EnsureSpriteStateBuffer(const size_t size)
{
    if(!m_SpriteStateBuffer || m_SpriteStateBuffer.GetSize() < size)
    {
        m_SpriteStateBuffer =
            m_WebGpu->CreateStorageBuffer(static_cast<uint32_t>(size), "SpriteStateBuffer");
        GG_CHECK(m_SpriteStateBuffer, "Failed to create sprite state buffer");

        // Need to recreate the bind group.
        m_SpriteBindGroup0 = {};
    }

    return Result<>::Ok;
}

Result<>
Renderer::EnsureTileMapShaderBuffers()
{
    if(!m_CellMapParamsBuffer)
    {
        m_CellMapParamsBuffer =
            m_WebGpu->CreateUniformBuffer(sizeof(GpuCellMapParams), "CellMapParams");
        GG_CHECK(m_CellMapParamsBuffer, "Failed to create cell map params buffer");
    }

    return Result<>::Ok;
}

Result<>
Renderer::EnsureRectangleStateBuffer(const size_t size)
{
    if(!m_RectangleStateBuffer || m_RectangleStateBuffer.GetSize() < size)
    {
        m_RectangleStateBuffer =
            m_WebGpu->CreateStorageBuffer(static_cast<uint32_t>(size), "RectangleStateBuffer");
        GG_CHECK(m_RectangleStateBuffer, "Failed to create rectangle state buffer");

        // Need to recreate the bind group.
        m_RectangleBindGroup0 = {};
    }

    return Result<>::Ok;
}

Result<>
Renderer::EnsureSpriteBindGroups()
{
    if(!m_SpriteBindGroup0)
    {
        m_SpriteBindGroup0 = CreateSpriteShaderBindGroup0(m_WebGpu->GetDevice(),
            m_SpritePipeline.GetBindGroupLayout(0),
            m_SpriteStateBuffer,
            m_CellMapParamsBuffer);

        GG_CHECK(m_SpriteBindGroup0, "Failed to create sprite bind group 0");
    }

    return Result<>::Ok;
}

Result<>
Renderer::EnsureCellMapBindGroups()
{
    if(!m_MapBindGroup0)
    {
        m_MapBindGroup0 = CreateMapShaderBindGroup0(m_WebGpu->GetDevice(),
            m_MapPipeline.GetBindGroupLayout(0),
            m_CellMapParamsBuffer);

        GG_CHECK(m_MapBindGroup0, "Failed to create map bind group 0");
    }

    return Result<>::Ok;
}

Result<>
Renderer::EnsureRectangleBindGroups()
{
    if(!m_RectangleBindGroup0)
    {
        m_RectangleBindGroup0 = CreateRectangleShaderBindGroup0(m_WebGpu->GetDevice(),
            m_RectanglePipeline.GetBindGroupLayout(0),
            m_RectangleStateBuffer,
            m_CellMapParamsBuffer);

        GG_CHECK(m_RectangleBindGroup0, "Failed to create rectangle bind group 0");
    }

    return Result<>::Ok;
}