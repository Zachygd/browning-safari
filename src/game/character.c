/*
 * character.c - talking characters with lip sync (character.obj)
 *
 * While a voice clip plays, every 100 ms the current 16-bit sample of the
 * clip is inspected; loud samples pick one of three open-mouth frames,
 * quiet ones the closed mouth.  Eyes blink at random.  Esc skips.
 */
#include <stdio.h>
#include <stdlib.h>
#include "safari.h"

const uint8_t *SoundData(int sndnum);
char guide_said[64];

/* sample at the read pointer; the original stepped 0x113a bytes (100 ms) */
typedef struct { const uint8_t *base; uint32_t size; uint32_t offset; } lipsync_t;

static void lip_init(lipsync_t *l, uint32_t size)
{
    l->base = SoundData(0x11);
    l->size = size;
    l->offset = 0x2c;
}

static short lip_next(lipsync_t *l)
{
    short v = 0;
    if (l->base && l->offset + 1 < l->size)
        v = (short)(l->base[l->offset] | l->base[l->offset + 1] << 8);
    if (l->offset + 0x113a < l->size) l->offset += 0x113a;
    return v;
}

static int escPressed(void)
{
    event_t e;
    return EVT_peekNext(&e, EVT_KEYDOWN) && ((e.message >> 8) & 0xff) == 0x01;
}

void DoCharacterComment(int bkg, int character, int comment, int xpos, int ypos)
{
    char name[32];
    uint32_t lastcount = 0;
    snprintf(name, sizeof name, "chr_%02d_%d", bkg, character);
    int num_gfx = LoadGraphics(0xd2, name);
    if (num_gfx != 7) {
        Log("CHARACTER ERROR: num_gfx = %d, should be 7", num_gfx);
        return;
    }
    if (comment < 1) {
        Log("CHARACTER ERROR: comment = %d", comment);
        return;
    }
    SoundStop(0x11);
    SoundFree(0x11);
    snprintf(name, sizeof name, "chr_%02d_%d", bkg, character);
    uint32_t size = SoundSeek(name, comment - 1, 0x11);
    HideMouseOnPage(backpage);
    HideMouseOnPage(frontpage);
    int x = graphic[0xd2].hotx + xpos, y = graphic[0xd2].hoty + ypos;
    int w = graphic[0xd2].width, h = graphic[0xd2].height;
    if (size == 0) {
        Log("ERROR: DoCharacterComment(%d)", comment);
    } else {
        lipsync_t l;
        SoundPlay(0x11);
        lip_init(&l, size);
        BlitGraphic(0xd2, xpos, ypos, backpage);
        BlitPageToPage(backpage, frontpage, x, y, w, h);
        int skipped = 0;
        while (SoundPoll(0x11)) {
            uint32_t count = GetLapCount();
            if (count - lastcount > 100) {
                short s = lip_next(&l);
                lastcount = count;
                if (abs(s) < 0x101) BlitGraphic(0xd3, xpos, ypos, backpage);
                else BlitGraphic(rand() % 3 + 0xd4, xpos, ypos, backpage);
                if (rand() % 8 == 2) BlitGraphic(0xd8, xpos, ypos, backpage);
                else BlitGraphic(0xd7, xpos, ypos, backpage);
                BlitPageToPage(backpage, frontpage, x, y, w, h);
            }
            if (escPressed()) { skipped = 1; break; }
        }
        if (skipped) SoundStop(0x11);
        SoundFree(0x11);
    }
    FreeGraphics(0xd2, num_gfx);
    BlitPageToPage(workpage, backpage, x, y, w, h);
    BlitPageToPage(backpage, frontpage, x, y, w, h);
    ShowMouse();
}

/* the hunting guide (chr_99_N) talking in the corner of the hunt screen */
void DoGuideComment(int guide, int comment)
{
    char name[32];
    uint32_t lastcount = 0;
    if (debug) Log("DoPartnerComment(%d)", comment);
    SoundStop(0x11);
    SoundFree(0x11);
    snprintf(name, sizeof name, "chr_99_%d", guide);
    uint32_t size = SoundSeek(name, comment - 1, 0x11);
    if (size == 0) return;
    lipsync_t l;
    SoundPlay(0x11);
    lip_init(&l, size);
    int x = graphic[200].hotx + 300, y = graphic[200].hoty + 360;
    int w = graphic[200].width, h = graphic[200].height;
    HideMouseOnPage(frontpage);
    BlitGraphicLayer(200, 300, 360, 0x69);
    BlitPageToPage(backpage, frontpage, x, y, w, h);
    int skipped = 0;
    while (SoundPoll(0x11)) {
        uint32_t count = GetLapCount();
        if (count - lastcount > 100) {
            short s = lip_next(&l);
            lastcount = count;
            if (abs(s) < 0x101) BlitGraphic(0xc9, 300, 360, backpage);
            else BlitGraphic(rand() % 3 + 0xca, 300, 360, backpage);
            if (rand() % 8 == 2) BlitGraphic(0xce, 300, 360, backpage);
            else BlitGraphic(0xcd, 300, 360, backpage);
            BlitPageToPage(backpage, frontpage, x, y, w, h);
        }
        if (escPressed()) { skipped = 1; break; }
    }
    if (skipped) SoundStop(0x11);
    SoundFree(0x11);
    BlitPageToPage(workpage, backpage, x, y, w, h);
    BlitPageToPage(backpage, frontpage, x, y, w, h);
    ShowMouseOnPage(frontpage);
    if (comment >= 0 && comment < 64) guide_said[comment] = 1;
}

/* the game warden at the jail (game over) screen; drawn scaled and depth-tested */
void DoWardenJailComment(int comment, int xpos, int ypos)
{
    uint32_t lastcount = 0;
    int zpos = 0x77;
    int num_gfx = LoadGraphics(0xd2, "chr_20_1");
    if (num_gfx != 7) {
        Log("CHARACTER ERROR: num_gfx = %d, should be 7", num_gfx);
        return;
    }
    int x = graphic[0xd2].hotx + xpos, y = graphic[0xd2].hoty + ypos;
    int w = graphic[0xd2].width, h = graphic[0xd2].height;
    ScaleGraphic(0xd2, xpos, ypos, 0x77, 100, 0);
    BlitPageToPage(backpage, frontpage, x, y, w, h);
    SoundStop(0x11);
    SoundFree(0x11);
    uint32_t size = SoundSeek("chr_20_1", comment - 1, 0x11);
    HideMouseOnPage(backpage);
    HideMouseOnPage(frontpage);
    if (size == 0) {
        Log("ERROR: DoWardenComment(%d)", comment);
    } else {
        lipsync_t l;
        SoundPlay(0x11);
        lip_init(&l, size);
        while (SoundPoll(0x11)) {
            uint32_t count = GetLapCount();
            if (count - lastcount > 100) {
                short s = lip_next(&l);
                if (abs(s) < 0x101) ScaleGraphic(0xd3, xpos, ypos, 0x77, 100, 0);
                else ScaleGraphic(rand() % 3 + 0xd4, xpos, ypos, zpos, 100, 0);
                if (rand() % 8 == 2) ScaleGraphic(0xd8, xpos, ypos, 0x77, 100, 0);
                else ScaleGraphic(0xd7, xpos, ypos, 0x77, 100, 0);
                BlitPageToPage(backpage, frontpage, x, y, w, h);
                lastcount = count;
            }
        }
        SoundFree(0x11);
    }
    FreeGraphics(0xd2, 7);
    BlitPageToPage(workpage, backpage, x, y, w, h);
    BlitPageToPage(backpage, frontpage, x, y, w, h);
    ShowMouse();
}

static void StopGuideSounds(void)
{
    for (int i = 0x1d; i <= 0x26; i++) SoundStop(i);
}

void DoGuideVoice(int guide, int comment)
{
    char file[32];
    StopGuideSounds();
    snprintf(file, sizeof file, "chr_99_%d", guide);
    StartVoice(file, comment - 1);
}

void DoCharacterVoice(int bkg, int character, int comment)
{
    char str[32];
    snprintf(str, sizeof str, "chr_%02d_%d", bkg, character);
    StartVoice(str, comment - 1);
}

void DoWardenComment(int comment)
{
    HideMouse();
    DoCharacterComment(99, 2, comment, 0xdc, 0x1d6);
}

/* Mr. Snidley the shopkeeper */
void InitSnidleyComment(void)
{
    LoadGraphics(0xd2, "chr_07_1");
    HideMouseOnPage(backpage);
    HideMouseOnPage(frontpage);
}

void ExitSnidleyComment(void)
{
    int x = graphic[0xd2].hotx, y = graphic[0xd2].hoty;
    int w = graphic[0xd2].width, h = graphic[0xd2].height;
    FreeGraphics(0xd2, 7);
    BlitPageToPage(workpage, backpage, x + 320, y + 440, w, h);
    BlitPageToPage(backpage, frontpage, x + 320, y + 440, w, h);
    ShowMouse();
}

void DoSnidleyComment(int comment)
{
    uint32_t lastcount = 0;
    int xpos = 320, ypos = 440;
    SoundStop(0x11);
    SoundFree(0x11);
    uint32_t size = SoundSeek("chr_07_1", comment - 1, 0x11);
    if (size == 0) {
        Log("ERROR: DoCharacterComment(%d)", comment);
        return;
    }
    lipsync_t l;
    SoundPlay(0x11);
    lip_init(&l, size);
    int x = graphic[0xd2].hotx + xpos, y = graphic[0xd2].hoty + ypos;
    int w = graphic[0xd2].width, h = graphic[0xd2].height;
    BlitGraphic(0xd2, xpos, ypos, backpage);
    BlitPageToPage(backpage, frontpage, x, y, w, h);
    int skipped = 0;
    while (SoundPoll(0x11)) {
        uint32_t count = GetLapCount();
        if (count - lastcount > 100) {
            short s = lip_next(&l);
            if (abs(s) < 0x101) BlitGraphic(0xd3, xpos, ypos, backpage);
            else BlitGraphic(rand() % 3 + 0xd4, xpos, ypos, backpage);
            if (rand() % 8 == 2) BlitGraphic(0xd8, xpos, ypos, backpage);
            else BlitGraphic(0xd7, xpos, ypos, backpage);
            BlitPageToPage(backpage, frontpage, x, y, w, h);
            lastcount = count;
        }
        if (escPressed()) { skipped = 1; break; }
    }
    if (skipped) SoundStop(0x11);
    SoundFree(0x11);
}
