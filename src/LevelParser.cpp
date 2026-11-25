#include "LevelParser.h"

#include "BoundedVector.h"
#include "JsonDocument.h"
#include "Result.h"

#include <glm/vec2.hpp>
#include <optional>
#include <string_view>

namespace
{

Result<SpriteFrameSequenceDef>
ParseSpriteSequence(const JsonObject& sequence)
{
    std::string_view texPath;
    std::string_view name;
    std::optional<Rect> optSrcRect;
    std::optional<Dimension2D> optSize;
    std::optional<Rectf> optColRect;
    glm::vec2 pivot;
    float scale = 1;
    glm::vec4 tint;

    GG_CHECKB(sequence.TryGetString("texPath", texPath) && !texPath.empty(),
        "Missing texture path (line:{})",
        sequence.GetLine());
    GG_CHECKB(!texPath.empty(), "Texture path is empty (line:{})", sequence.GetLine());

    if(!sequence.TryGetString("name", name) || name.empty())
    {
        // If the name is not specified, derive it from the texture path.

        name = texPath;
        // Remove the directory and extension to get the stem of the file path.
        name.remove_prefix(name.find_last_of('/') + 1);
        const size_t extension = name.find_last_of('.');
        if(extension != std::string_view::npos && extension > 0)
        {
            name = name.substr(0, extension);
        }
    }
    GG_CHECKB(!name.empty(), "Name is empty (line:{})", sequence.GetLine());

    if(JsonObject jsonObj; sequence.TryGetObject("pivot", jsonObj))
    {
        GG_CHECKB(jsonObj.TryGetFloat("x", pivot.x) && jsonObj.TryGetFloat("y", pivot.y),
            "Invalid pivot coordinates (line:{})",
            jsonObj.GetLine());
    }
    else
    {
        // Pivot is optional - default to the center of the sprite.
        pivot = glm::vec2{ 0.5f, 0.5f };
    }

    // srcRect is optional - if not present then the entire texture will be used.
    if(JsonObject jsonObj; sequence.TryGetObject("srcRect", jsonObj))
    {
        Rect srcRect{ 0, 0, 0, 0 };
        const bool haveX = jsonObj.TryGetUint32("x", srcRect.x);
        const bool haveY = jsonObj.TryGetUint32("y", srcRect.y);
        const bool haveW = jsonObj.TryGetUint32("w", srcRect.w);
        const bool haveH = jsonObj.TryGetUint32("h", srcRect.h);

        // If any of the srcRect values are specified, all must be specified.
        // An empty srcRect {} is permitted.
        if(haveX && haveY && haveW && haveH)
        {
            optSrcRect = srcRect;
        }
        else
        {
            GG_CHECKB(!haveX && !haveY && !haveW && !haveH,
                "Incomplete srcRect: {} (line:{})",
                name,
                jsonObj.GetLine());
        }
    }

    // Tint is optional - default to white.
    tint = glm::vec4{ 1.0f, 1.0f, 1.0f, 1.0f };
    if(JsonObject jsonObj; sequence.TryGetObject("tint", jsonObj))
    {
        float r{}, g{}, b{}, a{};
        const bool haveR = jsonObj.TryGetFloat("r", r);
        const bool haveG = jsonObj.TryGetFloat("g", g);
        const bool haveB = jsonObj.TryGetFloat("b", b);
        const bool haveA = jsonObj.TryGetFloat("a", a);

        // If any of the tint values are specified, all must be specified.
        if(haveR && haveG && haveB && haveA)
        {
            tint = glm::vec4{ r, g, b, a };
        }
        else
        {
            GG_CHECKB(!haveR && !haveG && !haveB && !haveA,
                "Incomplete tint: {} (line:{})",
                name,
                jsonObj.GetLine());
        }
    }

    if(JsonObject jsonObj; sequence.TryGetObject("size", jsonObj))
    {
        uint32_t w{}, h{};

        const bool haveW = jsonObj.TryGetUint32("w", w);
        const bool haveH = jsonObj.TryGetUint32("h", h);

        // If any of the size values are specified, all must be specified.
        if(haveW && haveH)
        {
            optSize = Dimension2D{ .w = w, .h = h };
        }
        else
        {
            GG_CHECKB(!haveW && !haveH, "Incomplete size: {} (line:{})", name, jsonObj.GetLine());
        }
    }

    if(JsonArray jsonArray; sequence.TryGetArray("collisionRects", jsonArray))
    {
        for(const JsonValue& value : jsonArray)
        {
            JsonObject jsonObj;
            GG_CHECKB(value.TryGetObject(jsonObj), "Failed to get collision rect object: {}", name);

            Rectf colRect{ 0, 0, 0, 0 };

            const bool haveX = jsonObj.TryGetFloat("x", colRect.x);
            const bool haveY = jsonObj.TryGetFloat("y", colRect.y);
            const bool haveW = jsonObj.TryGetFloat("w", colRect.w);
            const bool haveH = jsonObj.TryGetFloat("h", colRect.h);

            // If any of the collision rect values are specified, all must be specified.
            if(!haveX || !haveY || !haveW || !haveH)
            {
                GG_CHECKB(!haveX && !haveY && !haveW && !haveH,
                    "Incomplete collision rect: {} (line:{})",
                    name,
                    jsonObj.GetLine());
            }

            optColRect = colRect;
        }
    }

    if(!sequence.TryGetFloat("scale", scale))
    {
        /// Scale is optional - default to 1.0f.
        scale = 1.0f;
    }

    return SpriteFrameSequenceDef //
        {
            .Name = name,
            .TexturePath = texPath,
            .SrcRect = optSrcRect,
            .Size = optSize,
            .ColRect = optColRect,
            .Pivot = pivot,
            .Tint = tint,
            .Scale = scale,
        };
}

Result<BoundedVector<SpriteFrameSequenceDef>>
ParseSpriteSequences(const JsonArray& sequences)
{
    BoundedVector<SpriteFrameSequenceDef> spriteSeqDefs(sequences.size());

    for(const JsonValue& value : sequences)
    {
        JsonObject sequence;
        GG_CHECKB(value.TryGetObject(sequence),
            "Failed to get sprite sequence object (line:{})",
            value.GetLine());

        auto result = ParseSpriteSequence(sequence);
        GG_CHECKB(result, "Failed to parse sprite sequence (line:{})", value.GetLine());

        spriteSeqDefs.push_back(*result);
    }

    return spriteSeqDefs;
}

Result<SpriteDef>
ParseSprite(const JsonObject& sprite)
{
    std::optional<std::string_view> optName;
    std::vector<ResourceName> seqNames;

    if(std::string_view name; sprite.TryGetString("name", name))
    {
        optName = name;
    }
    GG_CHECKB(optName.has_value() && !optName->empty(), "Missing name (line:{})", sprite.GetLine());

    if(JsonArray seqArray; sprite.TryGetArray("sequences", seqArray))
    {
        seqNames.reserve(seqArray.size());

        for(const JsonValue& value : seqArray)
        {
            std::string_view seqName;
            GG_CHECKB(value.TryGetString(seqName),
                "Failed to get sequence name (line:{})",
                value.GetLine());

            GG_CHECKB(!seqName.empty(), "Empty sequence name (line:{})", value.GetLine());

            seqNames.emplace_back(seqName);
        }
    }
    GG_CHECKB(!seqNames.empty(),
        "Missing or empty sequences: {} (line:{})",
        *optName,
        sprite.GetLine());

    return SpriteDef //
        {
            .Name = *optName,
            .SequenceNames = std::move(seqNames),
        };
}

Result<BoundedVector<SpriteDef>>
ParseSprites(const JsonArray& sprites)
{
    BoundedVector<SpriteDef> spriteDefs(sprites.size());

    for(const JsonValue& value : sprites)
    {
        JsonObject sprite;
        GG_CHECKB(value.TryGetObject(sprite),
            "Failed to get sprite object (line:{})",
            value.GetLine());

        auto result = ParseSprite(sprite);
        GG_CHECKB(result, "Failed to parse sprite (line:{})", value.GetLine());

        spriteDefs.push_back(std::move(*result));
    }

    return spriteDefs;
}

Result<PropDef>
ParseProp(const JsonObject& prop)
{
    std::optional<std::string_view> optSpriteName;
    std::optional<glm::vec2> optPos;
    std::optional<float> optRot;

    if(std::string_view spriteName; prop.TryGetString("sprite", spriteName))
    {
        optSpriteName = spriteName;
    }
    GG_CHECKB(optSpriteName.has_value(), "Missing sprite name (line:{})", prop.GetLine());
    GG_CHECKB(!optSpriteName->empty(), "Empty sprite name (line:{})", prop.GetLine());

    if(JsonObject jsonObj; prop.TryGetObject("pos", jsonObj))
    {
        if(float x{}, y{}; jsonObj.TryGetFloat("x", x) && jsonObj.TryGetFloat("y", y))
        {
            optPos = glm::vec2{ x, y };
        }
    }
    GG_CHECKB(optPos.has_value(), "Missing position (line:{})", prop.GetLine());

    if(float rot{}; prop.TryGetFloat("rot", rot))
    {
        optRot = rot;
    }
    GG_CHECKB(optRot.has_value(), "Missing rotation (line:{})", prop.GetLine());

    return PropDef //
        {
            .SpriteName = *optSpriteName,
            .Transform //
            {
                .Pos = *optPos,
                .Rot = Angle::FromDegrees(*optRot),
            },
        };
}

Result<BoundedVector<PropDef>>
ParseProps(const JsonArray& jsonProps   )
{
    BoundedVector<PropDef> props(jsonProps.size());

    for(const JsonValue& value : jsonProps)
    {
        JsonObject prop;
        GG_CHECKB(value.TryGetObject(prop), "Failed to get prop object (line:{})", value.GetLine());

        auto result = ParseProp(prop);
        GG_CHECKB(result, "Failed to parse prop (line:{})", value.GetLine());

        props.push_back(*result);
    }

    return props;
}

Result<CellMapLayerDef>
ParseLayer(const JsonObject& layer)
{
    std::optional<uint32_t> optSheetIndex;
    std::optional<JsonArray> optCellsArray;

    if(uint32_t sheetIndex{}; layer.TryGetUint32("sheet", sheetIndex))
    {
        optSheetIndex = sheetIndex;
    }
    GG_CHECKB(optSheetIndex.has_value(), "Missing sheet index (line:{})", layer.GetLine());

    if(JsonArray cellsArray; layer.TryGetArray("cells", cellsArray))
    {
        optCellsArray = cellsArray;
    }
    GG_CHECKB(optCellsArray.has_value(), "Missing cells array (line:{})", layer.GetLine());

    const size_t cellCount = optCellsArray->size();
    BoundedVector<uint32_t> cells(cellCount);

    for(const JsonValue& cell : *optCellsArray)
    {
        uint32_t cellValue{ 0 };

        GG_CHECKB(cell.TryGetUint32(cellValue));

        cells.push_back(cellValue);
    }

    return CellMapLayerDef{ .SheetIndex =*optSheetIndex };
}

Result<BoundedVector<CellMapLayerDef>>
ParseLayers(const JsonArray& jsonLayers)
{
    const size_t count = jsonLayers.size();

    BoundedVector<CellMapLayerDef> layers(count);

    for(const JsonValue& value : jsonLayers)
    {
        JsonObject layer;
        GG_CHECKB(value.TryGetObject(layer),
            "Failed to get layer object (line:{})",
            value.GetLine());

        auto layerResult = ParseLayer(layer);
        GG_CHECKB(layerResult, "Failed to parse layer (line:{})", value.GetLine());
        layers.push_back(*layerResult);
    }

    return layers;
}

Result<BoundedVector<AudioClipDef>>
ParseAudioClips(const JsonArray& audioClipsArray)
{
    const size_t count = audioClipsArray.size();

    BoundedVector<AudioClipDef> audioClips(count);

    for(const JsonValue& value : audioClipsArray)
    {
        JsonObject audioClip;
        GG_CHECKB(value.TryGetObject(audioClip),
            "Failed to get audio clip object (line:{})",
            value.GetLine());

        std::string_view name;
        GG_CHECKB(audioClip.TryGetString("name", name),
            "Missing audio clip name (line:{})",
            audioClip.GetLine());

        std::string_view filePath;
        GG_CHECKB(audioClip.TryGetString("filePath", filePath),
            "Missing audio clip file path (line:{})",
            audioClip.GetLine());

        audioClips.push_back(AudioClipDef{ .Name = name, .FilePath = filePath });
    }

    return audioClips;
}
} // namespace

LevelParser::LevelParser(FetchRequestPtr&& fetchRequest)
    : m_FetchRequest(std::move(fetchRequest))
{
}

Result<LevelParser>
LevelParser::Fetch(const FilePath& path, FileFetcher& fetcher)
{
    auto fetchResult = fetcher.Fetch(path);
    GG_CHECKB(fetchResult, "Failed to fetch file: {}", path.c_str());

    return LevelParser(std::move(*fetchResult));
}

Result<>
LevelParser::Update()
{
    switch(m_Status)
    {
        case Status::FetchingFile:
            if(!m_FetchRequest->IsPending())
            {
                m_Status = Status::ParsingLevel;
            }

            break;
        case Status::ParsingLevel:
        {
            m_Status = Status::Failed;

            auto dataResult = m_FetchRequest->TakeData();
            GG_CHECK(dataResult, "Failed to get data from fetch request");

            GG_CHECK(Parse(std::move(*dataResult)), "Failed to parse level data");

            // Free up the fetch request.
            m_FetchRequest.reset();

            m_Status = Status::Fetched;
        }
        break;
        case Status::Fetched:
        case Status::Failed:
        case Status::Taken:
            break;
    }
    return Result<>::Ok;
}

bool
LevelParser::IsPending() const
{
    return m_Status == Status::FetchingFile || m_Status == Status::ParsingLevel;
}

Result<ParsedLevel>
LevelParser::Take()
{
    GG_CHECKB(m_Status != Status::Taken, "Level data already taken");
    GG_CHECKB(m_Status == Status::Fetched, "Level data not fetched");

    m_Status = Status::Taken;

    return std::move(*m_ParsedLevel);
}

// private:

Result<>
LevelParser::Parse(std::vector<std::byte>&& data)
{
    auto docResult = JsonDocument::Create(std::move(data));
    GG_CHECK(docResult, "Failed to create JSON parser");

    const JsonObject jobj = docResult->GetRootObject();

    std::optional<uint32_t> optGridSize;

    std::optional<BoundedVector<FilePath>> optTileSheetPaths;
    std::optional<BoundedVector<FilePath>> optMaskSheetPaths;
    std::optional<BoundedVector<SpriteFrameSequenceDef>> optSpriteSeqDefs;
    std::optional<BoundedVector<SpriteDef>> optSpriteDefs;
    std::optional<BoundedVector<CellMapLayerDef>> optLayers;
    std::optional<BoundedVector<PropDef>> optPropDefs;
    std::optional<BoundedVector<AudioClipDef>> optAudioClipDefs;

    // Grid size
    if(uint32_t gridSize{}; jobj.TryGetUint32("gridSize", gridSize))
    {
        optGridSize = gridSize;
    }
    GG_CHECKB(optGridSize.has_value(), "Missing grid size (line:{})", jobj.GetLine());

    // Tile sheets
    if(JsonArray tileSheetsArray; jobj.TryGetArray("tileSheets", tileSheetsArray))
    {
        optTileSheetPaths.emplace(tileSheetsArray.size());

        for(const JsonValue& sheet : tileSheetsArray)
        {
            std::string_view str;
            GG_CHECKB(sheet.TryGetString(str),
                "Failed to get sheet string (line:{})",
                sheet.GetLine());
            optTileSheetPaths->push_back(str);
        }
    }
    GG_CHECKB(optTileSheetPaths.has_value(), "Missing tile sheet paths (line:{})", jobj.GetLine());

    // Mask sheets
    if(JsonArray maskSheetsArray; jobj.TryGetArray("maskSheets", maskSheetsArray))
    {
        optMaskSheetPaths.emplace(maskSheetsArray.size());

        for(const JsonValue& sheet : maskSheetsArray)
        {
            std::string_view str;
            GG_CHECKB(sheet.TryGetString(str),
                "Failed to get sheet string (line:{})",
                sheet.GetLine());
            optMaskSheetPaths->push_back(str);
        }
    }
    GG_CHECKB(optMaskSheetPaths.has_value(), "Missing mask sheet paths (line:{})", jobj.GetLine());

    // Sprite sequences
    if(JsonArray sequencesArray; jobj.TryGetArray("sequences", sequencesArray))
    {
        auto sequencesResult = ParseSpriteSequences(sequencesArray);
        GG_CHECK(sequencesResult,
            "Failed to parse sprite sequences (line:{})",
            sequencesArray.GetLine());

        optSpriteSeqDefs = std::move(*sequencesResult);
    }
    // sequences is optional - it can be absent in the JSON

    // Ensure we have a valid sprite sequence list, even if it's empty.
    // Sequence defs are optional in the JSON, but we need a valid list to work with.
    if(!optSpriteSeqDefs.has_value())
    {
        optSpriteSeqDefs.emplace(BoundedVector<SpriteFrameSequenceDef>(0));
    }

    // Sprites
    if(JsonArray spritesArray; jobj.TryGetArray("sprites", spritesArray))
    {
        auto spritesResult = ParseSprites(spritesArray);
        GG_CHECK(spritesResult, "Failed to parse sprites (line:{})", spritesArray.GetLine());

        optSpriteDefs = std::move(*spritesResult);
    }
    // sprites is optional - it can be absent in the JSON

    // Ensure we have a valid sprite definition list, even if it's empty.
    // Sprite defs are optional in the JSON, but we need a valid list to work with.
    if(!optSpriteDefs.has_value())
    {
        optSpriteDefs.emplace(BoundedVector<SpriteDef>(0));
    }

    // Props
    if(JsonArray propsArray; jobj.TryGetArray("props", propsArray))
    {
        auto propResult = ParseProps(propsArray);
        GG_CHECK(propResult, "Failed to parse props (line:{})", propsArray.GetLine());

        optPropDefs.emplace(std::move(*propResult));
    }
    // props is optional - it can be absent in the JSON

    // Ensure we have a valid prop definition list, even if it's empty.
    // Prop defs are optional in the JSON, but we need a valid list to work with.
    if(!optPropDefs.has_value())
    {
        optPropDefs.emplace(BoundedVector<PropDef>(0));
    }

    // Layers
    if(JsonArray layersArray; jobj.TryGetArray("layers", layersArray))
    {
        auto layerResult = ParseLayers(layersArray);
        GG_CHECK(layerResult, "Failed to parse layer (line:{})", layersArray.GetLine());

        optLayers.emplace(std::move(*layerResult));
    }
    GG_CHECKB(optLayers.has_value(), "Missing layers (line:{})", jobj.GetLine());

    // Extensions
    if(JsonObject extensionsObj; jobj.TryGetObject("extensions", extensionsObj))
    {
        // Currently, we only handle audio clips within extensions.
        if(JsonArray audioClipsArray; extensionsObj.TryGetArray("audioClips", audioClipsArray))
        {
            auto audioClipResult = ParseAudioClips(audioClipsArray);
            GG_CHECK(audioClipResult,
                "Failed to parse audio clips (line:{})",
                audioClipsArray.GetLine());

            optAudioClipDefs.emplace(std::move(*audioClipResult));
        }
    }
    // audioClips is optional - it can be absent in the JSON

    // Ensure we have a valid audio clip definition list, even if it's empty.
    // Audio clip defs are optional in the JSON, but we need a valid list to work with.
    if(!optAudioClipDefs.has_value())
    {
        optAudioClipDefs.emplace(BoundedVector<AudioClipDef>(0));
    }

    ParsedLevel parsedLevel(*optGridSize,
        std::move(*optTileSheetPaths),
        std::move(*optMaskSheetPaths),
        std::move(*optLayers),
        std::move(*optSpriteSeqDefs),
        std::move(*optSpriteDefs),
        std::move(*optPropDefs),
        std::move(*optAudioClipDefs));

    m_ParsedLevel = std::move(parsedLevel);

    return Result<>::Ok;
}