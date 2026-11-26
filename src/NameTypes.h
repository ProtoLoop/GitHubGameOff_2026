#pragma once

#include "InlineString.h"
#include "Result.h"

namespace detail
{
constexpr size_t kResourceNameLength = 64 - 1;
constexpr size_t kMaxFilePathLength = 256 - 1;
} // namespace detail

class ResourceName : public InlineString<detail::kResourceNameLength + 1>
{
public:
    ResourceName() = delete;

    static Result<ResourceName> Create(const std::string_view name)
    {
        GG_CHECKB(name.size() <= detail::kResourceNameLength, "Resource name too long: {}", name);
        return ResourceName(name);
    }

private:
    constexpr explicit ResourceName(const std::string_view name)
        : InlineString<detail::kResourceNameLength + 1>(name)
    {
    }
};

class FilePath : public InlineString<detail::kMaxFilePathLength + 1>
{
public:
    static const FilePath kEmpty;

    FilePath() = delete;

    static Result<FilePath> Create(const std::string_view name)
    {
        GG_CHECKB(name.size() <= detail::kMaxFilePathLength, "File path too long: {}", name);
        return FilePath(name);
    }

private:
    constexpr explicit FilePath(const std::string_view name)
        : InlineString<detail::kMaxFilePathLength + 1>(name)
    {
    }
};

inline constexpr FilePath FilePath::kEmpty{ "" };

/// Formatter specialization for ResourceName.
template<>
struct std::formatter<ResourceName> : std::formatter<std::string_view>
{
    auto format(const ResourceName& str, auto& context) const
    {
        return std::formatter<std::string_view>::format(std::string_view(str.c_str(), str.size()),
            context);
    }
};

/// Formatter specialization for FilePath.
template<>
struct std::formatter<FilePath> : std::formatter<std::string_view>
{
    auto format(const FilePath& str, auto& context) const
    {
        return std::formatter<std::string_view>::format(std::string_view(str.c_str(), str.size()),
            context);
    }
};