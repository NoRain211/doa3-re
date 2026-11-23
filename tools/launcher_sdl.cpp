// SPDX-License-Identifier: GPL-3.0-or-later
// Linux launcher: pick the installed version to play, resolution,
// anti-aliasing, volume, widescreen and the
// DOAXBV swimsuit switch (its disc: private/doaxbv-path.txt or an .iso there),
// remember them in private/launcher.json, then exit 0 so run_game.py starts
// the game. Mirrors tools/launcher.cs without SMAA (Direct3D 11 only), and its
// GAME page builds the game from the player's disc: tools/launcher-background.png under a top bar,
// and a panel on the left with the page tabs, the chosen page's rows and their
// information. Gamepad, keyboard, mouse and touch all work, so it runs from
// Steam Game Mode. Exits 1 when closed without Play.
#include <SDL3/SDL.h>

#include <algorithm>
#include <cctype>
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
// Horizon Bar layout, as in tools/launcher.cs.
constexpr float kTopH = 30, kSide = 20, kFadeY = 230;
// The left panel: tab strip, rows, then two information lines at its bottom.
constexpr float kPanelW = 320, kPad = 14, kTabY = 44, kRowY = 72, kRowH = 24, kInfoY = 360;

const int kHeights[] = {480, 720, 1080, 1440, 2160};
const int kSamples[] = {1, 2, 4, 8};
const char *const kScreens[] = {"4:3", "16:9", "16:10", "ULTRAWIDE"};
const int kVolumes[] = {100, 75, 50, 25, 0};

// Tab names are short: all five must fit the 320-pixel panel.
// Every item before CHOOSE_ISO is a setting with choices; the rest are buttons, so they come last and have no choice count.
enum Item { RESOLUTION, MSAA, VOLUME, WIDESCREEN, FREE_CAMERA, SWIMSUITS, START_FAVORITE, CHECK_UPDATES, MOVIES, VERSION,
            CHOOSE_ISO, CHOOSE_FOLDER, BUILD, UPDATE, PLAY, ITEM_COUNT };
const char *kLabels[] = {"RESOLUTION", "MSAA", "VOLUME", "SCREEN", "FREE CAMERA", "SWIMSUITS", "START IN FAVORITE", "CHECK AT START",
                         "MOVIES", "VERSION", "DISC ISO", "DISC FOLDER", "BUILD", "UPDATE", "START GAME"};
// Each row's information line; the panel shows two lines of it.
const char *kRowInfo[] = {"Sets the render resolution. 480p opens in a window",
                          "Smooths polygon edges with multisampling",
                          "Sets the master volume",
                          "Wider 3D view. The HUD and menus stay 4:3",
                          "In game, F1 or a right-stick click flies the camera; P freezes",
                          "In a fight, click L3: LB / LT P1, RB / RT P2, X / B favorite",
                          "Each fighter starts in her first favorite (L3 + X / B)",
                          "Looks for a newer release when the launcher opens",
                          "Off saves 3.4 GB; the game then skips every movie",
                          "The version START GAME plays; each keeps its own saves",
                          "", // the build status
                          "", // the build status
                          "Builds the game from your own DOA3 disc; takes 10-20 minutes",
                          "Checks GitHub for a newer release and installs it",
                          "Saves these settings and starts the game"};
static_assert(std::size(kLabels) == ITEM_COUNT, "kLabels needs one label per Item");
static_assert(std::size(kRowInfo) == ITEM_COUNT, "kRowInfo needs one line per Item");

// How many values each setting steps through, before the buttons.
const int kCounts[] = {static_cast<int>(std::size(kHeights)), static_cast<int>(std::size(kSamples)),
    static_cast<int>(std::size(kVolumes)), static_cast<int>(std::size(kScreens)), 2, 2, 2, 2, 2,
    0}; // VERSION: count() gives the installed versions
static_assert(std::size(kCounts) == CHOOSE_ISO, "kCounts needs one count per setting");

struct Page { const char *name, *info; std::vector<int> rows; };
const Page kPages[] = {
    {"GAME", "Starts the game, or builds it from your own DOA3 disc", {PLAY, VERSION, CHOOSE_ISO, CHOOSE_FOLDER, MOVIES, BUILD}},
    {"VIDEO", "Sets resolution, anti-aliasing and screen shape", {RESOLUTION, MSAA, WIDESCREEN}},
    {"AUDIO", "Sets the master volume", {VOLUME}},
    {"GAMEPLAY", "Swimsuits need your DOAXBV disc: an .iso in private/ or doaxbv-path.txt",
     {FREE_CAMERA, SWIMSUITS, START_FAVORITE}},
    {"UPDATE", "Checks GitHub for a newer release and installs it", {UPDATE, CHECK_UPDATES}},
};
constexpr int kPageCount = static_cast<int>(std::size(kPages)), kGamePage = 0, kUpdatePage = 4;
// The GAME page's rows.
constexpr int kPlayRow = 0, kDiscRow = 2, kBuildRow = 5;
// tools/update.py's exit codes; the last line it prints is the message to show.
enum UpdateResult { UPDATE_CURRENT = 0, UPDATE_FAILED = 1, UPDATE_AVAILABLE = 2, UPDATE_INSTALLED = 3 };

struct Settings {
    int height = 0;
    int msaa = 1;
    int volume = 100;
    std::string screen = "4:3";
    bool free_camera = false;  // F1, ` or the right stick's click takes the camera
    bool swimsuits = true;   // DOAXBV swimsuits from private/doaxbv-path.txt or an .iso in private/
    bool start_favorite = false;
    bool check_updates = true; // look for a newer release when the launcher opens
    bool movies = true;        // the build installs the disc's movies (3.4 GB)
    std::string version;       // the installed version START GAME plays
};

struct Color { Uint8 r, g, b; };
constexpr Color kDark{42, 29, 21}, kWhite{243, 238, 232}, kSand{241, 197, 143};
constexpr Color kMuted{191, 178, 165}, kGold{232, 191, 138}, kBar{13, 10, 9};

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

// A quoted string's text, which jsonValue leaves its quotes on; fallback when absent.
std::string jsonText(const std::string &text, const char *key, const char *fallback)
{
    std::string value;
    if (!jsonValue(text, key, value) || value.size() < 2 || value.front() != '"') return fallback;
    return value.substr(1, value.size() - 2);
}

// The objects of the array under key, each as the text of one { } object. The
// build writes a flat list of flat objects.
std::vector<std::string> jsonObjects(const std::string &text, const char *key)
{
    std::vector<std::string> objects;
    const auto at = text.find(std::string("\"") + key + "\"");
    if (at == std::string::npos) return objects;
    const auto open = text.find('[', at);
    if (open == std::string::npos) return objects;
    for (auto object = text.find('{', open); object != std::string::npos; object = text.find('{', object)) {
        const auto end = text.find('}', object);
        if (end == std::string::npos) break;
        objects.push_back(text.substr(object, end - object + 1));
        const auto comma = text.find(',', end), close = text.find(']', end);
        if (close != std::string::npos && (comma == std::string::npos || close < comma)) break;
        object = end;
    }
    return objects;
}

std::string upper(std::string text)
{
    std::transform(text.begin(), text.end(), text.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return text;
}

// A version the build installed: its own runner folder, play folder (with the
// saves) and XBE name, from private/build.json's installed list.
struct Install { std::string name, play_disc, xbe, build_dir; };

Settings loadSettings(const std::string &path)
{
    Settings settings;
    std::ifstream file(path);
    const std::string text{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
    std::string value;
    if (jsonValue(text, "height", value)) settings.height = std::atoi(value.c_str());
    if (jsonValue(text, "msaa", value)) settings.msaa = std::atoi(value.c_str());
    if (jsonValue(text, "volume", value)) settings.volume = std::atoi(value.c_str());
    // Settings saved before 16:10 hold a widescreen flag.
    if (jsonValue(text, "widescreen", value)) settings.screen = value == "true" ? "16:9" : "4:3";
    if (jsonValue(text, "screen", value) && value.size() > 2) settings.screen = value.substr(1, value.size() - 2);
    if (jsonValue(text, "free_camera", value)) settings.free_camera = value == "true";
    if (jsonValue(text, "swimsuits", value)) settings.swimsuits = value != "false";
    if (jsonValue(text, "swimsuit_start_favorite", value)) settings.start_favorite = value == "true";
    if (jsonValue(text, "check_updates", value)) settings.check_updates = value != "false";
    if (jsonValue(text, "movies", value)) settings.movies = value != "false";
    settings.version = jsonText(text, "version", "");
    return settings;
}

bool saveSettings(const std::string &path, const Settings &settings)
{
    std::ofstream file(path, std::ios::trunc);
    file << "{\n  \"height\": " << settings.height << ",\n  \"msaa\": " << settings.msaa
         << ",\n  \"smaa\": false,\n  \"volume\": " << settings.volume
         << ",\n  \"screen\": \"" << settings.screen << '"'
         << ",\n  \"free_camera\": " << (settings.free_camera ? "true" : "false")
         << ",\n  \"swimsuits\": " << (settings.swimsuits ? "true" : "false")
         << ",\n  \"swimsuit_start_favorite\": " << (settings.start_favorite ? "true" : "false")
         << ",\n  \"movies\": " << (settings.movies ? "true" : "false")
         << ",\n  \"check_updates\": " << (settings.check_updates ? "true" : "false")
         << ",\n  \"version\": \"" << settings.version << "\"\n}\n";
    return static_cast<bool>(file);
}

template <size_t N>
int indexOf(const int (&values)[N], int value, int fallback)
{
    const auto it = std::find(std::begin(values), std::end(values), value);
    return it == std::end(values) ? fallback : static_cast<int>(it - std::begin(values));
}

// The top bar's text in at most two lines of room characters, wrapped at a space when
// there is one. The version list is shortened with "..." so the UPDATE marker always shows.
std::pair<std::string, std::string> headerLines(std::string names, const std::string &update, size_t room)
{
    std::string state = names + update;
    for (;;) {
        size_t cut = state.size(), rest = state.size();
        if (state.size() > room) {
            const size_t space = state.rfind(' ', room);
            cut = space == std::string::npos ? room : space;
            rest = space == std::string::npos ? cut : cut + 1;
        }
        if (state.size() - rest <= room || names.empty()) return {state.substr(0, cut), state.substr(rest, room)};
        names.pop_back();
        state = names + "..." + update;
    }
}

float tabWidth(int m) { return static_cast<float>(SDL_strlen(kPages[m].name) * 8); }
// The tabs span the panel's inner width, first at its left edge and last at its right, evenly spaced.
float tabGap()
{
    float total = 0;
    for (int m = 0; m < kPageCount; ++m) total += tabWidth(m);
    return (kPanelW - 2 * kPad - total) / (kPageCount - 1);
}
float tabX(int m)
{
    float x = kPad;
    for (int i = 0; i < m; ++i) x += tabWidth(i) + tabGap();
    return x;
}

int hitTab(float x, float y)
{
    if (y < kTopH || y >= kRowY - 6) return -1;
    for (int m = 0; m < kPageCount; ++m)
        if (x >= tabX(m) - tabGap() / 2 && x < tabX(m) + tabWidth(m) + tabGap() / 2) return m;
    return -1;
}

int hitRow(float x, float y, size_t rows)
{
    if (x >= kPanelW || y < kRowY) return -1;
    const int i = static_cast<int>((y - kRowY) / kRowH);
    return i < static_cast<int>(rows) ? i : -1;
}

// Word-wraps text into the information area's two lines; anything longer ends in "...".
std::vector<std::string> wrap(const std::string &value)
{
    const size_t width = static_cast<size_t>((kPanelW - 2 * kPad) / 8);
    std::vector<std::string> lines;
    size_t at = 0;
    while (lines.size() < 2) {
        at = std::min(value.find_first_not_of(' ', at), value.size());
        if (at == value.size()) break;
        if (value.size() - at <= width) { lines.push_back(value.substr(at)); at = value.size(); break; }
        size_t cut = value.rfind(' ', at + width);
        if (cut == std::string::npos || cut <= at) cut = at + width; // a word longer than a line
        lines.push_back(value.substr(at, cut - at));
        at = cut;
    }
    if (value.find_first_not_of(' ', at) != std::string::npos) {
        std::string &last = lines.back();
        last.erase(std::min(last.size(), width - 3));
        last.erase(last.find_last_not_of(' ') + 1);
        last += "...";
    }
    return lines;
}

struct Launcher {
    SDL_Renderer *renderer = nullptr;
    SDL_Texture *background = nullptr;
    int choice[ITEM_COUNT] = {};
    // menu is the chosen page. The focus is on its row cursor while in_page, else on the tab strip.
    int menu = kGamePage, row = 0;
    bool in_page = true;
    SDL_Window *window = nullptr;
    // The versions built here, most recently built last, and START GAME's VERSION row indexes them.
    std::vector<Install> versions;
    // The build runs build_game.sh (build_steamdeck.sh on SteamOS) as a child that logs to private/build.log.
    // build_info is the GAME page's notice: build progress or a refused action, kept until the next input.
    std::string game_source;   // the chosen ISO or disc folder, remembered in private/game-source.txt
    std::string build_info;
    SDL_Process *build = nullptr;
    int build_pid = 0;
    bool stop_armed = false;   // the first exit request during a build asks for a second
    Uint64 next_log = 0;
    // tools/update.py runs as a child process; pending says a check found a newer release.
    enum class Work { Idle, Checking, Installing };
    std::string root;
    bool pending = false, quiet = false;
    Work work = Work::Idle;
    SDL_Process *update = nullptr;
    // Each page's information line; the update page's follows what update.py reports.
    std::vector<std::string> info = [] {
        std::vector<std::string> lines;
        for (const Page &page : kPages) lines.emplace_back(page.info);
        return lines;
    }();

    const std::vector<int> &rows() const { return kPages[menu].rows; }

    std::string valueText(int item) const
    {
        switch (item) {
        case RESOLUTION: return std::to_string(kHeights[choice[item]]) + "P";
        case MSAA: return kSamples[choice[item]] == 1 ? "OFF" : std::to_string(kSamples[choice[item]]) + "X";
        case VOLUME: return kVolumes[choice[item]] == 0 ? "MUTE" : std::to_string(kVolumes[choice[item]]) + "%";
        case WIDESCREEN: return kScreens[choice[item]];
        case VERSION: return versions.empty() ? "NONE" : upper(versions[choice[item]].name);
        case CHOOSE_ISO:
        case CHOOSE_FOLDER: return "CHOOSE";
        case BUILD: return build != nullptr ? "WORKING" : "BUILD";
        case UPDATE: return work != Work::Idle ? "WORKING" : pending ? "INSTALL" : "CHECK";
        case PLAY: return "START";
        default: return choice[item] ? "ON" : "OFF";
        }
    }

    void change(int item, int step)
    {
        if (item >= CHOOSE_ISO) return; // a button has no choices to step through
        if (count(item) == 0) return; // no version is installed yet
        choice[item] = std::clamp(choice[item] + step, 0, count(item) - 1);
    }

    // Where a value row's "< VALUE >" starts; it ends at the panel's inner right edge.
    float valueLeft(int item) const { return kPanelW - kPad - 24 - valueText(item).size() * 16.f; }

    std::string rowInfo(int item) const
    {
        if (item == CHOOSE_ISO || item == CHOOSE_FOLDER) return buildStatus();
        if (item == UPDATE && !info[kUpdatePage].empty()) return info[kUpdatePage];
        return kRowInfo[item];
    }

    // A refused action's notice lasts until the next input; a running build keeps its progress up.
    void clearNotice()
    {
        if (build == nullptr) build_info.clear();
    }

    // An action row does its job; Start Game returns 0 once the game may start. A setting does nothing.
    int activate(int item)
    {
        if (item == PLAY) return play();
        if (item == CHOOSE_ISO) chooseDisc(false);
        else if (item == CHOOSE_FOLDER) chooseDisc(true);
        else if (item == BUILD) startBuild();
        else if (item == UPDATE) startUpdate(false);
        return -1;
    }

    static bool isButton(int item) { return item >= CHOOSE_ISO; }
    int count(int item) const { return item == VERSION ? static_cast<int>(versions.size()) : kCounts[item]; }

    bool built() const { return !versions.empty(); }

    // "INSTALLED: USA 3.0, JAPAN 3.1", or NOT BUILT.
    std::string installedText() const
    {
        if (versions.empty()) return "NOT BUILT";
        std::string names;
        for (const auto &version : versions) names += (names.empty() ? "" : ", ") + upper(version.name);
        return "INSTALLED: " + names;
    }

    // The version the VERSION row shows, or "" without one.
    std::string chosenName() const
    {
        return versions.empty() ? "" : versions[choice[VERSION]].name;
    }

    // The versions installed now, and the row's pick: the remembered one while it is
    // installed, else the one built last.
    void chooseVersion(const std::string &remembered)
    {
        choice[VERSION] = static_cast<int>(versions.size()) - 1;
        for (int i = 0; i < static_cast<int>(versions.size()); ++i)
            if (versions[i].name == remembered) choice[VERSION] = i;
        if (choice[VERSION] < 0) choice[VERSION] = 0;
    }

    // A disc extracted by an earlier build (private/imported-disc*/receipt.json) needs no source.
    bool extracted() const
    {
        return SDL_GetPathInfo((root + "/private/imported-disc/receipt.json").c_str(), nullptr)
            || SDL_GetPathInfo((root + "/private/imported-disc-jp/receipt.json").c_str(), nullptr)
            || SDL_GetPathInfo((root + "/private/imported-disc-eur/receipt.json").c_str(), nullptr);
    }

    // The versions the build recorded in private/build.json, most recently built last.
    // A record older than its installed list is the one version its top-level keys
    // describe, in build/recomp-program, and without the file it is USA 3.0.
    void refreshInstalled()
    {
        versions.clear();
        std::ifstream file(root + "/private/build.json");
        const std::string text{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
        const bool listed = text.find("\"installed\"") != std::string::npos;
        std::vector<std::string> entries = listed ? jsonObjects(text, "installed") : std::vector<std::string>{text};
        for (const std::string &entry : entries) {
            const Install version{jsonText(entry, "version", "USA 3.0"), jsonText(entry, "play_disc", "play-disc"),
                                  jsonText(entry, "xbe", "default.xbe"), jsonText(entry, "build_dir", "build/recomp-program")};
            // A version stays installed while its runner and its play folder exist.
            if (!SDL_GetPathInfo((root + "/" + version.build_dir + "/recomp_program_runner").c_str(), nullptr)) continue;
            if (!SDL_GetPathInfo((root + "/private/" + version.play_disc).c_str(), nullptr)) continue;
            versions.push_back(version);
        }
    }

    std::string buildStatus() const
    {
        const size_t slash = game_source.find_last_of('/');
        std::string line = game_source.empty() ? (extracted() ? "Disc: the one already extracted. " : "No disc chosen. ")
                                               : "Disc: " + game_source.substr(slash == std::string::npos ? 0 : slash + 1) + ". ";
        line += built() ? installedText() + ". Build again after an update or for another disc"
                        : "Not built yet: choose your DOA3 disc (Japan 3.1, USA or Europe), then Build";
        return line;
    }

    // Takes an ISO, a disc folder or its default.xbe from a dialog or a drop; anything else is refused.
    void useDisc(std::string path)
    {
        if (build != nullptr) { build_info = "Wait for the build to finish"; return; }
        SDL_PathInfo path_info;
        const bool exists = SDL_GetPathInfo(path.c_str(), &path_info);
        const auto ends = [&](const char *suffix) {
            const size_t n = SDL_strlen(suffix);
            return path.size() >= n && SDL_strcasecmp(path.c_str() + path.size() - n, suffix) == 0;
        };
        if (exists && path_info.type == SDL_PATHTYPE_FILE && ends("default.xbe")) path.erase(path.find_last_of('/'));
        else if (!exists || (path_info.type == SDL_PATHTYPE_FILE ? !ends(".iso") : path_info.type != SDL_PATHTYPE_DIRECTORY)) {
            build_info = "Choose an Xbox .iso, or a disc folder with default.xbe";
            menu = kGamePage;
            in_page = true;
            row = kDiscRow;
            return;
        }
        while (path.size() > 1 && path.back() == '/') path.pop_back();
        game_source = path;
        SDL_CreateDirectory((root + "/private").c_str());
        std::ofstream(root + "/private/game-source.txt", std::ios::trunc) << game_source << '\n';
        menu = kGamePage;
        in_page = true;
        row = kBuildRow;
        build_info = buildStatus();
    }

    // The dialog may call back from another thread, so the path goes through the event queue.
    static void SDLCALL discChosen(void *, const char *const *files, int)
    {
        if (files == nullptr || files[0] == nullptr) return;
        SDL_Event event{};
        event.type = SDL_EVENT_USER;
        event.user.data1 = SDL_strdup(files[0]);
        SDL_PushEvent(&event);
    }

    void chooseDisc(bool folder)
    {
        static const SDL_DialogFileFilter iso[] = {{"Xbox disc image", "iso"}, {"All files", "*"}};
        if (folder) SDL_ShowOpenFolderDialog(discChosen, nullptr, window, nullptr, false);
        else SDL_ShowOpenFileDialog(discChosen, nullptr, window, iso, 2, nullptr, false);
    }

    static bool onSteamOS()
    {
        std::ifstream file("/etc/os-release");
        for (std::string line; std::getline(file, line);)
            if (line == "ID=steamos") return true;
        return false;
    }

    void startBuild()
    {
        if (build != nullptr) return;
        if (work == Work::Installing) { build_info = "Wait for the update to finish"; return; }
        if (game_source.empty() && !extracted()) { build_info = "Choose your DOA3 ISO or disc folder first"; return; }
        SDL_CreateDirectory((root + "/private").c_str());
        // SteamOS has no compiler, so its script builds in a container. The child runs in its own session, so
        // stopping the build can signal the whole group; its output goes to the log, and it reads no input.
        const std::string script = root + (onSteamOS() ? "/build_steamdeck.sh" : "/build_game.sh");
        const std::string log = root + "/private/build.log";
        const char *flag = choice[MOVIES] ? "--movies" : "--no-movies";
        const char *shell = "export PYTHONUNBUFFERED=1 PYTHONIOENCODING=utf-8; log=$1; shift; "
                            "if command -v setsid >/dev/null; then exec setsid \"$@\" >\"$log\" 2>&1 </dev/null; fi; "
                            "exec \"$@\" >\"$log\" 2>&1 </dev/null";
        std::vector<const char *> args = {"/bin/sh", "-c", shell, "sh", log.c_str(), script.c_str()};
        if (!game_source.empty()) args.push_back(game_source.c_str());
        args.push_back(flag);
        args.push_back(nullptr);
        build = SDL_CreateProcess(args.data(), false);
        if (build == nullptr) { build_info = std::string("Cannot start the build: ") + SDL_GetError(); return; }
        build_pid = static_cast<int>(SDL_GetNumberProperty(SDL_GetProcessProperties(build), SDL_PROP_PROCESS_PID_NUMBER, 0));
        stop_armed = false;
        build_info = "Starting the build";
    }

    // The last non-empty line of the build log, cut to the information area's 75 characters.
    std::string logTail() const
    {
        std::ifstream file(root + "/private/build.log", std::ios::binary | std::ios::ate);
        if (!file) return "";
        const std::streamoff size = file.tellg();
        file.seekg(std::max<std::streamoff>(0, size - 4096));
        std::string text{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
        text.erase(std::remove(text.begin(), text.end(), '\r'), text.end());
        text.erase(text.find_last_not_of(" \n") + 1);
        text.erase(0, text.rfind('\n') + 1);
        return text.substr(0, 75);
    }

    // Shows the build's latest line while it runs, and its outcome once it has exited.
    void pollBuild()
    {
        if (build == nullptr) return;
        int code = 0;
        if (!SDL_WaitProcess(build, false, &code)) {
            const Uint64 now = SDL_GetTicks();
            if (now >= next_log) {
                next_log = now + 500;
                const std::string line = logTail();
                if (!line.empty() && !stop_armed) build_info = line;
            }
            return;
        }
        SDL_DestroyProcess(build);
        build = nullptr;
        const std::string chosen = chosenName();
        refreshInstalled();
        chooseVersion(chosen);
        menu = kGamePage;
        in_page = true;
        if (code == 0) {
            build_info = "Build complete. Choose Start Game to play";
            row = kPlayRow;
        } else {
            build_info = "Build failed (see private/build.log): " + logTail();
            row = kBuildRow;
        }
    }

    // Signals the build's whole process group, then collects it.
    void stopBuild()
    {
        if (build == nullptr) return;
        if (build_pid > 0) {
            // The kill program, not the shell's: dash's builtin rejects "--".
            const std::string group = "-" + std::to_string(build_pid);
            const char *kill[] = {"kill", "-s", "TERM", "--", group.c_str(), nullptr};
            if (SDL_Process *signal = SDL_CreateProcess(kill, false)) {
                SDL_WaitProcess(signal, true, nullptr);
                SDL_DestroyProcess(signal);
            }
        }
        SDL_KillProcess(build, true);
        SDL_WaitProcess(build, true, nullptr);
        SDL_DestroyProcess(build);
        build = nullptr;
    }

    // Checks for a newer release, or installs the one found. A quiet check says nothing unless it finds one.
    void startUpdate(bool quietly)
    {
        if (work != Work::Idle) return;
        quiet = quietly;
        const bool install = pending;
        const std::string script = root + "/tools/update.py";
        // The interpreter run_game.sh and build_game.sh prefer, else python3 on PATH.
        std::string python = root + "/private/venv/bin/python3";
        if (!SDL_GetPathInfo(python.c_str(), nullptr)) python = "python3";
        const char *args[] = {python.c_str(), script.c_str(), install ? "install" : "check", nullptr};
        update = SDL_CreateProcess(args, true);
        if (update == nullptr) {
            if (!quiet) info[kUpdatePage] = std::string("Cannot run python3: ") + SDL_GetError();
            return;
        }
        work = install ? Work::Installing : Work::Checking;
        if (!quiet) info[kUpdatePage] = install ? "Downloading the update" : "Checking for updates";
    }

    // Collects the update process once it has exited and shows its last line.
    void pollUpdate()
    {
        int code = 0;
        if (update == nullptr || !SDL_WaitProcess(update, false, &code)) return;
        char *data = static_cast<char *>(SDL_ReadProcess(update, nullptr, nullptr));
        SDL_DestroyProcess(update);
        update = nullptr;
        work = Work::Idle;
        std::string output(data != nullptr ? data : "");
        SDL_free(data);
        output.erase(output.find_last_not_of(" \r\n") + 1);
        output.erase(0, output.rfind('\n') + 1);
        // Any other code, such as a crash, is a failure and keeps what an earlier check found.
        if (code == UPDATE_CURRENT || code == UPDATE_AVAILABLE || code == UPDATE_INSTALLED) pending = code == UPDATE_AVAILABLE;
        // The information area fits one 75-character line above the settings.
        if (!quiet || code == UPDATE_AVAILABLE)
            info[kUpdatePage] = output.empty() ? "update.py exited with " + std::to_string(code) : output.substr(0, 75);
    }

    // Play and exit wait for an install, which replaces the tools; this shows why on the update page.
    bool waitForInstall()
    {
        if (work != Work::Installing) return false;
        menu = kUpdatePage;
        in_page = false;
        info[kUpdatePage] = "Wait for the update to finish";
        return true;
    }

    // Leaving needs an install to finish, and a running build to be stopped, which takes a second request.
    int exitRequested()
    {
        if (waitForInstall()) return -1;
        if (build != nullptr && !stop_armed) {
            stop_armed = true;
            menu = kGamePage;
            in_page = false;
            build_info = "A build is running. Exit again to stop it";
            return -1;
        }
        return 1;
    }

    // Start Game from the GAME page or the Start button: 0 once the game is built, else -1 and the row to fix it.
    int play()
    {
        if (waitForInstall()) return -1;
        if (build != nullptr || !built()) {
            menu = kGamePage;
            in_page = true;
            row = build != nullptr || !game_source.empty() || extracted() ? kBuildRow : kDiscRow;
            build_info = build != nullptr ? "Wait for the build to finish" : "Build the game first";
            return -1;
        }
        return 0;
    }

    // A on the tab strip enters the page; on a row it runs the row's action.
    int accept()
    {
        if (!in_page) { in_page = true; row = 0; return -1; }
        return activate(rows()[row]);
    }

    // B leaves the rows for the tab strip, and the tab strip closes the launcher.
    int back()
    {
        if (!in_page) return exitRequested();
        in_page = false;
        return -1;
    }

    // Up and down move between rows, up from the first one to the tab strip; left and right change a
    // value, or the page on the tab strip.
    void move(int dx, int dy)
    {
        if (!in_page) {
            if (dx != 0) menu = std::clamp(menu + dx, 0, kPageCount - 1);
            else if (dy > 0) accept();
        } else if (dx != 0) {
            if (!isButton(rows()[row])) change(rows()[row], dx);
        } else if (dy < 0 && row == 0) {
            in_page = false;
        } else {
            row = std::clamp(row + dy, 0, static_cast<int>(rows().size()) - 1);
        }
    }

    // LB/RB, Q/E and Page Up/Down switch pages from anywhere; in the rows the cursor starts at the top.
    void switchPage(int step)
    {
        const int m = std::clamp(menu + step, 0, kPageCount - 1);
        if (m != menu) { menu = m; row = 0; }
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

    void draw()
    {
        SDL_SetRenderDrawColor(renderer, 74, 60, 51, 255);
        SDL_RenderClear(renderer);
        if (background != nullptr) SDL_RenderTexture(renderer, background, nullptr, nullptr);
        // Letterbox: a solid top bar, and the beach fading into the same dark under the settings.
        fill({0, 0, kWidth, kTopH}, kBar);
        for (float y = kFadeY; y < kHeight; y += 2) {
            const float t = (y - kFadeY) / (kHeight - kFadeY);
            fill({0, y, kWidth, 2}, kBar, static_cast<Uint8>(t < .26f ? t / .26f * 230 : 230 + (t - .26f) / .74f * 25));
        }

        // Top bar: the button prompts at the right, then the installed versions in the room left of
        // them, on a second line when they do not fit on one.
        float x = kWidth - kSide;
        for (const auto &hint : {std::pair<const char *, const char *>{"B", "BACK"}, {"A", "SELECT"}, {"<>", "CHANGE"}, {"LB/RB", "PAGE"}}) {
            x -= SDL_strlen(hint.second) * 8;
            text(x, (kTopH - 8) / 2, hint.second, kMuted);
            const float w = static_cast<float>(SDL_strlen(hint.first) * 8 + 8);
            x -= w + 6;
            fill({x, (kTopH - 12) / 2, w, 12}, {242, 227, 207});
            text(x + 4, (kTopH - 8) / 2, hint.first, kDark);
            x -= 16;
        }
        // The debug font is 8 pixels a character, so the room is what the prompts left.
        const size_t room = static_cast<size_t>((x - kSide) / 8);
        const auto [names, more] = headerLines(installedText(), pending ? " - UPDATE" : "", room);
        text(kSide, more.empty() ? (kTopH - 8) / 2 : 6, names, kMuted);
        if (!more.empty()) text(kSide, 16, more, kMuted);

        // Logo over the cliff.
        chrome(kWidth - 28 - 32 - 14 - 13 * 16, 64, "DEAD OR ALIVE", 2);
        chrome(kWidth - 28 - 32, 48, "3", 4);

        // The panel on the left: page tabs, the chosen page's rows, then their information.
        fill({0, kTopH, kPanelW, kHeight - kTopH}, kBar, 200);
        // The current tab is underlined in sunset gold, and highlighted while the tab strip has the focus.
        for (int m = 0; m < kPageCount; ++m) {
            const bool current = m == menu;
            if (current && !in_page) fill({tabX(m) - 4, kTabY - 6, tabWidth(m) + 8, 22}, kGold, 45);
            text(tabX(m), kTabY, kPages[m].name, !current ? Color{169, 156, 144} : in_page ? Color{255, 243, 228} : Color{255, 255, 255});
            if (current) fill({tabX(m), kTabY + 12, tabWidth(m), 2}, kGold);
        }
        fill({kPad, kRowY - 8, kPanelW - 2 * kPad, 1}, kWhite, 36);

        // One row per setting: its label on the left, "< VALUE >" or an action's verb on the right.
        const Page &page = kPages[menu];
        const float right = kPanelW - kPad;
        for (size_t i = 0; i < page.rows.size(); ++i) {
            const int item = page.rows[i];
            const float y = kRowY + i * kRowH;
            const bool selected = in_page && static_cast<int>(i) == row;
            if (selected) fill({kPad - 6, y, kPanelW - 2 * kPad + 12, kRowH}, kGold, 45);
            text(kPad, y + 8, kLabels[item], selected ? kWhite : Color{198, 182, 166});
            const std::string value = valueText(item);
            if (isButton(item)) {
                const float w = value.size() * 8 + 12.f;
                fill({right - w, y + 5, w, 14}, selected ? Color{242, 227, 207} : kSand, selected ? 255 : 40);
                text(right - w + 6, y + 8, value, selected ? kDark : kSand);
                continue;
            }
            // The arrows show which way the value can still step.
            const float left = valueLeft(item);
            const Color arrow = selected ? kSand : Color{180, 150, 112};
            if (choice[item] > 0) text(left, y + 8, "<", arrow);
            text(left + 12, y + 4, value, selected ? kWhite : Color{232, 222, 212}, 2);
            if (choice[item] < count(item) - 1) text(right - 8, y + 8, ">", arrow);
        }

        // The GAME page's notice, else what the focused row or page does.
        fill({kPad, kInfoY - 10, kPanelW - 2 * kPad, 1}, kWhite, 36);
        const std::string message = menu == kGamePage && !build_info.empty() ? build_info
            : in_page ? rowInfo(page.rows[row])
            : menu == kUpdatePage && !info[kUpdatePage].empty() ? info[kUpdatePage] : page.info;
        const std::vector<std::string> lines = wrap(message);
        for (size_t i = 0; i < lines.size(); ++i) text(kPad, kInfoY + i * 12, lines[i], {216, 202, 188});
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
    launcher.root = root;
    launcher.refreshInstalled();
    {
        std::ifstream file(root + "/private/game-source.txt");
        std::getline(file, launcher.game_source);
    }
    if (!SDL_CreateWindowAndRenderer("Dead or Alive 3", kWidth * 2, kHeight * 2,
            fullscreen ? SDL_WINDOW_FULLSCREEN : SDL_WINDOW_RESIZABLE, &window, &launcher.renderer)) {
        std::fprintf(stderr, "launcher: cannot open a window: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    launcher.window = window;
    // The window icon, when private/launcher-icon.png is present (fan art, so it stays untracked).
    if (SDL_Surface *icon = SDL_LoadPNG((root + "/private/launcher-icon.png").c_str())) {
        SDL_SetWindowIcon(window, icon);
        SDL_DestroySurface(icon);
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
    launcher.choice[WIDESCREEN] = 3; // a saved ultrawide "W:H"
    for (int i = 0; i < static_cast<int>(std::size(kScreens)); ++i) {
        if (settings.screen == kScreens[i]) launcher.choice[WIDESCREEN] = i;
    }
    launcher.choice[FREE_CAMERA] = settings.free_camera ? 1 : 0;
    launcher.choice[SWIMSUITS] = settings.swimsuits ? 1 : 0;
    launcher.choice[START_FAVORITE] = settings.start_favorite ? 1 : 0;
    launcher.choice[CHECK_UPDATES] = settings.check_updates ? 1 : 0;
    launcher.choice[MOVIES] = settings.movies ? 1 : 0;
    launcher.chooseVersion(settings.version);
    launcher.row = launcher.built() ? kPlayRow : kDiscRow;
    if (settings.check_updates) launcher.startUpdate(true);
    // A plain exit keeps an untouched saved ultrawide; play saves what the row shows.
    const Settings loaded = settings;
    const int shown_screen = launcher.choice[WIDESCREEN];

    int result = -1;
    int held_dx = 0, held_dy = 0;
    Uint64 next_repeat = 0;
    while (result < 0) {
        SDL_Event event;
        while (result < 0 && SDL_PollEvent(&event)) {
            SDL_ConvertEventToRenderCoordinates(launcher.renderer, &event);
            switch (event.type) {
            case SDL_EVENT_QUIT:
                result = launcher.exitRequested();
                break;
            case SDL_EVENT_USER: // a chosen disc from the file dialog
                launcher.useDisc(static_cast<char *>(event.user.data1));
                SDL_free(event.user.data1);
                break;
            case SDL_EVENT_DROP_FILE:
                launcher.useDisc(event.drop.data);
                break;
            case SDL_EVENT_GAMEPAD_ADDED:
                SDL_OpenGamepad(event.gdevice.which);
                break;
            case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
                launcher.clearNotice();
                if (event.gbutton.button == SDL_GAMEPAD_BUTTON_SOUTH) result = launcher.accept();
                else if (event.gbutton.button == SDL_GAMEPAD_BUTTON_START) result = launcher.play();
                else if (event.gbutton.button == SDL_GAMEPAD_BUTTON_EAST || event.gbutton.button == SDL_GAMEPAD_BUTTON_BACK)
                    result = launcher.back();
                else if (event.gbutton.button == SDL_GAMEPAD_BUTTON_LEFT_SHOULDER) launcher.switchPage(-1);
                else if (event.gbutton.button == SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER) launcher.switchPage(1);
                break;
            case SDL_EVENT_KEY_DOWN:
                launcher.clearNotice();
                switch (event.key.key) {
                case SDLK_UP: launcher.move(0, -1); break;
                case SDLK_DOWN: launcher.move(0, 1); break;
                case SDLK_LEFT: launcher.move(-1, 0); break;
                case SDLK_RIGHT: launcher.move(1, 0); break;
                case SDLK_Q:
                case SDLK_PAGEUP: launcher.switchPage(-1); break;
                case SDLK_E:
                case SDLK_PAGEDOWN: launcher.switchPage(1); break;
                case SDLK_RETURN:
                case SDLK_SPACE: result = launcher.accept(); break;
                case SDLK_ESCAPE:
                case SDLK_BACKSPACE: result = launcher.back(); break;
                default: break;
                }
                break;
            case SDL_EVENT_MOUSE_MOTION: {
                // Hovering a tab shows its page with the tab strip focused; hovering a row focuses it.
                const int m = hitTab(event.motion.x, event.motion.y);
                const int i = hitRow(event.motion.x, event.motion.y, launcher.rows().size());
                if (m >= 0) {
                    launcher.menu = m;
                    launcher.in_page = false;
                } else if (i >= 0) {
                    launcher.in_page = true;
                    launcher.row = i;
                }
                break;
            }
            case SDL_EVENT_MOUSE_BUTTON_DOWN: {
                // Touch arrives as mouse input. A tap on an action row runs it; on a setting, the left
                // half of its value steps down and the right half up.
                launcher.clearNotice();
                if (event.button.button == SDL_BUTTON_RIGHT) { result = launcher.back(); break; }
                const float x = event.button.x, y = event.button.y;
                const int m = hitTab(x, y);
                if (m >= 0) {
                    launcher.menu = m;
                    launcher.in_page = false;
                    break;
                }
                const int i = hitRow(x, y, launcher.rows().size());
                if (i < 0) break;
                launcher.in_page = true;
                launcher.row = i;
                const int item = launcher.rows()[i];
                const float left = launcher.valueLeft(item);
                if (Launcher::isButton(item)) result = launcher.activate(item);
                else if (x >= left) launcher.change(item, x < (left + kPanelW - kPad) / 2 ? -1 : 1);
                break;
            }
            default:
                break;
            }
        }

        // D-pad and stick: one step on press, then repeat while held.
        int dx, dy;
        heldDirection(dx, dy);
        if (dx != 0) dy = 0; // a diagonal changes a value without also moving the row
        const Uint64 now = SDL_GetTicks();
        if (dx != held_dx || dy != held_dy) {
            held_dx = dx;
            held_dy = dy;
            next_repeat = now + 350;
            if (dx || dy) {
                launcher.clearNotice();
                launcher.move(dx, dy);
            }
        } else if ((dx || dy) && now >= next_repeat) {
            next_repeat = now + 120;
            launcher.move(dx, dy);
        }
        launcher.pollUpdate();
        launcher.pollBuild();
        if (result < 0) launcher.draw();
    }

    // Settings hold after a plain exit too, so the game plays with them later.
    {
        settings.height = kHeights[launcher.choice[RESOLUTION]];
        settings.msaa = kSamples[launcher.choice[MSAA]];
        settings.volume = kVolumes[launcher.choice[VOLUME]];
        settings.screen = kScreens[launcher.choice[WIDESCREEN]];
        if (launcher.choice[WIDESCREEN] == 3) {
            // The primary screen's own shape when it is wider than 16:9 (at most the
            // runtime's 4:1), else 21:9 (2560x1080).
            const SDL_DisplayMode *mode = SDL_GetCurrentDisplayMode(SDL_GetPrimaryDisplay());
            settings.screen = mode != nullptr && mode->w * 9 > mode->h * 16
                ? std::to_string(std::min(mode->w, mode->h * 4)) + ":" + std::to_string(mode->h)
                : "64:27";
        }
        settings.free_camera = launcher.choice[FREE_CAMERA] != 0;
        settings.swimsuits = launcher.choice[SWIMSUITS] != 0;
        settings.start_favorite = launcher.choice[START_FAVORITE] != 0;
        settings.check_updates = launcher.choice[CHECK_UPDATES] != 0;
        settings.movies = launcher.choice[MOVIES] != 0;
        // Play, and the next start, keep the version the row shows; without one the saved name stays.
        if (!launcher.versions.empty()) settings.version = launcher.chosenName();
        if (result != 0 && launcher.choice[WIDESCREEN] == shown_screen) settings.screen = loaded.screen;
        if (!saveSettings(settings_path, settings)) {
            std::fprintf(stderr, "launcher: cannot write %s\n", settings_path.c_str());
            if (result == 0) result = 1;
        }
    }
    SDL_DestroyTexture(launcher.background);
    launcher.stopBuild(); // a second exit request while a build ran
    // Every quit path waits for an install, so only a check can still be running here; stop it rather than leave it behind.
    if (launcher.update != nullptr) {
        SDL_KillProcess(launcher.update, true);
        SDL_WaitProcess(launcher.update, true, nullptr);
        SDL_DestroyProcess(launcher.update);
    }
    SDL_DestroyRenderer(launcher.renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return result;
}
