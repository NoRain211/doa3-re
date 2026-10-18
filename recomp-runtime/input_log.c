#include "input_log.h"

#include <string.h>

static const char input_log_magic[8] = {'D', 'O', 'A', '3', 'I', 'N', '1', '\0'};

typedef struct InputLogRecord {
    uint32_t frame;
    uint32_t port;
    uint32_t present;
    RecompInputGamepad gamepad;
} InputLogRecord;

bool recomp_input_log_write_header(FILE *file)
{
    return fwrite(input_log_magic, sizeof input_log_magic, 1u, file) == 1u;
}

bool recomp_input_log_read_header(FILE *file)
{
    char magic[sizeof input_log_magic];
    return fread(magic, sizeof magic, 1u, file) == 1u &&
        memcmp(magic, input_log_magic, sizeof magic) == 0;
}

bool recomp_input_log_write(FILE *file, uint32_t frame, uint32_t port,
    bool present, const RecompInputGamepad *gamepad)
{
    InputLogRecord record;
    memset(&record, 0, sizeof record);
    record.frame = frame;
    record.port = port;
    record.present = present ? 1u : 0u;
    record.gamepad = *gamepad;
    return fwrite(&record, sizeof record, 1u, file) == 1u;
}

RecompInputLogRead recomp_input_log_read(FILE *file, uint32_t frame,
    uint32_t port, bool *present, RecompInputGamepad *gamepad)
{
    InputLogRecord record;
    if (fread(&record, sizeof record, 1u, file) != 1u) return RECOMP_INPUT_LOG_END;
    if (record.frame != frame || record.port != port) return RECOMP_INPUT_LOG_DESYNC;
    *present = record.present != 0u;
    *gamepad = record.gamepad;
    return RECOMP_INPUT_LOG_SAMPLE;
}
