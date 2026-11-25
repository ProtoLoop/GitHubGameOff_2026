#pragma once

#include "InlineString.h"

namespace detail
{
constexpr size_t kResourceNameLength = 64 - 1;
constexpr size_t kMaxFilePathLength = 256 - 1;
}

using ResourceName = InlineString<detail::kResourceNameLength + 1>;

using FilePath = InlineString<detail::kMaxFilePathLength + 1>;

