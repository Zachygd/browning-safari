/*
 * mgl.c - SciTech MGL subset on plain memory surfaces.
 */
#include <stdlib.h>
#include <string.h>
#include "mgl.h"

static MGLDC   *curDC;
static uint32_t curColor;

static MGLDC *newDC(int w, int h)
{
    MGLDC *dc = calloc(1, sizeof(MGLDC));
    if (!dc) return NULL;
    dc->surface = calloc((size_t)w * h, 1);
    if (!dc->surface) { free(dc); return NULL; }
    dc->width = w; dc->height = h;
    dc->maxx = w - 1; dc->maxy = h - 1;
    dc->bytesPerLine = w;
    dc->clipL = 0; dc->clipT = 0; dc->clipR = w; dc->clipB = h;
    return dc;
}

MGLDC *MGL_createDisplayDC(void)
{
    MGLDC *dc = newDC(640, 480);
    if (dc) dc->isDisplay = 1;
    return dc;
}

MGLDC *MGL_createMemoryDC(int xres, int yres) { return newDC(xres, yres); }

void MGL_destroyDC(MGLDC *dc)
{
    if (!dc) return;
    if (curDC == dc) curDC = NULL;
    free(dc->surface);
    free(dc);
}

MGLDC *MGL_makeCurrentDC(MGLDC *dc)
{
    MGLDC *old = curDC;
    curDC = dc;
    return old;
}

void MGL_setClipRectDC(MGLDC *dc, int l, int t, int r, int b)
{
    if (l < 0) l = 0;
    if (t < 0) t = 0;
    if (r > dc->width) r = dc->width;
    if (b > dc->height) b = dc->height;
    dc->clipL = l; dc->clipT = t; dc->clipR = r; dc->clipB = b;
}

/* Clip a blit of src rect (l,t,r,b) to dst (dl,dt) against both DCs, like MGL does. */
static int clipBlt(MGLDC *dst, MGLDC *src, int *l, int *t, int *r, int *b, int *dl, int *dt)
{
    int L = *l, T = *t, R = *r, B = *b, DL = *dl, DT = *dt;
    /* source clip */
    if (L < src->clipL) { DL += src->clipL - L; L = src->clipL; }
    if (T < src->clipT) { DT += src->clipT - T; T = src->clipT; }
    if (R > src->clipR) R = src->clipR;
    if (B > src->clipB) B = src->clipB;
    if (R <= L || B <= T) return 0;
    /* destination clip */
    if (DL < dst->clipL) { L += dst->clipL - DL; DL = dst->clipL; }
    if (DT < dst->clipT) { T += dst->clipT - DT; DT = dst->clipT; }
    if (DL + (R - L) > dst->clipR) R = L + (dst->clipR - DL);
    if (DT + (B - T) > dst->clipB) B = T + (dst->clipB - DT);
    if (R <= L || B <= T) return 0;
    *l = L; *t = T; *r = R; *b = B; *dl = DL; *dt = DT;
    return 1;
}

void MGL_bitBltCoord(MGLDC *dst, MGLDC *src, int left, int top, int right, int bottom,
                     int dstLeft, int dstTop, int op)
{
    (void)op;
    if (!dst || !src) return;
    if (!clipBlt(dst, src, &left, &top, &right, &bottom, &dstLeft, &dstTop)) return;
    int w = right - left, h = bottom - top;
    if (dst == src && dstTop > top) {
        for (int y = h - 1; y >= 0; y--)
            memmove(dst->surface + (dstTop + y) * dst->bytesPerLine + dstLeft,
                    src->surface + (top + y) * src->bytesPerLine + left, w);
    } else {
        for (int y = 0; y < h; y++)
            memmove(dst->surface + (dstTop + y) * dst->bytesPerLine + dstLeft,
                    src->surface + (top + y) * src->bytesPerLine + left, w);
    }
    if (dst->isDisplay) sys_mark_dirty();
}

void MGL_transBltCoord(MGLDC *dst, MGLDC *src, int left, int top, int right, int bottom,
                       int dstLeft, int dstTop, uint32_t transparent, int sourceTrans)
{
    if (!dst || !src || dst == src) return;
    if (!clipBlt(dst, src, &left, &top, &right, &bottom, &dstLeft, &dstTop)) return;
    int w = right - left, h = bottom - top;
    uint8_t tc = (uint8_t)transparent;
    for (int y = 0; y < h; y++) {
        uint8_t *s = src->surface + (top + y) * src->bytesPerLine + left;
        uint8_t *d = dst->surface + (dstTop + y) * dst->bytesPerLine + dstLeft;
        for (int x = 0; x < w; x++) {
            if (sourceTrans) { if (s[x] != tc) d[x] = s[x]; }
            else             { if (d[x] == tc) d[x] = s[x]; }
        }
    }
    if (dst->isDisplay) sys_mark_dirty();
}

void MGL_stretchBltCoord(MGLDC *dst, MGLDC *src, int left, int top, int right, int bottom,
                         int dstLeft, int dstTop, int dstRight, int dstBottom)
{
    if (!dst || !src || dst == src) return;
    int sw = right - left, sh = bottom - top;
    int dw = dstRight - dstLeft, dh = dstBottom - dstTop;
    if (sw <= 0 || sh <= 0 || dw <= 0 || dh <= 0) return;
    /* 16.16 fixed point stepping through the source, as MGL does */
    int32_t xstep = (int32_t)(((int64_t)sw << 16) / dw);
    int32_t ystep = (int32_t)(((int64_t)sh << 16) / dh);
    int32_t sy = 0;
    for (int y = 0; y < dh; y++, sy += ystep) {
        int dy = dstTop + y;
        int srow = top + (sy >> 16);
        if (dy < dst->clipT || dy >= dst->clipB) continue;
        if (srow < src->clipT || srow >= src->clipB) continue;
        uint8_t *s = src->surface + srow * src->bytesPerLine;
        uint8_t *d = dst->surface + dy * dst->bytesPerLine;
        int32_t sx = 0;
        for (int x = 0; x < dw; x++, sx += xstep) {
            int dx = dstLeft + x;
            int scol = left + (sx >> 16);
            if (dx < dst->clipL || dx >= dst->clipR) continue;
            if (scol < src->clipL || scol >= src->clipR) continue;
            d[dx] = s[scol];
        }
    }
    if (dst->isDisplay) sys_mark_dirty();
}

void MGL_setColor(uint32_t color) { curColor = color; }

void MGL_fillRectCoord(int left, int top, int right, int bottom)
{
    MGLDC *dc = curDC;
    if (!dc) return;
    if (left < dc->clipL) left = dc->clipL;
    if (top < dc->clipT) top = dc->clipT;
    if (right > dc->clipR) right = dc->clipR;
    if (bottom > dc->clipB) bottom = dc->clipB;
    if (right <= left || bottom <= top) return;
    for (int y = top; y < bottom; y++)
        memset(dc->surface + y * dc->bytesPerLine + left, (uint8_t)curColor, right - left);
    if (dc->isDisplay) sys_mark_dirty();
}

static void putPixel(MGLDC *dc, int x, int y)
{
    if (x < dc->clipL || x >= dc->clipR || y < dc->clipT || y >= dc->clipB) return;
    dc->surface[y * dc->bytesPerLine + x] = (uint8_t)curColor;
}

void MGL_lineCoord(int x1, int y1, int x2, int y2)
{
    MGLDC *dc = curDC;
    if (!dc) return;
    int dx = abs(x2 - x1), sx = x1 < x2 ? 1 : -1;
    int dy = -abs(y2 - y1), sy = y1 < y2 ? 1 : -1;
    int err = dx + dy;
    for (;;) {
        putPixel(dc, x1, y1);
        if (x1 == x2 && y1 == y2) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x1 += sx; }
        if (e2 <= dx) { err += dx; y1 += sy; }
    }
    if (dc->isDisplay) sys_mark_dirty();
}

/* MGL_rectCoord: outline of [left,right) x [top,bottom) with a 1 pixel pen */
void MGL_rectCoord(int left, int top, int right, int bottom)
{
    if (top + 1 < bottom && left + 1 < right) {
        MGL_lineCoord(left, top, right - 1, top);
        MGL_lineCoord(left, bottom - 1, right - 1, bottom - 1);
        MGL_lineCoord(left, top + 1, left, bottom - 2);
        MGL_lineCoord(right - 1, top + 1, right - 1, bottom - 2);
    }
}

void MGL_clearDevice(void)
{
    MGLDC *dc = curDC;
    if (!dc) return;
    memset(dc->surface, 0, (size_t)dc->bytesPerLine * dc->height);
    if (dc->isDisplay) sys_mark_dirty();
}

void MGL_setPalette(MGLDC *dc, const palette_t *pal, int numColors, int startIndex)
{
    if (!dc) return;
    for (int i = startIndex; i < startIndex + numColors && i < 256; i++) {
        dc->pal[i] = pal[i];
        dc->pal[i].alpha = 0;
    }
}

void MGL_realizePalette(MGLDC *dc, int numColors, int startIndex, int waitVRT)
{
    (void)numColors; (void)startIndex; (void)waitVRT;
    if (dc && dc->isDisplay) sys_mark_dirty();
}

int MGL_fadePalette(MGLDC *dc, const palette_t *full, int numColors, int startIndex,
                    uint8_t intensity)
{
    int any = 0;
    for (int i = startIndex; i < startIndex + numColors && i < 256; i++) {
        uint8_t r = (uint8_t)((full[i].red * intensity) / 255);
        uint8_t g = (uint8_t)((full[i].green * intensity) / 255);
        uint8_t b = (uint8_t)((full[i].blue * intensity) / 255);
        dc->pal[i].red = r; dc->pal[i].green = g; dc->pal[i].blue = b;
        any |= r | g | b;
    }
    return any;
}

long MGL_divotSize(MGLDC *dc, int l, int t, int r, int b)
{
    (void)dc;
    if (r <= l || b <= t) return (long)sizeof(divot_t);
    return (long)(sizeof(divot_t) + (size_t)(r - l) * (b - t));
}

static void clampRect(MGLDC *dc, int *l, int *t, int *r, int *b)
{
    if (*l < 0) *l = 0;
    if (*t < 0) *t = 0;
    if (*r > dc->width) *r = dc->width;
    if (*b > dc->height) *b = dc->height;
}

void MGL_getDivot(MGLDC *dc, int l, int t, int r, int b, divot_t *d)
{
    int L = l, T = t, R = r, B = b;
    clampRect(dc, &L, &T, &R, &B);
    d->l = L; d->t = T; d->r = R; d->b = B;
    for (int y = T; y < B; y++)
        memcpy(d->data + (size_t)(y - T) * (R - L), dc->surface + y * dc->bytesPerLine + L, R - L);
}

void MGL_putDivot(MGLDC *dc, divot_t *d)
{
    for (int y = d->t; y < d->b; y++)
        memcpy(dc->surface + y * dc->bytesPerLine + d->l, d->data + (size_t)(y - d->t) * (d->r - d->l),
               d->r - d->l);
    if (dc->isDisplay) sys_mark_dirty();
}
