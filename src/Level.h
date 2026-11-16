#pragma once

#include "CellMap.h"
#include "Nodes.h"
#include "Sprite.h"

#include <memory>
#include <span>
#include <string_view>
#include <vector>

struct SpriteDrawRecord;
class Level;

class LevelLoader
{
public:
    LevelLoader() = delete;
    ~LevelLoader() = default;
    LevelLoader(const LevelLoader&) = delete;
    LevelLoader& operator=(const LevelLoader&) = delete;
    LevelLoader(LevelLoader&&) = default;
    LevelLoader& operator=(LevelLoader&&) = default;

    static Result<LevelLoader> Load(const std::span<const SpriteAnimDef> spriteAnimDefs,
        const CellMapDef& cellMapDef,
        FileFetcher& fileFetcher,
        Renderer& renderer,
        WebGpu& webGpu);

    bool IsPending() const;

    Result<> Update();

    Result<std::unique_ptr<Level>> Take();

private:
    LevelLoader(CellMapLoader&& cellMapLoader, BoundedVector<SpriteAnimLoader>&& spriteAnimLoaders)
        : m_CellMapLoader(std::move(cellMapLoader)),
          m_SpriteAnimLoaders(std::move(spriteAnimLoaders))
    {
    }

    CellMapLoader m_CellMapLoader;
    BoundedVector<SpriteAnimLoader> m_SpriteAnimLoaders;

    bool m_Taken{ false };
};

/// Owns the cell map, sprite animations, and node storage.
/// Level cannot be copied or moved because owned node handles retain its address.
/// Transfer ownership through std::unique_ptr<Level>.
/// All owned node handles must be released before the level is destroyed.
class Level
{
public:
    static constexpr size_t kMaxNodes = 1024;
    static constexpr size_t kMaxSpriteAnimNodes = 1024;

    Level() = delete;
    ~Level();
    Level(const Level&) = delete;
    Level& operator=(const Level&) = delete;
    Level(Level&&) = delete;
    Level& operator=(Level&&) = delete;

    void Tick(const float deltaTime);

    const CellMap& GetCellMap() const;

    Result<const SpriteAnim&> GetSpriteAnim(const std::string_view name) const;

    Result<SpriteAnim::Instance> CreateSpriteAnimInstance(const std::string_view name) const;

    Result<OwnedNodeHandle<Node2D>> CreateNode2D();

    Result<OwnedNodeHandle<SpriteAnimNode>> CreateSpriteAnimNode(const BorrowedHandle<Node2D>& node2D,
        const SpriteAnim::Instance& instance);

    void Destroy(OwnedNodeHandle<Node2D>& handle);

    void Destroy(OwnedNodeHandle<SpriteAnimNode>& handle);

    std::span<const SpriteAnimNode> GetSpriteAnimNodes() const;

private:

    friend class LevelLoader;

    Level(CellMap&& cellMap, BoundedVector<SpriteAnim>&& spriteAnims);

    template<typename T>
    void DestroyNode(BoundedVector<T>& nodes, OwnedNodeHandle<T>& handle)
    {
        if(!GG_VERIFYB(handle.m_Slot, "Invalid handle slot"))
        {
            return;
        }

        T* node = handle.operator->();
        if(!GG_VERIFYB(node, "Invalid node"))
        {
            return;
        }

        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        T* back = nodes.data() + nodes.size() - 1;

        // Make sure the node we're destroying is within the valid range of nodes.
        if(!GG_VERIFYB(node >= nodes.data() && node <= back, "Node out of range"))
        {
            return;
        }

        // Move the last node to the position of the node being destroyed.

        // Find the handle slot corresponding to the node at the back.
        // so we can fix up its pointer after moving the node.
        auto it = std::ranges::find(m_HandleSlots, back, &HandleSlot::m_Ptr);
        if(!GG_VERIFYB(it != m_HandleSlots.end(), "Handle slot not found"))
        {
            return;
        }

        // Move the last node to the position of the node being destroyed.
        if(node != back)
        {
            *node = std::move(*back);
        }

        // Update the handle slot to point to the new location of the node.
        it->m_Ptr = node;

        // Deallocate the node we just moved from the back.
        nodes.pop_back();

        FreeHandleSlot(handle.m_Slot);

        handle.m_Owner = nullptr;
        handle.m_Slot = nullptr;
        handle.m_Generation = 0;
    }

    Result<HandleSlot*> AllocateHandleSlot();
    void FreeHandleSlot(HandleSlot* handleSlot);

    CellMap m_CellMap;
    BoundedVector<SpriteAnim> m_SpriteAnims;
    BoundedVector<Node2D> m_Nodes{kMaxNodes};
    BoundedVector<SpriteAnimNode> m_SpriteAnimNodes{kMaxSpriteAnimNodes};
    BoundedVector<HandleSlot> m_HandleSlots{kMaxNodes + kMaxSpriteAnimNodes};
    BoundedVector<HandleSlot*> m_FreeHandleSlots{kMaxNodes + kMaxSpriteAnimNodes};
};
