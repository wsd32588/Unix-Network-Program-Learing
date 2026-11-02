#include "unp.h"

#include <chrono>
#include <ctime>
#include <iostream>
#include <string_view>
#include <thread>

namespace {

    void serve_client(unp::sockfd connection) {
        // 仅用于观察并发效果，正式代码可删除。
        std::this_thread::sleep_for(std::chrono::seconds(2));

        const std::time_t ticks = std::time(nullptr);
        char buffer[128]{};

#ifdef _WIN32
        if (::ctime_s(buffer, sizeof(buffer), &ticks) != 0) {
            std::cerr << "ctime_s failed\n";
            return;
        }
#else
        if (::ctime_r(&ticks, buffer) == nullptr) {
            std::cerr << "ctime_r failed\n";
            return;
        }
#endif

        const auto written = unp::writen(
            connection,
            std::string_view{ buffer }
        );
        if (!written) {
            std::cerr << "write failed: "
                << written.error().message() << '\n';
        }
    }

} // namespace

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
        unp::sockfd connection =
            unp::accept_socket(listen_socket.get());
        if (!connection.valid()) {
            std::cerr << "accept failed: "
                << unp::last_socket_error() << '\n';
            continue;
        }

        try {
            std::thread(
                serve_client,
                std::move(connection)
            ).detach();
        }
        catch (const std::system_error& error) {
            std::cerr << "thread creation failed: "
                << error.what() << '\n';
        }
    }
}
