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
    struct Params
    {
        uint32_t LogicalDisplayWidth;   // Logical width of the display in pixels
        uint32_t LogicalDisplayHeight;  // Logical height of the display in pixels
        uint32_t CellPixelsPerSide;     // Number of pixels along each axis of a cell.
        uint32_t SheetTilesPerSide;     // Number of tiles along each axis in of a tile sheet.
    };

    static Result<std::unique_ptr<Renderer>> Create(const Params& params,
        const WebGpu& webGpu,
        const CellMap& cellMap,
        BoundedVector<LostSoul>&& lostSouls);

    Renderer(const Renderer&) = delete;
    ~Renderer() = default;
    Renderer& operator=(const Renderer&) = delete;
    Renderer(Renderer&&) = delete;
    Renderer& operator=(Renderer&&) = delete;

    Result<> Begin();
    Result<> End();

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

    Renderer(const Params& params,
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

    Params m_Params;
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