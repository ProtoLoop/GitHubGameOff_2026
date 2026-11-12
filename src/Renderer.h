#pragma once

#include "BoundedVector.h"
#include "Sprite.h"
#include "WebGpu.h"

#include <webgpu/webgpu_cpp.h>
#include <vector>

class CellMap;
class SpriteLib;

struct LostSoul
{
};

class Renderer
{
public:
    static Result<std::unique_ptr<Renderer>> Create(const uint32_t logicalDisplayWidth,
        const uint32_t logicalDisplayHeight,
        const WebGpu& webGpu,
        const CellMap& cellMap,
        BoundedVector<LostSoul>&& lostSouls);

    Renderer(const Renderer&) = delete;
    ~Renderer() = default;
    Renderer& operator=(const Renderer&) = delete;
    Renderer(Renderer&&) = delete;
    Renderer& operator=(Renderer&&) = delete;

    Result<> BeginFrame();
    Result<> EndFrame();

    void SetView(const float viewX, const float viewY);

    Result<> Render(const Sprite& sprite);

private:
    struct SpriteState
    {        
        float mapXY[2]{};       // Map space cell coordinates of the sprite.
        uint32_t layer{ 0 };    // Layer of the texture array containing the sprite.
        uint32_t pad0{ 0 };     // Padding to align the structure to 16 bytes.
        float srcRect[4]{};     // Source rectangle in the texture (x, y, w, h)

    };

    Renderer(const uint32_t logicalDisplayWidth,
        const uint32_t logicalDisplayHeight,
        const WebGpu& webGpu,
        const CellMap& cellMap,
        BoundedVector<LostSoul>&& lostSouls);

    Result<> RenderMap(const wgpu::RenderPassEncoder& renderPass);

    Result<> RenderSprites(const wgpu::RenderPassEncoder& renderPass, const Sprite& sprite);

    Result<> SetViewport(const wgpu::Texture& renderTarget, const wgpu::RenderPassEncoder& renderPass) const;
    Result<> EnsurePipelines(const wgpu::Texture& renderTarget);
    Result<> EnsureSpriteStateBuffer();
    Result<> EnsureCellMapParamsBuffer();
    Result<> EnsureBindGroups();

    BoundedVector<LostSoul> m_LostSouls;
    std::vector<SpriteState> m_SpriteStates;
    std::vector<bool> m_Visibility;

    float m_ViewY{ 0 };
    float m_ViewX{ 0 };

    uint32_t m_LogicalDisplayWidth{ 0 };
    uint32_t m_LogicalDisplayHeight{ 0 };
    const WebGpu* m_WebGpu{ nullptr };
    const CellMap* m_CellMap{ nullptr };
    wgpu::TextureFormat m_PipelineFormat{ wgpu::TextureFormat::Undefined };
    wgpu::Buffer m_SpriteStateBuffer;
    wgpu::Buffer m_CellMapParamsBuffer;
    wgpu::RenderPipeline m_SpritePipeline;
    wgpu::BindGroup m_SpriteStatesBindGroup;
    
    wgpu::RenderPipeline m_TilePipeline;
    wgpu::BindGroup m_TileBindGroup;

    wgpu::CommandEncoder m_CmdEncoder;
};