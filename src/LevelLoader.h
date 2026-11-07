#pragma once

#include "FileFetcher.h"
#include "Result.h"

class LevelLoader
{
public:
    static Result<LevelLoader> Load(const FilePath& path, FileFetcher& fetcher);

    void Update();

    bool IsPending() const;

private:

    enum class Status
    {
        FetchingFile,
        LoadingLevel,
        Loaded,
        Failed,
        Taken
    };

    explicit LevelLoader(FetchRequestPtr&& fetchRequest);

    FetchRequestPtr m_FetchRequest;

    Status m_Status{ Status::FetchingFile };
};