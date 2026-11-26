#include "LevelParser.h"

#include "BoundedVector.h"
#include "JsonDocument.h"
#include "Result.h"

#include <glm/vec2.hpp>
#include <optional>
#include <string_view>

namespace
{

Result<ResourceName>
TryParseResourceName(const JsonObject jobj, const std::string_view key)
{
    std::string_view name;
    GG_CHECKB(jobj.TryGetString(key, name), "Failed to get resource name for key: {}", key);
    GG_TRY(resName, ResourceName::Create(name), "Failed to create resource name from {}", name);
    return resName;
}

Result<ResourceName>
TryParseResourceName(const JsonValue jval)
{
    std::string_view name;
    GG_CHECKB(jval.TryGetString(name), "Failed to get resource name from JsonValue");
    GG_TRY(resName, ResourceName::Create(name), "Failed to create resource name from {}", name);
    return resName;
}

Result<SpriteFrameSequenceDef>
ParseSpriteSequence(const JsonObject& sequence)
{
    std::string_view texPathStr;
    std::optional<ResourceName> optName;
    std::optional<Rect> optSrcRect;
    std::optional<Dimension2D> optSize;
    std::optional<Rectf> optColRect;
    glm::vec2 pivot;
    float scale = 1;
    glm::vec4 tint;

    GG_CHECKB(sequence.TryGetString("texPath", texPathStr) && !texPathStr.empty(),
        "Missing texture path (line:{})",
        sequence.GetLine());
    GG_CHECKB(!texPathStr.empty(), "Texture path is empty (line:{})", sequence.GetLine());

    GG_TRY(texPath, FilePath::Create(texPathStr), "Failed to create file path from {}", texPathStr);

    if(std::string_view name; !sequence.TryGetString("name", name) || name.empty())
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

        GG_TRY(resName, ResourceName::Create(name));

        optName = resName;
    }
    else
    {
        GG_TRY(resName, ResourceName::Create(name));

        optName = resName;
    }
    GG_CHECKB(optName.has_value() && !optName->empty(),
        "Name is empty (line:{})",
        sequence.GetLine());

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
                *optName,
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
                *optName,
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
            GG_CHECKB(!haveW && !haveH,
                "Incomplete size: {} (line:{})",
                *optName,
                jsonObj.GetLine());
        }
    }

    if(JsonArray jsonArray; sequence.TryGetArray("collisionRects", jsonArray))
    {
        for(const JsonValue& value : jsonArray)
        {
            JsonObject jsonObj;
            GG_CHECKB(value.TryGetObject(jsonObj),
                "Failed to get collision rect object: {}",
                *optName);

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
                    *optName,
                    jsonObj.GetLine());
            }

            optColRect = colRect;

            GG_CHECKB(optColRect.has_value(), "Collision rect not specified for: {}", *optName);
        }
    }

    if(!sequence.TryGetFloat("scale", scale))
    {
        /// Scale is optional - default to 1.0f.
        scale = 1.0f;
    }

    return SpriteFrameSequenceDef //
        {
            .Name = *optName,
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
ParseSpriteSequences(const JsonObject& jobj)
{
    JsonArray sequences;
    if(!jobj.TryGetArray("sequences", sequences))
    {
        // Sequences array is optional - return an empty vector if not present.
        return BoundedVector<SpriteFrameSequenceDef>(0);
    }

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
    std::optional<ResourceName> optName;
    std::vector<ResourceName> seqNames;

    GG_TRY(resName, TryParseResourceName(sprite, "name"));
    optName = resName;
    GG_CHECKB(optName.has_value() && !optName->empty(), "Missing name (line:{})", sprite.GetLine());

    if(JsonArray seqArray; sprite.TryGetArray("sequences", seqArray))
    {
        seqNames.reserve(seqArray.size());

        for(const JsonValue& value : seqArray)
        {
            GG_TRY(seqResName, TryParseResourceName(value));

            GG_CHECKB(!seqResName.empty(), "Empty sequence name (line:{})", value.GetLine());

            seqNames.emplace_back(seqResName);
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
ParseSprites(const JsonObject& jobj)
{
    JsonArray sprites;
    if(!jobj.TryGetArray("sprites", sprites))
    {
        // Sprites array is optional - return an empty vector if not present.
        return BoundedVector<SpriteDef>(0);
    }

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

Result<PropTypeDef>
ParsePropType(const JsonObject& propType)
{
    GG_TRY(resName, TryParseResourceName(propType, "name"));
    GG_CHECKB(!resName.empty(), "Missing name (line:{})", propType.GetLine());

    GG_TRY(spriteName, TryParseResourceName(propType, "sprite"));
    GG_CHECKB(!spriteName.empty(), "Missing sprite name (line:{})", propType.GetLine());

    return PropTypeDef //
        {
            .Name = resName,
            .SpriteName = spriteName,
        };
}

Result<BoundedVector<PropTypeDef>>
ParsePropTypes(const JsonObject& jobj)
{
    JsonArray propTypesArray;
    if(!jobj.TryGetArray("propTypes", propTypesArray))
    {
        // Prop types array is optional - return an empty vector if not present.
        return BoundedVector<PropTypeDef>(0);
    }

    BoundedVector<PropTypeDef> propTypes(propTypesArray.size());

    for(const JsonValue& value : propTypesArray)
    {
        JsonObject propType;
        GG_CHECKB(value.TryGetObject(propType),
            "Failed to get prop type object (line:{})",
            value.GetLine());

        auto result = ParsePropType(propType);
        GG_CHECKB(result, "Failed to parse prop type (line:{})", value.GetLine());

        propTypes.push_back(*result);
    }

    return propTypes;
}

Result<ActorTypeDef>
ParseActorType(const JsonObject& actorType)
{
    GG_TRY(resName, TryParseResourceName(actorType, "name"));
    GG_CHECKB(!resName.empty(), "Missing name (line:{})", actorType.GetLine());

    GG_TRY(className, TryParseResourceName(actorType, "class"));
    GG_CHECKB(!className.empty(), "Missing actor class name (line:{})", actorType.GetLine());

    GG_TRY(spriteName, TryParseResourceName(actorType, "sprite"));
    GG_CHECKB(!spriteName.empty(), "Missing sprite name (line:{})", actorType.GetLine());

    return ActorTypeDef //
        {
            .Name = resName,
            .ClassName = className,
            .SpriteName = spriteName,
        };
}

Result<BoundedVector<ActorTypeDef>>
ParseActorTypes(const JsonObject& jobj)
{
    JsonArray actorTypesArray;
    if(!jobj.TryGetArray("actorTypes", actorTypesArray))
    {
        // Actor types array is optional - return an empty vector if not present.
        return BoundedVector<ActorTypeDef>(0);
    }

    BoundedVector<ActorTypeDef> actorTypes(actorTypesArray.size());

    for(const JsonValue& value : actorTypesArray)
    {
        JsonObject actorType;
        GG_CHECKB(value.TryGetObject(actorType),
            "Failed to get actor type object (line:{})",
            value.GetLine());

        auto result = ParseActorType(actorType);
        GG_CHECKB(result, "Failed to parse actor type (line:{})", value.GetLine());

        actorTypes.push_back(*result);
    }

    return actorTypes;
}

Result<PropInstanceDef>
ParseProp(const JsonObject& prop)
{
    std::optional<glm::vec2> optPos;
    std::optional<float> optRot;

    GG_TRY(typeName, TryParseResourceName(prop, "type"));
    GG_CHECKB(!typeName.empty(), "Missing type name (line:{})", prop.GetLine());

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

    return PropInstanceDef //
        {
            .TypeName = typeName,
            .Transform //
            {
                .Pos = *optPos,
                .Rot = Angle::FromDegrees(*optRot),
            },
        };
}

Result<BoundedVector<PropInstanceDef>>
ParseProps(const JsonObject& jobj)
{
    JsonArray jsonProps;
    if(!jobj.TryGetArray("props", jsonProps))
    {
        // Props array is optional - return an empty vector if not present.
        return BoundedVector<PropInstanceDef>(0);
    }

    BoundedVector<PropInstanceDef> props(jsonProps.size());

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

Result<ActorInstanceDef>
ParseActor(const JsonObject& actor)
{
    std::optional<glm::vec2> optPos;
    std::optional<float> optRot;

    GG_TRY(typeName, TryParseResourceName(actor, "type"));
    GG_CHECKB(!typeName.empty(), "Missing type name (line:{})", actor.GetLine());

    if(JsonObject jsonObj; actor.TryGetObject("pos", jsonObj))
    {
        if(float x{}, y{}; jsonObj.TryGetFloat("x", x) && jsonObj.TryGetFloat("y", y))
        {
            optPos = glm::vec2{ x, y };
        }
    }
    GG_CHECKB(optPos.has_value(), "Missing position (line:{})", actor.GetLine());

    if(float rot{}; actor.TryGetFloat("rot", rot))
    {
        optRot = rot;
    }
    GG_CHECKB(optRot.has_value(), "Missing rotation (line:{})", actor.GetLine());

    return ActorInstanceDef //
        {
            .TypeName = typeName,
            .Transform //
            {
                .Pos = *optPos,
                .Rot = Angle::FromDegrees(*optRot),
            },
        };
}

Result<BoundedVector<ActorInstanceDef>>
ParseActors(const JsonObject& jobj)
{
    JsonArray jsonActors;
    if(!jobj.TryGetArray("actors", jsonActors))
    {
        // Actors array is optional - return an empty vector if not present.
        return BoundedVector<ActorInstanceDef>(0);
    }

    BoundedVector<ActorInstanceDef> actors(jsonActors.size());

    for(const JsonValue& value : jsonActors)
    {
        JsonObject actor;
        GG_CHECKB(value.TryGetObject(actor),
            "Failed to get actor object (line:{})",
            value.GetLine());

        auto result = ParseActor(actor);
        GG_CHECKB(result, "Failed to parse actor (line:{})", value.GetLine());

        actors.push_back(*result);
    }

    return actors;
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

    return CellMapLayerDef{ .SheetIndex = *optSheetIndex };
}

Result<BoundedVector<CellMapLayerDef>>
ParseLayers(const JsonObject& jobj)
{
    // Layers array is required - fail if not present.
    JsonArray jsonLayers;
    GG_CHECKB(jobj.TryGetArray("layers", jsonLayers),
        "Missing layers array (line:{})",
        jobj.GetLine());

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
ParseAudioClips(const JsonObject& jobj)
{
    JsonObject extensionsObj;
    if(!jobj.TryGetObject("extensions", extensionsObj))
    {
        // Extensions object is optional - continue if not present.
        return BoundedVector<AudioClipDef>(0);
    }

    JsonArray audioClipsArray;
    if(!extensionsObj.TryGetArray("audioClips", audioClipsArray))
    {
        // Audio clips array is optional - return an empty vector if not present.
        return BoundedVector<AudioClipDef>(0);
    }

    const size_t count = audioClipsArray.size();

    BoundedVector<AudioClipDef> audioClips(count);

    for(const JsonValue& value : audioClipsArray)
    {
        JsonObject audioClip;
        GG_CHECKB(value.TryGetObject(audioClip),
            "Failed to get audio clip object (line:{})",
            value.GetLine());

        std::string_view nameStr;
        GG_CHECKB(audioClip.TryGetString("name", nameStr),
            "Missing audio clip name (line:{})",
            audioClip.GetLine());

        GG_TRY(resName,
            ResourceName::Create(nameStr),
            "Failed to create resource name from {}",
            nameStr);

        std::string_view filePathStr;
        GG_CHECKB(audioClip.TryGetString("filePath", filePathStr),
            "Missing audio clip file path (line:{})",
            audioClip.GetLine());

        GG_TRYB(filePath,
            FilePath::Create(filePathStr),
            "Failed to create file path from {}",
            filePathStr);

        audioClips.push_back(AudioClipDef{ .Name = resName, .FilePath = filePath });
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

            GG_TRYB(filePath, FilePath::Create(str), "Failed to create file path from {}", str);

            optTileSheetPaths->push_back(filePath);
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

            GG_TRYB(filePath, FilePath::Create(str), "Failed to create file path from {}", str);

            optMaskSheetPaths->push_back(filePath);
        }
    }
    GG_CHECKB(optMaskSheetPaths.has_value(), "Missing mask sheet paths (line:{})", jobj.GetLine());

    // Sprite sequences
    // sequences is optional - it can be absent in the JSON
    auto sequenceDefsResult = ParseSpriteSequences(jobj);
    GG_CHECK(sequenceDefsResult, "Failed to parse sprite sequences (line:{})", jobj.GetLine());

    // Sprites
    // sprites is optional - it can be absent in the JSON
    auto spriteDefsResult = ParseSprites(jobj);
    GG_CHECK(spriteDefsResult, "Failed to parse sprites (line:{})", jobj.GetLine());

    // Prop types
    // propTypes is optional - it can be absent in the JSON
    auto propTypeDefsResult = ParsePropTypes(jobj);
    GG_CHECK(propTypeDefsResult, "Failed to parse prop types (line:{})", jobj.GetLine());

    // Actor types
    // actorTypes is optional - it can be absent in the JSON
    auto actorTypeDefsResult = ParseActorTypes(jobj);
    GG_CHECK(actorTypeDefsResult, "Failed to parse actor types (line:{})", jobj.GetLine());

    // Props
    // props is optional - it can be absent in the JSON
    auto propDefsResult = ParseProps(jobj);
    GG_CHECK(propDefsResult, "Failed to parse props (line:{})", jobj.GetLine());

    // Actors
    // actors is optional - it can be absent in the JSON
    auto actorDefsResult = ParseActors(jobj);
    GG_CHECK(actorDefsResult, "Failed to parse actors (line:{})", jobj.GetLine());

    // Layers
    auto layersResult = ParseLayers(jobj);
    GG_CHECK(layersResult, "Failed to parse layers (line:{})", jobj.GetLine());

    // Audio clips
    // audioClips is optional - it can be absent in the JSON
    auto audioClipDefsResult = ParseAudioClips(jobj);
    GG_CHECK(audioClipDefsResult, "Failed to parse audio clips (line:{})", jobj.GetLine());

    ParsedLevel parsedLevel(*optGridSize,
        std::move(*optTileSheetPaths),
        std::move(*optMaskSheetPaths),
        std::move(*layersResult),
        std::move(*sequenceDefsResult),
        std::move(*spriteDefsResult),
        std::move(*propTypeDefsResult),
        std::move(*actorTypeDefsResult),
        std::move(*propDefsResult),
        std::move(*actorDefsResult),
        std::move(*audioClipDefsResult));

    m_ParsedLevel = std::move(parsedLevel);

    return Result<>::Ok;
}