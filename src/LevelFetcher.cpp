#include "LevelFetcher.h"

#include "BoundedVector.h"
#include "JsonDocument.h"
#include "Result.h"

#include <glm/vec2.hpp>
#include <optional>
#include <string_view>

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

class Prop
{
public:
    Prop() = delete;
    ~Prop() = default;
    Prop(const Prop&) = delete;
    Prop& operator=(const Prop&) = delete;
    Prop(Prop&&) = default;
    Prop& operator=(Prop&&) = default;

    Prop(uint32_t assetIndex, glm::vec2 position, float rotation)
        : m_AssetIndex(assetIndex),
          m_Position(position),
          m_Rotation(rotation)
    {
    }

    /// Returns the asset index of the prop.
    uint32_t GetAssetIndex() const { return m_AssetIndex; }

    /// Returns the position of the prop.
    glm::vec2 GetPosition() const { return m_Position; }

    /// Returns the rotation of the prop in degrees.
    float GetRotation() const { return m_Rotation; }

private:

    uint32_t m_AssetIndex;
    glm::vec2 m_Position;
    float m_Rotation;
};

Result<Prop>
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

    return Prop{ *assetIndex, glm::vec2(*posX, *posY), *rot };
}

Result<BoundedVector<Prop>>
ParseProps(const JsonArray& jsonProps)
{
    const size_t count = jsonProps.size();

    BoundedVector<Prop> props(count);

    for(const JsonValue& value : jsonProps)
    {
        JsonObject prop;
        GG_CHECKB(value.TryGetObject(prop), "Failed to get prop object");

        auto result = ParseProp(prop);
        GG_CHECKB(result, "Failed to parse prop");

        props.push_back(std::move(*result));
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

Result<>
Parse(std::vector<std::byte>&& data)
{
    auto docResult = JsonDocument::Create(std::move(data));
    GG_CHECK(docResult, "Failed to create JSON parser");

    const JsonObject jobj = docResult->GetRootObject();

    std::optional<uint32_t> gridSize;

    std::optional<BoundedVector<std::string_view>> tileSheets;
    std::optional<BoundedVector<std::string_view>> maskSheets;
    std::optional<BoundedVector<std::string_view>> propAssets;
    std::optional<BoundedVector<Layer>> layers;
    std::optional<BoundedVector<Prop>> props;

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
            GG_CHECKB(!gridSize.has_value(), "Duplicate \"gridSize\"");
            GG_CHECKB(value.TryGetUint32(gridSize.emplace()), "Failed to get gridSize");
        }
        else if(key == "tileSheets")
        {
            GG_CHECKB(!tileSheets.has_value(), "Duplicate \"tileSheets\"");

            JsonArray array;
            GG_CHECKB(value.TryGetArray(array), "Failed to get tileSheets array");

            const size_t count = array.size();

            tileSheets.emplace(count);

            for(const auto& sheet : array)
            {
                std::string_view str;
                GG_CHECKB(sheet.TryGetString(str), "Failed to get sheet string");
                tileSheets->push_back(str);
            }
        }
        else if(key == "maskSheets")
        {
            GG_CHECKB(!maskSheets.has_value(), "Duplicate \"maskSheets\"");

            JsonArray array;
            GG_CHECKB(value.TryGetArray(array), "Failed to get maskSheets array");

            const size_t count = array.size();

            maskSheets.emplace(count);

            for(const auto& sheet : array)
            {
                std::string_view str;
                GG_CHECKB(sheet.TryGetString(str), "Failed to get sheet string");
                maskSheets->push_back(str);
            }
        }
        else if(key == "propAssets")
        {
            GG_CHECKB(!propAssets.has_value(), "Duplicate \"propAssets\"");

            JsonArray array;
            GG_CHECKB(value.TryGetArray(array), "Failed to get propAssets array");

            const size_t count = array.size();

            propAssets.emplace(count);

            for(const auto& sheet : array)
            {
                std::string_view str;
                GG_CHECKB(sheet.TryGetString(str), "Failed to get sheet string");
                propAssets->push_back(str);
            }
        }
        else if(key == "props")
        {
            GG_CHECKB(!props.has_value(), "Duplicate \"props\"");

            JsonArray array;
            GG_CHECK(value.TryGetArray(array), "Failed to get props array");
            auto propResult = ParseProps(array);
            GG_CHECK(propResult, "Failed to parse props");

            props.emplace(std::move(*propResult));
        }
        else if(key == "layers")
        {
            GG_CHECKB(!layers.has_value(), "Duplicate \"layers\"");

            JsonArray array;
            GG_CHECK(value.TryGetArray(array), "Failed to get layers array");
            auto layerResult = ParseLayers(array);
            GG_CHECK(layerResult, "Failed to parse layer");

            layers.emplace(std::move(*layerResult));
        }
    }

    return Result<>::Ok;
}
} // namespace

LevelFetcher::LevelFetcher(FetchRequestPtr&& fetchRequest)
    : m_FetchRequest(std::move(fetchRequest))
{
}

Result<LevelFetcher>
LevelFetcher::Fetch(const FilePath& path, FileFetcher& fetcher)
{
    auto fetchResult = fetcher.Fetch(path);
    GG_CHECKB(fetchResult, "Failed to fetch file: {}", path.c_str());

    return LevelFetcher(std::move(*fetchResult));
}

Result<>
LevelFetcher::Update()
{
    switch(m_Status)
    {
        case Status::FetchingFile:
            if(!m_FetchRequest->IsPending())
            {
                m_Status = Status::LoadingLevel;
            }

            break;
        case Status::LoadingLevel:
        {
            m_Status = Status::Failed;

            auto dataResult = m_FetchRequest->TakeData();
            GG_CHECK(dataResult, "Failed to get data from fetch request");

            const auto parseResult = Parse(std::move(*dataResult));
            GG_CHECK(parseResult, "Failed to parse level data");
            
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
    return m_Status == Status::FetchingFile || m_Status == Status::LoadingLevel;
}
