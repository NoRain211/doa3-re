#ifndef DOA3_BALANCE_H
#define DOA3_BALANCE_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Loads the "offset old new" hex lines tools/balance31.py writes. A missing
   or malformed file leaves the previous table, normally empty, in place. */
bool recomp_doa3_balance_load(const char *path);
bool recomp_doa3_balance_applies_to(const char *host_path);
/* Rewrites only bytes that still hold the USA value; offset is the file
   offset of bytes[0]. */
void recomp_doa3_balance_apply(uint64_t offset, uint8_t *bytes, uint32_t count);
/* Loads doa3-31-balance.txt from the disc root and filters loadfile.afs. */
void recomp_doa3_balance_install(const char *disc_root);

#ifdef __cplusplus
}
#endif

#endif
