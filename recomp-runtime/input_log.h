#ifndef DOA3_RECOMP_INPUT_LOG_H
#define DOA3_RECOMP_INPUT_LOG_H

#include "input_model.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Input log for --input-record and --input-replay: a magic, then one record
   per controller sample in call order. A replay feeds the records back and
   stops at the first frame or port that does not match. */
typedef enum RecompInputLogRead {
    RECOMP_INPUT_LOG_SAMPLE,
    RECOMP_INPUT_LOG_END,
    RECOMP_INPUT_LOG_DESYNC,
} RecompInputLogRead;

bool recomp_input_log_write_header(FILE *file);
bool recomp_input_log_read_header(FILE *file);
bool recomp_input_log_write(FILE *file, uint32_t frame, uint32_t port,
    bool present, const RecompInputGamepad *gamepad);
/* END covers a short final record as well as a clean end of file. */
RecompInputLogRead recomp_input_log_read(FILE *file, uint32_t frame,
    uint32_t port, bool *present, RecompInputGamepad *gamepad);

#ifdef __cplusplus
}
#endif

#endif
