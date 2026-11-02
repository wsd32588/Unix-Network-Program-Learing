#pragma once

#include <cerrno>
#include <system_error>
#include <type_traits>

#ifdef _WIN32
    #include <winsock2.h>
#endif

namespace unp::detail {

[[nodiscard]] inline int last_socket_error_value() noexcept {
#ifdef _WIN32
    return ::WSAGetLastError();
#else
    return errno;
#endif
}

[[nodiscard]] inline std::error_code last_socket_error_code() noexcept {
    const int error = last_socket_error_value();
#ifdef _WIN32
    return std::error_code(
        error,
        std::system_category()
    );
#else
    return std::error_code(
        error,
        std::generic_category()
    );
#endif
}

inline void set_invalid_argument_error() noexcept {
#ifdef _WIN32
    ::WSASetLastError(WSAEINVAL);
#else
    errno = EINVAL;
#endif
}

inline void set_message_too_large_error() noexcept {
#ifdef _WIN32
    ::WSASetLastError(WSAEMSGSIZE);
#else
    errno = EMSGSIZE;
#endif
}

inline void set_connection_closed_error() noexcept {
#ifdef _WIN32
    ::WSASetLastError(WSAECONNRESET);
#else
    errno = EPIPE;
#endif
}

[[nodiscard]] inline bool is_interrupted_error(int error) noexcept {
#ifdef _WIN32
    return error == WSAEINTR;
#else
    return error == EINTR;
#endif
}

template<typename T>
[[nodiscard]] inline bool is_negative_value(T value) noexcept {
    if constexpr (std::is_signed_v<T>) {
        return value < 0;
    }
    else {
        return false;
    }
}

} // namespace unp::detail
