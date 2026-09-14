#pragma once

#include "unp/detail/socket_error.h"
#include "unp/net/socket.h"

#include <concepts>
#include <cstddef>
#include <cstring>
#include <stdexcept>
#include <string>
#include <type_traits>

namespace unp::net {

template<typename T>
concept SocketAddressType =
    std::same_as<std::remove_cvref_t<T>, sockaddr_in> ||
    std::same_as<std::remove_cvref_t<T>, sockaddr_in6>;

struct socket_address_view {
    const sockaddr* address;
    socket_length_t length;

    operator const sockaddr*() const noexcept {
        return address;
    }
};

template<SocketAddressType Address>
[[nodiscard]] inline socket_address_view auto_sockaddr(Address& address) noexcept {
    return {
        reinterpret_cast<const sockaddr*>(&address),
        static_cast<socket_length_t>(sizeof(Address))
    };
}

template<SocketAddressType Address>
[[nodiscard]] inline const void* get_in_addr(const Address& address) noexcept {
    using AddressType = std::remove_cvref_t<Address>;
    if constexpr (std::same_as<AddressType, sockaddr_in>) {
        return &address.sin_addr;
    } else {
        return &address.sin6_addr;
    }
}

template<SocketAddressType Address>
[[nodiscard]] inline void* get_in_addr(Address& address) noexcept {
    using AddressType = std::remove_cvref_t<Address>;
    if constexpr (std::same_as<AddressType, sockaddr_in>) {
        return &address.sin_addr;
    } else {
        return &address.sin6_addr;
    }
}

template<SocketAddressType Address>
[[nodiscard]] inline constexpr int get_family(const Address&) noexcept {
    using AddressType = std::remove_cvref_t<Address>;
    if constexpr (std::same_as<AddressType, sockaddr_in>) {
        return AF_INET;
    } else {
        return AF_INET6;
    }
}

[[nodiscard]] inline const char* address_info_error_message(int error) noexcept {
#ifdef _WIN32
    return ::gai_strerrorA(error);
#else
    return ::gai_strerror(error);
#endif
}

template<SocketAddressType Address>
[[nodiscard]] inline bool pton(const char* source, Address& destination) noexcept {
    if (source == nullptr) {
        detail::set_invalid_argument_error();
        return false;
    }

    const int result = ::inet_pton(
        get_family(destination),
        source,
        get_in_addr(destination)
    );
    if (result != 1) {
        return false;
    }

    using AddressType = std::remove_cvref_t<Address>;
    if constexpr (std::same_as<AddressType, sockaddr_in>) {
        destination.sin_family = static_cast<family_t>(AF_INET);
    } else {
        destination.sin6_family = static_cast<family_t>(AF_INET6);
    }
    return true;
}

namespace detail {

template<typename T>
inline constexpr std::size_t ip_string_capacity = 0;

template<>
inline constexpr std::size_t ip_string_capacity<sockaddr_in> = INET_ADDRSTRLEN;

template<>
inline constexpr std::size_t ip_string_capacity<sockaddr_in6> = INET6_ADDRSTRLEN;

} // namespace detail

template<SocketAddressType Address>
[[nodiscard]] inline std::string ntop(const Address& source) {
    char buffer[
        detail::ip_string_capacity<std::remove_cvref_t<Address>>
    ]{};
    const char* result = ::inet_ntop(
        get_family(source),
        get_in_addr(source),
        buffer,
        sizeof(buffer)
    );
    if (result == nullptr) {
        throw std::runtime_error(
            std::string("inet_ntop failed: ") +
#ifdef _WIN32
            std::to_string(last_socket_error())
#else
            std::strerror(last_socket_error())
#endif
        );
    }

    return std::string(buffer);
}

} // namespace unp::net
