#pragma once

#include "BoundedVector.h"
#include "WebGpu.h"

#include <webgpu/webgpu_cpp.h>
#include <vector>

class SpriteLib;

struct LostSoul
{
};

class Scene
{
public:
    static constexpr uint32_t kLogicalWidth = 800;
    static constexpr uint32_t kLogicalHeight = 600;

    static Result<Scene> Create(
        const WebGpu& webGpu, const SpriteLib& spriteLib, BoundedVector<LostSoul>&& lostSouls);

    void SetFrameAndDir(uint32_t index, uint32_t frame, uint32_t dir);

    Result<> Render();

private:
    struct SpriteState
    {
        uint32_t frame{ 0 };
        uint32_t dir{ 0 };
        uint32_t sheet{ 0 };
    };

    Scene(const WebGpu& webGpu, const SpriteLib& spriteLib, BoundedVector<LostSoul>&& lostSouls);

    Result<> EnsurePipeline(const wgpu::Texture renderTarget);
    Result<> EnsureSpriteStateBuffer();
    Result<> EnsureLogicalTargetSizeBuffer();

    BoundedVector<LostSoul> m_LostSouls;
    std::vector<SpriteState> m_SpriteStates;
    std::vector<bool> m_Visibility;

    const WebGpu* m_WebGpu{ nullptr };
    const SpriteLib* m_SpriteLib{ nullptr };
    wgpu::TextureFormat m_PipelineFormat{ wgpu::TextureFormat::Undefined };
    wgpu::RenderPipeline m_Pipeline;
    wgpu::Buffer m_SpriteStateBuffer;
    wgpu::Buffer m_TargetSizeBuffer;
    wgpu::BindGroup m_BindGroup;
};