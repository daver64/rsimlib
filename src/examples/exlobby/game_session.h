#pragma once

// The game session for exlobby: one player hosts (a sl::NetServer), the others join (a sl::NetClient). The host is player 0
// and is authoritative for the roster and for starting the game. Everything received is validated.
//
// Channel 0 is reliable (hello, welcome, roster, ready) and channel 1 is unreliable (positions, 20 per second).

#include "net.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace lobby
{
    struct Player
    {
        std::uint32_t id = 0;
        std::string nick;
        bool ready = false;
        float x = 0.5f; // arena position, 0..1
        float y = 0.5f;
    };

    class GameSession
    {
    public:
        enum class Mode
        {
            none,
            host,
            client
        };
        enum class Phase : std::uint8_t
        {
            waiting = 0,
            playing = 1
        };

        Mode mode() const { return mode_; }
        Phase phase() const { return phase_; }
        /** True when hosting, or once a joined client has received the host's welcome. */
        bool joined() const { return mode_ == Mode::host || (mode_ == Mode::client && my_id_ != invalid_id); }
        std::uint32_t my_id() const { return my_id_; }
        const std::vector<Player> &players() const { return players_; }
        const std::string &error() const { return error_; }
        std::uint16_t port() const { return server_.port(); }
        std::uint32_t ping_ms() const { return client_.ping_ms(); }

        bool host(std::uint16_t port, int max_players, const std::string &nick)
        {
            leave();
            error_.clear();
            if (!server_.start(port, static_cast<std::size_t>(std::clamp(max_players, 2, 16) - 1), 2))
            {
                error_ = "Unable to listen on port " + std::to_string(port);
                return false;
            }
            mode_ = Mode::host;
            my_id_ = 0;
            next_id_ = 1;
            players_.push_back(Player{0, clean_nick(nick), true});
            return true;
        }

        bool join(const std::string &address, std::uint16_t port, const std::string &nick)
        {
            leave();
            error_.clear();
            if (!client_.connect(address, port, 2))
            {
                error_ = "Unable to connect to " + address + ":" + std::to_string(port);
                return false;
            }
            mode_ = Mode::client;
            nick_ = clean_nick(nick);
            return true;
        }

        void leave()
        {
            server_.stop();
            client_.disconnect();
            // Let the disconnect reach the other side.
            sl::NetEvent ignored;
            while (client_.poll(ignored)) {}
            mode_ = Mode::none;
            phase_ = Phase::waiting;
            my_id_ = invalid_id;
            players_.clear();
            peers_.clear();
            roster_dirty_ = false;
        }

        /** Host only: the game can start when every other player is ready. */
        bool can_start() const
        {
            if (mode_ != Mode::host || phase_ != Phase::waiting)
                return false;
            return std::all_of(players_.begin(), players_.end(), [](const Player &p) { return p.ready; });
        }

        bool start_game()
        {
            if (!can_start())
                return false;
            phase_ = Phase::playing;
            roster_dirty_ = true;
            return true;
        }

        void set_ready(bool ready)
        {
            if (mode_ == Mode::client && joined())
            {
                if (Player *me = find(my_id_))
                    me->ready = ready;
                sl::PacketWriter packet;
                packet.write(Msg::ready);
                packet.write<std::uint8_t>(ready ? 1 : 0);
                client_.send(packet.bytes(), 0, true);
            }
        }

        void set_position(float x, float y)
        {
            if (Player *me = find(my_id_))
            {
                me->x = clamp01(x);
                me->y = clamp01(y);
            }
        }

        void update()
        {
            if (mode_ == Mode::host)
                update_host();
            else if (mode_ == Mode::client)
                update_client();
        }

    private:
        enum class Msg : std::uint8_t
        {
            hello = 1,
            ready = 2,
            pos = 3,
            welcome = 10,
            roster = 11,
            state = 12
        };
        static constexpr std::uint32_t invalid_id = 0xFFFFFFFFu;
        using Clock = std::chrono::steady_clock;

        static float clamp01(float v) { return std::isfinite(v) ? std::clamp(v, 0.0f, 1.0f) : 0.5f; }

        static std::string clean_nick(const std::string &nick)
        {
            std::string out;
            for (char c : nick)
                if (static_cast<unsigned char>(c) >= 0x20 && c != 0x7f)
                    out.push_back(c);
            if (out.size() > 24)
                out.resize(24);
            return out.empty() ? std::string("player") : out;
        }

        Player *find(std::uint32_t id)
        {
            for (Player &p : players_)
                if (p.id == id)
                    return &p;
            return nullptr;
        }

        bool tick_due()
        {
            const Clock::time_point now = Clock::now();
            if (now - last_state_ < std::chrono::milliseconds(50))
                return false;
            last_state_ = now;
            return true;
        }

        void send_roster_to_all()
        {
            sl::PacketWriter packet;
            packet.write(Msg::roster);
            packet.write(static_cast<std::uint8_t>(phase_));
            packet.write(static_cast<std::uint8_t>(players_.size()));
            for (const Player &p : players_)
            {
                packet.write(p.id);
                packet.write_string(p.nick);
                packet.write<std::uint8_t>(p.ready ? 1 : 0);
            }
            server_.broadcast(packet.bytes(), 0, true);
        }

        void update_host()
        {
            sl::NetEvent event;
            while (server_.poll(event))
            {
                if (event.type == sl::NetEventType::disconnected)
                {
                    auto it = peers_.find(event.peer);
                    if (it != peers_.end())
                    {
                        players_.erase(std::remove_if(players_.begin(), players_.end(),
                                                      [&](const Player &p) { return p.id == it->second; }),
                                       players_.end());
                        peers_.erase(it);
                        roster_dirty_ = true;
                    }
                }
                else if (event.type == sl::NetEventType::received)
                {
                    host_receive(event);
                }
            }
            if (roster_dirty_)
            {
                roster_dirty_ = false;
                send_roster_to_all();
            }
            if (tick_due() && server_.client_count() > 0)
            {
                sl::PacketWriter packet;
                packet.write(Msg::state);
                packet.write(static_cast<std::uint8_t>(players_.size()));
                for (const Player &p : players_)
                {
                    packet.write(p.id);
                    packet.write(p.x);
                    packet.write(p.y);
                }
                server_.broadcast(packet.bytes(), 1, false);
            }
        }

        void host_receive(const sl::NetEvent &event)
        {
            sl::PacketReader reader(event.data);
            Msg type;
            if (!reader.read(type))
                return;
            const auto known = peers_.find(event.peer);
            if (type == Msg::hello)
            {
                std::string nick;
                if (known != peers_.end() || !reader.read_string(nick) || phase_ != Phase::waiting)
                {
                    if (known == peers_.end())
                        server_.disconnect(event.peer);
                    return;
                }
                Player player;
                player.id = next_id_++;
                player.nick = clean_nick(nick);
                peers_[event.peer] = player.id;
                players_.push_back(player);
                sl::PacketWriter welcome;
                welcome.write(Msg::welcome);
                welcome.write(player.id);
                server_.send(event.peer, welcome.bytes(), 0, true);
                roster_dirty_ = true;
                return;
            }
            if (known == peers_.end())
                return; // ignore anything before hello
            Player *player = find(known->second);
            if (!player)
                return;
            if (type == Msg::ready)
            {
                std::uint8_t value = 0;
                if (reader.read(value) && phase_ == Phase::waiting)
                {
                    player->ready = value != 0;
                    roster_dirty_ = true;
                }
            }
            else if (type == Msg::pos)
            {
                float x, y;
                if (reader.read(x) && reader.read(y))
                {
                    player->x = clamp01(x);
                    player->y = clamp01(y);
                }
            }
        }

        void update_client()
        {
            sl::NetEvent event;
            while (client_.poll(event))
            {
                if (event.type == sl::NetEventType::connected)
                {
                    sl::PacketWriter hello;
                    hello.write(Msg::hello);
                    hello.write_string(nick_);
                    client_.send(hello.bytes(), 0, true);
                }
                else if (event.type == sl::NetEventType::disconnected)
                {
                    error_ = joined() ? "The host closed the game." : "Could not connect to the game.";
                    leave();
                    return;
                }
                else
                {
                    client_receive(event);
                }
            }
            if (joined() && tick_due())
            {
                if (const Player *me = find(my_id_))
                {
                    sl::PacketWriter packet;
                    packet.write(Msg::pos);
                    packet.write(me->x);
                    packet.write(me->y);
                    client_.send(packet.bytes(), 1, false);
                }
            }
        }

        void client_receive(const sl::NetEvent &event)
        {
            sl::PacketReader reader(event.data);
            Msg type;
            if (!reader.read(type))
                return;
            if (type == Msg::welcome)
            {
                std::uint32_t id;
                if (my_id_ == invalid_id && reader.read(id))
                    my_id_ = id;
            }
            else if (type == Msg::roster && my_id_ != invalid_id)
            {
                std::uint8_t phase = 0, count = 0;
                if (!reader.read(phase) || !reader.read(count))
                    return;
                std::vector<Player> roster;
                for (int i = 0; i < count; ++i)
                {
                    Player p;
                    std::uint8_t ready = 0;
                    if (!reader.read(p.id) || !reader.read_string(p.nick) || !reader.read(ready))
                        return;
                    p.nick = clean_nick(p.nick);
                    p.ready = ready != 0;
                    if (const Player *old = find(p.id))
                    {
                        p.x = old->x;
                        p.y = old->y;
                    }
                    roster.push_back(p);
                }
                phase_ = phase == 1 ? Phase::playing : Phase::waiting;
                players_ = roster;
            }
            else if (type == Msg::state && my_id_ != invalid_id)
            {
                std::uint8_t count = 0;
                if (!reader.read(count))
                    return;
                for (int i = 0; i < count; ++i)
                {
                    std::uint32_t id;
                    float x, y;
                    if (!reader.read(id) || !reader.read(x) || !reader.read(y))
                        return;
                    if (id == my_id_)
                        continue;
                    if (Player *p = find(id))
                    {
                        p->x = clamp01(x);
                        p->y = clamp01(y);
                    }
                }
            }
        }

        Mode mode_ = Mode::none;
        Phase phase_ = Phase::waiting;
        sl::NetServer server_;
        sl::NetClient client_;
        std::vector<Player> players_;
        std::map<std::uint32_t, std::uint32_t> peers_; // net peer -> player id
        std::uint32_t my_id_ = invalid_id;
        std::uint32_t next_id_ = 1;
        std::string nick_;
        std::string error_;
        bool roster_dirty_ = false;
        Clock::time_point last_state_ = Clock::now();
    };

} // namespace lobby
