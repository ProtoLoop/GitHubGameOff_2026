#include "LevelLoader.h"

#include "BoundedVector.h"
#include "JsonDocument.h"

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
            if(!GG_VERIFYB(prop.TryGetValue(value), "Failed to get property value")
                || !GG_VERIFYB(value.TryGetUint32(sheetIndex.emplace()),
                    "Failed to get sheet index"))
            {
                return Result<>::Error;
            }
        }
        else if(key == "cells")
        {
            GG_CHECKB(!cellsArray.has_value(), "Duplicate cells array");

            JsonValue value;
            if(!GG_VERIFYB(prop.TryGetValue(value), "Failed to get property value")
                || !GG_VERIFYB(value.TryGetArray(cellsArray.emplace()),
                    "Failed to get cells array"))
            {
                return Result<>::Error;
            }
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

        GG_CHECKB(cells.push_back(cellValue));
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
        if(!GG_VERIFYB(value.TryGetObject(layer), "Failed to get layer object"))
        {
            return Result<>::Error;
        }

        auto layerResult = ParseLayer(layer);
        GG_CHECKB(layerResult, "Failed to parse layer");
        GG_CHECKB(layers.push_back(std::move(*layerResult)));
    }

    return std::move(layers);
}

Result<>
Parse(std::vector<std::byte> data)
{
    auto docResult = JsonDocument::Create(std::move(data));
    GG_CHECK(docResult, "Failed to create JSON parser");

    const JsonObject jobj = docResult->GetRootObject();

    std::optional<uint32_t> gridSize, tileSize;

    std::optional<BoundedVector<std::string_view>> tileSheets;
    std::optional<BoundedVector<std::string_view>> maskSheets;
    std::optional<BoundedVector<Layer>> layers;

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
        else if(key == "tileSize")
        {
            GG_CHECKB(!tileSize.has_value(), "Duplicate \"tileSize\"");
            GG_CHECKB(value.TryGetUint32(tileSize.emplace()), "Failed to get tileSize");
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
                GG_CHECKB(tileSheets->push_back(str));
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
                GG_CHECKB(maskSheets->push_back(str));
            }
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

LevelLoader::LevelLoader(FetchRequestPtr&& fetchRequest)
    : m_FetchRequest(std::move(fetchRequest))
{
}

Result<LevelLoader>
LevelLoader::Load(const FilePath& path, FileFetcher& fetcher)
{
    auto fetchResult = fetcher.Fetch(path);
    GG_CHECKB(fetchResult, "Failed to fetch file: {}", path.c_str());

    return LevelLoader(std::move(*fetchResult));
}

Result<>
LevelLoader::Update()
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
LevelLoader::IsPending() const
{
    return m_Status == Status::FetchingFile || m_Status == Status::LoadingLevel;
}
