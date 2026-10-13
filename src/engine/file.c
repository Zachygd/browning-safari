/*
 * file.c - the game's File* wrappers (originally thin CreateFile/ReadFile
 * wrappers).  Paths built by the game use DOS separators and assume a
 * case-insensitive filesystem, so lookups are normalised here.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <dirent.h>
#include <unistd.h>
#include "safari.h"

static void normalise(const char *in, char *out, size_t n)
{
    size_t i;
    for (i = 0; in[i] && i < n - 1; i++) out[i] = (in[i] == '\\') ? '/' : in[i];
    out[i] = 0;
}

/* find a directory entry matching 'name' case-insensitively */
static int ci_find(const char *dir, const char *name, char *out, size_t n)
{
    DIR *d = opendir(dir[0] ? dir : ".");
    if (!d) return 0;
    struct dirent *e;
    int found = 0;
    while ((e = readdir(d))) {
        if (strcasecmp(e->d_name, name) == 0) {
            snprintf(out, n, "%s", e->d_name);
            found = 1;
            break;
        }
    }
    closedir(d);
    return found;
}

/* Resolve a game path to an existing file, ignoring case of the file name. */
static int resolve(const char *path, char *out, size_t n)
{
    char p[1024];
    normalise(path, p, sizeof p);
    if (access(p, F_OK) == 0) { snprintf(out, n, "%s", p); return 1; }
    char *slash = strrchr(p, '/');
    char dir[1024], base[256], hit[256];
    if (slash) {
        size_t dl = (size_t)(slash - p);
        memcpy(dir, p, dl); dir[dl] = 0;
        if (dl == 0) strcpy(dir, "/");
        snprintf(base, sizeof base, "%s", slash + 1);
    } else {
        dir[0] = 0;
        snprintf(base, sizeof base, "%s", p);
    }
    if (ci_find(dir, base, hit, sizeof hit)) {
        if (dir[0]) snprintf(out, n, "%s/%s", strcmp(dir, "/") ? dir : "", hit);
        else snprintf(out, n, "%s", hit);
        return 1;
    }
    snprintf(out, n, "%s", p);
    return 0;
}

FHANDLE FileSave(const char *name)
{
    char p[1024];
    resolve(name, p, sizeof p);
    /* OPEN_ALWAYS: open without truncating, create if missing */
    FILE *f = fopen(p, "r+b");
    if (!f) f = fopen(p, "w+b");
    return f ? (FHANDLE)f : BAD_HANDLE;
}

FHANDLE FileOpen(const char *name)
{
    char p[1024];
    if (!resolve(name, p, sizeof p)) return BAD_HANDLE;
    FILE *f = fopen(p, "rb");
    return f ? (FHANDLE)f : BAD_HANDLE;
}

void FileClose(FHANDLE fh)
{
    if (fh && fh != BAD_HANDLE) fclose((FILE *)fh);
}

uint32_t FileRead(FHANDLE fh, void *buffer, uint32_t size)
{
    if (!fh || fh == BAD_HANDLE) return 0;
    return (uint32_t)fread(buffer, 1, size, (FILE *)fh);
}

uint32_t FileWrite(FHANDLE fh, const void *buffer, uint32_t size)
{
    if (!fh || fh == BAD_HANDLE) return 0;
    return (uint32_t)fwrite(buffer, 1, size, (FILE *)fh);
}

void FileSeek(FHANDLE fh, long size, int origin)
{
    if (!fh || fh == BAD_HANDLE) return;
    fseek((FILE *)fh, size, origin == 0 ? SEEK_SET : origin == 1 ? SEEK_CUR : SEEK_END);
}

uint32_t FileSize(FHANDLE fh)
{
    if (!fh || fh == BAD_HANDLE) return 0;
    FILE *f = (FILE *)fh;
    long cur = ftell(f);
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, cur, SEEK_SET);
    return (uint32_t)sz;
}

int FileDelete(const char *name)
{
    char p[1024];
    if (!resolve(name, p, sizeof p)) return 0;
    return unlink(p) == 0;
}
