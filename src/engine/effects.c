/*
 * effects.c - screen transitions and palette fades (effects.obj)
 */
#include <stdlib.h>
#include "safari.h"

static const int pftbl[16][2] = {
    {0, 0}, {12, 8}, {0, 12}, {8, 0}, {8, 12}, {4, 4}, {4, 12}, {12, 0},
    {0, 8}, {4, 0}, {8, 8}, {8, 4}, {12, 12}, {12, 4}, {4, 8}, {0, 4},
};

static void tile(int x, int y)
{
    MGL_bitBltCoord(frontpage, backpage, x * 32, y * 24, x * 32 + 32, y * 24 + 24, x * 32, y * 24, 0);
}

void SpiralWipe(void)
{
    int num = 1, x = 10, y = 9;
    MGL_bitBltCoord(frontpage, backpage, 320, 216, 352, 240, 320, 216, 0);
    for (int i = 0; i < 10; i++) {
        TimeDelay(0);
        for (int j = 0; j < num; j++) { y++; tile(x, y); }
        for (int j = 0; j < num; j++) { x--; tile(x, y); }
        for (int j = 0; j < num + 1; j++) { y--; tile(x, y); }
        for (int j = 0; j < num + 1; j++) { x++; tile(x, y); }
        TimeDelay(40);
        num += 2;
    }
}

void DiagonalWipe(void)
{
    int num = 1;
    for (int i = 0; i < 20; i++) {
        TimeDelay(0);
        int x = 0, y = i;
        for (int j = 0; j < num; j++) { tile(x, y); x++; y--; }
        TimeDelay(10);
        num++;
    }
    num = 19;
    for (int i = 1; i < 20; i++) {
        TimeDelay(0);
        int x = i, y = 19;
        for (int j = 0; j < num; j++) { tile(x, y); x++; y--; }
        TimeDelay(10);
        num--;
    }
}

void DiagonalWipe2(void)
{
    int num = 1;
    for (int i = 19; i >= 0; i--) {
        TimeDelay(0);
        int x = 0, y = i;
        for (int j = 0; j < num; j++) { tile(x, y); x++; y++; }
        TimeDelay(10);
        num++;
    }
    num = 19;
    for (int i = 1; i < 20; i++) {
        TimeDelay(0);
        int x = i, y = 0;
        for (int j = 0; j < num; j++) { tile(x, y); x++; y++; }
        TimeDelay(10);
        num--;
    }
}

void FadePalette(int intensity)
{
    palette_t quadpal[256];
    gamepalette[0] = gamepalette[1] = gamepalette[2] = 0;
    for (int i = 0, j = 0; i < 256; i++, j += 3) {
        quadpal[i].red = gamepalette[j];
        quadpal[i].green = gamepalette[j + 1];
        quadpal[i].blue = gamepalette[j + 2];
        quadpal[i].alpha = 0;
    }
    MGL_fadePalette(frontpage, quadpal, 256, 0, (uint8_t)intensity);
    MGL_realizePalette(frontpage, 256, 0, 1);
}

void DissolveArea(int xpos, int ypos, int width, int height)
{
    int w = ((width + 15) / 16) * 16;
    int h = ((height + 15) / 16) * 16;
    if (xpos < 641 && ypos < 481 && w < 641 && h < 481) {
        for (int t = 0; t < 16; t++) {
            TimeDelay(0);
            for (int y = 0; y < h; y += 16)
                for (int x = 0; x < w; x += 16) {
                    int px = pftbl[t][0] + xpos + x;
                    int py = pftbl[t][1] + y + ypos;
                    MGL_bitBltCoord(frontpage, backpage, px, py, px + 4, py + 4, px, py, 0);
                }
            TimeDelay(10);
        }
    } else {
        Log("ERROR: DissolveArea(%d,%d,%d,%d)", xpos, ypos, width, height);
    }
}

void RandomWipe(void)
{
    switch (rand() % 3) {
    case 0: SpiralWipe(); break;
    case 1: DiagonalWipe(); break;
    case 2: DiagonalWipe2(); break;
    }
}

void Checkerboard(void)
{
    uchar *ptr = workpage->surface;
    long end = (long)workpage->bytesPerLine * workpage->height;
    int h = 480;
    while (h != 0) {
        h--;
        if ((h & 1) == 0) ptr++;
        else ptr--;
        for (int x = 0; x < 640; x++) {
            long o = ptr - workpage->surface;
            if ((x & 1) && o >= 0 && o < end) *ptr = 0;
            ptr++;
        }
    }
}

void CheckerboardArea(int xpos, int ypos, int wide, int high)
{
    uchar *base = backpage->surface;
    long end = (long)backpage->bytesPerLine * backpage->height;
    long row = (long)ypos * 640 + xpos;
    long p = row;
    while (high != 0) {
        high--;
        for (int x = xpos; x < wide + xpos; x++) {
            if ((x & 1) && p >= 0 && p < end) base[p] = 0;
            p++;
        }
        row += 640;
        p = row;
        if (high & 1) p = row + 1;
    }
}
