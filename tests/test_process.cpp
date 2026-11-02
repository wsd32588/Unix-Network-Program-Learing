#include "unp/sys/process.h"

#include <cstdlib>
#include <iostream>
#include <string>
#include <type_traits>
#include <vector>

#ifndef _WIN32
    #include <cerrno>
    #include <sys/wait.h>
    #include <unistd.h>

namespace {

    [[nodiscard]] bool test_fork_and_wait() {
        auto child = unp::sys::process::fork_process();
        if (!child) {
            std::cerr << "fork_process failed: "
                << child.error().message() << '\n';
            return false;
        }

        if (!child->valid()) {
            ::_exit(23);
        }

        auto status = child->wait();
        if (!status) {
            std::cerr << "wait failed: "
                << status.error().message() << '\n';
            return false;
        }
        if (!status->exited_normally() || status->value != 23) {
            std::cerr << "unexpected child exit status\n";
            return false;
        }
        if (child->valid()) {
            std::cerr << "process remained valid after wait\n";
            return false;
        }

        const auto second_wait = child->wait();
        if (second_wait ||
            second_wait.error() !=
                std::make_error_code(std::errc::bad_file_descriptor)) {
            std::cerr << "second wait did not report bad_file_descriptor\n";
            return false;
        }

        return true;
    }

    [[nodiscard]] bool test_external_reap_invalidates_process() {
        auto child = unp::sys::process::fork_process();
        if (!child) {
            std::cerr << "fork_process failed: "
                << child.error().message() << '\n';
            return false;
        }

        if (!child->valid()) {
            ::_exit(0);
        }

        int status = 0;
        pid_t wait_result = 0;
        do {
            wait_result = ::waitpid(child->id(), &status, 0);
        } while (wait_result < 0 && errno == EINTR);

        if (wait_result < 0) {
            std::cerr << "external waitpid failed\n";
            return false;
        }

        const auto second_wait = child->wait();
        if (second_wait ||
            second_wait.error() != std::errc::no_child_process) {
            std::cerr << "wait did not report no_child_process\n";
            return false;
        }
        if (child->valid()) {
            std::cerr << "ECHILD left the process valid\n";
            return false;
        }

        return true;
    }

} // namespace
#endif

int main() {
    static_assert(!std::is_copy_constructible_v<unp::sys::process>);
    static_assert(!std::is_copy_assignable_v<unp::sys::process>);
    static_assert(std::is_nothrow_move_constructible_v<unp::sys::process>);
    static_assert(std::is_nothrow_move_assignable_v<unp::sys::process>);

    const unp::sys::process_exit_status normal_exit{
        unp::sys::process_exit_kind::exited,
        0
    };
    if (!normal_exit.exited_normally()) {
        return EXIT_FAILURE;
    }

    const unp::sys::process_exit_status signal_exit{
        unp::sys::process_exit_kind::signaled,
        9
    };
    if (signal_exit.exited_normally()) {
        return EXIT_FAILURE;
    }

    const unp::sys::process process;
    if (process.valid()) {
        return EXIT_FAILURE;
    }

#ifdef _WIN32
    if (unp::detail::quote_windows_argument("plain") != "plain" ||
        unp::detail::quote_windows_argument("") != "\"\"" ||
        unp::detail::quote_windows_argument("two words") !=
            "\"two words\"" ||
        unp::detail::quote_windows_argument("say \"hi\"") !=
            "\"say \\\"hi\\\"\"" ||
        unp::detail::quote_windows_argument("C:\\path with space\\") !=
            "\"C:\\path with space\\\\\"") {
        return EXIT_FAILURE;
    }
#endif

#ifndef _WIN32
    if (!test_fork_and_wait()) {
        return EXIT_FAILURE;
    }
    if (!test_external_reap_invalidates_process()) {
        return EXIT_FAILURE;
    }
#endif

#ifdef _WIN32
    std::string program = "cmd.exe";
    std::vector<std::string> args{ "/D", "/C", "exit", "23" };
#else
    std::string program = "sh";
    std::vector<std::string> args{ "-c", "exit 23" };
#endif

    auto other_child = unp::sys::process::spawn_process(program, args);

    if (!other_child) {
        return EXIT_FAILURE;
    }
    auto the_status = other_child->wait();

    if (!the_status ||
        !the_status->exited_normally() ||
        the_status->value != 23) {
        return EXIT_FAILURE;
    }

    const auto missing_child = unp::sys::process::spawn_process(
        "unp_process_test_program_that_does_not_exist_7f32b6e1",
        {}
    );
    if (missing_child) {
        std::cerr << "spawn_process accepted a missing program\n";
        return EXIT_FAILURE;
    }
    if (missing_child.error() !=
        std::errc::no_such_file_or_directory) {
        std::cerr << "missing program returned unexpected error: "
            << missing_child.error().message() << '\n';
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
