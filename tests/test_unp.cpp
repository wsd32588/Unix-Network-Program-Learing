#include "unp/fs/file.h"
#include "unp/address.h"
#include "unp/io.h"

#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>
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

    const unp::socket_address_view ipv4_view =
        unp::auto_sockaddr(ipv4);
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
    expect(!unp::pton(nullptr, invalid),
           "pton rejects a null address string");
}

void test_socket_ownership() {
    static_assert(!std::is_copy_constructible_v<unp::sockfd>);
    static_assert(!std::is_copy_assignable_v<unp::sockfd>);
    static_assert(std::is_nothrow_move_constructible_v<unp::sockfd>);
    static_assert(std::is_nothrow_move_assignable_v<unp::sockfd>);

    unp::sockfd socket =
        unp::sockfd::create(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    expect(socket.valid(), "sockfd::create creates a TCP socket");

    const unp::native_socket_t native_socket = socket.get();
    socket.reset(native_socket);
    expect(socket.get() == native_socket,
           "sockfd::reset ignores the currently owned socket");

    unp::sockfd moved = std::move(socket);
    expect(!socket.valid(), "moving sockfd clears the source");
    expect(moved.valid(), "moving sockfd preserves the socket");

    const unp::native_socket_t released_socket = moved.release();
    expect(!moved.valid(), "sockfd::release clears the owner");

    unp::sockfd adopted{released_socket};
    expect(adopted.valid(), "sockfd construction adopts a native socket");
    adopted.reset();
    expect(!adopted.valid(), "sockfd::reset closes the socket");
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
    expect(unp::listen_socket(listener.get(), 1) == 0,
           "listen starts the test listener");

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
    expect(unp::connect_socket(client.get(), listener_view) == 0,
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
    const auto written = unp::writen(peer, std::string_view{message});
    expect(written && *written == message.size(),
           "writen sends the complete message");

    const auto invalid_write = unp::writen(
        unp::invalid_socket,
        std::span<const std::byte>{}
    );
    expect(!invalid_write, "writen rejects an invalid socket");

    char buffer[64]{};
    expect(unp::readn(client.get(), nullptr, 0) == 0,
           "readn accepts an empty null buffer");
    expect(unp::readn(client.get(), nullptr, 1) < 0,
           "readn rejects a non-empty null buffer");

    const unp::socket_io_result_t received = unp::readn(
        client.get(),
        buffer,
        message.size()
    );
    expect(received == static_cast<unp::socket_io_result_t>(message.size()),
           "readn receives the requested number of bytes");

    expect(std::string(buffer, static_cast<std::size_t>(received)) == message,
           "received data matches the complete message");

    const std::string lines = "first line\nsecond";
    const auto lines_written = unp::writen(
        peer,
        std::string_view{lines}
    );
    expect(lines_written && *lines_written == lines.size(),
           "writen sends line-oriented test data");
    peer.reset();

    unp::buffered_reader reader{client.get()};
    char line_buffer[7]{};
    const unp::socket_io_result_t first_chunk =
        unp::readline(reader, line_buffer);
    expect(first_chunk == 6 && std::string(line_buffer) == "first ",
           "fixed-buffer readline returns a truncated chunk");

    static_assert(std::is_nothrow_move_constructible_v<
        unp::buffered_reader
    >);
    static_assert(std::is_nothrow_move_assignable_v<
        unp::buffered_reader
    >);
    unp::buffered_reader moved_reader{std::move(reader)};
    expect(!reader.valid() && moved_reader.valid(),
           "moving a buffered reader transfers its socket binding");

    std::string line;
    const unp::socket_io_result_t first_line_end =
        unp::readline(moved_reader, line);
    expect(first_line_end == 5 && line == "line\n",
           "move construction preserves unread buffered data");

    unp::buffered_reader assigned_reader;
    assigned_reader = std::move(moved_reader);
    expect(!moved_reader.valid() && assigned_reader.valid(),
           "move assignment transfers the socket binding");

    const unp::socket_io_result_t second_chunk =
        unp::readline(assigned_reader, line, 3);
    expect(second_chunk == 3 && line == "sec",
           "move assignment preserves unread buffered data");

    const unp::socket_io_result_t final_chunk =
        unp::readline(assigned_reader, line);
    expect(final_chunk == 3 && line == "ond",
           "string readline returns partial data before EOF");
    expect(unp::readline(assigned_reader, line) == 0 && line.empty(),
           "string readline reports EOF with no remaining data");

    assigned_reader.reset(unp::invalid_socket);
    expect(!assigned_reader.valid(), "reset can detach a buffered reader");
    char ignored = '\0';
    expect(assigned_reader.get_char(ignored) < 0,
           "an unbound buffered reader rejects reads");
}

void test_file_helpers() {
    expect(unp::fs::open_file(nullptr, "rb") == nullptr,
           "open_file rejects a null filename");

    FILE* file = unp::fs::open_file(__FILE__, "rb");
    expect(file != nullptr, "open_file opens an existing file");
    expect(std::fclose(file) == 0, "fclose closes the opened file");
}

} // namespace

int main() {
    unp::socket_env environment;
    test_address_helpers();
    test_socket_ownership();
    test_socket_io();
    test_file_helpers();
    return EXIT_SUCCESS;
}
