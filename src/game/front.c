/*
 * front.c - title screen, credits, new/load/delete game screens and the
 * name entry field (Title.obj, credits.obj, newgame.obj, loadgame.obj,
 * delgame.obj, Select.obj)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "safari.h"

/* ------------------------------------------------------------- title -- */
void ShowTitleScreen(void)
{
    BlackPalette();
    LoadBackground("bkg_01");
    BlitWorkToBack();
    FontSetColor(0xfc);
    FontDraw(0x23f, 0x1cc, "Ver 1.02", backpage);
    BlitBackToFront();
    StartMusic("title");
    FadeFromBlack();
    TimeDelay(0);
    TimeDelay(2000);
}

/* ----------------------------------------------------------- credits -- */
static const char *credits[] = {
    "2 A very special thanks to", "2 the following individuals", "1 Claude & Julie Meredith",
    "1 HUNGWE Tours & Safaris", "1 ", "1 Clive Stockil", "1 & the wonderful staff at",
    "1 SENUKO Safari Lodge", "1 ", "1 Mark & Georgina Homann", "1 MAHENYE Safari Lodge", "1 ",
    "1 Ian DuPreez", "1 IMIRE Safari Ranch", "1 ", "1 ", "2 Executive Producers",
    "1 Edwin T. Cornia", "1 Grant H. Heaton", "1 Don H. Lund", "1 Bryan M. Tamietti",
    "1 John A. Willener", "1 ", "2 Programming", "1 David Lindsley", "1 ",
    "2 Cartooning/Animation", "1 Scott Jarrard", "1 Andrea Pearson", "1 ", "2 Graphics",
    "1 August Coleman", "1 Kevin A. Ernst", "1 Bryan M. Tamietti", "1 Liz Wetmore", "1 ",
    "2 3D Graphics/Animation", "1 Bryan M. Tamietti", "1 ", "2 Photography", "1 Edwin T. Cornia",
    "1 ", "2 Stock Photography & Clip Art", "1 Corel", "1 ", "2 Voice Talent",
    "1 Mr. Snidley - Russell Durrant", "1 Annie - Renee Kaplan", "1 Mike - Trevor Kaplan",
    "1 Makena - BD Dixie Kolditz", "1 Claude - Jacques du Plessis", "1 Warden - Monarch Tshuma",
    "1 Villager - Evans Mieso", "1 Kantigi - Monarch Tshuma", "1 ", "2 Music", "1 John Hancock",
    "1 Rich Dixon", "1 Creative Support Services Music (BMI)", "1 ", "2 Additional Sound Effects",
    "1 Provided by", "1 Network Music, Inc.", "1 ", "1 ", "1 ", "2 The Browning African Safari ~",
    "1 \x7f 1998 Multimedia Holding, L.C.", "1 ", "1 Uses Miles Sound System",
    "1 \x7f 1991-1997 by RAD Game Tools, Inc.", "1 ", "1 Graphics Libraries provided by",
    "1 SciTech Software, Inc.", "1 ",
    /* added for this port */
    "1 ", "2 Native Linux port", "1 Rebuilt from the original 1998 program", "1 ", "",
};

void ShowCredits(void)
{
    int width = 1, i;
    uint32_t lastcount = 0;
    for (i = 0; credits[i][0] != 0; i++) {
        int l = ClrFontLength(1, credits[i]);
        if (width <= l) width = l;
    }
    int num_lines = i;
    width = ((width + 7) / 8) * 8 + 16;
    int height = i * 22;
    MGLDC *dc = MGL_createMemoryDC(width, height);
    if (!dc) return;
    MGL_makeCurrentDC(dc);
    MGL_setColor(0);
    MGL_fillRectCoord(0, 0, width, height);
    MGL_makeCurrentDC(frontpage);
    FadeToBlack();
    LoadBackground("bkg_25");
    ClrFontFree(0);
    ClrFontFree(1);
    ClrFontFree(2);
    ClrFontFree(3);
    LoadFonts("fnt_02.dat");
    int y = 0;
    activeDC = dc;
    for (i = 0; i < num_lines; i++) {
        int font = credits[i][0] - '0';
        const char *ptr = credits[i] + 2;
        int x = (width - ClrFontLength(font, ptr)) / 2;
        ClrFontDraw(font, x, y, ptr);
        y += 22;
    }
    activeDC = backpage;
    BlitWorkToBack();
    BlitBackToFront();
    FadeFromBlack();
    StopMusic();
    StartMusic("credits");
    int sy = 0;
    int xo = (640 - width) / 2;
    int dy = 478;
    int limit = height;
    EVT_flush(EVT_KEYDOWN | EVT_MOUSEDOWN);
    i = 0;
    event_t evt;
    for (;;) {
        if ((limit + 480) / 3 <= i) {
            WaitMusic();
            break;
        }
        uint32_t count = GetLapCount();
        if (count - lastcount > 60) {
            int h = 479 - dy;
            if (limit < sy + h) h = limit - sy;
            lastcount = count;
            MGL_bitBltCoord(backpage, workpage, xo, 0, xo + width, 480, xo, 0, 0);
            MGL_transBltCoord(backpage, dc, 0, sy, width, sy * 2 + h, xo, dy, 0, 1);
            MGL_bitBltCoord(frontpage, backpage, xo, 0, xo + width, 480, xo, 0, 0);
            if (dy == 0) sy += 3;
            dy -= 3;
            if (dy < 0) dy = 0;
            i++;
        }
        if (EVT_peekNext(&evt, EVT_KEYDOWN | EVT_MOUSEDOWN)) break;
        if (sys_quit_requested) break;
    }
    MGL_destroyDC(dc);
    StopMusic();
    SoundPlay(0xb);
    FadeToRed();
    activeDC = workpage;
    for (i = 3; i < 0x300; i += 3) {
        gamepalette[i] = 0xff;
        gamepalette[i + 1] = 0;
        gamepalette[i + 2] = 0;
    }
    SetPalette();
    BlitWorkToBack();
    BlitBackToFront();
}

/* ---------------------------------------------------------- new game -- */
static HOT_SPOT new_hotspots[] = {
    {251, 452, 138, 22, 1, 1, 0, 0, 0, ExitScreen, -1},
};

void DrawNewScreen(void)
{
    int y = 100;
    char str[128];
    FadeToBlack();
    LoadBackground("bkg_03");
    num_screen_gfx = LoadGraphics(0x29, "spr_03");
    screen_hotspots = new_hotspots;
    BlitWorkToBack();
    for (int i = 0; i < 8; i++) {
        if (players[i].name[0] > ' ') {
            ClrFontFormatCenter(3, 320, y, "%s", players[i].name);
            y += 35;
        }
    }
    for (current_player = 0; current_player < 8 && players[current_player].name[0] > ' '; current_player++) {}
    BlitBackToFront();
    ShowMouse();
    FadeFromBlack();
    for (int i = 0; i < 8; i++) {
        if (players[i].name[0] > ' ' && players[i].saved_game == 0) {
            snprintf(str, sizeof str, "%s|has not been saved.|Save this game first?", players[i].name);
            if (AlertBox(str, "Yes|No") == 1) SaveGame(i);
        }
    }
    if (current_player == 8) {
        SoundPlay(5);
        AlertBox("No room for new player!|Delete a player first to make room.", "Okay");
        current_player = 7;
    } else {
        memset(&players[current_player], 0, sizeof(PLAYER));
        player = &players[current_player];
        players[current_player].unk20 = (uchar)current_player;
        player->music_volume = 80;
        player->sound_volume = 80;
        player->dollars = 0;
        player->saved_game = 0;
        GetName(player->name, 320, y);
        for (int i = 0; i < 15; i++) player->shop_art[i] = 1;
        SaveGame(current_player);
    }
}

/* ---------------------------------------------------- load / delete -- */
static void LoadPickGame(int which);
static void DeletePickGame(int which);

static HOT_SPOT load_hotspots[] = {
    {100, 100, 390, 24, 1, 0, 0, 0, 0, LoadPickGame, 1},
    {100, 135, 390, 24, 1, 0, 0, 0, 1, LoadPickGame, 2},
    {100, 170, 390, 24, 1, 0, 0, 0, 2, LoadPickGame, 3},
    {100, 205, 390, 24, 1, 0, 0, 0, 3, LoadPickGame, 4},
    {100, 240, 390, 24, 1, 0, 0, 0, 4, LoadPickGame, 5},
    {100, 275, 390, 24, 1, 0, 0, 0, 5, LoadPickGame, 6},
    {100, 310, 390, 24, 1, 0, 0, 0, 6, LoadPickGame, 7},
    {100, 345, 390, 24, 1, 0, 0, 0, 7, LoadPickGame, 8},
    {251, 452, 138, 22, 1, 1, 0, 0, 0, ExitScreen, -1},
};

static HOT_SPOT delete_hotspots[] = {
    {100, 100, 390, 24, 1, 0, 0, 0, 0, DeletePickGame, 1},
    {100, 135, 390, 24, 1, 0, 0, 0, 1, DeletePickGame, 2},
    {100, 170, 390, 24, 1, 0, 0, 0, 2, DeletePickGame, 3},
    {100, 205, 390, 24, 1, 0, 0, 0, 3, DeletePickGame, 4},
    {100, 240, 390, 24, 1, 0, 0, 0, 4, DeletePickGame, 5},
    {100, 275, 390, 24, 1, 0, 0, 0, 5, DeletePickGame, 6},
    {100, 310, 390, 24, 1, 0, 0, 0, 6, DeletePickGame, 7},
    {100, 345, 390, 24, 1, 0, 0, 0, 7, DeletePickGame, 8},
    {251, 452, 138, 22, 1, 1, 0, 0, 0, ExitScreen, -1},
};

/* list the players; each listed name gets the next hot spot */
static void ShowGames(HOT_SPOT *hs)
{
    int y = 100;
    BlitWorkToBack();
    for (int i = 0; i < 8; i++) hs[i].active = 0;
    int k = 0;
    for (int i = 0; i < 8; i++) {
        if (players[i].name[0] > ' ') {
            int font = (current_player == i) ? 4 : 3;
            ClrFontFormatCenter(font, 320, y, "%s", players[i].name);
            y += 35;
            hs[k].param = players[i].unk20;
            hs[k].active = 1;
            k++;
        }
    }
}

void DrawLoadScreen(void)
{
    FadeToBlack();
    LoadBackground("bkg_04");
    num_screen_gfx = LoadGraphics(0x29, "spr_04");
    screen_hotspots = load_hotspots;
    ShowGames(load_hotspots);
    BlitBackToFront();
    ShowMouse();
    FadeFromBlack();
}

static void LoadPickGame(int which)
{
    char str[128];
    for (int i = 0; i < 8; i++) load_hotspots[i].is_hilited = 0;
    if (current_player == which) {
        if (player->saved_game == 0) {
            snprintf(str, sizeof str, "Abandon %s|and reload game from disk?", player->name);
            if (AlertBox(str, "Yes|No") == 1) LoadPlayer(which);
        }
    } else if (players[which].name[0] > ' ' && players[which].saved_game == 0) {
        snprintf(str, sizeof str, "%s|has not been saved.|Save this game first?", players[which].name);
        if (AlertBox(str, "Yes|No") == 1) SaveGame(which);
    }
    current_player = which;
    player = &players[which];
    sound_volume = players[which].sound_volume;
    music_volume = players[which].music_volume;
    SoundPlay(0);
    SoundSetVolume(0x10, player->music_volume);
    ShowGames(load_hotspots);
    BlitBackToFront();
    ShowMouse();
}

void DrawDeleteScreen(void)
{
    FadeToBlack();
    LoadBackground("bkg_05");
    num_screen_gfx = LoadGraphics(0x29, "spr_05");
    screen_hotspots = delete_hotspots;
    ShowGames(delete_hotspots);
    BlitBackToFront();
    ShowMouse();
    FadeFromBlack();
}

static void DeletePickGame(int which)
{
    char str[256];
    for (int i = 0; i < 8; i++) delete_hotspots[i].is_hilited = 0;
    current_player = which;
    player = &players[which];
    ShowGames(delete_hotspots);
    BlitBackToFront();
    ShowMouse();
    SoundPlay(3);
    snprintf(str, sizeof str, "Delete|%s|permantently from computer?", player->name);
    if (AlertBox(str, "Yes|No") != 2) {
        memset(&players[player->unk20], 0, sizeof(PLAYER));
        DeleteGame(current_player);
        for (current_player = 0; current_player < 8 && players[current_player].name[0] < '!'; current_player++) {}
        if (current_player >= 8) current_player = 0;
        player = &players[current_player];
        ShowGames(delete_hotspots);
        BlitBackToFront();
        ShowMouse();
    }
}

/* -------------------------------------------------------- name entry -- */
int GetName(char *szName, int x, int y)
{
    EDITFIELDINFO efi;
    memset(&efi, 0, sizeof efi);
    efi.x = x;
    efi.y = y;
    efi.w = 480;
    efi.h = 40;
    efi.font = 4;
    efi.color = 0xda;
    efi.cursorx = 0;
    efi.infield = 0;
    efi.index = 0;
    snprintf(efi.str, sizeof efi.str, "%s", szName);
    efi.len = (int)strlen(efi.str);
    EditField(&efi);
    snprintf(szName, 32, "%s", efi.str);
    return 1;
}

static int kern(EDITFIELDINFO *e, int c)
{
    if (!colorfont[e->font] || c < 0 || c > 128) return 0;
    return colorfont[e->font]->letterkerns[c];
}

void EditField(EDITFIELDINFO *e)
{
    int cursorOn = 0;
    uint32_t lastcount = 0;
    for (;;) {
        uint32_t count = GetLapCount();
        int redraw = count - lastcount > 300;
        if (redraw) {
            cursorOn ^= 1;
            lastcount = count;
        }
        CheckEvents();
        if (lastCode != 0) {
            e->len = (int)strlen(e->str);
            switch (lastCode) {
            case 0x0e:   /* backspace */
                if (e->index != 0) {
                    e->cursorx -= kern(e, (signed char)e->str[e->index - 1]);
                    for (int i = e->index; i < e->len; i++) e->str[i - 1] = e->str[i];
                    e->str[e->len - 1] = 0;
                    e->len--;
                    e->index--;
                    redraw = 1;
                    cursorOn = 1;
                }
                break;
            case 0x1c:   /* enter */
                HideMouse();
                BlitPageToPage(workpage, backpage, e->x - e->w / 2, e->y, e->w, e->h);
                ClrFontFormatCenter(e->font, e->x, e->y, "%s", e->str);
                BlitPageToPage(backpage, frontpage, e->x - e->w / 2, e->y, e->w, e->h);
                ShowMouse();
                return;
            case 0x47:   /* home */
                e->index = 0;
                e->cursorx = 0;
                redraw = 1;
                cursorOn = 1;
                break;
            case 0x4b:   /* left */
                if (e->index != 0) {
                    e->cursorx -= kern(e, (signed char)e->str[e->index - 1]);
                    e->index--;
                    cursorOn = 1;
                    redraw = 1;
                }
                break;
            case 0x4d:   /* right */
                if (e->index < e->len) {
                    e->cursorx += kern(e, (signed char)e->str[e->index]);
                    e->index++;
                    cursorOn = 1;
                    redraw = 1;
                }
                break;
            case 0x4f:   /* end */
                e->index = e->len;
                e->cursorx = ClrFontLength(e->font, e->str);
                redraw = 1;
                cursorOn = 1;
                break;
            case 0x53:   /* delete */
                if (e->index < e->len) {
                    for (int i = e->index; i < e->len; i++) e->str[i] = e->str[i + 1];
                    e->len--;
                    redraw = 1;
                    cursorOn = 1;
                }
                break;
            }
        }
        if (lastKey != 0) {
            e->len = (int)strlen(e->str);
            if (lastKey > 0x1f && lastKey < 0x80 && e->len < 30) {
                for (int i = e->len; e->index < i; i--) e->str[i] = e->str[i - 1];
                e->str[e->len + 1] = 0;
                e->str[e->index] = (char)lastKey;
                e->cursorx += kern(e, lastKey);
                e->index++;
                redraw = 1;
                cursorOn = 1;
            }
        }
        if (redraw) {
            HideMouse();
            BlitPageToPage(workpage, backpage, e->x - e->w / 2, e->y, e->w, e->h);
            ClrFontFormatCenter(e->font, e->x, e->y, "%s", e->str);
            if (cursorOn) {
                int tl = ClrFontLength(e->font, e->str);
                DrawRect(e->cursorx + e->x - tl / 2, e->y, 1, e->h, 0xff, 1);
            }
            BlitPageToPage(backpage, frontpage, e->x - e->w / 2, e->y, e->w, e->h);
            ShowMouse();
        }
    }
}
