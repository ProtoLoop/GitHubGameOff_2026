#include "Scene.h"

#include <webgpu/webgpu_cpp.h>

namespace
{
// Code is at the bottom of this file
extern const std::string_view kShaderCode;

constexpr const char* VertexEntry = "vs_main";
/// Entry point for the fragment shader when outputting to a non-sRGB framebuffer.
constexpr const char* FragmentEntry_non_srgb = "fs_main_non_srgb";
/// Entry point for the fragment shader when outputting to an sRGB framebuffer.
constexpr const char* FragmentEntry_srgb = "fs_main_srgb";

wgpu::ShaderModule
CreateShaderModule(const wgpu::Device& gpuDevice)
{
    const wgpu::StringView shaderCode{ kShaderCode };
    const wgpu::StringView label = "Scene::Shader";
    const wgpu::ShaderSourceWGSL wgsl{ { .code = shaderCode } };
    const wgpu::ShaderModuleDescriptor shaderModelDesc{ .nextInChain = &wgsl, .label = label };

    return gpuDevice.CreateShaderModule(&shaderModelDesc);
}

wgpu::PipelineLayout
CreatePipelineLayout(const wgpu::Device& gpuDevice)
{
    const wgpu::PipelineLayoutDescriptor pipelineLayoutDesc //
        {
            .label = "Scene",
            .bindGroupLayoutCount = 0,
            .bindGroupLayouts = nullptr,
        };

    return gpuDevice.CreatePipelineLayout(&pipelineLayoutDesc);
}

Result<wgpu::RenderPipeline>
CreatePipeline(const wgpu::Device& gpuDevice, wgpu::TextureFormat targetFormat)
{
    static constexpr wgpu::BlendState blendState //
    {
        .color =
        {
            .operation = wgpu::BlendOperation::Add,
            .srcFactor = wgpu::BlendFactor::One,
            .dstFactor = wgpu::BlendFactor::Zero,
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

    const wgpu::PipelineLayout pipelineLayout = CreatePipelineLayout(gpuDevice);
    GG_CHECK(pipelineLayout, "Failed to create pipeline layout");

    if(wgpu::TextureFormat::RGBA8UnormSrgb == targetFormat
        || wgpu::TextureFormat::BGRA8UnormSrgb == targetFormat)
    {
        fragmentEntry = FragmentEntry_srgb;
    }
    else
    {
        fragmentEntry = FragmentEntry_non_srgb;
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
        .layout = pipelineLayout,
        .vertex =
        {
            .module = shader,
            .entryPoint = VertexEntry,
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
} // namespace

Result<Scene>
Scene::Create(WebGpu& /*webGpu*/)
{
    return Scene{};
}

Result<>
Scene::Render(WebGpu& webGpu)
{
    auto texResult = webGpu.GetSwapchainTexture();
    GG_CHECK(texResult, "Failed to acquire swapchain texture");
    const wgpu::Texture renderTarget = *texResult;

    const wgpu::Device gpuDevice = webGpu.GetDevice();
    const wgpu::CommandEncoder encoder = gpuDevice.CreateCommandEncoder();
    GG_CHECK(encoder, "Failed to create command encoder");

    /*if(!m_RenderTarget
        || m_RenderTarget.GetWidth() != width
        || m_RenderTarget.GetHeight() != height)
    {
        m_RenderTarget = webGpu.CreateRenderTarget(width, height);

        GG_CHECK(m_RenderTarget, "Failed to create render target");
    }*/

    if(!m_Pipeline || m_PipelineFormat != renderTarget.GetFormat())
    {
        m_PipelineFormat = renderTarget.GetFormat();
        auto pipelineResult = CreatePipeline(gpuDevice, m_PipelineFormat);
        GG_CHECK(pipelineResult, "Failed to create render pipeline");
        m_Pipeline = std::move(*pipelineResult);
    }

    const wgpu::RenderPassEncoder renderPass = CreateRenderPassEncoder(encoder, renderTarget);
    GG_CHECK(renderPass, "Failed to create render pass");

    renderPass.SetPipeline(m_Pipeline);
    renderPass.Draw(3, 1, 0, 0);
    renderPass.End();

    const wgpu::CommandBuffer commandBuffer = encoder.Finish();
    GG_CHECK(commandBuffer, "Failed to finish command buffer");

    webGpu.GetQueue().Submit(1, &commandBuffer);

    return Result<>::Success();
}

namespace
{
const std::string_view kShaderCode = R"(
struct VSOut
{
    @builtin(position) pos : vec4<f32>,
    @location(0)       uv  : vec2<f32>,
    @location(1)       color : vec3<f32>,
};

//@group(0) @binding(0) var srcTex : texture_2d<f32>;
//@group(0) @binding(1) var srcSmp : sampler;

@vertex
fn vs_main(@builtin(vertex_index) vid : u32) -> VSOut
{
    // NDC positions for fullscreen triangle.
    const vsout = array<VSOut, 3>
    (
        VSOut(vec4<f32>(-1.0, -1.0, 0.0, 1.0), vec2<f32>(-1.0 * 0.5 + 0.5, -1.0 * 0.5 + .5), vec3<f32>(1.0, 0.0, 0.0)),
        VSOut(vec4<f32>(-1.0, 3.0, 0.0, 1.0), vec2<f32>(-1.0 * 0.5 + 0.5,  3.0 * 0.5 + 0.5), vec3<f32>(0.0, 1.0, 0.0)),
        VSOut(vec4<f32>(3.0, -1.0, 0.0, 1.0), vec2<f32>(3.0 * 0.5 + 0.5, -1.0 * 0.5 + 0.5), vec3<f32>(0.0, 0.0, 1.0))
    );

    var o = vsout[vid];

    // Flip v.
    o.uv.y = 1.0 - o.uv.y;

    return o;
}

fn linear_to_srgb(linear: vec3f) -> vec3f
{
    let lo = linear * 12.92;
    let hi = 1.055 * pow(max(linear, vec3f(0.0)), vec3f(1.0 / 2.4)) - 0.055;

    return select(hi, lo,linear <= vec3f(0.0031308));
}

/// Used when outputting to an sRGB framebuffer.
@fragment
fn fs_main_srgb(i : VSOut) -> @location(0) vec4<f32>
{
    //return textureSample(srcTex, srcSmp, i.uv);
    return vec4<f32>(i.color, 1.0);
}

/// Used when outputting to a non-sRGB framebuffer.
@fragment
fn fs_main_non_srgb(i : VSOut) -> @location(0) vec4<f32>
{
    //let p = linear_to_srgb(textureSample(srcTex, srcSmp, i.uv).rgb);
    let p = linear_to_srgb(i.color);
    return vec4<f32>(p, 1.0);
}
)";
} // namespace