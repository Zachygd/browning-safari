/*
 * screens.c - sound settings, village, travel agency, flight map, lodge,
 * lounge and hunting-area selection screens
 * (sound_set.obj, village.obj, Travel.obj, map.obj, lodge.obj,
 *  lounge.obj, huntmenu.obj)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "safari.h"

/* ---------------------------------------------------- sound settings -- */
void DoMusic(int param);
void DoVolume(int param);

static HOT_SPOT sound_hotspots[] = {
    {24, 186, 58, 40, 1, 0, 0, 0, 0, DoMusic, 1},
    {88, 180, 66, 48, 1, 0, 0, 1, 17, DoMusic, 2},
    {158, 183, 76, 46, 1, 0, 0, 2, 34, DoMusic, 3},
    {232, 179, 84, 51, 1, 0, 0, 3, 50, DoMusic, 4},
    {318, 177, 92, 56, 1, 0, 0, 4, 67, DoMusic, 5},
    {412, 175, 100, 60, 1, 0, 0, 5, 83, DoMusic, 6},
    {500, 170, 120, 75, 1, 0, 0, 6, 100, DoMusic, 7},
    {24, 291, 58, 39, 1, 0, 0, 7, 0, DoVolume, 8},
    {90, 289, 66, 44, 1, 0, 0, 8, 17, DoVolume, 9},
    {160, 288, 72, 48, 1, 0, 0, 9, 34, DoVolume, 10},
    {234, 288, 80, 52, 1, 0, 0, 10, 50, DoVolume, 11},
    {316, 284, 92, 58, 1, 0, 0, 11, 67, DoVolume, 12},
    {412, 282, 98, 61, 1, 0, 0, 12, 83, DoVolume, 13},
    {500, 278, 114, 65, 1, 0, 0, 13, 100, DoVolume, 14},
    {251, 452, 138, 22, 1, 1, 0, 14, 0, ExitScreen, -1},
};

void DoMusic(int param)
{
    player->music_volume = (uchar)param;
    music_volume = param;
    SoundSetVolume(0x10, player->music_volume);
    SoundPlay(1);
    SoundSetVolume(1, player->music_volume);
    BlitPageToPage(workpage, backpage, 10, 0x9a, 0x267, 0x5a);
    for (int i = 0; i < 7; i++) sound_hotspots[i].is_hilited = 0;
    HideMouse();
    for (int i = 0; i <= player->music_volume >> 4; i++) {
        sound_hotspots[i].is_hilited = 1;
        BlitGraphic(i + 0x29, sound_hotspots[i].x, sound_hotspots[i].y, backpage);
    }
    BlitPageToPage(backpage, frontpage, 10, 0x9a, 0x267, 0x5a);
    ShowMouse();
    player->saved_game = 0;
}

void DoVolume(int param)
{
    player->sound_volume = (uchar)param;
    sound_volume = param;
    SoundPlay(1);
    BlitPageToPage(workpage, backpage, 10, 0x116, 0x267, 0x5a);
    for (int i = 7; i < 14; i++) sound_hotspots[i].is_hilited = 0;
    HideMouse();
    for (int i = 7; i <= (player->sound_volume >> 4) + 7; i++) {
        sound_hotspots[i].is_hilited = 1;
        BlitGraphic(i + 0x29, sound_hotspots[i].x, sound_hotspots[i].y, backpage);
    }
    BlitPageToPage(backpage, frontpage, 10, 0x116, 0x267, 0x5a);
    ShowMouse();
    player->saved_game = 0;
}

void RestoreSoundScreen(void)
{
    for (int i = 0; i < 14; i++) sound_hotspots[i].is_hilited = 0;
    for (int i = 0; i <= player->music_volume >> 4; i++) {
        sound_hotspots[i].is_hilited = 1;
        BlitGraphic(i + 0x29, sound_hotspots[i].x, sound_hotspots[i].y, backpage);
    }
    for (int i = 7; i <= (player->sound_volume >> 4) + 7; i++) {
        sound_hotspots[i].is_hilited = 1;
        BlitGraphic(i + 0x29, sound_hotspots[i].x, sound_hotspots[i].y, backpage);
    }
    BlitBackToFront();
    ShowMouse();
    FadeFromBlack();
}

void DrawSoundScreen(void)
{
    FadeToBlack();
    LoadBackground("bkg_06");
    num_screen_gfx = LoadGraphics(0x29, "spr_06");
    screen_hotspots = sound_hotspots;
    BlitWorkToBack();
    RestoreSoundScreen();
}

/* ------------------------------------------------------------ village -- */
static void DoVillager(int param);

static HOT_SPOT village_hotspots[] = {
    {274, 166, 72, 258, 1, 0, 0, 0, 0, DoVillager, 1},
    {156, 398, 66, 34, 1, 1, 0, 0, 8, ExitScreen, -1},
};

/* Which villager picture applies: 0 = none, else chr_12_N.
 * The village grows as the player's trophy fees fund it. */
static int VillagerSet(void)
{
    uint32_t fees = player->trophy_fees;
    int talk = player->village_talk;
    if (fees < 3001 && talk < 2) return 1;
    if (fees >= 3001 && talk <= 5) return 1;
    if (fees >= 6001 && talk <= 7) return 2;
    if (fees >= 10001 && talk <= 8) return 3;
    if (fees >= 25001 && talk <= 10) return 3;
    if (fees >= 50001 && talk <= 11) return 4;
    if (fees > 100000 && talk < 13) return 4;
    return 0;
}

static void ShowVillager(void)
{
    char name[16];
    int set = VillagerSet();
    if (!set) return;
    snprintf(name, sizeof name, "chr_12_%d", set);
    LoadGraphics(0xd2, name);
    BlitGraphic(0xd2, 0x136, 0x1a6, workpage);
    FreeGraphics(0xd2, 7);
}

static void DoVillager(int param)
{
    (void)param;
    if (time_hour <= 5 || time_hour >= 20 || player->village_talk >= 13) return;
    uint32_t fees = player->trophy_fees;
    if (fees < 3001 && player->village_talk < 2) {
        DoCharacterComment(12, 1, player->village_talk + 1, 0x136, 0x1a6);
        player->village_talk++;
    } else if (fees >= 3001 && player->village_talk <= 5) {
        if (player->village_talk < 2) player->village_talk = 2;
        DoCharacterComment(12, 1, player->village_talk + 1, 0x136, 0x1a6);
        player->village_talk++;
    } else if (fees >= 6001 && player->village_talk <= 7) {
        ShowVillager();
        DoCharacterComment(12, 2, player->village_talk - 5, 0x136, 0x1a6);
        player->village_talk++;
    } else if (fees >= 10001 && player->village_talk <= 8) {
        ShowVillager();
        DoCharacterComment(12, 3, player->village_talk - 7, 0x136, 0x1a6);
        player->village_talk++;
    } else if (fees >= 25001 && player->village_talk <= 10) {
        DoCharacterComment(12, 3, player->village_talk - 7, 0x136, 0x1a6);
        player->village_talk++;
    } else if (fees >= 50001 && player->village_talk <= 11) {
        DoCharacterComment(12, 3, player->village_talk - 7, 0x136, 0x1a6);
        player->village_talk++;
    } else if (fees > 100000) {
        ShowVillager();
        DoCharacterComment(12, 4, 1, 0x136, 0x1a6);
        player->village_talk = 13;
    }
}

void DrawVillageScreen(void)
{
    FadeToBlack();
    LoadBackground("bkg_12");
    num_screen_gfx = LoadGraphics(0x29, "spr_12");
    screen_hotspots = village_hotspots;
    FreeGraphics(0, 2);
    LoadGraphics(0, "ptr_12");
    SetMouseGraphic(1);
    /* new buildings appear as the trophy fees grow */
    if (player->trophy_fees > 3000) BlitGraphic(0x2a, 0x20c, 0x102, workpage);
    if (player->trophy_fees > 6000) BlitGraphic(0x2b, 0, 0x6a, workpage);
    if (player->trophy_fees > 10000) BlitGraphic(0x2c, 0x126, 0x84, workpage);
    if (player->trophy_fees > 25000) BlitGraphic(0x2d, 0x180, 0x8c, workpage);
    if (player->trophy_fees > 50000) BlitGraphic(0x2e, 0x7a, 0xe8, workpage);
    if (player->trophy_fees > 100000) BlitGraphic(0x2f, 0, 0x114, workpage);
    StopMusic();
    if (time_hour > 5 && time_hour < 20) {
        StartMusic("bkg_12");
        if (player->village_talk < 13) ShowVillager();
    }
    BlitWorkToBack();
    BlitBackToFront();
    ShowMouse();
    FadeFromBlack();
}

/* ----------------------------------------------------- travel agency -- */
static HOT_SPOT travel_hotspots[] = {
    {10, 448, 118, 24, 1, 1, 0, 0, 0, ExitScreen, 1},
    {140, 448, 118, 24, 1, 1, 0, 1, 5, ExitScreen, 2},
    {26, 16, 126, 180, 1, 1, 0, 2, 1, PickGuide, 3},
    {312, 15, 126, 180, 1, 1, 0, 3, 0, PickGuide, -1},
};
static int travelWait;
static uint32_t travelCount, travelLast;
static int said_thing;

void PickGuide(int which)
{
    player->guide = (uchar)which;
    if (player->dollars >= 7000) {
        player->dollars -= 7000;
        SoundPlay(0);
        SwitchScreen(7);
    }
}

void DrawTravelScreen(void)
{
    FadeToBlack();
    LoadBackground("bkg_08");
    num_screen_gfx = LoadGraphics(0x29, "spr_08");
    screen_hotspots = travel_hotspots;
    FreeGraphics(0, 2);
    LoadGraphics(0, "ptr_08");
    SetMouseGraphic(1);
    ClrFontFree(0);
    ClrFontFree(1);
    LoadFonts("fnt_07.dat");
    BlitWorkToBack();
    ClrFontFormatRightJustify(0, 0x26d, 0x1c2, "$%d", player->dollars);
    BlitBackToFront();
    ShowMouse();
    FadeFromBlack();
    if (player->travel == 0) {
        DoCharacterComment(8, 1, 1, 0xf2, 0xd2);
        player->travel = 1;
        travelWait = 0;
    } else {
        travelWait = 3;
    }
}

void DoTravelScreen(void)
{
    if (travelWait != 0) {
        travelCount = GetLapCount();
        if (travelCount - travelLast > 600) {
            travelWait--;
            travelLast = travelCount;
        }
        if (travelWait == 0 && said_thing == 0) {
            DoCharacterComment(8, 1, (rand() % 2) + 2, 0xf2, 0xd2);
            said_thing = 1;
        }
    }
    DoScreen();
}

/* ------------------------------------------------------- flight map -- */
void DrawMapScreen(void)
{
    FadeToBlack();
    if (oldGame_Screen == 6) {
        /* flying out to Africa: a new 7 day safari starts */
        num_screen_gfx = LoadGraphics(0x29, "spr_09a");
        time_day = 0;
        time_hour = 6;
        time_min = 0;
        temperature = 14;
        DivideAnimals();
        EmptyPlayerGameBag();
    } else {
        palette_steps = 32;
        num_screen_gfx = LoadGraphics(0x29, "spr_09b");
        player->been_to_africa++;
    }
    LoadBackground("bkg_09");
    BlitWorkToBack();
    BlitBackToFront();
    FadeFromBlack();
    int xpos = 320, ypos = 240, width = 16, height = 16;
    StopMusic();
    StartMusic("bkg_09");
    for (int i = 0; i < 30; i++) {
        TimeDelay(0);
        BlitPageToPage(workpage, backpage, xpos, ypos, width, height);
        int num = i + 0x29;
        BlitPageToPage(backpage, frontpage, xpos, ypos, width, height);
        BlitGraphic(num, 320, 240, backpage);
        xpos = graphic[num].hotx + 320;
        ypos = graphic[num].hoty + 240;
        width = graphic[num].width;
        height = graphic[num].height;
        BlitPageToPage(backpage, frontpage, xpos, ypos, width, height);
        TimeDelay(60);
    }
    FreeGraphics(0, 2);
    LoadGraphics(0, "ptr_09");
    SetMouseGraphic(1);
    WaitMusic();
    StopMusic();
    ShowMouse();
}

void DoMapScreen(void)
{
    if (oldGame_Screen == 6) {
        FreeGraphics(0xbe, 1);
        LoadGraphics(0xbe, "dlg_09");
        FadeToBlack();
        ClearScreen();
        ClearBackScreen();
        EmptyHuntGameBag();
        ShowDay();
        SwitchScreen(8);
    } else {
        palette_steps = 32;
        SwitchScreen(5);
    }
}

/* -------------------------------------------------------------- lodge -- */
static void DoGoHome(int param);
static HOT_SPOT lodge_hotspots[] = {
    {538, 414, 56, 34, 1, 1, 0, 5, 16, ExitScreen, 1},
    {512, 20, 98, 40, 1, 1, 0, 3, 0, DoBed, 2},
    {36, 418, 58, 34, 1, 1, 0, 4, 0, DoGoHome, 3},
    {0, 218, 278, 170, 1, 1, 0, 0, 0, DoShop, 4},
    {434, 262, 206, 152, 1, 1, 0, 1, 9, ExitScreen, 5},
    {314, 96, 56, 36, 1, 1, 0, 2, 10, ExitScreen, -1},
};

void ShowDay(void)
{
    StretchGraphic(0xbe, 0xaa, 0x8c, 300, 200);
    ClrFontFree(0);
    LoadFonts("fnt_10.dat");
    if (time_day < 7) {
        ClrFontFormatCenter(0, 320, 0xd7, "Day %d", time_day + 1);
        ClrFontFormatCenter(0, 320, 0xfa, "Time: %d:%02d", time_hour, time_min);
    } else {
        ClrFontFormatCenter(0, 320, 0xd7, "Day 8");
        ClrFontFormatCenter(0, 320, 0xfa, "Time to go home");
    }
    FadeFromBlack();
    BlitBackToFront();
    TimeDelay(0);
    TimeDelay(1000);
    StopMusic();
    if (time_day == 7) {
        SwitchScreen(7);
    } else {
        hunter.lifebar_sun = 100;
        hunter.lifebar_other = 0;
        palette_steps = 16;
        StartMusic("bkg_10");
        ShowMouse();
    }
}

/* right click in the lodge shows the day and time */
void ShowTime(void)
{
    ClrFontFree(0);
    LoadFonts("fnt_10.dat");
    StretchGraphic(0xbe, 0xaa, 0x8c, 300, 200);
    if (time_day < 7) {
        ClrFontFormatCenter(0, 320, 0xd7, "Day %d", time_day + 1);
        ClrFontFormatCenter(0, 320, 0xfa, "Time: %d:%02d", time_hour, time_min);
    } else {
        ClrFontFormatCenter(0, 320, 0xd7, "Day 8");
        ClrFontFormatCenter(0, 320, 0xfa, "Time to go home");
    }
    HideMouse();
    BlitBackToFront();
    TimeDelay(0);
    TimeDelay(1000);
    BlitWorkToBack();
    BlitBackToFront();
    ShowMouse();
}

void DoBed(int param)
{
    (void)param;
    HideMouse();
    FadeToNight();
    time_day++;
    time_hour = 6;
    time_min = 0;
    palette_steps = 16;
    temperature = 14;
    StopMusic();
    StartMusic("bkg_10a");
    WaitMusic();
    StopMusic();
    ShowDay();
    BlitWorkToBack();
    BlitBackToFront();
    ShowMouse();
}

/* the airplane: go home (trophy room if anything was bagged, else the map) */
static void DoGoHome(int param)
{
    (void)param;
    int empty = 1;
    SoundPlay(0);
    for (int i = 0; i < 70; i++) {
        if (huntbag[i].shot != 0) { empty = 0; break; }
    }
    if (empty) SwitchScreen(7);
    else SwitchScreen(0x14);
}

void DoShop(int unused)
{
    (void)unused;
    if (time_hour < 6 || time_hour > 20) {
        /* the shop is closed */
        if (rand() % 2 == 0) DoCharacterVoice(14, 0, 0x18);
        else DoCharacterVoice(14, 1, 0x17);
    } else {
        SoundPlay(0);
        SwitchScreen(0xc);
    }
}

void DrawLodgeScreen(void)
{
    FadeToBlack();
    LoadBackground("bkg_10");
    num_screen_gfx = LoadGraphics(0x29, "spr_10");
    screen_hotspots = lodge_hotspots;
    FreeGraphics(0, 2);
    LoadGraphics(0, "ptr_10");
    SetMouseGraphic(1);
    FreeGraphics(0xbe, 1);
    LoadGraphics(0xbe, "dlg_10");
    BlitWorkToBack();
    BlitBackToFront();
    ShowMouse();
    FadeFromBlack();
}

void DoLodgeScreen(void)
{
    AdvanceClock();
    if (lastInput != 0) {
        if (lastInput == 1) {
            HOT_SPOT *hs = CheckHotSpots(screen_hotspots);
            if (hs) hs->fptr(hs->param);
        } else if (lastInput == 2) {
            ShowTime();
        } else if (lastInput == 4) {
            CheckHotSpots(screen_hotspots);
        }
        lastInput = 0;
    }
}

/* ------------------------------------------------------------- lounge -- */
static HOT_SPOT lounge_hotspots[] = {
    {144, 134, 180, 277, 1, 0, 0, 0, 0, DoGuide, 1},
    {556, 130, 60, 34, 1, 1, 0, 0, 8, ExitScreen, -1},
};

void DoGuide(int param)
{
    (void)param;
    if (time_hour > 5 && time_hour < 20) {
        DoCharacterComment(11, player->guide + 1, player->lounge_talk + 1, 0xed, 0x193);
        player->lounge_talk++;
        if (player->lounge_talk > 8) player->lounge_talk = 0;
    }
}

void DrawLoungeScreen(void)
{
    FadeToBlack();
    num_screen_gfx = LoadGraphics(0x29, "spr_11");
    LoadBackground("bkg_11");
    screen_hotspots = lounge_hotspots;
    FreeGraphics(0, 2);
    LoadGraphics(0, "ptr_11");
    SetMouseGraphic(1);
    if (time_hour < 20) {
        if (player->guide == 1) {
            LoadGraphics(0xd2, "chr_11_2");
            lounge_hotspots[0].x = 0x90;
            lounge_hotspots[0].y = 0x86;
            lounge_hotspots[0].w = 0xb4;
            lounge_hotspots[0].h = 0x115;
        } else {
            LoadGraphics(0xd2, "chr_11_1");
            lounge_hotspots[0].x = 0xb1;
            lounge_hotspots[0].y = 0xa7;
            lounge_hotspots[0].w = 0x84;
            lounge_hotspots[0].h = 0xf4;
        }
        BlitGraphic(0xd2, 0xed, 0x193, workpage);
    }
    FreeGraphics(0xd2, 7);
    BlitWorkToBack();
    BlitBackToFront();
    ShowMouse();
    FadeFromBlack();
}

/* ------------------------------------------------ hunting area select -- */
static HOT_SPOT location_hotspots[] = {
    {424, 0, 216, 200, 1, 1, 0, 0, 0, DoMap, 1},
    {232, 304, 298, 176, 1, 1, 0, 1, 1, DoMap, 2},
    {450, 208, 190, 272, 1, 1, 0, 2, 2, DoMap, 3},
    {0, 148, 202, 194, 1, 1, 0, 3, 3, DoMap, 4},
    {0, 0, 180, 164, 1, 1, 0, 4, 4, DoMap, 5},
    {142, 0, 286, 178, 1, 1, 0, 5, 5, DoMap, 6},
    {0, 268, 262, 212, 1, 1, 0, 6, 6, DoMap, 7},
    {258, 180, 190, 122, 1, 1, 0, 7, 8, ExitScreen, -1},
};

void DoMap(int which)
{
    SoundPlay(0);
    player->background = (uchar)which;
    SwitchScreen(0x11);
}

void DrawLocationScreen(void)
{
    FadeToBlack();
    LoadBackground("bkg_17");
    num_screen_gfx = LoadGraphics(0x29, "spr_17");
    screen_hotspots = location_hotspots;
    FreeGraphics(0, 2);
    LoadGraphics(0, "ptr_17");
    SetMouseGraphic(1);
    BlitWorkToBack();
    BlitBackToFront();
    ShowMouse();
    FadeFromBlack();
    StopMusic();
    StartMusic("bkg_17");
    MoveAnimals();
}

/* Keyboard hint on the area screen: flash the area holding most of a species. */
void LocationKey(int which)
{
    int most = 0, area = 0;
    for (int i = 0; i < 7; i++) {
        if (most < area_pop[i * 7 + which]) {
            most = area_pop[i * 7 + which];
            area = i;
        }
    }
    HideMouse();
    for (int i = 0; i < 5; i++) {
        TimeDelay(0);
        BlitWorkToBack();
        BlitGraphic(area + 0x29, location_hotspots[area].x, location_hotspots[area].y, backpage);
        BlitBackToFront();
        TimeDelay(60);
    }
    BlitWorkToBack();
    BlitBackToFront();
    ShowMouse();
}
