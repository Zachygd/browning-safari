/*
 * font.c - bitmap and colour font rendering (font.obj)
 */
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include "safari.h"
#include "chicago.inc"

static const unsigned char *fontptr = chicagofont;
static const unsigned char *fontwidths = chicagowidths;
static uchar fontcolor;
static short fontwidth = 8, fontheight = 16;
short    num_fonts;
CLRFONT *colorfont[11];

void FontSetColor(int color) { fontcolor = (uchar)color; }

void FontChoose(int choice)
{
    (void)choice;
    fontptr = chicagofont;
    fontwidths = chicagowidths;
    fontwidth = 8;
    fontheight = 16;
}

int FontLength(const char *str)
{
    int total = 0;
    if (!str) return 0;
    for (int count = (int)strlen(str); count; count--) {
        char c = *str++;
        total += fontwidths[(unsigned char)(c - 0x20)];
    }
    return total;
}

void FontDraw(int dx, int dy, const char *str, MGLDC *page)
{
    if (!str) return;
    int bpl = page->bytesPerLine;
    long limit = (long)bpl * page->height;
    long dst = (long)dy * bpl + dx;
    for (int count = (int)strlen(str); count; count--) {
        int ch = (short)(*str++ - 0x20);
        if (ch < 0 || ch >= 96) ch = 0;
        int width = fontwidths[ch];
        long d = dst;
        for (int y = 0; y < fontheight; y++) {
            uchar bits = fontptr[fontheight * ch + y];
            for (int x = 0; x < width; x++)
                if ((bits & (0x80 >> x)) && d + x >= 0 && d + x < limit)
                    page->surface[d + x] = fontcolor;
            d += bpl;
        }
        dst += width;
    }
    if (page->isDisplay) sys_mark_dirty();
}

void ClrFontDraw(int which, int dx, int dy, const char *str)
{
    if (!str) return;
    if (which >= 11) { Log("ERROR font = %d", which); return; }
    CLRFONT *f = colorfont[which];
    if (!f) return;
    int height = f->height;
    int bpl = activeDC->bytesPerLine;
    long limit = (long)bpl * activeDC->height;
    long dst = (long)bpl * dy + dx;
    int count = (int)strlen(str);
    while (count-- > 0) {
        int ch = (signed char)*str++;
        if (ch < 0x80 && ch >= 0) {
            const uchar *src = f->bitmap + f->letterstarts[ch];
            int width = f->letterwidths[ch];
            long d = dst;
            for (int y = 0; y < height; y++) {
                for (int x = 0; x < width; x++)
                    if (src[x] && d + x >= 0 && d + x < limit) activeDC->surface[d + x] = src[x];
                d += bpl;
                src += f->width;
            }
            dst += f->letterkerns[ch];
        }
    }
    if (activeDC->isDisplay) sys_mark_dirty();
}

void ClrFontFormat(int font, int x, int y, const char *format, ...)
{
    char szBuffer[256];
    va_list ap;
    va_start(ap, format);
    vsnprintf(szBuffer, sizeof szBuffer, format, ap);
    va_end(ap);
    ClrFontDraw(font, x, y, szBuffer);
}

void ClrFontFormatRightJustify(int font, int x, int y, const char *format, ...)
{
    char str[256];
    va_list ap;
    va_start(ap, format);
    vsnprintf(str, sizeof str, format, ap);
    va_end(ap);
    int len = ClrFontLength(font, str);
    ClrFontDraw(font, x - len, y, str);
}

void ClrFontFormatCenter(int font, int x, int y, const char *format, ...)
{
    char str[256];
    va_list ap;
    va_start(ap, format);
    vsnprintf(str, sizeof str, format, ap);
    va_end(ap);
    int len = ClrFontLength(font, str);
    ClrFontDraw(font, x - len / 2, y, str);
}

static int rd16(FHANDLE fh) { uchar b[2] = {0, 0}; FileRead(fh, b, 2); return b[0] | b[1] << 8; }

/*
 * Font file: short count, 0x202 bytes skipped, then per font:
 *   long signature 0x14643, 0x14 bytes, ushort width (bytes per row of
 *   packed 4-bit pixels), ushort height, 9 bytes, uchar colors,
 *   129 ushort letter starts, 129 ushort kerning advances,
 *   16 RGBx palette entries, width*height bytes of nibbles.
 */
int LoadFonts(const char *szName)
{
    snprintf(filename, sizeof filename, "%s%s", cdPath, szName);
    FHANDLE fh = FileOpen(filename);
    if (fh == BAD_HANDLE) { Log("ERROR: Unable to load font file %s", filename); return 0; }
    num_fonts = (short)rd16(fh);
    FileSeek(fh, 0x202, 1);
    for (int which = 0; which < num_fonts && which < 11; which++) {
        uint32_t test = 0;
        FileRead(fh, &test, 4);
        if (test != 0x14643) {
            Log("ERROR: LoadFonts: signature wrong");
            FileClose(fh);
            return 0;
        }
        if (colorfont[which]) ClrFontFree(which);
        CLRFONT *ptr = colorfont[which] = SafeMalloc(sizeof(CLRFONT));
        FileSeek(fh, 0x14, 1);
        ptr->width = (ushort)rd16(fh);
        ptr->height = (ushort)rd16(fh);
        FileSeek(fh, 9, 1);
        uchar colors = 0;
        FileRead(fh, &colors, 1);
        ptr->colors = colors;
        for (int i = 0; i < 129; i++) ptr->letterstarts[i] = (ushort)rd16(fh);
        for (int i = 0; i < 129; i++) ptr->letterkerns[i] = (ushort)rd16(fh);
        for (int i = 0; i < 16; i++) {
            uchar junk;
            FileRead(fh, &ptr->palette[i][0], 1);
            FileRead(fh, &ptr->palette[i][1], 1);
            FileRead(fh, &ptr->palette[i][2], 1);
            FileRead(fh, &junk, 1);
        }
        ptr->palette[0][0] = ptr->palette[0][1] = ptr->palette[0][2] = 0;
        int height = (short)ptr->height;
        int stride = ptr->width * 2;   /* pitch used by ClrFontDraw/RemapFonts */
        long lSize = (long)ptr->width * height;
        uchar *buffer = SafeMalloc(lSize);
        ptr->bitmap = SafeMalloc((size_t)stride * height * 2 + 16);
        FileRead(fh, buffer, (uint32_t)lSize);
        ptr->width = (ushort)(ptr->width * 2);
        const uchar *src = buffer;
        uchar *dst = ptr->bitmap;
        int rowbytes = (short)ptr->width;
        for (int row = 0; row < height; row++) {
            for (int i = 0; i < rowbytes; i += 2) {
                uchar b = *src++;
                dst[i] = (uchar)(b >> 4);
                dst[(short)(i + 1)] = b & 0xf;
            }
            dst += stride;
        }
        for (int i = 0; i < 128; i++) ptr->letterwidths[i] = ptr->letterstarts[i + 1] - ptr->letterstarts[i];
        ptr->letterwidths[128] = 0;
        free(buffer);
    }
    FileClose(fh);
    RemapFonts();
    return 1;
}

/* map each font's 16 colours onto the nearest entries of the game palette */
void RemapFonts(void)
{
    uchar remap[16];
    for (int which = 0; which < num_fonts && which < 11; which++) {
        CLRFONT *f = colorfont[which];
        if (!f) continue;
        int width = f->width;
        int height = f->height;
        int stride = width;
        remap[0] = 0;
        for (int i = 1; i < 16; i++)
            remap[i] = (uchar)FindRGB(gamepalette, f->palette[i][2], f->palette[i][1], f->palette[i][0]);
        for (int row = 0; row < height; row++) {
            uchar *p = f->bitmap + (size_t)row * stride;
            for (int i = 0; i < width; i++) p[i] = remap[p[i] & 0xf];
        }
    }
}

int FindRGB(const uchar *palette, int r, int g, int b)
{
    for (int range = 0; range < 0x40; range++) {
        int r1 = r + range, g1 = g + range, b1 = b + range;
        if (r1 < 0) r1 = 0;
        if (r1 > 255) r1 = 255;
        if (g1 < 0) g1 = 0;
        if (g1 > 255) g1 = 255;
        if (b1 < 0) b1 = 0;
        if (b1 > 255) b1 = 255;
        const uchar *pp = palette;
        for (int p = 0; p < 256; p++, pp += 3)
            if (p != 0 && abs(r1 - pp[0]) <= range && abs(g1 - pp[1]) <= range && abs(b1 - pp[2]) <= range)
                return p;
        r1 = r - range; g1 = g - range; b1 = b - range;
        if (r1 < 0) r1 = 0;
        if (r1 > 255) r1 = 255;
        if (g1 < 0) g1 = 0;
        if (g1 > 255) g1 = 255;
        if (b1 < 0) b1 = 0;
        if (b1 > 255) b1 = 255;
        pp = palette;
        for (int p = 0; p < 256; p++, pp += 3)
            if (p != 0 && abs(r1 - pp[0]) <= range && abs(g1 - pp[1]) <= range && abs(b1 - pp[2]) <= range)
                return p;
    }
    return 0xff;
}

void ClrFontFree(int which)
{
    CLRFONT *f = colorfont[which];
    if (f) {
        free(f->bitmap);
        f->bitmap = NULL;
        free(f);
        colorfont[which] = NULL;
    }
}

void FreeFonts(void)
{
    for (int i = 0; i < num_fonts && i < 11; i++) ClrFontFree(i);
}

int ClrFontLength(int which, const char *str)
{
    int len = 0;
    CLRFONT *f = colorfont[which];
    if (!f || !str) return 0;
    for (int count = (int)strlen(str); count; count--) {
        int c = (signed char)*str++;
        if (c >= 0 && c < 129) len += f->letterkerns[c];
    }
    return len;
}

int LetterWidth(int which, int letter)
{
    if (!colorfont[which]) return 0;
    if (letter < 0 || letter >= 129) return 0;
    return colorfont[which]->letterkerns[letter];
}
