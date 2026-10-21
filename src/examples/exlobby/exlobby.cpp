// exlobby: a game lobby that uses an IRC channel to find games.
//
// Players meet in an IRC channel (plain text, port 6667 by default). A host announces a game with "!host" and other
// players see it in the Games list. Joining connects directly to the host over UDP using sl::NetServer / NetClient,
// where the players ready up and then move a dot around a shared arena.
//
//   exlobby [--irc-host HOST] [--irc-port PORT] [--channel #CHANNEL] [--nick NAME]
//           [--advertise ADDRESS] [--game-port PORT] [--gl | --vulkan | --d3d12]
//
// Hosting needs an address other players can reach (--advertise, or the Host field in the window) and the game port
// open to UDP traffic. IRC here is unencrypted: do not type anything private into the chat.

#include "sl.h"

#include "game_session.h"
#include "lobby.h"

#include <imgui.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <string>
#include <vector>

namespace
{
    struct Options
    {
        std::string irc_host = "salyut.ddns.net";
        int irc_port = 6667;
        std::string channel = "#simlib-lobby";
        std::string nick;
        std::string advertise;
        int game_port = 7777;
        bool auto_connect = false; // set when --irc-host is given
    };

    void print_usage()
    {
        std::printf("Usage: exlobby [options]\n"
                    "  --irc-host HOST      IRC server to use (default irc.libera.chat; connects at start)\n"
                    "  --irc-port PORT      IRC port, plain text (default 6667)\n"
                    "  --channel #CHANNEL   channel to meet in (default #simlib-lobby)\n"
                    "  --nick NAME          your nick (default simlib-<number>)\n"
                    "  --advertise ADDRESS  address other players use to reach you when hosting\n"
                    "  --game-port PORT     UDP port for hosted games (default 7777)\n"
                    "  --gl | --vulkan | --d3d12   choose the graphics backend\n");
    }

    // Parses our own options and leaves only the graphics backend flags in backend_args.
    bool parse_args(int argc, char *argv[], Options &options, std::vector<char *> &backend_args)
    {
        backend_args.push_back(argv[0]);
        for (int i = 1; i < argc; ++i)
        {
            const std::string arg = argv[i];
            const auto value = [&](std::string &out)
            {
                if (i + 1 >= argc)
                    return false;
                out = argv[++i];
                return true;
            };
            std::string text;
            if (arg == "--irc-host")
            {
                if (!value(options.irc_host))
                    return false;
                options.auto_connect = true;
            }
            else if (arg == "--channel")
            {
                if (!value(options.channel))
                    return false;
            }
            else if (arg == "--nick")
            {
                if (!value(options.nick))
                    return false;
            }
            else if (arg == "--advertise")
            {
                if (!value(options.advertise))
                    return false;
            }
            else if (arg == "--irc-port" || arg == "--game-port")
            {
                if (!value(text))
                    return false;
                const int port = std::atoi(text.c_str());
                if (port < 1 || port > 65535)
                    return false;
                (arg == "--irc-port" ? options.irc_port : options.game_port) = port;
            }
            else if (arg == "--gl" || arg == "--opengl" || arg == "--vulkan" || arg == "--d3d12")
            {
                backend_args.push_back(argv[i]);
            }
            else
            {
                return false;
            }
        }
        return true;
    }

    void copy_text(char *destination, std::size_t size, const std::string &text)
    {
        std::snprintf(destination, size, "%s", text.c_str());
    }
}

int main(int argc, char *argv[])
{
    sl::set_text_panel_style({true});
    Options options;
    std::vector<char *> backend_args;
    if (!parse_args(argc, argv, options, backend_args))
    {
        print_usage();
        return 1;
    }
    int backend_argc = static_cast<int>(backend_args.size());
    if (!sl::configure_graphics_backend_from_args(backend_argc, backend_args.data()) ||
        !sl::set_gfx_mode(sl::GFX_AUTODETECT_WINDOWED, 1100, 720))
    {
        return -1;
    }
    sl::gui_init();

    lobby::IrcClient irc;
    lobby::GameBoard board;
    lobby::GameSession session;

    std::deque<std::string> chat;
    const auto add_chat = [&](const std::string &line)
    {
        chat.push_back(line);
        while (chat.size() > 300)
            chat.pop_front();
    };

    bool announce_now = false;
    irc.on_status = [&](const std::string &text) { add_chat("* " + text); };
    irc.on_message = [&](const std::string &nick, const std::string &text, bool action)
    {
        bool wants_announce = false;
        if (!action && board.handle_message(nick, text, wants_announce))
        {
            announce_now = announce_now || wants_announce;
            return;
        }
        add_chat(action ? "* " + nick + " " + text : "<" + nick + "> " + text);
    };
    irc.on_user_joined = [&](const std::string &) { announce_now = true; };
    irc.on_user_left = [&](const std::string &nick) { board.remove(nick); };

    char irc_host[128], irc_port[8], nick[32], channel[64], advertise[128], game_port[8], title[48], chat_input[400];
    int max_players = 4;
    copy_text(irc_host, sizeof irc_host, options.irc_host);
    copy_text(irc_port, sizeof irc_port, std::to_string(options.irc_port));
    copy_text(nick, sizeof nick,
              options.nick.empty() ? "simlib" + std::to_string(1000 + sl::time_ms() % 9000) : options.nick);
    copy_text(channel, sizeof channel, options.channel);
    copy_text(advertise, sizeof advertise, options.advertise);
    copy_text(game_port, sizeof game_port, std::to_string(options.game_port));
    copy_text(title, sizeof title, "Come and play");
    chat_input[0] = '\0';

    std::string notice;
    bool announced = false;
    std::uint64_t next_announce = 0;
    std::uint64_t last_announce = 0;
    std::uint64_t last_frame = sl::time_ms();
    bool running = true;

    const auto leave_game = [&]
    {
        session.leave();
        if (announced && irc.state() == lobby::IrcClient::State::online)
            irc.say("!close");
        announced = false;
    };
    const auto connect_irc = [&]
    {
        board.clear();
        chat.clear();
        const int port = std::atoi(irc_port);
        if (port < 1 || port > 65535)
        {
            notice = "IRC port must be 1-65535.";
            return;
        }
        irc.connect(irc_host, static_cast<std::uint16_t>(port), nick, channel);
    };

    if (options.auto_connect)
        connect_irc();

    while (running)
    {
        sl::Event event;
        while (sl::poll_event(&event))
        {
            if (event.type() == sl::Event::Type::quit)
                running = false;
            else if (event.type() == sl::Event::Type::key_down && event.key() == sl::Event::Key::escape &&
                     !ImGui::GetIO().WantTextInput)
            {
                if (session.mode() != lobby::GameSession::Mode::none)
                    leave_game();
                else
                    running = false;
            }
            sl::gui_handle_event(event);
            sl::display_handle_event(event);
        }

        const std::uint64_t now = sl::time_ms();
        const float dt = std::min(0.1f, static_cast<float>(now - last_frame) / 1000.0f);
        last_frame = now;

        irc.update();
        board.expire();
        session.update();
        if (!session.error().empty() && session.mode() == lobby::GameSession::Mode::none)
            notice = session.error();

        // Hosts re-announce every 30 seconds and when someone new arrives. Announcing stops once the game starts.
        const bool online = irc.state() == lobby::IrcClient::State::online && irc.in_channel();
        const bool hosting = session.mode() == lobby::GameSession::Mode::host;
        const bool hosting_open = hosting && session.phase() == lobby::GameSession::Phase::waiting;
        if (hosting && announced && !hosting_open && online)
        {
            irc.say("!close");
            announced = false;
        }
        if (hosting_open && online && (now >= next_announce || (announce_now && now - last_announce > 5000)))
        {
            irc.say(lobby::make_host_command(advertise, session.port(), max_players, title));
            announced = true;
            last_announce = now;
            next_announce = now + 30000;
        }
        announce_now = false;

        // Movement in the arena.
        if (session.mode() != lobby::GameSession::Mode::none && session.joined() &&
            session.phase() == lobby::GameSession::Phase::playing && !ImGui::GetIO().WantTextInput)
        {
            float dx = 0, dy = 0;
            if (sl::key_down(SDL_SCANCODE_LEFT) || sl::key_down(SDL_SCANCODE_A))
                dx -= 1;
            if (sl::key_down(SDL_SCANCODE_RIGHT) || sl::key_down(SDL_SCANCODE_D))
                dx += 1;
            if (sl::key_down(SDL_SCANCODE_UP) || sl::key_down(SDL_SCANCODE_W))
                dy -= 1;
            if (sl::key_down(SDL_SCANCODE_DOWN) || sl::key_down(SDL_SCANCODE_S))
                dy += 1;
            for (const lobby::Player &p : session.players())
                if (p.id == session.my_id())
                    session.set_position(p.x + dx * 0.4f * dt, p.y + dy * 0.4f * dt);
        }

        const float width = static_cast<float>(sl::screen_width());
        const float height = static_cast<float>(sl::screen_height());
        const float half = width * 0.5f;

        sl::clear_to_colour(sl::screen, {18, 23, 32});

        // Arena drawn behind the GUI, in the lower right.
        const bool playing = session.mode() != lobby::GameSession::Mode::none && session.joined() &&
                             session.phase() == lobby::GameSession::Phase::playing;
        if (playing)
        {
            const float left = half + 4, top = 210, right = width - 8, bottom = height - 8;
            sl::rectfill(sl::screen, left, top, right, bottom, {28, 36, 50});
            sl::rect(sl::screen, left, top, right, bottom, {90, 110, 140}, 2.0f);
            for (const lobby::Player &p : session.players())
            {
                const float x = left + p.x * (right - left);
                const float y = top + p.y * (bottom - top);
                const bool me = p.id == session.my_id();
                sl::circlefill(sl::screen, x, y, 10, me ? sl::Colour{255, 210, 80} : sl::Colour{90, 190, 255});
                sl::textout(sl::get_default_monospace_font(), static_cast<int>(x) + 14, static_cast<int>(y) - 6,
                            {230, 230, 230}, p.nick);
            }
        }

        sl::gui_new_frame();

        // IRC window.
        ImGui::SetNextWindowPos({8, 8}, ImGuiCond_Always);
        ImGui::SetNextWindowSize({half - 12, height - 16}, ImGuiCond_Always);
        ImGui::Begin("IRC lobby", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);
        const bool irc_active = irc.state() != lobby::IrcClient::State::disconnected;
        ImGui::BeginDisabled(irc_active);
        ImGui::SetNextItemWidth(220);
        ImGui::InputText("Host", irc_host, sizeof irc_host);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(60);
        ImGui::InputText("Port", irc_port, sizeof irc_port, ImGuiInputTextFlags_CharsDecimal);
        ImGui::SetNextItemWidth(160);
        ImGui::InputText("Nick", nick, sizeof nick);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(160);
        ImGui::InputText("Channel", channel, sizeof channel);
        ImGui::EndDisabled();
        if (!irc_active)
        {
            if (ImGui::Button("Connect"))
                connect_irc();
        }
        else if (ImGui::Button("Disconnect"))
        {
            leave_game();
            irc.disconnect();
            board.clear();
        }
        ImGui::SameLine();
        const char *state_text = "offline";
        switch (irc.state())
        {
        case lobby::IrcClient::State::connecting: state_text = "connecting"; break;
        case lobby::IrcClient::State::registering: state_text = "registering"; break;
        case lobby::IrcClient::State::online:
            state_text = irc.in_channel() ? "in channel" : "joining";
            break;
        default: break;
        }
        ImGui::Text("%s as %s (plain text, no encryption)", state_text, irc.nick().empty() ? nick : irc.nick().c_str());

        const float users_height = 90;
        const float input_height = ImGui::GetFrameHeightWithSpacing();
        ImGui::BeginChild("chat", {0, -(users_height + input_height + 8)}, true);
        for (const std::string &line : chat)
            ImGui::TextWrapped("%s", line.c_str());
        if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 4)
            ImGui::SetScrollHereY(1.0f);
        ImGui::EndChild();

        ImGui::SetNextItemWidth(-60);
        const bool send = ImGui::InputText("##chat", chat_input, sizeof chat_input, ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::SameLine();
        const bool pressed = ImGui::Button("Send");
        if ((send || pressed) && chat_input[0])
        {
            if (irc.say(chat_input))
                add_chat("<" + irc.nick() + "> " + lobby::clean_text(chat_input));
            else
                add_chat("* Not connected to a channel.");
            chat_input[0] = '\0';
            if (send)
                ImGui::SetKeyboardFocusHere(-1);
        }

        ImGui::BeginChild("users", {0, users_height}, true);
        std::string users;
        for (const std::string &user : irc.users())
            users += user + "  ";
        ImGui::TextWrapped("In channel (%zu): %s", irc.users().size(), users.c_str());
        ImGui::EndChild();
        ImGui::End();

        // Games / session window.
        const float panel_height = playing ? 198 : height * 0.5f - 12;
        ImGui::SetNextWindowPos({half + 4, 8}, ImGuiCond_Always);
        ImGui::SetNextWindowSize({half - 12, playing ? panel_height : height - 16}, ImGuiCond_Always);
        if (session.mode() == lobby::GameSession::Mode::none)
        {
            ImGui::Begin("Games", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);
            if (!notice.empty())
            {
                ImGui::TextColored({1.0f, 0.6f, 0.4f, 1.0f}, "%s", notice.c_str());
                ImGui::SameLine();
                if (ImGui::SmallButton("dismiss"))
                    notice.clear();
            }
            ImGui::TextUnformatted("Open games in the channel:");
            if (ImGui::Button("Refresh") && online)
                irc.say("!games");
            ImGui::BeginChild("games", {0, height * 0.3f}, true);
            const std::vector<lobby::GameListing> games = board.games();
            if (games.empty())
                ImGui::TextDisabled("No games announced yet.");
            for (const lobby::GameListing &game : games)
            {
                ImGui::PushID(game.host_nick.c_str());
                if (ImGui::Button("Join"))
                {
                    if (session.join(game.address, game.port, nick))
                        notice.clear();
                }
                ImGui::PopID();
                ImGui::SameLine();
                ImGui::Text("%s  by %s  (%d max)  %s:%u", game.title.c_str(), game.host_nick.c_str(), game.max_players,
                            game.address.c_str(), static_cast<unsigned>(game.port));
            }
            ImGui::EndChild();

            ImGui::Separator();
            ImGui::TextUnformatted("Host a game");
            ImGui::InputText("Title", title, sizeof title);
            ImGui::SetNextItemWidth(100);
            ImGui::SliderInt("Players", &max_players, 2, 16);
            ImGui::InputText("Your address", advertise, sizeof advertise);
            ImGui::SetNextItemWidth(60);
            ImGui::InputText("UDP port", game_port, sizeof game_port, ImGuiInputTextFlags_CharsDecimal);
            ImGui::TextDisabled("The address must be reachable by the other players, and the UDP port open.");
            const int port = std::atoi(game_port);
            std::string address, trimmed = advertise;
            std::uint16_t unused = 0;
            const bool address_ok = lobby::split_endpoint(trimmed + ":1", address, unused);
            const bool can_host = port >= 1 && port <= 65535 && std::strlen(title) > 0 && address_ok;
            ImGui::BeginDisabled(!can_host);
            if (ImGui::Button("Host game"))
            {
                if (session.host(static_cast<std::uint16_t>(port), max_players, nick))
                {
                    notice.clear();
                    next_announce = 0;
                }
            }
            ImGui::EndDisabled();
            if (!can_host)
            {
                ImGui::SameLine();
                ImGui::TextDisabled("(enter your address, port and title)");
            }
            ImGui::End();
        }
        else
        {
            ImGui::Begin("Game", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);
            if (!session.joined())
            {
                ImGui::TextUnformatted("Connecting to the host ...");
            }
            else
            {
                const bool is_host = session.mode() == lobby::GameSession::Mode::host;
                if (is_host)
                    ImGui::Text("Hosting on UDP port %u", static_cast<unsigned>(session.port()));
                else
                    ImGui::Text("Connected, ping %u ms", session.ping_ms());
                for (const lobby::Player &p : session.players())
                    ImGui::BulletText("%s%s%s", p.nick.c_str(), p.id == 0 ? " (host)" : "",
                                      p.ready ? "  - ready" : "");
                if (session.phase() == lobby::GameSession::Phase::waiting)
                {
                    if (is_host)
                    {
                        ImGui::BeginDisabled(!session.can_start());
                        if (ImGui::Button("Start game"))
                            session.start_game();
                        ImGui::EndDisabled();
                        ImGui::SameLine();
                        ImGui::TextDisabled("starts when everyone is ready");
                    }
                    else
                    {
                        bool ready = false;
                        for (const lobby::Player &p : session.players())
                            if (p.id == session.my_id())
                                ready = p.ready;
                        if (ImGui::Checkbox("Ready", &ready))
                            session.set_ready(ready);
                    }
                }
                else
                {
                    ImGui::TextUnformatted("Arrow keys / WASD to move.");
                }
            }
            if (ImGui::Button("Leave game (Esc)"))
                leave_game();
            ImGui::End();
        }

        sl::gui_render();
        sl::show_video_bitmap();
        sl::end_frame();
    }

    leave_game();
    irc.disconnect();
    sl::gui_shutdown();
    sl::wait_for_graphics();
    sl::shutdown();
    return 0;
}
