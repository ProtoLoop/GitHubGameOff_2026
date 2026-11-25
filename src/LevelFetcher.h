#pragma once

#include "FileFetcher.h"
#include "LevelLoader.h"

class Level;
class Renderer;
class WebGpu;
class AudioPlayer;

class LevelFetcher
{
public:
    static Result<LevelFetcher> Fetch(const FilePath& path,
        FileFetcher& fetcher,
        Renderer& renderer,
        WebGpu& webGpu,
        AudioPlayer& player);

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

    explicit LevelFetcher(FetchRequestPtr&& fetchRequest,
        FileFetcher& fetcher,
        Renderer& renderer,
        WebGpu& webGpu,
        AudioPlayer& player);

    FetchRequestPtr m_FetchRequest;
    FileFetcher* m_Fetcher;
    Renderer* m_Renderer;
    WebGpu* m_WebGpu;
    AudioPlayer* m_Player;

    std::optional<LevelLoader> m_LevelLoader;

    Status m_Status{ Status::FetchingFile };
};