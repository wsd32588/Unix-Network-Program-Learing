#pragma once

#include "unp/detail/socket_error.h"
#include "unp/net/address.h"
#include "unp/net/socket.h"

#include <cstddef>
#include <limits>
#include <type_traits>

namespace unp::net {

namespace detail {

[[nodiscard]] inline bool valid_address_length(std::size_t length) noexcept {
    return length <= static_cast<std::size_t>(
        (std::numeric_limits<socket_length_t>::max)()
    );
}

} // namespace detail

[[nodiscard]] inline int set_ipv6_dual_stack(native_socket_t socket) noexcept {
#ifdef _WIN32
    const DWORD ipv6_only = 0;
    return ::setsockopt(
        socket,
        IPPROTO_IPV6,
        IPV6_V6ONLY,
        reinterpret_cast<const char*>(&ipv6_only),
        static_cast<int>(sizeof(ipv6_only))
    );
#else
    const int ipv6_only = 0;
    return ::setsockopt(
        socket,
        IPPROTO_IPV6,
        IPV6_V6ONLY,
        &ipv6_only,
        static_cast<socklen_t>(sizeof(ipv6_only))
    );
#endif
}

[[nodiscard]] inline int bind_socket(
    native_socket_t socket,
    const sockaddr* address,
    std::size_t address_length
) noexcept {
    if ((address == nullptr && address_length != 0) ||
        !detail::valid_address_length(address_length)) {
        detail::set_invalid_argument_error();
        return -1;
    }

    return ::bind(
        socket,
        address,
        static_cast<socket_length_t>(address_length)
    );
}

[[nodiscard]] inline int bind_socket(
    native_socket_t socket,
    socket_address_view address
) noexcept {
    if constexpr (std::is_signed_v<socket_length_t>) {
        if (address.length < 0) {
            detail::set_invalid_argument_error();
            return -1;
        }
    }

    return bind_socket(
        socket,
        address.address,
        static_cast<std::size_t>(address.length)
    );
}

[[nodiscard]] inline int connect_socket(
    native_socket_t socket,
    const sockaddr* address,
    std::size_t address_length
) noexcept {
    if ((address == nullptr && address_length != 0) ||
        !detail::valid_address_length(address_length)) {
        detail::set_invalid_argument_error();
        return -1;
    }

    return ::connect(
        socket,
        address,
        static_cast<socket_length_t>(address_length)
    );
}

[[nodiscard]] inline int connect_socket(
    native_socket_t socket,
    socket_address_view address
) noexcept {
    if constexpr (std::is_signed_v<socket_length_t>) {
        if (address.length < 0) {
            detail::set_invalid_argument_error();
            return -1;
        }
    }

    return connect_socket(
        socket,
        address.address,
        static_cast<std::size_t>(address.length)
    );
}

[[nodiscard]] inline int listen_socket(
    native_socket_t socket,
    int backlog
) noexcept {
    return ::listen(socket, backlog);
}

[[nodiscard]] inline sockfd accept_socket(
    native_socket_t listen_socket,
    sockaddr_storage& client_address,
    socket_length_t& address_length
) noexcept {
    return sockfd{
        ::accept(
            listen_socket,
            reinterpret_cast<sockaddr*>(&client_address),
            &address_length
        )
    };
}

[[nodiscard]] inline socket_io_result_t send_socket(
    native_socket_t socket,
    const char* buffer,
    std::size_t length,
    int flags = 0
) noexcept {
    if (buffer == nullptr && length != 0) {
        detail::set_invalid_argument_error();
        return static_cast<socket_io_result_t>(-1);
    }

#ifdef _WIN32
    if (length > static_cast<std::size_t>((std::numeric_limits<int>::max)())) {
        detail::set_message_too_large_error();
        return SOCKET_ERROR;
    }
    return ::send(socket, buffer, static_cast<int>(length), flags);
#else
    #ifdef MSG_NOSIGNAL
        flags |= MSG_NOSIGNAL;
    #endif
    return ::send(socket, buffer, length, flags);
#endif
}

[[nodiscard]] inline socket_io_result_t recv_socket(
    native_socket_t socket,
    char* buffer,
    std::size_t length,
    int flags = 0
) noexcept {
    if (buffer == nullptr && length != 0) {
        detail::set_invalid_argument_error();
        return static_cast<socket_io_result_t>(-1);
    }

#ifdef _WIN32
    if (length > static_cast<std::size_t>((std::numeric_limits<int>::max)())) {
        detail::set_message_too_large_error();
        return SOCKET_ERROR;
    }
    return ::recv(socket, buffer, static_cast<int>(length), flags);
#else
    return ::recv(socket, buffer, length, flags);
#endif
}

} // namespace unp::net
