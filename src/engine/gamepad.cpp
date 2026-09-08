#include "gamepad.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_gamecontroller.h>

#include <algorithm>
#include <vector>

namespace simlib {
namespace {

struct ConnectedGamepad {
    SDL_GameController* controller = nullptr;
    SDL_JoystickID instance_id = -1;
};

std::vector<ConnectedGamepad> controllers;
float deadzone = 0.15f;
bool subsystem_initialised = false;

SDL_GameControllerButton to_sdl_button(GamepadButton button) {
    switch (button) {
    case GamepadButton::south: return SDL_CONTROLLER_BUTTON_A;
    case GamepadButton::east: return SDL_CONTROLLER_BUTTON_B;
    case GamepadButton::west: return SDL_CONTROLLER_BUTTON_X;
    case GamepadButton::north: return SDL_CONTROLLER_BUTTON_Y;
    case GamepadButton::back: return SDL_CONTROLLER_BUTTON_BACK;
    case GamepadButton::guide: return SDL_CONTROLLER_BUTTON_GUIDE;
    case GamepadButton::start: return SDL_CONTROLLER_BUTTON_START;
    case GamepadButton::left_stick: return SDL_CONTROLLER_BUTTON_LEFTSTICK;
    case GamepadButton::right_stick: return SDL_CONTROLLER_BUTTON_RIGHTSTICK;
    case GamepadButton::left_shoulder: return SDL_CONTROLLER_BUTTON_LEFTSHOULDER;
    case GamepadButton::right_shoulder: return SDL_CONTROLLER_BUTTON_RIGHTSHOULDER;
    case GamepadButton::dpad_up: return SDL_CONTROLLER_BUTTON_DPAD_UP;
    case GamepadButton::dpad_down: return SDL_CONTROLLER_BUTTON_DPAD_DOWN;
    case GamepadButton::dpad_left: return SDL_CONTROLLER_BUTTON_DPAD_LEFT;
    case GamepadButton::dpad_right: return SDL_CONTROLLER_BUTTON_DPAD_RIGHT;
    }
    return SDL_CONTROLLER_BUTTON_INVALID;
}

SDL_GameControllerAxis to_sdl_axis(GamepadAxis axis) {
    switch (axis) {
    case GamepadAxis::left_x: return SDL_CONTROLLER_AXIS_LEFTX;
    case GamepadAxis::left_y: return SDL_CONTROLLER_AXIS_LEFTY;
    case GamepadAxis::right_x: return SDL_CONTROLLER_AXIS_RIGHTX;
    case GamepadAxis::right_y: return SDL_CONTROLLER_AXIS_RIGHTY;
    case GamepadAxis::left_trigger: return SDL_CONTROLLER_AXIS_TRIGGERLEFT;
    case GamepadAxis::right_trigger: return SDL_CONTROLLER_AXIS_TRIGGERRIGHT;
    }
    return SDL_CONTROLLER_AXIS_INVALID;
}

void open_controller(int device_index) {
    if (!SDL_IsGameController(device_index)) return;
    SDL_GameController* controller = SDL_GameControllerOpen(device_index);
    if (!controller) return;
    SDL_Joystick* joystick = SDL_GameControllerGetJoystick(controller);
    const SDL_JoystickID instance_id = SDL_JoystickInstanceID(joystick);
    const auto existing = std::find_if(controllers.begin(), controllers.end(), [instance_id](const ConnectedGamepad& gamepad) {
        return gamepad.instance_id == instance_id;
    });
    if (existing != controllers.end()) {
        SDL_GameControllerClose(controller);
        return;
    }
    controllers.push_back({controller, instance_id});
}

const ConnectedGamepad* controller_at(int index) {
    return index >= 0 && index < static_cast<int>(controllers.size()) ? &controllers[static_cast<std::size_t>(index)] : nullptr;
}

float normalize_axis(Sint16 value) {
    const float normalized = value < 0 ? static_cast<float>(value) / 32768.0f : static_cast<float>(value) / 32767.0f;
    const float magnitude = std::abs(normalized);
    if (magnitude <= deadzone) return 0.0f;
    return std::clamp((magnitude - deadzone) / (1.0f - deadzone), 0.0f, 1.0f) * (normalized < 0.0f ? -1.0f : 1.0f);
}

} // namespace

bool gamepad_init() {
    if (subsystem_initialised) return true;
    if (SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER) != 0) return false;
    subsystem_initialised = true;
    for (int index = 0; index < SDL_NumJoysticks(); ++index) open_controller(index);
    return true;
}

void gamepad_shutdown() {
    for (const ConnectedGamepad& gamepad : controllers) SDL_GameControllerClose(gamepad.controller);
    controllers.clear();
    if (subsystem_initialised) SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER);
    subsystem_initialised = false;
}

void gamepad_handle_event(const SDL_Event& event) {
    if (!subsystem_initialised) return;
    if (event.type == SDL_CONTROLLERDEVICEADDED) {
        open_controller(event.cdevice.which);
    } else if (event.type == SDL_CONTROLLERDEVICEREMOVED) {
        const auto gamepad = std::find_if(controllers.begin(), controllers.end(), [&event](const ConnectedGamepad& connected) {
            return connected.instance_id == event.cdevice.which;
        });
        if (gamepad != controllers.end()) {
            SDL_GameControllerClose(gamepad->controller);
            controllers.erase(gamepad);
        }
    }
}

int gamepad_count() { return static_cast<int>(controllers.size()); }

bool gamepad_connected(int index) { return controller_at(index) != nullptr; }

std::string gamepad_name(int index) {
    const ConnectedGamepad* gamepad = controller_at(index);
    const char* name = gamepad ? SDL_GameControllerName(gamepad->controller) : nullptr;
    return name ? name : "";
}

bool gamepad_button(int index, GamepadButton button) {
    const ConnectedGamepad* gamepad = controller_at(index);
    return gamepad && SDL_GameControllerGetButton(gamepad->controller, to_sdl_button(button)) != 0;
}

float gamepad_axis(int index, GamepadAxis axis) {
    const ConnectedGamepad* gamepad = controller_at(index);
    return gamepad ? normalize_axis(SDL_GameControllerGetAxis(gamepad->controller, to_sdl_axis(axis))) : 0.0f;
}

void gamepad_set_deadzone(float value) { deadzone = std::clamp(value, 0.0f, 0.99f); }

float gamepad_deadzone() { return deadzone; }

} // namespace simlib