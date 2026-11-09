#include "FileFetcher.h"

#include "Shortcuts.h"

#ifdef __EMSCRIPTEN__
#include <emscripten/fetch.h>
#include <limits>
#else

#include <cerrno>
#include <cstdio>
#endif

namespace
{
class FileCloser
{
public:
    void operator()(std::FILE* file) const
    {
        if(file)
        {
            // NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
            std::fclose(file);
        }
    }
};
using FilePtr = std::unique_ptr<std::FILE, FileCloser>;
} // namespace

/// ReleaseFetcher

void
ReleaseFetcher::operator()(FileFetchRequest* rqst) const
{
    if(rqst)
    {
        // Shouldn't be able to release more than once.
        GG_ASSERT(!rqst->m_Released, "Request has already been released");
        rqst->m_Released = true;

        // If the request is still pending it will be disposed once it completes.
        // Otherwise dispose it now.

        if(!rqst->IsPending())
        {
            rqst->m_Fetcher->Dispose(rqst);
        }
    }
}

/// FileFetchRequest

FileFetchRequest::~FileFetchRequest()
{
#ifdef __EMSCRIPTEN__
    if(m_Fetch)
    {
        // Clear the userData so onError or onSuccess won't access it
        // if closing the fetch triggers those callbacks.
        m_Fetch->userData = nullptr;
        emscripten_fetch_close(m_Fetch);
        m_Fetch = nullptr;
    }
#endif
}

Result<std::span<const std::byte>>
FileFetchRequest::GetData() const
{
    GG_CHECKB(!IsPending(), "Request is still pending");
    GG_CHECKB(m_Status != Status::Taken, "Data has already been taken");
    GG_CHECK(m_Status == Status::Succeeded, "Request did not succeed");

#ifdef __EMSCRIPTEN__
    GG_CHECKB(m_Fetch, "Fetch object is null");
    GG_CHECKB(m_Fetch->numBytes <= std::numeric_limits<size_t>::max(),
        "Data size exceeds maximum allowed size");

    return std::span(recast<const std::byte*>(m_Fetch->data), scast<size_t>(m_Fetch->numBytes));
#else

    return std::span<const std::byte>(m_Data);
#endif
}

Result<std::vector<std::byte>>
FileFetchRequest::TakeData()
{
    GG_CHECKB(!IsPending(), "Request is still pending");
    GG_CHECKB(m_Status != Status::Taken, "Data has already been taken");
    GG_CHECK(m_Status == Status::Succeeded, "Request did not succeed");

    std::vector<std::byte> data;

#ifdef __EMSCRIPTEN__
    GG_CHECKB(m_Fetch, "Fetch object is null");
    GG_CHECKB(m_Fetch->numBytes <= std::numeric_limits<size_t>::max(),
        "Data size exceeds maximum allowed size");

    const std::span sp =
        std::span(recast<const std::byte*>(m_Fetch->data), scast<size_t>(m_Fetch->numBytes));

    data.assign(sp.begin(), sp.end());

    emscripten_fetch_close(m_Fetch);
    m_Fetch = nullptr;
#else

    data = std::move(m_Data);
#endif

    m_Status = Status::Taken;
    return std::move(data);
}

/// FileFetcher

FileFetcher::FileFetcher()
{
    for(size_t i = 0; i < m_RequestHeap.capacity(); ++i)
    {
        const Result<> result = m_RequestHeap.emplace_back(FileFetchRequest::PassKey(this));
        if(!GG_VERIFYB(result))
        {
            continue;
        }

        FileFetchRequest* rqst = m_RequestHeap.TryGet(i);

        rqst->AddToList(m_FreeList);
    }
}

Result<FetchRequestPtr>
// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
FileFetcher::Fetch(const FilePath& path)
{
    GG_INFO("Fetching: {}", path.c_str());

    GG_CHECKB(m_FreeList, "Fetch request pool exhausted");

    FileFetchRequest* request = m_FreeList;

    GG_ASSERT(request->m_Released, "Request has not been released");

#ifdef __EMSCRIPTEN__
    emscripten_fetch_attr_t attr;
    emscripten_fetch_attr_init(&attr);
    std::ranges::copy("GET", std::begin(attr.requestMethod));
    attr.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY;
    attr.onsuccess = FileFetcher::onSuccess;
    attr.onerror = FileFetcher::onError;
    attr.userData = request;

    request->m_Path = path;
    request->m_Fetch = emscripten_fetch(&attr, path.c_str());
    GG_CHECKB(request->m_Fetch, "Failed to initiate fetch");

    request->m_Status = FileFetchRequest::Status::Pending;

#else

    std::string_view sv = path;

    while(!sv.empty() && sv.front() == '/')
    {
        sv.remove_prefix(1);
    }

    const FilePath localPath(sv);

    const FilePtr file(std::fopen(localPath.c_str(), "rb"));
    GG_CHECKB(file, "Failed to open file: {} - {}", localPath.c_str(), std::strerror(errno));

    std::vector<std::byte> result;
    std::byte bytes[8192]; // NOLINT(readability-magic-numbers)

    const std::span buffer(bytes);

    for(; !std::feof(file.get());)
    {
        const auto count = std::fread(buffer.data(), 1, buffer.size(), file.get());

        GG_CHECKB(!std::ferror(file.get()),
            "Failed to read file: {} - {}",
            path.c_str(),
            std::strerror(errno));

        result.append_range(buffer.first(count));
    }

    request->m_Data = std::move(result);

    request->m_Status = FileFetchRequest::Status::Succeeded;

#endif // __EMSCRIPTEN__

    request->RemoveFromList(m_FreeList);
    request->AddToList(m_PendingList);
    request->m_Released = false;

    return FetchRequestPtr(request);
}

void
FileFetcher::Dispose(FileFetchRequest* request)
{
    if(request)
    {
        GG_ASSERT(request->m_Released, "Request has not been released");
        GG_ASSERT(!request->IsPending(), "Cannot dispose a pending request");

#ifdef __EMSCRIPTEN__
        if(request->m_Fetch)
        {
            emscripten_fetch_close(request->m_Fetch);
            request->m_Fetch = nullptr;
        }
#else
        // Release the memory held by the data vector
        auto bye = std::move(request->m_Data);
#endif
        request->RemoveFromList(m_PendingList);
        request->AddToList(m_FreeList);
        request->m_Path = "";
        request->m_Status = FileFetchRequest::Status::None;
        request->m_Released = true;
    }
}

#ifdef __EMSCRIPTEN__

void
FileFetcher::onSuccess(emscripten_fetch_t* fetch)
{
    FileFetchRequest* request = static_cast<FileFetchRequest*>(fetch->userData);
    if(request == nullptr)
    {
        // request is null in the case where the request dtor was called
        // and the fetch was closed.
        return;
    }

    GG_INFO("Fetched: {}", request->GetPath());

    request->m_Status = FileFetchRequest::Status::Succeeded;

    if(request->m_Released)
    {
        request->m_Fetcher->Dispose(request);
    }
}

void
FileFetcher::onError(emscripten_fetch_t* fetch)
{
    FileFetchRequest* request = static_cast<FileFetchRequest*>(fetch->userData);
    if(request == nullptr)
    {
        // request is null in the case where the request dtor was called
        // and the fetch was closed.
        return;
    }

    GG_INFO("Fetch failed: {} - HTTP {}", request->GetPath(), fetch->status);

    request->m_Status = FileFetchRequest::Status::Failed;

    if(request->m_Released)
    {
        request->m_Fetcher->Dispose(request);
    }
}

#endif // __EMSCRIPTEN__