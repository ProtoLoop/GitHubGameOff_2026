#include "Scene.h"

#include "Shortcuts.h"
#include "SpriteLib.h"

#include <iterator>
#include <webgpu/webgpu_cpp.h>

namespace
{
// Code is at the bottom of this file
extern const std::string_view kShaderCode;

constexpr const char* kVertexEntry = "vs_main";
/// Entry point for the fragment shader when outputting to a non-sRGB framebuffer.
constexpr const char* kFragmentEntry_non_srgb = "fs_main_non_srgb";
/// Entry point for the fragment shader when outputting to an sRGB framebuffer.
constexpr const char* kFragmentEntry_srgb = "fs_main_srgb";

wgpu::ShaderModule
CreateShaderModule(const wgpu::Device& gpuDevice)
{
    const wgpu::StringView shaderCode{ kShaderCode };
    const wgpu::StringView label = "Scene::Shader";
    const wgpu::ShaderSourceWGSL wgsl{ { .code = shaderCode } };
    const wgpu::ShaderModuleDescriptor shaderModelDesc{ .nextInChain = &wgsl, .label = label };

    return gpuDevice.CreateShaderModule(&shaderModelDesc);
}

Result<wgpu::RenderPipeline>
CreateRenderPipeline(const wgpu::Device& gpuDevice, wgpu::TextureFormat targetFormat)
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

    const wgpu::ShaderModule shader = CreateShaderModule(gpuDevice);
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
        .label = "Scene",
        .layout = nullptr,
        .vertex =
        {
            .module = shader,
            .entryPoint = kVertexEntry,
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
CreateBindGroup(wgpu::Device gpuDevice,
    wgpu::BindGroupLayout layout,
    wgpu::Texture texture,
    wgpu::Sampler sampler,
    wgpu::Buffer spriteStateBuffer,
    wgpu::Buffer targetSizeBuffer)
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
            {
                .binding = 3,
                .buffer = targetSizeBuffer,
            },
        };

    const wgpu::BindGroupDescriptor desc //
        {
            .layout = layout,
            .entryCount = std::size(entries),
            .entries = &entries[0],
        };

    return gpuDevice.CreateBindGroup(&desc);
}
} // namespace

Result<Scene>
Scene::Create(const WebGpu& webGpu, const SpriteLib& spriteLib, BoundedVector<LostSoul>&& lostSouls)
{
    return Scene{ webGpu, spriteLib, std::move(lostSouls) };
}

Scene::Scene(const WebGpu& webGpu, const SpriteLib& spriteLib, BoundedVector<LostSoul>&& lostSouls)
    : m_LostSouls(std::move(lostSouls)),
      m_SpriteStates(m_LostSouls.size()),
      m_Visibility(m_LostSouls.size()),
      m_WebGpu(&webGpu),
      m_SpriteLib(&spriteLib)
{
}

void Scene::SetFrameAndDir(uint32_t index, uint32_t frame, uint32_t dir)
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

Result<>
Scene::Render()
{
    auto texResult = m_WebGpu->GetSwapchainTexture();
    GG_CHECK(texResult, "Failed to acquire swapchain texture");
    const wgpu::Texture renderTarget = *texResult;

    const wgpu::Device gpuDevice = m_WebGpu->GetDevice();
    const wgpu::CommandEncoder encoder = gpuDevice.CreateCommandEncoder();
    GG_CHECK(encoder, "Failed to create command encoder");

    GG_CHECK(EnsurePipeline(renderTarget), "Failed to ensure pipeline");

    GG_CHECK(EnsureSpriteStateBuffer(), "Failed to ensure sprite state buffer");

    GG_CHECK(EnsureLogicalTargetSizeBuffer(), "Failed to ensure target size buffer");

    if(!m_BindGroup)
    {
        m_BindGroup = CreateBindGroup(gpuDevice,
            m_Pipeline.GetBindGroupLayout(0),
            m_SpriteLib->GetTexture(),
            m_WebGpu->GetDefaultSampler(),
            m_SpriteStateBuffer,
            m_TargetSizeBuffer);

        GG_CHECK(m_BindGroup, "Failed to create bind group");
    }

    // Update the sprite state buffer with the current sprite states
    m_WebGpu->GetQueue().WriteBuffer(m_SpriteStateBuffer,
        0,
        m_SpriteStates.data(),
        sizeof(SpriteState) * m_SpriteStates.size());

    const wgpu::RenderPassEncoder renderPass = CreateRenderPassEncoder(encoder, renderTarget);
    GG_CHECK(renderPass, "Failed to create render pass");

    renderPass.SetBindGroup(0, m_BindGroup);

    renderPass.SetPipeline(m_Pipeline);

    const float rtW = scast<float>(renderTarget.GetWidth());
    const float rtH = scast<float>(renderTarget.GetHeight());
    const float scale = std::min(rtW / kLogicalWidth, rtH / kLogicalHeight);
    const float vpW = kLogicalWidth * scale;
    const float vpH = kLogicalHeight * scale;
    const float vpX = (rtW - vpW)  / 2;
    const float vpY = (rtH - vpH) / 2;

    renderPass.SetViewport(vpX, vpY, vpW, vpH, 0.0f, 1.0f);

    for(size_t instanceIndex = 0; instanceIndex < m_SpriteStates.size(); ++instanceIndex)
    {
        if(!m_Visibility[instanceIndex])
        {
            continue;
        }
        // NOLINTNEXTLINE(readability-magic-numbers)
        renderPass.Draw(6, 1, 0, scast<uint32_t>(instanceIndex));
    }
    renderPass.End();


    const wgpu::CommandBuffer commandBuffer = encoder.Finish();
    GG_CHECK(commandBuffer, "Failed to finish command buffer");

    m_WebGpu->GetQueue().Submit(1, &commandBuffer);

    return Result<>::Success();
}

// private:

Result<>
Scene::EnsurePipeline(const wgpu::Texture renderTarget)
{
    if(!m_Pipeline || m_PipelineFormat != renderTarget.GetFormat())
    {
        m_PipelineFormat = renderTarget.GetFormat();
        auto pipelineResult = CreateRenderPipeline(m_WebGpu->GetDevice(), m_PipelineFormat);
        GG_CHECK(pipelineResult, "Failed to create render pipeline");
        m_Pipeline = std::move(*pipelineResult);
    }

    return Result<>::Success();
}

Result<>
Scene::EnsureSpriteStateBuffer()
{
    const size_t bufferSize = sizeof(SpriteState) * m_SpriteStates.size();

    if(!m_SpriteStateBuffer || m_SpriteStateBuffer.GetSize() < bufferSize)
    {
        m_SpriteStateBuffer =
            m_WebGpu->CreateStorageBuffer(scast<uint32_t>(bufferSize), "SpriteStateBuffer");
        GG_CHECK(m_SpriteStateBuffer, "Failed to create sprite state buffer");
    }

    return Result<>::Success();
}

Result<>
Scene::EnsureLogicalTargetSizeBuffer()
{
    if(!m_TargetSizeBuffer)
    {
        const float targetSize[] //
            {
                static_cast<float>(kLogicalWidth),
                static_cast<float>(kLogicalHeight),
            };

        m_TargetSizeBuffer = m_WebGpu->CreateUniformBuffer(sizeof(targetSize), "LogicalTargetSize");
        GG_CHECK(m_TargetSizeBuffer, "Failed to create target size buffer");

        m_WebGpu->GetQueue().WriteBuffer(m_TargetSizeBuffer, 0, &targetSize[0], sizeof(targetSize));
    }

    return Result<>::Success();
}

namespace
{
const std::string_view kShaderCode = R"(
struct VSOut
{
    @builtin(position) pos : vec4<f32>,
    @location(0)       uv  : vec2<f32>,
    @location(1) @interpolate(flat)       sheet : u32,
};

struct SpriteState
{
    frame: u32,
    dir: u32,
    sheet: u32,
};

@group(0) @binding(0) var texSheets : texture_2d_array<f32>;
@group(0) @binding(1) var smp : sampler;
@group(0) @binding(2) var<storage, read> spriteStates : array<SpriteState>;
@group(0) @binding(3) var<uniform> logicalTargetSize : vec2<f32>;

const rowCount: f32 = 8.0;
const colCount: f32 = 8.0;

const vsout = array<VSOut, 6>
(
    VSOut(vec4<f32>(-1.0, 1.0, 0.0, 1.0), vec2<f32>(0.0, 0.0), 0),
    VSOut(vec4<f32>(1.0, 1.0, 0.0, 1.0), vec2<f32>(1.0/colCount, 0.0), 0),
    VSOut(vec4<f32>(1.0, -1.0, 0.0, 1.0), vec2<f32>(1.0/colCount, 1.0/rowCount), 0),

    VSOut(vec4<f32>(-1.0, 1.0, 0.0, 1.0), vec2<f32>(0.0, 0.0), 0),
    VSOut(vec4<f32>(1.0, -1.0, 0.0, 1.0), vec2<f32>(1.0/colCount, 1.0/rowCount), 0),
    VSOut(vec4<f32>(-1.0, -1.0, 0.0, 1.0), vec2<f32>(0.0, 1.0/rowCount), 0)
);

@vertex
fn vs_main(@builtin(vertex_index) vid : u32, @builtin(instance_index) spriteId: u32) -> VSOut
{
    let texDims = vec2<f32>(textureDimensions(texSheets, 0).xy);
    let spriteSize = vec2<f32>(texDims.x / colCount, texDims.y / rowCount);
    var v = vsout[vid];

    let state = spriteStates[spriteId];

    v.pos.xy *= spriteSize / logicalTargetSize;

    v.uv.x += f32(state.frame) / colCount;
    v.uv.y += f32(state.dir) / rowCount;
    v.sheet = state.sheet;

    return v;
}

fn linear_to_srgb(linear: vec4f) -> vec4f
{
    let lo = vec4f(linear.rgb * 12.92, linear.a);
    let hi = vec4f(1.055 * pow(max(linear.rgb, vec3f(0.0)), vec3f(1.0 / 2.4)) - 0.055, linear.a);

    return select(hi, lo, vec4(linear.rgb <= vec3f(0.0031308), true));
}

/// Used when outputting to an sRGB framebuffer.
@fragment
fn fs_main_srgb(i : VSOut) -> @location(0) vec4<f32>
{
    return textureSample(texSheets, smp, i.uv, i.sheet);
}

/// Used when outputting to a non-sRGB framebuffer.
@fragment
fn fs_main_non_srgb(i : VSOut) -> @location(0) vec4<f32>
{
    return linear_to_srgb(textureSample(texSheets, smp, i.uv, i.sheet));
}
)";
} // namespace