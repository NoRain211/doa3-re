/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RECOMP_RUNNER_BOOT_H
#define RECOMP_RUNNER_BOOT_H

#include "input_adapter.h"
#include "input_pulse_source.h"
#include "xbe_parser.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

constexpr std::size_t kRamSize = 64u * 1024u * 1024u;
constexpr std::uint32_t kStackTop = 0x00f7fff0u;

// What every boot of the game needs again (runner_boot.cpp); main fills it once.
struct BootConfig {
    doaxbv::XbeParseResult xbe;
    std::uint8_t *ram = nullptr;
    bool ramWriteWatch = false;
    RecompInputSampleSource inputSource = nullptr; // under a session's pads
    bool deterministic = false; // input logs and SyncTest; a session adds its own
    bool scriptedInput = false; // any --input-* option, which keeps the controller's rumble off
};
extern BootConfig bootConfig;
void bootGuest();
// Boots the game again in this process and window, from a frame boundary.
void reboot();

// From runner.cpp.
extern RecompInputPulseSource inputPulseSource;
extern std::uint32_t inputFrame;
bool mapXbe(
    const doaxbv::XbeParseResult& parsed,
    std::vector<std::uint8_t>& memory,
    std::string& error);
void onPresent(std::uint32_t swap);

#endif
