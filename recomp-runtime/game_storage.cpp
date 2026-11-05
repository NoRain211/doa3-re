/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "game_state.h"
extern "C" {
#include "kernel_abi.h"
}

#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <system_error>

namespace fs = std::filesystem;

/* The game copies its archives to the Z: cache over its first boots, and a boot
   with a partial cache runs the copy and diverges from one without. Both sides
   start with the whole cache: full copies (the copy reopens a stale file with
   CREATE_ALWAYS, so no hardlinks), which the game accepts as it does its own.
   The session formats its own Z: headers (kernel_file.c), so a drive this
   player never formatted boots like one they did. */
static bool seedArchiveCache(const fs::path &disc)
{
    // shortcut: XAPI has picked partition 5 for Z: on every run; read its choice if another appears.
    char root[512];
    if (!recomp_kernel_storage_root(nullptr, '5', false, root, sizeof root)) return false;
    const fs::path cache = fs::u8path(root);
    for (const char *name : {"bgm.afs", "loadfile.afs", "voice.afs"}) {
        std::error_code error;
        const fs::path source = disc / name, target = cache / name;
        const std::uintmax_t size = fs::file_size(source, error);
        if (error) return false;
        if (fs::file_size(target, error) == size && !error) continue;
        std::fprintf(stderr, "recomp netplay: copying %s to the game's cache\n", name);
        fs::path partial = target;
        partial += ".tmp";
        fs::create_directories(cache, error);
        if (error || !fs::copy_file(source, partial, fs::copy_options::overwrite_existing, error) || error) return false;
        fs::rename(partial, target, error);
        if (error) return false;
    }
    return true;
}

bool recomp_game_prepare_session(const char *disc_root)
{
    if (seedArchiveCache(fs::u8path(disc_root))) return true;
    std::cerr << "recomp runner: cannot copy the game's archives to its cache\n";
    return false;
}
