/*
 * blit.c - graphics loading and software blitters (blit.obj).
 *
 * Floating point: the original was compiled for x87 with 53-bit precision
 * and stores intermediates as 32-bit floats.  F() reproduces that: compute
 * in double, round to float on every store.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "safari.h"

#define F(x) ((float)(x))

GRAPHIC   graphic[MAX_GRAPHICS];
int       maxGraphics = MAX_GRAPHICS;
palette_t pal[256];
uchar     gamepalette[768];

static int dcW(MGLDC *p) { return p->maxx + 1; }
static int dcH(MGLDC *p) { return p->maxy + 1; }

void BlitGraphic(int num, int xpos, int ypos, MGLDC *page)
{
    int maxw = dcW(page), maxh = dcH(page);
    if (num > maxGraphics) { Log("BlitGraphic: num %d exceeds maximum", num); return; }
    int src_offset = 0;
    int w = graphic[num].width;
    int h = graphic[num].height;
    uchar *data = graphic[num].data;
    if (!data) { Log("BlitGraphic: Graphic %d, data = NULL", num); return; }
    xpos += graphic[num].hotx;
    ypos += graphic[num].hoty;
    if (w + xpos >= 0 && h + ypos >= 0 && xpos < maxw && ypos < maxh) {
        int width = w;
        if (xpos < 0) {
            width = w + (short)xpos;
            src_offset = -xpos;
            xpos = 0;
        }
        const uchar *src = data + src_offset;
        int bpl = page->bytesPerLine;
        int dst_offset = ypos * bpl + xpos;
        for (int y = 0; y < h; y++) {
            if (dst_offset >= 0) {
                uchar *lpDst = page->surface + dst_offset;
                for (int x = 0; x < width; x++)
                    if (x + xpos < maxw && src[x] != 0) lpDst[x] = src[x];
            }
            dst_offset += bpl;
            src += w;
            if (maxh * maxw <= dst_offset) break;
        }
        if (page->isDisplay) sys_mark_dirty();
    }
}

void BlitGraphic2(int num, int xpos, int ypos, int height, MGLDC *page)
{
    if (num > maxGraphics) { Log("BlitGraphic2: num %d exceeds maximum", num); return; }
    int w = graphic[num].width;
    const uchar *src = graphic[num].data;
    if (!src) return;
    int bpl = page->bytesPerLine;
    int dst_offset = ypos * bpl + xpos;
    long limit = (long)page->bytesPerLine * page->height;
    for (int y = 0; y < height; y++) {
        if (dst_offset >= 0 && dst_offset + w <= limit)
            memcpy(page->surface + dst_offset, src, w);
        dst_offset += bpl;
        src += w;
    }
    if (page->isDisplay) sys_mark_dirty();
}

void BlitGraphicLayer(int num, int xpos, int ypos, int zpos)
{
    uchar *z_map = layer_map;
    if (num > maxGraphics) { Log("BlitGraphicLayer: num %d exceeds maximum", num); return; }
    if (!z_map) { Log("BlitGraphicLayer: z_map = NULL"); return; }
    int src_offset = 0;
    int w = graphic[num].width;
    int h = graphic[num].height;
    if (!graphic[num].data) { Log("BlitGraphicLayer: Graphic %d data = NULL", num); return; }
    xpos += graphic[num].hotx;
    ypos += graphic[num].hoty;
    if (xpos + w >= 0 && ypos + h >= 0 && xpos < 640 && ypos < 480) {
        int width = w;
        if (xpos < 0) { width = w + (short)xpos; src_offset = -xpos; xpos = 0; }
        if (ypos + h > 479) h = 480 - ypos;
        const uchar *src = graphic[num].data + src_offset;
        int bpl = backpage->bytesPerLine;
        int dst_offset = ypos * bpl + xpos;
        for (int y = 0; y < h; y++) {
            if (dst_offset >= 0) {
                uchar *lpDst = backpage->surface + dst_offset;
                for (int x = 0; x < width; x++)
                    if (xpos + x < 640 && z_map[dst_offset + x] < zpos && src[x] != 0)
                        lpDst[x] = src[x];
            }
            dst_offset += bpl;
            src += w;
            if (dst_offset > 0x4afff) break;
        }
    }
}

void BlitShadow(int num, int xpos, int ypos, uchar color, MGLDC *page)
{
    if (num > maxGraphics) { Log("BlitShadow: num %d exceeds maximum", num); return; }
    int src_offset = 0;
    int w = graphic[num].width;
    int h = graphic[num].height;
    uchar *data = graphic[num].data;
    if (!data) { Log("BlitShadow: Graphic %d, data = NULL", num); return; }
    xpos += graphic[num].hotx;
    ypos += graphic[num].hoty;
    if (xpos + w >= 0 && ypos + h >= 0 && xpos < 640 && ypos < 480) {
        int width = w;
        if (xpos < 0) { width = w + (short)xpos; src_offset = -xpos; xpos = 0; }
        const uchar *src = data + src_offset;
        int bpl = page->bytesPerLine;
        int dst_offset = ypos * bpl + xpos;
        for (int y = 0; y < h; y++) {
            if (dst_offset >= 0) {
                uchar *lpDst = page->surface + dst_offset;
                for (int x = 0; x < width; x++)
                    if (xpos + x < 640 && src[x] != 0) lpDst[x] = color;
            }
            dst_offset += bpl;
            src += w;
            if (dst_offset > 0x4afff) break;
        }
        if (page->isDisplay) sys_mark_dirty();
    }
}

/* Scaled, depth-tested sprite blit onto the back page. */
void ScaleGraphic(int num, int xpos, int ypos, int zpos, int percent, int flip)
{
    int src_offset = 0;
    if (percent <= 4) return;
    if (num > maxGraphics || num < 0) { Log("ScaleGraphic: num %d", num); return; }
    int w = graphic[num].width;
    int h = graphic[num].height;
    const uchar *src = graphic[num].data;
    if (!src) { Log("ScaleGraphic: graphic %d = NULL!", num); return; }
    uchar rowbuf[8192];
    uchar *z_map = layer_map;
    if (!z_map) { Log("ScaleGraphic: z_map = NULL"); return; }
    float xscale = F((double)(percent * w) / 100.0);
    float yscale = F((double)(percent * h) / 100.0);
    float x_step = F((double)w / xscale);
    float y_step = F((double)h / yscale);
    float y_index = 0.0f;
    int hotx;
    if (flip == 0) hotx = (graphic[num].hotx * percent) / 100;
    else hotx = -(((graphic[num].width + graphic[num].hotx) * percent) / 100);
    ypos += (graphic[num].hoty * percent) / 100;
    xpos += hotx;
    if (xpos + (int)xscale < 0 || ypos + (int)yscale < 0 || xpos >= 640 || ypos >= 480) return;
    if (xpos < 0) {
        xscale = F((double)xpos + xscale);
        src_offset = -((xpos * 100) / percent);
        xpos = 0;
    }
    if ((double)ypos + yscale >= 480.0) yscale = F(480 - ypos);
    int bpl = backpage->bytesPerLine;
    int dst_offset = bpl * ypos + xpos;
    for (int y = 0; y < h; y++) {
        if (flip == 0) { for (int n = 0; n < w; n++) rowbuf[n] = *src++; }
        else { for (int n = w - 1; n >= 0; n--) rowbuf[n] = *src++; }
        while ((int)y_index == y) {
            if (dst_offset >= 0) {
                uchar *lpDst = backpage->surface + dst_offset;
                float x_index = 0.0f;
                for (int x = 0; x < (int)xscale; x++) {
                    if (x + xpos >= 640) continue;
                    if (x + dst_offset > 0x4afff) return;
                    if (z_map[x + dst_offset] < zpos) {
                        uchar pixel = rowbuf[src_offset + (int)x_index];
                        if (pixel) lpDst[x] = pixel;
                    }
                    x_index = F((double)x_index + x_step);
                }
            }
            y_index = F((double)y_index + y_step);
            dst_offset += bpl;
            if (dst_offset >= 0x4b000) break;
        }
    }
}

void StretchGraphic(int num, int xpos, int ypos, int dwidth, int dheight)
{
    int src_offset = 0;
    if (num > maxGraphics || num < 0) { Log("StretchGraphic: num %d", num); return; }
    int w = graphic[num].width;
    int h = graphic[num].height;
    const uchar *src = graphic[num].data;
    if (!src) { Log("StretchGraphic: graphic %d = NULL!", num); return; }
    uchar rowbuf[8192];
    float xscale = F(dwidth);
    float yscale = F(dheight);
    float x_step = F((double)w / xscale);
    float y_step = F((double)h / yscale);
    float y_index = 0.0f;
    if (xpos + (int)xscale < 0 || ypos + (int)yscale < 0 || xpos >= 640) return;
    if (!((double)ypos - yscale < 480.0)) return;
    if (xpos < 0) {
        xscale = F((double)xpos + xscale);
        src_offset = -xpos;
        xpos = 0;
    }
    int bpl = backpage->bytesPerLine;
    int dst_offset = ypos * bpl + xpos;
    long limit = (long)bpl * backpage->height;
    for (int y = 0; y < h; y++) {
        for (int n = 0; n < w; n++) rowbuf[n] = *src++;
        while ((int)y_index == y) {
            if (dst_offset >= 0 && dst_offset < limit) {
                uchar *lpDst = backpage->surface + dst_offset;
                float x_index = 0.0f;
                for (int x = 0; x < (int)xscale; x++) {
                    if (x + xpos >= 640) continue;
                    uchar pixel = rowbuf[src_offset + (int)x_index];
                    if (pixel) lpDst[x] = pixel;
                    x_index = F((double)x_index + x_step);
                }
            }
            y_index = F((double)y_index + y_step);
            dst_offset += bpl;
        }
    }
}

/* draws the decoy onto the work page, hidden behind scenery nearer than 100 */
void BlitDecoy(int num, int xpos, int ypos, int percent)
{
    int src_offset = 0;
    int zpos = 100;
    int w = graphic[num].width;
    int h = graphic[num].height;
    const uchar *src = graphic[num].data;
    uchar rowbuf[8192];
    uchar *z_map = layer_map;
    if (!src) { Log("BlitDecoy: graphic %d = NULL!", num); return; }
    if (!z_map) { Log("BlitDecoy layer_map = NULL"); return; }
    float xscale = F((double)(w * percent) / 100.0);
    float yscale = F((double)(h * percent) / 100.0);
    float x_step = F((double)w / xscale);
    float y_step = F((double)h / yscale);
    float y_index = 0.0f;
    xpos += (graphic[num].hotx * percent) / 100;
    ypos += (graphic[num].hoty * percent) / 100;
    if (xpos + (int)xscale < 0 || ypos + (int)yscale < 0 || xpos >= 640 || ypos >= 480) return;
    if (xpos < 0) {
        xscale = F((double)xpos + xscale);
        src_offset = -((xpos * 100) / percent);
        xpos = 0;
    }
    if ((double)ypos + yscale >= 480.0) yscale = F(480 - ypos);
    int bpl = workpage->bytesPerLine;
    int dst_offset = ypos * 640 + xpos;
    for (int y = 0; y < h; y++) {
        for (int n = 0; n < w; n++) rowbuf[n] = *src++;
        while ((int)y_index == y) {
            if (dst_offset >= 0) {
                uchar *lpDst = workpage->surface + dst_offset;
                float x_index = 0.0f;
                for (int x = 0; x < (int)xscale; x++) {
                    if (x + xpos >= 640) continue;
                    if (z_map[x + dst_offset] < zpos) {
                        uchar pixel = rowbuf[src_offset + (int)x_index];
                        if (pixel) lpDst[x] = pixel;
                    }
                    x_index = F((double)x_index + x_step);
                }
            }
            y_index = F((double)y_index + y_step);
            dst_offset += bpl;
            if (dst_offset >= 0x4b000) break;
        }
    }
}

void BlitWorkToBack(void)
{
    MGL_bitBltCoord(backpage, workpage, 0, 0, 640, 480, 0, 0, 0);
}

void BlitBackToFront(void)
{
    MGL_bitBltCoord(frontpage, backpage, 0, 0, 640, 480, 0, 0, 0);
}

void BlitPageToPage(MGLDC *src, MGLDC *dst, int x, int y, int w, int h)
{
    MGL_bitBltCoord(dst, src, x, y, w + x, h + y, x, y, 0);
}

void SaveImage(MGLDC *page, uchar *Image, int sx, int sy, int w, int h)
{
    if (debug) Log("SaveImage(%d, %d, %d, %d)", sx, sy, w, h);
    if (sx < 0) sx = 0;
    if (sy < 0) sy = 0;
    int bpl = page->bytesPerLine;
    const uchar *src = page->surface + bpl * sy + sx;
    uchar *dst = Image;
    if (page->maxx + 1 < w + sx) w = page->maxx + 1 - sx;
    if (page->maxy + 1 < h + sy) h = page->maxy + 1 - sy;
    for (int i = 0; i < h; i++) {
        memcpy(dst, src, w);
        src += bpl;
        dst += w;
    }
}

void RestoreImage(uchar *Image, MGLDC *page, int dx, int dy, int w, int h)
{
    if (debug) Log("RestoreImage(%d, %d, %d, %d)", dx, dy, w, h);
    if (dx < 0) dx = 0;
    if (dy < 0) dy = 0;
    int bpl = page->bytesPerLine;
    const uchar *src = Image;
    uchar *dst = page->surface + bpl * dy + dx;
    if (page->maxx + 1 < w + dx) w = page->maxx + 1 - dx;
    if (page->maxy + 1 < h + dy) h = page->maxy + 1 - dy;
    for (int i = 0; i < h; i++) {
        memcpy(dst, src, w);
        dst += bpl;
        src += w;
    }
    if (page->isDisplay) sys_mark_dirty();
}

int LoadBackground(const char *file)
{
    uchar bitmapHdr[8], hdrpal[768], bitmapInfo[16];
    uchar *surface = workpage->surface;
    int bytesPerLine = workpage->bytesPerLine;
    if (debug) Log("LoadBackground(%s)", file);
    snprintf(filename, sizeof filename, "%s%s.256", cdPath, file);
    FHANDLE fh = FileOpen(filename);
    if (fh == BAD_HANDLE) { Log("ERROR: LoadBackground(%s) not found", filename); return 0; }
    FileRead(fh, bitmapHdr, 8);
    FileRead(fh, hdrpal, 0x300);
    FileRead(fh, bitmapInfo, 0x10);
    for (int y = 0; y < 480; y++) {
        FileRead(fh, surface, 640);
        surface += bytesPerLine;
    }
    FileClose(fh);
    memcpy(gamepalette, hdrpal, 0x300);
    return 1;
}

int LoadBackground2(const char *file)
{
    uchar bitmapHdr[0x308], bitmapInfo[16];
    uchar *surface = backpage->surface;
    int bytesPerLine = backpage->bytesPerLine;
    if (debug) Log("LoadBackground2(%s)", file);
    snprintf(filename, sizeof filename, "%s%s.256", cdPath, file);
    FHANDLE fh = FileOpen(filename);
    if (fh == BAD_HANDLE) { Log("ERROR: LoadBackground2(%s) not found", filename); return 0; }
    FileRead(fh, bitmapHdr, 0x308);
    FileRead(fh, bitmapInfo, 0x10);
    for (int y = 0; y < 480; y++) {
        FileRead(fh, surface, 640);
        surface += bytesPerLine;
    }
    FileClose(fh);
    return 1;
}

void readGraphicHeader(FHANDLE fh, GRAPHIC *g)
{
    uchar b[16];
    FileRead(fh, b, 16);
    g->size   = (int32_t)(b[0] | b[1] << 8 | b[2] << 16 | (uint32_t)b[3] << 24);
    g->width  = (int16_t)(b[4] | b[5] << 8);
    g->height = (int16_t)(b[6] | b[7] << 8);
    g->hotx   = (int16_t)(b[8] | b[9] << 8);
    g->hoty   = (int16_t)(b[10] | b[11] << 8);
    g->data   = NULL;
}

/* PCX-style RLE: 0xC0|n followed by value repeats value n times */
static void decodeRLE(const uchar *src, uchar *dst, int w, int h)
{
    uchar rowbuf[8192 + 64];
    int dstOffset = 0;
    for (int y = 0; y < h; y++) {
        int n = 0;
        do {
            uchar c = *src;
            if ((c & 0xc0) == 0xc0) {
                int j = c & 0x3f;
                c = src[1];
                src += 2;
                while (j-- > 0) rowbuf[n++] = c;
            } else {
                rowbuf[n++] = c;
                src++;
            }
        } while (n < w);
        memcpy(dst + dstOffset, rowbuf, w);
        dstOffset += w;
    }
}

int LoadGraphics(int start, const char *file)
{
    uchar hdr[8], hdrpal[768];
    if (debug) Log("LoadGraphics(%d, %s)", start, file);
    snprintf(filename, sizeof filename, "%s%s.256", cdPath, file);
    FHANDLE fh = FileOpen(filename);
    if (fh == BAD_HANDLE) { Log("ERROR: LoadGraphics(%s) not found", filename); return 0; }
    FileRead(fh, hdr, 8);
    short number = (short)(hdr[4] | hdr[5] << 8);
    char compression = (char)hdr[6];
    char format = (char)hdr[7];
    if (format != 3) {
        FileRead(fh, hdrpal, 0x300);
        hdrpal[0] = hdrpal[1] = hdrpal[2] = 0;
        for (int i = 0; i < 256; i++) {
            pal[i].red = hdrpal[i * 3];
            pal[i].green = hdrpal[i * 3 + 1];
            pal[i].blue = hdrpal[i * 3 + 2];
            pal[i].alpha = 0;
        }
    }
    if (maxGraphics < number + start) {
        FileClose(fh);
        Log("ERROR: LoadGraphics: file %s start %d + number %d exceeds maximum", file, start, number);
        return 0;
    }
    for (int i = start; i < number + start; i++) {
        if (graphic[i].data) {
            Log("ERROR: LoadGraphics: file %s, graphic %d not freed", file, i);
            free(graphic[i].data);
            graphic[i].data = NULL;
        }
        readGraphicHeader(fh, &graphic[i]);
        int w = graphic[i].width, h = graphic[i].height;
        if (compression == 0) {
            long size = (long)h * w;
            graphic[i].data = SafeMalloc(size);
            FileRead(fh, graphic[i].data, (uint32_t)size);
        } else if (compression == 4) {
            long size = (long)h * ((w + 7) / 8);
            graphic[i].data = SafeMalloc(size);
            FileRead(fh, graphic[i].data, (uint32_t)size);
        } else {
            long size = graphic[i].size;
            uchar *buffer = SafeMalloc(size + 128);
            FileRead(fh, buffer, (uint32_t)size);
            graphic[i].data = SafeMalloc((size_t)h * w + 64);
            decodeRLE(buffer, graphic[i].data, w, h);
            free(buffer);
        }
    }
    FileClose(fh);
    if (debug) Log("    read %d graphics", number);
    return number;
}

int SeekGraphics(const char *file, int start, int seek, int number)
{
    uchar hdr[8];
    uint32_t temp[4];
    if (debug) Log("SeekGraphics(%s, %d, %d, %d)", file, start, seek, number);
    snprintf(filename, sizeof filename, "%s%s.256", cdPath, file);
    FHANDLE fh = FileOpen(filename);
    if (fh == BAD_HANDLE) { Log("ERROR: SeekGraphics(%s) not found", filename); return 0; }
    FileRead(fh, hdr, 8);
    for (int i = 0; i < seek; i++) {
        FileRead(fh, temp, 0x10);
        FileSeek(fh, (long)temp[0], 1);
    }
    if (maxGraphics < number + start) {
        FileClose(fh);
        Log("ERROR: SeekGraphics: file %s start exceeds maximum", file);
        return 0;
    }
    for (int i = start; i < number + start; i++) {
        if (graphic[i].data) {
            Log("ERROR: SeekGraphics: file %s, Graphic %d not freed", file, i);
            free(graphic[i].data);
            graphic[i].data = NULL;
        }
        readGraphicHeader(fh, &graphic[i]);
        int w = graphic[i].width, h = graphic[i].height;
        uint32_t size = (uint32_t)graphic[i].size;
        uchar *buffer = SafeMalloc(size + 128);
        uint32_t got = FileRead(fh, buffer, size);
        if (got != size) {
            Log("ERROR: SeekGraphics %s, %d, %d", file, start, seek);
            Log("    FileRead did not read %d bytes", size);
        }
        graphic[i].data = SafeMalloc((size_t)h * w + 64);
        decodeRLE(buffer, graphic[i].data, w, h);
        free(buffer);
    }
    FileClose(fh);
    if (debug) Log("    read %d graphics", number);
    return number;
}

void FreeGraphics(int start, int num)
{
    if (debug) Log("FreeGraphics(%d, %d)", start, num);
    if (maxGraphics < num + start) {
        Log("ERROR FreeGraphics(%d, %d) exceeds maximum", start, num);
        return;
    }
    for (int i = start; i < num + start; i++) {
        if (graphic[i].data) {
            free(graphic[i].data);
            graphic[i].data = NULL;
        }
    }
}
