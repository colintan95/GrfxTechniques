#pragma once

namespace utils
{

inline size_t Align(size_t value, size_t alignment)
{
    return ((value - 1) / alignment + 1) * alignment;
}

} // namespace utils
