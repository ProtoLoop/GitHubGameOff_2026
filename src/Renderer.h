#pragma once

#include "Nodes.h"
#include "Result.h"

#include <webgpu/webgpu_cpp.h>

class CellMap;
class Node2D;
class Level;
class Sprite;
class WebGpu;

struct SpriteDrawRecord
{
    Transform2D Transform;
    Sprite Sprite;
    size_t SortTieBreaker{};
};

class Renderer
{
public:
    struct Params
    {
        uint32_t LogicalDisplayWidth;  // Logical width of the display in pixels
        uint32_t LogicalDisplayHeight; // Logical height of the display in pixels
        uint32_t CellPixelsPerSide;    // Number of pixels along each axis of a cell.
        uint32_t SheetTilesPerSide;    // Number of tiles along each axis in of a tile sheet.
    };

    static Result<std::unique_ptr<Renderer>> Create(const Params& params, const WebGpu& webGpu);

    Renderer() = delete;
    Renderer(const Renderer&) = delete;
    ~Renderer() = default;
    Renderer& operator=(const Renderer&) = delete;
    Renderer(Renderer&&) = delete;
    Renderer& operator=(Renderer&&) = delete;

    Result<wgpu::BindGroup> CreateSpriteTextureBindGroup(const wgpu::Texture& texture);

    Result<wgpu::BindGroup> CreateTileSetTextureBindGroup(const wgpu::Texture& texture);

    void SetView(const float viewX, const float viewY);

    Result<> Render(const Level& level);

private:
    struct SpriteState
    {
        float mapXY[2]{};    // Map space cell coordinates of the sprite.
        uint32_t pad0{ 0 };  // Padding to align the structure to 16 bytes.
        uint32_t pad1{ 0 };  // Padding to align the structure to 16 bytes.
        float srcRect[4]{};  // Source rectangle in the texture (x, y, w, h)
    };

    struct CellMapParams
    {
        float viewXY[2]{};                 // Camera view position in map space
        uint32_t cellMapCellsPerSide{ 0 }; // Number of cells along each axis in the map
        uint32_t pad1{ 0 };
    };

    Renderer(const Params& params, const WebGpu& webGpu);

    Result<> RenderMap(const CellMap& cellMap, const wgpu::RenderPassEncoder& renderPass);

    Result<> RenderSprites(const wgpu::RenderPassEncoder& renderPass);

    Result<> SetViewport(const wgpu::Texture& renderTarget,
        const wgpu::RenderPassEncoder& renderPass) const;
    Result<> EnsurePipelines();
    Result<> EnsureSpriteStateBuffer(const size_t size);
    Result<> EnsureCellMapParamsBuffer();
    Result<> EnsureSpriteBindGroups();
    Result<> EnsureCellMapBindGroups();

    std::vector<SpriteState> m_SpriteStates;
    std::vector<SpriteDrawRecord> m_SpriteDrawRecords;
    CellMapParams m_CellMapParams;

    Params m_Params;
    const WebGpu* m_WebGpu{ nullptr };
    wgpu::TextureFormat m_RenderTargetFormat{ wgpu::TextureFormat::Undefined };
    wgpu::Buffer m_SpriteStateBuffer;
    wgpu::Buffer m_CellMapParamsBuffer;

    wgpu::RenderPipeline m_SpritePipeline;
    wgpu::BindGroup m_SpriteBindGroup0;

    wgpu::RenderPipeline m_MapPipeline;
    wgpu::BindGroup m_MapBindGroup0;
};