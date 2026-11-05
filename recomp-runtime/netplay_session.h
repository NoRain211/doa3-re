/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RECOMP_NETPLAY_SESSION_H
#define RECOMP_NETPLAY_SESSION_H

#include "input_model.h"
#include "input_pulse_source.h"

#include <cstdint>
#include <filesystem>
#include <string>

/* Rollback sessions over the running game (docs/rollback-netcode.md): a
   lobby session from the command line or the Versus menu's room, a replay,
   or the RECOMP_ROLLBACK test modes. The runner boots the game; this module
   drives every frame of a session and returns a room's players to it. */

// What a session borrows from the runner.
struct RecompNetplaySessionHost {
    std::filesystem::path disc;          // the play folder
    std::string digest;                  // RECOMP_NETPLAY_BUILD, so both sides simulate alike
    RecompInputPulseSource *pulses;      // the input script a scripted lobby plays
    bool scriptedInput;                  // any --input-* option replaces the controller
    void (*reboot)();                    // boots the game again in this process
};
void recomp_netplay_session_init(const RecompNetplaySessionHost &host);

// --netplay-host <port> [--netplay-delay <frames>] or --netplay-join <host>:<port>.
struct RecompNetplayLaunch {
    std::uint32_t hostPort = 0u, delay = 2u;
    std::string join;
};
// Opens the socket, connects to the other player and readies the session; false with a message on failure.
bool recomp_netplay_session_connect(const RecompNetplayLaunch &launch);
// --replay <file>: false with a message for a file from another build.
bool recomp_netplay_session_load_replay(const std::string &path);

/* Per boot. read_env takes RECOMP_ROLLBACK when no session is set (false
   when it is malformed); enabled says a session runs this boot. */
bool recomp_netplay_session_read_env();
bool recomp_netplay_session_enabled();
/* Installs this boot's frame and save hooks for a session, or the Versus
   menu when neither a session nor SyncTest runs, and hides the walks to
   Versus. The runner clears the frame and save hooks first. */
void recomp_netplay_session_install(bool synctest);

/* Input: true when the session decides the guest port's pad, with present
   set; back_pad replaces port 0's pad while the game walks back to the
   Versus menu after a room session. */
bool recomp_netplay_session_pad(std::uint32_t port, RecompInputGamepad *pad, bool *present);
bool recomp_netplay_session_back_pad(RecompInputGamepad *pad);
void recomp_netplay_session_set_vibration(std::uint32_t port, std::uint16_t left, std::uint16_t right);
// Each presented frame: the walk back to Versus and a room's launch.
void recomp_netplay_session_on_swap(std::uint32_t frame);
// At exit: keeps the replay and tells the other player.
void recomp_netplay_session_exit();

#endif
