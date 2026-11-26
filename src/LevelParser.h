#pragma once

#include "BoundedVector.h"
#include "FileFetcher.h"
#include "ResourceDefs.h"

class Level;
class Renderer;
class WebGpu;
class AudioPlayer;

/// Represents a parsed level, containing all necessary definitions for the level.
class ParsedLevel
{
public:
    ParsedLevel() = delete;

    const LevelDef& GetLevelDef() const { return m_LevelDef; }

private:
    friend class LevelParser;

    ParsedLevel(const uint32_t cellsPerSide,
        BoundedVector<FilePath>&& tileSheetPaths,
        BoundedVector<FilePath>&& maskSheetPaths,
        BoundedVector<CellMapLayerDef>&& layers,
        BoundedVector<SpriteFrameSequenceDef>&& spriteSeqDefs,
        BoundedVector<SpriteDef>&& spriteDefs,
        BoundedVector<PropTypeDef>&& propTypeDefs,
        BoundedVector<ActorTypeDef>&& actorTypeDefs,
        BoundedVector<PropDef>&& propDefs,
        BoundedVector<AudioClipDef>&& audioClipDefs)
        : m_CellsPerSide(cellsPerSide),
          m_TileSheetPaths(std::move(tileSheetPaths)),
          m_MaskSheetPaths(std::move(maskSheetPaths)),
          m_Layers(std::move(layers)),
          m_SpriteSeqDefs(std::move(spriteSeqDefs)),
          m_SpriteDefs(std::move(spriteDefs)),
          m_PropTypeDefs(std::move(propTypeDefs)),
          m_ActorTypeDefs(std::move(actorTypeDefs)),
          m_PropDefs(std::move(propDefs)),
          m_AudioClipDefs(std::move(audioClipDefs))
    {
    }

    uint32_t m_CellsPerSide;
    BoundedVector<FilePath> m_TileSheetPaths;
    BoundedVector<FilePath> m_MaskSheetPaths;
    BoundedVector<CellMapLayerDef> m_Layers;
    BoundedVector<SpriteFrameSequenceDef> m_SpriteSeqDefs;
    BoundedVector<SpriteDef> m_SpriteDefs;
    BoundedVector<PropTypeDef> m_PropTypeDefs;
    BoundedVector<ActorTypeDef> m_ActorTypeDefs;
    BoundedVector<PropDef> m_PropDefs;
    BoundedVector<AudioClipDef> m_AudioClipDefs;
    LevelDef m_LevelDef //
        {
            .CellMapDef //
            {
                .CellsPerSide = m_CellsPerSide,
                .TileSheets{ m_TileSheetPaths },
                .Layers{ m_Layers },
            },
            .SpriteSeqDefs{ m_SpriteSeqDefs },
            .SpriteDefs{ m_SpriteDefs },
            .PropTypeDefs{ m_PropTypeDefs },
            .ActorTypeDefs{ m_ActorTypeDefs },
            .PropDefs{ m_PropDefs },
            .AudioClipDefs{ m_AudioClipDefs },
        };
};

/// Fetches level data from a file and parses it into a ParsedLevel.
/// Performs syntax checking only.  No semantic validation is done.
class LevelParser
{
public:
    static Result<LevelParser> Fetch(const FilePath& path, FileFetcher& fetcher);

    Result<> Update();

    bool IsPending() const;

    Result<ParsedLevel> Take();

private:
    enum class Status
    {
        FetchingFile,
        ParsingLevel,
        Fetched,
        Failed,
        Taken
    };

    explicit LevelParser(FetchRequestPtr&& fetchRequest);

    Result<> Parse(std::vector<std::byte>&& data);

    FetchRequestPtr m_FetchRequest;

    std::optional<ParsedLevel> m_ParsedLevel;

    Status m_Status{ Status::FetchingFile };
};