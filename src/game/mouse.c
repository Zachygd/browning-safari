/*
 * mouse.c - software mouse cursor, binoculars/scope/bow-peep views and the
 * main event dispatcher (mouse.obj)
 */
#include <string.h>
#include "safari.h"

#define F(x) ((float)(x))

int mouseX = 320, mouseY = 240;
int lastInput, lastKey, lastCode;
int bFlipMouse;
int debugLog;               /* F6 toggles verbose logging in the original */

static int oldX, oldY;      /* where the cursor was last drawn */
static int m_repeat;        /* button already reported down */
static int save_graphic;
static int hotx, hoty;
static int num_mice;
static MGLDC *mouseSave;    /* background under the cursor */
static MGLDC *mouseDC;      /* cursor image */
static MGLDC *scopeWork;    /* 104x60 zoomed view */
static MGLDC *scopeLayer;   /* 104x60 zoomed depth map */
static int mousewide, mousehigh;
static int mouse_graphic;

int InitMouse(void)
{
    num_mice = LoadGraphics(0, "ptr_99");
    if (num_mice < 34) {
        Log("num_mice = %d", num_mice);
        return 0;
    }
    mouseDC = MGL_createMemoryDC(104, 96);
    mouseSave = MGL_createMemoryDC(104, 96);
    scopeWork = MGL_createMemoryDC(104, 60);
    scopeLayer = MGL_createMemoryDC(104, 60);
    if (!mouseDC || !mouseSave || !scopeWork || !scopeLayer) return 0;
    InitMouseSurface(0);
    mouse_graphic = 0;
    MGL_setPalette(mouseDC, pal, 256, 0);
    MGL_realizePalette(mouseDC, 256, 0, 0);
    return 1;
}

void InitMouseSurface(int num)
{
    int bpl = mouseDC->bytesPerLine;
    uchar *surface = mouseDC->surface;
    memset(surface, 0, (size_t)(mouseDC->maxy + 1) * bpl);
    for (int y = 0; y < graphic[num].height && y <= mouseDC->maxy; y++)
        for (int x = 0; x < graphic[num].width && x <= mouseDC->maxx; x++)
            surface[bpl * y + x] = graphic[num].data[graphic[num].width * y + x];
    hotx = graphic[num].hotx;
    hoty = graphic[num].hoty;
    mousewide = graphic[num].width;
    mousehigh = graphic[num].height;
}

void ExitMouse(void) { FreeGraphics(0, num_mice); }

void SetMouseGraphic(int which)
{
    InitMouseSurface(which);
    mouse_graphic = which;
}

int GetMouseGraphic(void) { return mouse_graphic; }

void VerifyMouse(void) { EVT_flush(EVT_MOUSEDOWN); }

void WaitMouse(void)
{
    event_t evt;
    EVT_halt(&evt, EVT_MOUSEDOWN);
}

void WaitMouseUp(void)
{
    int button = 0, mx, my;
    GetMouse(&mx, &my, &button);
    while (button != 0) GetMouse(&mx, &my, &button);
    lastInput = 0;
    m_repeat = 0;
}

void GetMouse(int *mx, int *my, int *button)
{
    event_t evt;
    EVT_getNext(&evt, EVT_MOUSEDOWN | EVT_MOUSEUP | EVT_MOUSEMOVE);
    if (evt.what == EVT_MOUSEDOWN) {
        *button = 1;
        *mx = evt.where_x;
        *my = evt.where_y;
    } else if (evt.what == EVT_MOUSEUP) {
        *button = 0;
    } else if (evt.what == EVT_MOUSEMOVE) {
        *mx = evt.where_x;
        *my = evt.where_y;
    }
    m_repeat = 0;
}

void InitMouseSave(void)
{
    oldX = mouseX + hotx;
    oldY = mouseY + hoty;
}

void MoveMouse(int x, int y)
{
    MS_moveTo(x, y);
    mouseX = x;
    mouseY = y;
    oldX = x;
    oldY = y;
}

void TrackMouse(void)
{
    MGL_bitBltCoord(frontpage, mouseSave, 0, 0, mousewide, mousehigh, oldX, oldY, 0);
    MGL_bitBltCoord(mouseSave, frontpage, mouseX + hotx, mouseY + hoty, mouseX + mousewide + hotx,
                    mouseY + mousehigh + hoty, 0, 0, 0);
    MGL_transBltCoord(frontpage, mouseDC, 0, 0, mousewide, mousehigh, mouseX + hotx, mouseY + hoty, 0, 1);
    oldX = mouseX + hotx;
    oldY = mouseY + hoty;
}

void BlitMouse(int x, int y)
{
    MGL_transBltCoord(backpage, mouseDC, 0, 0, mousewide, mousehigh, x + hotx, y + hoty, 0, 1);
}

void DrawMouse(void)
{
    MGL_bitBltCoord(mouseSave, backpage, mouseX + hotx, mouseY + hoty, mouseX + mousewide + hotx,
                    mouseY + mousehigh + hoty, 0, 0, 0);
    MGL_transBltCoord(backpage, mouseDC, 0, 0, mousewide, mousehigh, mouseX + hotx, mouseY + hoty, 0, 1);
}

void CopyMouse(void)
{
    BlitPageToPage(backpage, frontpage, oldX, oldY, mousewide, mousehigh);
    BlitPageToPage(backpage, frontpage, mouseX + hotx, mouseY + hoty, mousewide, mousehigh);
    oldX = mouseX + hotx;
    oldY = mouseY + hoty;
}

void DrawBinocs(void)
{
    PrepareScopeLayer();
    MGL_bitBltCoord(mouseSave, backpage, mouseX - 52, mouseY - 30, mouseX + 52, mouseY + 30, 0, 0, 0);
    MGL_stretchBltCoord(scopeWork, workpage, mouseX - 26, mouseY - 15, mouseX + 26, mouseY + 15, 0, 0, 104, 60);
    for (int k = 0; k < number_of_species; k++) {
        SPRITE *s = &sprites[sp_sort[k]];
        if (s->active)
            ScaleElk(s->image + s->frame, s->x, s->y, s->z, s->percent, s->flip);
    }
    BlitGraphic(4, 52, 30, scopeWork);
    MGL_transBltCoord(backpage, scopeWork, 0, 0, 104, 59, mouseX - 52, mouseY - 30, 30, 1);
}

void PrepareScopeLayer(void)
{
    if (!layer_map) { Log("PrepareScopeLayer: layer_map = NULL"); return; }
    int src_offset = (mouseY + hoty / 2) * 4480 + hotx / 2 + mouseX + game_offset;
    if (src_offset < 0) src_offset = 0;
    if (src_offset < 0x168f00) {
        const uchar *src = layer_map + src_offset;
        int bpl = scopeWork->bytesPerLine;
        uchar *lpDst = scopeWork->surface;
        for (int y = 0; y < 30; y++) {
            memcpy(lpDst, src, 52);
            lpDst += bpl;
            src += 4480;
        }
        MGL_stretchBltCoord(scopeLayer, scopeWork, 0, 0, 52, 30, 0, 0, 104, 60);
    }
}

/* Draw a sprite into the zoomed binocular/scope view (2x magnification). */
void ScaleElk(int num, int xpos, int ypos, int zpos, int percent, int flip)
{
    uchar rowbuf[640];
    int sx = 0;
    int dst_offset;
    int off;
    if (game_offset < 0xc81 || game_offset <= xpos) off = -game_offset;
    else off = 0xf00 - game_offset;
    xpos += off;
    if (xpos > 0xc80) xpos -= 0xf00;
    const uchar *z_map = scopeLayer->surface;
    int src_offset = 0;
    percent *= 2;
    if (percent <= 4) return;
    int w = graphic[num].width;
    int h = graphic[num].height;
    const uchar *src = graphic[num].data;
    if (!src) { Log("ScaleElk: graphic %d = NULL!", num); return; }
    float xscale = F((double)(percent * w) / 100.0);
    float yscale = F((double)(percent * h) / 100.0);
    float x_step = F((double)w / xscale);
    float y_step = F((double)h / yscale);
    float y_index = 0.0f;
    int hx, hy;
    if (flip == 0) {
        hx = (graphic[num].hotx * percent) / 100;
        hy = graphic[num].hoty;
    } else {
        hx = -((percent * (graphic[num].width + graphic[num].hotx)) / 100);
        hy = graphic[num].hoty;
    }
    ypos = ((ypos - mouseY) * 2 - hoty) + (hy * percent) / 100;
    xpos = ((xpos - mouseX) * 2 - hotx) + hx;
    if (xpos + (int)xscale < 0 || ypos + (int)yscale < 0 || xpos >= 104 || ypos >= 60) return;
    if (xpos < 0) {
        xscale = F((double)xpos + xscale);
        sx -= (xpos * 100) / percent;
        xpos = 0;
        src_offset = sx;
    }
    if ((double)ypos + yscale >= 60.0) yscale = F(60 - ypos);
    int bpl = scopeWork->bytesPerLine;
    dst_offset = bpl * ypos + xpos;
    for (int y = 0; y < h; y++) {
        if (flip == 0) { for (int n = 0; n < w; n++) rowbuf[n] = *src++; }
        else { for (int n = w - 1; n >= 0; n--) rowbuf[n] = *src++; }
        while ((int)y_index == y) {
            if (dst_offset >= 0) {
                uchar *lpDst = scopeWork->surface + dst_offset;
                float x_index = 0.0f;
                for (int x = 0; x < (int)xscale; x++) {
                    if (xpos + x >= 104) continue;
                    if (dst_offset + x > 0x185f) return;
                    if (z_map[dst_offset + x] < zpos) {
                        uchar pixel = rowbuf[src_offset + (int)x_index];
                        if (pixel) lpDst[x] = pixel;
                    }
                    x_index = F((double)x_index + x_step);
                }
            }
            y_index = F((double)y_index + y_step);
            dst_offset += bpl;
            if (dst_offset >= 0x1860) break;
        }
    }
}

void DrawScope(void)
{
    PrepareScopeLayer();
    MGL_bitBltCoord(mouseSave, backpage, mouseX - 30, mouseY - 30, mouseX + 30, mouseY + 30, 0, 0, 0);
    MGL_stretchBltCoord(scopeWork, workpage, mouseX - 15, mouseY - 15, mouseX + 15, mouseY + 15, 0, 0, 60, 60);
    for (int k = 0; k < number_of_species; k++) {
        SPRITE *s = &sprites[sp_sort[k]];
        if (s->active)
            ScaleElk(s->image + s->frame, s->x, s->y, s->z, s->percent, s->flip);
    }
    BlitGraphic(5, 30, 30, scopeWork);
    MGL_transBltCoord(backpage, scopeWork, 0, 0, 60, 59, mouseX - 30, mouseY - 30, 30, 1);
    MGL_transBltCoord(backpage, mouseDC, 0, 0, 60, 60, mouseX - 30, mouseY - 30, 0, 1);
}

void InitBowPeepSave(void)
{
    MGL_bitBltCoord(mouseSave, backpage, mouseX - 16, mouseY - 47, mouseX + 40, mouseY + 49, 0, 0, 0);
    oldX = mouseX;
    oldY = mouseY;
}

void DrawBowPeep(void)
{
    MGL_bitBltCoord(mouseSave, backpage, mouseX - 16, mouseY - 47, mouseX + 40, mouseY + 49, 0, 0, 0);
    MGL_transBltCoord(backpage, mouseDC, 0, 0, mousewide, mousehigh, bow_sight_x * 2 + mouseX - 10,
                      bow_sight_y * 3 + mouseY - 14, 0, 1);
    BlitGraphic(7, mouseX, mouseY, backpage);
}

void CopyBowPeep(void)
{
    BlitPageToPage(backpage, frontpage, oldX - 16, oldY - 47, 56, 96);
    BlitPageToPage(backpage, frontpage, mouseX - 16, mouseY - 47, 56, 96);
    oldX = mouseX;
    oldY = mouseY;
}

/*
 * Pull one event and translate it into the game's input globals:
 *   lastInput 1 = left click, 2 = right click, 4 = mouse moved,
 *   5 = drag with left button, 6 = drag with right button,
 *   0x200/0x400/0x800/0x1000 = arrow keys
 */
void CheckEvents(void)
{
    event_t evt;
    lastInput = 0;
    lastKey = 0;
    lastCode = 0;
    if (!EVT_getNext(&evt, EVT_EVERYEVT)) return;
    switch (evt.what) {
    case EVT_KEYDOWN:
        lastKey = evt.message & 0xff;
        lastCode = (evt.message >> 8) & 0xff;
        switch (lastCode) {
        case 0x05:   /* Ctrl+4 in the shop with the mouse in the corner: +$1000 */
            if (game_screen == 0xc && (evt.modifiers & EVT_CTRLSTATE) && mouseX == 0 && mouseY == 0) {
                player->dollars += 1000;
                SoundPlay(6);
                RestoreShopScreen();
            }
            break;
        case 0x11: if (game_screen == 0x10) LocationKey(5); break;   /* W */
        case 0x13: if (game_screen == 0x10) LocationKey(4); break;   /* R */
        case 0x17: if (game_screen == 0x10) LocationKey(2); break;   /* I */
        case 0x26:                                                    /* L */
            if (game_screen == 0x11) LoadWeapon();
            if (game_screen == 0x10) LocationKey(3);
            break;
        case 0x2c: if (game_screen == 0x10) LocationKey(6); break;   /* Z */
        case 0x2e: if (game_screen == 0x10) LocationKey(1); break;   /* C */
        case 0x30:                                                    /* B */
            if (game_screen == 0x11) ToggleBinocs();
            if (game_screen == 0x10) LocationKey(0);
            break;
        case 0x40: debugLog = 1; break;                               /* F6 */
        case 0x48: lastInput = 0x200; break;
        case 0x4b: lastInput = 0x800; break;
        case 0x4d: lastInput = 0x1000; break;
        case 0x50: lastInput = 0x400; break;
        case 0x53:   /* Ctrl+Del in the shop with the mouse in the corner: empty supplies */
            if (game_screen == 0xc && (evt.modifiers & EVT_CTRLSTATE) && mouseX == 0 && mouseY == 0) {
                for (int i = 0; i < 17; i++) player->supplies[i] = 0;
                player->scopes[0] = 0;
                player->scopes[1] = 0;
                SoundPlay(5);
            }
            break;
        case 0x57:   /* Ctrl+F11 while hunting: reset the clock to 9:00 */
            if ((evt.modifiers & EVT_CTRLSTATE) && game_screen == 0x11) {
                time_hour = 9;
                time_min = 0;
                time_total = 540;
                update_clock = 1;
            }
            break;
        }
        break;
    case EVT_KEYUP:
        lastInput = 0;
        break;
    case EVT_MOUSEDOWN:
        if (m_repeat == 0) {
            mouseX = evt.where_x;
            mouseY = evt.where_y;
            if (evt.message & EVT_LEFTBMASK) lastInput = 1;
            if (evt.message & EVT_RIGHTBMASK) lastInput = 2;
            m_repeat = 1;
        }
        break;
    case EVT_MOUSEUP:
        m_repeat = 0;
        break;
    case EVT_MOUSEMOVE:
        mouseX = evt.where_x;
        mouseY = evt.where_y;
        lastInput = 4;
        if (evt.modifiers == EVT_LEFTBUT && m_repeat == 0) lastInput = 5;
        if (evt.modifiers == EVT_RIGHTBUT) lastInput = 6;
        if (bFlipMouse == 0) TrackMouse();
        break;
    }
}

void MouseHourGlass(void)
{
    save_graphic = mouse_graphic;
    HideMouse();
    SetMouseGraphic(1);
    ShowMouse();
}

void MouseRestore(void)
{
    HideMouse();
    SetMouseGraphic(save_graphic);
    ShowMouse();
}

void MouseNormal(void)
{
    HideMouse();
    SetMouseGraphic(0);
    ShowMouse();
}

void ShowMouse(void)
{
    if (mouse_graphic == 9) {
        MGL_bitBltCoord(mouseSave, backpage, mouseX + hotx, mouseY + hoty, mouseX + hotx + 60,
                        mouseY + hoty + 46, 0, 0, 0);
        MGL_transBltCoord(backpage, mouseDC, 0, 0, mousewide, mousehigh, mouseX + hotx, mouseY + hoty, 0, 1);
        oldX = mouseX + hotx;
        oldY = mouseY + hoty;
    } else if (mouse_graphic == 8) {
        MGL_bitBltCoord(mouseSave, backpage, mouseX - 16, mouseY - 47, mouseX + 40, mouseY + 49, 0, 0, 0);
        MGL_transBltCoord(backpage, mouseDC, 0, 0, mousewide, mousehigh, bow_sight_x * 2 + mouseX - 10,
                          bow_sight_y * 3 + mouseY - 14, 0, 1);
        BlitGraphic(7, mouseX, mouseY, backpage);
        oldX = mouseX;
        oldY = mouseY;
    } else if (bFlipMouse == 0) {
        MGL_bitBltCoord(mouseSave, frontpage, mouseX + hotx, mouseY + hoty, mouseX + mousewide + hotx,
                        mouseY + mousehigh + hoty, 0, 0, 0);
        MGL_transBltCoord(frontpage, mouseDC, 0, 0, mousewide, mousehigh, mouseX + hotx, mouseY + hoty, 0, 1);
        oldX = mouseX + hotx;
        oldY = mouseY + hoty;
    } else {
        MGL_bitBltCoord(mouseSave, backpage, mouseX + hotx, mouseY + hoty, mouseX + mousewide + hotx,
                        mouseY + mousehigh + hoty, 0, 0, 0);
        MGL_transBltCoord(backpage, mouseDC, 0, 0, mousewide, mousehigh, mouseX + hotx, mouseY + hoty, 0, 1);
        oldX = mouseX + hotx;
        oldY = mouseY + hoty;
    }
}

void HideMouse(void)
{
    if (bFlipMouse == 0) {
        if (mouse_graphic == 8)
            MGL_bitBltCoord(backpage, mouseSave, 0, 0, 56, 96, oldX - 16, oldY - 47, 0);
        else
            MGL_bitBltCoord(frontpage, mouseSave, 0, 0, mousewide, mousehigh, oldX, oldY, 0);
    } else if (mouse_graphic == 9) {
        MGL_bitBltCoord(backpage, mouseSave, 0, 0, 60, 46, oldX, oldY, 0);
    } else if (mouse_graphic == 8) {
        MGL_bitBltCoord(backpage, mouseSave, 0, 0, 56, 96, oldX - 16, oldY - 47, 0);
    } else {
        MGL_bitBltCoord(backpage, mouseSave, 0, 0, mousewide, mousehigh, oldX, oldY, 0);
    }
}

void HideMouseOnPage(MGLDC *page)
{
    if (mouse_graphic == 9)
        MGL_bitBltCoord(page, mouseSave, 0, 0, 60, 46, oldX, oldY, 0);
    else if (mouse_graphic == 8)
        MGL_bitBltCoord(page, mouseSave, 0, 0, 56, 96, oldX - 16, oldY - 47, 0);
    else
        MGL_bitBltCoord(page, mouseSave, 0, 0, mousewide, mousehigh, oldX, oldY, 0);
}

void ShowMouseOnPage(MGLDC *page)
{
    if (mouse_graphic == 9) {
        MGL_bitBltCoord(mouseSave, page, mouseX + hotx, mouseY + hoty, mouseX + hotx + 60, mouseY + hoty + 46, 0, 0, 0);
        MGL_transBltCoord(page, mouseDC, 0, 0, mousewide, mousehigh, mouseX + hotx, mouseY + hoty, 0, 1);
        oldX = mouseX + hotx;
        oldY = mouseY + hoty;
    } else if (mouse_graphic == 8) {
        MGL_bitBltCoord(mouseSave, page, mouseX - 16, mouseY - 47, mouseX + 40, mouseY + 49, 0, 0, 0);
        MGL_transBltCoord(page, mouseDC, 0, 0, mousewide, mousehigh, bow_sight_x * 2 + mouseX - 10,
                          bow_sight_y * 3 + mouseY - 14, 0, 1);
        BlitGraphic(7, mouseX, mouseY, page);
        oldX = mouseX;
        oldY = mouseY;
    } else {
        MGL_bitBltCoord(mouseSave, page, mouseX + hotx, mouseY + hoty, mouseX + mousewide + hotx,
                        mouseY + mousehigh + hoty, 0, 0, 0);
        MGL_transBltCoord(page, mouseDC, 0, 0, mousewide, mousehigh, mouseX + hotx, mouseY + hoty, 0, 1);
        oldX = mouseX + hotx;
        oldY = mouseY + hoty;
    }
}
