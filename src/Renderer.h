#pragma once

#include "InlineVector.h"
#include "Sprite.h"
#include "Transform2D.h"

#include <glm/mat2x2.hpp>
#include <glm/vec2.hpp>
#include <glm/vec4.hpp>
#include <webgpu/webgpu_cpp.h>

class CellMap;
class Level;
class WebGpu;

class Renderer
{
public:

    static constexpr size_t kMaxSpritesPerFrame = 1024;
    static constexpr size_t kMaxRectanglesPerFrame = kMaxSpritesPerFrame * 3 / 2;

    enum class ProjectionType
    {
        Orthographic,   // 1:1 aspect ratio, top-down view (square tiles)
        Isometric,      // 2:1 aspect ratio, angled top-down view (diamond shaped tiles)
        ThreeQuarter    // 3/4 view, commonly used in isometric games (rectangular tiles)
    };

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

    /// Returns the number of pixels along each axis of a cell.
    uint32_t GetCellPixelsPerSide() const;

    /// Returns the current projection type used for rendering.
    ProjectionType GetProjectionType() const;

    /// Returns the current projection matrix used for rendering.
    /// Isometric, orthographic, or 3/4.
    const glm::mat2x2& GetProjectionMatrix() const;

    /// Queues a sprite for the next Render() call.
    /// Transform rotation does not rotate the sprite.
    /// Returns an error if the queue is full.
    Result<> QueueSprite(const SpriteFrame& spriteFrame, const Transform2D& transform);

    /// Queues a rectangle for the next Render() call.
    /// Transform rotation does not rotate the rectangle.
    /// Returns an error if the queue is full.
    Result<> QueueRectangle(const Rectf& rectangle,
        const Transform2D& transform,
        const glm::vec4& color);

    Result<wgpu::BindGroup> CreateSpriteTextureBindGroup(const wgpu::Texture& texture);

    Result<wgpu::BindGroup> CreateTileSetTextureBindGroup(const wgpu::Texture& texture);

    void SetView(const float viewX, const float viewY);

    /// Draws the level and queued sprites.
    /// Clears queued sprites on return, including on failure.
    /// Submit sprites again before the next call.
    Result<> Render(const Level& level);

private:
    static constexpr size_t k16ByteAlignment = 16;

    struct alignas(k16ByteAlignment) GpuSpriteState
    {
        glm::vec2 cellXY{};   // Map space cell coordinates of the sprite.
        glm::vec2 pivot{};   // Pivot point of the sprite.
        glm::vec4 tint{};   // Color tint for the sprite.
        glm::vec4 srcRect{}; // Source rectangle in the texture (x, y, w, h)
        float scale{ 1.0f }; // Scale factor for the sprite.
        uint32_t pad0{ 0 };
        uint32_t pad1{ 0 };
        uint32_t pad2{ 0 };
    };

    struct alignas(k16ByteAlignment) GpuRectangleState
    {
        glm::vec4 xywh{};    // x, y, width, height in pixels
        glm::vec4 color{};   // linear RGBA outline color
        glm::vec2 cellXY{};  // Anchor point of the rectangle in cell coordinates (x, y) - where the pivot point lies in map space
        uint32_t pad0{ 0 };
        uint32_t pad1{ 0 };
    };

    struct alignas(k16ByteAlignment) GpuCellMapParams
    {
        glm::mat2x2 proj{};                 // projection matrix (isometric, orthographic, etc)
        glm::vec2 viewXY{};                 // Camera view position in map space
        uint32_t cellMapCellsPerSide{ 0 };  // Number of cells along each axis in the map
        uint32_t pad1{ 0 };
    };

    struct SpriteDrawRecord
    {
        Transform2D Transform;
        SpriteFrame SpriteFrame;
        size_t SortTieBreaker{};
    };

    struct RectangleDrawRecord
    {
        Transform2D Transform;
        Rectf Rectangle{0,0,0,0};
        glm::vec4 color{1, 0, 1, 1};   // linear RGBA color for the rectangle
    };

    // NOLINTBEGIN(readability-magic-numbers)
    static_assert(offsetof(GpuSpriteState, cellXY) == 0, "SpriteState::cellXY must be at offset 0");
    static_assert(offsetof(GpuSpriteState, pivot) == 8, "SpriteState::pivot must be at offset 8");
    static_assert(offsetof(GpuSpriteState, tint) == 16, "SpriteState::tint must be at offset 16");
    static_assert(offsetof(GpuSpriteState, srcRect) == 32, "SpriteState::srcRect must be at offset 32");
    static_assert(offsetof(GpuSpriteState, scale) == 48, "SpriteState::scale must be at offset 48");

    static_assert(offsetof(GpuRectangleState, xywh) == 0, "RectangleState::xywh must be at offset 0");
    static_assert(offsetof(GpuRectangleState, color) == 16, "RectangleState::color must be at offset 16");
    static_assert(offsetof(GpuRectangleState, cellXY) == 32, "RectangleState::cellXY must be at offset 32");
    static_assert(offsetof(GpuRectangleState, pad0) == 40, "RectangleState::pad0 must be at offset 40");
    static_assert(offsetof(GpuRectangleState, pad1) == 44, "RectangleState::pad1 must be at offset 44");

    static_assert(offsetof(GpuCellMapParams, proj) == 0, "CellMapParams::proj must be at offset 0");
    static_assert(offsetof(GpuCellMapParams, viewXY) == 16, "CellMapParams::viewXY must be at offset 16");
    static_assert(offsetof(GpuCellMapParams, cellMapCellsPerSide) == 24, "CellMapParams::cellMapCellsPerSide must be at offset 24");
    static_assert(offsetof(GpuCellMapParams, pad1) == 28, "CellMapParams::pad1 must be at offset 28");
    // NOLINTEND(readability-magic-numbers)

    Renderer(const Params& params, const WebGpu& webGpu);

    Result<> RenderCellMap(const CellMap& cellMap, const wgpu::RenderPassEncoder& renderPass);

    Result<> RenderSprites(const wgpu::RenderPassEncoder& renderPass);

    Result<> RenderRectangles(const wgpu::RenderPassEncoder& renderPass);

    Result<> SetViewport(const wgpu::Texture& renderTarget,
        const wgpu::RenderPassEncoder& renderPass) const;
    Result<> EnsurePipelines();
    Result<> EnsureSpriteStateBuffer(const size_t size);
    Result<> EnsureTileMapShaderBuffers();
    Result<> EnsureRectangleStateBuffer(const size_t size);
    Result<> EnsureSpriteBindGroups();
    Result<> EnsureCellMapBindGroups();
    Result<> EnsureRectangleBindGroups();

    /// Backing storage for GPU sprite state data.
    InlineVector<GpuSpriteState, kMaxSpritesPerFrame> m_SpriteStates;

    /// Backing storage for GPU rectangle state data.
    InlineVector<GpuRectangleState, kMaxRectanglesPerFrame> m_RectangleStates;

    /// Queue of sprites to be drawn this frame.
    InlineVector<SpriteDrawRecord, kMaxSpritesPerFrame> m_SpriteDrawQueue;

    /// Queue of rectangles to be drawn this frame.
    InlineVector<RectangleDrawRecord, kMaxRectanglesPerFrame> m_RectangleDrawQueue;
    
    GpuCellMapParams m_CellMapParams;

    Params m_Params;
    const WebGpu* m_WebGpu{ nullptr };
    wgpu::TextureFormat m_RenderTargetFormat{ wgpu::TextureFormat::Undefined };
    wgpu::Buffer m_SpriteStateBuffer;
    wgpu::Buffer m_CellMapParamsBuffer;
    wgpu::Buffer m_RectangleStateBuffer;

    wgpu::RenderPipeline m_SpritePipeline;
    wgpu::BindGroup m_SpriteBindGroup0;

    wgpu::RenderPipeline m_MapPipeline;
    wgpu::BindGroup m_MapBindGroup0;

    wgpu::RenderPipeline m_RectanglePipeline;
    wgpu::BindGroup m_RectangleBindGroup0;
};