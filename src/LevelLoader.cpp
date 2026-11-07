#include "LevelLoader.h"

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
            // Load the level from the fetched data
            // For now, we just mark it as succeeded
            m_Status = Status::Loaded;
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
