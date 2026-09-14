#include "unp/unp.h"

#include <cstdio>
#include <cstring>
#include <ctime>
#include <iostream>
#include <memory>


int main(int argc, char** argv) {
    unp::net::socket_env env;

    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <IP-address>\n";
        return 1;
    }

    const char* serv_ip = argv[1];

    auto sock = unp::net::sockfd::create(
        AF_INET,
        SOCK_STREAM,
        0
    );

    if (!sock.valid()) {
        std::cerr << "socket error: " << unp::net::last_socket_error() << '\n';
        return 1;
    }

    sockaddr_in serv_addr{};
    if (!unp::net::pton(serv_ip, serv_addr)) {
        std::cerr << "Invalid IP address: " << serv_ip << '\n';
        return 1;
    }
    serv_addr.sin_port = htons(13);

    if (unp::net::connect_socket(sock.get(), unp::net::auto_sockaddr(serv_addr)) < 0) {
        std::cerr << "Connect failed: " << unp::net::last_socket_error() << '\n';
        return 1;
    }


    std::cout << "Connected to " << unp::net::ntop(serv_addr) << ":13\n";

    unp::net::io::buffered_reader reader(sock.get());
    std::string time_str;

    const auto n = unp::net::io::readline(reader, time_str);
    if (n > 0) {
        std::cout << "Time received: " << time_str;
    }
    else if (n == 0) {
        std::cout << "Server closed connection without data.\n";
    }
    else {
        std::cerr << "Read error: " << unp::net::last_socket_error() << '\n';
    }

    return 0;
}

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
