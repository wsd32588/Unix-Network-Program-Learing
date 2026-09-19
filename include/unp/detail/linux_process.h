#pragma once

#ifndef _WIN32

#include <cerrno>
#include <expected>
#include <string>
#include <system_error>
#include <vector>

#include <fcntl.h>
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
        ssize_t bytes_written = 0;
        do {
            bytes_written =
                ::write(pipefd[1], &exec_error, sizeof(exec_error));
        } while (bytes_written < 0 && errno == EINTR);

        ::_exit(127);
    }

    ::close(pipefd[1]);
    int child_errno = 0;
    ssize_t bytes_read = 0;
    do {
        bytes_read =
            ::read(pipefd[0], &child_errno, sizeof(child_errno));
    } while (bytes_read < 0 && errno == EINTR);

    const int read_error = bytes_read < 0 ? errno : 0;
    ::close(pipefd[0]);

    if (bytes_read == 0) {
        return *result;
    }

    if (bytes_read == static_cast<ssize_t>(sizeof(child_errno))) {
        int status = 0;
        pid_t wait_result = 0;
        do {
            wait_result = ::waitpid(result->returned_id, &status, 0);
        } while (wait_result < 0 && errno == EINTR);

        return std::unexpected(
            std::error_code(child_errno, std::generic_category()));
    }

    if (bytes_read < 0) {
        return std::unexpected(
            std::error_code(read_error, std::generic_category()));
    }

    return std::unexpected(
        std::make_error_code(std::errc::io_error));
}

} // namespace unp::detail

#endif
