#include "input_log.h"

#include <stdio.h>
#include <string.h>

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "input log: line %d: %s\n", __LINE__, #condition); return 1; } } while (0)

int main(void)
{
    const char *path = "recomp_input_log_test.bin";
    RecompInputGamepad pressed = {0};
    pressed.buttons = 0x0010u;
    pressed.analog_buttons[0] = 0xffu;
    pressed.thumb_lx = -32768;
    const RecompInputGamepad idle = {0};

    FILE *file = fopen(path, "wb");
    CHECK(file != NULL);
    CHECK(recomp_input_log_write_header(file));
    CHECK(recomp_input_log_write(file, 0u, 0u, true, &pressed));
    CHECK(recomp_input_log_write(file, 0u, 1u, false, &idle));
    CHECK(recomp_input_log_write(file, 1u, 0u, true, &idle));
    fputc(0x55, file); /* a record cut short by a crash */
    CHECK(fclose(file) == 0);

    /* A recorded run replays sample for sample. */
    bool present = false;
    RecompInputGamepad gamepad;
    file = fopen(path, "rb");
    CHECK(file != NULL && recomp_input_log_read_header(file));
    CHECK(recomp_input_log_read(file, 0u, 0u, &present, &gamepad) == RECOMP_INPUT_LOG_SAMPLE);
    CHECK(present && memcmp(&gamepad, &pressed, sizeof gamepad) == 0);
    CHECK(recomp_input_log_read(file, 0u, 1u, &present, &gamepad) == RECOMP_INPUT_LOG_SAMPLE);
    CHECK(!present);
    CHECK(recomp_input_log_read(file, 1u, 0u, &present, &gamepad) == RECOMP_INPUT_LOG_SAMPLE);
    CHECK(recomp_input_log_read(file, 2u, 0u, &present, &gamepad) == RECOMP_INPUT_LOG_END);
    fclose(file);

    /* The game polling a port or frame the log lacks is a desync. */
    file = fopen(path, "rb");
    CHECK(file != NULL && recomp_input_log_read_header(file));
    CHECK(recomp_input_log_read(file, 0u, 1u, &present, &gamepad) == RECOMP_INPUT_LOG_DESYNC);
    fclose(file);

    /* Any other file is refused. */
    file = fopen(path, "wb");
    CHECK(file != NULL && fputs("not a log", file) >= 0 && fclose(file) == 0);
    file = fopen(path, "rb");
    CHECK(file != NULL && !recomp_input_log_read_header(file));
    fclose(file);
    remove(path);
    return 0;
}
