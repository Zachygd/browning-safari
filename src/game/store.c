/*
 * store.c - Mr. Snidley's museum, the safari shop, suitcase, supplies,
 * scope picker and the hunting handbook
 * (Museum.obj, shop.obj, suitcase.obj, supplies.obj, scopes.obj, regs.obj)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "safari.h"

/* screen save buffers shared by the overlay screens */
static uchar temp_save[0x8000];
static uchar temp_save2[0x8000];
static uchar temp_save3[0x8000];
static uchar temp_save4[0x8000];

/* draw "$NNN" into the money box (back page) and show it */
static void ShowDollars(int x)
{
    ClrFontFormatRightJustify(0, x, 0x1c2, "$%d", player->dollars);
}

/* Load a wide canvas (.256 single image) into a DC and take its palette. */
int LoadCanvas(const char *file, MGLDC *page)
{
    uchar bitmapHdr[8], hdrpal[768], bitmapInfo[16];
    uchar *surface = page->surface;
    int bytesPerLine = page->bytesPerLine;
    snprintf(filename, sizeof filename, "%s%s.256", cdPath, file);
    FHANDLE fh = FileOpen(filename);
    if (fh == BAD_HANDLE) {
        Log("ERROR: LoadCanvas: %s not found!", filename);
        return 0;
    }
    FileRead(fh, bitmapHdr, 8);
    FileRead(fh, hdrpal, 0x300);
    memcpy(gamepalette, hdrpal, 0x300);
    FileRead(fh, bitmapInfo, 0x10);
    short w = (short)(bitmapInfo[4] | bitmapInfo[5] << 8);
    short h = (short)(bitmapInfo[6] | bitmapInfo[7] << 8);
    for (int y = 0; y < h && y < page->height; y++) {
        int n = w < page->width ? w : page->width;
        FileRead(fh, surface, (uint32_t)n);
        if (w > n) FileSeek(fh, w - n, 1);
        surface += bytesPerLine;
    }
    FileClose(fh);
    return 1;
}

/* pixel test against a graphic: is (xpos,ypos) inside the drawn shape? */
int CheckBitmapData(int which, int xpos, int ypos)
{
    int offset = graphic[which].width * ypos + xpos;
    if (!graphic[which].data) return 0;
    if (graphic[which].height * graphic[which].width < offset || offset < 0) {
        Log("ERROR: CheckBitmapData: offset = %d", offset);
        return 0;
    }
    return graphic[which].data[offset] != 0;
}

/* =============================================================== museum */
static void DoTravelButton(int param);
static void DoElevator(int param) { (void)param; }
static void DoMuseumItem(int param) { (void)param; }

static const TROPHY_ITEM trophy_items[7] = {
    {30000, 15000, 678}, {10000, 6000, 946}, {3000, 500, 572}, {20000, 10500, 836},
    {200000, 75000, 474}, {9000, 3600, 274}, {7000, 2000, 86},
};
static const ART_ITEM art_items[15] = {
    {550, 1136}, {190, 386}, {700, 106}, {2500, 1120}, {600, 1230}, {4000, 8}, {2500, 1076},
    {70, 22}, {150, 36}, {1800, 302}, {550, 62}, {240, 874}, {90, 946}, {90, 972}, {170, 82},
};

static HOT_SPOT museum_hotspots[] = {
    {678, 96, 148, 148, 1, 0, 0, 1, 0, DoMuseumItem, 1},
    {946, 220, 198, 120, 1, 0, 0, 6, 1, DoMuseumItem, 2},
    {572, 254, 130, 176, 1, 0, 0, 5, 2, DoMuseumItem, 3},
    {836, 142, 140, 132, 1, 0, 0, 0, 3, DoMuseumItem, 4},
    {474, 88, 106, 156, 1, 0, 0, 2, 4, DoMuseumItem, 5},
    {274, 126, 156, 164, 1, 0, 0, 3, 5, DoMuseumItem, 6},
    {86, 130, 172, 218, 1, 0, 0, 4, 6, DoMuseumItem, 7},
    {1136, 226, 88, 200, 1, 0, 0, 7, 23, DoMuseumItem, 8},
    {386, 112, 24, 26, 1, 0, 0, 8, 17, DoMuseumItem, 9},
    {106, 90, 106, 22, 1, 0, 0, 9, 18, DoMuseumItem, 10},
    {1120, 160, 48, 70, 1, 0, 0, 10, 19, DoMuseumItem, 11},
    {1230, 96, 50, 220, 1, 0, 0, 11, 20, DoMuseumItem, 12},
    {8, 34, 86, 88, 1, 0, 0, 12, 21, DoMuseumItem, 13},
    {1076, 160, 44, 64, 1, 0, 0, 13, 22, DoMuseumItem, 14},
    {22, 148, 40, 30, 1, 0, 0, 14, 24, DoMuseumItem, 15},
    {36, 204, 46, 40, 1, 0, 0, 15, 25, DoMuseumItem, 16},
    {302, 100, 26, 38, 1, 0, 0, 16, 26, DoMuseumItem, 17},
    {62, 230, 84, 198, 1, 0, 0, 17, 27, DoMuseumItem, 18},
    {874, 104, 24, 32, 1, 0, 0, 18, 28, DoMuseumItem, 19},
    {946, 110, 28, 28, 1, 0, 0, 19, 29, DoMuseumItem, 20},
    {972, 104, 26, 34, 1, 0, 0, 20, 30, DoMuseumItem, 21},
    {82, 112, 42, 30, 1, 0, 0, 21, 31, DoMuseumItem, 22},
    {616, 110, 30, 88, 0, 0, 0, 22, 0, DoElevator, 23},
    {8, 447, 120, 26, 1, 1, 0, 24, 0, ExitScreen, 24},
    {138, 447, 120, 26, 1, 1, 0, 25, 0, DoTravelButton, -1},
};

static MGLDC *museumDC;
static int museum_gfx;
static int museum_offset = 320;
static int museum_wait;
static uint32_t museum_count, museum_last;

static void DoTravelButton(int param)
{
    (void)param;
    SoundPlay(0);
    if (player->dollars < 7000) {
        hunter.game_over = 30;
        SwitchScreen(0x15);
        return;
    }
    SwitchScreen(6);
}

void RestoreMuseumScreen(void)
{
    ShowDollars(0x26d);
    BlitBackToFront();
    ShowMouse();
    FadeFromBlack();
}

static void MuseumRefresh(void)
{
    MGL_bitBltCoord(workpage, museumDC, museum_offset, 0, museum_offset + 640, 440, 0, 0, 0);
    BlitGraphic(0x40, 0, 440, workpage);
    BlitWorkToBack();
    BlitGraphic(0xd2, 320, 440, backpage);
    ShowDollars(0x26d);
    BlitBackToFront();
    ShowMouse();
}

static void MuseumMoney(void)
{
    BlitPageToPage(workpage, backpage, 0x212, 0x1c1, 100, 0x18);
    ShowDollars(0x26d);
    BlitPageToPage(backpage, frontpage, 0x212, 0x1c1, 100, 0x18);
    ShowMouse();
}

static void PlaceTrophy(int i)
{
    HOT_SPOT *hs = &museum_hotspots[i];
    hs->x = trophy_items[i].xpos - museum_offset;
    BlitGraphic(hs->graphic_num + 0x29, museum_offset + hs->x, hs->y, museumDC);
}

static void PlaceArt(int i)
{
    HOT_SPOT *hs = &museum_hotspots[7 + i];
    hs->x = art_items[i].xpos - museum_offset;
    BlitGraphic(hs->graphic_num + 0x29, museum_offset + hs->x, hs->y, museumDC);
}

void DrawMuseumScreen(void)
{
    museumDC = MGL_createMemoryDC(1280, 440);
    if (!museumDC) return;
    FadeToBlack();
    LoadCanvas("bkg_07", museumDC);
    museum_gfx = LoadGraphics(0x29, "spr_07");
    FreeGraphics(0, 4);
    LoadGraphics(0, "ptr_07");
    SetMouseGraphic(1);
    ClrFontFree(0);
    ClrFontFree(1);
    LoadFonts("fnt_07.dat");
    museum_offset = 320;
    for (int i = 0; i < 7; i++)
        if (player->museum_trophy[i]) PlaceTrophy(i);
    for (int i = 0; i < 15; i++)
        if (player->museum_art[i]) PlaceArt(i);
    if (plug_in) {
        museum_hotspots[22].x = 0x268 - museum_offset;
        BlitGraphic(museum_hotspots[22].graphic_num + 0x29, museum_hotspots[22].x + museum_offset,
                    museum_hotspots[22].y, museumDC);
    }
    museum_hotspots[22].active = plug_in != 0;
    MGL_bitBltCoord(workpage, museumDC, museum_offset, 0, museum_offset + 640, 440, 0, 0, 0);
    BlitGraphic(0x40, 0, 440, workpage);
    BlitWorkToBack();
    RestoreMuseumScreen();
    SoundFade(0x10);
    StopMusic();
    HideMouse();
    museum_wait = 0;
    InitSnidleyComment();

    if (player->res[0] == 0) {
        /* first visit: Mr. Snidley sponsors the safari */
        DoSnidleyComment(1);
        player->dollars += 10000;
        player->res[0] = 1;
        player->saved_game = 0;
        MuseumMoney();
        ExitSnidleyComment();
        return;
    }
    if (player->last_screen != 7) {
        if (player->museum_complete) DoSnidleyComment(rand() % 2 + 3);
        else museum_wait = 6;
        ExitSnidleyComment();
        return;
    }

    /* back from a safari: hand over trophies and souvenirs */
    int bagged = 0;
    for (int i = 0; i < 7; i++) if (player->gamebag[i]) { bagged = 1; break; }
    DoSnidleyComment(bagged ? rand() % 2 + 10 : rand() % 2 + 12);
    for (int i = 0, c = 14; i < 7; i++, c += 4) {
        TimeDelay(0);
        TimeDelay(60);
        int show = 0;
        player->saved_game = 0;
        int n = player->gamebag[i];
        if (n == 1) {
            show = 1;
            if (player->museum_trophy[i] == 0) {
                DoSnidleyComment(c);
                player->dollars += trophy_items[i].reward1;
                player->gamebag[i] = 0;
                player->museum_trophy[i] = 1;
            } else {
                DoSnidleyComment(c + 2);
                player->dollars += trophy_items[i].reward2;
                player->gamebag[i] = 0;
            }
        } else if (n > 1) {
            show = 1;
            if (player->museum_trophy[i] != 0) {
                DoSnidleyComment(c + 3);
                player->dollars += trophy_items[i].reward2;
                for (int k = 1; k < player->gamebag[i]; k++) player->dollars += trophy_items[i].reward2;
                player->gamebag[i] = 0;
            } else {
                DoSnidleyComment(c + 1);
                player->dollars += trophy_items[i].reward1;
                for (int k = 1; k < player->gamebag[i]; k++) player->dollars += trophy_items[i].reward2;
                player->gamebag[i] = 0;
                player->museum_trophy[i] = 1;
            }
        }
        if (show) {
            PlaceTrophy(i);
            MuseumRefresh();
        }
    }
    for (int i = 0; i < 15; i++)
        if (player->suitcase[i]) { DoSnidleyComment(rand() % 2 + 0x2c); break; }
    for (int i = 0; i < 15; i++) {
        TimeDelay(0);
        TimeDelay(60);
        if (player->suitcase[i]) {
            player->saved_game = 0;
            DoSnidleyComment(i + 0x2e);
            player->dollars += art_items[i].reward;
            player->suitcase[i] = 0;
            player->museum_art[i] = 1;
            PlaceArt(i);
            MuseumRefresh();
        }
    }
    if (player->museum_complete && !player->bonus_given) {
        DoSnidleyComment(rand() % 2 + 5);
        player->dollars += 100000;
        player->bonus_given = 1;
        MuseumMoney();
    }
    if (player->museum_trophy[0] && player->museum_trophy[3] && player->museum_trophy[4] &&
        !player->bagged_big_3) {
        /* buffalo, lion and rhino: the "Big 3" */
        DoSnidleyComment(9);
        ExitSnidleyComment();
        player->dollars += 25000;
        MuseumMoney();
        hunter.game_over = 10;
        player->bagged_big_3 = 1;
        SwitchScreen(0x15);
        return;
    }
    int i;
    for (i = 0; i < 7 && player->museum_trophy[i]; i++) {}
    if (i == 7) {
        for (i = 0; i < 15 && player->museum_art[i]; i++) {}
        if (i == 15) player->museum_complete = 1;
    }
    if (player->dollars < 7000) {
        /* broke: can't afford another trip */
        DoSnidleyComment(rand() % 2 + 0x2a);
        ExitSnidleyComment();
        hunter.game_over = 30;
        ShowMouse();
        SwitchScreen(0x15);
        return;
    }
    ExitSnidleyComment();
}

void DumpMuseumScreen(void)
{
    MGL_destroyDC(museumDC);
    museumDC = NULL;
    FreeGraphics(0x29, museum_gfx);
}

void MoveMuseum(int direction)
{
    if (!ScrollTick()) return;
    museum_offset += direction;
    if (museum_offset < 0) museum_offset = 0;
    if (museum_offset > 0x271) museum_offset = 0x271;
    MGL_bitBltCoord(workpage, museumDC, museum_offset, 0, museum_offset + 640, 440, 0, 0, 0);
    BlitPageToPage(workpage, backpage, 0, 0, 640, 440);
    for (int i = 0; i < 7; i++) museum_hotspots[i].x = trophy_items[i].xpos - museum_offset;
    /* (the 1998 build updated hot spots 0-14 here; the art spots are 7-21) */
    for (int i = 0; i < 15; i++) museum_hotspots[7 + i].x = art_items[i].xpos - museum_offset;
    museum_hotspots[22].x = 0x268 - museum_offset;
    HideMouse();
    BlitPageToPage(backpage, frontpage, 0, 0, 640, 440);
    ShowMouse();
}

void DoMuseumScreen(void)
{
    if (museum_wait != 0) {
        museum_count = GetLapCount();
        if (museum_count - museum_last > 600) {
            museum_wait--;
            museum_last = museum_count;
        }
        if (museum_wait == 0) {
            int comment = player->been_to_africa == 0 ? 7 : rand() % 2 + 7;
            DoCharacterComment(7, 1, comment, 320, 440);
        }
    }
    int scrolled = 0;
    if (mouseY < 400) {
        if (mouseX < 50 && museum_offset != 0) {
            if (GetMouseGraphic() != 2) SetMouseGraphic(2);
            MoveMuseum(mouseX - 50);
            scrolled = 1;
        } else if (mouseX > 0x24e && museum_offset < 0x271) {
            if (GetMouseGraphic() != 3) SetMouseGraphic(3);
            MoveMuseum(mouseX - 0x24e);
            scrolled = 1;
        }
    }
    if (!scrolled && GetMouseGraphic() > 1) {
        HideMouse();
        SetMouseGraphic(0);
        ShowMouse();
    }
    if (lastInput != 0) {
        if (lastInput == 1) {
            HOT_SPOT *hs = CheckHotSpots(museum_hotspots);
            if (hs) hs->fptr(hs->param);
        } else if (lastInput == 4) {
            CheckHotSpots(museum_hotspots);
        }
        lastInput = 0;
    }
}

/* ================================================================= shop */
static void ExitToLodge(int param);
static void ExitShop(int param);

static SHOP_ITEM shop_items[32] = {
    {"Handbook", 20, 1090, 1, 3}, {"Safari hat", 30, 562, 1, 2}, {"Shoes", 100, 568, 1, 3},
    {"Safari shirt", 35, 412, 1, 5}, {"Safari pants", 45, 418, 1, 1}, {"Sunscreen", 5, 952, 20, 4},
    {"Repellant", 5, 888, 25, 3}, {"Canteen", 5, 86, 10, 4}, {"Scope", 250, 276, 1, 2},
    {"Binoculars", 75, 1000, 1, 2}, {"458 rifle", 1100, 98, 1, 1}, {".30-06 rifle", 800, 50, 1, 1},
    {"Bow", 650, 1098, 1, 1}, {"458 ammo", 50, 80, 20, 10}, {".30-06 ammo", 20, 0, 20, 10},
    {"Arrows & quiver", 40, 1154, 4, 1}, {"Malaria pills", 50, 788, 1, 10},
    {"African doll", 275, 184, 1, 1}, {"Owl", 95, 268, 1, 1}, {"Fish carving", 350, 264, 1, 1},
    {"Man's head", 1250, 390, 1, 1}, {"Wood carving", 300, 514, 1, 1}, {"Mask", 390, 704, 1, 1},
    {"Woman's head", 1250, 802, 1, 1}, {"Sugar bowls", 35, 778, 1, 1}, {"Coasters", 75, 888, 1, 1},
    {"Elephant", 460, 924, 1, 1}, {"African doll", 275, 952, 1, 1}, {"Luck token", 120, 1098, 1, 1},
    {"Small ele", 45, 1166, 1, 1}, {"Small ele", 45, 1219, 1, 1}, {"Chameleon", 85, 1150, 1, 1},
};

static HOT_SPOT shop_hotspots[] = {
    {1090, 354, 56, 72, 1, 1, 0, 91, 0, BuyItem, 1},
    {562, 268, 84, 52, 1, 1, 0, 40, 1, BuyItem, 2},
    {568, 328, 78, 50, 1, 1, 0, 43, 2, BuyItem, 3},
    {412, 274, 92, 48, 1, 1, 0, 31, 3, BuyItem, 4},
    {418, 338, 88, 36, 1, 1, 0, 34, 4, BuyItem, 5},
    {952, 270, 46, 56, 1, 1, 0, 67, 5, BuyItem, 6},
    {888, 266, 38, 52, 1, 1, 0, 58, 6, BuyItem, 7},
    {86, 362, 74, 74, 1, 1, 0, 13, 7, BuyItem, 8},
    {276, 290, 116, 28, 1, 1, 0, 22, 8, BuyItem, 9},
    {1000, 50, 80, 124, 1, 1, 0, 70, 9, BuyItem, 10},
    {98, 10, 72, 268, 1, 1, 0, 4, 10, BuyItem, 11},
    {50, 2, 66, 280, 1, 1, 0, 1, 11, BuyItem, 12},
    {1098, 6, 78, 266, 1, 1, 0, 76, 12, BuyItem, 13},
    {80, 312, 76, 40, 1, 1, 0, 10, 13, BuyItem, 14},
    {0, 318, 86, 46, 1, 1, 0, 7, 14, BuyItem, 15},
    {1154, 134, 74, 146, 1, 1, 0, 79, 15, BuyItem, 16},
    {788, 270, 34, 46, 1, 1, 0, 52, 16, BuyItem, 17},
    {184, 242, 138, 198, 1, 1, 0, 16, 17, BuyItem, 18},
    {268, 172, 60, 96, 1, 1, 0, 19, 18, BuyItem, 19},
    {264, 340, 140, 50, 1, 1, 0, 25, 19, BuyItem, 20},
    {390, 120, 80, 144, 1, 1, 0, 28, 20, BuyItem, 21},
    {514, 32, 50, 260, 1, 1, 0, 37, 21, BuyItem, 22},
    {704, 64, 72, 98, 1, 1, 0, 46, 22, BuyItem, 23},
    {802, 128, 74, 134, 1, 1, 0, 49, 23, BuyItem, 24},
    {778, 326, 78, 48, 1, 1, 0, 55, 24, BuyItem, 25},
    {888, 330, 90, 54, 1, 1, 0, 61, 25, BuyItem, 26},
    {924, 156, 68, 112, 1, 1, 0, 64, 26, BuyItem, 27},
    {952, 242, 130, 198, 1, 1, 0, 73, 27, BuyItem, 28},
    {1098, 276, 66, 74, 1, 1, 0, 82, 28, BuyItem, 29},
    {1166, 292, 54, 52, 1, 1, 0, 85, 29, BuyItem, 30},
    {1219, 298, 46, 68, 1, 1, 0, 88, 30, BuyItem, 31},
    {1150, 392, 70, 48, 1, 1, 0, 94, 31, BuyItem, 32},
    {2, 448, 66, 26, 1, 1, 0, 96, 0, ExitToLodge, 33},
    {68, 448, 66, 26, 1, 1, 0, 97, 15, ExitShop, 34},
    {134, 448, 66, 26, 1, 1, 0, 98, 14, ExitShop, -1},
};

static MGLDC *shopDC;
static int shop_gfx;
static int shop_offset = 320;
static int clerk;          /* 1 or 2: which shop assistant is on duty */
static int talking;        /* an item description is playing */

void ResetShopItems(void)
{
    for (int i = 0; i < 17; i++) shop_items[i].status = rand() % 8 + 2;
    for (int i = 17; i < 32; i++) shop_items[i].status = 1;
}

static void ExitToLodge(int param)
{
    (void)param;
    SoundPlay(0);
    StopMusic();
    SwitchScreen(8);
}

static void ExitShop(int param)
{
    SoundPlay(0);
    TempSwitchScreen(param);
}

void BuyItem(int item)
{
    player->saved_game = 0;
    if (player->dollars < shop_items[item].cost) {
        DoCharacterVoice(14, clerk, rand() % 2 + 6);   /* "you can't afford that" */
        return;
    }
    if (item < 17) {
        if (player->supplies[item] > 999) {
            SoundPlay(1);
            return;
        }
        if (player->supplies[item] != 0 && item != 13 && item != 14 && item != 7 && item != 6 &&
            item != 5 && item != 8) {
            DoCharacterVoice(14, 2, rand() % 2 + 4);   /* "you already have one" */
            return;
        }
    }
    /* supplies[17..31] in the original overlap suitcase[0..14] */
    if (item < 17) player->supplies[item] += (ushort)shop_items[item].qty;
    else player->suitcase[item - 17] += (ushort)shop_items[item].qty;
    player->dollars -= shop_items[item].cost;
    SoundPlay(4);
    HOT_SPOT *hs = &shop_hotspots[item];
    BlitGraphic(hs->graphic_num + 0x28, hs->x, hs->y, backpage);
    BlitPageToPage(workpage, backpage, 0x212, 0x1c0, 0x6d, 0x18);
    ShowDollars(0x276);
    HideMouse();
    BlitBackToFront();
    ShowMouse();
    shop_items[item].status--;
    if (shop_items[item].status == 0) {
        /* sold out: remove it from the shelf */
        BlitGraphic(hs->graphic_num + 0x28, shop_items[item].xpos, hs->y, shopDC);
        hs->active = 0;
    } else {
        BlitGraphic(hs->graphic_num + 0x29, hs->x, hs->y, backpage);
        HideMouse();
        DissolveArea(hs->x, hs->y, hs->w, hs->h);
        ShowMouse();
        SoundPlay(2);
    }
    /* mounting scopes on rifles */
    if (item == 10 || item == 11) {
        if (player->supplies[8] != 0) {
            if (player->scopes[0] == 0 && item == 10) {
                player->scopes[0] = 1;
                player->supplies[8]--;
            } else if (player->scopes[1] == 0 && item == 11) {
                player->scopes[1] = 1;
                player->supplies[8]--;
            }
        }
    } else if (item == 8) {
        if (player->supplies[10] != 0 && player->supplies[11] == 0) {
            if (player->scopes[0] == 0) {
                player->scopes[0] = 1;
                player->supplies[8]--;
            }
        } else if (player->supplies[10] == 0 && player->supplies[11] != 0) {
            if (player->scopes[1] == 0) {
                player->scopes[1] = 1;
                player->supplies[8]--;
            }
        } else if (player->supplies[10] != 0 && player->supplies[11] != 0) {
            if (player->scopes[0] == 0 && player->scopes[1] != 0) {
                player->scopes[0] = 1;
                player->supplies[8]--;
            } else if (player->scopes[0] == 0 || player->scopes[1] != 0) {
                TempSwitchScreen(0xd);   /* ask which rifle */
            } else {
                player->scopes[1] = 1;
                player->supplies[8]--;
            }
        }
    }
}

void RestoreShopScreen(void)
{
    ShowDollars(0x276);
    BlitBackToFront();
    ShowMouse();
    if (oldGame_Screen != 0xf) FadeFromBlack();
}

void DrawShopScreen(void)
{
    shopDC = MGL_createMemoryDC(1280, 440);
    if (!shopDC) return;
    if (oldGame_Screen != 0xf) FadeToBlack();
    LoadCanvas("bkg_14", shopDC);
    shop_gfx = LoadGraphics(0x29, "spr_14");
    FreeGraphics(0, 4);
    LoadGraphics(0, "ptr_14");
    SetMouseGraphic(1);
    ClrFontFree(0);
    ClrFontFree(1);
    LoadFonts("fnt_07.dat");
    shop_offset = 320;
    clerk = rand() % 2 + 1;
    for (int i = 0; i < 17; i++)
        if (shop_items[i].status == 0) shop_items[i].status = rand() % 4;
    for (int i = 0; i < 32; i++) {
        shop_hotspots[i].active = 0;
        if (shop_items[i].status != 0) {
            shop_hotspots[i].active = 1;
            shop_hotspots[i].x = shop_items[i].xpos - shop_offset;
            BlitGraphic(shop_hotspots[i].graphic_num + 0x29, shop_hotspots[i].x + shop_offset,
                        shop_hotspots[i].y, shopDC);
        }
    }
    MGL_bitBltCoord(workpage, shopDC, shop_offset, 0, shop_offset + 640, 440, 0, 0, 0);
    BlitGraphic(0x89, 0, 440, workpage);
    BlitWorkToBack();
    RestoreShopScreen();
    StopMusic();
    StartMusicLoop("bkg_14");
    if (player->shop_talk == 0) {
        DoCharacterComment(14, clerk, 1, 320, 440);
        player->shop_talk = 1;
    }
}

void DumpShopScreen(void)
{
    DoCharacterComment(14, clerk, rand() % 2 + 2, 320, 440);
    MGL_destroyDC(shopDC);
    shopDC = NULL;
    FreeGraphics(0x29, shop_gfx);
}

void MoveShop(int direction)
{
    if (!ScrollTick()) return;
    shop_offset += direction;
    if (shop_offset < 0) shop_offset = 0;
    if (shop_offset > 0x271) shop_offset = 0x271;
    MGL_bitBltCoord(workpage, shopDC, shop_offset, 0, shop_offset + 640, 440, 0, 0, 0);
    BlitPageToPage(workpage, backpage, 0, 0, 640, 440);
    for (int i = 0; i < 32; i++) shop_hotspots[i].x = shop_items[i].xpos - shop_offset;
    HideMouse();
    BlitPageToPage(backpage, frontpage, 0, 0, 640, 440);
    ShowMouse();
}

void StartDescription(int which, int soundnum)
{
    char file[32];
    snprintf(file, sizeof file, "chr_14_%d", which);
    if (SoundSeek(file, soundnum + 7, 0x11) == 0)
        Log("SoundSeek failed: file=%s, soundnum=%d", file, soundnum + 7);
    else
        SoundPlay(0x11);
    EVT_flush(EVT_KEYDOWN | EVT_MOUSEDOWN);
    talking = 1;
}

void StopDescription(void)
{
    SoundStop(0x11);
    SoundFree(0x11);
    EVT_flush(EVT_KEYDOWN | EVT_MOUSEDOWN);
    talking = 0;
}

void MonitorDescription(void)
{
    event_t evt;
    if (SoundPoll(0x11) == 0) StopDescription();
    else if (EVT_peekNext(&evt, EVT_KEYDOWN | EVT_MOUSEDOWN)) StopDescription();
}

/* hot spot check with per-pixel hit testing and the item name/price line */
HOT_SPOT *CheckShopHotSpots(HOT_SPOT *hs)
{
    if (!hs) return NULL;
    HOT_SPOT *ptr = hs;
    for (;;) {
        if (ptr->active) {
            if (!MouseInBox(ptr->x, ptr->y, ptr->w + ptr->x, ptr->h + ptr->y, mouseX, mouseY)) {
                if (ptr->hilite && ptr->is_hilited == 1) {
                    HideMouse();
                    ptr->is_hilited = 0;
                    if (ptr->graphic_num < 0x60) {
                        BlitGraphic(ptr->graphic_num + 0x29, ptr->x, ptr->y, backpage);
                        BlitPageToPage(workpage, backpage, 0xcc, 0x1c2, 0x10e, 0x18);
                        BlitPageToPage(backpage, frontpage, 0xcc, 0x1c2, 0x10e, 0x18);
                    } else {
                        BlitPageToPage(workpage, backpage, ptr->x, ptr->y, ptr->w, ptr->h);
                    }
                    BlitPageToPage(backpage, frontpage, ptr->x, ptr->y, ptr->w, ptr->h);
                    ShowMouse();
                }
            } else if (ptr->graphic_num > 0x60 ||
                       CheckBitmapData(ptr->graphic_num + 0x2a, mouseX - ptr->x, mouseY - ptr->y)) {
                if (ptr->hilite == 0 || ptr->is_hilited != 0) return ptr;
                for (HOT_SPOT *p = hs;; p = hs + p->next) {
                    if (p != ptr && p->active && p->hilite && p->is_hilited) {
                        HideMouse();
                        p->is_hilited = 0;
                        if (p->graphic_num < 0x60) BlitGraphic(p->graphic_num + 0x29, p->x, p->y, backpage);
                        else BlitPageToPage(workpage, backpage, p->x, p->y, p->w, p->h);
                        BlitPageToPage(backpage, frontpage, p->x, p->y, p->w, p->h);
                        ShowMouse();
                    }
                    if (p->next == -1) break;
                }
                HideMouse();
                BlitGraphic(ptr->graphic_num + 0x2a, ptr->x, ptr->y, backpage);
                if (ptr->graphic_num < 0x60) {
                    BlitPageToPage(workpage, backpage, 0xcc, 0x1c2, 0x10e, 0x18);
                    ClrFontFormat(0, 0xcc, 0x1c2, "%s : $%d", shop_items[ptr->param].name,
                                  shop_items[ptr->param].cost);
                    BlitPageToPage(backpage, frontpage, 0xcc, 0x1c2, 0x10e, 0x18);
                }
                ptr->is_hilited = 1;
                BlitPageToPage(backpage, frontpage, ptr->x, ptr->y, ptr->w, ptr->h);
                ShowMouse();
                return ptr;
            }
        }
        if (ptr->next == -1) return NULL;
        ptr = hs + ptr->next;
    }
}

void DoShopScreen(void)
{
    if (mouseY < 400) {
        if (mouseX < 50 && shop_offset != 0) {
            if (GetMouseGraphic() != 2) SetMouseGraphic(2);
            MoveShop(mouseX - 50);
        } else if (mouseX < 0x24f || shop_offset > 0x270) {
            if (GetMouseGraphic() > 1) {
                HideMouse();
                SetMouseGraphic(0);
                ShowMouse();
            }
        } else {
            if (GetMouseGraphic() != 3) SetMouseGraphic(3);
            MoveShop(mouseX - 0x24e);
        }
    } else if (GetMouseGraphic() > 1) {
        HideMouse();
        SetMouseGraphic(0);
        ShowMouse();
    }
    if (talking) MonitorDescription();
    HOT_SPOT *hs;
    switch (lastInput) {
    case 0:
        return;
    case 1:
        hs = CheckShopHotSpots(shop_hotspots);
        if (hs) hs->fptr(hs->param);
        break;
    case 2:   /* right click: hear about an item */
        hs = CheckShopHotSpots(shop_hotspots);
        if (hs) {
            if (hs->next > 0 && hs->next < 0x12) {
                if (talking) StopDescription();
                StartDescription(2, hs->param);
            }
            if (hs->next > 0x11 && hs->next < 0x21) {
                if (talking) StopDescription();
                StartDescription(1, hs->param - 0x11);
            }
        }
        break;
    case 4:
        CheckShopHotSpots(shop_hotspots);
        break;
    }
    lastInput = 0;
}

/* ============================================ suitcase and supplies */
static void DoItem(int param) { (void)param; }
static void CaseExitLodge(int unused);
static void CaseToSupplies(int unused);
static void CaseExit(int unused);
static void SuppliesToCase(int unused);

static const char *art_names[15] = {
    "African doll", "Owl", "Fish carving", "Man's head", "Wood carving", "Mask", "Woman's head",
    "Sugar bowls", "Coasters", "Elephant", "African doll", "Luck token", "Small ele", "Small ele",
    "Chameleon",
};
static const char *supply_names[17] = {
    "Handbook", "Safari hat", "Shoes", "Safari shirt", "Safari pants", "Sunscreen", "Repellant",
    "Canteen", "Scope", "Binoculars", "458 rifle", ".30-06 rifle", "Bow", "458 ammo", ".30-06 ammo",
    "Arrows & quiver", "Malaria pills",
};

static HOT_SPOT suitcase_hotspots[] = {
    {102, 102, 88, 196, 1, 0, 0, 101, 0, DoItem, 1},
    {332, 138, 60, 100, 1, 0, 0, 102, 1, DoItem, 2},
    {130, 280, 148, 54, 1, 0, 0, 103, 2, DoItem, 3},
    {178, 172, 68, 120, 1, 0, 0, 104, 3, DoItem, 4},
    {262, 362, 278, 50, 1, 0, 0, 105, 4, DoItem, 5},
    {282, 12, 82, 104, 1, 0, 0, 106, 5, DoItem, 6},
    {394, 182, 62, 112, 1, 0, 0, 107, 6, DoItem, 7},
    {260, 306, 86, 56, 1, 0, 0, 108, 7, DoItem, 8},
    {416, 302, 96, 58, 1, 0, 0, 109, 8, DoItem, 9},
    {242, 122, 76, 118, 1, 0, 0, 110, 9, DoItem, 10},
    {446, 104, 82, 194, 1, 0, 0, 111, 10, DoItem, 11},
    {300, 220, 50, 82, 1, 0, 0, 112, 11, DoItem, 12},
    {144, 346, 50, 60, 1, 0, 0, 113, 12, DoItem, 13},
    {200, 328, 44, 76, 1, 0, 0, 114, 13, DoItem, 14},
    {356, 282, 52, 44, 1, 0, 0, 115, 14, DoItem, 15},
    {2, 448, 66, 26, 1, 1, 0, 116, 0, CaseExitLodge, 16},
    {68, 448, 66, 26, 1, 1, 0, 117, 0, CaseToSupplies, 17},
    {134, 448, 66, 26, 1, 1, 0, 118, 0, CaseExit, -1},
};

static HOT_SPOT supplies_hotspots[] = {
    {142, 160, 52, 82, 1, 0, 0, 101, 0, DoItem, 1},
    {458, 192, 92, 54, 1, 0, 0, 102, 1, DoItem, 2},
    {106, 354, 80, 58, 1, 0, 0, 103, 2, DoItem, 3},
    {470, 384, 98, 42, 1, 0, 0, 104, 3, DoItem, 4},
    {374, 398, 94, 30, 1, 0, 0, 105, 4, DoItem, 5},
    {430, 172, 32, 62, 1, 0, 0, 106, 5, DoItem, 6},
    {400, 174, 28, 58, 1, 0, 0, 107, 6, DoItem, 7},
    {192, 160, 56, 80, 1, 0, 0, 108, 7, DoItem, 8},
    {386, 330, 122, 24, 1, 0, 0, 109, 8, DoItem, 9},
    {456, 4, 102, 144, 1, 0, 0, 110, 9, DoItem, 10},
    {234, 362, 266, 66, 1, 0, 0, 111, 10, DoItem, 11},
    {110, 350, 324, 76, 1, 0, 0, 112, 11, DoItem, 12},
    {116, 308, 284, 66, 1, 0, 0, 113, 12, DoItem, 13},
    {316, 206, 86, 44, 1, 0, 0, 114, 13, DoItem, 14},
    {244, 206, 76, 38, 1, 0, 0, 115, 14, DoItem, 15},
    {78, 76, 62, 164, 1, 0, 0, 116, 15, DoItem, 16},
    {512, 328, 28, 50, 1, 0, 0, 117, 16, DoItem, 17},
    {2, 448, 66, 26, 1, 1, 0, 118, 0, CaseExitLodge, 18},
    {68, 448, 66, 26, 1, 1, 0, 119, 0, CaseExit, 19},
    {134, 448, 66, 26, 1, 1, 0, 120, 0, SuppliesToCase, -1},
};

static int case_gfx, supplies_gfx;

static void CaseExitLodge(int unused)
{
    (void)unused;
    SoundPlay(0);
    TempSwitchScreen(0xc);
    SwitchScreen(8);
}
static void CaseToSupplies(int unused) { (void)unused; SoundPlay(0); SwitchScreen(0xf); }
static void SuppliesToCase(int unused) { (void)unused; SoundPlay(0); SwitchScreen(0xe); }
static void CaseExit(int unused) { (void)unused; SoundPlay(0); TempSwitchScreen(0xc); }

void DrawSuitcaseScreen(void)
{
    case_gfx = LoadGraphics(0x92, "spr_15");
    BlitGraphic(0x92, 0, 0, backpage);
    SaveImage(workpage, temp_save, 0x86, 0x1c0, 0x42, 0x1a);
    BlitPageToPage(backpage, workpage, 0x86, 0x1c0, 0x42, 0x1a);
    for (int i = 0; i < 15; i++) {
        HOT_SPOT *hs = &suitcase_hotspots[i];
        if (player->suitcase[i] == 0) {
            hs->active = 0;
        } else {
            BlitGraphic(hs->graphic_num + 0x2e, hs->x, hs->y, backpage);
            hs->active = 1;
        }
    }
    ShowDollars(0x276);
    BlitBackToFront();
    ShowMouse();
}

void DumpSuitcaseScreen(void)
{
    FreeGraphics(0x92, case_gfx);
    RestoreImage(temp_save, workpage, 0x86, 0x1c0, 0x42, 0x1a);
    BlitWorkToBack();
    ShowDollars(0x276);
    HideMouse();
    BlitBackToFront();
    ShowMouse();
}

/* shared logic of CheckSuitcaseHotSpots / CheckSuppliesHotSpots */
static HOT_SPOT *CheckItemHotSpots(HOT_SPOT *hs, int limit, const char *const *names)
{
    if (!hs) return NULL;
    HOT_SPOT *ptr = hs;
    for (;;) {
        if (ptr->active) {
            if (!MouseInBox(ptr->x, ptr->y, ptr->w + ptr->x, ptr->y + ptr->h, mouseX, mouseY)) {
                if (ptr->graphic_num < limit) {
                    BlitGraphic(ptr->graphic_num + 0x2e, ptr->x, ptr->y, backpage);
                    BlitPageToPage(workpage, backpage, 0xcc, 0x1c2, 0x10e, 0x18);
                    BlitPageToPage(backpage, frontpage, 0xcc, 0x1c2, 0x10e, 0x18);
                }
                if (ptr->hilite && ptr->is_hilited == 1) {
                    HideMouse();
                    ptr->is_hilited = 0;
                    BlitPageToPage(workpage, backpage, ptr->x, ptr->y, ptr->w, ptr->h);
                    BlitPageToPage(backpage, frontpage, ptr->x, ptr->y, ptr->w, ptr->h);
                    ShowMouse();
                }
            } else if (ptr->graphic_num > limit ||
                       CheckBitmapData(ptr->graphic_num + 0x2e, mouseX - ptr->x, mouseY - ptr->y)) {
                if (ptr->graphic_num < limit) {
                    BlitPageToPage(workpage, backpage, 0xcc, 0x1c2, 0x10e, 0x18);
                    ClrFontFormat(0, 0xcc, 0x1c2, "%s", names[ptr->param]);
                    BlitPageToPage(backpage, frontpage, 0xcc, 0x1c2, 0x10e, 0x18);
                }
                if (ptr->hilite == 0 || ptr->is_hilited != 0) return ptr;
                for (HOT_SPOT *p = hs;; p = hs + p->next) {
                    if (p != ptr && p->active && p->hilite && p->is_hilited) {
                        HideMouse();
                        p->is_hilited = 0;
                        if (p->graphic_num < limit) BlitPageToPage(workpage, backpage, p->x, p->y, p->w, p->h);
                        else BlitGraphic(p->graphic_num + 0x2e, p->x, p->y, backpage);
                        BlitPageToPage(backpage, frontpage, p->x, p->y, p->w, p->h);
                        ShowMouse();
                    }
                    if (p->next == -1) break;
                }
                HideMouse();
                BlitGraphic(ptr->graphic_num + 0x2e, ptr->x, ptr->y, backpage);
                ptr->is_hilited = 1;
                BlitPageToPage(backpage, frontpage, ptr->x, ptr->y, ptr->w, ptr->h);
                ShowMouse();
                return ptr;
            }
        }
        if (ptr->next == -1) return NULL;
        ptr = hs + ptr->next;
    }
}

HOT_SPOT *CheckSuitcaseHotSpots(HOT_SPOT *hs) { return CheckItemHotSpots(hs, 0x74, art_names); }
HOT_SPOT *CheckSuppliesHotSpots(HOT_SPOT *hs) { return CheckItemHotSpots(hs, 0x76, supply_names); }

void DoSuitcaseScreen(void)
{
    if (lastInput != 0) {
        if (lastInput == 1) {
            HOT_SPOT *hs = CheckHotSpots(suitcase_hotspots);
            if (hs) hs->fptr(hs->param);
        } else if (lastInput == 4) {
            CheckSuitcaseHotSpots(suitcase_hotspots);
        }
        lastInput = 0;
    }
}

void DrawSuppliesScreen(void)
{
    supplies_gfx = LoadGraphics(0x92, "spr_16");
    BlitGraphic(0x92, 0, 0, backpage);
    SaveImage(workpage, temp_save, 0x44, 0x1c0, 0x42, 0x1a);
    BlitPageToPage(backpage, workpage, 0x44, 0x1c0, 0x42, 0x1a);
    for (int i = 0; i < 17; i++) {
        HOT_SPOT *hs = &supplies_hotspots[i];
        if (player->supplies[i] == 0) {
            hs->active = 0;
        } else {
            BlitGraphic(hs->graphic_num + 0x2e, hs->x, hs->y, backpage);
            hs->active = 1;
        }
    }
    /* a scope mounted on either rifle still shows the scope */
    for (int i = 0; i < 2; i++) {
        if (player->scopes[i]) {
            BlitGraphic(supplies_hotspots[8].graphic_num + 0x2e, supplies_hotspots[8].x,
                        supplies_hotspots[8].y, backpage);
            supplies_hotspots[8].active = 1;
            break;
        }
    }
    ShowDollars(0x276);
    BlitBackToFront();
    ShowMouse();
}

void DumpSuppliesScreen(void)
{
    FreeGraphics(0x92, supplies_gfx);
    RestoreImage(temp_save, workpage, 0x44, 0x1c0, 0x42, 0x1a);
    BlitWorkToBack();
    ShowDollars(0x276);
    HideMouse();
    BlitBackToFront();
    ShowMouse();
}

void DoSuppliesScreen(void)
{
    if (lastInput != 0) {
        if (lastInput == 1) {
            HOT_SPOT *hs = CheckHotSpots(supplies_hotspots);
            if (hs) hs->fptr(hs->param);
        } else if (lastInput == 4) {
            CheckSuppliesHotSpots(supplies_hotspots);
        }
        lastInput = 0;
    }
}

/* ======================================================= scope picker */
static HOT_SPOT scopes_hotspots[] = {
    {158, 219, 276, 60, 1, 1, 0, 102, 0, PickRifle, 1},
    {227, 300, 284, 56, 1, 1, 0, 101, 1, PickRifle, -1},
};

void CheckerboardPage(void)
{
    uchar *base = backpage->surface;
    long end = (long)backpage->bytesPerLine * backpage->height;
    long p = 0;
    int h = 480;
    while (h != 0) {
        h--;
        if ((h & 1) == 0) p++;
        else p--;
        for (int x = 0; x < 640; x++) {
            if ((x & 1) && p >= 0 && p < end) base[p] = 0;
            p++;
        }
    }
}

void PickRifle(int which)
{
    SoundPlay(0);
    player->scopes[which] = 1;
    player->supplies[8]--;
    TempSwitchScreen(0xc);
}

void DrawScopesScreen(void)
{
    CheckerboardPage();
    BlitGraphic(0x8d, 0x76, 0x4f, backpage);
    SaveImage(workpage, temp_save3, 0x9e, 0xdb, 0x114, 0x3c);
    BlitPageToPage(backpage, workpage, 0x9e, 0xdb, 0x114, 0x3c);
    SaveImage(workpage, temp_save4, 0xe3, 300, 0x11c, 0x38);
    BlitPageToPage(backpage, workpage, 0xe3, 300, 0x11c, 0x38);
    BlitBackToFront();
    ShowMouse();
}

void DumpScopesScreen(void)
{
    RestoreImage(temp_save3, workpage, 0x9e, 0xdb, 0x114, 0x3c);
    RestoreImage(temp_save4, workpage, 0xe3, 300, 0x11c, 0x38);
    BlitWorkToBack();
    HideMouse();
    BlitBackToFront();
    ShowMouse();
}

void DoScopesScreen(void)
{
    if (lastInput != 0) {
        if (lastInput == 1) {
            HOT_SPOT *hs = CheckHotSpots(scopes_hotspots);
            if (hs) hs->fptr(hs->param);
        } else if (lastInput == 4) {
            CheckHotSpots(scopes_hotspots);
        }
        lastInput = 0;
    }
}

/* ========================================================== handbook */
static const char *book_strs[7][13] = {
    {"CAPE BUFFALO", "Syncerus caffer", "5,000", "Extremely tough, and very difficult", "to bring down, the Cape Buffalo", "is a trophy any hunter can be", "proud of. These animals are", "dangerous when wounded.", "You must place your shot perfectly ", "or you will have a real problem", "on your hands.", " ", " "},
    {"CROCODILE", "Crocodylus niloticus", "2,000", "A pure predator, the Crocodile", "hunts by instict and kills with", "impunity. To the crocodile, you", "are nothing but a meal.", "A beautiful and unique trophy", "for any experienced hunter.", " ", " ", " ", " "},
    {"IMPALA", "Aepyceros melampus", "200", "Fast, agile and graceful, the Impala", "is an excellent example of African", "plains game. Though small in size,", "this beautiful animal can flee wildly", "at bounds of up to 12 meters.", " ", " ", " ", " ", " "},
    {"LION", "Panthera leo", "3,500", "King of the African carnivores, the", "magnificent lion roams the wilds of", "Africa hunting and controlling his", "domain. By the end of the nineteenth", "century, lions were nearly eliminated", "from North and South Africa. Through", "controlled hunting and wildlife", "management programs, the lion has", "recovered in savana and plains", "habitats where prey is available."},
    {"White RHINOCEROS", "Ceratotherium simum", "75,000", "The Rhinoceros is an animal unique", "to Africa. Poaching has taken a toll", "in the past, but with controlled", "hunting and management, populations", "of this wonderful animal are getting", "larger. Very strong and resilient,", "they can be dangerous to hunt.", " ", " ", " "},
    {"Blue WILDEBEEST", "Connochaetes taurinus", "1200", "The Blue Wildebeest of the Low", "Veld of Africa is a truly unique", "animal. Different form the Black", "Wildebeest found in most collections,", "the Blue Wildebeest has a bluish-gray", "coat which is unique to the species.", " ", " ", " ", " "},
    {"Plains ZEBRA", "Equus burchellii", "850", "The Zebra is an African version of", "the horse. It is one of the most", "numerous large herbivores in", "Africa. It has adapted to a broad", "range of grassland habitats.", " ", " ", " ", " ", " "},
};

static HOT_SPOT handbook_hotspots[] = {
    {18, 402, 150, 60, 1, 1, 0, 37, -1, NextPage, 1},
    {536, 412, 84, 40, 1, 1, 0, 38, 1, NextPage, 2},
    {113, 88, 86, 23, 1, 1, 0, 39, 0, ExitBook, -1},
};
static int book_page, book_gfx;

void ExitBook(int param)
{
    (void)param;
    SoundPlay(0);
    VerifyMouse();
    TempSwitchScreen(0x11);
}

void RestoreHandbookScreen(void)
{
    BlitGraphic(0x46, 0, 0, backpage);
    BlitGraphic(book_page + 0x47, 0x18, 0x72, backpage);
    activeDC = backpage;
    ClrFontFormatCenter(3, 0xa0, 0x145, "%s", book_strs[book_page][0]);
    ClrFontFormatCenter(2, 0xa0, 0x15e, "%s", book_strs[book_page][1]);
    ClrFontFormat(2, 0x16c, 0x145, "TROPHY FEE: $%s.00", book_strs[book_page][2]);
    ClrFontFormat(2, 0x16c, 0x157, "PERMITS AVAILABLE: %d", num_permits[book_page]);
    for (int i = 0; i < 10; i++) ClrFontDraw(2, 0x16c, i * 16 + 0x80, book_strs[book_page][3 + i]);
    BlitBackToFront();
    ShowMouse();
}

void NextPage(int dir)
{
    book_page += dir;
    if (book_page < 0) book_page = 6;
    if (book_page > 6) book_page = 0;
    HideMouse();
    SoundPlay(0xc);
    RestoreHandbookScreen();
}

void DrawHandbookScreen(void)
{
    book_gfx = LoadGraphics(0x46, "spr_19");
    bFlipMouse = 0;
    book_page = 0;
    SetMouseGraphic(0);
    RestoreHandbookScreen();
    SaveImage(workpage, temp_save, 0x71, 0x58, 0x56, 0x17);
    BlitPageToPage(backpage, workpage, 0x71, 0x58, 0x56, 0x17);
    SaveImage(workpage, temp_save2, 0x12, 0x192, 0x96, 0x3c);
    BlitPageToPage(backpage, workpage, 0x12, 0x192, 0x96, 0x3c);
    SaveImage(workpage, temp_save3, 0x218, 0x19c, 0x54, 0x28);
    BlitPageToPage(backpage, workpage, 0x218, 0x19c, 0x54, 0x28);
}

void DumpHandbookScreen(void)
{
    FreeGraphics(0x46, book_gfx);
    RestoreImage(temp_save, workpage, 0x71, 0x58, 0x56, 0x17);
    RestoreImage(temp_save2, workpage, 0x12, 0x192, 0x96, 0x3c);
    RestoreImage(temp_save3, workpage, 0x218, 0x19c, 0x54, 0x28);
}

void DoHandbookScreen(void)
{
    if (lastInput != 0) {
        if (lastInput == 1) {
            HOT_SPOT *hs = CheckHotSpots(handbook_hotspots);
            if (hs) hs->fptr(hs->param);
        } else if (lastInput == 4) {
            CheckHotSpots(handbook_hotspots);
        }
        lastInput = 0;
    }
}
