#pragma once

#include <algorithm>
#include <concepts>
#include <cstddef>
#include <cstdio>
#include <string>
#include <ranges>
#include <utility>
#include <iostream>
#include <limits>
#include <cstring>
#include <stdexcept>

#ifdef _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <io.h>
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #ifdef _MSC_VER
        #pragma comment(lib, "ws2_32.lib")
    #endif
#else
    #include <arpa/inet.h>
    #include <cerrno>
    #include <netinet/in.h>
    #include <sys/socket.h>
    #include <unistd.h>
#endif

namespace unp {

// 2. 跨平台类型与常量
#ifdef _WIN32
    using native_socket_t = SOCKET;
    using socket_length_t = int;
    using socket_io_result_t = int;
    inline constexpr native_socket_t invalid_socket = INVALID_SOCKET;
#else
    using native_socket_t = int;
    using socket_length_t = socklen_t;
    using socket_io_result_t = ssize_t;
    inline constexpr native_socket_t invalid_socket = -1;
#endif

[[nodiscard]] inline int last_socket_error() noexcept {
#ifdef _WIN32
    return ::WSAGetLastError();
#else
    return errno;
#endif
}

[[nodiscard]] inline const char* address_info_error_message(int error) noexcept {
#ifdef _WIN32
    return ::gai_strerrorA(error);
#else
    return ::gai_strerror(error);
#endif
}

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
    if (address_length > static_cast<std::size_t>(
            (std::numeric_limits<socket_length_t>::max)())) {
#ifdef _WIN32
        ::WSASetLastError(WSAEINVAL);
        return SOCKET_ERROR;
#else
        errno = EINVAL;
        return -1;
#endif
    }

    return ::bind(
        socket,
        address,
        static_cast<socket_length_t>(address_length)
    );
}

// 3. 严格遵循 RAII 的 socket 封装
class sockfd {
private:
    native_socket_t fd_{invalid_socket};

    void close() noexcept {
        if (fd_ != invalid_socket) {
#ifdef _WIN32
            ::closesocket(fd_);
#else
            ::close(fd_);
#endif
            fd_ = invalid_socket;
        }
    }

public:
    sockfd() noexcept = default;

    explicit sockfd(native_socket_t fd) noexcept : fd_(fd) {}

    ~sockfd() noexcept {
        close();
    }

    // 禁止拷贝
    sockfd(const sockfd&) = delete;
    sockfd& operator=(const sockfd&) = delete;

    // 允许移动
    sockfd(sockfd&& other) noexcept : fd_(std::exchange(other.fd_, invalid_socket)) {}

    sockfd& operator=(sockfd&& other) noexcept {
        if (this != &other) {
            close();
            fd_ = std::exchange(other.fd_, invalid_socket);
        }
        return *this;
    }

    [[nodiscard]] bool valid() const noexcept {
        return fd_ != invalid_socket;
    }

    [[nodiscard]] native_socket_t get() const noexcept {
        return fd_;
    }

    [[nodiscard]] native_socket_t errSocket(
        int family,
        int type,
        int protocol
    ) {
        native_socket_t n = invalid_socket;

        if ((n = ::socket(family, type, protocol)) == invalid_socket) {
#ifdef _WIN32
            std::cerr << "Socket creation failed: "
                      << ::WSAGetLastError() << '\n';
#else
            std::cerr << "Socket creation failed: "
                      << std::strerror(errno) << '\n';
#endif
        }

        return n;
    }

    // 释放管理权，但不关闭底层 socket
    native_socket_t release() noexcept {
        return std::exchange(fd_, invalid_socket);
    }

    // 手动显式重置/关闭
    void reset(native_socket_t new_fd = invalid_socket) noexcept {
        close();
        fd_ = new_fd;
    }
};

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
#ifdef _WIN32
    if (length > static_cast<std::size_t>(
            (std::numeric_limits<int>::max)())) {
        ::WSASetLastError(WSAEMSGSIZE);
        return SOCKET_ERROR;
    }

    return ::send(socket, buffer, static_cast<int>(length), flags);
#else
    return ::send(socket, buffer, length, flags);
#endif
}

// 4. Windows 下的自动网络环境初始化工具（Linux 下为空操作）
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

// 5. 文件安全打开辅助
inline FILE* safe_fopen(const char* filename, const char* mode) {
#ifdef _WIN32
    FILE* file = nullptr;
    const errno_t err = ::fopen_s(&file, filename, mode);
    if (err != 0) {
        std::fprintf(stderr, "Failed to open '%s', error: %d\n", filename, static_cast<int>(err));
        return nullptr;
    }
    return file;
#else
    FILE* file = ::fopen(filename, mode);
    if (file == nullptr) {
        std::fprintf(stderr, "Failed to open '%s': %s\n", filename, std::strerror(errno));
        return nullptr;
    }
    return file;
#endif
}

} // namespace unp
