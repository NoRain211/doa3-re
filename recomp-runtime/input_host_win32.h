#ifndef DOAXBV_RECOMP_INPUT_HOST_WIN32_H
#define DOAXBV_RECOMP_INPUT_HOST_WIN32_H

#include "input_model.h"

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

bool recomp_input_host_sample(uint32_t port, RecompInputGamepad *gamepad);

void recomp_input_host_set_vibration(
    uint32_t port,
    uint16_t left_motor,
    uint16_t right_motor);
void recomp_input_host_stop_vibration(void);
/* Optional hook on each pad sample. While it is set the stick clicks (L3,
   R3) never reach the game; when it returns true (a held combo) the face
   buttons, shoulder buttons, triggers, d-pad, Start and sticks are kept from
   the game too. */
typedef bool (*RecompInputComboHook)(uint32_t port, const RecompInputGamepad *gamepad);
void recomp_input_host_set_combo_hook(RecompInputComboHook hook);

#ifdef __cplusplus
}
#endif

#endif
