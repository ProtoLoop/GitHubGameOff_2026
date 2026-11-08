#include "LevelLoader.h"

#include "JsonDocument.h"

#include <optional>

namespace
{

struct Layer
{
    uint32_t m_SheetIndex;
    std::vector<uint32_t> m_Cells;
};

Result<Layer>
ParseLayers(const JsonArray& layers)
{
    for(const auto& val : layers)
    {
        const auto obj = val.AsObject();
        GG_CHECK(obj, "Failed to parse layer object");
        
        for(const auto& prop : *obj)
        {
            const auto key = prop.Key();
            GG_CHECK(key, "Failed to get property key");
            const auto value = prop.Value();
            GG_CHECK(value, "Failed to get property value");
            
            if(*key == "sheet")
            {
                const auto sheetIndex = value->AsUint32();
                GG_CHECK(sheetIndex, "Failed to parse sheet index");
                GG_INFO("Sheet: {}", *sheetIndex);
            }
            else if(*key == "cells")
            {
                const auto cells = value->AsArray();
                GG_CHECK(cells, "Failed to parse cells array");
                for(const auto& cell : *cells)
                {
                    const auto cellResult = cell.AsUint32();
                    GG_CHECK(cellResult, "Failed to parse cell");
                    GG_INFO("Cell: {}", *cellResult);
                }
            }
        }
    }

    return Layer{};
}

Result<>
Parse(std::vector<std::byte> data)
{
    auto docResult = JsonDocument::Create(std::move(data));
    GG_CHECK(docResult, "Failed to create JSON parser");

    const JsonObject jobj = docResult->GetRootObject();

    std::optional<uint32_t> gridSize, tileSize;

    for(const auto& prop : jobj)
    {
        const auto key = prop.Key();
        GG_CHECK(key, "Failed to get property key");
        const auto& value = prop.Value();
        GG_CHECK(value, "Failed to get property value");

        if(*key == "gridSize")
        {
            const auto valueResult = value->AsUint32();
            GG_CHECK(valueResult, "Failed to parse gridSize");
            gridSize = *valueResult;
        }
        else if(*key == "tileSize")
        {
            const auto valueResult = value->AsUint32();
            GG_CHECK(valueResult, "Failed to parse tileSize");
            tileSize = *valueResult;
        }
        else if(*key == "tileSheets")
        {
            const auto sheets = value->AsArray();
            GG_CHECK(sheets, "Failed to parse tileSheets array");
            for(const auto& sheet : *sheets)
            {
                const auto sheetResult = sheet.AsString();
                GG_CHECK(sheetResult, "Failed to parse tile sheet");
                GG_INFO("Parsing tile sheet: {}", *sheetResult);
            }
        }
        else if(*key == "maskSheets")
        {
            const auto sheets = value->AsArray();
            GG_CHECK(sheets, "Failed to parse maskSheets array");
            for(const auto& sheet : *sheets)
            {
                const auto sheetResult = sheet.AsString();
                GG_CHECK(sheetResult, "Failed to parse tile sheet");
                GG_INFO("Parsing tile sheet: {}", *sheetResult);
            }
        }
        else if(*key == "layers")
        {
            auto objResult = value->AsArray();
            GG_CHECK(objResult, "Failed to parse layer object");
            const auto layerResult = ParseLayers(*objResult);
            GG_CHECK(layerResult, "Failed to parse layer");
        }
    }

    return Result<>::Success();
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

void
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
            m_Status = Status::Failed;

            if(auto dataResult = m_FetchRequest->TakeData(); !dataResult)
            {
                GG_ERROR("Failed to get data from fetch request");
            }
            else if(const auto parseResult = Parse(std::move(*dataResult)); !parseResult)
            {
                GG_ERROR("Failed to parse level data");
            }
            else
            {
                m_Status = Status::Loaded;
            }
            break;
        case Status::Loaded:
        case Status::Failed:
        case Status::Taken:
            break;
    }
}

bool
LevelLoader::IsPending() const
{
    return m_Status == Status::FetchingFile || m_Status == Status::LoadingLevel;
}
