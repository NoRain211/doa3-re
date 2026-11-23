#ifndef DOAXBV_RECOMP_DIRECTORY_MODEL_H
#define DOAXBV_RECOMP_DIRECTORY_MODEL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum {
    /* FATX's limit per directory. DOA3 reads up to 256 costume files from its
       title save folder; at 64 the mod costumes past the 64th name vanished. */
    RECOMP_DIRECTORY_MAX_ENTRIES = 4096u,
    RECOMP_DIRECTORY_NAME_SIZE = 260u,
};

typedef struct RecompDirectoryEntry {
    char name[RECOMP_DIRECTORY_NAME_SIZE];
    uint64_t creation_time;
    uint64_t last_access_time;
    uint64_t last_write_time;
    uint64_t change_time;
    uint64_t size;
    uint64_t allocation_size;
    uint32_t attributes;
} RecompDirectoryEntry;

typedef struct RecompDirectoryModel {
    RecompDirectoryEntry *entries; /* Heap, grown as names arrive. */
    size_t capacity;
    size_t count;
    bool truncated;
    size_t cursor;
} RecompDirectoryModel;

/* Frees the entries and empties the model. A model starts zeroed. */
void recomp_directory_reset(RecompDirectoryModel *model);
/* False for an invalid entry or when memory runs out before any name fits.
   A full model keeps its smallest names and sets truncated: full at
   RECOMP_DIRECTORY_MAX_ENTRIES, or at its capacity once growth has failed. */
bool recomp_directory_add(
    RecompDirectoryModel *model,
    const RecompDirectoryEntry *entry);
void recomp_directory_restart(RecompDirectoryModel *model);
bool recomp_directory_next(
    RecompDirectoryModel *model,
    const char *pattern,
    RecompDirectoryEntry *entry);
bool recomp_directory_serialize(
    const RecompDirectoryEntry *entry,
    void *buffer,
    size_t buffer_size,
    size_t *bytes_written);

#endif
