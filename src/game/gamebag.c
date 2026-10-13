/*
 * gamebag.c - the game bag overlay and the trophy fee tally after a hunt
 * (gamebag.obj)
 */
#include "safari.h"

static int num_bag_gfx;
static const COORDS count_xy[7] = {
    {100, 65}, {320, 65}, {540, 65}, {100, 226}, {540, 226}, {100, 380}, {540, 380}
};

void EmptyHuntGameBag(void)
{
    hunter.gameBagNum = 0;
    for (int i = 0; i < 70; i++) {
        huntbag[i].shot = 0;
        huntbag[i].species = 0;
        huntbag[i].trophy_fee = 0;
        huntbag[i].charging = 0;
    }
}

void EmptyPlayerGameBag(void)
{
    for (int i = 0; i < 7; i++) player->gamebag[i] = 0;
}

void RestoreGamebagScreen(void)
{
    num_bag_gfx = LoadGraphics(0x46, "bkg_18");
    BlitGraphic(0x46, 1, 1, backpage);
    activeDC = backpage;
    for (int i = 0; i < 7; i++) {
        if (player->gamebag[i] != 0) {
            BlitGraphic(i + 0x47, 0, 0, backpage);
            ClrFontFormatCenter(0, count_xy[i].x, count_xy[i].y, "%d", (unsigned)player->gamebag[i]);
        }
    }
    ClrFontFormatRightJustify(0, 0x17e, 0x1bc, "$%d", player->dollars);
    BlitBackToFront();
    ShowMouse();
}

void DrawGamebagScreen(void)
{
    bFlipMouse = 0;
    RestoreGamebagScreen();
}

/* Count the day's bag and pay the trophy fees, animal by animal. */
void DrawTrophyScreen(void)
{
    int count[7];
    FadeToBlack();
    palette_steps = 0x20;
    num_bag_gfx = LoadGraphics(0x46, "bkg_18");
    GetPaletteFromGraphicsFile();
    ClrFontFree(0);
    ClrFontFree(1);
    ClrFontFree(2);
    ClrFontFree(3);
    LoadFonts("fnt_99.dat");
    BlitGraphic(0x46, 1, 1, backpage);
    BlitBackToFront();
    ShowMouse();
    BlitPageToPage(backpage, workpage, 0, 0, 0x280, 0x1e0);
    ClrFontFormatRightJustify(0, 0x17e, 0x1bc, "$%d", player->dollars);
    BlitBackToFront();
    FadeFromBlack();
    int dollars = (int)player->dollars;
    for (int k = 0; k < 70; k++) {
        if (huntbag[k].shot == 0) continue;
        if (!guide_said[0x33]) {
            DoCharacterComment(99, player->guide + 1, rand() % 2 + 0x33, 0x140, 0x1e0);
            guide_said[0x33] = 1;
        }
        dollars -= huntbag[k].trophy_fee;
    }
    if (dollars < 0)
        DoCharacterComment(99, player->guide + 1, rand() % 2 + 0x35, 0x140, 0x1e0);
    for (int i = 0; i < 7; i++) count[i] = 0;
    dollars = (int)player->dollars;
    for (int k = 0; k < 70; k++) {
        if (huntbag[k].shot == 0) continue;
        TimeDelay(0);
        int spc = huntbag[k].species;
        if (spc < 0 || spc > 6) continue;
        count[spc]++;
        activeDC = backpage;
        BlitGraphic(spc + 0x47, 0, 0, backpage);
        ClrFontFormatCenter(0, count_xy[spc].x, count_xy[spc].y, "%d", count[spc]);
        BlitBackToFront();
        if (huntbag[k].charging == 0 || huntbag[k].provoked != 0) {
            dollars -= huntbag[k].trophy_fee;
            player->trophy_fees += huntbag[k].trophy_fee;
        } else {
            /* the warden waives the fee for an unprovoked charge */
            DoCharacterComment(0xd, 1, 1, 0x140, 0x1e0);
        }
        SoundPlay(0xc);
        BlitPageToPage(workpage, backpage, 0xfa, 0x1b8, 0x8c, 0x1e);
        ClrFontFormatRightJustify(0, 0x17e, 0x1bc, "%d", dollars);
        BlitPageToPage(backpage, frontpage, 0xfa, 0x1b8, 0x8c, 0x1e);
        SoundWait(0xc);
        TimeDelay(1000);
        if (dollars < 0) {
            dollars = 0;
            player->dollars = 0;
            hunter.game_over = 2;
            DoCharacterComment(99, player->guide + 1, rand() % 2 + 0x35, 0x140, 0x1e0);
        }
        if (num_permits[spc] < count[spc]) hunter.game_over = 1;
        if (hunter.game_over != 0) {
            SwitchScreen(0x15);
            return;
        }
    }
    player->dollars = (uint32_t)dollars;
    TimeDelay(0);
    TimeDelay(1000);
    SwitchScreen(7);
}

void DumpGamebagScreen(void) { FreeGraphics(0x46, num_bag_gfx); }
void DumpTrophyScreen(void) { FreeGraphics(0x46, num_bag_gfx); }

void DoGamebagScreen(void)
{
    if (lastInput == 1) {
        lastInput = 0;
        SoundPlay(0);
        WaitMouseUp();
        TempSwitchScreen(0x11);
    }
}

void DoTrophyScreen(void) {}
