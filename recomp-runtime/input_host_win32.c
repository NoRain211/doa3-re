#include "input_host_win32.h"
#include "input_keyboard_win32.h"
#include "input_pulse_source.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
#include <SDL3/SDL.h>

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

enum {
    XBOX_DPAD_UP = 0x0001u,
    XBOX_DPAD_DOWN = 0x0002u,
    XBOX_DPAD_LEFT = 0x0004u,
    XBOX_DPAD_RIGHT = 0x0008u,
    XBOX_START = 0x0010u,
    XBOX_BACK = 0x0020u,
    XBOX_LEFT_THUMB = 0x0040u,
    XBOX_RIGHT_THUMB = 0x0080u,
    /* The game resends motors every frame; a stalled game stops rumbling. */
    RUMBLE_DURATION_MS = 1000u,
};

static SDL_Gamepad *pads[RECOMP_INPUT_PORT_COUNT];

#ifdef _WIN32
#define KEY(virtual_key, scancode) (virtual_key)
#else
#define KEY(virtual_key, scancode) (scancode)
#endif

#ifdef RECOMP_INPUT_TEST
static bool test_keys[RECOMP_INPUT_KEY_COUNT];
#endif

#ifdef _WIN32
static bool pressed(int key)
{
#ifdef RECOMP_INPUT_TEST
    return key >= 0 && key < RECOMP_INPUT_KEY_COUNT && test_keys[key];
#else
    return (GetAsyncKeyState(key) & 0x8000) != 0;
#endif
}

static bool focused(void)
{
    DWORD foreground_process = 0;
    GetWindowThreadProcessId(GetForegroundWindow(), &foreground_process);
    return foreground_process == GetCurrentProcessId();
}

static unsigned long long tick_ms(void)
{
    return GetTickCount64();
}
#else
/* SDL tracks the keyboard from the events the presenter pumps each frame. */
static bool pressed(int key)
{
#ifdef RECOMP_INPUT_TEST
    return key >= 0 && key < RECOMP_INPUT_KEY_COUNT && test_keys[key];
#else
    const bool *keyboard = SDL_GetKeyboardState(NULL);
    return keyboard != NULL && keyboard[key];
#endif
}

static bool focused(void)
{
    return SDL_GetKeyboardFocus() != NULL;
}

static unsigned long long tick_ms(void)
{
    return SDL_GetTicks();
}
#endif

#ifdef RECOMP_INPUT_TEST
/* The test executable has no window; its keys stand in for the real ones. */
void recomp_input_host_test_key(uint32_t virtual_key, uint32_t scancode, bool down)
{
    const int key = KEY(virtual_key, scancode);
    if (key >= 0 && key < RECOMP_INPUT_KEY_COUNT) test_keys[key] = down;
}
#endif

static bool trace_enabled(void)
{
    static int enabled = -1;
    if (enabled < 0) enabled = getenv("RECOMP_INPUT_TRACE") != NULL;
    return enabled != 0;
}

#ifndef _WIN32
/* RECOMP_KEYMAP holds Windows virtual keys; SDL reads scancodes. These are
   the keys run_game.py lets a player bind. */
static uint8_t scancode_from_vk(unsigned long vk)
{
    if (vk >= 'A' && vk <= 'Z') return (uint8_t)(SDL_SCANCODE_A + (vk - 'A'));
    if (vk >= '1' && vk <= '9') return (uint8_t)(SDL_SCANCODE_1 + (vk - '1'));
    if (vk == '0') return SDL_SCANCODE_0;
    if (vk >= 0x70u && vk <= 0x7bu && vk != 0x79u && vk != 0x7au)
        return (uint8_t)(SDL_SCANCODE_F1 + (vk - 0x70u));
    switch (vk) {
    case 0x08u: return SDL_SCANCODE_BACKSPACE;
    case 0x09u: return SDL_SCANCODE_TAB;
    case 0x0du: return SDL_SCANCODE_RETURN;
    case 0x10u: return SDL_SCANCODE_LSHIFT;
    case 0x11u: return SDL_SCANCODE_LCTRL;
    case 0x1bu: return SDL_SCANCODE_ESCAPE;
    case 0x20u: return SDL_SCANCODE_SPACE;
    case 0x25u: return SDL_SCANCODE_LEFT;
    case 0x26u: return SDL_SCANCODE_UP;
    case 0x27u: return SDL_SCANCODE_RIGHT;
    case 0x28u: return SDL_SCANCODE_DOWN;
    default: return 0u;
    }
}
#endif

static uint8_t key_from_vk(unsigned long vk)
{
#ifdef _WIN32
    return vk > 0ul && vk < RECOMP_INPUT_KEY_COUNT ? (uint8_t)vk : 0u;
#else
    return scancode_from_vk(vk);
#endif
}

/* The keys that press pad buttons for port 0. RECOMP_KEYMAP="name=hexVK,..."
   (built by run_game.py from private/keyboard.json) replaces the defaults. */
static struct { const char *name; uint8_t key; uint16_t button; int analog; } keymap[] = {
    {"up", KEY(VK_UP, SDL_SCANCODE_UP), XBOX_DPAD_UP, -1},
    {"down", KEY(VK_DOWN, SDL_SCANCODE_DOWN), XBOX_DPAD_DOWN, -1},
    {"left", KEY(VK_LEFT, SDL_SCANCODE_LEFT), XBOX_DPAD_LEFT, -1},
    {"right", KEY(VK_RIGHT, SDL_SCANCODE_RIGHT), XBOX_DPAD_RIGHT, -1},
    {"start", KEY(VK_RETURN, SDL_SCANCODE_RETURN), XBOX_START, -1},
    {"back", KEY(VK_BACK, SDL_SCANCODE_BACKSPACE), XBOX_BACK, -1},
    {"a", KEY('Z', SDL_SCANCODE_Z), 0, RECOMP_INPUT_ANALOG_A},
    {"b", KEY('X', SDL_SCANCODE_X), 0, RECOMP_INPUT_ANALOG_B},
    {"x", KEY('A', SDL_SCANCODE_A), 0, RECOMP_INPUT_ANALOG_X},
    {"y", KEY('S', SDL_SCANCODE_S), 0, RECOMP_INPUT_ANALOG_Y},
    {"white", KEY('Q', SDL_SCANCODE_Q), 0, RECOMP_INPUT_ANALOG_WHITE},
    {"black", KEY('W', SDL_SCANCODE_W), 0, RECOMP_INPUT_ANALOG_BLACK},
    {"ltrig", KEY('E', SDL_SCANCODE_E), 0, RECOMP_INPUT_ANALOG_LTRIG},
    {"rtrig", KEY('R', SDL_SCANCODE_R), 0, RECOMP_INPUT_ANALOG_RTRIG},
};

/* A bound Shift or Ctrl accepts either side, as the camera keys do. */
static bool pressed_binding(uint8_t key)
{
#ifdef _WIN32
    return pressed(key);
#else
    if (key == SDL_SCANCODE_LSHIFT) return pressed(SDL_SCANCODE_LSHIFT) || pressed(SDL_SCANCODE_RSHIFT);
    if (key == SDL_SCANCODE_LCTRL) return pressed(SDL_SCANCODE_LCTRL) || pressed(SDL_SCANCODE_RCTRL);
    return pressed(key);
#endif
}

static void load_keymap(void)
{
    static bool loaded;
    bool bound[sizeof keymap / sizeof keymap[0]] = {false};
    if (loaded) return;
    loaded = true;
    const char *at = getenv("RECOMP_KEYMAP");
    while (at != NULL && *at != '\0') {
        const char *next = strchr(at, ',');
        const char *equals = strchr(at, '=');
        if (equals != NULL && (next == NULL || equals < next)) { /* an entry without = is skipped */
            const uint8_t key = key_from_vk(strtoul(equals + 1, NULL, 16));
            for (size_t i = 0; i < sizeof keymap / sizeof keymap[0]; ++i) {
                if (key != 0u && strlen(keymap[i].name) == (size_t)(equals - at) &&
                    strncmp(at, keymap[i].name, equals - at) == 0) {
                    keymap[i].key = key;
                    bound[i] = true;
                }
            }
        }
        at = next != NULL ? next + 1 : NULL;
    }
    /* A key given to one button leaves the button that had it by default. */
    for (size_t i = 0; i < sizeof keymap / sizeof keymap[0]; ++i)
        for (size_t j = 0; j < sizeof keymap / sizeof keymap[0]; ++j)
            if (!bound[i] && bound[j] && keymap[i].key == keymap[j].key) keymap[i].key = 0u;
}

/* Only the test target defines RECOMP_INPUT_TEST, so RECOMP_TEST_FOCUSED
   never takes the focus check away in the game. */
static bool test_focused(void)
{
#ifdef RECOMP_INPUT_TEST
    static int set = -1;
    if (set < 0) set = getenv("RECOMP_TEST_FOCUSED") != NULL;
    return set != 0;
#else
    return false;
#endif
}

static RecompInputGamepad keyboard_buttons; /* what the keymap pressed in port 0's last sample */

void recomp_input_host_keyboard_buttons(RecompInputGamepad *pressed_buttons)
{
    *pressed_buttons = keyboard_buttons;
}

/* SDL starts lazily on the guest thread that polls it. There is no SDL event
   loop; SDL_UpdateGamepads pumps it. A new gamepad takes the lowest free port
   and keeps it until it disconnects. */
static void update_pads(void)
{
    static bool initialized;

    if (!initialized) {
        initialized = true;
        SDL_SetHint(SDL_HINT_NO_SIGNAL_HANDLERS, "1");
        SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
        if (!SDL_Init(SDL_INIT_GAMEPAD)) {
            fprintf(stderr, "recomp input: SDL gamepad init failed: %s\n", SDL_GetError());
        }
        SDL_SetJoystickEventsEnabled(false);
        SDL_SetGamepadEventsEnabled(false);
    }
    SDL_UpdateGamepads();
    for (uint32_t port = 0u; port < RECOMP_INPUT_PORT_COUNT; ++port) {
        if (pads[port] != NULL && !SDL_GamepadConnected(pads[port])) {
            SDL_CloseGamepad(pads[port]);
            pads[port] = NULL;
        }
    }
    int count = 0;
    SDL_JoystickID *ids = SDL_GetGamepads(&count);
    for (int i = 0; ids != NULL && i < count; ++i) {
        uint32_t free_port = RECOMP_INPUT_PORT_COUNT;
        bool open = false;
        for (uint32_t port = 0u; port < RECOMP_INPUT_PORT_COUNT; ++port) {
            if (pads[port] == NULL) {
                if (free_port == RECOMP_INPUT_PORT_COUNT) free_port = port;
            } else if (SDL_GetGamepadID(pads[port]) == ids[i]) {
                open = true;
            }
        }
        if (!open && free_port < RECOMP_INPUT_PORT_COUNT) {
            pads[free_port] = SDL_OpenGamepad(ids[i]);
            if (pads[free_port] != NULL) {
                fprintf(stderr, "recomp input: gamepad '%s' on port=%u\n",
                    SDL_GetGamepadName(pads[free_port]), (unsigned)free_port);
            }
        }
    }
    SDL_free(ids);
}

void recomp_input_host_set_vibration(
    uint32_t port,
    uint16_t left_motor,
    uint16_t right_motor)
{
    static uint32_t traced[RECOMP_INPUT_PORT_COUNT];
    uint32_t motors = (uint32_t)left_motor << 16 | right_motor;

    if (port >= RECOMP_INPUT_PORT_COUNT || pads[port] == NULL) {
        return;
    }
    /* SDL sends only changed values and keeps a held value alive. */
    bool sent = SDL_RumbleGamepad(pads[port], left_motor, right_motor, RUMBLE_DURATION_MS);
    if (trace_enabled() && traced[port] != motors) {
        traced[port] = motors;
        fprintf(stderr, "recomp input: vibration port=%u left=%u right=%u sent=%u\n",
            (unsigned)port, (unsigned)left_motor, (unsigned)right_motor, (unsigned)sent);
    }
}

void recomp_input_host_stop_vibration(void)
{
    for (uint32_t port = 0u; port < RECOMP_INPUT_PORT_COUNT; ++port) {
        recomp_input_host_set_vibration(port, 0u, 0u);
    }
}

static void trace_sample(
    const RecompInputGamepad *pad, SDL_Gamepad *host, bool is_focused)
{
    static bool seen;
    static unsigned lines;
    static RecompInputGamepad previous;
    static SDL_JoystickID previous_id;
    static bool previous_focused;
    if (!trace_enabled() || lines >= 4096u) return;
    SDL_JoystickID id = host != NULL ? SDL_GetGamepadID(host) : 0;
    if (seen && memcmp(&previous, pad, sizeof previous) == 0 &&
        previous_id == id && previous_focused == is_focused) return;
    const char *host_name = host != NULL ? SDL_GetGamepadName(host) : NULL;
    fprintf(stderr,
        "[DEBUG-r501-input] tick_ms=%llu pad='%s' focused=%u"
        " digital=%04x analog=%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x"
        " axes=%d,%d,%d,%d\n",
        tick_ms(), host_name != NULL ? host_name : "none",
        is_focused ? 1u : 0u,
        (unsigned)pad->buttons, pad->analog_buttons[0], pad->analog_buttons[1],
        pad->analog_buttons[2], pad->analog_buttons[3], pad->analog_buttons[4],
        pad->analog_buttons[5], pad->analog_buttons[6], pad->analog_buttons[7],
        pad->thumb_lx, pad->thumb_ly, pad->thumb_rx, pad->thumb_ry);
    previous = *pad;
    previous_id = id;
    previous_focused = is_focused;
    seen = true;
    ++lines;
}

/* Host face buttons are digital, but Xbox ones report pressure, and games in
   analog-button mode read a full press as a hard hit. Report a light press
   instead: 0x20 is DOA3's pressed threshold, well below its 0x80 hard-press
   threshold. RECOMP_INPUT_BUTTON_PRESSURE (1-255) overrides it. */
static uint8_t pressed_pressure(void)
{
    static int pressure = -1;
    if (pressure < 0) {
        const char *text = getenv("RECOMP_INPUT_BUTTON_PRESSURE");
        long value = text != NULL ? strtol(text, NULL, 0) : 0;
        pressure = value >= 1 && value <= 255 ? (int)value : 0x20;
    }
    return (uint8_t)pressure;
}

static uint8_t button_pressure(SDL_Gamepad *gamepad, SDL_GamepadButton button)
{
    return SDL_GetGamepadButton(gamepad, button) ? pressed_pressure() : 0u;
}

/* SDL Y axes point down; XInput Y axes point up. */
static int16_t up_axis(SDL_Gamepad *gamepad, SDL_GamepadAxis axis)
{
    int16_t down = SDL_GetGamepadAxis(gamepad, axis);
    return down == INT16_MIN ? INT16_MAX : (int16_t)-down;
}

static void read_pad(SDL_Gamepad *host, RecompInputGamepad *gamepad)
{
    static const struct { SDL_GamepadButton button; uint16_t mask; } digital[] = {
        {SDL_GAMEPAD_BUTTON_DPAD_UP, XBOX_DPAD_UP},
        {SDL_GAMEPAD_BUTTON_DPAD_DOWN, XBOX_DPAD_DOWN},
        {SDL_GAMEPAD_BUTTON_DPAD_LEFT, XBOX_DPAD_LEFT},
        {SDL_GAMEPAD_BUTTON_DPAD_RIGHT, XBOX_DPAD_RIGHT},
        {SDL_GAMEPAD_BUTTON_START, XBOX_START},
        {SDL_GAMEPAD_BUTTON_BACK, XBOX_BACK},
        {SDL_GAMEPAD_BUTTON_LEFT_STICK, XBOX_LEFT_THUMB},
        {SDL_GAMEPAD_BUTTON_RIGHT_STICK, XBOX_RIGHT_THUMB},
    };
    for (size_t i = 0; i < sizeof digital / sizeof digital[0]; ++i) {
        if (SDL_GetGamepadButton(host, digital[i].button)) {
            gamepad->buttons |= digital[i].mask;
        }
    }
    /* SDL names face buttons by position, so Cross/Circle/Square/Triangle
       land on the Xbox A/B/X/Y slots. */
    gamepad->analog_buttons[RECOMP_INPUT_ANALOG_A] = button_pressure(host, SDL_GAMEPAD_BUTTON_SOUTH);
    gamepad->analog_buttons[RECOMP_INPUT_ANALOG_B] = button_pressure(host, SDL_GAMEPAD_BUTTON_EAST);
    gamepad->analog_buttons[RECOMP_INPUT_ANALOG_X] = button_pressure(host, SDL_GAMEPAD_BUTTON_WEST);
    gamepad->analog_buttons[RECOMP_INPUT_ANALOG_Y] = button_pressure(host, SDL_GAMEPAD_BUTTON_NORTH);
    gamepad->analog_buttons[RECOMP_INPUT_ANALOG_BLACK] =
        button_pressure(host, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER);
    gamepad->analog_buttons[RECOMP_INPUT_ANALOG_WHITE] =
        button_pressure(host, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER);
    /* SDL triggers are 0..32767; the guest expects 0..255. */
    gamepad->analog_buttons[RECOMP_INPUT_ANALOG_LTRIG] =
        (uint8_t)(SDL_GetGamepadAxis(host, SDL_GAMEPAD_AXIS_LEFT_TRIGGER) >> 7);
    gamepad->analog_buttons[RECOMP_INPUT_ANALOG_RTRIG] =
        (uint8_t)(SDL_GetGamepadAxis(host, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) >> 7);
    gamepad->thumb_lx = SDL_GetGamepadAxis(host, SDL_GAMEPAD_AXIS_LEFTX);
    gamepad->thumb_ly = up_axis(host, SDL_GAMEPAD_AXIS_LEFTY);
    gamepad->thumb_rx = SDL_GetGamepadAxis(host, SDL_GAMEPAD_AXIS_RIGHTX);
    gamepad->thumb_ry = up_axis(host, SDL_GAMEPAD_AXIS_RIGHTY);
}

static RecompInputComboHook combo_hook;

void recomp_input_host_set_combo_hook(RecompInputComboHook hook)
{
    combo_hook = hook;
}

static RecompInputCaptureHook capture_hook;

void recomp_input_host_set_capture_hook(RecompInputCaptureHook hook)
{
    capture_hook = hook;
}

static bool sample(uint32_t port, RecompInputGamepad *gamepad, uint16_t *stick_clicks);

bool recomp_input_host_sample(uint32_t port, RecompInputGamepad *gamepad)
{
    uint16_t stick_clicks = 0u;
    const bool present = sample(port, gamepad, &stick_clicks);
    if (present && capture_hook != NULL) capture_hook(port, gamepad, stick_clicks);
    return present;
}

static bool sample(uint32_t port, RecompInputGamepad *gamepad, uint16_t *stick_clicks)
{
    if (gamepad == NULL || port >= RECOMP_INPUT_PORT_COUNT) {
        return false;
    }
    memset(gamepad, 0, sizeof *gamepad);
    update_pads();
    SDL_Gamepad *host = pads[port];
    if (host != NULL) {
        read_pad(host, gamepad);
        *stick_clicks = gamepad->buttons & (XBOX_LEFT_THUMB | XBOX_RIGHT_THUMB);
        /* The stick clicks are the combo's own: the game never sees them. */
        const bool combo = combo_hook != NULL && combo_hook(port, gamepad);
        if (combo_hook != NULL) gamepad->buttons &= (uint16_t)~(XBOX_LEFT_THUMB | XBOX_RIGHT_THUMB);
        if (combo) {
            gamepad->analog_buttons[RECOMP_INPUT_ANALOG_BLACK] = 0u;
            gamepad->analog_buttons[RECOMP_INPUT_ANALOG_WHITE] = 0u;
            gamepad->analog_buttons[RECOMP_INPUT_ANALOG_LTRIG] = 0u;
            gamepad->analog_buttons[RECOMP_INPUT_ANALOG_RTRIG] = 0u;
            gamepad->analog_buttons[RECOMP_INPUT_ANALOG_X] = 0u;
            gamepad->analog_buttons[RECOMP_INPUT_ANALOG_Y] = 0u;
            gamepad->analog_buttons[RECOMP_INPUT_ANALOG_A] = 0u;
            gamepad->analog_buttons[RECOMP_INPUT_ANALOG_B] = 0u;
            /* Nor the d-pad, Start and the sticks while the combo is held. */
            gamepad->buttons &= (uint16_t)~(XBOX_DPAD_UP | XBOX_DPAD_DOWN | XBOX_DPAD_LEFT | XBOX_DPAD_RIGHT |
                XBOX_START);
            gamepad->thumb_lx = gamepad->thumb_ly = gamepad->thumb_rx = gamepad->thumb_ry = 0;
        }
    }
    if (port != 0u) {
        /* The keyboard drives port 0 only. */
        return host != NULL;
    }
    memset(&keyboard_buttons, 0, sizeof keyboard_buttons);
    if (!focused() && !test_focused()) {
        trace_sample(gamepad, host, false);
        return true;
    }

    load_keymap();
    for (size_t i = 0; i < sizeof keymap / sizeof keymap[0]; ++i) {
        if (keymap[i].key == 0u || !pressed_binding(keymap[i].key)) continue;
        if (keymap[i].button != 0u) keyboard_buttons.buttons |= keymap[i].button;
        else if (keymap[i].analog == RECOMP_INPUT_ANALOG_LTRIG || keymap[i].analog == RECOMP_INPUT_ANALOG_RTRIG)
            keyboard_buttons.analog_buttons[keymap[i].analog] = 0xffu;
        else keyboard_buttons.analog_buttons[keymap[i].analog] = pressed_pressure();
    }
    gamepad->buttons |= keyboard_buttons.buttons;
    for (unsigned i = 0; i < RECOMP_INPUT_ANALOG_BUTTON_COUNT; ++i)
        gamepad->analog_buttons[i] |= keyboard_buttons.analog_buttons[i];
    trace_sample(gamepad, host, true);
    return true;
}

bool recomp_input_host_key(RecompInputHostKey key)
{
    if (!focused()) return false;
    switch (key) {
    case RECOMP_INPUT_HOST_KEY_F1: return pressed(KEY(VK_F1, SDL_SCANCODE_F1));
    case RECOMP_INPUT_HOST_KEY_GRAVE: return pressed(KEY(VK_OEM_3, SDL_SCANCODE_GRAVE));
    case RECOMP_INPUT_HOST_KEY_F12: return pressed(KEY(VK_F12, SDL_SCANCODE_F12));
    case RECOMP_INPUT_HOST_KEY_W: return pressed(KEY('W', SDL_SCANCODE_W));
    case RECOMP_INPUT_HOST_KEY_A: return pressed(KEY('A', SDL_SCANCODE_A));
    case RECOMP_INPUT_HOST_KEY_S: return pressed(KEY('S', SDL_SCANCODE_S));
    case RECOMP_INPUT_HOST_KEY_D: return pressed(KEY('D', SDL_SCANCODE_D));
    case RECOMP_INPUT_HOST_KEY_Q: return pressed(KEY('Q', SDL_SCANCODE_Q));
    case RECOMP_INPUT_HOST_KEY_E: return pressed(KEY('E', SDL_SCANCODE_E));
    case RECOMP_INPUT_HOST_KEY_R: return pressed(KEY('R', SDL_SCANCODE_R));
    case RECOMP_INPUT_HOST_KEY_Z: return pressed(KEY('Z', SDL_SCANCODE_Z));
    case RECOMP_INPUT_HOST_KEY_X: return pressed(KEY('X', SDL_SCANCODE_X));
    case RECOMP_INPUT_HOST_KEY_C: return pressed(KEY('C', SDL_SCANCODE_C));
    case RECOMP_INPUT_HOST_KEY_P: return pressed(KEY('P', SDL_SCANCODE_P));
    case RECOMP_INPUT_HOST_KEY_H: return pressed(KEY('H', SDL_SCANCODE_H));
    case RECOMP_INPUT_HOST_KEY_PERIOD: return pressed(KEY(VK_OEM_PERIOD, SDL_SCANCODE_PERIOD));
    case RECOMP_INPUT_HOST_KEY_BACKSLASH: return pressed(KEY(VK_OEM_5, SDL_SCANCODE_BACKSLASH));
    case RECOMP_INPUT_HOST_KEY_MINUS: return pressed(KEY(VK_OEM_MINUS, SDL_SCANCODE_MINUS));
    case RECOMP_INPUT_HOST_KEY_EQUALS: return pressed(KEY(VK_OEM_PLUS, SDL_SCANCODE_EQUALS));
    case RECOMP_INPUT_HOST_KEY_UP: return pressed(KEY(VK_UP, SDL_SCANCODE_UP));
    case RECOMP_INPUT_HOST_KEY_DOWN: return pressed(KEY(VK_DOWN, SDL_SCANCODE_DOWN));
    case RECOMP_INPUT_HOST_KEY_LEFT: return pressed(KEY(VK_LEFT, SDL_SCANCODE_LEFT));
    case RECOMP_INPUT_HOST_KEY_RIGHT: return pressed(KEY(VK_RIGHT, SDL_SCANCODE_RIGHT));
    case RECOMP_INPUT_HOST_KEY_RETURN: return pressed(KEY(VK_RETURN, SDL_SCANCODE_RETURN));
    case RECOMP_INPUT_HOST_KEY_BACKSPACE: return pressed(KEY(VK_BACK, SDL_SCANCODE_BACKSPACE));
    case RECOMP_INPUT_HOST_KEY_SHIFT:
        return pressed(KEY(VK_LSHIFT, SDL_SCANCODE_LSHIFT)) || pressed(KEY(VK_RSHIFT, SDL_SCANCODE_RSHIFT));
    case RECOMP_INPUT_HOST_KEY_CONTROL:
        return pressed(KEY(VK_LCONTROL, SDL_SCANCODE_LCTRL)) || pressed(KEY(VK_RCONTROL, SDL_SCANCODE_RCTRL));
    case RECOMP_INPUT_HOST_KEY_COUNT: break;
    }
    return false;
}

bool recomp_input_host_mouse_drag(float *dx, float *dy)
{
    static bool dragging;
    static float last_x, last_y;
    float x = 0.0f, y = 0.0f;
    bool held = false;

    *dx = *dy = 0.0f;
    if (focused()) {
#ifdef _WIN32
        /* GetAsyncKeyState reads physical buttons; swapped buttons put the
           logical right one on the left. */
        const int right = GetSystemMetrics(SM_SWAPBUTTON) ? VK_LBUTTON : VK_RBUTTON;
        POINT point = {0, 0};
        held = pressed(right) && GetCursorPos(&point);
        if (held && !dragging) {
            /* A drag starts only over the game's client area. */
            HWND window = GetForegroundWindow();
            POINT client = point;
            RECT area;
            held = ScreenToClient(window, &client) && GetClientRect(window, &area) &&
                PtInRect(&area, client);
        }
        x = (float)point.x;
        y = (float)point.y;
#else
        held = (SDL_GetMouseState(&x, &y) & SDL_BUTTON_RMASK) != 0u;
#endif
    }
    if (held && dragging) {
        *dx = x - last_x;
        *dy = y - last_y;
    }
    dragging = held;
    last_x = x;
    last_y = y;
    return held;
}
