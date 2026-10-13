/*
 * intrface.c - the hunting HUD: clock, weather, life bar, ammunition and
 * the equipment icons along the bottom of the screen (intrface.obj)
 */
#include <stdio.h>
#include <string.h>
#include "safari.h"

int time_hour, time_total, time_day, time_min;
int temperature = 34;
static int wind_change = 0;
static int wind_direction = 0;   /* 0..14, compass rose frame */
int update_temp, update_sprite, update_score, update_lifebar, update_wind, update_clock;
int mouse_shape;                 /* cursor base for the current weapon */
int hunt_pointer;                /* cursor image currently shown over the scene */
int gunSight;                    /* 0..24, iron sight offset grid */
int lastMouseX, lastMouseY;
void (*DrawPointer)(void) = DrawMouse;
void (*CopyPointer)(void) = CopyMouse;
void (*DrawWeapon)(void) = DrawMouse;
void (*CopyWeapon)(void) = CopyMouse;
int weapon_array[3];             /* owns .458, .30-06, bow */

static int misc_showing;         /* 0 = sunscreen, 1 = bug spray */
static int binocs_showing;
static const int shot_max[3] = {5, 5, 1};
static uint32_t clock_count, sprite_lastcount, clock_lastcount;
static int num_hud_gfx;

static uchar score_save[0x20 * 0x10];
static uchar ammo_save[0x30 * 0x10];
static uchar canteen_save[0x20 * 0x29];
static uchar misc_save[0x22 * 0x32];

static void DoWeaponIcon(int param);
static void DoClockIcon(int param);
static void DoExitIcon(int param);
static void DoGameBagIcon(int param);
static void DoRegsIcon(int param);
static void DoCanteenIcon(int param);
static void DoBinocsIcon(int param);
static void DoMiscIcon(int param);
static void DoAmmoIcon(int param);

HOT_SPOT hunt_hotspots[] = {
    {134, 366, 128, 39, 1, 1, 0, 192, 7, DoWeaponIcon, 1},
    {240, 374, 82, 24, 1, 0, 0, 0, 1, DoClockIcon, 2},
    {328, 375, 100, 32, 1, 1, 0, 198, 8, DoAmmoIcon, 3},
    {16, 372, 120, 93, 1, 1, 0, 190, 0, DoExitIcon, 4},
    {168, 415, 38, 45, 1, 1, 0, 210, 3, DoGameBagIcon, 5},
    {204, 420, 56, 45, 1, 1, 0, 212, 2, DoRegsIcon, 6},
    {260, 415, 48, 47, 1, 1, 0, 208, 6, DoCanteenIcon, 7},
    {308, 420, 60, 49, 1, 1, 0, 204, 6, DoBinocsIcon, 8},
    {368, 413, 34, 50, 1, 1, 0, 206, 6, DoMiscIcon, -1},
};
enum { HS_WEAPON, HS_CLOCK, HS_AMMO, HS_EXIT, HS_BAG, HS_REGS, HS_CANTEEN, HS_BINOCS, HS_MISC };

void ToggleBinocs(void)
{
    lastInput = 1;
    DoBinocsIcon(0);
    lastInput = 0;
}

void LoadWeapon(void)
{
    lastInput = 1;
    DoAmmoIcon(0);
    lastInput = 0;
}

void PutAmmoBackInBox(void)
{
    if (hunter.ammo != -1)
        player->supplies[hunter.ammo + 0xd] += (ushort)hunter.gunShells;
}

/* Game time: one minute every half second; animals step every 100 ms. */
void AdvanceClock(void)
{
    clock_count = GetLapCount();
    if (clock_count - sprite_lastcount > 100) {
        update_sprite = 1;
        sprite_lastcount = clock_count;
    }
    if (clock_count - clock_lastcount <= 500) return;
    time_total++;
    time_min++;
    clock_lastcount = clock_count;
    if (time_min > 0x3b) {
        time_min = 0;
        /* sunrise and sunset */
        if (time_hour > 4 && time_hour < 9) palette_steps += 8;
        if (time_hour > 0x10 && time_hour < 0x15) palette_steps -= 8;
        if (palette_steps > 0x20) palette_steps = 0x20;
        if (palette_steps < 8) palette_steps = 8;
        FadeStep((char)palette_steps);
        time_hour++;
        if (time_hour > 0x17) {
            time_hour = 0;
            time_day++;
            if (time_day > 6) time_day = 0;
        }
        if (time_hour == 10 && game_screen == 0x11 && hunter.x6e == 0) {
            /* no boots */
            hunter.lifebar_other += 0x1e;
            update_lifebar = 1;
            pending_comment = rand() % 2 + 0x31;
        }
        if (time_hour == 0xd && game_screen == 0x11 && hunter.used_sunscreen == 0) {
            hunter.lifebar_other += 0x1e;
            update_lifebar = 1;
            pending_comment = 0x20;
        }
        if (time_hour == 0xf && game_screen == 0x11 && hunter.x6f == 0) {
            /* no hat */
            hunter.lifebar_other += 0x32;
            update_lifebar = 1;
            pending_comment = rand() % 2 + 0x29;
        }
        if (time_hour == 6 && game_screen == 8) ShowTime();
        if (time_hour < 4 || time_hour > 0xe) temperature -= 3;
        else temperature += 3;
    }
    update_clock = 1;
    if (wind_change == 0 && rand() % 0x1e == 10) wind_change = rand() % 0xc - 6;
    if (wind_change != 0 && rand() % 10 == 5) {
        if (wind_change < 0) {
            if (wind_direction != 0xf) {
                wind_direction++;
                if (wind_direction == 0xf) wind_direction = 0;
                update_wind = 1;
            }
            wind_change++;
        } else {
            if (wind_direction != 0) {
                wind_direction--;
                update_wind = 1;
            }
            wind_change--;
        }
    }
    if (game_screen == 0x11) {
        if (time_total % 4 == 0) {
            update_lifebar = 1;
            hunter.lifebar_sun--;
        }
        if (rand() % 0x32 == 0x18) {
            /* mosquito */
            if (hunter.has_pills == 0) {
                hunter.lifebar_other = 100;
                update_lifebar = 1;
                hunter.game_over = 0x14;
                if (guide_said[0x27] == 0) {
                    guide_said[0x27] = 1;
                    pending_comment = rand() % 2 + 0x27;
                }
            }
            if (hunter.used_bugspray) {
                SoundPlay(rand() % 3 + 0x13);
                return;
            }
            if (hunter.game_over != 0x14) SoundPlay(0x16);
        }
    }
}

void DrawLifebar(void)
{
    int lifebar = (int)hunter.lifebar_sun - (int)hunter.lifebar_other;
    if (lifebar < 0) {
        hunter.game_over = 0x14;
        lifebar = 0;
    }
    if (lifebar > 100) lifebar = 100;
    if (lifebar < 0x23) {
        if (pending_comment == 0 && guide_said[0x37] == 0) {
            DoGuideComment(player->guide + 1, rand() % 2 + 0x37);
            guide_said[0x37] = 1;
        }
    } else {
        guide_said[0x37] = 0;
    }
    lifebar *= 3;
    if (lifebar > 0x11d) lifebar = 0x11d;
    DrawRect(0x8c, 0x197, 0x11e, 5, 0xff, 1);
    if (lifebar > 200) DrawRect(0x8c, 0x197, lifebar, 5, 0xd9, 1);
    else if (lifebar > 100) DrawRect(0x8c, 0x197, lifebar, 5, 0xe9, 1);
    else DrawRect(0x8c, 0x197, lifebar, 5, 0xa5, 1);
}

void BlitLifebar(void)
{
    BlitPageToPage(backpage, frontpage, 0x8c, 0x197, 0x11d, 5);
    update_lifebar = 0;
}

void DrawCompass(void) { BlitGraphic(game_offset / 0xf0 + 0x100, 0x22a, 0x197, backpage); }
void BlitCompass(void) { BlitPageToPage(backpage, frontpage, 0x22a, 0x197, 0x34, 0x35); }
void DrawWind(void) { BlitGraphic(wind_direction + 0x100, 0x1c6, 0x197, backpage); }
void BlitWind(void) { BlitPageToPage(backpage, frontpage, 0x1c6, 0x197, 0x34, 0x35); }

void DrawClock(void)
{
    BlitPageToPage(workpage, backpage, 0xf4, 0x17c, 0x4c, 0x18);
    ClrFontFormatCenter(1, 0x119, 0x17f, "%d:%02d", time_hour, time_min);
}

void BlitClock(void)
{
    BlitPageToPage(backpage, frontpage, 0xf4, 0x17c, 0x4c, 0x18);
    update_clock = 0;
}

void DrawTemperature(void)
{
    BlitPageToPage(workpage, backpage, 0x1fe, 0x18e, 0x28, 0x1e);
    ClrFontFormatCenter(1, 0x214, 0x192, "%d^", temperature);
}

void BlitTemperature(void)
{
    BlitPageToPage(backpage, frontpage, 0x1fe, 0x18e, 0x28, 0x1e);
    update_temp = 0;
}

void DrawScore(void)
{
    RestoreImage(score_save, workpage, 0xc4, 0x180, 0x20, 0x10);
    RestoreImage(ammo_save, workpage, 0x17c, 0x180, 0x30, 0x10);
    activeDC = workpage;
    ClrFontFormatCenter(0, 0xd8, 0x17e, "%d", hunter.gunShells);
    if (hunter.has_ammo)
        ClrFontFormatCenter(0, 0x196, 0x17e, "%d", (unsigned)player->supplies[hunter.ammo + 0xd]);
    activeDC = backpage;
    BlitPageToPage(workpage, backpage, 0xc4, 0x180, 0x20, 0x10);
    BlitPageToPage(workpage, backpage, 0x17c, 0x180, 0x30, 0x10);
}

void BlitScore(void)
{
    BlitPageToPage(backpage, frontpage, 0xc4, 0x180, 0x20, 0x10);
    BlitPageToPage(backpage, frontpage, 0x17c, 0x180, 0x30, 0x10);
    update_score = 0;
}

static void flipMisc(void)
{
    BlitPageToPage(workpage, backpage, 0x170, 0x19d, 0x22, 0x32);
    BlitPageToPage(backpage, frontpage, 0x170, 0x19d, 0x22, 0x32);
    ShowMouse();
}

static void showBugspray(void)
{
    misc_showing = 1;
    hunt_hotspots[HS_MISC].graphic_num = 0xce;
    BlitGraphic(0xf6, hunt_hotspots[HS_MISC].x, hunt_hotspots[HS_MISC].y, workpage);
    HideMouse();
}

static void showSunscreen(void)
{
    misc_showing = 0;
    hunt_hotspots[HS_MISC].graphic_num = 0xd6;
    BlitGraphic(0xfe, hunt_hotspots[HS_MISC].x, hunt_hotspots[HS_MISC].y, workpage);
    HideMouse();
}

static void hideMisc(void)
{
    hunt_hotspots[HS_MISC].active = 0;
    HideMouse();
    RestoreImage(misc_save, workpage, 0x170, 0x19d, 0x22, 0x32);
}

/* sunscreen / bug spray: right click swaps, left click applies */
static void DoMiscIcon(int param)
{
    (void)param;
    if (lastInput == 2) {
        if (hunter.has_bugspray && misc_showing == 0) {
            showBugspray();
            flipMisc();
            SoundPlay(1);
            return;
        }
        if (hunter.has_sunscreen && misc_showing == 1) {
            showSunscreen();
            flipMisc();
            SoundPlay(1);
            return;
        }
    } else if (lastInput == 1) {
        player->saved_game = 0;
        if (misc_showing == 0 && hunter.has_sunscreen) {
            hunter.used_sunscreen = 1;
            SoundPlay(0xf);
            player->supplies[5]--;
            if (player->supplies[5] == 0) {
                hunter.has_sunscreen = 0;
                hunter.used_sunscreen = 0;
                guide_said[0x1f] = 1;
                pending_comment = rand() % 2 + 0x21;
                if (hunter.has_bugspray == 0) hideMisc();
                else showBugspray();
                flipMisc();
            }
        }
        if (misc_showing == 1 && hunter.has_bugspray) {
            hunter.used_bugspray = 1;
            SoundPlay(0xd);
            player->supplies[6]--;
            if (player->supplies[6] == 0) {
                hunter.has_bugspray = 0;
                hunter.used_bugspray = 0;
                guide_said[0x23] = 1;
                pending_comment = rand() % 2 + 0x25;
                if (hunter.has_sunscreen) {
                    showSunscreen();
                    flipMisc();
                    return;
                }
                hideMisc();
                flipMisc();
            }
        }
    }
}

static void DoCanteenIcon(int param)
{
    (void)param;
    if (!hunter.has_canteen) return;
    hunter.lifebar_sun += 0x14;
    if (hunter.lifebar_sun > 100) hunter.lifebar_sun = 100;
    update_lifebar = 1;
    player->saved_game = 0;
    SoundPlay(0xe);
    player->supplies[7]--;
    if (player->supplies[7] == 0) {
        hunt_hotspots[HS_CANTEEN].active = 0;
        hunter.has_canteen = 0;
        guide_said[0x1b] = 1;
        HideMouse();
        RestoreImage(canteen_save, workpage, 0x10c, 0x1a0, 0x20, 0x29);
        BlitPageToPage(workpage, backpage, 0x10c, 0x1a0, 0x20, 0x29);
        BlitPageToPage(backpage, frontpage, 0x10c, 0x1a0, 0x20, 0x29);
        ShowMouse();
        pending_comment = rand() % 2 + 0x1d;
    }
}

static void DoExitIcon(int param)
{
    (void)param;
    if (lastInput == 1) {
        SoundStopAll();
        SoundPlay(0);
        SwitchScreen(0x10);
    }
}

static void DoClockIcon(int param)
{
    (void)param;
    if (lastInput == 1) {
        HideMouse();
        AlertBox(" |Game Paused....| ", "Resume");
    }
}

static void DoRegsIcon(int param)
{
    (void)param;
    if (lastInput == 1 && hunter.has_license) {
        SoundPlay(0);
        update_score = 1;
        update_clock = 1;
        TempSwitchScreen(0x13);
    }
}

static void DoGameBagIcon(int param)
{
    (void)param;
    if (lastInput == 1) {
        SoundPlay(0);
        update_score = 1;
        update_clock = 1;
        TempSwitchScreen(0x12);
    }
}

static void DoBinocsIcon(int param)
{
    (void)param;
    if (lastInput == 1 && hunter.has_binocs) {
        SoundPlay(0);
        if (binocs_showing == 0) {
            binocs_showing = 1;
            SwitchWeapon(200);
            return;
        }
        binocs_showing = 0;
        SwitchWeapon(hunter.weapon);
    }
}

static int weaponIcon(void)
{
    switch (hunter.weapon) {
    case 0: hunt_hotspots[HS_WEAPON].graphic_num = 0xc0; return 0xe8;
    case 1: hunt_hotspots[HS_WEAPON].graphic_num = 0xc2; return 0xea;
    case 2: hunt_hotspots[HS_WEAPON].graphic_num = 0xc4; return 0xec;
    }
    return -1;
}

/* right click cycles through the weapons the player owns */
static void DoWeaponIcon(int param)
{
    (void)param;
    if (lastInput != 2 || !hunter.has_weapon) return;
    update_score = 1;
    if (hunter.has_ammo) player->supplies[hunter.ammo + 0xd] += (ushort)hunter.gunShells;
    hunter.weapon++;
    hunter.gun_empty = 1;
    hunter.gunShells = 0;
    if (hunter.weapon > 2) hunter.weapon = 0;
    while (hunter.weapon < 3 && weapon_array[hunter.weapon] == 0) hunter.weapon++;
    if (hunter.weapon > 2) {
        hunter.weapon = 0;
        while (hunter.weapon < 2 && weapon_array[hunter.weapon] == 0) hunter.weapon++;
    }
    SoundPlay(1);
    HideMouse();
    int icon = weaponIcon();
    if (icon >= 0) BlitGraphic(icon, hunt_hotspots[HS_WEAPON].x, hunt_hotspots[HS_WEAPON].y, workpage);
    SaveImage(workpage, score_save, 0xc4, 0x180, 0x20, 0x10);
    hunter.ammo = -1;
    hunter.has_ammo = 0;
    if (hunter.weapon == 0) { if (player->supplies[0xd]) hunter.ammo = 0; }
    else if (hunter.weapon == 1) { if (player->supplies[0xe]) hunter.ammo = 1; }
    else if (hunter.weapon == 2 && player->supplies[0xf]) hunter.ammo = 2;
    if (hunter.ammo == -1) {
        hunt_hotspots[HS_AMMO].active = 0;
    } else {
        hunter.has_ammo = 1;
        hunt_hotspots[HS_AMMO].graphic_num = hunter.ammo * 2 + 0xc6;
        BlitGraphic(hunter.ammo * 2 + 0xee, hunt_hotspots[HS_AMMO].x, hunt_hotspots[HS_AMMO].y, workpage);
        hunt_hotspots[HS_AMMO].active = 1;
        SaveImage(workpage, ammo_save, 0x17c, 0x180, 0x30, 0x10);
    }
    BlitPageToPage(workpage, backpage, 0x86, 0x16e, 0x80, 0x27);
    BlitPageToPage(backpage, frontpage, 0x86, 0x16e, 0x80, 0x27);
    BlitPageToPage(workpage, backpage, 0x148, 0x177, 100, 0x20);
    BlitPageToPage(backpage, frontpage, 0x148, 0x177, 100, 0x20);
    ShowMouse();
    if (hunter.weapon != -1) SwitchWeapon(hunter.weapon);
    else SwitchWeapon(100);
}

/* left click loads one round from the box */
static void DoAmmoIcon(int param)
{
    (void)param;
    if (!hunter.has_ammo) return;
    update_score = 1;
    if (lastInput == 2 || lastInput != 1) return;
    ushort *box = &player->supplies[hunter.ammo + 0xd];
    if (hunter.gunShells < shot_max[hunter.weapon] && *box != 0) {
        hunter.gun_empty = 0;
        hunter.gunShells++;
        (*box)--;
        RestoreImage(score_save, workpage, 0xc4, 0x180, 0x20, 0x10);
        if (hunter.weapon == 2 && player->supplies[0xf] == 0) {
            /* arrows are recovered after each shot, occasionally one breaks */
            player->supplies[hunter.ammo + 0xd] = (ushort)hunter.totalArrows;
            if (rand() % 100 == 0x32) {
                player->supplies[0xf]--;
                hunter.totalArrows--;
                update_score = 1;
            }
        }
        SoundPlay(hunter.weapon + 7);
        if (player->supplies[hunter.ammo + 0xd] == 0) {
            int count = 0;
            for (int i = 0; i < 4; i++)
                if (player->supplies[i] != 0) count++;
            if (count == 0) {
                hunter.has_ammo = 0;
                hunter.ammo = -1;
            }
        }
    } else if (hunter.gunShells > 0 || *box != 0) {
        SoundPlay(5);
    }
}

void InitInterfaceIcons(void)
{
    num_hud_gfx = LoadGraphics(0xe6, "spr_99");
    BlitGraphic(0xe6, 0, 0x168, workpage);
    FreeGraphics(0xe6, 1);
    SaveImage(workpage, canteen_save, 0x10c, 0x1a0, 0x20, 0x29);
    SaveImage(workpage, misc_save, 0x170, 0x19d, 0x22, 0x32);
    memset(guide_said, 0, 65 < sizeof guide_said ? 65 : sizeof guide_said);
    int license = player->supplies[0] != 0;
    hunter.has_binocs = player->supplies[9] != 0;
    hunter.has_scope[0] = player->scopes[0] != 0;
    hunter.has_scope[1] = player->scopes[1] != 0;
    hunter.has_canteen = player->supplies[7] != 0;
    hunter.has_bugspray = player->supplies[6] != 0;
    hunter.has_sunscreen = player->supplies[5] != 0;
    hunter.has_pills = player->supplies[0x10] != 0;
    hunter.x6e = player->supplies[2] != 0;
    hunter.x6f = player->supplies[1] != 0;
    hunter.weapon = -1;
    weapon_array[2] = player->supplies[0xc] != 0;
    if (weapon_array[2]) hunter.weapon = 2;
    weapon_array[1] = player->supplies[0xb] != 0;
    if (weapon_array[1]) hunter.weapon = 1;
    hunter.has_weapon = weapon_array[1] || weapon_array[2];
    weapon_array[0] = player->supplies[10] != 0;
    if (weapon_array[0]) {
        hunter.weapon = 0;
        hunter.has_weapon = 1;
    }
    hunter.has_ammo = 0;
    hunter.ammo = -1;
    if (hunter.weapon == 0) { if (player->supplies[0xd]) { hunter.ammo = 0; hunter.has_ammo = 1; } }
    else if (hunter.weapon == 1) { if (player->supplies[0xe]) { hunter.ammo = 1; hunter.has_ammo = 1; } }
    else if (hunter.weapon == 2 && player->supplies[0xf]) { hunter.ammo = 2; hunter.has_ammo = 1; }
    hunter.totalArrows = player->supplies[0xf];
    hunter.has_license = (char)license;
    if (license) BlitGraphic(0xfc, hunt_hotspots[HS_REGS].x, hunt_hotspots[HS_REGS].y, workpage);
    hunt_hotspots[HS_REGS].active = license;
    if (hunter.has_binocs) BlitGraphic(0xf4, hunt_hotspots[HS_BINOCS].x, hunt_hotspots[HS_BINOCS].y, workpage);
    hunt_hotspots[HS_BINOCS].active = hunter.has_binocs != 0;
    if (!hunter.has_weapon) {
        hunt_hotspots[HS_WEAPON].active = 0;
    } else {
        int icon = weaponIcon();
        if (icon >= 0) BlitGraphic(icon, hunt_hotspots[HS_WEAPON].x, hunt_hotspots[HS_WEAPON].y, workpage);
        hunt_hotspots[HS_WEAPON].active = 1;
    }
    SaveImage(workpage, score_save, 0xc4, 0x180, 0x20, 0x10);
    if (hunter.has_ammo) {
        hunt_hotspots[HS_AMMO].graphic_num = hunter.ammo * 2 + 0xc6;
        BlitGraphic(hunter.ammo * 2 + 0xee, hunt_hotspots[HS_AMMO].x, hunt_hotspots[HS_AMMO].y, workpage);
    }
    hunt_hotspots[HS_AMMO].active = hunter.has_ammo != 0;
    SaveImage(workpage, ammo_save, 0x17c, 0x180, 0x30, 0x10);
    if (hunter.has_canteen) BlitGraphic(0xf8, hunt_hotspots[HS_CANTEEN].x, hunt_hotspots[HS_CANTEEN].y, workpage);
    hunt_hotspots[HS_CANTEEN].active = hunter.has_canteen != 0;
    hunt_hotspots[HS_MISC].active = 0;
    if (hunter.has_bugspray) {
        BlitGraphic(0xf6, hunt_hotspots[HS_MISC].x, hunt_hotspots[HS_MISC].y, workpage);
        misc_showing = 1;
        hunt_hotspots[HS_MISC].graphic_num = 0xce;
    }
    hunt_hotspots[HS_MISC].active = hunter.has_bugspray != 0;
    if (hunter.has_sunscreen) {
        BlitGraphic(0xfe, hunt_hotspots[HS_MISC].x, hunt_hotspots[HS_MISC].y, workpage);
        hunt_hotspots[HS_MISC].active = 1;
        hunt_hotspots[HS_MISC].graphic_num = 0xd6;
        misc_showing = 0;
    }
    binocs_showing = 0;   /* PORT: the original kept this across hunts */
    if (hunter.weapon == -1) SwitchWeapon(100);
    else SwitchWeapon(hunter.weapon);
}

void ExitInterfaceIcons(void)
{
    FreeGraphics(0xe6, num_hud_gfx);
}
