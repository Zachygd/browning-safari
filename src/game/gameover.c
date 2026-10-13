/*
 * gameover.c - endings: museum complete, mauled, malaria, jail (gameover.obj)
 *
 * hunter.game_over: 1 = over the permit limit, 2 = can't pay the fees,
 * 10 = finished, 20 = killed, 30 = other death; anything else is a
 * warden's comment number.
 */
#include "safari.h"

static void playGameOverMusic(void)
{
    if (player->music_volume == 0) {
        TimeDelay(0);
        TimeDelay(6000);
    } else {
        StartMusic("gameover");
        WaitMusic();
    }
}

static void showEnding(const char *bkg)
{
    LoadBackground(bkg);
    BlitWorkToBack();
    BlitBackToFront();
    FadeFromBlack();
    playGameOverMusic();
}

void DrawGameOverScreen(void)
{
    FadeToBlack();
    palette_steps = 0x20;
    if (hunter.game_over == 10) {
        showEnding("bkg_22");
        player->game_over = 1;
        ShowMouse();
        SwitchScreen(5);
    } else if (hunter.game_over == 0x14) {
        showEnding("bkg_23");
        StartOver();
        ShowMouse();
        SwitchScreen(0);
    } else if (hunter.game_over == 0x1e) {
        showEnding("bkg_24");
        StartOver();
        ShowMouse();
        SwitchScreen(0);
    } else {
        /* hauled before the game warden */
        LoadGraphics(0x62, "bkg_20");
        GetPaletteFromGraphicsFile();
        BlitGraphic(0x62, 0, 0, workpage);
        FreeGraphics(0x62, 1);
        BlitWorkToBack();
        BlitBackToFront();
        FadeFromBlack();
        ShowMouse();
        DoWardenJailComment(hunter.game_over, 0x96, 0x1df);
        FreeGraphics(99, 1);
        if (hunter.game_over == 1 || hunter.game_over == 2) {
            LoadBackground("bkg_21");
            LoadGraphics(100, "spr_21");
            BlitWorkToBack();
            BlitGraphic(hunter.game_over == 1 ? 100 : 0x65, 0x8c, 0x112, backpage);
            FreeGraphics(100, 2);
            BlitBackToFront();
            FadeFromBlack();
            playGameOverMusic();
            StartOver();
            ShowMouse();
            SwitchScreen(0);
        } else {
            hunter.game_over = 0;
            SwitchScreen(0);
        }
    }
}

void DumpGameOverScreen(void) {}
void DoGameOverScreen(void) {}
