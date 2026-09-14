#pragma once

#include <cerrno>
#include <cstdio>

namespace unp::fs {

// The caller owns the returned FILE and must close it with std::fclose.
[[nodiscard]] inline FILE* open_file(
    const char* filename,
    const char* mode
) noexcept {
    if (filename == nullptr || mode == nullptr) {
        errno = EINVAL;
        return nullptr;
    }

#ifdef _MSC_VER
    FILE* file = nullptr;
    const errno_t error = ::fopen_s(&file, filename, mode);
    if (error != 0) {
        errno = error;
        return nullptr;
    }
    return file;
#else
    return ::fopen(filename, mode);
#endif
}

} // namespace unp::fs
