/*
 * dialog.c - simple object-list dialogs (Dialog.obj)
 *
 * A dialog is an array of OBJECTs terminated by type 0x80:
 *   1 = box (style 4 fill, 5 shadow, 6 sunken, 8 bevel, 9 frame, 12 stretched graphic)
 *   2 = text (style 1 left, 2 centred, 3 right, 12 colour font centred)
 *   3 = sunken text field, 4 = button, 5 = rule, 6 = group box, 10/13 = check item
 */
#include <stdlib.h>
#include <string.h>
#include "safari.h"

static divot_t *dlgsave;

int MouseInBox(int left, int top, int right, int bottom, int x, int y)
{
    return !(x < left || right <= x || y < top || bottom <= y);
}

int DLG_Check(OBJECT *d, int mousex, int mousey)
{
    if (!d) return -1;
    int max = 0;
    while (d[max].type != 0x80) max++;
    for (int i = max - 1; i >= 0; i--) {
        if (d[i].type != 0 &&
            MouseInBox(d[i].x, d[i].y, d[i].w + d[i].x, d[i].y + d[i].h, mousex, mousey))
            return i;
    }
    return -1;
}

void DLG_Draw(OBJECT *d, int start, int stop)
{
    MGL_makeCurrentDC(backpage);
    for (int i = start; i < stop; i++) {
        OBJECT *o = &d[i];
        int style = (char)o->style;
        uchar color = (uchar)o->color;
        int x = o->x, y = o->y, w = o->w, h = o->h;
        char *str = o->text;
        switch (o->type) {
        case 0:
            break;
        case 1:
            switch (style) {
            case 4: DrawRect(x, y, w, h, color, 1); break;
            case 5: ShadowBox(x, y, w, h, color); break;
            case 6: SunkenBox(x, y, w, h, color); break;
            case 8:
                DrawRect(x, y, w, h, colorBlack & 0xff, 1);
                DrawLine(x + 1, y + 1, w + x - 2, y + 1, (uchar)colorWhite);
                DrawLine(x + 1, y + 1, x + 1, h + y - 2, (uchar)colorWhite);
                DrawLine(w + x - 1, y + 1, w + x - 1, h + y - 1, 1);
                DrawLine(x + 1, h + y - 1, w + x - 1, h + y - 1, 1);
                break;
            case 9: DrawRect(x, y, w, h, color, 0); break;
            case 12: StretchGraphic(color, x, y, w, h); break;
            default: break;
            }
            break;
        case 2:
            if (style == 12) {
                int len = ClrFontLength(0, str);
                ClrFontDraw(0, x - len / 2, y, str);
            } else {
                if (style == 2) x += (w - FontLength(str)) / 2;
                else if (style == 3) x = x + w - FontLength(str);
                FontSetColor(color);
                FontDraw(x, y, str, backpage);
            }
            break;
        case 3:
            SunkenBox(x, y, w, h, color);
            ClrFontDraw(0, x + 2, y + 2, *(char **)str);
            break;
        case 4:
            ShadowBox(x, y, w, h, color);
            if (style == 4) {
                if (str) {
                    int len = ClrFontLength(0, str);
                    ClrFontDraw(0, x + (w - len) / 2, y + (h - 24) / 2, str);
                }
            } else if (str) {
                FontChoose(0);
                FontSetColor(colorBlack);
                int len = FontLength(str);
                FontDraw(x + (w - len) / 2, y + (h - 16) / 2 + 2, str, backpage);
            }
            break;
        case 5:
            if (h == 0) {
                DrawLine(x, y, w + x, y, (uchar)colorWhite);
                DrawLine(x, y + 1, w + x, y + 1, (uchar)colorBlack);
            } else {
                DrawLine(x + 1, y, w + x + 1, h + y, (uchar)colorWhite);
            }
            break;
        case 6:
            SunkenBox(x, y, w, h, color);
            if (str) {
                FontChoose(0);
                FontSetColor(colorBlack);
                FontDraw(((w + (int)strlen(str) * -8) >> 1) + x, y - 14, str, backpage);
            }
            break;
        case 10:
        case 13:
            ShadowBox(x, y, 16, 18, color);
            if (style == 1) ClrFontDraw(0, x + 3, y - 10, "x");
            ClrFontDraw(0, x + 20, y - 6, str);
            break;
        case 0x80:
            MGL_makeCurrentDC(frontpage);
            return;
        }
    }
    MGL_makeCurrentDC(frontpage);
}

/* action 0: save the screen behind the dialog (plus shadow), 1: restore it */
void DLG_Form(OBJECT *d, int action)
{
    if (action == 0) {
        int x = d[0].x, y = d[0].y, w = d[0].w + 11, h = d[0].h + 11;
        if (dlgsave) { free(dlgsave); dlgsave = NULL; }
        long size = MGL_divotSize(backpage, x, y, w + x, h + y);
        dlgsave = malloc((size_t)size);
        if (dlgsave) MGL_getDivot(backpage, x, y, w + x, h + y, dlgsave);
    } else if (dlgsave) {
        MGL_putDivot(backpage, dlgsave);
        free(dlgsave);
        dlgsave = NULL;
    }
}

void DLG_Center(OBJECT *d)
{
    if (d[0].x != 0 || d[0].y != 0) {
        int x0 = d[0].x, y0 = d[0].y;
        for (int i = 0; d[i].type != 0x80; i++) {
            d[i].x -= x0;
            d[i].y -= y0;
        }
    }
    int w = d[0].w, h = d[0].h;
    for (int i = 0; d[i].type != 0x80; i++) {
        if (d[i].type != 8) {
            d[i].x += (640 - w) / 2;
            d[i].y += (480 - h) / 2;
        }
    }
}
