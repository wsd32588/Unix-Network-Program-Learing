#pragma once

#ifndef _WIN32

#include <cerrno>
#include <expected>
#include <string>
#include <system_error>
#include <vector>

#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

namespace unp::detail {

struct fork_result {
    pid_t returned_id;

    [[nodiscard]] bool in_child() const noexcept {
        return returned_id == 0;
    }

    [[nodiscard]] bool in_parent() const noexcept {
        return returned_id > 0;
    }
};

[[nodiscard]] inline std::expected<fork_result, std::error_code>
fork_id() noexcept {
    const pid_t pid = ::fork();
    if (pid < 0) {
        return std::unexpected(
            std::error_code(errno, std::generic_category()));
    }

    return fork_result{pid};
}

[[nodiscard]] inline std::vector<char*> make_argv(
    const std::string& program,
    const std::vector<std::string>& args
) {
    std::vector<char*> argv;
    argv.reserve(args.size() + 2);
    argv.push_back(const_cast<char*>(program.c_str()));
    for (const auto& argument : args) {
        argv.push_back(const_cast<char*>(argument.c_str()));
    }
    argv.push_back(nullptr);
    return argv;
}

inline void kill_and_reap_child(pid_t pid) noexcept {
    ::kill(pid, SIGKILL);

    int status = 0;
    pid_t wait_result = 0;
    do {
        wait_result = ::waitpid(pid, &status, 0);
    } while (wait_result < 0 && errno == EINTR);
}

[[nodiscard]] inline std::expected<fork_result, std::error_code>
spawn_native_process(
    const std::string& program,
    const std::vector<std::string>& args
) {
    std::vector<char*> argv = make_argv(program, args);

    int pipefd[2];
    if (::pipe2(pipefd, O_CLOEXEC) < 0) {
        return std::unexpected(
            std::error_code(errno, std::generic_category()));
    }

    const auto result = fork_id();
    if (!result) {
        ::close(pipefd[0]);
        ::close(pipefd[1]);
        return std::unexpected(result.error());
    }

    if (result->in_child()) {
        ::close(pipefd[0]);
        ::execvp(program.c_str(), argv.data());

        const int exec_error = errno;
        const char* source = reinterpret_cast<const char*>(&exec_error);
        std::size_t bytes_left = sizeof(exec_error);
        while (bytes_left > 0) {
            const ssize_t bytes_written =
                ::write(pipefd[1], source, bytes_left);
            if (bytes_written < 0) {
                if (errno == EINTR) {
                    continue;
                }
                break;
            }
            if (bytes_written == 0) {
                break;
            }

            source += bytes_written;
            bytes_left -= static_cast<std::size_t>(bytes_written);
        }
        ::_exit(127);
    }

    ::close(pipefd[1]);
    int child_errno = 0;
    char* const dest = reinterpret_cast<char*>(&child_errno);
    std::size_t total_read = 0;
    std::error_code pipe_error;
    while (total_read < sizeof(child_errno)) {
        const ssize_t bytes_read = ::read(
            pipefd[0],
            dest + total_read,
            sizeof(child_errno) - total_read
        );
        if (bytes_read > 0) {
            total_read += static_cast<std::size_t>(bytes_read);
            continue;
        }
        if (bytes_read == 0) {
            break;
        }
        if (errno == EINTR) {
            continue;
        }
        pipe_error = std::error_code(errno, std::generic_category());
        break;
    }

    ::close(pipefd[0]);

    if (!pipe_error && total_read == 0) {
        return *result;
    }

    if (!pipe_error && total_read == sizeof(child_errno)) {
        int status = 0;
        pid_t wait_result = 0;
        do {
            wait_result = ::waitpid(result->returned_id, &status, 0);
        } while (wait_result < 0 && errno == EINTR);

        return std::unexpected(
            std::error_code(child_errno, std::generic_category()));
    }

    kill_and_reap_child(result->returned_id);
    if (pipe_error) {
        return std::unexpected(pipe_error);
    }

    return std::unexpected(
        std::make_error_code(std::errc::io_error));
}

} // namespace unp::detail

#endif
