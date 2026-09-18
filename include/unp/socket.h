#pragma once

#include <bit>
#include <cerrno>
#include <stdexcept>
#include <string>
#include <utility>

#ifdef _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #ifdef _MSC_VER
        #pragma comment(lib, "ws2_32.lib")
    #endif
#else
    #include <arpa/inet.h>
    #include <netinet/in.h>
    #include <sys/socket.h>
    #include <unistd.h>
#endif

namespace unp {

#ifdef _WIN32
using native_socket_t = SOCKET;
using socket_length_t = int;
using socket_io_result_t = int;
using family_t = ADDRESS_FAMILY;
inline constexpr native_socket_t invalid_socket = INVALID_SOCKET;
#else
using native_socket_t = int;
using socket_length_t = socklen_t;
using socket_io_result_t = ssize_t;
using family_t = sa_family_t;
inline constexpr native_socket_t invalid_socket = -1;
#endif

inline constexpr std::endian native_endian = std::endian::native;

[[nodiscard]] inline int last_socket_error() noexcept {
#ifdef _WIN32
    return ::WSAGetLastError();
#else
    return errno;
#endif
}

class sockfd {
private:
    native_socket_t socket_{invalid_socket};

    void close_current() noexcept {
        if (socket_ == invalid_socket) {
            return;
        }

#ifdef _WIN32
        ::closesocket(socket_);
#else
        ::close(socket_);
#endif
        socket_ = invalid_socket;
    }

public:
    sockfd() noexcept = default;

    // Construction adopts ownership of the supplied native socket.
    explicit sockfd(native_socket_t socket) noexcept : socket_(socket) {}

    ~sockfd() noexcept {
        close_current();
    }

    sockfd(const sockfd&) = delete;
    sockfd& operator=(const sockfd&) = delete;

    sockfd(sockfd&& other) noexcept
        : socket_(std::exchange(other.socket_, invalid_socket)) {}

    sockfd& operator=(sockfd&& other) noexcept {
        if (this != &other) {
            close_current();
            socket_ = std::exchange(other.socket_, invalid_socket);
        }
        return *this;
    }

    [[nodiscard]] bool valid() const noexcept {
        return socket_ != invalid_socket;
    }

    [[nodiscard]] explicit operator bool() const noexcept {
        return valid();
    }

    [[nodiscard]] native_socket_t get() const noexcept {
        return socket_;
    }

    [[nodiscard]] static sockfd create(
        int family,
        int type,
        int protocol
    ) noexcept {
        return sockfd{::socket(family, type, protocol)};
    }

    [[nodiscard]] native_socket_t release() noexcept {
        return std::exchange(socket_, invalid_socket);
    }

    void reset(native_socket_t new_socket = invalid_socket) noexcept {
        if (new_socket == socket_) {
            return;
        }

        close_current();
        socket_ = new_socket;
    }
};

struct socket_env {
    socket_env() {
#ifdef _WIN32
        WSADATA wsa_data{};
        const int error = ::WSAStartup(MAKEWORD(2, 2), &wsa_data);
        if (error != 0) {
            throw std::runtime_error(
                "WSAStartup failed: " + std::to_string(error)
            );
        }
#endif
    }

    ~socket_env() noexcept {
#ifdef _WIN32
        ::WSACleanup();
#endif
    }

    socket_env(const socket_env&) = delete;
    socket_env& operator=(const socket_env&) = delete;
};

} // namespace unp
