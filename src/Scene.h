#pragma once

#include "BoundedVector.h"
#include "WebGpu.h"

#include <webgpu/webgpu_cpp.h>
#include <vector>

class CellMap;
class SpriteLib;

struct LostSoul
{
};

class Scene
{
public:
    static Result<Scene> Create(const uint32_t logicalDisplayWidth,
        const uint32_t logicalDisplayHeight,
        const WebGpu& webGpu,
        const SpriteLib& spriteLib,
        const CellMap& cellMap,
        BoundedVector<LostSoul>&& lostSouls);

    void SetFrameAndDir(uint32_t index, uint32_t frame, uint32_t dir);

    Result<> Render();

private:
    struct SpriteState
    {
        uint32_t frame{ 0 };
        uint32_t dir{ 0 };
        uint32_t sheet{ 0 };
    };

    Scene(const uint32_t logicalDisplayWidth,
        const uint32_t logicalDisplayHeight,
        const WebGpu& webGpu,
        const SpriteLib& spriteLib,
        const CellMap& cellMap,
        BoundedVector<LostSoul>&& lostSouls);

    Result<> EnsurePipelines(const wgpu::Texture renderTarget);
    Result<> EnsureSpriteStateBuffer();
    Result<> EnsureLogicalTargetSizeBuffer();
    Result<> EnsureCellMapParamsBuffer();
    Result<> EnsureBindGroups();

    BoundedVector<LostSoul> m_LostSouls;
    std::vector<SpriteState> m_SpriteStates;
    std::vector<bool> m_Visibility;

    uint32_t m_LogicalDisplayWidth{ 0 };
    uint32_t m_LogicalDisplayHeight{ 0 };
    const WebGpu* m_WebGpu{ nullptr };
    const SpriteLib* m_SpriteLib{ nullptr };
    const CellMap* m_CellMap{ nullptr };
    wgpu::TextureFormat m_PipelineFormat{ wgpu::TextureFormat::Undefined };
    wgpu::Buffer m_SpriteStateBuffer;
    wgpu::Buffer m_TargetSizeBuffer;
    wgpu::Buffer m_CellMapParamsBuffer;
    wgpu::RenderPipeline m_SpritePipeline;
    wgpu::BindGroup m_SpriteBindGroup;

    wgpu::RenderPipeline m_TilePipeline;
    wgpu::BindGroup m_TileBindGroup;
};