#pragma once

#ifdef _WIN32

#include <cstddef>
#include <expected>
#include <limits>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

#ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
    #define NOMINMAX
#endif
#include <windows.h>

namespace unp::detail {

class created_process {
private:
    HANDLE handle_{nullptr};
    DWORD id_{0};

    void close_handle() noexcept {
        if (handle_ != nullptr && handle_ != INVALID_HANDLE_VALUE) {
            ::CloseHandle(handle_);
        }
        handle_ = nullptr;
        id_ = 0;
    }

public:
    created_process(HANDLE handle, DWORD id) noexcept
        : handle_(handle), id_(id) {}

    ~created_process() noexcept {
        close_handle();
    }

    created_process(const created_process&) = delete;
    created_process& operator=(const created_process&) = delete;

    created_process(created_process&& other) noexcept
        : handle_(std::exchange(other.handle_, nullptr)),
          id_(std::exchange(other.id_, 0)) {}

    created_process& operator=(created_process&& other) noexcept {
        if (this != &other) {
            close_handle();
            handle_ = std::exchange(other.handle_, nullptr);
            id_ = std::exchange(other.id_, 0);
        }
        return *this;
    }

    [[nodiscard]] HANDLE release_handle() noexcept {
        return std::exchange(handle_, nullptr);
    }

    [[nodiscard]] DWORD id() const noexcept {
        return id_;
    }
};

[[nodiscard]] inline std::string quote_windows_argument(
    std::string_view argument
) {
    if (!argument.empty() &&
        argument.find_first_of(" \t\"") == std::string_view::npos) {
        return std::string(argument);
    }

    std::string quoted;
    quoted.reserve(argument.size() + 2);
    quoted.push_back('"');

    std::size_t backslash_count = 0;
    for (const char character : argument) {
        if (character == '\\') {
            ++backslash_count;
            continue;
        }

        if (character == '"') {
            quoted.append(backslash_count * 2 + 1, '\\');
            quoted.push_back('"');
        }
        else {
            quoted.append(backslash_count, '\\');
            quoted.push_back(character);
        }
        backslash_count = 0;
    }

    quoted.append(backslash_count * 2, '\\');
    quoted.push_back('"');
    return quoted;
}

[[nodiscard]] inline std::expected<std::wstring, std::error_code>
utf8_to_wide(std::string_view str) {
    if (str.empty()) {
        return std::wstring{};
    }

    if (str.size() >
        static_cast<std::size_t>((std::numeric_limits<int>::max)())) {
        return std::unexpected(
            std::make_error_code(std::errc::value_too_large));
    }

    const int input_size = static_cast<int>(str.size());
    const int size_needed = ::MultiByteToWideChar(
        CP_UTF8,
        0,
        str.data(),
        input_size,
        nullptr,
        0
    );
    if (size_needed == 0) {
        return std::unexpected(std::error_code(
            static_cast<int>(::GetLastError()),
            std::system_category()
        ));
    }

    std::wstring result(static_cast<std::size_t>(size_needed), L'\0');
    if (::MultiByteToWideChar(
            CP_UTF8,
            0,
            str.data(),
            input_size,
            result.data(),
            size_needed
        ) == 0) {
        return std::unexpected(std::error_code(
            static_cast<int>(::GetLastError()),
            std::system_category()
        ));
    }

    return result;
}

[[nodiscard]] inline std::expected<created_process, std::error_code>
create_process(std::wstring command_line) {
    STARTUPINFOW startup_info{};
    startup_info.cb = sizeof(startup_info);

    PROCESS_INFORMATION process_info{};
    if (!::CreateProcessW(
            nullptr,
            command_line.data(),
            nullptr,
            nullptr,
            FALSE,
            0,
            nullptr,
            nullptr,
            &startup_info,
            &process_info
        )) {
        return std::unexpected(std::error_code(
            static_cast<int>(::GetLastError()),
            std::system_category()
        ));
    }

    ::CloseHandle(process_info.hThread);
    return created_process{
        process_info.hProcess,
        process_info.dwProcessId
    };
}

} // namespace unp::detail

#endif
