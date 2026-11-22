#include "LevelFetcher.h"

#include "BoundedVector.h"
#include "JsonDocument.h"
#include "Result.h"

#include <glm/vec2.hpp>
#include <optional>
#include <string_view>

// DO NOT SUBMIT
#include "Resources/AnimResources.h"

namespace
{

class Layer
{
public:
    Layer() = delete;
    ~Layer() = default;
    Layer(const Layer&) = delete;
    Layer& operator=(const Layer&) = delete;
    Layer(Layer&&) = default;
    Layer& operator=(Layer&&) = default;

    Layer(uint32_t sheetIndex, BoundedVector<uint32_t>&& cells)
        : m_SheetIndex(sheetIndex),
          m_Cells(std::move(cells))
    {
    }

    uint32_t GetSheetIndex() const { return m_SheetIndex; }
    std::span<const uint32_t> GetCells() const { return m_Cells; }

private:
    uint32_t m_SheetIndex;
    BoundedVector<uint32_t> m_Cells;
};

struct ParsedLevel
{
    uint32_t CellsPerSide;
    BoundedVector<FilePath> TileSheetPaths;
    BoundedVector<FilePath> MaskSheetPaths;
    BoundedVector<SpriteFrameSequenceDef> SpriteSeqDefs;
    BoundedVector<Layer> Layers;
    BoundedVector<PropDef> PropDefs;
    LevelDef LevelDef //
        {
            .CellMapDef //
            {
                .CellsPerSide = CellsPerSide,
                .TileSets{ TileSheetPaths },
            },
            .SpriteSeqDefs{ SpriteSeqDefs },
            .PropDefs{ PropDefs },
        };
};

Result<SpriteFrameSequenceDef>
ParsePropAsset(const JsonObject& propAsset)
{
    std::optional<std::string_view> optTexPath;
    std::optional<std::string_view> optName;
    std::optional<Rect> optSrcRect;
    std::optional<Dimension2D> optSize;
    std::optional<Rectf> optColRect;
    std::optional<glm::vec2> optPivot;
    std::optional<float> optScale;

    if(std::string_view name; propAsset.TryGetString("name", name))
    {
        optName = name;
    }
    GG_CHECKB(optName.has_value(), "Missing name (line:{})", propAsset.GetLine());

    if(std::string_view path; propAsset.TryGetString("texPath", path))
    {
        optTexPath = path;
    }
    GG_CHECKB(optTexPath.has_value(),
        "Missing texture path: {} (line:{})",
        *optName,
        propAsset.GetLine());

    if(JsonObject jsonObj; propAsset.TryGetObject("pivot", jsonObj))
    {
        if(float x{}, y{}; jsonObj.TryGetFloat("x", x) && jsonObj.TryGetFloat("y", y))
        {
            optPivot = glm::vec2{ x, y };
        }
    }

    if(JsonObject jsonObj; propAsset.TryGetObject("srcRect", jsonObj))
    {
        uint32_t x{}, y{}, w{}, h{};

        const bool haveX = jsonObj.TryGetUint32("x", x).HasValue();
        const bool haveY = jsonObj.TryGetUint32("y", y).HasValue();
        const bool haveW = jsonObj.TryGetUint32("w", w).HasValue();
        const bool haveH = jsonObj.TryGetUint32("h", h).HasValue();
        
        if(haveX && haveY && haveW && haveH)
        {
            optSrcRect = Rect{ x, y, w, h };
        }
        else
        {
            GG_CHECKB(!haveX && !haveY && !haveW && !haveH,
                "Incomplete srcRect: {} (line:{})",
                *optName,
                jsonObj.GetLine());
        }
    }

    if(JsonObject jsonObj; propAsset.TryGetObject("size", jsonObj))
    {
        uint32_t w{}, h{};

        const bool haveW = jsonObj.TryGetUint32("w", w).HasValue();
        const bool haveH = jsonObj.TryGetUint32("h", h).HasValue();
        
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

    if(JsonArray jsonArray; propAsset.TryGetArray("collisionRects", jsonArray))
    {
        for(const JsonValue& value : jsonArray)
        {
            JsonObject jsonObj;
            GG_CHECKB(value.TryGetObject(jsonObj),
                "Failed to get collision rect object: {}",
                *optName);
            float x{}, y{}, w{}, h{};

            GG_CHECKB(jsonObj.TryGetFloat("x", x),
                "Missing x in collision rect: {} (line:{})",
                *optName,
                value.GetLine());
            GG_CHECKB(jsonObj.TryGetFloat("y", y),
                "Missing y in collision rect: {} (line:{})",
                *optName,
                value.GetLine());
            GG_CHECKB(jsonObj.TryGetFloat("w", w),
                "Missing w in collision rect: {} (line:{})",
                *optName,
                value.GetLine());
            GG_CHECKB(jsonObj.TryGetFloat("h", h),
                "Missing h in collision rect: {} (line:{})",
                *optName,
                value.GetLine());

            optColRect = Rectf{ x, y, w, h };
        }
    }

    if(float scale{}; propAsset.TryGetFloat("scale", scale))
    {
        optScale = scale;
    }

    const glm::vec2 pivot = optPivot ? *optPivot : glm::vec2{ 0.5f, 0.5f };

    const float scaleValue = optScale ? *optScale : 1.0f;

    return SpriteFrameSequenceDef //
        {
            .Name = *optName,
            .TexturePath = *optTexPath,
            .SrcRect = optSrcRect,
            .Size = optSize,
            .ColRect = optColRect,
            .Pivot = pivot,
            .Scale = scaleValue,
        };
}

Result<BoundedVector<SpriteFrameSequenceDef>>
ParsePropAssets(const JsonArray& propAssets)
{
    BoundedVector<SpriteFrameSequenceDef> spriteSeqDefs(propAssets.size());

    for(const JsonValue& value : propAssets)
    {
        JsonObject propAsset;
        GG_CHECKB(value.TryGetObject(propAsset),
            "Failed to get prop asset object (line:{})",
            value.GetLine());

        auto result = ParsePropAsset(propAsset);
        GG_CHECKB(result, "Failed to parse prop asset (line:{})", value.GetLine());

        for(const SpriteFrameSequenceDef& spriteSeqDef : spriteSeqDefs)
        {
            if(spriteSeqDef.Name == result->Name)
            {
                GG_CHECKB(false,
                    "Duplicate sprite sequence name: {} (line:{})",
                    result->Name,
                    value.GetLine());
            }
        }

        spriteSeqDefs.push_back(*result);
    }

    return spriteSeqDefs;
}

Result<PropDef>
ParseProp(const JsonObject& prop)
{
    std::optional<uint32_t> optAssetIndex;
    std::optional<glm::vec2> optPos;
    std::optional<float> optRot;

    if(uint32_t assetIndex{}; prop.TryGetUint32("index", assetIndex))
    {
        optAssetIndex = assetIndex;
    }
    GG_CHECKB(optAssetIndex.has_value(), "Missing asset index (line:{})", prop.GetLine());

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
            .SpriteId = *optAssetIndex,
            .Transform //
            {
                .Pos = *optPos,
                .Rot = Angle::FromDegrees(*optRot),
            },
        };
}

Result<BoundedVector<PropDef>>
ParseProps(const JsonArray& jsonProps)
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

Result<Layer>
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

    return Layer{ *optSheetIndex, std::move(cells) };
}

Result<BoundedVector<Layer>>
ParseLayers(const JsonArray& jsonLayers)
{
    const size_t count = jsonLayers.size();

    BoundedVector<Layer> layers(count);

    for(const JsonValue& value : jsonLayers)
    {
        JsonObject layer;
        GG_CHECKB(value.TryGetObject(layer),
            "Failed to get layer object (line:{})",
            value.GetLine());

        auto layerResult = ParseLayer(layer);
        GG_CHECKB(layerResult, "Failed to parse layer (line:{})", value.GetLine());
        layers.push_back(std::move(*layerResult));
    }

    return layers;
}

Result<ParsedLevel>
Parse(std::vector<std::byte>&& data)
{
    auto docResult = JsonDocument::Create(std::move(data));
    GG_CHECK(docResult, "Failed to create JSON parser");

    const JsonObject jobj = docResult->GetRootObject();

    std::optional<uint32_t> optGridSize;

    std::optional<BoundedVector<FilePath>> optTileSheetPaths;
    std::optional<BoundedVector<FilePath>> optMaskSheetPaths;
    std::optional<BoundedVector<SpriteFrameSequenceDef>> optSpriteSeqDefs;
    std::optional<BoundedVector<Layer>> optLayers;
    std::optional<BoundedVector<PropDef>> optPropDefs;

    if(uint32_t gridSize{}; jobj.TryGetUint32("gridSize", gridSize))
    {
        optGridSize = gridSize;
    }
    GG_CHECKB(optGridSize.has_value(), "Missing grid size (line:{})", jobj.GetLine());

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
    GG_CHECKB(optMaskSheetPaths.has_value(), "Missing mask sheet paths");

    if(JsonArray propAssetsArray; jobj.TryGetArray("propAssets", propAssetsArray))
    {
        auto propAssetsResult = ParsePropAssets(propAssetsArray);
        GG_CHECK(propAssetsResult,
            "Failed to parse prop assets (line:{})",
            propAssetsArray.GetLine());

        optSpriteSeqDefs = std::move(*propAssetsResult);
    }
    // propAssets is optional - it can be absent in the JSON

    if(JsonArray propsArray; jobj.TryGetArray("props", propsArray))
    {
        auto propResult = ParseProps(propsArray);
        GG_CHECK(propResult, "Failed to parse props (line:{})", propsArray.GetLine());

        optPropDefs.emplace(std::move(*propResult));
    }
    // props is optional - it can be absent in the JSON

    if(JsonArray layersArray; jobj.TryGetArray("layers", layersArray))
    {
        auto layerResult = ParseLayers(layersArray);
        GG_CHECK(layerResult, "Failed to parse layer (line:{})", layersArray.GetLine());

        optLayers.emplace(std::move(*layerResult));
    }
    GG_CHECKB(optLayers.has_value(), "Missing layers (line:{})", jobj.GetLine());

    if(!optSpriteSeqDefs || optSpriteSeqDefs->empty())
    {
        GG_CHECKB(!optPropDefs || optPropDefs->empty(),
            "Props must be empty if no prop assets are defined");
    }

    BoundedVector<SpriteFrameSequenceDef> spriteSeqDefs =
        optSpriteSeqDefs ? std::move(*optSpriteSeqDefs) : BoundedVector<SpriteFrameSequenceDef>(0);
    BoundedVector<PropDef> propDefs =
        optPropDefs ? std::move(*optPropDefs) : BoundedVector<PropDef>(0);

    return ParsedLevel //
        {
            .CellsPerSide = *optGridSize,
            .TileSheetPaths = std::move(*optTileSheetPaths),
            .MaskSheetPaths = std::move(*optMaskSheetPaths),
            .SpriteSeqDefs = std::move(spriteSeqDefs),
            .Layers = std::move(*optLayers),
            .PropDefs = std::move(propDefs),
        };
}
} // namespace

LevelFetcher::LevelFetcher(
    FetchRequestPtr&& fetchRequest, FileFetcher& fetcher, Renderer& renderer, WebGpu& webGpu)
    : m_FetchRequest(std::move(fetchRequest)),
      m_Fetcher(&fetcher),
      m_Renderer(&renderer),
      m_WebGpu(&webGpu)
{
}

Result<LevelFetcher>
LevelFetcher::Fetch(const FilePath& path, FileFetcher& fetcher, Renderer& renderer, WebGpu& webGpu)
{
    auto fetchResult = fetcher.Fetch(path);
    GG_CHECKB(fetchResult, "Failed to fetch file: {}", path.c_str());

    return LevelFetcher(std::move(*fetchResult), fetcher, renderer, webGpu);
}

Result<>
LevelFetcher::Update()
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

            const auto parseResult = Parse(std::move(*dataResult));
            GG_CHECK(parseResult, "Failed to parse level data");

            auto levelLoaderResult = LevelLoader::Load(AnimResources::GetAnimDefs(),
                parseResult->LevelDef,
                *m_Fetcher,
                *m_Renderer,
                *m_WebGpu);
            GG_CHECK(levelLoaderResult, "Failed to load level");

            m_LevelLoader = std::move(*levelLoaderResult);

            m_Status = Status::LoadingLevel;
        }
        break;
        case Status::LoadingLevel:
            if(!m_LevelLoader->IsPending())
            {
                m_Status = Status::Loaded;
            }
            break;
        case Status::Loaded:
        case Status::Failed:
        case Status::Taken:
            break;
    }
    return Result<>::Ok;
}

bool
LevelFetcher::IsPending() const
{
    return m_Status == Status::FetchingFile
        || m_Status == Status::ParsingLevel
        || m_Status == Status::LoadingLevel;
}

Result<std::unique_ptr<Level>>
LevelFetcher::Take()
{
    GG_CHECKB(m_Status == Status::Loaded, "Level not loaded");
    GG_CHECKB(m_LevelLoader.has_value(), "Level not loaded");

    m_Status = Status::Taken;

    auto result = m_LevelLoader->Take();

    m_LevelLoader.reset();

    return result;
}
