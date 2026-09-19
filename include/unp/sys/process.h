#pragma once
#include <cerrno>
#include <cstdint>
#include <expected>
#include <limits>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include "exec.h"

#ifdef _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
    #define NOMINMAX
#endif
    #include <windows.h>
#else
    #include <sys/types.h>
    #include <sys/wait.h>
    #include <unistd.h>
#endif

namespace unp::sys {

#ifdef _WIN32
namespace detail {

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

} // namespace detail
#endif

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
#ifndef _WIN32
    struct fork_result {
        process_id_t returned_id;

        [[nodiscard]] bool in_child() const noexcept {
            return returned_id == 0;
        }

        [[nodiscard]] bool in_parent() const noexcept {
            return returned_id > 0;
        }
    };
#endif
    struct adopt_process_t {
        explicit adopt_process_t() = default;
    };
    inline constexpr adopt_process_t adopt_process_handle{};

    class process {
    private:
#ifdef _WIN32
        native_process_handle_t handle_{ invalid_process_handle };
        static bool is_valid_handle(native_process_handle_t handle) {
            return handle != invalid_process_handle && handle != INVALID_HANDLE_VALUE;
        }
#endif
        process_id_t id_{ invalid_process_id };

        void close_handle() noexcept {
#ifdef _WIN32
            if (handle_ != invalid_process_handle) {
                ::CloseHandle(handle_);
                handle_ = invalid_process_handle;
            }
#endif
            id_ = invalid_process_id;
        }

    public:

        process() noexcept = default;
#ifdef _WIN32
        process(adopt_process_t,
            native_process_handle_t& handle,
            process_id_t id = 0) noexcept {
            if (!is_valid_handle(handle)) {
                return;
            }

            const DWORD real_pid = ::GetProcessId(handle);
            if (real_pid == 0 ||
                (id != 0 && id != real_pid)
                ) {
                return;
            }
            handle_ = std::exchange(handle,invalid_process_handle);
            id_ = real_pid;
        }
#else
        explicit process(adopt_process_t,process_id_t id) noexcept {
            if (id > 0) {
                id_ = id;
            }
        }
#endif
        ~process() noexcept {
            close_handle();
        }

        process(const process&) = delete;
        process& operator=(const process&) = delete;

        process(process&& other) noexcept
#ifdef _WIN32
            :handle_(std::exchange(other.handle_, invalid_process_handle)),
            id_(std::exchange(other.id_, invalid_process_id)) {
        }
#else
            : id_(std::exchange(other.id_, invalid_process_id)) {}
#endif
        process& operator=(process&& other) noexcept {
            if (this != &other) {
                close_handle();
#ifdef _WIN32
                handle_ = std::exchange(other.handle_, invalid_process_handle);
#endif
                id_ = std::exchange(other.id_, invalid_process_id);
            }
            return *this;
        }

        [[nodiscard]] inline bool valid() const noexcept {
#ifdef _WIN32
            return  id_ != invalid_process_id &&
                handle_ != invalid_process_handle;
#else
            return id_ > 0;
#endif
        }
        [[nodiscard]] inline explicit operator bool() const noexcept {
            return valid();
        }

        [[nodiscard]] inline process_id_t id() const noexcept {
            return id_;
        }


#ifdef _WIN32
        [[nodiscard]] inline native_process_handle_t native_handle() const noexcept {
            return handle_;
        }
#endif

#ifdef _WIN32

        [[nodiscard]] static std::expected<std::wstring,std::error_code> utf8_to_wide(
            std::string_view str
        ) {
            if (str.empty()) {
                return {};
            }
            
            int size_needed{};
            if (str.size() > std::numeric_limits<int>::max()) {
                return std::unexpected(
                    std::make_error_code(std::errc::value_too_large));
            }

            if (!(size_needed = ::MultiByteToWideChar(
                CP_UTF8, 0, str.data(), static_cast<int>(str.size()), nullptr, 0
            ))) {
                return std::unexpected(
                    std::error_code(
                        ::GetLastError(), std::system_category())
                );
            }
            std::wstring result(size_needed, L'\0');

            if (!(::MultiByteToWideChar(
                CP_UTF8, 0, str.data(), static_cast<int>(str.size()), result.data(), size_needed
            ))) {
                return std::unexpected(
                    std::error_code(
                        ::GetLastError(), std::system_category())
                );
            }
            return result;
        }

        [[nodiscard]] static std::expected<process, std::error_code> create_process(
            std::wstring command_line
        ) {
            STARTUPINFOW startup_info{};
            startup_info.cb = sizeof(startup_info);

            PROCESS_INFORMATION process_info{};

            if (!::CreateProcessW(
                nullptr, command_line.data(), nullptr, nullptr,
                FALSE, 0, nullptr, nullptr, &startup_info, &process_info
            )) {
                return std::unexpected(
                    std::error_code(
                        static_cast<int>(::GetLastError()),
                        std::system_category()
                    ));
            }

            ::CloseHandle(process_info.hThread);

            process child;
            child.handle_ = process_info.hProcess;
            child.id_ = process_info.dwProcessId;

            return child;
        }
#endif

        [[nodiscard]] std::expected<process_exit_status, std::error_code> wait() {
            if (!valid()) {
                return std::unexpected(
                    std::make_error_code(
                        std::errc::bad_file_descriptor));
            }
#ifdef _WIN32
            const DWORD wait_result = ::WaitForSingleObject(handle_, INFINITE);
            if (wait_result == WAIT_FAILED) {
                return std::unexpected(
                    std::error_code(
                        static_cast<int>(::GetLastError()),
                        std::system_category()));
            }
            if (wait_result != WAIT_OBJECT_0) {
                return std::unexpected(
                    std::make_error_code(std::errc::io_error));
            }

            DWORD exit_code = 0;
            const bool exit_ok = ::GetExitCodeProcess(handle_, &exit_code);
            const DWORD err = exit_ok ? 0 : GetLastError();

            close_handle();
            if (!exit_ok) {
                return std::unexpected(
                    std::error_code(static_cast<int>(err),
                        std::system_category())
                );
            }

            return process_exit_status{
                process_exit_kind::exited,
                static_cast<std::uint32_t>(exit_code)
            };
#else
            int status = 0;
            process_id_t res = 0;

            do {
                res = ::waitpid(id_, &status, 0);
            } while (res < 0 && errno == EINTR);

            if (res < 0) {
                return std::unexpected(
                    std::error_code(
                        errno, std::generic_category()));
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
        [[nodiscard]] static std::expected<fork_result, std::error_code> fork_id() noexcept{
            const process_id_t pid = ::fork();

            if (pid < 0) {
                return std::unexpected(std::error_code(errno, std::generic_category()));
            }

            if (pid == 0) {
                return fork_result{ 0 };
            }

            return fork_result{ pid };
        }

        [[nodiscard]] static std::expected<process, std::error_code> fork_process() noexcept {

            auto result = fork_id();

            if (!result) {
                return std::unexpected(result.error());
            }

            else if (result->in_child()) {
                return process{};
            }

            return process(adopt_process_handle,result->returned_id);

        }
#endif

        [[nodiscard]] static std::expected<process, std::error_code> spawn_process(
            const std::string& program,
            const std::vector<std::string>& args
        ) {
#ifdef _WIN32
            std::string command_line = detail::quote_windows_argument(program);
            for (const auto& arg : args) {
                command_line.push_back(' ');
                command_line += detail::quote_windows_argument(arg);
            }

            auto wide_command = utf8_to_wide(command_line);
            if (!wide_command) {
                return std::unexpected(wide_command.error());
            }
            return create_process(std::move(*wide_command));
#else
            std::vector<char*> argv;
            argv.reserve(args.size() + 2);
            argv.push_back(const_cast<char*>(program.c_str()));
            for (const auto& arg : args) {
                argv.push_back(const_cast<char*>(arg.c_str()));
            }
            argv.push_back(nullptr);

            const auto res = fork_id();

            if (!res) {
                return std::unexpected(res.error());
            }

            if (res->in_child()) {
                const auto exec_result =
                    exec_program(program.c_str(), argv.data());
                const auto exec_error = exec_result.error().value();

                ::_exit(exec_error == ENOENT ? 127 : 126);
            }
            return process(adopt_process_handle, res->returned_id);
#endif
        }

};


} // namespace unp::sys
