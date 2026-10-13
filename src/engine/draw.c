/*
 * draw.c - primitive drawing helpers (draw.obj)
 */
#include "safari.h"

void RenderLine(int x1, int y1, int x2, int y2, uchar color)
{
    MGLDC *old = MGL_makeCurrentDC(workpage);
    MGL_setColor(color);
    MGL_lineCoord(x1, y1, x2, y2);
    MGL_makeCurrentDC(old);
}

void DrawRect(int x, int y, int w, int h, int color, int fill)
{
    MGLDC *old = MGL_makeCurrentDC(activeDC);
    MGL_setColor((uint32_t)color);
    if (fill == 0) MGL_rectCoord(x, y, w + x, h + y);
    else MGL_fillRectCoord(x, y, w + x, h + y);
    MGL_makeCurrentDC(old);
}

void DrawLine(int x1, int y1, int x2, int y2, uchar color)
{
    MGL_setColor(color);
    MGL_lineCoord(x1, y1, x2, y2);
}

void ClearScreen(void)
{
    MGLDC *old = MGL_makeCurrentDC(frontpage);
    MGL_setColor(0);
    MGL_clearDevice();
    MGL_makeCurrentDC(old);
}

void ClearBackScreen(void)
{
    MGLDC *old = MGL_makeCurrentDC(backpage);
    MGL_setColor(0);
    MGL_fillRectCoord(0, 0, 640, 480);
    MGL_makeCurrentDC(old);
}

void ShadowBox(int x, int y, int w, int h, uchar color)
{
    DrawRect(x + 1, y + 1, w - 1, h - 1, color, 1);
    DrawRect(x + 1, y + 1, w - 1, h - 1, colorBlack, 0);
    DrawRect(x, y, w - 1, h - 1, colorWhite, 0);
}

void SunkenBox(int x, int y, int w, int h, uchar color)
{
    DrawRect(x, y, w, h, color, 1);
    DrawLine(x, y, x, h + y - 1, (uchar)(color + 3));
    DrawLine(x, y, w + x - 1, y, (uchar)(color + 3));
    DrawLine(w + x, y + 1, w + x, h + y, (uchar)(color - 3));
    DrawLine(x + 1, h + y, w + x, h + y, (uchar)(color - 3));
}
