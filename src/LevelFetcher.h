#pragma once

#include "FileFetcher.h"
#include "Result.h"

class LevelFetcher
{
public:
    static Result<LevelFetcher> Fetch(const FilePath& path, FileFetcher& fetcher);

    Result<> Update();

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

    explicit LevelFetcher(FetchRequestPtr&& fetchRequest);

    FetchRequestPtr m_FetchRequest;

    Status m_Status{ Status::FetchingFile };
};