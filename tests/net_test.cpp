#include "net.h"

#include <cassert>
#include <chrono>
#include <iostream>
#include <string>
#include <thread>

namespace
{
    using Clock = std::chrono::steady_clock;

    template <typename Predicate>
    bool wait_for(Predicate done, int timeout_ms = 5000)
    {
        const auto limit = Clock::now() + std::chrono::milliseconds(timeout_ms);
        while (Clock::now() < limit)
        {
            if (done())
                return true;
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        return done();
    }

    void test_packets()
    {
        sl::PacketWriter writer;
        writer.write<std::uint8_t>(7);
        writer.write<std::int32_t>(-123456);
        writer.write<float>(1.5f);
        writer.write<std::uint64_t>(0x0102030405060708ULL);
        writer.write_string("hello lobby");
        // Integers are little-endian on the wire regardless of the host.
        assert(writer.bytes()[1] == 0xC0 && writer.bytes()[2] == 0x1D && writer.bytes()[3] == 0xFE);

        sl::PacketReader reader(writer.bytes());
        std::uint8_t a = 0;
        std::int32_t b = 0;
        float c = 0;
        std::uint64_t d = 0;
        std::string text;
        assert(reader.read(a) && a == 7);
        assert(reader.read(b) && b == -123456);
        assert(reader.read(c) && c == 1.5f);
        assert(reader.read(d) && d == 0x0102030405060708ULL);
        assert(reader.read_string(text) && text == "hello lobby");
        assert(reader.remaining() == 0 && reader.ok());
        std::uint8_t extra = 0;
        assert(!reader.read(extra) && !reader.ok());

        // A string whose length prefix overruns the packet is rejected, not read.
        const std::uint8_t lying[] = {0xFF, 0x00, 'a', 'b'};
        sl::PacketReader liar(lying, sizeof(lying));
        std::string out;
        assert(!liar.read_string(out) && !liar.ok());
    }

    void test_udp()
    {
        sl::NetServer server;
        assert(server.start(0, 4, 2));
        assert(server.running() && server.port() != 0);

        sl::NetClient client;
        assert(client.connect("127.0.0.1", server.port(), 2));

        sl::NetEvent event;
        std::uint32_t peer = 0;
        bool client_connected = false;
        assert(wait_for(
            [&]
            {
                while (server.poll(event))
                    if (event.type == sl::NetEventType::connected)
                        peer = event.peer;
                while (client.poll(event))
                    if (event.type == sl::NetEventType::connected)
                        client_connected = true;
                return peer != 0 && client_connected;
            }));
        assert(client.connected() && server.client_count() == 1);

        sl::PacketWriter hello;
        hello.write_string("ping");
        assert(client.send(hello.bytes(), 1));
        std::string received;
        assert(wait_for(
            [&]
            {
                client.poll(event);
                while (server.poll(event))
                    if (event.type == sl::NetEventType::received)
                    {
                        assert(event.peer == peer && event.channel == 1);
                        sl::PacketReader reader(event.data);
                        reader.read_string(received);
                    }
                return !received.empty();
            }));
        assert(received == "ping");

        // Server reply, then a broadcast, both reach the client.
        const std::string reply = "pong";
        assert(server.send(peer, reply.data(), reply.size()));
        server.broadcast("all", 3);
        int packets = 0;
        assert(wait_for(
            [&]
            {
                server.poll(event);
                while (client.poll(event))
                    if (event.type == sl::NetEventType::received)
                        ++packets;
                return packets == 2;
            }));

        // Bad arguments are rejected rather than crashing.
        assert(!client.send("x", 1, 9));
        assert(!server.send(999, "x", 1));

        client.disconnect();
        assert(!client.active());
        bool gone = false;
        assert(wait_for(
            [&]
            {
                while (server.poll(event))
                    if (event.type == sl::NetEventType::disconnected && event.peer == peer)
                        gone = true;
                return gone;
            }));
        assert(server.client_count() == 0);
        server.stop();
        assert(!server.running());

        // Connecting to a closed port ends in a disconnected event, not a hang.
        sl::NetClient lonely;
        assert(lonely.connect("127.0.0.1", 9, 2));
        bool failed = false;
        assert(wait_for(
            [&]
            {
                while (lonely.poll(event))
                    if (event.type == sl::NetEventType::disconnected)
                        failed = true;
                return failed;
            },
            15000));
        assert(!lonely.connected());
        assert(!sl::NetClient().connect("name.invalid", 1234, 2));
    }

    void test_tcp()
    {
        sl::TcpListener listener;
        assert(listener.listen(0));
        assert(listener.listening() && listener.port() != 0);

        sl::TcpSocket client;
        assert(client.connect("127.0.0.1", listener.port()));
        sl::TcpSocket accepted;
        assert(wait_for(
            [&]
            {
                client.update();
                return listener.accept(accepted);
            }));
        assert(wait_for([&] { return client.update() == sl::TcpState::connected; }));
        assert(accepted.connected());

        // Data sent before the connect finished must not be lost, and lines arrive in order.
        assert(client.send("NICK test\r\n"));
        assert(client.send("USER test 0 * :Test\r\n"));
        std::string line;
        assert(wait_for(
            [&]
            {
                accepted.receive(line);
                return line.size() >= 31;
            }));
        assert(line == "NICK test\r\nUSER test 0 * :Test\r\n");

        // A large message crosses the socket buffer and arrives intact.
        const std::string big(600000, 'x');
        assert(accepted.send(big));
        std::string got;
        assert(wait_for(
            [&]
            {
                client.receive(got);
                accepted.update();
                return got.size() >= big.size();
            }));
        assert(got == big);

        // An orderly close by the peer is reported.
        accepted.close();
        assert(wait_for(
            [&]
            {
                std::string unused;
                client.receive(unused);
                return client.state() == sl::TcpState::closed;
            }));
        assert(!client.error().empty());
        assert(!client.send("late"));

        // Refused connections and unknown hosts fail with an error rather than hanging.
        sl::TcpSocket refused;
        assert(refused.connect("127.0.0.1", 9, 5000));
        assert(wait_for([&] { return refused.update() == sl::TcpState::closed; }, 8000));
        assert(!refused.error().empty());

        sl::TcpSocket unknown;
        assert(unknown.connect("name.invalid", 6667, 8000));
        assert(wait_for([&] { return unknown.update() == sl::TcpState::closed; }, 15000));
        assert(!unknown.error().empty());
        assert(!unknown.connect("", 6667));
    }
}

int main()
{
    test_packets();
    test_udp();
    test_tcp();
    std::cout << "net_test passed\n";
    return 0;
}
