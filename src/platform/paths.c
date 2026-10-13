/*
 * paths.c - program entry, data/save directories and first-run import of
 * the game data from the original CD (ISO image or mounted disc).
 *
 * No game assets ship with this port: on first launch the files in the
 * CD's \Safari folder are copied into $XDG_DATA_HOME/browning-safari/data.
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <errno.h>
#include <dirent.h>
#include <unistd.h>
#include <glob.h>
#include <sys/stat.h>
#include <SDL.h>
#include "safari.h"

char savePath[1024];
static char dataDir[1024];

int SafariMain(void);

/* ------------------------------------------------------------ helpers -- */
static int mkdirs(const char *path)
{
    char tmp[1024];
    snprintf(tmp, sizeof tmp, "%s", path);
    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = 0;
            if (mkdir(tmp, 0755) != 0 && errno != EEXIST) return 0;
            *p = '/';
        }
    }
    return mkdir(tmp, 0755) == 0 || errno == EEXIST;
}

static int isDir(const char *p) { struct stat st; return stat(p, &st) == 0 && S_ISDIR(st.st_mode); }
static int isFile(const char *p) { struct stat st; return stat(p, &st) == 0 && S_ISREG(st.st_mode); }

static int haveData(const char *dir)
{
    char p[1100];
    snprintf(p, sizeof p, "%s/fonts.dat", dir);
    if (!isFile(p)) return 0;
    snprintf(p, sizeof p, "%s/bkg_01.256", dir);
    return isFile(p);
}

static int skipFile(const char *name)
{
    const char *dot = strrchr(name, '.');
    if (!dot) return 0;
    return !strcasecmp(dot, ".exe") || !strcasecmp(dot, ".pdb") || !strcasecmp(dot, ".ilk") ||
           !strcasecmp(dot, ".doc");
}

static void lower(char *s) { for (; *s; s++) *s = (char)tolower((unsigned char)*s); }

/* progress reporting through zenity when available */
static FILE *progress;
static void progressOpen(void)
{
    if (getenv("DISPLAY") || getenv("WAYLAND_DISPLAY")) {
        if (system("command -v zenity >/dev/null 2>&1") == 0)
            progress = popen("zenity --progress --auto-close --no-cancel --title='Browning African Safari' "
                             "--text='Copying game data from the CD...' 2>/dev/null", "w");
    }
}
static void progressSet(int pct)
{
    if (progress) { fprintf(progress, "%d\n", pct); fflush(progress); }
    fprintf(stderr, "\rImporting game data... %3d%%", pct);
}
static void progressClose(void)
{
    if (progress) { pclose(progress); progress = NULL; }
    fprintf(stderr, "\n");
}

/* ------------------------------------------------------ ISO 9660 read -- */
typedef struct { uint32_t lba, size; int dir; char name[256]; } isoent;

static uint32_t le32(const uint8_t *p) { return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24; }

static int isoReadDir(FILE *f, uint32_t lba, uint32_t size, int joliet, isoent **out, int *n)
{
    uint8_t *buf = malloc(size);
    if (!buf) return 0;
    if (fseeko(f, (off_t)lba * 2048, SEEK_SET) != 0 || fread(buf, 1, size, f) != size) { free(buf); return 0; }
    *out = NULL; *n = 0;
    uint32_t o = 0;
    while (o < size) {
        uint8_t len = buf[o];
        if (len == 0) { o = (o / 2048 + 1) * 2048; continue; }
        uint8_t nl = buf[o + 32];
        const uint8_t *nm = buf + o + 33;
        if (!(nl == 1 && (nm[0] == 0 || nm[0] == 1))) {
            isoent e;
            e.lba = le32(buf + o + 2);
            e.size = le32(buf + o + 10);
            e.dir = (buf[o + 25] & 2) != 0;
            int k = 0;
            if (joliet) {
                for (int i = 0; i + 1 < nl && k < 255; i += 2) {
                    unsigned c = (unsigned)(nm[i] << 8 | nm[i + 1]);
                    e.name[k++] = c < 128 ? (char)c : '_';
                }
            } else {
                for (int i = 0; i < nl && k < 255; i++) e.name[k++] = (char)nm[i];
            }
            e.name[k] = 0;
            char *semi = strchr(e.name, ';');
            if (semi) *semi = 0;
            size_t L = strlen(e.name);
            if (L && e.name[L - 1] == '.') e.name[L - 1] = 0;
            *out = realloc(*out, sizeof(isoent) * (*n + 1));
            (*out)[(*n)++] = e;
        }
        o += len;
    }
    free(buf);
    return 1;
}

static int importIso(const char *iso, const char *dest)
{
    FILE *f = fopen(iso, "rb");
    if (!f) { fprintf(stderr, "cannot open %s\n", iso); return 0; }
    uint8_t vd[2048];
    uint32_t rootLba = 0, rootSize = 0;
    int joliet = 0;
    for (int s = 16; s < 64; s++) {
        if (fseeko(f, (off_t)s * 2048, SEEK_SET) != 0 || fread(vd, 1, 2048, f) != 2048) break;
        if (memcmp(vd + 1, "CD001", 5) != 0) break;
        if (vd[0] == 255) break;
        if (vd[0] == 1 && !rootLba) { rootLba = le32(vd + 156 + 2); rootSize = le32(vd + 156 + 10); }
        if (vd[0] == 2 && vd[88] == '%' && vd[89] == '/' && (vd[90] == '@' || vd[90] == 'C' || vd[90] == 'E')) {
            rootLba = le32(vd + 156 + 2); rootSize = le32(vd + 156 + 10); joliet = 1;
        }
    }
    if (!rootLba) { fclose(f); fprintf(stderr, "%s is not an ISO 9660 image\n", iso); return 0; }
    isoent *root; int nroot;
    if (!isoReadDir(f, rootLba, rootSize, joliet, &root, &nroot)) { fclose(f); return 0; }
    isoent *sd = NULL;
    for (int i = 0; i < nroot; i++) if (root[i].dir && !strcasecmp(root[i].name, "Safari")) sd = &root[i];
    if (!sd) { free(root); fclose(f); fprintf(stderr, "no Safari folder in %s: not the African Safari CD?\n", iso); return 0; }
    isoent *files; int nf;
    if (!isoReadDir(f, sd->lba, sd->size, joliet, &files, &nf)) { free(root); fclose(f); return 0; }
    uint64_t total = 0, doneB = 0;
    for (int i = 0; i < nf; i++) if (!files[i].dir && !skipFile(files[i].name)) total += files[i].size;
    progressOpen();
    int ok = 1;
    uint8_t *buf = malloc(1 << 20);
    for (int i = 0; i < nf && ok; i++) {
        if (files[i].dir || skipFile(files[i].name)) continue;
        char name[256], out[1400];
        snprintf(name, sizeof name, "%s", files[i].name);
        lower(name);
        snprintf(out, sizeof out, "%s/%s", dest, name);
        FILE *o = fopen(out, "wb");
        if (!o) { ok = 0; break; }
        fseeko(f, (off_t)files[i].lba * 2048, SEEK_SET);
        uint32_t left = files[i].size;
        while (left) {
            uint32_t n = left > (1u << 20) ? (1u << 20) : left;
            if (fread(buf, 1, n, f) != n || fwrite(buf, 1, n, o) != n) { ok = 0; break; }
            left -= n;
            doneB += n;
            progressSet(total ? (int)(doneB * 100 / total) : 100);
        }
        fclose(o);
    }
    free(buf);
    progressClose();
    free(files);
    free(root);
    fclose(f);
    return ok;
}

/* copy from a mounted disc / extracted folder (the CD root or its Safari folder) */
static int importDir(const char *src, const char *dest)
{
    char sdir[1100];
    snprintf(sdir, sizeof sdir, "%s", src);
    if (!haveData(sdir)) {
        DIR *d = opendir(src);
        int found = 0;
        if (d) {
            struct dirent *e;
            while ((e = readdir(d)))
                if (!strcasecmp(e->d_name, "Safari")) {
                    snprintf(sdir, sizeof sdir, "%s/%s", src, e->d_name);
                    found = 1;
                    break;
                }
            closedir(d);
        }
        if (!found) { fprintf(stderr, "no Safari folder found in %s\n", src); return 0; }
    }
    DIR *d = opendir(sdir);
    if (!d) return 0;
    struct dirent *e;
    uint64_t total = 0, doneB = 0;
    struct stat st;
    char p[1400];
    while ((e = readdir(d))) {
        snprintf(p, sizeof p, "%s/%s", sdir, e->d_name);
        if (isFile(p) && !skipFile(e->d_name) && stat(p, &st) == 0) total += (uint64_t)st.st_size;
    }
    rewinddir(d);
    progressOpen();
    int ok = 1;
    uint8_t *buf = malloc(1 << 20);
    while (ok && (e = readdir(d))) {
        snprintf(p, sizeof p, "%s/%s", sdir, e->d_name);
        if (!isFile(p) || skipFile(e->d_name)) continue;
        char name[256], out[1400];
        snprintf(name, sizeof name, "%s", e->d_name);
        lower(name);
        snprintf(out, sizeof out, "%s/%s", dest, name);
        FILE *in = fopen(p, "rb"), *o = fopen(out, "wb");
        if (!in || !o) { ok = 0; if (in) fclose(in); if (o) fclose(o); break; }
        size_t n;
        while ((n = fread(buf, 1, 1 << 20, in)) > 0) {
            if (fwrite(buf, 1, n, o) != n) { ok = 0; break; }
            doneB += n;
            progressSet(total ? (int)(doneB * 100 / total) : 100);
        }
        fclose(in);
        fclose(o);
    }
    free(buf);
    closedir(d);
    progressClose();
    return ok;
}

static int importFrom(const char *src)
{
    if (!mkdirs(dataDir)) { fprintf(stderr, "cannot create %s\n", dataDir); return 0; }
    int ok = isDir(src) ? importDir(src, dataDir) : importIso(src, dataDir);
    if (ok && haveData(dataDir)) {
        fprintf(stderr, "Game data imported into %s\n", dataDir);
        return 1;
    }
    fprintf(stderr, "Import failed.\n");
    return 0;
}

/* look for an inserted/mounted CD */
static int findMountedCD(char *out, size_t n)
{
    const char *pats[] = {"/run/media/*/*/Safari", "/run/media/*/*/SAFARI", "/media/*/*/Safari",
                          "/media/*/Safari", "/mnt/*/Safari", NULL};
    for (int i = 0; pats[i]; i++) {
        glob_t g;
        if (glob(pats[i], 0, NULL, &g) == 0) {
            for (size_t k = 0; k < g.gl_pathc; k++)
                if (haveData(g.gl_pathv[k])) {
                    snprintf(out, n, "%s", g.gl_pathv[k]);
                    globfree(&g);
                    return 1;
                }
            globfree(&g);
        }
    }
    return 0;
}

static int askForSource(char *out, size_t n)
{
    const char *cmds[] = {
        "zenity --file-selection --title='Select the African Safari CD image (SAFARI.iso)' "
        "--file-filter='CD images | *.iso *.ISO' --file-filter='All files | *' 2>/dev/null",
        "kdialog --title 'Select the African Safari CD image (SAFARI.iso)' --getopenfilename ~ "
        "'*.iso *.ISO|CD images' 2>/dev/null",
        NULL};
    for (int i = 0; cmds[i]; i++) {
        char which[64];
        snprintf(which, sizeof which, "command -v %s >/dev/null 2>&1", i == 0 ? "zenity" : "kdialog");
        if (system(which) != 0) continue;
        FILE *p = popen(cmds[i], "r");
        if (!p) continue;
        char line[1024] = {0};
        char *r = fgets(line, sizeof line, p);
        pclose(p);
        if (!r) return 0;
        line[strcspn(line, "\r\n")] = 0;
        if (line[0]) { snprintf(out, n, "%s", line); return 1; }
        return 0;
    }
    return -1;
}

static void usage(void)
{
    printf("The Browning African Safari - native Linux port\n\n"
           "usage: browning-safari [options]\n"
           "  --import PATH    copy the game data from SAFARI.iso, a mounted CD or a folder\n"
           "  --data DIR       use game data from DIR (default: %s)\n"
           "  --fullscreen     start in fullscreen (Alt+Enter or F11 toggles)\n"
           "  --8bit-sound     use the CD's 8-bit 11 kHz sound set\n"
           "  --nosound        disable audio\n"
           "  --debug          verbose log on stderr\n", dataDir);
}

int main(int argc, char **argv)
{
    const char *xdg = getenv("XDG_DATA_HOME");
    const char *home = getenv("HOME");
    if (xdg && *xdg) snprintf(savePath, sizeof savePath, "%s/browning-safari", xdg);
    else snprintf(savePath, sizeof savePath, "%s/.local/share/browning-safari", home ? home : ".");
    snprintf(dataDir, sizeof dataDir, "%s/data", savePath);
    const char *envData = getenv("SAFARI_DATA");
    if (envData && *envData) snprintf(dataDir, sizeof dataDir, "%s", envData);

    const char *importSrc = NULL;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--import") && i + 1 < argc) importSrc = argv[++i];
        else if (!strcmp(argv[i], "--data") && i + 1 < argc) snprintf(dataDir, sizeof dataDir, "%s", argv[++i]);
        else if (!strcmp(argv[i], "--fullscreen")) setenv("SAFARI_FULLSCREEN", "1", 1);
        else if (!strcmp(argv[i], "--8bit-sound")) init_8bitsound = 1;
        else if (!strcmp(argv[i], "--nosound")) setenv("SAFARI_NOSOUND", "1", 1);
        else if (!strcmp(argv[i], "--debug")) { setenv("SAFARI_DEBUG", "1", 1); debug = 1; }
        else if (!strcmp(argv[i], "--help") || !strcmp(argv[i], "-h")) { usage(); return 0; }
        else if (argv[i][0] != '-' && !importSrc && !haveData(dataDir)) importSrc = argv[i];
    }
    mkdirs(savePath);

    if (importSrc) {
        if (!importFrom(importSrc)) return 1;
        if (argc == 3 && !strcmp(argv[1], "--import")) return 0;   /* import only */
    }

    if (!haveData(dataDir)) {
        char src[1024];
        if (findMountedCD(src, sizeof src)) {
            fprintf(stderr, "Found the CD at %s\n", src);
            if (!importFrom(src)) return 1;
        } else {
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, "Browning African Safari",
                "The game data has not been installed yet.\n\n"
                "Select your SAFARI.iso image (or insert the original CD and try again).\n"
                "The data is copied once to ~/.local/share/browning-safari.", NULL);
            int r = askForSource(src, sizeof src);
            if (r == 1) {
                if (!importFrom(src)) {
                    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Browning African Safari",
                        "That file does not look like the African Safari CD.", NULL);
                    return 1;
                }
            } else {
                if (r < 0)
                    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, "Browning African Safari",
                        "Run this once from a terminal to install the game data:\n\n"
                        "    browning-safari --import /path/to/SAFARI.iso", NULL);
                return 1;
            }
        }
    }

    /* the game addresses its data as cdPath + name, saves relative to cwd */
    snprintf(cdPath, sizeof cdPath, "%s/", dataDir);
    snprintf(basPath, sizeof basPath, "%s/", savePath);
    if (chdir(savePath) != 0) { fprintf(stderr, "cannot enter %s\n", savePath); return 1; }
    if (!isFile("options.dat")) {
        char src[1100];
        snprintf(src, sizeof src, "%s/options.dat", dataDir);
        FILE *in = fopen(src, "rb");
        if (in) {
            FILE *o = fopen("options.dat", "wb");
            char b[4096];
            size_t k;
            while (o && (k = fread(b, 1, sizeof b, in)) > 0) fwrite(b, 1, k, o);
            if (o) fclose(o);
            fclose(in);
        }
    }

    if (!sys_init()) return 1;
    int rc = SafariMain();
    if (rc == 2)
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Browning African Safari",
                                 "Please place African Safari CD into computer", NULL);
    sys_shutdown();
    return rc == 2 ? 1 : rc;
}
