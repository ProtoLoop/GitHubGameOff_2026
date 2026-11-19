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
    BoundedVector<SpriteDef> SpriteDefs;
    BoundedVector<Layer> Layers;
    BoundedVector<PropDef> PropDefs;
    LevelDef LevelDef //
        {
            .CellMapDef //
            {
                .CellsPerSide = CellsPerSide,
                .TileSets{ TileSheetPaths },
            },
            .SpriteDefs{ SpriteDefs },
            .PropDefs{ PropDefs },
        };
};

Result<SpriteDef>
ParsePropAsset(const JsonObject& propAsset)
{
    std::optional<std::string_view> texPath;
    std::optional<float> pivotX;
    std::optional<float> pivotY;
    std::optional<float> scale;

    for(const auto& p : propAsset)
    {
        if(std::string_view key; !GG_VERIFYB(p.TryGetKey(key), "Failed to get property key"))
        {
            return Result<>::Error;
        }
        else if(key == "texPath")
        {
            GG_CHECKB(!texPath.has_value(), "Duplicate texture path");
            GG_CHECKB(p.TryGetString(texPath.emplace()), "Failed to get texture path");
        }
        else if(key == "pivotX")
        {
            GG_CHECKB(!pivotX.has_value(), "Duplicate pivotX");
            GG_CHECKB(p.TryGetFloat(pivotX.emplace()), "Failed to get pivotX");
        }
        else if(key == "pivotY")
        {
            GG_CHECKB(!pivotY.has_value(), "Duplicate pivotY");
            GG_CHECKB(p.TryGetFloat(pivotY.emplace()), "Failed to get pivotY");
        }
        else if(key == "scale")
        {
            GG_CHECKB(!scale.has_value(), "Duplicate scale");
            GG_CHECKB(p.TryGetFloat(scale.emplace()), "Failed to get scale");

            GG_CHECKB(*scale > 0.0f, "Scale must be positive: {}", *scale);
        }
    }

    GG_CHECKB(texPath.has_value(), "Missing texture path");

    const Pivot2D pivot //
        {
            .x = pivotX ? *pivotX : Pivot2D::kDefault.x,
            .y = pivotY ? *pivotY : Pivot2D::kDefault.y,
        };

    const float scaleValue = scale ? *scale : 1.0f;

    std::string_view name = *texPath;

    // Extract the file name without the path and extension
    name = name.substr(name.find_last_of('/') + 1);
    name = name.substr(0, name.find_last_of('.'));

    return SpriteDef//
    {
        .Name = name,
        .TexturePath = *texPath,
        .Pivot = pivot,
        .Scale = scaleValue,
    };
}

Result<BoundedVector<SpriteDef>>
ParsePropAssets(const JsonArray& propAssets)
{
    BoundedVector<SpriteDef> spriteDefs(propAssets.size());

    for(const JsonValue& value : propAssets)
    {
        JsonObject propAsset;
        GG_CHECKB(value.TryGetObject(propAsset), "Failed to get prop asset object");

        auto result = ParsePropAsset(propAsset);
        GG_CHECKB(result, "Failed to parse prop asset");

        for(const SpriteDef& spriteDef : spriteDefs)
        {
            if(spriteDef.Name == result->Name)
            {
                GG_CHECKB(false, "Duplicate sprite name: {}", result->Name);
            }
        }

        spriteDefs.push_back(*result);
    }

    return spriteDefs;
}

Result<PropDef>
ParseProp(const JsonObject& prop)
{
    std::optional<uint32_t> assetIndex;
    std::optional<float> posX;
    std::optional<float> posY;
    std::optional<float> rot;

    for(const auto& p : prop)
    {
        if(std::string_view key; !GG_VERIFYB(p.TryGetKey(key), "Failed to get property key"))
        {
            return Result<>::Error;
        }
        else if(key == "index")
        {
            GG_CHECKB(!assetIndex.has_value(), "Duplicate asset index");
            GG_CHECKB(p.TryGetUint32(assetIndex.emplace()), "Failed to get asset index");
        }
        else if(key == "x")
        {
            GG_CHECKB(!posX.has_value(), "Duplicate x position");
            GG_CHECKB(p.TryGetFloat(posX.emplace()), "Failed to get x position");
        }
        else if(key == "y")
        {
            GG_CHECKB(!posY.has_value(), "Duplicate y position");
            GG_CHECKB(p.TryGetFloat(posY.emplace()), "Failed to get y position");
        }
        else if(key == "rot")
        {
            GG_CHECKB(!rot.has_value(), "Duplicate rotation");
            GG_CHECKB(p.TryGetFloat(rot.emplace()), "Failed to get rotation");
        }
    }

    GG_CHECKB(assetIndex.has_value(), "Missing asset index");
    GG_CHECKB(posX.has_value(), "Missing x position");
    GG_CHECKB(posY.has_value(), "Missing y position");
    GG_CHECKB(rot.has_value(), "Missing rotation");

    return PropDef//
    {
        .SpriteId = *assetIndex,
        .Transform//
        {
            .Pos{*posX, *posY},
            .Rot = Angle::FromDegrees(*rot)
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
        GG_CHECKB(value.TryGetObject(prop), "Failed to get prop object");

        auto result = ParseProp(prop);
        GG_CHECKB(result, "Failed to parse prop");

        props.push_back(*result);
    }

    return props;
}

Result<Layer>
ParseLayer(const JsonObject& layer)
{
    std::optional<uint32_t> sheetIndex;
    std::optional<JsonArray> cellsArray;

    for(const auto& prop : layer)
    {
        if(std::string_view key; !GG_VERIFYB(prop.TryGetKey(key), "Failed to get property key"))
        {
            return Result<>::Error;
        }
        else if(key == "sheet")
        {
            GG_CHECKB(!sheetIndex.has_value(), "Duplicate sheet index");

            JsonValue value;
            GG_CHECKB(prop.TryGetValue(value), "Failed to get property value");
            GG_CHECKB(value.TryGetUint32(sheetIndex.emplace()), "Failed to get sheet index");
        }
        else if(key == "cells")
        {
            GG_CHECKB(!cellsArray.has_value(), "Duplicate cells array");

            JsonValue value;
            GG_CHECKB(prop.TryGetValue(value), "Failed to get property value");
            GG_CHECKB(value.TryGetArray(cellsArray.emplace()), "Failed to get cells array");
        }
    }

    GG_CHECKB(sheetIndex.has_value(), "Missing sheet index");
    GG_CHECKB(cellsArray.has_value(), "Missing cells array");

    const size_t cellCount = cellsArray->size();
    BoundedVector<uint32_t> cells(cellCount);

    for(const JsonValue& cell : *cellsArray)
    {
        uint32_t cellValue{ 0 };

        GG_CHECKB(cell.TryGetUint32(cellValue));

        cells.push_back(cellValue);
    }

    return Layer{ *sheetIndex, std::move(cells) };
}

Result<BoundedVector<Layer>>
ParseLayers(const JsonArray& jsonLayers)
{
    const size_t count = jsonLayers.size();

    BoundedVector<Layer> layers(count);

    for(const JsonValue& value : jsonLayers)
    {
        JsonObject layer;
        GG_CHECKB(value.TryGetObject(layer), "Failed to get layer object");

        auto layerResult = ParseLayer(layer);
        GG_CHECKB(layerResult, "Failed to parse layer");
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
    std::optional<BoundedVector<SpriteDef>> optSpriteDefs;
    std::optional<BoundedVector<Layer>> optLayers;
    std::optional<BoundedVector<PropDef>> optPropDefs;

    for(const auto& prop : jobj)
    {
        if(std::string_view key; !GG_VERIFYB(prop.TryGetKey(key), "Failed to get property key"))
        {
            return Result<>::Error;
        }
        else if(JsonValue value;
            !GG_VERIFYB(prop.TryGetValue(value), "Failed to get property value"))
        {
            return Result<>::Error;
        }
        else if(key == "gridSize")
        {
            GG_CHECKB(!optGridSize.has_value(), "Duplicate \"gridSize\"");
            GG_CHECKB(value.TryGetUint32(optGridSize.emplace()), "Failed to get gridSize");
        }
        else if(key == "tileSheets")
        {
            GG_CHECKB(!optTileSheetPaths.has_value(), "Duplicate \"tileSheets\"");

            JsonArray array;
            GG_CHECKB(value.TryGetArray(array), "Failed to get tileSheets array");

            const size_t count = array.size();

            optTileSheetPaths.emplace(count);

            for(const auto& sheet : array)
            {
                std::string_view str;
                GG_CHECKB(sheet.TryGetString(str), "Failed to get sheet string");
                optTileSheetPaths->push_back(str);
            }
        }
        else if(key == "maskSheets")
        {
            GG_CHECKB(!optMaskSheetPaths.has_value(), "Duplicate \"maskSheets\"");

            JsonArray array;
            GG_CHECKB(value.TryGetArray(array), "Failed to get maskSheets array");

            const size_t count = array.size();

            optMaskSheetPaths.emplace(count);

            for(const auto& sheet : array)
            {
                std::string_view str;
                GG_CHECKB(sheet.TryGetString(str), "Failed to get sheet string");
                optMaskSheetPaths->push_back(str);
            }
        }
        else if(key == "propAssets")
        {
            GG_CHECKB(!optSpriteDefs.has_value(), "Duplicate \"propAssets\"");

            JsonArray array;
            GG_CHECKB(value.TryGetArray(array), "Failed to get propAssets array");

            auto propAssetsResult = ParsePropAssets(array);
            GG_CHECK(propAssetsResult, "Failed to parse prop assets");

            optSpriteDefs = std::move(*propAssetsResult);
        }
        else if(key == "props")
        {
            GG_CHECKB(!optPropDefs.has_value(), "Duplicate \"props\"");

            JsonArray array;
            GG_CHECK(value.TryGetArray(array), "Failed to get props array");
            auto propResult = ParseProps(array);
            GG_CHECK(propResult, "Failed to parse props");

            optPropDefs.emplace(std::move(*propResult));
        }
        else if(key == "layers")
        {
            GG_CHECKB(!optLayers.has_value(), "Duplicate \"layers\"");

            JsonArray array;
            GG_CHECK(value.TryGetArray(array), "Failed to get layers array");
            auto layerResult = ParseLayers(array);
            GG_CHECK(layerResult, "Failed to parse layer");

            optLayers.emplace(std::move(*layerResult));
        }
    }

    GG_CHECKB(optGridSize.has_value(), "Missing \"gridSize\"");
    GG_CHECKB(optTileSheetPaths.has_value(), "Missing \"tileSheets\"");
    GG_CHECKB(optMaskSheetPaths.has_value(), "Missing \"maskSheets\"");
    GG_CHECKB(optLayers.has_value(), "Missing \"layers\"");

    if(!optSpriteDefs || optSpriteDefs->empty())
    {
        GG_CHECKB(!optPropDefs || optPropDefs->empty(),
            "Props must be empty if no prop assets are defined");
    }

    BoundedVector<SpriteDef> spriteDefs =
        optSpriteDefs ? std::move(*optSpriteDefs) : BoundedVector<SpriteDef>(0);
    BoundedVector<PropDef> propDefs =
        optPropDefs ? std::move(*optPropDefs) : BoundedVector<PropDef>(0);

    return ParsedLevel //
        {
            .CellsPerSide = *optGridSize,
            .TileSheetPaths = std::move(*optTileSheetPaths),
            .MaskSheetPaths = std::move(*optMaskSheetPaths),
            .SpriteDefs = std::move(spriteDefs),
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
