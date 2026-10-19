#include "doa3_balance.h"
#include "kernel_abi.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

RecompReadFilter (*recomp_kernel_read_filter_for)(const char *host_path);

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "line %d: %s\n", __LINE__, #condition); exit(1); \
} } while (0)

static void write_file(const char *path, const char *text)
{
    FILE *file = fopen(path, "w");
    CHECK(file != NULL);
    fputs(text, file);
    fclose(file);
}

int main(void)
{
    const char *path = "doa3_balance_test.txt";
    CHECK(!recomp_doa3_balance_applies_to("D:\\disc\\loadfile.afs"));

    write_file(path, "# offset old new\n00001001 05 02\n00001003 0c 08\r\n00002000 1e 1c\n");
    CHECK(recomp_doa3_balance_load(path));
    CHECK(recomp_doa3_balance_applies_to("D:\\disc\\LoadFile.AFS"));
    CHECK(recomp_doa3_balance_applies_to("z:/cache/loadfile.afs"));
    CHECK(!recomp_doa3_balance_applies_to("D:\\disc\\voice.afs"));
    CHECK(!recomp_doa3_balance_applies_to("D:\\disc\\loadfile.afs.bak"));

    /* A read starting mid-file sees only the changes it covers; a byte that
       no longer holds the USA value is left alone. */
    uint8_t bytes[4] = {0x05, 0x05, 0x07, 0x0c};
    recomp_doa3_balance_apply(0x1000, bytes, sizeof bytes);
    CHECK(memcmp(bytes, "\x05\x02\x07\x08", 4) == 0);
    recomp_doa3_balance_apply(0x1000, bytes, sizeof bytes);
    CHECK(memcmp(bytes, "\x05\x02\x07\x08", 4) == 0);
    uint8_t mismatch[1] = {0x1d};
    recomp_doa3_balance_apply(0x2000, mismatch, 1);
    CHECK(mismatch[0] == 0x1d);

    write_file(path, "00001001 05 102\n");
    CHECK(!recomp_doa3_balance_load(path));
    write_file(path, "00001001 05 02 extra\n");
    CHECK(!recomp_doa3_balance_load(path));
    CHECK(recomp_doa3_balance_applies_to("loadfile.afs"));
    CHECK(!recomp_doa3_balance_load("missing-balance.txt"));

    remove(path);
    puts("doa3 balance tests passed");
    return 0;
}
