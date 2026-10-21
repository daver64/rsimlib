#include "game_session.h"
#include "lobby.h"

#include <cassert>
#include <chrono>
#include <iostream>
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

    void test_parse()
    {
        lobby::IrcMessage m;
        assert(lobby::parse_irc_line(":bob!u@h PRIVMSG #chan :hello there\r\n", m));
        assert(m.nick() == "bob" && m.command == "PRIVMSG" && m.params.size() == 2);
        assert(m.params[0] == "#chan" && m.params[1] == "hello there");

        assert(lobby::parse_irc_line("@time=2024-01-01T00:00:00Z;x=y :srv 353 me = #c :@op +voice plain", m));
        assert(m.command == "353" && m.params.size() == 4 && m.params[3] == "@op +voice plain");

        assert(lobby::parse_irc_line("ping :token", m));
        assert(m.command == "PING" && m.prefix.empty() && m.params[0] == "token");

        assert(!lobby::parse_irc_line("", m));
        assert(!lobby::parse_irc_line(":onlyprefix", m));
        assert(!lobby::parse_irc_line("@tags", m));

        // A flood of parameters is capped.
        std::string many = "CMD";
        for (int i = 0; i < 40; ++i)
            many += " p";
        assert(lobby::parse_irc_line(many, m) && m.params.size() == 15);

        assert(lobby::make_nick("bad nick!") == "badnick");
        assert(lobby::make_nick("9lives").find_first_of("0123456789") != 0);
        assert(lobby::make_channel("lobby") == "#lobby" && lobby::make_channel("#a,b") == "#ab");
        assert(lobby::clean_text(std::string("a\x03" "4b\x01" "c")) == "a4bc");
    }

    void test_board()
    {
        lobby::GameBoard board;
        bool announce = false;
        assert(board.handle_message("Bob", "!host example.org:7777 4 Fun game", announce));
        assert(board.games().size() == 1);
        const lobby::GameListing game = board.games()[0];
        assert(game.host_nick == "Bob" && game.address == "example.org" && game.port == 7777 &&
               game.max_players == 4 && game.title == "Fun game");

        // The same host updates its listing rather than adding another.
        board.handle_message("bob", "!host 10.0.0.2:9000 8 Renamed", announce);
        assert(board.games().size() == 1 && board.games()[0].port == 9000);

        // Invalid announcements are swallowed but not listed.
        assert(board.handle_message("eve", "!host bad_host:7777 4 x", announce));
        assert(board.handle_message("eve", "!host a.b:0 4 x", announce));
        assert(board.handle_message("eve", "!host a.b:70000 4 x", announce));
        assert(board.handle_message("eve", "!host a.b:7777 1 x", announce));
        assert(board.handle_message("eve", "!host a.b:7777 99 x", announce));
        assert(board.handle_message("eve", "!host a.b:7777 4", announce));
        assert(board.handle_message("eve", "!host a.b:7777 4 " + std::string(41, 'x'), announce));
        assert(board.handle_message("eve", "!host", announce));
        assert(board.games().size() == 1);

        assert(!board.handle_message("bob", "plain chat", announce));
        assert(!board.handle_message("bob", "!unknown", announce));
        assert(board.handle_message("bob", "!games", announce) && announce);

        // Expiry.
        board.expire(Clock::now() + std::chrono::seconds(lobby::GameBoard::expiry_seconds + 1));
        assert(board.games().empty());
        board.handle_message("bob", "!host a.b:7777 4 t", announce);
        board.handle_message("bob", "!close", announce);
        assert(board.games().empty());

        std::string address;
        std::uint16_t port = 0;
        assert(lobby::split_endpoint("host.name:65535", address, port) && port == 65535);
        assert(!lobby::split_endpoint("host", address, port) && !lobby::split_endpoint(":80", address, port));
        assert(!lobby::split_endpoint("a:b:80", address, port) && !lobby::split_endpoint("a:8x", address, port));
    }

    // A tiny scripted IRC server on loopback.
    struct FakeIrc
    {
        sl::TcpListener listener;
        sl::TcpSocket peer;
        std::string buffer;
        std::vector<std::string> lines;

        FakeIrc() { assert(listener.listen(0)); }

        void pump()
        {
            if (!peer.connected())
                listener.accept(peer);
            if (!peer.connected())
                return;
            peer.update();
            std::string data;
            peer.receive(data);
            buffer += data;
            std::size_t pos;
            while ((pos = buffer.find("\r\n")) != std::string::npos)
            {
                lines.push_back(buffer.substr(0, pos));
                buffer.erase(0, pos + 2);
            }
        }

        bool has(const std::string &line) const { return std::find(lines.begin(), lines.end(), line) != lines.end(); }
        void say(const std::string &line) { peer.send(line + "\r\n"); }
    };

    void test_irc_client()
    {
        FakeIrc server;
        lobby::IrcClient irc;
        std::vector<std::string> status, chat, left, joined;
        irc.on_status = [&](const std::string &s) { status.push_back(s); };
        irc.on_message = [&](const std::string &n, const std::string &t, bool action)
        { chat.push_back((action ? "*" : "") + n + ":" + t); };
        irc.on_user_left = [&](const std::string &n) { left.push_back(n); };
        irc.on_user_joined = [&](const std::string &n) { joined.push_back(n); };

        const auto run = [&](auto done)
        {
            return wait_for(
                [&]
                {
                    irc.update();
                    server.pump();
                    return done();
                });
        };

        assert(irc.connect("127.0.0.1", server.listener.port(), "tester", "test"));
        assert(irc.channel() == "#test");
        assert(run([&] { return server.has("NICK tester") && server.has("USER tester 0 * :simlib exlobby"); }));
        assert(irc.state() == lobby::IrcClient::State::registering);

        // Nick collision: the client retries with an underscore.
        server.say(":srv 433 * tester :Nickname is already in use");
        assert(run([&] { return server.has("NICK tester_"); }));
        server.say(":srv 001 tester_ :Welcome");
        assert(run([&] { return server.has("JOIN #test"); }));
        assert(irc.state() == lobby::IrcClient::State::online && irc.nick() == "tester_");

        server.say(":tester_!u@h JOIN #test");
        server.say(":srv 353 tester_ = #test :@op +voice tester_ plain");
        server.say(":bob!u@h JOIN #test");
        assert(run([&] { return irc.users().size() == 5 && !joined.empty(); }));
        assert(irc.in_channel() && irc.users().count("op") == 1 && irc.users().count("voice") == 1);
        assert(joined[0] == "bob");

        server.say("PING :abc123");
        assert(run([&] { return server.has("PONG :abc123"); }));

        server.say(":bob!u@h PRIVMSG #test :hello \x03" "4world");
        server.say(":bob!u@h PRIVMSG #test :\x01" "ACTION waves\x01");
        server.say(":bob!u@h PRIVMSG #test :\x01VERSION\x01");
        server.say(":bob!u@h PRIVMSG tester_ :private message");
        server.say(":bob!u@h PRIVMSG #other :elsewhere");
        server.say(":bob!u@h PRIVMSG #TEST :last");
        assert(run([&] { return chat.size() >= 3; }));
        assert(chat[0] == "bob:hello 4world" && chat[1] == "*bob:waves" && chat[2] == "bob:last" && chat.size() == 3);
        // CTCP VERSION must never be answered.
        for (const std::string &line : server.lines)
            assert(line.find("NOTICE") == std::string::npos);

        assert(irc.say("hi everyone"));
        assert(run([&] { return server.has("PRIVMSG #test :hi everyone"); }));

        // Messages are rate limited: a burst of 4, then roughly one a second.
        std::size_t before = server.lines.size();
        for (int i = 0; i < 8; ++i)
            irc.say("m" + std::to_string(i));
        const auto until = Clock::now() + std::chrono::milliseconds(400);
        while (Clock::now() < until)
        {
            irc.update();
            server.pump();
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        std::size_t sent = 0;
        for (std::size_t i = before; i < server.lines.size(); ++i)
            sent += server.lines[i].rfind("PRIVMSG", 0) == 0;
        assert(sent >= 3 && sent <= 5);

        // A long message is cut on a character boundary.
        std::string long_text;
        for (int i = 0; i < 300; ++i)
            long_text += "\xC3\xA9"; // 600 bytes of two-byte characters
        assert(irc.say(long_text));

        server.say(":bob!u@h PART #test :bye");
        server.say(":op!u@h QUIT :gone");
        server.say(":voice!u@h NICK :newvoice");
        assert(run([&] { return left.size() >= 3; }));
        assert(irc.users().count("bob") == 0 && irc.users().count("op") == 0 && irc.users().count("newvoice") == 1);

        // The server dropping the connection is reported, not hung on.
        server.peer.close();
        assert(run([&] { return irc.state() == lobby::IrcClient::State::disconnected; }));
        assert(!irc.in_channel() && irc.users().empty());
    }

    void test_irc_failures()
    {
        lobby::IrcClient irc;
        std::vector<std::string> status;
        irc.on_status = [&](const std::string &s) { status.push_back(s); };
        sl::TcpListener closed;
        assert(closed.listen(0));
        const std::uint16_t port = closed.port();
        closed.close();
        assert(irc.connect("127.0.0.1", port, "tester", "#test"));
        assert(wait_for(
            [&]
            {
                irc.update();
                return irc.state() == lobby::IrcClient::State::disconnected;
            }));
        assert(status.size() >= 2);
        assert(!irc.say("not connected"));
    }

    void test_session()
    {
        lobby::GameSession host, guest;
        assert(host.host(0, 4, "Hosty"));
        assert(host.joined() && host.my_id() == 0 && host.players().size() == 1);
        assert(host.can_start()); // alone, the host is ready

        assert(guest.join("127.0.0.1", host.port(), "Guesty"));
        const auto run = [&](auto done)
        {
            return wait_for(
                [&]
                {
                    host.update();
                    guest.update();
                    return done();
                });
        };
        assert(run([&] { return guest.joined() && guest.players().size() == 2 && host.players().size() == 2; }));
        assert(guest.my_id() == 1 && guest.players()[1].nick == "Guesty");

        // The host cannot start until the guest is ready.
        assert(!host.can_start() && !host.start_game());
        guest.set_ready(true);
        assert(run([&] { return host.can_start(); }));

        host.set_position(0.25f, 0.75f);
        guest.set_position(0.9f, 0.1f);
        assert(host.start_game());
        assert(run([&] { return guest.phase() == lobby::GameSession::Phase::playing; }));

        // Positions flow both ways; out of range values are clamped.
        guest.set_position(5.0f, -3.0f);
        assert(run(
            [&]
            {
                const auto &a = host.players();
                const auto &b = guest.players();
                return a.size() == 2 && b.size() == 2 && a[1].x == 1.0f && a[1].y == 0.0f && b[0].x == 0.25f &&
                       b[0].y == 0.75f;
            }));

        // A guest leaving updates the host's roster; the host leaving ends the guest's session with an error.
        guest.leave();
        assert(run([&] { return host.players().size() == 1; }));

        lobby::GameSession late;
        assert(late.join("127.0.0.1", host.port(), "Late"));
        // The game is in progress, so late joiners are turned away.
        assert(wait_for(
            [&]
            {
                host.update();
                late.update();
                return late.mode() == lobby::GameSession::Mode::none;
            }));
        assert(!late.error().empty() && host.players().size() == 1);

        lobby::GameSession second;
        lobby::GameSession hosting2;
        assert(hosting2.host(0, 2, "H2"));
        assert(second.join("127.0.0.1", hosting2.port(), "G2"));
        assert(wait_for(
            [&]
            {
                hosting2.update();
                second.update();
                return second.joined();
            }));
        hosting2.leave();
        assert(wait_for(
            [&]
            {
                second.update();
                return second.mode() == lobby::GameSession::Mode::none;
            }));
        assert(!second.error().empty());

        // Connecting to nothing fails cleanly.
        lobby::GameSession lost;
        assert(lost.join("127.0.0.1", 1, "Nobody"));
        assert(wait_for(
            [&]
            {
                lost.update();
                return lost.mode() == lobby::GameSession::Mode::none;
            },
            15000));
        assert(!lost.error().empty());
    }
}

int main()
{
    test_parse();
    test_board();
    test_irc_client();
    test_irc_failures();
    test_session();
    std::cout << "lobby_test passed\n";
    return 0;
}
