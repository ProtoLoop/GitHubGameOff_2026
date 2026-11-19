#pragma once

#include "FileFetcher.h"
#include "Level.h"

class LevelFetcher
{
public:
    static Result<LevelFetcher> Fetch(
        const FilePath& path, FileFetcher& fetcher, Renderer& renderer, WebGpu& webGpu);

    Result<> Update();

    bool IsPending() const;

    Result<std::unique_ptr<Level>> Take();

private:
    enum class Status
    {
        FetchingFile,
        ParsingLevel,
        LoadingLevel,
        Loaded,
        Failed,
        Taken
    };

    explicit LevelFetcher(
        FetchRequestPtr&& fetchRequest, FileFetcher& fetcher, Renderer& renderer, WebGpu& webGpu);

    FetchRequestPtr m_FetchRequest;
    FileFetcher* m_Fetcher;
    Renderer* m_Renderer;
    WebGpu* m_WebGpu;

    std::optional<LevelLoader> m_LevelLoader;

    Status m_Status{ Status::FetchingFile };
};