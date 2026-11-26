#pragma once

#include "Geometry2D.h"
#include "NameTypes.h"
#include "Transform2D.h"
#include "Result.h"

#include <glm/vec2.hpp>
#include <glm/vec4.hpp>
#include <optional>
#include <span>

/// Defines frames within a rectangular region of a texture.
/// Size specifies each frame's pixel dimensions. Frames advance left to right,
/// then top to bottom. Incomplete columns or rows at the edges are ignored.
struct SpriteFrameSequenceDef
{
    /// The unique name of the sprite frame sequence.
    ResourceName Name;
    /// The file path to the texture containing the sprite frames.
    FilePath TexturePath;
    // If not present then use the entire texture.
    std::optional<Rect> OptSrcRect = std::nullopt;
    std::optional<Dimension2D> OptSize = std::nullopt;
    /// Collision rectangles are normalized within the sprite's frame.
    /// 0,0 represents the top-left corner of the frame.
    /// 1,1 represents the bottom-right corner of the frame.
    /// If not present then there is no collision.
    /// Collision rectangles CAN be outside the [0,1] range.
    /// When loaded normalized collision rectangles are converted to the sprite's coordinate space.
    std::optional<Rectf> OptColRect = std::nullopt;
    /// Normalized pivot point within the sprite's frame.
    /// 0,0 represents the top-left corner of the frame.
    /// 1,1 represents the bottom-right corner of the frame.
    glm::vec2 Pivot{ 0, 0 };
    glm::vec4 Tint = glm::vec4(1.0f);
};

/// Defines a sprite.
/// A sprite is a collection of sprite frame sequences, each representing a different facing direction.
/// The sequence at index 0 corresponds to the sprite facing zero turns (North),
/// and subsequent indices advance by 1 / (sequence count) turns.
/// Playback selects the nearest direction based on the angle. Each sequence's frame count
/// spans one complete animation cycle.
struct SpriteDef
{
    /// The unique name of the sprite.
    ResourceName Name;
    // FIXME(KB) - use a borrwed span from TBD SpriteCatalog
    /// The names of the sprite frame sequences associated with this sprite.
    std::vector<ResourceName> SequenceNames;
};

struct PropTypeDef
{
    /// The unique name of the prop type.
    ResourceName Name;
    /// Identifies the sprite associated with this prop type - i.e. the appearance of
    /// props of this type.
    /// Sometimes this sprite is only used as a place holder in the level editor and may not
    /// represent the final in-game appearance.
    ResourceName SpriteName;

    float Scale = 1.0f;
};

struct ActorTypeDef
{
    /// The unique name of the actor type.
    ResourceName Name;
    /// Actor class identifier.
    ResourceName ClassName;
    /// Identifies the sprite associated with this actor type - i.e. the appearance of
    /// actors of this type.
    /// Sometimes this sprite is only used as a place holder in the level editor and may not
    /// represent the final in-game appearance.
    ResourceName SpriteName;

    float Scale = 1.0f;
};

/// Defines a prop within the level.
struct PropInstanceDef
{
    /// The type of the prop.
    ResourceName TypeName;
    /// The transform (location/rotation) of the prop within the level.
    Transform2D Transform;
    /// Optional scale override.  If not present then
    /// scale from the prop type definition is used.
    std::optional<float> OptScale = std::nullopt;
};

struct ActorInstanceDef
{
    /// The type of the actor.
    ResourceName TypeName;
    /// The transform (location/rotation) of the actor within the level.
    Transform2D Transform;
    /// Optional scale override.  If not present then
    /// scale from the actor type definition is used.
    std::optional<float> OptScale = std::nullopt;
};

struct CellMapLayerDef
{
    /// The index of the tile sheet used by this layer.
    uint32_t SheetIndex;
};

/// Defines the layout of cells within the level.
struct CellMapDef
{
    /// The number of cells along one side of the square grid.
    uint32_t CellsPerSide{ 0 };
    /// The file paths to the tile sheets used in the cell map.
    std::span<const FilePath> TileSheets;
    /// The layers of the cell map.
    std::span<const CellMapLayerDef> Layers;
};

/// Defines an audio clip resource.
struct AudioClipDef
{
    /// The unique name of the audio clip resource.
    ResourceName Name;
    /// The file path to the audio clip.
    FilePath FilePath;
};

struct LevelDef
{
    CellMapDef CellMapDef;
    std::span<const SpriteFrameSequenceDef> SpriteSeqDefs;
    std::span<const SpriteDef> SpriteDefs;
    std::span<const PropTypeDef> PropTypeDefs;
    std::span<const ActorTypeDef> ActorTypeDefs;
    std::span<const PropInstanceDef> PropInstDefs;
    std::span<const ActorInstanceDef> ActorInstDefs;
    std::span<const AudioClipDef> AudioClipDefs;

    Result<const SpriteFrameSequenceDef&> GetSpriteFrameSequenceDef(const ResourceName& name) const;
    Result<const SpriteDef&> GetSpriteDef(const ResourceName& name) const;
    Result<const PropTypeDef&> GetPropTypeDef(const ResourceName& name) const;
    Result<const ActorTypeDef&> GetActorTypeDef(const ResourceName& name) const;
};