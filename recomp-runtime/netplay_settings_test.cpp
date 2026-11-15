/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "netplay_settings.h"

#include <chrono>
#include <cstdio>
#include <fstream>
#include <string>

static int failures;

static void check(const char *name, bool passed)
{
    if (!passed) {
        std::fprintf(stderr, "FAIL %s\n", name);
        ++failures;
    }
}

int main()
{
    namespace fs = std::filesystem;
    const auto root = fs::temp_directory_path() / ("recomp-netplay-settings-test-" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(root);
    const auto path = root / "netplay.txt";

    // A first run makes an ID and keeps it.
    RecompNetplaySettings first;
    recomp_netplay_settings_load(path, first);
    RecompNetplaySettings again;
    recomp_netplay_settings_load(path, again);
    check("id made and kept", first.id != 0u && again.id == first.id);

    // Every setting and blocked player survives a save, including a name with spaces and a control byte.
    first.delay = 4u;
    first.port = 47741u;
    first.display = 2;
    first.recent = "203.0.113.7:47701";
    first.matchmaker = "127.0.0.1:47713";
    first.privateRoom = true;
    first.onlineName = "KASUMI  2";
    first.blocked = {{0x0123456789abcdefull, "SOME ONE"}, {0xfedcba9876543210ull, "BAD\nNAME"}};
    recomp_netplay_settings_save(path, first);
    RecompNetplaySettings read;
    read.blocked = {{1u, "STALE"}};
    recomp_netplay_settings_load(path, read);
    check("settings round trip", read.delay == 4u && read.port == 47741u && read.display == 2 &&
        read.recent == first.recent && read.matchmaker == first.matchmaker && read.privateRoom && read.id == first.id &&
        read.onlineName == "KASUMI  2");
    check("block list round trip", read.blocked.size() == 2u && read.blocked[0].first == 0x0123456789abcdefull &&
        read.blocked[0].second == "SOME ONE" && read.blocked[1].first == 0xfedcba9876543210ull &&
        read.blocked[1].second == "BAD?NAME");

    // A file from before IDs gets one; a bad ID or block line is skipped.
    std::ofstream(path, std::ios::trunc) << "delay 3\nid 00000000000000zz\nblock 0000000000000000 ZERO\ndelay abc\n";
    RecompNetplaySettings old;
    recomp_netplay_settings_load(path, old);
    RecompNetplaySettings reread;
    recomp_netplay_settings_load(path, reread);
    check("old file gets an id", old.delay == 3u && old.id != 0u && old.blocked.empty() && reread.id == old.id &&
        reread.delay == 3u);
    check("no name by default", old.onlineName.empty());

    // A name is trimmed and upper-cased; anything but letters, digits and spaces, or over 15, is refused.
    check("name cleaned", recomp_netplay_clean_name("  ryu hayabusa ") == "RYU HAYABUSA");
    check("blank name empty", recomp_netplay_clean_name("   ").empty());
    check("symbol refused", recomp_netplay_clean_name("A_B").empty() && recomp_netplay_clean_name("\xc3\xa9").empty());
    check("15 kept, 16 refused", recomp_netplay_clean_name(std::string(15u, 'A')).size() == 15u &&
        recomp_netplay_clean_name(std::string(16u, 'A')).empty());
    std::ofstream(path, std::ios::trunc) << "name bad!name\n";
    RecompNetplaySettings badName;
    recomp_netplay_settings_load(path, badName);
    check("bad name line skipped", badName.onlineName.empty());

    // A fight labels a side without a name, and tells matching names apart.
    std::string names[2];
    recomp_netplay_fight_names("AYANE", "HAYATE", names);
    check("fight names kept", names[0] == "AYANE" && names[1] == "HAYATE");
    recomp_netplay_fight_names("AYANE", "", names);
    check("fight name missing", names[0] == "AYANE" && names[1] == "P2");
    recomp_netplay_fight_names("PLAYER", "PLAYER", names);
    check("fight names matching", names[0] == "P1 PLAYER" && names[1] == "P2 PLAYER");

    check("failed save reported", !recomp_netplay_settings_save(root / "missing" / "netplay.txt", old));

    fs::remove_all(root);
    if (failures != 0) return 1;
    std::puts("PASS netplay settings keep an install ID, the settings, the name and the block list");
    return 0;
}
