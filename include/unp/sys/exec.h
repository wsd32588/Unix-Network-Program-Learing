#pragma once
#ifndef _WIN32

#include <cerrno>
#include <expected>
#include <system_error>
#include <vector>
#include <span>
#include <string>
#include <string_view>
#include <unistd.h>

namespace unp::sys {

[[nodiscard]] inline std::expected<void, std::error_code> exec_program(
    const char* file,
    char* const arguments[]
) noexcept {
    ::execvp(file, arguments);

    return std::unexpected(
        std::error_code(errno, std::generic_category())
    );
}

[[nodiscard]] inline std::expected<void, std::error_code> exec_program(
    std::string_view file,
    std::span<const char* const> args
) {
    std::vector<char*> argv;
    argv.reserve(args.size() + 2);

    std::string file_string(file);
    // file_string remains unchanged and alive until exec_program returns.
    argv.push_back(file_string.data());

    for (const char* argument : args) {
        argv.push_back(const_cast<char*>(argument));
    }
    argv.push_back(nullptr);

    return exec_program(file_string.c_str(), argv.data());
}

} // namespace unp::sys

#endif
