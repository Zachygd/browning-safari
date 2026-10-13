/*
 * init.c - start-up, options and save games (Init.obj)
 *
 * The Windows original read its CD-ROM path and driver options from
 * HKLM\SOFTWARE\Oquirrh\Settings.  Here the data directory and options
 * come from the command line / environment, see platform/paths.c.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "safari.h"

char  cdPath[1024];
char  basPath[1024];
char  filename[1100];
int   debug;
int   init_DirectSound = 1, init_DirectX = 1, init_WinDirect = 1, init_8bitsound = 0;
int   colorBlack = 254, colorWhite = 255;
uchar plug_in;
int   current_player;
PLAYER players[8];

typedef char player_size_check[(sizeof(PLAYER) == 0xE4) ? 1 : -1];

int CheckRegistry(void)
{
    /* filled in by paths_init(); keep the original fallback logic */
    if (init_DirectX == 0 && init_WinDirect == 0) init_DirectX = 1;
    return cdPath[0] != 0;
}

void SetGameOptions(void)
{
    char signature[32];
    int32_t version = 0x1000;
    char cur_player;
    memset(signature, 0, sizeof signature);
    strcpy(signature, "Browning African Safari");
    signature[31] = 0x1a;
    cur_player = (char)current_player;
    if (cur_player < 0) cur_player = 0;
    if (cur_player > 7) cur_player = 0;
    FHANDLE fh = FileSave("options.dat");
    if (fh == BAD_HANDLE) {
        Log("Unable to save options.dat");
        return;
    }
    FileWrite(fh, signature, 0x20);
    FileWrite(fh, &version, 4);
    FileWrite(fh, &cur_player, 1);
    FileWrite(fh, &plug_in, 1);
    FileClose(fh);
}

void DeleteGame(int which)
{
    char name[128];
    snprintf(name, sizeof name, "GAME%02d.SAV", which + 1);
    FileDelete(name);
    AlertBox("Game has been deleted", "Okay");
}

void SaveGame(int which)
{
    char name[128];
    char signature[32];
    int32_t version = 0x1000;
    memset(signature, 0, sizeof signature);
    strcpy(signature, "Browning African Safari");
    signature[31] = 0x1a;
    snprintf(name, sizeof name, "GAME%02d.SAV", which + 1);
    FHANDLE fh = FileSave(name);
    if (fh == BAD_HANDLE) {
        Log("Unable to save game: %s", name);
        return;
    }
    players[which].saved_game = 1;
    FileWrite(fh, signature, 0x20);
    FileWrite(fh, &version, 4);
    FileWrite(fh, &players[which], 0xe4);
    FileClose(fh);
}

int LoadPlayer(int which)
{
    char name[128];
    char sig[32];
    int32_t version;
    snprintf(name, sizeof name, "GAME%02d.SAV", which + 1);
    FHANDLE fh = FileOpen(name);
    if (fh == BAD_HANDLE) return 0;
    FileRead(fh, sig, 0x20);
    sig[31] = 0;
    if (strcmp(sig, "Browning African Safari") != 0) {
        Log("wrong signature in %s: %s", name, sig);
        FileClose(fh);
        return 0;
    }
    FileRead(fh, &version, 4);
    if (version != 0x1000) {
        Log("wrong version in %s: %d", name, version);
        FileClose(fh);
        return 0;
    }
    FileRead(fh, &players[which], 0xe4);
    FileClose(fh);
    return 1;
}

void GraphicsExit(void)
{
    MGL_destroyDC(workpage);
    MGL_destroyDC(backpage);
    workpage = backpage = NULL;
}

int GetGameOptions(void)
{
    char cur_player = 0;
    char sig[32];
    int32_t version;
    current_player = 0;
    player = &players[0];
    players[0].music_volume = 80;
    players[0].sound_volume = 80;
    FHANDLE fh = FileOpen("options.dat");
    if (fh == BAD_HANDLE) {
        Log("Cannot find options.dat file");
        return 0;
    }
    FileRead(fh, sig, 0x20);
    sig[31] = 0;
    if (strcmp(sig, "Browning African Safari") != 0) {
        Log("wrong signature: %s", sig);
        FileClose(fh);
        return 0;
    }
    FileRead(fh, &version, 4);
    if (version != 0x1000) {
        Log("wrong version: %d", version);
        FileClose(fh);
        return 0;
    }
    FileRead(fh, &cur_player, 1);
    FileRead(fh, &plug_in, 1);
    FileClose(fh);
    int loaded = 0;
    for (int i = 0; i < 8; i++) loaded += LoadPlayer(i);
    if (loaded == 0) {
        current_player = 0;
        strcpy(players[0].name, "Your Game");
        players[0].sound_volume = 80;
        players[0].music_volume = 80;
    } else {
        current_player = cur_player;
        if (current_player < 0 || current_player > 7) current_player = 0;
    }
    if (players[current_player].name[0] < '!') {
        for (current_player = 0; current_player < 8 && players[current_player].name[0] < '!'; current_player++) {}
        if (current_player >= 8) current_player = 0;
    }
    player = &players[current_player];
    return 1;
}

int CheckCD(void)
{
    char file[1100];
    snprintf(file, sizeof file, "%sfonts.dat", cdPath);
    FHANDLE fh = FileOpen(file);
    if (fh == BAD_HANDLE) return 0;
    FileClose(fh);
    return 1;
}

int GraphicsInit(void)
{
    frontpage = sys_display;
    if (!frontpage) {
        Log("ERROR: could not create display");
        return 0;
    }
    backpage = MGL_createMemoryDC(640, 480);
    if (!backpage) { Log("ERROR: no memory for backpage"); return 0; }
    workpage = MGL_createMemoryDC(640, 480);
    if (!workpage) { Log("ERROR: no memory for workpage"); return 0; }
    MGL_setClipRectDC(workpage, 0, 0, 640, 480);
    MGL_setClipRectDC(backpage, 0, 0, 640, 480);
    MGL_setClipRectDC(frontpage, 0, 0, 640, 480);
    MGL_makeCurrentDC(frontpage);
    activeDC = backpage;
    return 1;
}

/* WinMain() of the original, minus the Windows plumbing */
int SafariMain(void)
{
    Log("Browning African Safari 1.0 Error Log");
    if (!CheckRegistry()) {
        Log("CheckRegistry failed");
        return 1;
    }
    if (!CheckCD()) return 2;
    Log("DirectX: %d, WinDirect: %d, DirectSound: %d", init_DirectX, init_WinDirect, init_DirectSound);
    GetGameOptions();
    if (!GraphicsInit()) return 1;
    SoundInit();
    if (GameInit()) {
        GameLoop();
        GameExit();
        SetGameOptions();
    }
    SoundExit();
    GraphicsExit();
    return 0;
}
