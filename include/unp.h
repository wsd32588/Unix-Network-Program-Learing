#pragma once

#include <bit>
#include <cerrno>
#include <concepts>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

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
    using family_t = ADDRESS_FAMILY;
#else
    using native_socket_t = int;
    using socket_length_t = socklen_t;
    using socket_io_result_t = ssize_t;
    inline constexpr native_socket_t invalid_socket = -1;
    using family_t = sa_family_t;
#endif


// 自动 sockaddr 装填
    template<typename T>
    concept SocketAddressType =
        std::same_as<std::remove_cvref_t<T>, sockaddr_in> ||
        std::same_as<std::remove_cvref_t<T>, sockaddr_in6>;

    struct socket_address_view {
        const sockaddr* address;
        socket_length_t length;

        operator const sockaddr* () const noexcept {
            return address;
        }
    };

    template<SocketAddressType Addr>
    [[nodiscard]] inline socket_address_view
        auto_sockaddr(Addr& addr) noexcept {
        return {
            reinterpret_cast<const sockaddr*>(&addr),
            static_cast<socket_length_t>(sizeof(Addr))
        };
    }

    template<SocketAddressType Addr>
    [[nodiscard]] inline const void*
        get_in_addr(const Addr& addr) noexcept {
        using T = std::remove_cvref_t<Addr>;

        if constexpr (std::same_as<T, sockaddr_in>) {
            return &addr.sin_addr;
        } else if constexpr (std::same_as<T, sockaddr_in6>) {
            return &addr.sin6_addr;
        }
    }

    template<SocketAddressType Addr>
    [[nodiscard]] inline constexpr int
        get_family(const Addr&) noexcept {
        using T = std::remove_cvref_t<Addr>;

        if constexpr (std::same_as<T, sockaddr_in>) {
            return AF_INET;
        } else if constexpr (std::same_as<T, sockaddr_in6>) {
            return AF_INET6;
        }
    }

    inline constexpr std::endian native_endian = std::endian::native;

    // 返回最新一次错误代码
[[nodiscard]] inline int last_socket_error() noexcept {
#ifdef _WIN32
    return ::WSAGetLastError();
#else
    return errno;
#endif
}

namespace detail {

inline void set_invalid_argument_error() noexcept {
#ifdef _WIN32
    ::WSASetLastError(WSAEINVAL);
#else
    errno = EINVAL;
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

} // namespace detail

[[nodiscard]] inline const char* address_info_error_message(int error) noexcept {
#ifdef _WIN32
    return ::gai_strerrorA(error);
#else
    return ::gai_strerror(error);
#endif
}

// 地址转换
template<SocketAddressType Addr>
[[nodiscard]] inline void* get_in_addr_ptr(Addr& addr) noexcept {
    using T = std::remove_cvref_t<Addr>;
    if constexpr (std::same_as<T, sockaddr_in>) {
        return &addr.sin_addr;
    } else {
        return &addr.sin6_addr;
    }
}

template<SocketAddressType Addr>
[[nodiscard]] inline bool pton(const char* src, Addr& dst) noexcept {
    if (src == nullptr) {
        detail::set_invalid_argument_error();
        return false;
    }

    const int result = ::inet_pton(
        static_cast<int>(get_family(dst)), src, get_in_addr_ptr(dst)
    );
    if (result == 1) {
        using T = std::remove_cvref_t<Addr>;
        if constexpr (std::same_as<T, sockaddr_in>) {
            dst.sin_family = static_cast<family_t>(AF_INET);
        } else {
            dst.sin6_family = static_cast<family_t>(AF_INET6);
        }
        return true;
    }

    return false;
}

template <typename T>
inline constexpr std::size_t ip_string_capacity_v = 0;

template <>
inline constexpr std::size_t ip_string_capacity_v<sockaddr_in> = INET_ADDRSTRLEN;

template <>
inline constexpr std::size_t ip_string_capacity_v<sockaddr_in6> = INET6_ADDRSTRLEN;

template<SocketAddressType Addr>
[[nodiscard]] inline std::string ntop(const Addr& src) {
    char buf[ip_string_capacity_v<std::remove_cvref_t<Addr>>]{};
    const char* res = ::inet_ntop(
        static_cast<int>(get_family(src)),
        get_in_addr(src),
        buf,
        sizeof(buf)
    );

    if (res == nullptr) {
        throw std::runtime_error(
            std::string("inet_ntop failed: ") +
#ifdef _WIN32
            std::to_string(last_socket_error())
#else
            std::strerror(last_socket_error())
#endif
        );
    }
    return std::string(buf);
}



// Socket 选项
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
// 系统调用包装

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
        ::WSASetLastError(WSAEMSGSIZE);
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
        ::WSASetLastError(WSAEMSGSIZE);
        return SOCKET_ERROR;
    }
    return ::recv(socket, buffer, static_cast<int>(length), flags);
#else
    return ::recv(socket, buffer, length, flags);
#endif
}

[[nodiscard]] inline bool send_all_socket(
    native_socket_t socket,
    const char* buffer,
    std::size_t length,
    int flags = 0
) noexcept {
    if (buffer == nullptr && length != 0) {
        detail::set_invalid_argument_error();
        return false;
    }

    std::size_t total_sent = 0;
    const std::size_t max_chunk = static_cast<std::size_t>(
        (std::numeric_limits<socket_io_result_t>::max)()
    );

    while (total_sent < length) {
        const std::size_t remaining = length - total_sent;
        const std::size_t chunk_size = remaining > max_chunk
            ? max_chunk
            : remaining;
        const socket_io_result_t sent = send_socket(
            socket,
            buffer + total_sent,
            chunk_size,
            flags
        );

        if (sent > 0) {
            total_sent += static_cast<std::size_t>(sent);
            continue;
        }

        if (sent == 0) {
            detail::set_connection_closed_error();
            return false;
        }

        if (detail::is_interrupted_error(last_socket_error())) {
            continue;
        }

        return false;
    }

    return true;
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

    [[nodiscard]] explicit operator bool() const noexcept {
        return valid();
    }

    [[nodiscard]] native_socket_t get() const noexcept {
        return fd_;
    }

    [[nodiscard]] static sockfd create(
        int family,
        int type,
        int protocol
    ) noexcept {
        return sockfd{::socket(family, type, protocol)};
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
    if (filename == nullptr || mode == nullptr) {
        errno = EINVAL;
        return nullptr;
    }

#ifdef _MSC_VER
    FILE* file = nullptr;
    const errno_t err = ::fopen_s(&file, filename, mode);
    if (err != 0) {
        char buf[256];
        strerror_s(buf, sizeof(buf), err);
        std::fprintf(stderr, "Failed to open '%s':  %s\n", filename, buf);
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
