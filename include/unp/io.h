#pragma once

#include "unp/detail/socket_error.h"
#include "unp/operations.h"

#include <algorithm>
#include <concepts>
#include <cstddef>
#include <expected>
#include <iterator>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <system_error>

namespace unp {

[[nodiscard]] inline socket_io_result_t readn(
    native_socket_t socket,
    void* buffer,
    std::size_t length
) noexcept {
    if (buffer == nullptr && length != 0) {
        detail::set_invalid_argument_error();
        return static_cast<socket_io_result_t>(-1);
    }

    constexpr const std::size_t max_result = static_cast<std::size_t>(
        (std::numeric_limits<socket_io_result_t>::max)()
    );
    if (length > max_result) {
        detail::set_message_too_large_error();
        return static_cast<socket_io_result_t>(-1);
    }

    std::size_t bytes_left = length;
    auto* destination = static_cast<char*>(buffer);

    while (bytes_left > 0) {
        const socket_io_result_t bytes_read = recv_socket(
            socket,
            destination,
            bytes_left
        );
        if (bytes_read < 0) {
            if (detail::is_interrupted_error(last_socket_error())) {
                continue;
            }
            return static_cast<socket_io_result_t>(-1);
        }

        if (bytes_read == 0) {
            break;
        }

        const std::size_t consumed = static_cast<std::size_t>(bytes_read);
        bytes_left -= consumed;
        destination += consumed;
    }

    return static_cast<socket_io_result_t>(length - bytes_left);
}

[[nodiscard]] inline socket_io_result_t readn(
    native_socket_t socket,
    std::span<std::byte> buffer
) noexcept {
    return readn(socket, buffer.data(), buffer.size_bytes());
}

class buffered_reader {
private:
    static constexpr std::size_t buffer_capacity = 8192;

    native_socket_t socket_{invalid_socket};
    char buffer_[buffer_capacity]{};
    std::size_t read_index_{0};
    std::size_t valid_bytes_{0};

    void clear_buffer() noexcept {
        read_index_ = 0;
        valid_bytes_ = 0;
    }

    void move_from(buffered_reader& other) noexcept {
        socket_ = other.socket_;

        const std::size_t unread_bytes =
            other.valid_bytes_ - other.read_index_;
        std::copy_n(
            other.buffer_ + other.read_index_,
            unread_bytes,
            buffer_
        );
        read_index_ = 0;
        valid_bytes_ = unread_bytes;

        other.socket_ = invalid_socket;
        other.clear_buffer();
    }

    [[nodiscard]] socket_io_result_t refill() noexcept {
        clear_buffer();

        const socket_io_result_t count = recv_socket(
            socket_,
            buffer_,
            sizeof(buffer_)
        );
        if (count > 0) {
            valid_bytes_ = static_cast<std::size_t>(count);
        }
        return count;
    }

public:
    buffered_reader() noexcept = default;

    explicit buffered_reader(native_socket_t socket) noexcept : socket_(socket) {}

    buffered_reader(const buffered_reader&) = delete;
    buffered_reader& operator=(const buffered_reader&) = delete;

    buffered_reader(buffered_reader&& other) noexcept {
        move_from(other);
    }

    buffered_reader& operator=(buffered_reader&& other) noexcept {
        if (this != &other) {
            move_from(other);
        }
        return *this;
    }

    [[nodiscard]] bool valid() const noexcept {
        return socket_ != invalid_socket;
    }

    [[nodiscard]] socket_io_result_t get_char(char& out_char) noexcept {
        if (!valid()) {
            detail::set_invalid_argument_error();
            return static_cast<socket_io_result_t>(-1);
        }

        if (read_index_ >= valid_bytes_) {
            const socket_io_result_t n = refill();
            if (n <= 0) {
                return n;
            }
        }

        out_char = buffer_[read_index_++];
        return 1;
    }

    void reset() noexcept {
        clear_buffer();
    }

    void reset(native_socket_t socket) noexcept {
        socket_ = socket;
        clear_buffer();
    }
};

// Reads at most out_buf.size() - 1 bytes and always reserves room for '\0'.
// If no newline fits, the remaining bytes stay buffered for the next call.
[[nodiscard]] inline socket_io_result_t readline(
    buffered_reader& reader,
    std::span<char> out_buf
) noexcept {
    if (out_buf.empty()) {
        detail::set_invalid_argument_error();
        return -1;
    }

    const std::size_t max_length = out_buf.size() - 1;
    constexpr std::size_t max_result = static_cast<std::size_t>(
        (std::numeric_limits<socket_io_result_t>::max)()
    );
    if (max_length > max_result) {
        detail::set_message_too_large_error();
        return static_cast<socket_io_result_t>(-1);
    }

    std::size_t total_read = 0;
    char c = '\0';

    while (total_read < max_length) {
        const socket_io_result_t rc = reader.get_char(c);

        if (rc == 1) {
            out_buf[total_read++] = c;
            if (c == '\n') {
                break;
            }
        } else if (rc == 0) {
            break;
        } else {
            if (detail::is_interrupted_error(last_socket_error())) {
                continue;
            }
            out_buf[total_read] = '\0';
            return static_cast<socket_io_result_t>(-1);
        }
    }

    out_buf[total_read] = '\0';
    return static_cast<socket_io_result_t>(total_read);
}

// Reads at most max_length bytes. A line that exceeds the limit continues on
// the next call, matching the fixed-buffer overload.
[[nodiscard]] inline socket_io_result_t readline(
    buffered_reader& reader,
    std::string& line,
    std::size_t max_length = 65535
) {
    line.clear();

    constexpr std::size_t max_result = static_cast<std::size_t>(
        (std::numeric_limits<socket_io_result_t>::max)()
    );
    if (max_length > max_result) {
        detail::set_message_too_large_error();
        return static_cast<socket_io_result_t>(-1);
    }

    char c = '\0';

    while (line.size() < max_length) {
        const socket_io_result_t rc = reader.get_char(c);

        if (rc == 1) {
            line.push_back(c);
            if (c == '\n') {
                return static_cast<socket_io_result_t>(line.size());
            }
        } else if (rc == 0) {
            return static_cast<socket_io_result_t>(line.size());
        } else {
            if (detail::is_interrupted_error(last_socket_error())) {
                continue;
            }
            return static_cast<socket_io_result_t>(-1);
        }
    }

    return static_cast<socket_io_result_t>(line.size());
}

[[nodiscard]] inline std::expected<std::size_t, std::error_code> writen(
    native_socket_t socket,
    std::span<const std::byte> bytes,
    int flags = 0
) noexcept {
    if (socket == invalid_socket) {
        detail::set_invalid_argument_error();
        return std::unexpected(std::make_error_code(std::errc::bad_file_descriptor));
    }

    std::size_t total_written = 0;
    const std::size_t total_size = bytes.size();

    constexpr std::size_t max_chunk = static_cast<std::size_t>(
        (std::numeric_limits<socket_io_result_t>::max)()
    );

    while (total_written < total_size) {
        const std::size_t remaining_size = total_size - total_written;
        const std::size_t chunk_size = remaining_size > max_chunk
            ? max_chunk
            : remaining_size;

        const socket_io_result_t sent = send_socket(
            socket,
            bytes.data() + total_written,
            chunk_size,
            flags
        );

        if (sent > 0) {
            total_written += static_cast<std::size_t>(sent);
            continue;
        }

        if (sent == 0) {
            detail::set_connection_closed_error();
            return std::unexpected(std::make_error_code(std::errc::broken_pipe));
        }

        const int err = last_socket_error();
        if (detail::is_interrupted_error(err)) {
            continue;
        }

#ifdef _WIN32
        return std::unexpected(std::error_code(err, std::system_category()));
#else
        return std::unexpected(std::error_code(err, std::generic_category()));
#endif
    }

    return total_written;
}

[[nodiscard]] inline std::expected<std::size_t, std::error_code> writen(
    const sockfd& fd,
    std::span<const std::byte> bytes,
    int flags = 0
) noexcept {
    return writen(fd.get(), bytes, flags);
}

[[nodiscard]] inline std::expected<std::size_t, std::error_code> writen(
    const sockfd& fd,
    std::string_view sv,
    int flags = 0
) noexcept {
    return writen(
        fd.get(),
        std::as_bytes(std::span(sv.data(), sv.size())),
        flags
    );
}

template <typename Container>
    requires requires(const Container& c) {
        { std::data(c) } -> std::contiguous_iterator;
        { std::size(c) } -> std::convertible_to<std::size_t>;
            requires sizeof(typename Container::value_type) == 1;
}
[[nodiscard]] inline std::expected<std::size_t, std::error_code> writen(
    const sockfd& fd,
    const Container& container,
    int flags = 0
) noexcept {
    return writen(
        fd.get(),
        std::as_bytes(std::span(std::data(container), std::size(container))),
        flags
    );
}
} // namespace unp
