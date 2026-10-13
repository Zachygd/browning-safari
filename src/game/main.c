/*
 * main.c - screen management, hot spots, palette fades and the game loop
 * (main.obj)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "safari.h"

MGLDC   *frontpage, *backpage, *workpage, *activeDC;
HOT_SPOT *screen_hotspots;
int      oldGame_Screen;
int      game_screen;
PLAYER  *player;
int      num_screen_gfx;
int      done;
int      palette_steps = 32;

static HOT_SPOT main_hotspots[] = {
    {212, 67, 196, 44, 1, 1, 0, 0, 1, ExitScreen, 1},
    {206, 110, 206, 42, 1, 1, 0, 1, 2, ExitScreen, 2},
    {206, 150, 206, 44, 1, 1, 0, 2, 0, DoSaveGame, 3},
    {184, 194, 250, 44, 1, 1, 0, 3, 3, ExitScreen, 4},
    {158, 236, 296, 46, 1, 1, 0, 4, 4, ExitScreen, 5},
    {210, 336, 208, 42, 1, 1, 0, 5, 5, ExitScreen, 6},
    {272, 378, 74, 41, 1, 1, 0, 6, 0, DoExit, -1},
};

static void (*func_ptr[22])(void);

void DoSaveGame(int unused)
{
    char str[128];
    (void)unused;
    SoundPlay(0);
    SaveGame(current_player);
    snprintf(str, sizeof str, "Game entitled:|%s|has been saved to disk", players[current_player].name);
    AlertBox(str, "Okay");
}

void StartOver(void)
{
    player->game_over = 0;
    player->saved_game = 0;
    player->finished = 0;
    player->lounge_talk = 0;
    player->village_talk = 0;
    player->shop_talk = 0;
    player->res[0] = 0;
    player->travel = 0;
    player->been_to_africa = 0;
    player->museum_complete = 0;
    player->bagged_big_3 = 0;
    player->bonus_given = 0;
    player->scopes[0] = 0;
    player->scopes[1] = 0;
    player->dollars = 0;
    player->trophy_fees = 0;
    player->level_points = 0;
    player->total_points = 0;
    for (int i = 0; i < 17; i++) player->supplies[i] = 0;
    for (int i = 0; i < 15; i++) player->suitcase[i] = 0;
    for (int i = 0; i < 7; i++) player->gamebag[i] = 0;
    for (int i = 0; i < 15; i++) player->shop_art[i] = 0;
    for (int i = 0; i < 7; i++) player->museum_trophy[i] = 0;
    for (int i = 0; i < 15; i++) player->museum_art[i] = 0;
    hunter.game_over = 0;
    ResetShopItems();
}

void DoExit(int unused)
{
    char str[128];
    (void)unused;
    SoundPlay(3);
    if (AlertBox("Exit|Browning African Safari?", "Yes|No") != 1) return;
    for (int which = 0; which < 8; which++) {
        if (players[which].name[0] > ' ' && players[which].saved_game == 0) {
            snprintf(str, sizeof str, "%s|has not been saved.|Save this game before quitting?", players[which].name);
            int r = AlertBox(str, "Save|Abandon|Resume");
            if (r == 1) SaveGame(which);
            if (r == 3) return;
        }
    }
    DumpScreen();
    StopMusic();
    ShowCredits();
    FadeToBlack();
    ClearBackScreen();
    ClearScreen();
    done = 1;
}

void DrawMainScreen(void)
{
    FadeToBlack();
    LoadBackground("bkg_02");
    num_screen_gfx = LoadGraphics(0x29, "spr_02");
    screen_hotspots = main_hotspots;
    FreeGraphics(0, 2);
    LoadGraphics(0, "ptr_02");
    SetMouseGraphic(1);
    ClrFontFree(0);
    ClrFontFree(1);
    ClrFontFree(2);
    ClrFontFree(3);
    LoadFonts("fnt_02.dat");
    FreeGraphics(0xbe, 1);
    LoadGraphics(0xbe, "dlg_02");
    colorBlack = 3;
    colorWhite = 0x29;
    BlitWorkToBack();
    BlitBackToFront();
    ShowMouse();
    FadeFromBlack();
}

void DumpScreen(void) { FreeGraphics(0x29, num_screen_gfx); }

void DumpScreen2(void)
{
    FreeGraphics(0x29, num_screen_gfx);
    SoundFade(0x10);
    StopMusic();
}

void DoScreen(void)
{
    if (lastInput != 0) {
        if (lastInput == 1) {
            HOT_SPOT *hs = CheckHotSpots(screen_hotspots);
            if (hs) hs->fptr(hs->param);
        } else if (lastInput == 4) {
            CheckHotSpots(screen_hotspots);
        }
        lastInput = 0;
    }
}

void DoScreen2(void)
{
    AdvanceClock();
    DoScreen();
}

/* Highlight the hot spot under the mouse; return it if it is active. */
HOT_SPOT *CheckHotSpots(HOT_SPOT *hs)
{
    if (!hs) return NULL;
    HOT_SPOT *ptr = hs;
    for (;;) {
        if (ptr->active) {
            if (MouseInBox(ptr->x, ptr->y, ptr->w + ptr->x, ptr->y + ptr->h, mouseX, mouseY)) {
                if (ptr->hilite == 0 || ptr->is_hilited != 0) return ptr;
                /* un-highlight every other hot spot */
                for (HOT_SPOT *p = hs;; p = hs + p->next) {
                    if (ptr != p && p->active && p->hilite && p->is_hilited) {
                        HideMouse();
                        p->is_hilited = 0;
                        MGL_bitBltCoord(backpage, workpage, p->x, p->y, p->w + p->x, p->y + p->h, p->x, p->y, 0);
                        MGL_bitBltCoord(frontpage, backpage, p->x, p->y, p->w + p->x, p->y + p->h, p->x, p->y, 0);
                        ShowMouse();
                    }
                    if (p->next == -1) break;
                }
                HideMouse();
                BlitGraphic(ptr->graphic_num + 0x29, ptr->x, ptr->y, backpage);
                ptr->is_hilited = 1;
                MGL_bitBltCoord(frontpage, backpage, ptr->x, ptr->y, ptr->w + ptr->x, ptr->y + ptr->h, ptr->x, ptr->y, 0);
                ShowMouse();
                return ptr;
            }
            if (ptr->hilite && ptr->is_hilited == 1) {
                HideMouse();
                ptr->is_hilited = 0;
                MGL_bitBltCoord(backpage, workpage, ptr->x, ptr->y, ptr->w + ptr->x, ptr->y + ptr->h, ptr->x, ptr->y, 0);
                MGL_bitBltCoord(frontpage, backpage, ptr->x, ptr->y, ptr->w + ptr->x, ptr->y + ptr->h, ptr->x, ptr->y, 0);
                ShowMouse();
            }
        }
        if (ptr->next == -1) return NULL;
        ptr = hs + ptr->next;
    }
}

/* Like CheckHotSpots but only updates the back page (caller flips). */
void CoverHotSpots(HOT_SPOT *hs)
{
    if (!hs) return;
    HOT_SPOT *ptr = hs;
    for (;;) {
        if (ptr->active && ptr->hilite) {
            if (MouseInBox(ptr->x, ptr->y, ptr->w + ptr->x, ptr->y + ptr->h, mouseX, mouseY)) {
                if (ptr->is_hilited) return;
                for (HOT_SPOT *p = hs;; p = hs + p->next) {
                    if (ptr != p && p->active && p->hilite && p->is_hilited) {
                        p->is_hilited = 0;
                        MGL_bitBltCoord(backpage, workpage, p->x, p->y, p->w + p->x, p->h + p->y, p->x, p->y, 0);
                    }
                    if (p->next == -1) break;
                }
                if (ptr->graphic_num != -1) BlitGraphic(ptr->graphic_num + 0x29, ptr->x, ptr->y, backpage);
                ptr->is_hilited = 1;
                return;
            }
            if (ptr->hilite && ptr->is_hilited == 1) {
                ptr->is_hilited = 0;
                MGL_bitBltCoord(backpage, workpage, ptr->x, ptr->y, ptr->w + ptr->x, ptr->y + ptr->h, ptr->x, ptr->y, 0);
            }
        }
        if (ptr->next == -1) return;
        ptr = hs + ptr->next;
    }
}

void CopyHotSpots(HOT_SPOT *hs)
{
    if (!hs) return;
    for (HOT_SPOT *ptr = hs;; ptr = hs + ptr->next) {
        if (ptr->active) BlitPageToPage(backpage, frontpage, ptr->x, ptr->y, ptr->w, ptr->h);
        if (ptr->next == -1) return;
    }
}

void ExitScreen(int param)
{
    SoundPlay(0);
    SwitchScreen(param);
}

void SwitchScreen(int which)
{
    if (game_screen == which) return;
    MouseHourGlass();
    switch (game_screen) {
    case 5: DumpMuseumScreen(); break;
    default: DumpScreen(); break;
    case 8: DumpScreen2(); break;
    case 10: DumpScreen2(); break;
    case 0xc: DumpShopScreen(); break;
    case 0xe: DumpSuitcaseScreen(); break;
    case 0xf: DumpSuppliesScreen(); break;
    case 0x10: DumpScreen2(); break;
    case 0x11: DumpHuntScreen(); break;
    case 0x12: DumpGamebagScreen(); break;
    case 0x13: DumpHandbookScreen(); break;
    case 0x14: DumpTrophyScreen(); break;
    case 0x15: DumpGameOverScreen(); break;
    }
    player->last_screen = (uchar)game_screen;
    oldGame_Screen = game_screen;
    game_screen = which;
    switch (which) {
    case 0: DrawMainScreen(); break;
    case 1: DrawNewScreen(); break;
    case 2: DrawLoadScreen(); break;
    case 3: DrawDeleteScreen(); break;
    case 4: DrawSoundScreen(); break;
    case 5: DrawMuseumScreen(); break;
    case 6: DrawTravelScreen(); break;
    case 7: DrawMapScreen(); break;
    case 8: DrawLodgeScreen(); break;
    case 9: DrawLoungeScreen(); break;
    case 10: DrawVillageScreen(); break;
    case 0xc: DrawShopScreen(); break;
    case 0xe: DrawSuitcaseScreen(); break;
    case 0xf: DrawSuppliesScreen(); break;
    case 0x10: DrawLocationScreen(); break;
    case 0x11:
        DrawHuntScreen();
        WaitMouseUp();
        return;
    case 0x12: DrawGamebagScreen(); break;
    case 0x13: DrawHandbookScreen(); break;
    case 0x14: DrawTrophyScreen(); break;
    case 0x15: DrawGameOverScreen(); break;
    }
    MouseNormal();
    WaitMouseUp();
}

/* Overlay screens opened from inside a hunt or the shop. */
void TempSwitchScreen(int which)
{
    if (which == game_screen) return;
    MouseHourGlass();
    switch (game_screen) {
    case 0xd: DumpScopesScreen(); break;
    case 0xe: DumpSuitcaseScreen(); break;
    case 0xf: DumpSuppliesScreen(); break;
    case 0x12: DumpGamebagScreen(); break;
    case 0x13: DumpHandbookScreen(); break;
    }
    oldGame_Screen = game_screen;
    game_screen = which;
    switch (which) {
    case 0xd: DrawScopesScreen(); break;
    case 0xe: DrawSuitcaseScreen(); break;
    case 0xf: DrawSuppliesScreen(); break;
    case 0x11:
        RestoreHuntScreen();
        SetMouseGraphic(0);
        ShowMouse();
        WaitMouseUp();
        return;
    case 0x12: DrawGamebagScreen(); break;
    case 0x13: DrawHandbookScreen(); break;
    }
    MouseNormal();
    SetMouseGraphic(0);
    WaitMouseUp();
}

int GameInit(void)
{
    sound_volume = player->sound_volume;
    music_volume = player->music_volume;
    srand(sys_ms());
    FontChoose(0);
    FontSetColor(5);
    for (int i = 0; i < 16; i++) SoundSeek("sounds", i, i);
    func_ptr[0] = DoScreen;
    func_ptr[1] = DoScreen;
    func_ptr[2] = DoScreen;
    func_ptr[3] = DoScreen;
    func_ptr[4] = DoScreen;
    func_ptr[5] = DoMuseumScreen;
    func_ptr[6] = DoTravelScreen;
    func_ptr[7] = DoMapScreen;
    func_ptr[8] = DoLodgeScreen;
    func_ptr[9] = DoScreen2;
    func_ptr[10] = DoScreen2;
    func_ptr[11] = DoScreen;
    func_ptr[12] = DoShopScreen;
    func_ptr[13] = DoScopesScreen;
    func_ptr[15] = DoSuppliesScreen;
    func_ptr[14] = DoSuitcaseScreen;
    func_ptr[16] = DoScreen2;
    func_ptr[17] = DoHuntScreen;
    func_ptr[18] = DoGamebagScreen;
    func_ptr[19] = DoHandbookScreen;
    func_ptr[20] = DoTrophyScreen;
    func_ptr[21] = DoGameOverScreen;
    if (!InitMouse()) {
        Log("Unable to init mouse");
        return 0;
    }
    ShowTitleScreen();
    if (!LoadFonts("fonts.dat")) return 0;
    game_screen = 0;
    DrawMainScreen();
    MouseNormal();
    return 1;
}

void GameExit(void)
{
    for (int i = 0; i < 16; i++) SoundFree(i);
    FreeFonts();
    ExitMouse();
}

void GameLoop(void)
{
    while (done == 0) {
        CheckEvents();
        if (func_ptr[game_screen]) func_ptr[game_screen]();
    }
}

void EmergencyExit(void) { done = 1; }

void SetPalette(void)
{
    gamepalette[2] = 0;
    gamepalette[1] = 0;
    gamepalette[0] = 0;
    for (int i = 0; i < 256; i++) {
        pal[i].red = gamepalette[i * 3];
        pal[i].green = gamepalette[i * 3 + 1];
        pal[i].blue = gamepalette[i * 3 + 2];
        pal[i].alpha = 0;
    }
    MGL_setPalette(frontpage, pal, 256, 0);
    MGL_realizePalette(frontpage, 256, 0, 0);
    MGL_setPalette(backpage, pal, 256, 0);
    MGL_realizePalette(backpage, 256, 0, 0);
    MGL_setPalette(workpage, pal, 256, 0);
    MGL_realizePalette(workpage, 256, 0, 0);
}

void FadeStep(char step) { FadePalette(step * 8 - 1); }

void BlackPalette(void)
{
    palette_t blackpal[256];
    memset(blackpal, 0, sizeof blackpal);
    MGL_setPalette(frontpage, blackpal, 256, 0);
    MGL_realizePalette(frontpage, 256, 0, 0);
}

void FadeToBlack(void)
{
    uint32_t lastcount = 0;
    uint32_t steps = (uint32_t)palette_steps;
    while (steps > 1) {
        uint32_t count = GetLapCount();
        if (count - lastcount > 1) {
            steps--;
            FadePalette((int)(steps * 8 - 1));
            lastcount = count;
        }
    }
    FadePalette(0);
}

void FadeFromBlack(void)
{
    uint32_t lastcount = 0;
    int steps = 0;
    while (steps < palette_steps) {
        uint32_t count = GetLapCount();
        if (count - lastcount > 1) {
            steps++;
            FadePalette(steps * 8 - 1);
            lastcount = count;
        }
    }
}

void FadeToRed(void)
{
    uint32_t lastcount = 0;
    for (int i = 0; i < 256; i++) {
        pal[i].red = gamepalette[i * 3];
        pal[i].green = gamepalette[i * 3 + 1];
        pal[i].blue = gamepalette[i * 3 + 2];
        pal[i].alpha = 0;
    }
    int steps = 0;
    while (steps < 0x40) {
        uint32_t count = GetLapCount();
        if (count - lastcount > 10) {
            steps++;
            for (int i = 1; i < 256; i++) {
                if (pal[i].red < 0xfc) pal[i].red += 4;
                if (pal[i].green > 3) pal[i].green -= 4;
                if (pal[i].blue > 3) pal[i].blue -= 4;
            }
            MGL_setPalette(frontpage, pal, 256, 0);
            MGL_realizePalette(frontpage, 256, 0, 1);
            lastcount = count;
        }
    }
}

void FadeToNight(void)
{
    uint32_t lastcount = 0;
    int dim = 0x20 - palette_steps;
    for (int i = 0; i < 256; i++) {
        int r = gamepalette[i * 3] - dim * 8;
        int g = gamepalette[i * 3 + 1] - dim * 8;
        int b = gamepalette[i * 3 + 2] - dim * 8;
        pal[i].red = (uchar)(r < 0 ? 0 : r);
        pal[i].green = (uchar)(g < 0 ? 0 : g);
        pal[i].blue = (uchar)(b < 0 ? 0 : b);
        pal[i].alpha = 0;
    }
    int steps = 0;
    while (steps < 0x40) {
        uint32_t count = GetLapCount();
        if (count - lastcount > 10) {
            steps++;
            for (int i = 1; i < 256; i++) {
                if (pal[i].red > 7) pal[i].red -= 3;
                if (pal[i].green > 15) pal[i].green -= 2;
                if (pal[i].blue > 23) pal[i].blue -= 1;
            }
            MGL_setPalette(frontpage, pal, 256, 0);
            MGL_realizePalette(frontpage, 256, 0, 1);
            lastcount = count;
        }
    }
}

void GetPaletteFromGraphicsFile(void)
{
    for (int i = 0; i < 256; i++) {
        gamepalette[i * 3] = pal[i].red;
        gamepalette[i * 3 + 1] = pal[i].green;
        gamepalette[i * 3 + 2] = pal[i].blue;
    }
}
