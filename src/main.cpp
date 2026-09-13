#include "unp.h"

#include <cstdio>
#include <cstring>
#include <ctime>
#include <iostream>
#include <memory>


int main(int argc, char** argv) {

    unp::socket_env env; //初始化WSADATA,并在初始化失败(即初始化结果返回错误不为0的适合抛出异常),只在Windows有必要
    if (argc > 2) {
        std::cerr << "Usage: " << argv[0] << " [service-or-port]\n";
        return 1;
    }

    const char* service = argc == 2 ? argv[1] : "8080";

    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;

    addrinfo* res = nullptr;
    const int status = ::getaddrinfo(nullptr, service, &hints, &res);

    /*
    * struct addrinfo {
        int ai_flags;
        int ai_family;
        int ai_socktype;
        int ai_protocol;
        socklen_t ai_addrlen; //ai_addr的地址长度
        struct sockaddr *ai_addr;
        char *ai_canonname;
        struct addrinfo *ai_next; //指向链表的下一个结点
    };

    *addrinfo表示创建一个保存主机地址和端口等信息的结构体
    * ai_family中family为协议族，AF_INET 代表 IPv4，AF_INET6 代表 IPv6，AF_UNSPEC 代表不限
    * 套接字协议类型,SOCK_STREAM 代表 TCP，SOCK_DGRAM 代表 UDP

    * ai_flags：由 AI_PASSIVE、AI_CANONNAME 等 AI_* 常量组合而成的选项
    * ai_family：地址族（如 AF_INET 代表 IPv4，AF_INET6 代表 IPv6，AF_UNSPEC 代表不限）
    * ai_socktype：套接字类型（如 SOCK_STREAM 代表 TCP，SOCK_DGRAM 代表 UDP）
    * ai_protocol：具体协议
    * ai_addrlen：后面 ai_addr 地址结构的长度
    * ai_addr：指向底层 socket 地址结构（如 sockaddr_in）的指针
    * ai_next：指向下一个 addrinfo 结构体的指针（形成链表）
    *int getaddrinfo( const char *hostname, const char *service, const struct addrinfo *hints, struct addrinfo **result );
    * hostname:一个主机名或者地址串(IPv4的点分十进制串或者IPv6的16进制串)
    * service：服务名可以是十进制的端口号，也可以是已定义的服务名称，如ftp、http等
    * hints：可以是一个空指针，也可以是一个指向某个addrinfo结构体的指针，调用者在这个结构中填入关于期望返回的信息类型的暗示。
    *   举例来说：指定的服务既可支持TCP也可支持UDP，
    *    所以调用者可以把hints结构中的ai_socktype成员设置成SOCK_DGRAM使得返回的仅仅是适用于数据报套接口的信息。
    * result：本函数通过result指针参数返回一个指向addrinfo结构体链表的指针。返回值：0——成功，非0——出错
    */


    if (status != 0) {
        std::cerr << "getaddrinfo failed: "
                  << unp::address_info_error_message(status) << '\n';
        return 1;
    }

    // 1. 成功后立刻交由 unique_ptr 托管，后续无需再关心释放问题
    std::unique_ptr<addrinfo, decltype(&::freeaddrinfo)> auto_free_res(res, &::freeaddrinfo);

    unp::sockfd listen_socket;
    bool bound = false;

    for (addrinfo* p = auto_free_res.get();
        p != nullptr;
        p = p->ai_next) {

        listen_socket =
            unp::sockfd::create(p->ai_family, p->ai_socktype, p->ai_protocol);

        if (!listen_socket.valid()) {
            continue;
        }

        // socket 创建以后、bind 以前设置
        if (p->ai_family == AF_INET6) {
            if (unp::set_ipv6_dual_stack(listen_socket.get()) != 0) {
                continue;
            }
        }

        if (unp::bind_socket(
                listen_socket.get(),
                p->ai_addr,
                p->ai_addrlen
            ) == 0) {
            bound = true;
            break;
        }
    }

    // 3. 统一检查连接状态（这里不需要手动调用 freeaddrinfo 了）
    if (!bound) {
        std::cerr << "Failed to bind\n";
        return 1;
    }

    if (::listen(listen_socket.get(), SOMAXCONN) != 0) {
        std::cerr << "Listen Failed\n"  ;
        return 1;
    }

    std::cout << "[Info] Listening Successfully\n";
    // 💡 注意：原先这里的 ::freeaddrinfo(res); 删掉了，智能指针会在函数结束时自动释放它

    for (;;) {
        sockaddr_storage client_addr{};
        unp::socket_length_t addr_len =
            static_cast<unp::socket_length_t>(sizeof(client_addr));

        unp::sockfd connfd = unp::accept_socket(
            listen_socket.get(),
            client_addr,
            addr_len
        );
        if (!connfd.valid()) {
            std::cerr << "accept failed: "
                      << unp::last_socket_error() << '\n';
            continue;
        }

        const std::time_t ticks = std::time(nullptr);
        std::tm local_time{};
#ifdef _WIN32
        if (::localtime_s(&local_time, &ticks) != 0) {
            std::cerr << "localtime failed\n";
            continue;
        }
#else
        if (::localtime_r(&ticks, &local_time) == nullptr) {
            std::cerr << "localtime failed\n";
            continue;
        }
#endif

        char buffer[128]{};
        if (std::strftime(
                buffer,
                sizeof(buffer),
                "%a %b %d %H:%M:%S %Y\r\n",
                &local_time
            ) == 0) {
            std::cerr << "strftime failed\n";
            continue;
        }

        if (!unp::send_all_socket(
                connfd.get(),
                buffer,
                std::strlen(buffer)
            )) {
            std::cerr << "send failed: "
                      << unp::last_socket_error() << '\n';
        }
    }

    return 0;


}
