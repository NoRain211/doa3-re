#include "doa3_balance.h"

#include "kernel_abi.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct BalanceChange {
    uint64_t offset;
    uint8_t old_value;
    uint8_t new_value;
} BalanceChange;

static BalanceChange *changes;
static size_t change_count;

bool recomp_doa3_balance_load(const char *path)
{
    FILE *file = fopen(path, "r");
    if (file == NULL) return false;
    BalanceChange *loaded = NULL;
    size_t count = 0, capacity = 0;
    bool ok = true;
    char line[128];
    while (ok && fgets(line, sizeof line, file) != NULL) {
        unsigned long long offset;
        unsigned old_value, new_value;
        char extra;
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') continue;
        if (sscanf(line, "%llx %x %x %c", &offset, &old_value, &new_value, &extra) != 3 ||
            old_value > 0xffu || new_value > 0xffu) {
            ok = false;
            break;
        }
        if (count == capacity) {
            capacity = capacity ? capacity * 2u : 256u;
            BalanceChange *grown = realloc(loaded, capacity * sizeof *loaded);
            if (grown == NULL) {
                ok = false;
                break;
            }
            loaded = grown;
        }
        loaded[count++] = (BalanceChange){offset, (uint8_t)old_value, (uint8_t)new_value};
    }
    fclose(file);
    if (!ok) {
        free(loaded);
        fprintf(stderr, "recomp doa3 balance: ignoring malformed %s\n", path);
        return false;
    }
    free(changes);
    changes = loaded;
    change_count = count;
    fprintf(stderr, "recomp doa3 balance: loaded %zu byte changes from %s\n", count, path);
    return true;
}

bool recomp_doa3_balance_applies_to(const char *host_path)
{
    static const char name[] = "loadfile.afs";
    const char *leaf = host_path;
    for (const char *p = host_path; *p != '\0'; ++p) {
        if (*p == '\\' || *p == '/') leaf = p + 1;
    }
    size_t i = 0;
    while (name[i] != '\0' && tolower((unsigned char)leaf[i]) == name[i]) ++i;
    return change_count != 0u && name[i] == '\0' && leaf[i] == '\0';
}

void recomp_doa3_balance_apply(uint64_t offset, uint8_t *bytes, uint32_t count)
{
    unsigned applied = 0;
    /* shortcut: a linear scan of about 200 changes per read; index by offset if the table grows. */
    for (size_t i = 0; i < change_count; ++i) {
        if (changes[i].offset < offset || changes[i].offset - offset >= count) continue;
        uint8_t *byte = bytes + (changes[i].offset - offset);
        if (*byte != changes[i].old_value) continue;
        *byte = changes[i].new_value;
        ++applied;
    }
    if (applied != 0u) {
        fprintf(stderr, "recomp doa3 balance: applied %u byte changes at offset %llu\n",
            applied, (unsigned long long)offset);
    }
}

static RecompReadFilter balance_filter_for(const char *host_path)
{
    return recomp_doa3_balance_applies_to(host_path) ? recomp_doa3_balance_apply : NULL;
}

void recomp_doa3_balance_install(const char *disc_root)
{
    char path[1024];
    if (snprintf(path, sizeof path, "%s/doa3-31-balance.txt", disc_root) >= (int)sizeof path) return;
    recomp_doa3_balance_load(path);
    recomp_kernel_read_filter_for = balance_filter_for;
}
