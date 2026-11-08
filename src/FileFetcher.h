#pragma once

#include "InlineString.h"
#include "InlineVector.h"
#include "Result.h"

#include <memory>
#include <span>
#include <vector>

#ifdef __EMSCRIPTEN__
struct emscripten_fetch_t;
#endif

static constexpr size_t kMaxFilePathLength = 256 - 1;
using FilePath = InlineString<kMaxFilePathLength + 1>;

class FileFetcher;

class FileFetchRequest
{
    // FileFetchRequest needs a public ctor so it can be constructed using emplace().
    // But we still want to restrict its construction to FileFetcher only.
    // Because only FileFetcher can create a PassKey, only FileFetcher can construct
    // FileFetchRequest instances.
    struct PassKey
    {
        explicit PassKey(FileFetcher* fetcher)
            : m_Fetcher(fetcher)
        {
        }

        FileFetcher* m_Fetcher{ nullptr };
    };

public:
    explicit FileFetchRequest(const PassKey& passkey)
        : m_Path(""),
          m_Fetcher(passkey.m_Fetcher)
    {
    }

    ~FileFetchRequest();
    FileFetchRequest(const FileFetchRequest&) = delete;
    FileFetchRequest& operator=(const FileFetchRequest&) = delete;
    FileFetchRequest(FileFetchRequest&&) = delete;
    FileFetchRequest& operator=(FileFetchRequest&&) = delete;

    bool IsPending() const { return m_Status == Status::Pending; }

    /// Borrows the fetched bytes without consuming the request.
    /// Requires a successful request whose data has not been taken.
    /// The returned span is valid until TakeData() succeeds or the request is released.
    /// The FileFetcher must outlive the request and any use of the span.
    Result<std::span<const std::byte>> GetData() const; // FIXME(KB) - implement borrowing semantics correctly

    /// Returns independently owned bytes and consumes the request's data.
    /// Requires a successful request whose data has not been taken.
    /// Moves the native buffer, or copies the browser buffer and closes the fetch.
    /// On success, any previous spans borrowed from GetData() become invalid.
    /// On failure, leaves the request unchanged.
    /// Does not release the request's pool slot. Release the FetchRequestPtr to do that.
    Result<std::vector<std::byte>> TakeData();

private:
    friend class FileFetcher;
    friend class ReleaseFetcher;

    const FilePath& GetPath() const { return m_Path; }

    void AddToList(FileFetchRequest*& list)
    {
        m_Next = list;
        if(list)
        {
            list->m_Prev = this;
        }
        list = this;
    }

    void RemoveFromList(FileFetchRequest*& list)
    {
        if(list == this)
        {
            list = m_Next;
        }
        Unlink();
    }

    void Unlink()
    {
        if(m_Prev)
        {
            m_Prev->m_Next = m_Next;
        }
        if(m_Next)
        {
            m_Next->m_Prev = m_Prev;
        }
        m_Next = nullptr;
        m_Prev = nullptr;
    }

    enum class Status
    {
        None,
        Pending,
        Succeeded,
        Failed,
        Taken
    };

    FileFetchRequest* m_Next{ nullptr };
    FileFetchRequest* m_Prev{ nullptr };
    FilePath m_Path;
    FileFetcher* const m_Fetcher{ nullptr };

#ifdef __EMSCRIPTEN__
    emscripten_fetch_t* m_Fetch{ nullptr };
#else
    std::vector<std::byte> m_Data;
#endif
    Status m_Status{ Status::None };

    // If true then there are no outstanding references to this request and it can be safely reused
    // once it is no longer pending.
    bool m_Released{ true };
};

// Custom deleter used with std::unique_ptr so we can allocate fetch requests from
// a pool and still enforce ownership semantics.
class ReleaseFetcher
{
public:
    void operator()(FileFetchRequest* rqst) const;
};

using FetchRequestPtr = std::unique_ptr<FileFetchRequest, ReleaseFetcher>;

class FileFetcher
{
public:
    FileFetcher();

    Result<FetchRequestPtr> Fetch(const FilePath& path);

private:
    friend class ReleaseFetcher;
    friend class FileFetchRequest;

    /// Disposes of a fetch request, releasing resources, and returning it to the free list if it is
    /// no longer pending.
    void Dispose(FileFetchRequest* request);

#ifdef __EMSCRIPTEN__
    static void onSuccess(emscripten_fetch_t* fetch);
    static void onError(emscripten_fetch_t* fetch);
#endif

    static constexpr size_t kRequestHeapSize = 32;

    InlineVector<FileFetchRequest, kRequestHeapSize> m_RequestHeap;

    FileFetchRequest* m_FreeList{ nullptr };
    FileFetchRequest* m_PendingList{ nullptr };
};