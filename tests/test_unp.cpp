#include "unp.h"

#include <cstdlib>
#include <iostream>
#include <string>
#include <type_traits>
#include <utility>

namespace {

template<typename T>
concept accepts_temporary_address = requires {
    unp::auto_sockaddr(T{});
};

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

void test_address_helpers() {
    static_assert(!accepts_temporary_address<sockaddr_in>);
    static_assert(!accepts_temporary_address<sockaddr_in6>);

    sockaddr_in ipv4{};
    expect(unp::pton("127.0.0.1", ipv4), "pton parses an IPv4 address");
    expect(ipv4.sin_family == AF_INET, "pton sets the IPv4 family");
    expect(unp::ntop(ipv4) == "127.0.0.1", "ntop formats an IPv4 address");

    const unp::socket_address_view ipv4_view = unp::auto_sockaddr(ipv4);
    expect(ipv4_view.address == reinterpret_cast<const sockaddr*>(&ipv4),
           "auto_sockaddr points at the source IPv4 address");
    expect(ipv4_view.length == static_cast<unp::socket_length_t>(sizeof(ipv4)),
           "auto_sockaddr reports the IPv4 address size");

    sockaddr_in6 ipv6{};
    expect(unp::pton("::1", ipv6), "pton parses an IPv6 address");
    expect(ipv6.sin6_family == AF_INET6, "pton sets the IPv6 family");
    expect(unp::ntop(ipv6) == "::1", "ntop formats an IPv6 address");

    sockaddr_in invalid{};
    expect(!unp::pton("not-an-address", invalid),
           "pton rejects an invalid address");
    expect(!unp::pton(nullptr, invalid), "pton rejects a null address string");
}

void test_socket_ownership() {
    static_assert(!std::is_copy_constructible_v<unp::sockfd>);
    static_assert(!std::is_copy_assignable_v<unp::sockfd>);
    static_assert(std::is_nothrow_move_constructible_v<unp::sockfd>);
    static_assert(std::is_nothrow_move_assignable_v<unp::sockfd>);

    unp::sockfd socket = unp::sockfd::create(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    expect(socket.valid(), "sockfd::create creates a TCP socket");

    unp::sockfd moved = std::move(socket);
    expect(!socket.valid(), "moving sockfd clears the source");
    expect(moved.valid(), "moving sockfd preserves the socket");

    moved.reset();
    expect(!moved.valid(), "sockfd::reset closes the socket");
}

void test_socket_io() {
    unp::sockfd listener = unp::sockfd::create(
        AF_INET,
        SOCK_STREAM,
        IPPROTO_TCP
    );
    expect(listener.valid(), "create makes a listener socket");

    sockaddr_in listener_address{};
    expect(unp::pton("127.0.0.1", listener_address),
           "pton prepares the loopback listener address");
    listener_address.sin_port = 0;

    const unp::socket_address_view listener_view =
        unp::auto_sockaddr(listener_address);
    expect(unp::bind_socket(listener.get(), listener_view) == 0,
           "bind_socket binds a socket_address_view");
    expect(::listen(listener.get(), 1) == 0, "listen starts the test listener");

    unp::socket_length_t listener_length =
        static_cast<unp::socket_length_t>(sizeof(listener_address));
    expect(::getsockname(
               listener.get(),
               reinterpret_cast<sockaddr*>(&listener_address),
               &listener_length
           ) == 0,
           "getsockname reads the assigned port");

    unp::sockfd client = unp::sockfd::create(
        AF_INET,
        SOCK_STREAM,
        IPPROTO_TCP
    );
    expect(client.valid(), "create makes a client socket");
    expect(::connect(
               client.get(),
               listener_view.address,
               listener_view.length
           ) == 0,
           "client connects to the loopback listener");

    sockaddr_storage peer_address{};
    unp::socket_length_t peer_length =
        static_cast<unp::socket_length_t>(sizeof(peer_address));
    unp::sockfd peer = unp::accept_socket(
        listener.get(),
        peer_address,
        peer_length
    );
    expect(peer.valid(), "accept_socket accepts the loopback client");

    const std::string message = "unp send-all test";
    expect(unp::send_all_socket(peer.get(), message.data(), message.size()),
           "send_all_socket sends the complete message");

    char buffer[64]{};
    std::size_t received_total = 0;
    while (received_total < message.size()) {
        const unp::socket_io_result_t received = unp::recv_socket(
            client.get(),
            buffer + received_total,
            message.size() - received_total
        );
        expect(received > 0, "recv_socket receives test data");
        received_total += static_cast<std::size_t>(received);
    }

    expect(std::string(buffer, received_total) == message,
           "received data matches the complete message");
}

} // namespace

int main() {
    unp::socket_env environment;
    test_address_helpers();
    test_socket_ownership();
    test_socket_io();
    return EXIT_SUCCESS;
}
