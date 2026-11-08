#include "stdafx.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <mutex>

namespace {
struct AndroidPadState {
    std::array<unsigned int, 256> actionMap{};
    std::array<unsigned int, 256> menuMap{};
    unsigned int buttons = 0;
    unsigned int previousButtons = 0;
    unsigned int pressedButtons = 0;
    unsigned int releasedButtons = 0;
    float lx = 0.0f;
    float ly = 0.0f;
    float rx = 0.0f;
    float ry = 0.0f;
    unsigned char leftTrigger = 0;
    unsigned char rightTrigger = 0;
    float sensitivity = 1.0f;
    std::chrono::steady_clock::time_point lastInput = std::chrono::steady_clock::now();
    bool menuDisplayed = false;
};

std::array<AndroidPadState, 4> g_pads;
std::mutex g_inputMutex;

AndroidPadState& padFor(int index) {
    if (index < 0 || index >= static_cast<int>(g_pads.size())) index = 0;
    return g_pads[static_cast<std::size_t>(index)];
}

float clampAxis(float value) {
    return std::max(-1.0f, std::min(1.0f, value));
}
}

C_4JInput InputManager;

void C_4JInput::Initialise(int, unsigned char, unsigned char, unsigned char) {
    std::lock_guard<std::mutex> lock(g_inputMutex);
    for (AndroidPadState& pad : g_pads) {
        pad = AndroidPadState{};
        for (unsigned int action = 0; action < 256; ++action) {
            pad.actionMap[action] = 0;
            pad.menuMap[action] = 0;
        }

        // Match the Legacy Edition Xbox controller action table. Android
        // touch controls emit the same controller bit masks, so the existing
        // LocalPlayer/Input.cpp path can consume them unchanged.
        pad.menuMap[ACTION_MENU_A] = _360_JOY_BUTTON_A;
        pad.menuMap[ACTION_MENU_B] = _360_JOY_BUTTON_B;
        pad.menuMap[ACTION_MENU_X] = _360_JOY_BUTTON_X;
        pad.menuMap[ACTION_MENU_Y] = _360_JOY_BUTTON_Y;
        pad.menuMap[ACTION_MENU_OK] = _360_JOY_BUTTON_A;
        pad.menuMap[ACTION_MENU_CANCEL] = _360_JOY_BUTTON_B;
        pad.menuMap[ACTION_MENU_UP] = _360_JOY_BUTTON_DPAD_UP;
        pad.menuMap[ACTION_MENU_DOWN] = _360_JOY_BUTTON_DPAD_DOWN;
        pad.menuMap[ACTION_MENU_LEFT] = _360_JOY_BUTTON_DPAD_LEFT;
        pad.menuMap[ACTION_MENU_RIGHT] = _360_JOY_BUTTON_DPAD_RIGHT;
        pad.menuMap[ACTION_MENU_PAGEUP] = _360_JOY_BUTTON_LT;
        pad.menuMap[ACTION_MENU_PAGEDOWN] = _360_JOY_BUTTON_RT;
        pad.menuMap[ACTION_MENU_RIGHT_SCROLL] = _360_JOY_BUTTON_RB;
        pad.menuMap[ACTION_MENU_LEFT_SCROLL] = _360_JOY_BUTTON_LB;
        pad.menuMap[ACTION_MENU_PAUSEMENU] = _360_JOY_BUTTON_START;

        pad.actionMap[MINECRAFT_ACTION_JUMP] = _360_JOY_BUTTON_A;
        pad.actionMap[MINECRAFT_ACTION_USE] = _360_JOY_BUTTON_LT;
        pad.actionMap[MINECRAFT_ACTION_ACTION] = _360_JOY_BUTTON_RT;
        pad.actionMap[MINECRAFT_ACTION_INVENTORY] = _360_JOY_BUTTON_Y;
        pad.actionMap[MINECRAFT_ACTION_PAUSEMENU] = _360_JOY_BUTTON_START;
        pad.actionMap[MINECRAFT_ACTION_DROP] = _360_JOY_BUTTON_B;
        pad.actionMap[MINECRAFT_ACTION_SNEAK_TOGGLE] = _360_JOY_BUTTON_RTHUMB;
        pad.actionMap[MINECRAFT_ACTION_CRAFTING] = _360_JOY_BUTTON_X;
        pad.actionMap[MINECRAFT_ACTION_RENDER_THIRD_PERSON] = _360_JOY_BUTTON_LTHUMB;
        pad.actionMap[MINECRAFT_ACTION_DPAD_LEFT] = _360_JOY_BUTTON_DPAD_LEFT;
        pad.actionMap[MINECRAFT_ACTION_DPAD_RIGHT] = _360_JOY_BUTTON_DPAD_RIGHT;
        pad.actionMap[MINECRAFT_ACTION_DPAD_UP] = _360_JOY_BUTTON_DPAD_UP;
        pad.actionMap[MINECRAFT_ACTION_DPAD_DOWN] = _360_JOY_BUTTON_DPAD_DOWN;
    }
}
void C_4JInput::Tick() {
    std::lock_guard<std::mutex> lock(g_inputMutex);
    for (AndroidPadState& pad : g_pads) {
        pad.pressedButtons = pad.buttons & ~pad.previousButtons;
        pad.releasedButtons = pad.previousButtons & ~pad.buttons;
        pad.previousButtons = pad.buttons;
    }
}
void C_4JInput::SetDeadzoneAndMovementRange(unsigned int, unsigned int) {}
void C_4JInput::SetGameJoypadMaps(unsigned char map, unsigned char action, unsigned int value) {
    std::lock_guard<std::mutex> lock(g_inputMutex);
    for (AndroidPadState& pad : g_pads) {
        if (map == MAP_STYLE_0) pad.actionMap[action] = value;
        else pad.menuMap[action] = value;
    }
}
unsigned int C_4JInput::GetGameJoypadMaps(unsigned char map, unsigned char action) {
    std::lock_guard<std::mutex> lock(g_inputMutex);
    return map == MAP_STYLE_0 ? g_pads[0].actionMap[action] : g_pads[0].menuMap[action];
}
void C_4JInput::SetJoypadMapVal(int index, unsigned char map) {
    std::lock_guard<std::mutex> lock(g_inputMutex);
    padFor(index).menuDisplayed = map != 0;
}
unsigned char C_4JInput::GetJoypadMapVal(int index) {
    std::lock_guard<std::mutex> lock(g_inputMutex);
    return padFor(index).menuDisplayed ? 1 : 0;
}
void C_4JInput::SetJoypadSensitivity(int index, float sensitivity) {
    std::lock_guard<std::mutex> lock(g_inputMutex);
    padFor(index).sensitivity = std::max(0.05f, std::min(4.0f, sensitivity));
}
unsigned int C_4JInput::GetValue(int index, unsigned char action, bool) {
    std::lock_guard<std::mutex> lock(g_inputMutex);
    const AndroidPadState& pad = padFor(index);
    const unsigned int mask = pad.actionMap[action];
    return pad.buttons & mask;
}
bool C_4JInput::ButtonPressed(int index, unsigned char action) {
    std::lock_guard<std::mutex> lock(g_inputMutex);
    const AndroidPadState& pad = padFor(index);
    const unsigned int mask = action == 255 ? pad.buttons : pad.actionMap[action];
    return (pad.pressedButtons & mask) != 0;
}
bool C_4JInput::ButtonReleased(int index, unsigned char action) {
    std::lock_guard<std::mutex> lock(g_inputMutex);
    const AndroidPadState& pad = padFor(index);
    const unsigned int mask = pad.actionMap[action];
    return (pad.releasedButtons & mask) != 0;
}
bool C_4JInput::ButtonDown(int index, unsigned char action) {
    std::lock_guard<std::mutex> lock(g_inputMutex);
    const AndroidPadState& pad = padFor(index);
    const unsigned int mask = action == 255 ? pad.buttons : pad.actionMap[action];
    return (pad.buttons & mask) != 0;
}
void C_4JInput::SetJoypadStickAxisMap(int, unsigned int, unsigned int) {}
void C_4JInput::SetJoypadStickTriggerMap(int, unsigned int, unsigned int) {}
void C_4JInput::SetKeyRepeatRate(float, float) {}
void C_4JInput::SetDebugSequence(const char*, int (*)(LPVOID), LPVOID) {}
FLOAT C_4JInput::GetIdleSeconds(int index) {
    std::lock_guard<std::mutex> lock(g_inputMutex);
    const auto elapsed = std::chrono::steady_clock::now() - padFor(index).lastInput;
    return std::chrono::duration<float>(elapsed).count();
}
bool C_4JInput::IsPadConnected(int) { return true; }
float C_4JInput::GetJoypadStick_LX(int index, bool) {
    std::lock_guard<std::mutex> lock(g_inputMutex);
    return padFor(index).lx;
}
float C_4JInput::GetJoypadStick_LY(int index, bool) {
    std::lock_guard<std::mutex> lock(g_inputMutex);
    return padFor(index).ly;
}
float C_4JInput::GetJoypadStick_RX(int index, bool) {
    std::lock_guard<std::mutex> lock(g_inputMutex);
    return padFor(index).rx;
}
float C_4JInput::GetJoypadStick_RY(int index, bool) {
    std::lock_guard<std::mutex> lock(g_inputMutex);
    return padFor(index).ry;
}
unsigned char C_4JInput::GetJoypadLTrigger(int index, bool) {
    std::lock_guard<std::mutex> lock(g_inputMutex);
    return padFor(index).leftTrigger;
}
unsigned char C_4JInput::GetJoypadRTrigger(int index, bool) {
    std::lock_guard<std::mutex> lock(g_inputMutex);
    return padFor(index).rightTrigger;
}
void C_4JInput::SetMenuDisplayed(int index, bool displayed) {
    std::lock_guard<std::mutex> lock(g_inputMutex);
    padFor(index).menuDisplayed = displayed;
}
EKeyboardResult C_4JInput::RequestKeyboard(LPCWSTR, LPCWSTR, DWORD, UINT, int (*)(LPVOID, const bool), LPVOID, EKeyboardMode) {
    return EKeyboard_Cancelled;
}
void C_4JInput::GetText(uint16_t* output) {
    if (output) output[0] = 0;
}
bool C_4JInput::VerifyStrings(WCHAR**, int, int (*callback)(LPVOID, STRING_VERIFY_RESPONSE*), LPVOID parameter) {
    if (callback) {
        STRING_VERIFY_RESPONSE response{};
        callback(parameter, &response);
    }
    return true;
}
void C_4JInput::CancelQueuedVerifyStrings(int (*)(LPVOID, STRING_VERIFY_RESPONSE*), LPVOID) {}
void C_4JInput::CancelAllVerifyInProgress() {}

extern "C" void MclgAndroidInputInitialize() {
    InputManager.Initialise(1, 3, 24, 24);
}

extern "C" void MclgAndroidInputButton(unsigned int mask, int pressed) {
    std::lock_guard<std::mutex> lock(g_inputMutex);
    AndroidPadState& pad = g_pads[0];
    if (pressed) pad.buttons |= mask;
    else pad.buttons &= ~mask;
    pad.lastInput = std::chrono::steady_clock::now();
}
extern "C" void MclgAndroidInputMove(float x, float y) {
    std::lock_guard<std::mutex> lock(g_inputMutex);
    AndroidPadState& pad = g_pads[0];
    pad.lx = clampAxis(x);
    pad.ly = clampAxis(y);
    pad.lastInput = std::chrono::steady_clock::now();
}
extern "C" void MclgAndroidInputLook(float dx, float dy) {
    std::lock_guard<std::mutex> lock(g_inputMutex);
    AndroidPadState& pad = g_pads[0];
    pad.rx = clampAxis(dx * pad.sensitivity);
    pad.ry = clampAxis(dy * pad.sensitivity);
    pad.lastInput = std::chrono::steady_clock::now();
}
extern "C" void MclgAndroidInputTrigger(int right, int value) {
    std::lock_guard<std::mutex> lock(g_inputMutex);
    AndroidPadState& pad = g_pads[0];
    unsigned char trigger = static_cast<unsigned char>(std::max(0, std::min(255, value)));
    if (right) pad.rightTrigger = trigger;
    else pad.leftTrigger = trigger;
    pad.lastInput = std::chrono::steady_clock::now();
}
