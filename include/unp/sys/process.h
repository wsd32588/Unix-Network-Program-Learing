#pragma once

#include <cerrno>
#include <cstdint>
#include <expected>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#ifdef _WIN32
    #include "unp/detail/windows_process.h"
#else
    #include "unp/detail/linux_process.h"
#endif

namespace unp::sys {

#ifdef _WIN32
using process_id_t = DWORD;
using native_process_handle_t = HANDLE;
inline constexpr native_process_handle_t invalid_process_handle = nullptr;
inline constexpr process_id_t invalid_process_id = 0;
#else
using process_id_t = pid_t;
inline constexpr process_id_t invalid_process_id = -1;
#endif

enum class process_exit_kind {
    exited,
    signaled
};

struct process_exit_status {
    process_exit_kind kind;
    std::uint32_t value;

    [[nodiscard]] bool exited_normally() const noexcept {
        return kind == process_exit_kind::exited;
    }
};

class process {
private:
#ifdef _WIN32
    using created_process = unp::detail::created_process;

    native_process_handle_t handle_{invalid_process_handle};
#else
    using created_process = unp::detail::fork_result;
#endif
    process_id_t id_{invalid_process_id};

#ifdef _WIN32
    [[nodiscard]] static bool is_valid_handle(
        native_process_handle_t handle
    ) noexcept {
        return handle != invalid_process_handle &&
            handle != INVALID_HANDLE_VALUE;
    }

    explicit process(created_process&& created) noexcept
        : handle_(created.release_handle()),
          id_(created.id()) {}
#else
    explicit process(created_process&& created) noexcept {
        if (created.in_parent()) {
            id_ = created.returned_id;
        }
    }
#endif

    void close_handle() noexcept {
#ifdef _WIN32
        if (is_valid_handle(handle_)) {
            ::CloseHandle(handle_);
        }
        handle_ = invalid_process_handle;
#endif
        id_ = invalid_process_id;
    }

public:
    process() noexcept = default;

    ~process() noexcept {
        close_handle();
    }

    process(const process&) = delete;
    process& operator=(const process&) = delete;

    process(process&& other) noexcept
#ifdef _WIN32
        : handle_(std::exchange(other.handle_, invalid_process_handle)),
          id_(std::exchange(other.id_, invalid_process_id)) {}
#else
        : id_(std::exchange(other.id_, invalid_process_id)) {}
#endif

    process& operator=(process&& other) noexcept {
        if (this != &other) {
            close_handle();
#ifdef _WIN32
            handle_ = std::exchange(
                other.handle_,
                invalid_process_handle
            );
#endif
            id_ = std::exchange(other.id_, invalid_process_id);
        }
        return *this;
    }

    [[nodiscard]] bool valid() const noexcept {
#ifdef _WIN32
        return id_ != invalid_process_id && is_valid_handle(handle_);
#else
        return id_ > 0;
#endif
    }

    [[nodiscard]] explicit operator bool() const noexcept {
        return valid();
    }

    [[nodiscard]] process_id_t id() const noexcept {
        return id_;
    }

#ifdef _WIN32
    [[nodiscard]] native_process_handle_t native_handle() const noexcept {
        return handle_;
    }
#endif

    [[nodiscard]] std::expected<process_exit_status, std::error_code> wait() {
        if (!valid()) {
            return std::unexpected(
                std::make_error_code(std::errc::bad_file_descriptor));
        }

#ifdef _WIN32
        const DWORD wait_result = ::WaitForSingleObject(handle_, INFINITE);
        if (wait_result == WAIT_FAILED) {
            return std::unexpected(std::error_code(
                static_cast<int>(::GetLastError()),
                std::system_category()
            ));
        }
        if (wait_result != WAIT_OBJECT_0) {
            return std::unexpected(
                std::make_error_code(std::errc::io_error));
        }

        DWORD exit_code = 0;
        const bool exit_ok = ::GetExitCodeProcess(handle_, &exit_code);
        const DWORD error = exit_ok ? 0 : ::GetLastError();

        close_handle();
        if (!exit_ok) {
            return std::unexpected(std::error_code(
                static_cast<int>(error),
                std::system_category()
            ));
        }

        return process_exit_status{
            process_exit_kind::exited,
            static_cast<std::uint32_t>(exit_code)
        };
#else
        int status = 0;
        process_id_t result = 0;

        do {
            result = ::waitpid(id_, &status, 0);
        } while (result < 0 && errno == EINTR);

        if (result < 0) {
            return std::unexpected(
                std::error_code(errno, std::generic_category()));
        }

        if (WIFEXITED(status)) {
            id_ = invalid_process_id;
            return process_exit_status{
                process_exit_kind::exited,
                static_cast<std::uint32_t>(WEXITSTATUS(status))
            };
        }
        if (WIFSIGNALED(status)) {
            id_ = invalid_process_id;
            return process_exit_status{
                process_exit_kind::signaled,
                static_cast<std::uint32_t>(WTERMSIG(status))
            };
        }

        return std::unexpected(
            std::make_error_code(std::errc::operation_in_progress));
#endif
    }

#ifndef _WIN32
    [[nodiscard]] static std::expected<process, std::error_code>
    fork_process() noexcept {
        auto created = unp::detail::fork_id();
        if (!created) {
            return std::unexpected(created.error());
        }

        return process(std::move(*created));
    }
#endif

    [[nodiscard]] static std::expected<process, std::error_code>
    spawn_process(
        const std::string& program,
        const std::vector<std::string>& args
    ) {
#ifdef _WIN32
        std::string command_line =
            unp::detail::quote_windows_argument(program);
        for (const auto& argument : args) {
            command_line.push_back(' ');
            command_line +=
                unp::detail::quote_windows_argument(argument);
        }

        auto wide_command = unp::detail::utf8_to_wide(command_line);
        if (!wide_command) {
            return std::unexpected(wide_command.error());
        }

        auto created =
            unp::detail::create_process(std::move(*wide_command));
#else
        auto created =
            unp::detail::spawn_native_process(program, args);
#endif
        if (!created) {
            return std::unexpected(created.error());
        }

        return process(std::move(*created));
    }
};

} // namespace unp::sys
