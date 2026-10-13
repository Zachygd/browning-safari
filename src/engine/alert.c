/*
 * alert.c - modal message and chooser boxes (Alert.obj)
 */
#include <stdlib.h>
#include <string.h>
#include "safari.h"

static OBJECT dialog[12] = {
    {1, 2, 5, 13, NULL, 0, 0, 544, 60},
    {0, 2, 2, 255, NULL, 8, 7, 528, 16},
    {0, 2, 2, 255, NULL, 8, 14, 528, 16},
    {0, 2, 2, 255, NULL, 8, 21, 528, 16},
    {0, 2, 2, 255, NULL, 8, 28, 528, 16},
    {0, 2, 2, 255, NULL, 8, 35, 528, 16},
    {0, 2, 2, 255, NULL, 8, 42, 528, 16},
    {0, 0, 0, 32, NULL, 5, 20, 64, 40},
    {0, 0, 0, 32, NULL, 90, 20, 64, 40},
    {0, 0, 0, 32, NULL, 175, 20, 64, 40},
    {0, 0, 0, 0, NULL, 0, 0, 0, 0},
    {0x80, 0, 0, 0, NULL, 0, 0, 0, 0},
};

static OBJECT cbox[17] = {
    {1, 2, 5, 230, NULL, 0, 0, 250, 200},
    {2, 2, 12, 0, NULL, 0, 125, 250, 16},
    {5, 2, 0, 255, NULL, 2, 30, 246, 0},
    {10, 0, 1, 217, NULL, 16, 42, 13, 13},
    {10, 0, 0, 217, NULL, 16, 59, 13, 13},
    {10, 0, 0, 217, NULL, 16, 76, 13, 13},
    {10, 0, 0, 217, NULL, 16, 93, 13, 13},
    {10, 0, 0, 217, NULL, 16, 110, 13, 13},
    {10, 0, 0, 217, NULL, 16, 127, 13, 13},
    {10, 0, 0, 217, NULL, 16, 144, 13, 13},
    {10, 0, 0, 217, NULL, 16, 161, 13, 13},
    {10, 0, 0, 217, NULL, 16, 178, 13, 13},
    {10, 0, 0, 217, NULL, 16, 195, 13, 13},
    {4, 0, 4, 217, "Okay", 45, 220, 64, 40},
    {4, 0, 4, 217, "Cancel", 141, 220, 64, 40},
    {0, 0, 8, 0, NULL, 0, 0, 0, 0},
    {0x80, 0, 0, 0, NULL, 0, 0, 0, 0},
};

int GetDialogInput(int *xpos, int *ypos)
{
    CheckEvents();
    *xpos = mouseX;
    *ypos = mouseY;
    return lastInput == 1;
}

/* str: lines separated by '|' (max 6); btn: buttons separated by '|' (max 3).
 * Returns the 1-based button index. */
int AlertBox(const char *str, const char *btn)
{
    int numlines = 0, numbutts = 0, width, twidth = 10, bwidth = 2, ret = 0;
    char *s = strdup(str);
    if (!s) return 0;
    char *b = strdup(btn);
    if (!b) { free(s); return 0; }
    dialog[0].style = 12;
    dialog[0].color = 190;
    dialog[0].y = 0;
    dialog[0].x = 0;
    dialog[9].h = 40;
    dialog[8].h = 40;
    dialog[7].h = 40;
    dialog[8].flags = 0;
    dialog[7].flags = 0;
    dialog[9].flags = 0;
    for (int i = 1; i < 10; i++) dialog[i].type = 0;
    char *save = NULL;
    for (int i = 0; i < 6; i++) {
        char *ptr = strtok_r(i == 0 ? s : NULL, "|", &save);
        if (!ptr) break;
        dialog[i + 1].type = 2;
        dialog[i + 1].y = i * 22 + 22;
        dialog[i + 1].text = ptr;
        dialog[i + 1].style = 12;
        width = ClrFontLength(0, ptr);
        if (twidth <= width) twidth = width;
        numlines++;
    }
    save = NULL;
    for (int i = 7; i < 10; i++) {
        char *ptr = strtok_r(i == 7 ? b : NULL, "|", &save);
        if (!ptr) break;
        dialog[i].type = 4;
        dialog[i].style = 4;
        dialog[i].color = 217;
        dialog[i].text = ptr;
        dialog[i].w = 64;
        width = ClrFontLength(0, ptr);
        if (width > 64) {
            width += 6;
            dialog[i].w = width;
        }
        if (bwidth <= width) bwidth = width;
        bwidth += 10;
        numbutts++;
    }
    int tw = twidth < 11 ? 10 : twidth;
    width = tw + 20;
    dialog[0].h = (numlines + 4) * 22 + 16;
    dialog[0].w = tw + 24;
    if (dialog[0].w < bwidth) dialog[0].w = bwidth;
    if (dialog[0].w < width) dialog[0].w = width;
    for (int i = 0; i < numlines; i++) {
        dialog[i + 1].x = dialog[0].w / 2;
        dialog[i + 1].w = dialog[0].w - 16;
    }
    int total_button_widths = 0;
    for (int i = 0; i < numbutts; i++) total_button_widths += dialog[i + 7].w;
    int button_spaces = (dialog[0].w - total_button_widths) / (numbutts + 1);
    dialog[8].x = dialog[7].w + button_spaces * 2;
    dialog[9].x = dialog[8].w + dialog[8].x + button_spaces;
    dialog[7].x = button_spaces;
    for (int i = 0; i < numbutts; i++) dialog[i + 7].y = (numlines + 2) * 22;

    if (bFlipMouse == 1) HideMouseOnPage(backpage);
    HideMouseOnPage(frontpage);
    int which = GetMouseGraphic();
    SetMouseGraphic(0);
    int saveFlip = bFlipMouse;
    bFlipMouse = 0;
    DLG_Center(dialog);
    DLG_Form(dialog, 0);
    CheckerboardArea(dialog[0].x + 10, dialog[0].y + 10, dialog[0].w, dialog[0].h);
    DLG_Draw(dialog, 0, 11);
    BlitBackToFront();
    ShowMouse();
    WaitMouseUp();
    int xpos = 320, ypos = 240;
    int done = 0;
    while (!done) {
        int r;
        do { r = GetDialogInput(&xpos, &ypos); } while (r == 0);
        int i = DLG_Check(dialog, xpos, ypos);
        if (i > 6 && i < 10) {
            SoundPlay(0);
            done = 1;
            ret = i - 6;
        }
    }
    free(s);
    free(b);
    DLG_Form(dialog, 1);
    BlitBackToFront();
    SetMouseGraphic(which);
    bFlipMouse = saveFlip;
    ShowMouse();
    WaitMouseUp();
    return ret;
}

/* list of up to 10 radio items separated by '|'; returns 1-based choice or 0 on Cancel */
int ChooseBox(const char *title, const char *str)
{
    int numlines = 0, width, twidth = 10, done, btn;
    char *s = strdup(str);
    if (!s) return 0;
    cbox[0].x = 0;
    cbox[0].y = 0;
    cbox[1].x = 0;
    cbox[1].y = 4;
    cbox[2].x = 1;
    cbox[2].y = 35;
    for (int i = 3; i < 13; i++) {
        cbox[i].type = 0;
        cbox[i].style = 0;
    }
    cbox[1].text = (char *)title;
    char *save = NULL;
    for (int i = 0; i < 10; i++) {
        char *ptr = strtok_r(i == 0 ? s : NULL, "|", &save);
        if (!ptr) break;
        cbox[i + 3].type = 10;
        cbox[i + 3].x = 16;
        cbox[i + 3].y = i * 22 + 42;
        cbox[i + 3].text = ptr;
        width = (int)strlen(ptr);
        if (twidth <= width) twidth = width;
        numlines++;
    }
    cbox[0].w = twidth * 8 + 64;
    cbox[1].x = cbox[0].w / 2;
    cbox[2].w = twidth * 8 + 60;
    cbox[1].w = cbox[0].w;
    for (int i = 0; i < 2; i++) {
        cbox[i + 13].x = (cbox[0].w / 2 - cbox[13].w) / 2;
        cbox[i + 13].x += (cbox[0].w / 2) * i;
        cbox[i + 13].y = (numlines + 2) * 22 + 30;
    }
    cbox[0].h = cbox[13].y + 50;
    cbox[3].style = 1;

    if (bFlipMouse == 1) HideMouseOnPage(backpage);
    HideMouseOnPage(frontpage);
    int which = GetMouseGraphic();
    SetMouseGraphic(0);
    int saveFlip = bFlipMouse;
    bFlipMouse = 0;
    DLG_Center(cbox);
    DLG_Form(cbox, 0);
    CheckerboardArea(cbox[0].x + 10, cbox[0].y + 10, cbox[0].w, cbox[0].h);
    DLG_Draw(cbox, 0, 17);
    BlitBackToFront();
    ShowMouse();
    WaitMouseUp();
    int mx = 320, my = 240;
    done = 0;
    btn = 1;
    while (done == 0) {
        int r;
        do { r = GetDialogInput(&mx, &my); } while (r == 0);
        int i = DLG_Check(cbox, mx, my);
        if (i >= 3 && i <= 12) {
            SoundPlay(0);
            for (int k = 3; k < numlines + 3; k++) cbox[k].style = 0;
            cbox[i].style = 1;
            DLG_Draw(cbox, 3, numlines + 3);
            BlitBackToFront();
            ShowMouse();
            do { r = GetDialogInput(&mx, &my); } while (r != 0);
            btn = i - 2;
        } else if (i == 13) {
            SoundPlay(0);
            done = 1;
        } else if (i == 14) {
            SoundPlay(0);
            done = 2;
        }
    }
    free(s);
    DLG_Form(cbox, 1);
    BlitBackToFront();
    SetMouseGraphic(which);
    bFlipMouse = saveFlip;
    ShowMouse();
    WaitMouseUp();
    if (done != 1) btn = 0;
    return btn;
}
