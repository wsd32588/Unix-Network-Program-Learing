#include "unp/sys/process.h"

#include <cstdlib>
#include <iostream>
#include <type_traits>

#ifndef _WIN32
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

#ifndef _WIN32
    if (!test_fork_and_wait()) {
        return EXIT_FAILURE;
    }
#endif

    return EXIT_SUCCESS;
}
