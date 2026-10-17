/*
 * hunt.c - the hunting screen: animal populations, the scrolling panorama,
 * animal behaviour state machines and shooting (hunt.obj)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "safari.h"

#define F(x) ((float)(x))

/* ------------------------------------------------------------- globals -- */
SPRITE  sprites[10];
int     sp_sort[5];
int     number_of_species;
int     game_offset;            /* panorama scroll, 0..0xf00 */
int     bow_sight_x, bow_sight_y;
int     animals_overall;
int     area_pop[49];           /* [area * 7 + species] */
int     num_permits[7];
HUNTER  hunter;
GAMEBAG huntbag[70];
MGLDC  *sceneDC;
int     pending_comment;        /* guide comment queued for when the shot sound ends */

static int spec1 = 255, spec2 = 255, spec3 = 255;
static int ratio1, ratio2, ratio3;
static int waitCounter[3];
static int waitTime[4];         /* [3] holds the total of all populations */

/* id, aggressive, dangerous, landtype, weapon, min/max permits, population,
 * trophy fee, then per-direction walk and run steps (x,y) for the
 * 3 directions + 1 unused facing left, and the same facing right */
SPECIES_TYPE species[7] = {
    /* buffalo */
    {0, 0, 1, 2, 0, -3, 7, 1000, 5000,
     {-5, -7, -5, 0, 5, 7, 5}, {2, 0, -2, 12, 2, 0, -2},
     {-17, -26, -17, 0, 17, 26, 17}, {5, 0, -5, 12, 5, 0, -5}},
    /* crocodile */
    {1, 0, 1, 1, 0, 5, 35, 2800, 2000,
     {-2, -2, -2, 0, 2, 2, 2}, {1, 0, -1, 15, 1, 0, -1},
     {-6, -8, -6, 0, 6, 8, 6}, {2, 0, -2, 15, 2, 0, -2}},
    /* impala */
    {2, 0, 0, 2, 1, 35, 90, 10000, 200,
     {-4, -5, -4, 0, 4, 5, 4}, {3, 0, -3, 16, 3, 0, -3},
     {-25, -40, -25, 0, 25, 40, 25}, {10, 0, -10, 16, 10, 0, -10}},
    /* lion */
    {3, 1, 1, 2, 0, -2, 5, 700, 3500,
     {-4, -6, -4, 0, 4, 6, 4}, {2, 0, -2, 16, 2, 0, -2},
     {-24, -35, -24, 0, 24, 35, 24}, {10, 0, -10, 16, 10, 0, -10}},
    /* rhino */
    {4, 0, 1, 2, 0, -8, 2, 350, 75000,
     {-7, -9, -7, 0, 7, 9, 7}, {3, 0, -3, 10, 3, 0, -3},
     {-20, -30, -20, 0, 20, 30, 20}, {8, 0, -8, 10, 8, 0, -8}},
    /* wildebeest */
    {5, 0, 0, 2, 1, 2, 14, 2000, 1200,
     {-5, -7, -5, 0, 5, 7, 5}, {2, 0, -2, 12, 2, 0, -2},
     {-20, -35, -20, 0, 20, 35, 20}, {7, 0, -7, 12, 7, 0, -7}},
    /* zebra */
    {6, 0, 0, 2, 1, -3, 6, 900, 850,
     {-4, -6, -4, 0, 4, 6, 4}, {2, 0, -2, 16, 2, 0, -2},
     {-26, -40, -26, 0, 26, 40, 26}, {10, 0, -10, 16, 10, 0, -10}},
};

/* land type of each hunting area: 1 = water, 2 = land, 3 = both */
int land[7] = {3, 2, 2, 2, 2, 1, 3};

/* -------------------------------------------------------- populations -- */
void DivideAnimals(void)
{
    waitTime[3] = 0;
    for (int i = 0; i < 7; i++) {
        waitTime[3] += species[i].population;
        int range = species[i].max_permits - species[i].min_permits;
        int rnum = rand() % range + species[i].min_permits;
        num_permits[i] = rnum;
        if (num_permits[i] < 0) num_permits[i] = 0;
        if (debugLog) Log("DivideAnimals: range = %d, rnum = %d, permits = %d", range, rnum, num_permits[i]);
    }
    memset(area_pop, 0, sizeof area_pop);
    for (int i = 0; i < 7; i++) {
        int loc_1, loc_2;
        do { loc_1 = rand() % 7; } while ((species[i].landtype & land[loc_1]) == 0);
        do {
            do { loc_2 = rand() % 7; } while (loc_2 == loc_1);
        } while ((species[i].landtype & land[loc_2]) == 0);
        int r = rand() % 100;
        area_pop[loc_1 * 7 + i] = (species[i].population * r) / 100;
        area_pop[loc_2 * 7 + i] = (species[i].population * (100 - r)) / 100;
    }
    if (debugLog) {
        for (int j = 0; j < 7; j++) {
            for (int i = 0; i < 7; i++) Log("area_pop[%d][%d] = %d", j, i, area_pop[j * 7 + i]);
            Log(" ");
        }
    }
}

void MoveAnimals(void)
{
    for (int j = 0; j < 7; j++) {
        for (int i = 0; i < 7; i++) {
            int cur_pop = area_pop[j * 7 + i];
            if (cur_pop == 0) continue;
            int num_to_move = rand() % cur_pop;
            int r;
            do { r = rand(); } while ((species[i].landtype & land[r % 7]) == 0);
            int new_loc = r % 7;
            area_pop[j * 7 + i] -= num_to_move;
            area_pop[new_loc * 7 + i] += num_to_move;
        }
    }
    if (debugLog) {
        Log("MoveAnimals:");
        for (int j = 0; j < 7; j++) {
            for (int i = 0; i < 7; i++) Log("area_pop[%d][%d] = %d", j, i, area_pop[j * 7 + i]);
            Log(" ");
        }
    }
}

/* a gunshot sends 80% of the land animals in this area somewhere else */
void ScareAnimals(void)
{
    int bg = player->background;
    for (int i = 0; i < 7; i++) {
        if (species[i].landtype & 1) continue;
        int num_to_move = (int)((double)area_pop[bg * 7 + i] * 0.8);
        int r;
        do { r = rand(); } while ((species[i].landtype & land[r % 7]) == 0);
        area_pop[bg * 7 + i] -= num_to_move;
        area_pop[(r % 7) * 7 + i] += num_to_move;
    }
    animals_overall = (int)((double)animals_overall * 0.2);
}

static void loadSpecies(int slot, int spec)
{
    static const int spr_base[3] = {300, 0x166, 0x1a0};
    static const int kz_base[3] = {500, 0x22e, 0x268};
    char file[32];
    snprintf(file, sizeof file, "spr_99_%d", spec + 1);
    LoadGraphics(spr_base[slot], file);
    snprintf(file, sizeof file, "kz_99_%d", spec + 1);
    LoadGraphics(kz_base[slot], file);
}

static void loadSpeciesSounds(int slot, int spec)
{
    char file[32];
    snprintf(file, sizeof file, "spr_99_%d", spec + 1);
    SoundSeek(file, 0, 0x17 + slot * 2);
    SoundSeek(file, 1, 0x18 + slot * 2);
}

/* Pick the (up to) three most numerous species in the current area. */
void SpeciesRatio(void)
{
    int bg = player->background;
    int most1 = 0, most2 = 0, most3 = 0;
    for (int i = 0; i < 7; i++) {
        int pop = area_pop[i + bg * 7];
        if (most1 < pop) {
            most3 = most2; most2 = most1; most1 = pop;
            spec3 = spec2; spec2 = spec1; spec1 = i;
        } else if (most2 < pop) {
            most3 = most2; most2 = pop;
            spec3 = spec2; spec2 = i;
        } else if (most3 < pop) {
            most3 = pop;
            spec3 = i;
        }
    }
    animals_overall = most3 + most2 + most1 + 1;
    if (debug) Log("SpeciesRatio: area %d: %d/%d/%d of species %d/%d/%d", bg, most1, most2, most3, spec1, spec2, spec3);
    if (most1 == 0) {
        number_of_species = 0;
    } else if (most2 == 0) {
        number_of_species = 1;
        ratio1 = 100; ratio2 = 0; ratio3 = 0;
        loadSpecies(0, spec1);
        loadSpeciesSounds(0, spec1);
    } else if (most3 == 0) {
        number_of_species = 2;
        /* PORT FIX: the original computed (most1 / animals_overall) * 100
         * in integer arithmetic, which is always 0, so areas holding exactly
         * two species never produced any animals.  Use the same floating
         * point formula as the three-species case. */
        ratio1 = (int)((double)most1 / animals_overall * 100.0);
        ratio2 = (int)((double)most2 / animals_overall * 100.0);
        ratio3 = 0;
        loadSpecies(0, spec1);
        loadSpecies(1, spec2);
        loadSpeciesSounds(0, spec1);
        loadSpeciesSounds(1, spec2);
    } else {
        number_of_species = 3;
        ratio1 = (int)((double)most1 / animals_overall * 100.0);
        ratio2 = (int)((double)most2 / animals_overall * 100.0);
        ratio3 = (int)((double)most3 / animals_overall * 100.0);
        loadSpecies(0, spec1);
        loadSpecies(1, spec2);
        loadSpecies(2, spec3);
        loadSpeciesSounds(0, spec1);
        loadSpeciesSounds(1, spec2);
        loadSpeciesSounds(2, spec3);
    }
}

int PickSpecies(int s)
{
    if (number_of_species == 0) return 0xff;
    if (waitTime[3] <= 0) return 0xff;
    if (animals_overall < rand() % waitTime[3]) return 0xff;
    int r = rand() % 100;
    if (r < ratio1) { sprites[s].id = 0; return spec1; }
    if (r < ratio2 + ratio1) { sprites[s].id = 1; return spec2; }
    if (r < ratio2 + ratio1 + ratio3) { sprites[s].id = 2; return spec3; }
    return 0xff;
}

/* first walkable row (from the horizon down) at the sprite's x position */
int GetSpriteYPosition(int s)
{
    if (!scale_map) { Log("GetSpriteYPosition: NO SCALE MAP DATA"); return 0; }
    if (!path_map) { Log("GetSpriteYPosition: NO PATH MAP DATA"); return 0; }
    const uchar *pathmap = path_map + sprites[s].x / 4;
    int lt = species[sprites[s].species].landtype;
    for (int ypos = 1; ypos < 0x5a; ypos++) {
        pathmap += 0x460;
        uchar p = *pathmap;
        if (p != 0 && (p != 7 || (lt & 2)) && (p != 0xb || (lt & 1)))
            return ypos * 4;
    }
    return 0;
}

/* Draw a scaled animal on the back page, hidden behind nearer scenery. */
void ScaleGraphic2(int num, int xpos, int ypos, int zpos, int percent, int flip)
{
    int src_offset = 0;
    int off;
    if (game_offset < 0xc81 || game_offset <= xpos) off = -game_offset;
    else off = 0xf00 - game_offset;
    xpos += off;
    if (xpos > 0xc80) xpos -= 0xf00;
    if (percent <= 4) return;
    if (num < 0 || num >= MAX_GRAPHICS) return;
    int w = graphic[num].width;
    int h = graphic[num].height;
    const uchar *src = graphic[num].data;
    if (!src) { Log("ScaleGraphic2: graphic %d = NULL!", num); return; }
    uchar rowbuf[8192];
    if (w <= 0 || w > 4096) { Log("ScaleGraphic2: w = %d, num = %d, rowbuf failed", w, num); return; }
    const uchar *lmap = layer_map;
    if (!lmap) { Log("ScaleGraphic2: z_map = NULL"); return; }
    int hx;
    if (flip == 0) hx = (graphic[num].hotx * percent) / 100;
    else hx = -(((graphic[num].width + graphic[num].hotx) * percent) / 100);
    ypos += (graphic[num].hoty * percent) / 100;
    xpos += hx;
    float xscale = F((double)(w * percent) / 100.0);
    float yscale = F((double)(h * percent) / 100.0);
    float x_step = F((double)w / xscale);
    float y_step = F((double)h / yscale);
    float y_index = 0.0f;
    if (xpos + (int)xscale < 0 || ypos + (int)yscale < 0 || xpos >= 640 || ypos >= 360) return;
    if (xpos < 0) {
        xscale = F((double)xpos + xscale);
        src_offset = -((xpos * 100) / percent);
        xpos = 0;
    }
    int bpl = backpage->bytesPerLine;
    int dst_offset = bpl * ypos + xpos;
    uchar *lpDst = backpage->surface + dst_offset;
    int layer_dst_offset = ypos * 0x1180 + game_offset + xpos;
    if (layer_dst_offset < 0) layer_dst_offset = 0;
    if (layer_dst_offset > 0x189c00) layer_dst_offset = 0;
    const int layer_size = 0x1180 * 0x168;
    for (int y = 0; y < h; y++) {
        if (flip == 0) { for (int n = 0; n < w; n++) rowbuf[n] = *src++; }
        else { for (int n = w - 1; n >= 0; n--) rowbuf[n] = *src++; }
        while ((int)y_index == y) {
            if (dst_offset >= 0) {
                float x_index = 0.0f;
                for (int x = 0; x < (int)xscale; x++) {
                    if (xpos + x >= 640) continue;
                    if (dst_offset + x > 0x383ff) return;
                    int lo = layer_dst_offset + x;
                    if (lo < layer_size && lmap[lo] < zpos) {
                        int si = src_offset + (int)x_index;
                        if (si >= 0 && si < 8192) {
                            uchar pixel = rowbuf[si];
                            if (pixel) lpDst[x] = pixel;
                        }
                    }
                    x_index = F((double)x_index + x_step);
                }
            }
            y_index = F((double)y_index + y_step);
            dst_offset += bpl;
            layer_dst_offset += 0x1180;
            lpDst += bpl;
            if (dst_offset >= 0x38400) break;
        }
    }
}

void RestoreHuntScreen(void)
{
    BlitWorkToBack();
    DrawCompass();
    BlitBackToFront();
    InitBowPeepSave();
    bFlipMouse = 1;
    update_clock = 1;
    update_score = 1;
    update_temp = 1;
    update_wind = 1;
    update_lifebar = 1;
}

void MoveScreen(int direction)
{
    if (!ScrollTick()) return;
    game_offset += direction;
    if (game_offset < 0) game_offset = 0xf00;
    if (game_offset > 0xf00) game_offset = 0;
    HideMouse();
    MGL_bitBltCoord(workpage, sceneDC, game_offset, 0, game_offset + 0x280, 0x168, 0, 0, 0);
    BlitPageToPage(workpage, backpage, 0, 0, 0x280, 0x168);
    for (int k = 0; k < number_of_species; k++) {
        SPRITE *sp = &sprites[sp_sort[k]];
        if (sp->active)
            ScaleGraphic2(sp->image + sp->frame, sp->x, sp->y, sp->z, sp->percent, sp->flip);
    }
    BlitPageToPage(backpage, frontpage, 0, 0, 0x280, 0x168);
    DrawCompass();
    BlitCompass();
    ShowMouse();
}

/* scn_NN.256: palette, path/scale/depth maps and a 4480x360 panorama */
int LoadGameScene(void)
{
    uchar hdr[8], hdrpal[768], bitmapInfo[16];
    char fname[1100];
    sceneDC = MGL_createMemoryDC(0x1180, 0x168);
    if (!sceneDC) { Log("game screen failed"); return 0; }
    uchar *surface = sceneDC->surface;
    int bytesPerLine = sceneDC->bytesPerLine;
    snprintf(fname, sizeof fname, "%sscn_%02d.256", cdPath, player->background + 1);
    FHANDLE fh = FileOpen(fname);
    if (fh == BAD_HANDLE) { Log("ERROR: LoadGameScene: %s not found", fname); return 0; }
    FileRead(fh, hdr, 8);
    FileRead(fh, hdrpal, 0x300);
    short number = (short)(hdr[4] | hdr[5] << 8);
    if (number != 4) {
        Log("Error: LoadGameScene: number = %d", number);
        FileClose(fh);
        return 0;
    }
    memcpy(gamepalette, hdrpal, 0x300);
    for (int i = 0x61; i < 100; i++) {
        if (graphic[i].data) {
            Log("ERROR: LoadGraphics: file %s, graphic %d not freed", fname, i);
            free(graphic[i].data);
            graphic[i].data = NULL;
        }
        readGraphicHeader(fh, &graphic[i]);
        long size = (long)graphic[i].height * graphic[i].width;
        graphic[i].data = SafeMalloc(size);
        if (!graphic[i].data) {
            Log("ERROR: Out of memory, LoadGraphics: file %s, graphic %d, size %ld", fname, i, size);
            FileClose(fh);
            return 0;
        }
        FileRead(fh, graphic[i].data, (uint32_t)size);
    }
    FileRead(fh, bitmapInfo, 0x10);
    for (int y = 0; y < 0x168; y++) {
        FileRead(fh, surface, 0x1180);
        surface += bytesPerLine;
    }
    FileClose(fh);
    return 1;
}

void FreeGameScene(void)
{
    MGL_destroyDC(sceneDC);
    sceneDC = NULL;
    FreeGraphics(0x61, 3);
}

void DrawHuntScreen(void)
{
    char file[32];
    FadeToBlack();
    LoadGameScene();
    MGL_bitBltCoord(workpage, sceneDC, game_offset, 0, game_offset + 0x280, 0x168, 0, 0, 0);
    SpeciesRatio();
    if (player->guide == 0) LoadGraphics(200, "chr_99_1");
    else LoadGraphics(200, "chr_99_2");
    InitInterfaceIcons();
    FreeGraphics(0, 0x22);
    LoadGraphics(0, "ptr_99");
    FreeGraphics(0xbe, 1);
    LoadGraphics(0xbe, "dlg_99");
    ClrFontFree(0);
    ClrFontFree(1);
    ClrFontFree(2);
    ClrFontFree(3);
    LoadFonts("fnt_99.dat");
    DrawPointer = DrawMouse;
    CopyPointer = CopyMouse;
    hunter.used_bugspray = 0;
    hunter.used_sunscreen = 0;
    hunter.gunShells = 0;
    hunter.shots_fired = 0;
    hunter.game_over = 0;
    hunter.gun_empty = 0;
    hunter.over_limit = 0;
    hunter.shot_something = 0;
    pending_comment = 0;
    time_total = time_hour * 60 + time_min;
    bFlipMouse = 1;
    hunt_pointer = 0;
    update_score = 1;
    update_clock = 1;
    update_temp = 1;
    update_wind = 1;
    RestoreHuntScreen();
    SoundFade(0x10);
    StopMusic();
    FadeFromBlack();
    InitSprites();
    for (int s = 0; s < number_of_species; s++) SpriteGone(s);
    snprintf(file, sizeof file, "chr_99_%d", player->guide + 1);
    for (int i = 0; i < 10; i++) SoundSeek(file, i, 0x1d + i);
    for (int i = 0; i < 5; i++) SoundSeek("scn_1", i, i + 0x12);
    SoundLoop(0x12);
    ShowMouse();
    MouseNormal();
}

/* ------------------------------------------------------ animal sprites -- */
static void setBounds(SPRITE *sp)
{
    GRAPHIC *g = &graphic[sp->frame + sp->image];
    sp->bx = g->hotx + sp->x;
    sp->by = g->hoty + sp->y;
    sp->bw = g->width;
    sp->bh = g->height;
}

static void setSeq(SPRITE *sp, const int *seq, int n, int numframes)
{
    for (int i = 0; i < n; i++) sp->sequence[i] = seq[i];
    sp->numframes = numframes;
    sp->seqindex = 0;
    sp->frame = 0;
}

static const int seq_walk[4] = {0, 1, 2, 3};

void InitSprites(void)
{
    sprites[0].active = 0;
    sprites[1].active = 0;
    sprites[2].active = 0;
    for (int s = 0; s < number_of_species; s++) {
        SPRITE *sp = &sprites[s];
        sp->id = (char)s;
        sp->base_image = 0;
        sp->image = sp->id * 0x3a + 300;
        sp->frame = 0;
        sp->active = 1;
        sp->x = 0;
        sp->y = 0x1e0;
        sp->z = 0;
        setBounds(sp);
        sp->lastx = sp->bx;
        sp->lasty = sp->by;
        sp->lastw = sp->bw;
        sp->lasth = sp->bh;
        sp->percent = 10;
        sp->flip = 0;
        sp->direction = 2;
        sp->shot = 0;
        setSeq(sp, seq_walk, 4, 3);
        sp->scale = 10;
        sp->action = 0;
        sp->fptr = SpriteWait;
    }
    for (int i = 0; i < 5; i++) sp_sort[i] = i;
}

/* an empty slot counts down until another animal wanders in */
void SpriteWait(int s)
{
    waitCounter[s]++;
    if (waitTime[s] < waitCounter[s]) SpriteGone(s);
}

void SpriteGone(int s)
{
    SPRITE *sp = &sprites[s];
    if (sp->shot) {
        BlitPageToPage(backpage, workpage, sp->lastx, sp->lasty, sp->lastw + 8, sp->lasth + 8);
        BlitPageToPage(backpage, frontpage, sp->lastx, sp->lasty, sp->lastw + 8, sp->lasth + 8);
        sp->y = 800;
    }
    hunter.shots_fired = 0;
    sp->species = PickSpecies(s);
    if (sp->species == 0xff) InitGone(s);
    else InitWalk(s);
}

void InitGone(int s)
{
    waitTime[s] = rand() % 0x1e + 0x1e;
    waitCounter[s] = 0;
    sprites[s].action = 0;
    sprites[s].fptr = SpriteWait;
}

void InitWalk(int s)
{
    SPRITE *sp = &sprites[s];
    sp->active = 1;
    sp->direction = 0;
    sp->base_image = 0;
    sp->image = sp->id * 0x3a + 300;
    setSeq(sp, seq_walk, 4, 3);
    sp->flip = (char)(rand() % 2);
    sp->action = 1;
    sp->fptr = SpriteWalk;
    sp->shot = 0;
    sp->charging = 0;
    sp->provoked = 0;
    sp->x = rand() % 3000;
    for (int tries = 0;; tries++) {
        sp->y = GetSpriteYPosition(s);
        if (sp->y != 0) break;
        if (tries > 10000) { InitGone(s); return; }   /* no walkable ground */
        sp->x = rand() % 3000;
    }
    sp->z = sp->y / 4;
    sp->percent = GetSpriteScale(sp->x, sp->y);
    if (sp->percent < 5) sp->percent = 5;
    AssignDelta(s, sp->flip * 4 + sp->direction);
    hunter.shots_fired = 0;
}

void AssignDelta(int s, int direction)
{
    SPRITE *sp = &sprites[s];
    const SPECIES_TYPE *t = &species[sp->species];
    if (direction < 0 || direction > 6) direction = 0;
    if (sp->action == 4) {
        sp->deltax = (double)t->run_x[direction];
        sp->deltay = (double)t->run_y[direction];
    } else {
        sp->deltax = (double)t->walk_x[direction];
        sp->deltay = (double)t->walk_y[direction];
    }
}

static void computeSteps(SPRITE *sp, int *stepx, int *stepy)
{
    *stepx = (int)((double)sp->percent * sp->deltax) / 100;
    *stepy = (int)((double)sp->percent * sp->deltay) / 100;
    if (*stepx == 0) {
        if (sp->deltax < 0.0) *stepx = -1;
        else if (sp->deltax > 0.0) *stepx = 1;
    }
    if (*stepy == 0) {
        if (sp->deltay < 0.0) *stepy = -1;
        else if (sp->deltay > 0.0) *stepy = 1;
    }
}

static int pathTest(SPRITE *sp, int stepx, int stepy)
{
    int test = CheckSpritePath(sp->x + stepx * 5, sp->y + stepy * 5, 0);
    int lt = species[sp->species].landtype;
    if (test == 0xb && (lt & 1) == 0) test = 0;
    if (test == 7 && (lt & 2) == 0) test = 0;
    return test;
}

static void nextFrame(SPRITE *sp)
{
    sp->frame = sp->sequence[sp->seqindex & 15];
    sp->seqindex++;
}

static void setImage(SPRITE *sp, int direction)
{
    sp->image = sp->id * 0x3a + sp->base_image + direction * 0x10 + 300;
}

void SpriteWalk(int s)
{
    SPRITE *sp = &sprites[s];
    int stepx, stepy, direction = 0;
    nextFrame(sp);
    if (sp->numframes < sp->seqindex) {
        if (sp->action == 4 && (species[sp->species].landtype & 1)) {
            SpriteGone(s);
            CommentAtEnd();
            return;
        }
        sp->seqindex = 0;
    }
    computeSteps(sp, &stepx, &stepy);
    sp->x += stepx;
    sp->y += stepy;
    if (sp->action == 1) {
        char saveflip = sp->flip;
        int test = pathTest(sp, stepx, stepy);
        /* blocked: try the three directions, then the other facing */
        for (int pass = 0; pass < 2 && test == 0; pass++) {
            if (pass == 1) sp->flip ^= 1;
            for (int j = 0; j < 3; j++) {
                direction = j;
                AssignDelta(s, sp->flip * 4 + j);
                computeSteps(sp, &stepx, &stepy);
                test = pathTest(sp, stepx, stepy);
                if (test != 0) {
                    sp->direction = j;
                    setImage(sp, j);
                    break;
                }
            }
        }
        if (test == 0) {
            sp->flip = saveflip ^ 1;
            setImage(sp, direction);
            AssignDelta(s, sp->flip * 4 + sp->direction);
        }
    }
    if (sp->action == 1) {
        if (sp->direction == 0 && rand() % 0x14 < 5) {
            sp->direction = 1;
            setImage(sp, 1);
            AssignDelta(s, sp->flip * 4 + sp->direction);
        } else if (sp->direction == 1 && rand() % 0x14 == 5) {
            if (rand() % 10 < 8) {
                sp->direction = 0;
                setImage(sp, 0);
            } else {
                sp->direction = 2;
                setImage(sp, 2);
            }
            AssignDelta(s, sp->flip * 4 + sp->direction);
        }
        if (sp->percent > 100) {
            sp->percent = 100;
            sp->direction = 1;
            setImage(sp, 1);
            AssignDelta(s, sp->flip * 4 + sp->direction);
        }
        if (sp->percent == 100 && rand() % 10 < 5) {
            sp->direction = 2;
            setImage(sp, 2);
            AssignDelta(s, sp->flip * 4 + sp->direction);
        }
    }
    if (sp->x > 0xeff) sp->x = 0;
    if (sp->x < 0) sp->x = 0xeff;
    sp->percent = GetSpriteScale(sp->x, sp->y);
    if (sp->percent < 5) {
        SpriteGone(s);
        CommentAtEnd();
        return;
    }
    sp->z = sp->y / 4;
    if (sp->action == 2) {
        InitShot(s);
    } else if (sp->action == 1) {
        if (hunter.shots_fired == 0) {
            if (sp->seqindex == 0 && rand() % 0x14 == 8) InitStill(s);
        } else if (species[sp->species].dangerous == 0 || sp->percent < 0x29) {
            StartRun(s);
        } else {
            sp->provoked = 1;
            InitCharge(s);
        }
    }
}

void InitStill(int s)
{
    waitTime[s] = rand() % 10 + 10;
    waitCounter[s] = 0;
    sprites[s].fptr = SpriteStill;
}

void InitCharge(int s)
{
    SPRITE *sp = &sprites[s];
    if (sp->shot) return;
    sp->base_image = 0x30;
    sp->image = sp->id * 0x3a + sp->base_image + 300;
    setSeq(sp, seq_walk, 4, 3);
    sp->action = 5;
    sp->charging = 1;
    sp->fptr = SpriteCharge;
    AssignDelta(s, 3);
    SoundPlay(sp->id * 2 + 0x17);
    pending_comment = rand() % 2 + 0x39;
}

void InitShot(int s)
{
    static const int seq[16] = {0, 0, 0, 0, 0, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2};
    SPRITE *sp = &sprites[s];
    sp->base_image = 10;
    if (sp->action == 5) sp->image = sp->id * 0x3a + 0x160;
    else setImage(sp, sp->direction);
    setSeq(sp, seq, 16, 0xf);
    sp->action = 2;
    sp->fptr = SpriteShot;
}

void StartRun(int s)
{
    SPRITE *sp = &sprites[s];
    sp->base_image = 4;
    sp->direction = 2;
    setImage(sp, sp->direction);
    setSeq(sp, seq_walk, 4, 3);
    sp->action = 4;
    sp->fptr = SpriteWalk;
    AssignDelta(s, sp->flip * 4 + sp->direction);
}

static void reactToShot(int s)
{
    SPRITE *sp = &sprites[s];
    if (species[sp->species].dangerous == 0 || sp->percent < 0x29) {
        StartRun(s);
    } else {
        sp->provoked = 1;
        InitCharge(s);
    }
}

void SpriteStill(int s)
{
    SPRITE *sp = &sprites[s];
    waitCounter[s]++;
    if (waitTime[s] < waitCounter[s]) {
        int r = rand() % 0x14;
        if (sp->action == 6) {
            if (r > 10) InitLookAway(s);
        } else if (r < 0xf) {
            InitFeed(s);
        } else if (r < 10) {
            InitLook(s);        /* unreachable in the original, kept as is */
        } else {
            StartWalk(s);
        }
    }
    if (hunter.shots_fired) reactToShot(s);
}

void InitFeed(int s)
{
    static const int seq[16] = {0, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 1, 0};
    SPRITE *sp = &sprites[s];
    sp->base_image = 0xd;
    setImage(sp, sp->direction);
    setSeq(sp, seq, 16, 0xf);
    sp->action = 3;
    sp->fptr = SpriteFeed;
}

void InitLook(int s)
{
    static const int seq[9] = {0, 1, 1, 1, 1, 1, 1, 1, 1};
    SPRITE *sp = &sprites[s];
    sp->base_image = 8;
    setImage(sp, sp->direction);
    setSeq(sp, seq, 9, 8);
    sp->action = 6;
    sp->fptr = SpriteLook;
}

void InitLookAway(int s)
{
    static const int seq[6] = {1, 1, 1, 0, 0, 0};
    SPRITE *sp = &sprites[s];
    sp->base_image = 8;
    setImage(sp, sp->direction);
    setSeq(sp, seq, 6, 5);
    sp->action = 7;
    sp->fptr = SpriteLook;
}

void StartWalk(int s)
{
    SPRITE *sp = &sprites[s];
    sp->base_image = 0;
    setImage(sp, sp->direction);
    setSeq(sp, seq_walk, 4, 3);
    sp->action = 1;
    sp->fptr = SpriteWalk;
    AssignDelta(s, sp->flip * 4 + sp->direction);
}

void SpriteFeed(int s)
{
    SPRITE *sp = &sprites[s];
    nextFrame(sp);
    if (sp->numframes < sp->seqindex) sp->seqindex = 0;
    if (sp->seqindex == 0) {
        if (rand() % 10 == 5) {
            StartWalk(s);
        } else if (species[sp->species].aggressive == 0 || sp->percent < 0x33) {
            InitStill(s);
        } else {
            sp->provoked = 0;
            InitCharge(s);
        }
    }
    if (hunter.shots_fired) reactToShot(s);
}

void SpriteLook(int s)
{
    SPRITE *sp = &sprites[s];
    nextFrame(sp);
    if (sp->numframes < sp->seqindex) sp->seqindex = 0;
    if (sp->seqindex == 0) {
        if (species[sp->species].aggressive == 0 || sp->percent < 0x33) {
            InitStill(s);
        } else {
            sp->provoked = 0;
            InitCharge(s);
        }
    } else if (hunter.shots_fired) {
        reactToShot(s);
    }
}

void SpriteCharge(int s)
{
    SPRITE *sp = &sprites[s];
    nextFrame(sp);
    if (sp->numframes < sp->seqindex) sp->seqindex = 0;
    int stepy = (int)((double)sp->percent * sp->deltay) / 100;
    if (stepy == 0) stepy = (int)sp->deltay;
    sp->y += stepy;
    sp->percent = GetSpriteScale(sp->x, sp->y);
    sp->z = sp->y / 4;
    if (sp->y > 0x140) InitKill(s);
}

void InitKill(int s)
{
    static const int seq[4] = {0, 1, 2, 2};
    SPRITE *sp = &sprites[s];
    sp->base_image = 0x37;
    sp->image = sp->id * 0x3a + sp->base_image + 300;
    setSeq(sp, seq, 4, 3);
    sp->action = 5;
    sp->charging = 1;
    sp->fptr = SpriteKill;
    SoundPlay(sp->id * 2 + 0x18);
}

void SpriteKill(int s)
{
    SPRITE *sp = &sprites[s];
    nextFrame(sp);
    if (sp->numframes < sp->seqindex) {
        sp->seqindex = 0;
        hunter.lifebar_other += 75;
        update_lifebar = 1;
        if ((int)hunter.lifebar_sun - (int)hunter.lifebar_other < 0x1e) {
            hunter.game_over = 0x14;
            DoGuideVoice(player->guide + 1, 0x3f);
            guide_said[0x37] = 1;
            FadeToRed();
            for (int i = 3; i < 0x300; i += 3) {
                gamepalette[i] = 0xff;
                gamepalette[i + 1] = 0;
                gamepalette[i + 2] = 0;
            }
            SetPalette();
            BlitWorkToBack();
            BlitBackToFront();
            TimeDelay(0);
            TimeDelay(1000);
        }
    }
    sp->y += 10;
    sp->percent += 10;
    sp->z = sp->y / 4;
}

void SpriteShot(int s)
{
    SPRITE *sp = &sprites[s];
    nextFrame(sp);
    if (sp->numframes < sp->seqindex) {
        sp->seqindex = 0;
        SpriteGone(s);
        CommentAtEnd();
    }
}

void DumpHuntScreen(void)
{
    ExitInterfaceIcons();
    FreeGameScene();
    FreeGraphics(300, 0xae);
    FreeGraphics(500, 0xae);
    FreeGraphics(200, 7);
    PutAmmoBackInBox();
    bFlipMouse = 0;
    InitMouseSave();
    FadePalette(0xff);
    for (int i = 0; i < 5; i++) { SoundStop(i + 0x12); SoundFree(i + 0x12); }
    for (int i = 0; i < 10; i++) { SoundStop(i + 0x1d); SoundFree(i + 0x1d); }
    for (int i = 0; i < number_of_species; i++) {
        SoundStop(i * 2 + 0x17);
        SoundStop(i * 2 + 0x18);
        SoundFree(i * 2 + 0x17);
        SoundFree(i * 2 + 0x18);
    }
}

/* ----------------------------------------------------- weapon & sights -- */
void SwitchWeapon(int weapon)
{
    switch (weapon) {
    case 0:
    case 1:
        if (hunter.has_scope[weapon] == 0) {
            mouse_shape = 9;
            DrawWeapon = DrawMouse;
        } else {
            mouse_shape = 6;
            DrawWeapon = DrawScope;
        }
        CopyWeapon = CopyMouse;
        break;
    case 2:
        mouse_shape = 8;
        DrawWeapon = DrawBowPeep;
        CopyWeapon = CopyBowPeep;
        break;
    case 200:
        mouse_shape = 4;
        DrawWeapon = DrawBinocs;
        CopyWeapon = CopyMouse;
        break;
    default:
        mouse_shape = 0;
        DrawWeapon = DrawMouse;
        CopyWeapon = CopyMouse;
        break;
    }
    gunSight = 0;
}

/* Iron sights: dragging with the right button walks the bead around a
 * 5x5 grid of cursor images instead of moving the pointer. */
void AlignGunSight(void)
{
    int x = gunSight % 5;
    int y = gunSight / 5;
    if (lastMouseX < mouseX) x++;
    if (mouseX < lastMouseX) x--;
    if (lastMouseY < mouseY) y++;
    if (mouseY < lastMouseY) y--;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x > 4) x = 4;
    if (y > 4) y = 4;
    gunSight = y * 5 + x;
    mouseX = lastMouseX;
    mouseY = lastMouseY;
    MS_moveTo(lastMouseX, lastMouseY);
    SetMouseGraphic(gunSight + 9);
}

void AlignGunSight2(void)
{
    int x = gunSight % 5;
    int y = gunSight / 5;
    if (lastInput == 0x200) y--;
    if (lastInput == 0x400) y++;
    if (lastInput == 0x800) x--;
    if (lastInput == 0x1000) x++;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x > 4) x = 4;
    if (y > 4) y = 4;
    gunSight = y * 5 + x;
    HideMouse();
    SetMouseGraphic(gunSight + 9);
    ShowMouse();
}

void AlignBowSight(void)
{
    if (!hunter.has_weapon) return;
    if (lastMouseX + 4 < mouseX) bow_sight_x++;
    if (mouseX < lastMouseX - 4) bow_sight_x--;
    if (lastMouseY + 4 < mouseY) bow_sight_y++;
    if (mouseY < lastMouseY - 4) bow_sight_y--;
    if (bow_sight_x < 0) bow_sight_x = 0;
    if (bow_sight_x > 8) bow_sight_x = 8;
    if (bow_sight_y < 0) bow_sight_y = 0;
    if (bow_sight_y > 8) bow_sight_y = 8;
    mouseX = lastMouseX;
    mouseY = lastMouseY;
    MS_moveTo(lastMouseX, lastMouseY);
}

void AlignBowSight2(void)
{
    if (!hunter.has_weapon) return;
    if (lastInput == 0x200) bow_sight_y--;
    if (lastInput == 0x400) bow_sight_y++;
    if (lastInput == 0x800) bow_sight_x--;
    if (lastInput == 0x1000) bow_sight_x++;
    if (bow_sight_x < 0) bow_sight_x = 0;
    if (bow_sight_x > 8) bow_sight_x = 8;
    if (bow_sight_y < 0) bow_sight_y = 0;
    if (bow_sight_y > 8) bow_sight_y = 8;
}

/* screen x of a panorama x coordinate, wrapping round the 360 degree view */
static int screenX(int x)
{
    int off;
    if (game_offset < 0xc81 || game_offset <= x) off = -game_offset;
    else off = 0xf00 - game_offset;
    x += off;
    if (x > 0xc80) x -= 0xf00;
    return x;
}

/* -------------------------------------------------------- main loop -- */
#ifdef SAFARI_TESTHOOKS
/* test builds: find a screen point that would be a kill (or with
 * SAFARI_TEST_AIM=wound, a body hit outside the kill zone) */
static int test_find_aim(int *ax, int *ay)
{
    const char *mode = getenv("SAFARI_TEST_AIM");
    if (!mode) return 0;
    int want_kill = strcmp(mode, "wound") != 0;
    for (int k = 0; k < number_of_species; k++) {
        int s = sp_sort[k];
        SPRITE *sp = &sprites[s];
        if (!sp->active || sp->shot || sp->species == 0xff) continue;
        if (sp->screenx < 1 || sp->screenx + sp->bw > 639) continue;
        for (int y = sp->by; y < sp->by + sp->bh && y < 0x168; y++)
            for (int x = sp->screenx; x < sp->screenx + sp->bw; x++)
                if (CheckLayer(s, x + game_offset, y) &&
                    CheckSpriteData(s, x - sp->screenx, y - sp->by, sp->flip, sp->percent) &&
                    !!CheckKillZone(s, x - sp->screenx, y - sp->by, sp->flip, sp->percent) == want_kill) {
                    *ax = x; *ay = y;
                    return 1;
                }
    }
    return 0;
}

static void test_aim(void)
{
    static uint32_t last;
    if (!getenv("SAFARI_TEST_AIM") || GetLapCount() - last < 1000) return;
    last = GetLapCount();
    for (int k = 0; k < number_of_species; k++)
        Log("SPR %d act %d spc %d x %d screenx %d by %d bw %d bh %d off %d", k, sprites[k].active,
            sprites[k].species, sprites[k].x, sprites[k].screenx, sprites[k].by, sprites[k].bw, sprites[k].bh, game_offset);
    int x, y;
    if (test_find_aim(&x, &y)) Log("AIM %d %d", x, y);
}
#endif

void DoHuntScreen(void)
{
#ifdef SAFARI_TESTHOOKS
    test_aim();
#endif
    if (mouseY < 0x14a) {
        if (mouseX < 0x32) MoveScreen(-(0x32 - mouseX));
        else if (mouseX > 0x24e) MoveScreen(mouseX - 0x24e);
    }
    AdvanceClock();
    HideMouseOnPage(backpage);
    CoverHotSpots(hunt_hotspots);
    if (update_sprite) {
        for (int k = 0; k < number_of_species; k++) {
            int s = sp_sort[k];
            SPRITE *sp = &sprites[s];
            if (!sp->active) continue;
            BlitPageToPage(workpage, backpage, screenX(sp->lastx), sp->lasty, sp->lastw, sp->lasth);
            if (sp->fptr) sp->fptr(s);
            GRAPHIC *g = &graphic[sp->frame + sp->image];
            if (sp->flip == 0)
                sp->bx = sp->x + (g->hotx * sp->percent) / 100;
            else
                sp->bx = sp->x - ((g->hotx * sp->percent) / 100 + (g->width * sp->percent) / 100);
            sp->by = sp->y + (g->hoty * sp->percent) / 100;
            sp->bw = (g->width * sp->percent) / 100 + 2;
            sp->bh = (g->height * sp->percent) / 100 + 2;
            if (sp->y + sp->bh > 0x168) {
                update_clock = 1;
                update_score = 1;
            }
        }
        SortSprites(number_of_species);
    }
    if (lastInput == 6 && mouseY < 0x168) {
        if (hunter.weapon == 2) AlignBowSight();
        else if (mouse_shape == 9) AlignGunSight();
    }
    if (update_sprite) {
        for (int k = 0; k < number_of_species; k++) {
            SPRITE *sp = &sprites[sp_sort[k]];
            if (sp->active)
                ScaleGraphic2(sp->frame + sp->image, sp->x, sp->y, sp->z, sp->percent, sp->flip);
        }
    }
    if (update_lifebar) DrawLifebar();
    if (update_score) DrawScore();
    if (update_temp) DrawTemperature();
    if (update_wind) DrawWind();
    if (update_clock) DrawClock();
    if (mouseY < 0x168) {
        if (gunSight + mouse_shape != hunt_pointer) {
            HideMouseOnPage(backpage);
            HideMouseOnPage(frontpage);
            SetMouseGraphic(gunSight + mouse_shape);
            hunt_pointer = gunSight + mouse_shape;
            DrawPointer = DrawWeapon;
            CopyPointer = CopyWeapon;
        }
    } else if (hunt_pointer != 0 && gunSight + mouse_shape == hunt_pointer) {
        HideMouseOnPage(backpage);
        HideMouseOnPage(frontpage);
        SetMouseGraphic(0);
        hunt_pointer = 0;
        DrawPointer = DrawMouse;
        CopyPointer = CopyMouse;
    }
    DrawPointer();
    if (update_sprite) {
        for (int k = 0; k < number_of_species; k++) {
            SPRITE *sp = &sprites[sp_sort[k]];
            if (!sp->active) continue;
            int x = screenX(sp->lastx);
            if (x < 0x280) BlitPageToPage(backpage, frontpage, x, sp->lasty, sp->lastw + 8, sp->lasth + 8);
            x = screenX(sp->bx);
            sp->screenx = x;
            if (x < 0x280) BlitPageToPage(backpage, frontpage, x, sp->by, sp->bw + 8, sp->bh + 8);
            sp->lastx = sp->bx;
            sp->lasty = sp->by;
            sp->lastw = sp->bw;
            sp->lasth = sp->bh;
        }
        update_sprite = 0;
    }
    if (update_lifebar) BlitLifebar();
    if (update_temp) BlitTemperature();
    if (update_score) BlitScore();
    if (update_wind) BlitWind();
    if (update_clock) BlitClock();
    CopyHotSpots(hunt_hotspots);
    CopyPointer();
    lastMouseX = mouseX;
    lastMouseY = mouseY;
    if (pending_comment != 0 && SoundPoll(hunter.weapon + 10) == 0) {
        /* PORT FIX: remarks 1-10 are preloaded into sounds 0x1d-0x26, so
         * remark N is sound 0x1c + N. The original added 0x1d and played
         * the next clip in the guide's list: a kill could get "I hope we
         * can find him", a wound "this could mean trouble", and remark 10
         * nothing at all. */
        if (debug) Log("guide remark %d", pending_comment);
        if (pending_comment < 0xb) PlayGuideSound(pending_comment + 0x1c);
        else DoGuideVoice(player->guide + 1, pending_comment);
        pending_comment = 0;
    }
    if (hunter.game_over != 0) {
        SwitchScreen(0x15);
        return;
    }
    if (time_hour > 0x14) {
        /* too dark to hunt: the guide takes you back to the lodge */
        DoGuideComment(player->guide + 1, rand() % 2 + 0x3b);
        SwitchScreen(8);
        lastInput = 0;
        return;
    }
    if (mouseY > 0x168) CheckHotSpots(hunt_hotspots);
    if (lastInput & 1) {
        if (mouseY < 0x168) {
            if (mouse_shape != 4) {
                ShootGun();
                CommentAfterShot();
            }
        } else {
            HOT_SPOT *hs = CheckHotSpots(hunt_hotspots);
            if (hs) hs->fptr(hs->param);
        }
    }
    if (lastInput == 2 && mouseY > 0x168) {
        HOT_SPOT *hs = CheckHotSpots(hunt_hotspots);
        if (hs) hs->fptr(hs->param);
    }
    if (lastInput > 0x1ff && mouseY < 0x168) {
        if (hunter.weapon == 2) AlignBowSight2();
        else if (mouse_shape == 9) AlignGunSight2();
    }
    lastInput = 0;
}

/* Play one of the guide's short remarks if nobody is talking. */
void PlayGuideSound(int sndnum)
{
    if (SoundPoll(sound_Voice) != 0) return;
    SoundStop(sound_Voice);
    SoundFree(sound_Voice);
    for (int i = 0x1d; i <= 0x26; i++) SoundStop(i);
    SoundPlay(sndnum);
}

/* --------------------------------------------------------------- shooting -- */
void ShootGun(void)
{
    int got_him = -1;
    int mx, my;
    SPRITE *sp = NULL;
    player->saved_game = 0;
    if (mouse_shape == 9) {
        mx = mouseX + (gunSight % 5 - 2) * 8;
        my = mouseY + (gunSight / 5 - 2) * 8;
    } else if (mouse_shape == 8) {
        mx = mouseX + (bow_sight_x - 4) * 8;
        my = mouseY + (bow_sight_y - 4) * 8;
    } else {
        mx = mouseX;
        my = mouseY;
    }
    if (hunter.gunShells == 0) {
        hunter.gun_empty = 1;
        return;
    }
#ifdef SAFARI_TESTHOOKS
    if (test_find_aim(&mx, &my)) Log("TESTSHOT %d %d (%s)", mx, my, getenv("SAFARI_TEST_AIM"));
#endif
    SoundPlay(hunter.weapon + 10);
    hunter.gunShells--;
    if (hunter.weapon != 2) hunter.shots_fired = 1;
    update_score = 1;
    pending_comment = 0;
    int s = 0;
    for (int k = 0; k < number_of_species; k++) {
        s = sp_sort[k];
        sp = &sprites[s];
        if (hunter.weapon == 2 && sp->percent <= 0x27) continue;   /* out of bow range */
        if (sp->species == 0xff || sp->shot) continue;
        int bbx = sp->screenx;
        int bby = sp->by;
        if (bbx < 1 || bbx > 0x27f) {
            /* off screen: the noise startles it */
            if (species[sp->species].dangerous == 0) {
                if (pending_comment == 0 && rand() % 5 == 3) pending_comment = rand() % 2 + 7;
            } else if (rand() % 2 == 1) {
                if (sp->percent < 0x29) {
                    StartRun(s);
                    if (pending_comment == 0 && rand() % 5 == 3) pending_comment = rand() % 2 + 9;
                } else {
                    InitCharge(s);
                    pending_comment = rand() % 2 + 0xb;
                }
            } else {
                StartRun(s);
                if (pending_comment == 0 && rand() % 5 == 3) pending_comment = rand() % 2 + 9;
            }
        } else if (bbx <= mx && mx < sp->bw + bbx && bby <= my && my < sp->bh + bby &&
                   CheckLayer(s, mx + game_offset, my) &&
                   CheckSpriteData(s, mx - bbx, my - bby, sp->flip, sp->percent)) {
            if (CheckKillZone(s, mx - bbx, my - bby, sp->flip, sp->percent)) {
                sp->shot = 1;
                got_him = s;
                break;
            }
            /* wounded */
            if (species[sp->species].dangerous == 0) {
                if (pending_comment == 0 && rand() % 5 == 3) pending_comment = rand() % 2 + 3;
            } else {
                if (pending_comment == 0 && rand() % 5 == 3) pending_comment = rand() % 2 + 5;
                if (rand() % 2 == 1) {
                    StartRun(s);
                } else if (sp->percent < 0x29) {
                    StartRun(s);
                } else {
                    InitCharge(s);
                    pending_comment = rand() % 2 + 0xb;
                }
            }
        }
    }
    if (hunter.weapon != 2) ScareAnimals();
    if (debug) Log("ShootGun: %s", got_him == -1 ? "no kill" : "kill");
    if (got_him == -1) return;
    if (species[sp->species].dangerous == 0 || hunter.weapon != 1) {
        GAMEBAG *b = &huntbag[hunter.gameBagNum];
        b->shot = 1;
        b->species = sp->species;
        b->trophy_fee = species[sp->species].trophy_fee;
        b->charging = sp->charging;
        b->provoked = sp->provoked;
        hunter.gameBagNum++;
        if (hunter.gameBagNum > 0x45) hunter.gameBagNum = 0x45;
        player->gamebag[sp->species]++;
        hunter.shot_something = 1;
        InitShot(got_him);
        pending_comment = 0;
        if (sp->charging == 0) {
            if (species[sprites[got_him].species].weapon == 1 && hunter.weapon == 0) {
                /* plains game taken with the .458 */
                if (rand() % 9 == 4) pending_comment = rand() % 2 + 0xf;
            } else if (rand() % 5 == 3) {
                pending_comment = rand() % 2 + 1;
            }
        } else if (sp->provoked == 0) {
            pending_comment = rand() % 2 + 0x2f;
        } else {
            pending_comment = rand() % 2 + 0x2d;
        }
    } else {
        /* dangerous game with the .30-06: it just makes them angry */
        if (rand() % 2 == 1) {
            pending_comment = rand() % 2 + 0xd;
            InitCharge(s);
        } else {
            if (pending_comment == 0 && rand() % 5 == 3) pending_comment = rand() % 2 + 5;
            StartRun(s);
        }
    }
}

void CommentAfterShot(void)
{
    if (hunter.gun_empty && !hunter.has_ammo) pending_comment = rand() % 2 + 0x13;
}

/* the guide's advice once an animal has left the scene */
void CommentAtEnd(void)
{
    if (!hunter.has_weapon) {
        if (!guide_said[0x11]) {
            pending_comment = rand() % 2 + 0x11;
            guide_said[0x11] = 1;
        }
    } else if (hunter.weapon == 0 && player->supplies[0xd] == 0) {
        pending_comment = rand() % 2 + 0x15;
    } else if (hunter.weapon == 1 && player->supplies[0xe] == 0) {
        pending_comment = rand() % 2 + 0x15;
    } else if (hunter.weapon == 2 && player->supplies[0xf] == 0) {
        pending_comment = rand() % 2 + 0x17;
    } else if (!hunter.has_scope[0] && !hunter.has_scope[1] && rand() % 10 == 5 && !guide_said[0x19]) {
        pending_comment = rand() % 2 + 0x19;
        guide_said[0x19] = 1;
    } else if (!hunter.has_canteen && rand() % 10 == 5) {
        pending_comment = rand() % 2 + 0x1b;
    } else if (!hunter.has_sunscreen && !guide_said[0x1f]) {
        pending_comment = rand() % 2 + 0x1f;
        guide_said[0x1f] = 1;
    } else if (!hunter.has_bugspray && !guide_said[0x23]) {
        pending_comment = rand() % 2 + 0x23;
        guide_said[0x23] = 1;
    } else if (!hunter.has_pills && !guide_said[0x27]) {
        pending_comment = rand() % 2 + 0x27;
        guide_said[0x27] = 1;
    } else if (player->supplies[1] == 0 && !guide_said[0x29]) {
        pending_comment = rand() % 2 + 0x29;
        guide_said[0x29] = 1;
    } else if ((player->supplies[3] == 0 || player->supplies[4] == 0) && !guide_said[0x2b]) {
        pending_comment = rand() % 2 + 0x2b;
        guide_said[0x2b] = 1;
    } else if (player->supplies[2] == 0 && !guide_said[0x31]) {
        pending_comment = rand() % 2 + 0x31;
        guide_said[0x31] = 1;
    }
}

void ExitHunt(void)
{
    SwitchScreen(0x10);
}
