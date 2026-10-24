#include "win32_compat.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition); return 1; } } while (0)

/* The disc's names differ in case from the game's, so lookups must ignore case. */
int main(void)
{
    char root[] = "/tmp/recomp-case-XXXXXX";
    char path[PATH_MAX];
    CHECK(mkdtemp(root) != NULL);
    snprintf(path, sizeof path, "%s/Dir", root);
    CHECK(mkdir(path, 0755) == 0);
    snprintf(path, sizeof path, "%s/Dir/File.TXT", root);
    FILE *file = fopen(path, "w");
    CHECK(file != NULL);
    fclose(file);

    snprintf(path, sizeof path, "%s/dir/file.txt", root);
    CHECK(GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES);
    CHECK(chdir(root) == 0);
    CHECK(GetFileAttributesA("DIR/FILE.txt") != INVALID_FILE_ATTRIBUTES);

    /* A new leaf keeps its own case inside the matched directory. */
    HANDLE handle = CreateFileA("dir/NewLeaf.bin", GENERIC_WRITE, 0, NULL, CREATE_NEW, 0, NULL);
    CHECK(handle != INVALID_HANDLE_VALUE);
    CloseHandle(handle);
    struct stat info;
    CHECK(stat("Dir/NewLeaf.bin", &info) == 0);

    WIN32_FIND_DATAA found;
    HANDLE find = FindFirstFileA("dir/*.TXT", &found);
    CHECK(find != INVALID_HANDLE_VALUE);
    CHECK(strcmp(found.cFileName, "File.TXT") == 0);
    FindClose(find);

    /* Windows tells a missing name from a missing directory. */
    CHECK(FindFirstFileA("dir/*.none", &found) == INVALID_HANDLE_VALUE);
    CHECK(GetLastError() == ERROR_FILE_NOT_FOUND);
    CHECK(FindFirstFileA("missing/*", &found) == INVALID_HANDLE_VALUE);
    CHECK(GetLastError() == ERROR_PATH_NOT_FOUND);
    CHECK(CreateFileA("missing/leaf", GENERIC_WRITE, 0, NULL, CREATE_NEW, 0, NULL) ==
        INVALID_HANDLE_VALUE);
    CHECK(GetLastError() == ERROR_PATH_NOT_FOUND);

    /* Wide names reach the disk as UTF-8 whatever the C locale. */
    handle = CreateFileW(L"dir/caf\u00e9.bin", GENERIC_WRITE, 0, NULL, CREATE_NEW, 0, NULL);
    CHECK(handle != INVALID_HANDLE_VALUE);
    CloseHandle(handle);
    CHECK(stat("Dir/caf\xc3\xa9.bin", &info) == 0);
    unlink("Dir/caf\xc3\xa9.bin");

    /* A -1 or 0 time leaves that time alone. */
    handle = CreateFileA("Dir/NewLeaf.bin", GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    CHECK(handle != INVALID_HANDLE_VALUE);
    FILE_BASIC_INFO basic;
    memset(&basic, 0, sizeof basic);
    basic.LastWriteTime.QuadPart = 116444736000000000LL + 10000000LL; // 1970-01-01 00:00:01
    CHECK(SetFileInformationByHandle(handle, FileBasicInfo, &basic, sizeof basic));
    basic.LastAccessTime.QuadPart = -1;
    basic.LastWriteTime.QuadPart = -1;
    CHECK(SetFileInformationByHandle(handle, FileBasicInfo, &basic, sizeof basic));
    CloseHandle(handle);
    CHECK(stat("Dir/NewLeaf.bin", &info) == 0);
    CHECK(info.st_mtime == 1);

    /* Listings come back in NTFS order: upcased, so '_' sorts after letters. */
    CHECK(mkdir("order", 0700) == 0);
    const char *const listed[] = {".", "..", "Alpha", "zeta", "_under"};
    for (int i = 2; i < 5; ++i) {
        char name[64];
        snprintf(name, sizeof name, "order/%s", listed[i]);
        FILE *file = fopen(name, "w");
        CHECK(file != NULL);
        fclose(file);
    }
    find = FindFirstFileA("order\\*", &found);
    CHECK(find != INVALID_HANDLE_VALUE);
    for (int i = 0; i < 5; ++i) {
        CHECK(strcmp(found.cFileName, listed[i]) == 0);
        CHECK(FindNextFileA(find, &found) == (i < 4));
    }
    FindClose(find);
    find = FindFirstFileA("order/*.*", &found);
    CHECK(find != INVALID_HANDLE_VALUE);
    CHECK(FindNextFileA(find, &found) && FindNextFileA(find, &found));
    CHECK(strcmp(found.cFileName, "Alpha") == 0);
    FindClose(find);
    for (int i = 2; i < 5; ++i) {
        char name[64];
        snprintf(name, sizeof name, "order/%s", listed[i]);
        unlink(name);
    }
    rmdir("order");

    /* Read-only must not drop the other attributes, also on a file that is already
       read-only, and clearing it must stick. */
    const DWORD hidden_readonly = FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_HIDDEN;
    CHECK(SetFileAttributesA("Dir/File.TXT", FILE_ATTRIBUTE_READONLY));
    CHECK(SetFileAttributesA("Dir/File.TXT", hidden_readonly));
    CHECK((GetFileAttributesA("Dir/File.TXT") & hidden_readonly) == hidden_readonly);
    CHECK((GetFileAttributesA("Dir/File.TXT") & FILE_ATTRIBUTE_NORMAL) == 0);
    CHECK(SetFileAttributesA("Dir/File.TXT", FILE_ATTRIBUTE_NORMAL));
    CHECK((GetFileAttributesA("Dir/File.TXT") & hidden_readonly) == 0);

    unlink("Dir/NewLeaf.bin");
    unlink("Dir/File.TXT");
    rmdir("Dir");
    CHECK(chdir("/") == 0);
    rmdir(root);
    return 0;
}
