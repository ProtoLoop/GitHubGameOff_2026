#pragma once

#include "WebGpu.h"
#include <webgpu/webgpu_cpp.h>

class Scene
{
public:

    static Result<Scene> Create(WebGpu& webGpu);

    Result<> Render(WebGpu& webGpu);

private:

    wgpu::Texture m_RenderTarget;
    wgpu::TextureFormat m_PipelineFormat{ wgpu::TextureFormat::Undefined };
    wgpu::RenderPipeline m_Pipeline;

};