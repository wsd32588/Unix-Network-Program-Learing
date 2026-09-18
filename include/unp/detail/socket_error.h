#pragma once

#include "unp/socket.h"

namespace unp::detail {

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

} // namespace unp::detail
