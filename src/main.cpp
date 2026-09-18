#include "unp.h"

#include <ctime>
#include <iostream>
#include <string_view>

int main() {
    unp::socket_env environment;

    unp::sockfd listen_socket = unp::sockfd::create(
        AF_INET,
        SOCK_STREAM,
        IPPROTO_TCP
    );
    if (!listen_socket.valid()) {
        std::cerr << "socket failed: " << unp::last_socket_error() << '\n';
        return 1;
    }

    sockaddr_in server_address{};
    server_address.sin_family = static_cast<unp::family_t>(AF_INET);
    server_address.sin_addr.s_addr = htonl(INADDR_ANY);
    server_address.sin_port = htons(13);

    if (unp::bind_socket(
            listen_socket.get(),
            unp::auto_sockaddr(server_address)
        ) != 0) {
        std::cerr << "bind failed: " << unp::last_socket_error() << '\n';
        return 1;
    }

    if (unp::listen_socket(listen_socket.get()) != 0) {
        std::cerr << "listen failed: " << unp::last_socket_error() << '\n';
        return 1;
    }

    std::cout << "Daytime server listening on 0.0.0.0:13...\n";

    for (;;) {
        sockaddr_storage client_address{};
        unp::socket_length_t client_address_length =
            static_cast<unp::socket_length_t>(sizeof(client_address));

        unp::sockfd connection = unp::accept_socket(
            listen_socket.get(),
            client_address,
            client_address_length
        );
        if (!connection.valid()) {
            std::cerr << "accept failed: " << unp::last_socket_error() << '\n';
            continue;
        }

        const std::time_t ticks = std::time(nullptr);
        char buffer[128]{};

#ifdef _WIN32
        if (::ctime_s(buffer, sizeof(buffer), &ticks) != 0) {
            std::cerr << "ctime_s failed\n";
            continue;
        }
#else
        if (::ctime_r(&ticks, buffer) == nullptr) {
            std::cerr << "ctime_r failed\n";
            continue;
        }
#endif

        const auto written = unp::writen(
            connection,
            std::string_view{buffer}
        );
        if (!written) {
            std::cerr << "write failed: "
                      << written.error().message() << '\n';
        }
    }
}
