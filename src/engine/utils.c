/*
 * utils.c - timing and debug logging (Utils.obj)
 */
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include "safari.h"

static uint32_t StartTime;
static int bFirstWrite = 1;
static int logLines;

/* AIL_ms_count(): milliseconds.  Also services the window, because the
 * game polls this in tight loops while fading and waiting. */
uint32_t GetLapCount(void)
{
    sys_pump();
    return sys_ms();
}

void TimeDelay(uint32_t how_long)
{
    if (how_long == 0) {
        StartTime = sys_ms();
    } else {
        while (GetLapCount() - StartTime <= how_long) {}
    }
}

void DebugStr(const char *format, ...)
{
    char szBuffer[256];
    va_list ap;
    va_start(ap, format);
    vsnprintf(szBuffer, sizeof szBuffer, format, ap);
    va_end(ap);
    MGLDC *old = MGL_makeCurrentDC(frontpage);
    MGL_setColor((uint32_t)colorBlack);
    MGL_fillRectCoord(0, 0, 640, 20);
    MGL_makeCurrentDC(old);
    FontDraw(10, 0, szBuffer, frontpage);
}

/* The original appended to C:\OQUIRRH.TXT (max 100 lines per run). */
void Log(const char *format, ...)
{
    char szBuffer[512];
    va_list ap;
    va_start(ap, format);
    vsnprintf(szBuffer, sizeof szBuffer, format, ap);
    va_end(ap);
    if (getenv("SAFARI_DEBUG")) fprintf(stderr, "%s\n", szBuffer);
    if (logLines > 100) return;
    char path[1100];
    snprintf(path, sizeof path, "%s/oquirrh.txt", savePath[0] ? savePath : ".");
    FILE *f = fopen(path, bFirstWrite ? "w" : "a");
    bFirstWrite = 0;
    if (!f) return;
    fprintf(f, "%s\r\n", szBuffer);
    fclose(f);
    logLines++;
}

void *SafeMalloc(size_t n)
{
    return calloc(1, n ? n : 1);
}

static uint32_t holdrand = 1;
#undef rand
#undef srand
void msvc_srand(unsigned int seed) { holdrand = seed; }
int msvc_rand(void)
{
    holdrand = holdrand * 214013u + 2531011u;
    return (int)((holdrand >> 16) & 0x7fff);
}

/* PORT: the original scrolled once per pass through its main loop, which a
 * 1998 PC ran at roughly 30 frames a second.  A modern machine runs that
 * loop hundreds of times faster, so edge scrolling is paced here. */
int ScrollTick(void)
{
    static uint32_t last;
    uint32_t now = sys_ms();
    if (now - last < 30) return 0;
    last = now;
    return 1;
}
