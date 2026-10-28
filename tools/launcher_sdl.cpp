// SPDX-License-Identifier: GPL-3.0-or-later
// Linux launcher: pick resolution, anti-aliasing, volume, widescreen and the
// DOAXBV swimsuit switch (its disc: private/doaxbv-path.txt or an .iso there),
// remember them in private/launcher.json, then exit 0 so run_game.py starts
// the game. Mirrors tools/launcher.cs without SMAA (Direct3D 11 only) or the
// build page: a page menu on the left over tools/launcher-background.png, the
// chosen page in a panel on the right. Gamepad, keyboard, mouse and touch all
// work, so it runs from Steam Game Mode. Exits 1 when closed without Play.
#include <SDL3/SDL.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace {

// Everything is laid out on a 640x400 canvas and scaled by whole numbers,
// so SDL's 8x8 debug font stays sharp: 2x on the Steam Deck's 1280x800.
constexpr int kWidth = 640, kHeight = 400;
constexpr float kMenuTop = 96, kMenuStep = 30, kItemH = 22, kMenuW = 200;
constexpr float kPanelX = 230, kPanelR = 616, kPanelTop = 104, kRowTop = 180, kStep = 26, kRowH = 22;
constexpr float kValueX = 480, kValueW = 120;

const int kHeights[] = {480, 720, 1080, 1440, 2160};
const int kSamples[] = {1, 2, 4, 8};
const int kVolumes[] = {100, 75, 50, 25, 0};

enum Item { RESOLUTION, MSAA, VOLUME, WIDESCREEN, FREE_CAMERA, SWIMSUITS, START_FAVORITE, ITEM_COUNT };
const char *kLabels[] = {"RESOLUTION", "MSAA", "VOLUME", "SCREEN", "FREE CAMERA", "SWIMSUITS", "START IN FAVORITE"};

struct Page { const char *name, *info; std::vector<int> rows; };
const Page kPages[] = {
    {"VIDEO SETTINGS", "Sets resolution, anti-aliasing and screen shape", {RESOLUTION, MSAA, WIDESCREEN}},
    {"AUDIO SETTINGS", "Sets the master volume", {VOLUME}},
    {"FREE CAMERA", "In game, F1 or a right-stick click flies the camera; P freezes", {FREE_CAMERA}},
    {"SWIMSUITS", "DOAXBV swimsuits; needs your own DOAXBV disc: an .iso in private/ or doaxbv-path.txt", {SWIMSUITS, START_FAVORITE}},
    {"START GAME", "Saves these settings and starts the game", {}},
    {"EXIT", "Closes the launcher", {}},
};
constexpr int kPageCount = static_cast<int>(std::size(kPages)), kPlayPage = 4, kExitPage = 5;

struct Settings {
    int height = 0;
    int msaa = 1;
    int volume = 100;
    bool widescreen = false;
    bool free_camera = false;  // F1, ` or the right stick's click takes the camera
    bool swimsuits = true;   // DOAXBV swimsuits from private/doaxbv-path.txt or an .iso in private/
    bool start_favorite = false;
};

struct Color { Uint8 r, g, b; };
constexpr Color kDark{42, 29, 21}, kLight{216, 207, 198}, kWhite{243, 238, 232}, kSand{241, 197, 143};
constexpr Color kMuted{188, 174, 159}, kGold{232, 191, 138}, kShade{22, 16, 14};

// Reads the flat object launcher.cs, launcher_macos.swift and this file write.
bool jsonValue(const std::string &text, const char *key, std::string &value)
{
    const auto at = text.find(std::string("\"") + key + "\"");
    if (at == std::string::npos) return false;
    auto start = text.find(':', at);
    if (start == std::string::npos) return false;
    start = text.find_first_not_of(" \t\r\n", start + 1);
    if (start == std::string::npos) return false;
    value = text.substr(start, text.find_first_of(",}\r\n", start) - start);
    value.erase(value.find_last_not_of(" \t") + 1);
    return true;
}

Settings loadSettings(const std::string &path)
{
    Settings settings;
    std::ifstream file(path);
    const std::string text{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
    std::string value;
    if (jsonValue(text, "height", value)) settings.height = std::atoi(value.c_str());
    if (jsonValue(text, "msaa", value)) settings.msaa = std::atoi(value.c_str());
    if (jsonValue(text, "volume", value)) settings.volume = std::atoi(value.c_str());
    if (jsonValue(text, "widescreen", value)) settings.widescreen = value == "true";
    if (jsonValue(text, "free_camera", value)) settings.free_camera = value == "true";
    if (jsonValue(text, "swimsuits", value)) settings.swimsuits = value != "false";
    if (jsonValue(text, "swimsuit_start_favorite", value)) settings.start_favorite = value == "true";
    return settings;
}

bool saveSettings(const std::string &path, const Settings &settings)
{
    std::ofstream file(path, std::ios::trunc);
    file << "{\n  \"height\": " << settings.height << ",\n  \"msaa\": " << settings.msaa
         << ",\n  \"smaa\": false,\n  \"volume\": " << settings.volume
         << ",\n  \"widescreen\": " << (settings.widescreen ? "true" : "false")
         << ",\n  \"free_camera\": " << (settings.free_camera ? "true" : "false")
         << ",\n  \"swimsuits\": " << (settings.swimsuits ? "true" : "false")
         << ",\n  \"swimsuit_start_favorite\": " << (settings.start_favorite ? "true" : "false") << "\n}\n";
    return static_cast<bool>(file);
}

template <size_t N>
int indexOf(const int (&values)[N], int value, int fallback)
{
    const auto it = std::find(std::begin(values), std::end(values), value);
    return it == std::end(values) ? fallback : static_cast<int>(it - std::begin(values));
}

int hitMenu(float x, float y)
{
    const int m = static_cast<int>((y - kMenuTop) / kMenuStep);
    return x < kMenuW && y >= kMenuTop && m < kPageCount && y - kMenuTop - m * kMenuStep < kItemH ? m : -1;
}

int hitRow(float x, float y, int rows)
{
    const int i = static_cast<int>((y - kRowTop) / kStep);
    return x >= kPanelX && x < kPanelR && y >= kRowTop && i < rows && y - kRowTop - i * kStep < kRowH ? i : -1;
}

struct Launcher {
    SDL_Renderer *renderer = nullptr;
    SDL_Texture *background = nullptr;
    int choice[ITEM_COUNT] = {};
    // The menu cursor picks a page; inside a page the row cursor picks a setting.
    int menu = kPlayPage, row = 0;
    bool in_page = false;
    std::string installed = "INSTALLED: USA 3.0"; // a build older than private/build.json

    const std::vector<int> &rows() const { return kPages[menu].rows; }

    std::string valueText(int item) const
    {
        switch (item) {
        case RESOLUTION: return std::to_string(kHeights[choice[item]]) + "P";
        case MSAA: return kSamples[choice[item]] == 1 ? "OFF" : std::to_string(kSamples[choice[item]]) + "X";
        case VOLUME: return kVolumes[choice[item]] == 0 ? "MUTE" : std::to_string(kVolumes[choice[item]]) + "%";
        case WIDESCREEN: return choice[item] ? "16:9" : "4:3";
        default: return choice[item] ? "ON" : "OFF";
        }
    }

    void change(int item, int step)
    {
        static const int counts[] = {static_cast<int>(std::size(kHeights)),
            static_cast<int>(std::size(kSamples)), static_cast<int>(std::size(kVolumes)), 2, 2, 2, 2};
        choice[item] = (choice[item] + counts[item] + step) % counts[item];
    }

    // Returns the exit code once the player picks Start Game (0) or Exit (1), else -1.
    int accept()
    {
        if (in_page) { change(rows()[row], 1); return -1; }
        if (menu == kPlayPage) return 0;
        if (menu == kExitPage) return 1;
        in_page = true;
        row = 0;
        return -1;
    }

    int back()
    {
        if (!in_page) return 1;
        in_page = false;
        return -1;
    }

    void move(int dy, int dx)
    {
        if (in_page) {
            row = std::clamp(row + dy, 0, static_cast<int>(rows().size()) - 1);
            if (dx != 0) change(rows()[row], dx);
        } else {
            menu = std::clamp(menu + dy, 0, kPageCount - 1);
            if (dx > 0 && !rows().empty()) accept();
        }
    }

    void fill(const SDL_FRect &rect, Color color, Uint8 alpha = 255)
    {
        SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, alpha);
        SDL_RenderFillRect(renderer, &rect);
    }

    // scale 2 draws 16x16 glyphs; coordinates stay on the 640x400 canvas.
    void text(float x, float y, const std::string &value, Color color, float scale = 1)
    {
        SDL_SetRenderScale(renderer, scale, scale);
        SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, 255);
        SDL_RenderDebugText(renderer, x / scale, y / scale, value.c_str());
        SDL_SetRenderScale(renderer, 1, 1);
    }

    // Silver text over a dark drop shadow, as on the title logo.
    void chrome(float x, float y, const std::string &value, float scale)
    {
        text(x + scale, y + scale, value, kDark, scale);
        text(x, y, value, {214, 216, 217}, scale);
    }

    // Wraps at spaces into lines of at most width pixels.
    void wrapped(float x, float y, const std::string &value, float width, Color color)
    {
        const size_t per_line = static_cast<size_t>(width / 8);
        size_t start = 0;
        while (start < value.size()) {
            size_t end = std::min(value.size(), start + per_line);
            if (end < value.size()) {
                const size_t space = value.rfind(' ', end);
                if (space != std::string::npos && space > start) end = space;
            }
            text(x, y, value.substr(start, end - start), color);
            start = value.find_first_not_of(' ', end);
            y += 12;
        }
    }

    void draw()
    {
        SDL_SetRenderDrawColor(renderer, 74, 60, 51, 255);
        SDL_RenderClear(renderer);
        if (background != nullptr) SDL_RenderTexture(renderer, background, nullptr, nullptr);
        // Darken the beach behind the menu: strong at the left edge, gone by two thirds across.
        for (int x = 0; x < kWidth * 2 / 3; x += 2) {
            const float t = x / static_cast<float>(kWidth);
            const float a = t < .28f ? 217 - t / .28f * 64 : t < .52f ? 153 - (t - .28f) / .24f * 128 : 25 - (t - .52f) / .14f * 25;
            fill({static_cast<float>(x), 0, 2, kHeight}, kShade, static_cast<Uint8>(std::max(0.f, a)));
        }

        // Logo, top right.
        chrome(kWidth - 24 - 32 - 14 - 13 * 16, 34, "DEAD OR ALIVE", 2);
        chrome(kWidth - 24 - 32, 18, "3", 4);

        // Menu down the left; the current page sits on a slanted sunset bar.
        for (int m = 0; m < kPageCount; ++m) {
            const float y = kMenuTop + m * kMenuStep;
            if (m == menu) {
                const SDL_FColor light{246 / 255.f, 223 / 255.f, 189 / 255.f, 1}, deep{185 / 255.f, 134 / 255.f, 87 / 255.f, 1};
                const SDL_Vertex bar[] = {{{0, y}, light, {}}, {{kMenuW, y}, deep, {}},
                    {{kMenuW - 10, y + kItemH}, deep, {}}, {{0, y + kItemH}, light, {}}};
                const int corners[] = {0, 1, 2, 0, 2, 3};
                SDL_RenderGeometry(renderer, nullptr, bar, 4, corners, 6);
            }
            text(24, y + (kItemH - 8) / 2, kPages[m].name, m == menu ? kDark : kLight);
        }
        text(24, kHeight - 24, installed, kMuted);

        // The chosen page in a panel over the beach. From the menu its rows show as a preview.
        const Page &page = kPages[menu];
        const float bottom = kRowTop + std::max<size_t>(page.rows.size(), 1) * kStep + 8;
        fill({kPanelX, kPanelTop, kPanelR - kPanelX, bottom - kPanelTop}, {25, 18, 15}, 170);
        fill({kPanelX, kPanelTop, 3, bottom - kPanelTop}, kGold);
        chrome(kPanelX + 16, kPanelTop + 14, page.name, 2);
        wrapped(kPanelX + 16, kPanelTop + 40, page.info, kPanelR - kPanelX - 32, kLight);
        if (menu == kPlayPage) chrome(kPanelX + (kPanelR - kPanelX - 18 * 16) / 2, kRowTop + 3, "PRESS START BUTTON", 2);
        for (size_t i = 0; i < page.rows.size(); ++i) {
            const int item = page.rows[i];
            const float y = kRowTop + i * kStep;
            const bool selected = in_page && static_cast<int>(i) == row;
            if (i > 0) fill({kPanelX + 16, y - 3, kPanelR - kPanelX - 32, 1}, kWhite, 30);
            if (selected) fill({kPanelX + 3, y, kPanelR - kPanelX - 3, kRowH}, kGold, 60);
            const float ty = y + (kRowH - 8) / 2;
            text(kPanelX + 16, ty, kLabels[item], selected ? kWhite : kLight);
            const std::string value = valueText(item);
            text(kValueX, ty, "<", kSand);
            text(kValueX + kValueW - 8, ty, ">", kSand);
            text(kValueX + (kValueW - value.size() * 8) / 2, ty, value, kWhite);
        }

        // Button prompts, bottom right.
        float x = kWidth - 24;
        for (const auto &hint : {std::pair<const char *, const char *>{"<>", "CHANGE"}, {"B", "BACK"}, {"A", "SELECT"}}) {
            x -= SDL_strlen(hint.second) * 8;
            text(x, kHeight - 24, hint.second, kWhite);
            const float w = static_cast<float>(SDL_strlen(hint.first) * 8 + 8);
            x -= w + 6;
            fill({x, kHeight - 26, w, 12}, {242, 227, 207});
            text(x + 4, kHeight - 24, hint.first, kDark);
            x -= 16;
        }
        SDL_RenderPresent(renderer);
    }
};

// Direction from every open gamepad's d-pad and left stick.
void heldDirection(int &dx, int &dy)
{
    dx = dy = 0;
    int count = 0;
    SDL_JoystickID *ids = SDL_GetGamepads(&count);
    for (int i = 0; ids != nullptr && i < count; ++i) {
        SDL_Gamepad *pad = SDL_GetGamepadFromID(ids[i]);
        if (pad == nullptr) continue;
        const int x = SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFTX);
        const int y = SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFTY);
        if (SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_DPAD_UP) || y < -20000) dy = -1;
        if (SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_DPAD_DOWN) || y > 20000) dy = 1;
        if (SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_DPAD_LEFT) || x < -20000) dx = -1;
        if (SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_DPAD_RIGHT) || x > 20000) dx = 1;
    }
    SDL_free(ids);
    if (dy != 0) dx = 0;  // a diagonal moves a row without also changing a value
}

} // namespace

int main(int argc, char **argv)
{
    const std::string root = argc > 1 ? argv[1] : ".";
    const std::string settings_path = root + "/private/launcher.json";
    Settings settings = loadSettings(settings_path);

    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) {
        std::fprintf(stderr, "launcher: SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }
    if (settings.height == 0) {
        const SDL_DisplayMode *mode = SDL_GetCurrentDisplayMode(SDL_GetPrimaryDisplay());
        settings.height = mode != nullptr ? mode->h : 720;
    }

    // Steam sets these in Game Mode and on the Deck, where a window would only
    // be letterboxed by gamescope anyway.
    const bool fullscreen = SDL_getenv("SteamTenfoot") != nullptr || SDL_getenv("SteamDeck") != nullptr;
    SDL_Window *window = nullptr;
    Launcher launcher;
    {
        std::ifstream file(root + "/private/build.json");
        const std::string text{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
        std::string version;
        if (jsonValue(text, "version", version) && version.size() > 2)
            launcher.installed = "INSTALLED: " + version.substr(1, version.size() - 2);
    }
    if (!SDL_CreateWindowAndRenderer("Dead or Alive 3", kWidth * 2, kHeight * 2,
            fullscreen ? SDL_WINDOW_FULLSCREEN : SDL_WINDOW_RESIZABLE, &window, &launcher.renderer)) {
        std::fprintf(stderr, "launcher: cannot open a window: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    SDL_SetRenderLogicalPresentation(launcher.renderer, kWidth, kHeight, SDL_LOGICAL_PRESENTATION_INTEGER_SCALE);
    SDL_SetRenderVSync(launcher.renderer, 1);
    SDL_SetRenderDrawBlendMode(launcher.renderer, SDL_BLENDMODE_BLEND);
    // Without the picture the launcher still works on a plain fill.
    if (SDL_Surface *picture = SDL_LoadPNG((root + "/tools/launcher-background.png").c_str())) {
        launcher.background = SDL_CreateTextureFromSurface(launcher.renderer, picture);
        SDL_DestroySurface(picture);
    }

    const int nearest = static_cast<int>(std::upper_bound(std::begin(kHeights), std::end(kHeights),
        settings.height) - std::begin(kHeights)) - 1;
    launcher.choice[RESOLUTION] = std::max(0, nearest);
    launcher.choice[MSAA] = indexOf(kSamples, settings.msaa, 0);
    launcher.choice[VOLUME] = indexOf(kVolumes, settings.volume, 0);
    launcher.choice[WIDESCREEN] = settings.widescreen ? 1 : 0;
    launcher.choice[FREE_CAMERA] = settings.free_camera ? 1 : 0;
    launcher.choice[SWIMSUITS] = settings.swimsuits ? 1 : 0;
    launcher.choice[START_FAVORITE] = settings.start_favorite ? 1 : 0;

    int result = -1;
    int held_dx = 0, held_dy = 0;
    Uint64 next_repeat = 0;
    while (result < 0) {
        SDL_Event event;
        while (result < 0 && SDL_PollEvent(&event)) {
            SDL_ConvertEventToRenderCoordinates(launcher.renderer, &event);
            switch (event.type) {
            case SDL_EVENT_QUIT:
                result = 1;
                break;
            case SDL_EVENT_GAMEPAD_ADDED:
                SDL_OpenGamepad(event.gdevice.which);
                break;
            case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
                if (event.gbutton.button == SDL_GAMEPAD_BUTTON_SOUTH || event.gbutton.button == SDL_GAMEPAD_BUTTON_START)
                    result = launcher.accept();
                else if (event.gbutton.button == SDL_GAMEPAD_BUTTON_EAST || event.gbutton.button == SDL_GAMEPAD_BUTTON_BACK)
                    result = launcher.back();
                break;
            case SDL_EVENT_KEY_DOWN:
                switch (event.key.key) {
                case SDLK_UP: launcher.move(-1, 0); break;
                case SDLK_DOWN: launcher.move(1, 0); break;
                case SDLK_LEFT: launcher.move(0, -1); break;
                case SDLK_RIGHT: launcher.move(0, 1); break;
                case SDLK_RETURN:
                case SDLK_SPACE: result = launcher.accept(); break;
                case SDLK_ESCAPE:
                case SDLK_BACKSPACE: result = launcher.back(); break;
                default: break;
                }
                break;
            case SDL_EVENT_MOUSE_MOTION: {
                const int m = hitMenu(event.motion.x, event.motion.y);
                const int i = hitRow(event.motion.x, event.motion.y, static_cast<int>(launcher.rows().size()));
                if (!launcher.in_page && m >= 0) launcher.menu = m;
                if (launcher.in_page && i >= 0) launcher.row = i;
                break;
            }
            case SDL_EVENT_MOUSE_BUTTON_DOWN: {
                // Touch arrives as mouse input. The first tap on a row enters its page;
                // then values step down on their left half and up on their right half.
                if (event.button.button == SDL_BUTTON_RIGHT) { result = launcher.back(); break; }
                const float x = event.button.x, y = event.button.y;
                const int m = hitMenu(x, y);
                if (m >= 0) {
                    launcher.menu = m;
                    launcher.in_page = false;
                    result = launcher.accept();
                    break;
                }
                const int i = hitRow(x, y, static_cast<int>(launcher.rows().size()));
                if (i < 0) break;
                const bool entering = !launcher.in_page;
                launcher.in_page = true;
                launcher.row = i;
                if (!entering && x >= kValueX - 8) launcher.change(launcher.rows()[i], x < kValueX + kValueW / 2 ? -1 : 1);
                break;
            }
            default:
                break;
            }
        }

        // D-pad and stick: one step on press, then repeat while held.
        int dx, dy;
        heldDirection(dx, dy);
        const Uint64 now = SDL_GetTicks();
        if (dx != held_dx || dy != held_dy) {
            held_dx = dx;
            held_dy = dy;
            next_repeat = now + 350;
            if (dx || dy) launcher.move(dy, dx);
        } else if ((dx || dy) && now >= next_repeat) {
            next_repeat = now + 120;
            launcher.move(dy, dx);
        }
        if (result < 0) launcher.draw();
    }

    if (result == 0) {
        settings.height = kHeights[launcher.choice[RESOLUTION]];
        settings.msaa = kSamples[launcher.choice[MSAA]];
        settings.volume = kVolumes[launcher.choice[VOLUME]];
        settings.widescreen = launcher.choice[WIDESCREEN] != 0;
        settings.free_camera = launcher.choice[FREE_CAMERA] != 0;
        settings.swimsuits = launcher.choice[SWIMSUITS] != 0;
        settings.start_favorite = launcher.choice[START_FAVORITE] != 0;
        if (!saveSettings(settings_path, settings)) {
            std::fprintf(stderr, "launcher: cannot write %s\n", settings_path.c_str());
            result = 1;
        }
    }
    SDL_DestroyTexture(launcher.background);
    SDL_DestroyRenderer(launcher.renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return result;
}
