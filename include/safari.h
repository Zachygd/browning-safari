/*
 * safari.h - The Browning African Safari (Oquirrh Productions, 1998)
 * Native port: shared types, globals and prototypes.
 *
 * Structure layouts follow the shipping SAFARI.EXE (Aug 26 1998 build),
 * reconstructed from the developers' PDB plus the binary itself.
 */
#ifndef SAFARI_H
#define SAFARI_H

#include <stdint.h>
#include <stddef.h>
#include "mgl.h"

typedef unsigned char  uchar;
typedef uint16_t       ushort;
typedef uint32_t       ulong32;

#define SCREEN_W 640
#define SCREEN_H 480

/* ---------------------------------------------------------------- data -- */

typedef struct {
    int32_t  size;
    int16_t  width, height;
    int16_t  hotx, hoty;
    uint8_t *data;
} GRAPHIC;

typedef struct SPRITE {
    char   active;
    char   id;
    char   flip;
    char   shot;
    int    x, y, z;
    int    bx, by, bw, bh;
    int    lastx, lasty, lastw, lasth;
    int    base_image;
    int    image;
    int    frame;
    int    numframes;
    int    percent;
    int    seqindex;
    int    direction;
    int    sequence[16];
    int    species;
    int    charging;
    int    provoked;
    int    scale;
    int    action;
    int    screenx;
    double deltax, deltay;
    double xAccum, yAccum;
    void (*fptr)(int);
} SPRITE;

typedef struct {
    int id;
    int aggressive;
    int dangerous;
    int landtype;
    int weapon;
    int min_permits;
    int max_permits;
    int population;
    int trophy_fee;
    int walk_x[7];
    int walk_y[7];
    int run_x[7];
    int run_y[7];
} SPECIES_TYPE;

typedef struct {
    int  ammo;
    int  weapon;
    int  gunShells;
    int  gameBagNum;
    int  scareFactor;
    int  totalArrows;
    int  game_over;
    char shots_fired;
    char used_bugspray;
    char used_sunscreen;
    char gun_empty;
    char shot_something;
    char over_limit;
    uchar lifebar_sun;
    uchar lifebar_other;
    char has_weapon;
    char has_ammo;
    char has_license;
    char has_binocs;
    char has_scope[2];
    char has_canteen;
    char has_bugspray;
    char has_sunscreen;
    char has_pills;
    char x6e;
    char x6f;
} HUNTER;

/* On-disk player record (GAME%02d.SAV): exactly 0xE4 bytes, little endian. */
typedef struct {
    char     name[32];
    uchar    unk20;
    uchar    music_volume;
    uchar    sound_volume;
    uchar    game_over;
    uchar    saved_game;
    uchar    finished;
    uchar    background;
    uchar    lounge_talk;
    uchar    village_talk;
    uchar    travel;
    uchar    guide;
    uchar    been_to_africa;
    uchar    last_screen;
    uchar    museum_complete;
    uchar    bagged_big_3;
    uchar    bonus_given;
    uchar    scopes[2];
    uchar    shop_talk;
    uchar    res[9];
    uint32_t dollars;
    uint32_t trophy_fees;
    uint32_t level_points;
    uint32_t total_points;
    ushort   supplies[17];
    ushort   suitcase[15];
    ushort   gamebag[7];
    ushort   shop_art[15];
    ushort   museum_trophy[7];
    ushort   museum_art[15];
} PLAYER;

typedef struct tag_HOT_SPOT {
    int  x, y, w, h;
    int  active;
    int  hilite;
    int  is_hilited;
    int  graphic_num;
    int  param;
    void (*fptr)(int);
    int  next;
} HOT_SPOT;

typedef struct {
    uchar  type;
    uchar  flags;
    int    style;
    uint32_t color;
    char  *text;
    int    x, y, w, h;
} OBJECT;

typedef struct {
    char    *name;
    uint32_t cost;
    int      xpos;
    int      qty;
    int      status;
} SHOP_ITEM;

typedef struct {
    uint32_t reward1;
    uint32_t reward2;
    int      xpos;
} TROPHY_ITEM;

typedef struct {
    uint32_t reward;
    int      xpos;
} ART_ITEM;

typedef struct {
    int shot;
    int species;
    int trophy_fee;
    int charging;
    int provoked;
} GAMEBAG;

typedef struct {
    ushort width;
    ushort height;
    short  colors;
    uchar  palette[16][3];
    ushort letterstarts[129];
    ushort letterwidths[129];
    ushort letterkerns[129];
    uchar *bitmap;
} CLRFONT;

typedef struct {
    int  x, y, w, h;
    int  font;
    int  cursorx;
    int  len;
    int  infield;
    int  index;
    char str[32];
    uchar color;
} EDITFIELDINFO;

typedef struct { int x, y; } COORDS;

/* file handle used by the engine's File* wrappers */
typedef void *FHANDLE;
#define BAD_HANDLE ((FHANDLE)(intptr_t)-1)

/* ------------------------------------------------------- engine globals -- */
#define MAX_GRAPHICS 1000
extern GRAPHIC   graphic[MAX_GRAPHICS];
/* per-scene maps live in fixed graphic slots: path (1120x90, 4x4 cells),
 * scale (2 bytes per scan line) and depth (one byte per pixel) */
#define path_map  (graphic[0x61].data)
#define scale_map (graphic[0x62].data)
#define layer_map (graphic[0x63].data)
extern int       maxGraphics;
extern palette_t pal[256];
extern uchar     gamepalette[768];
extern MGLDC    *frontpage, *backpage, *workpage, *activeDC;
extern char      filename[1100];
extern char      cdPath[1024];
extern char      basPath[1024];
extern char      savePath[1024];
extern int       debug;
extern int       init_DirectSound, init_DirectX, init_WinDirect, init_8bitsound;
extern int       colorBlack, colorWhite;
extern uchar     plug_in;

extern int  sound_volume, music_volume;
extern int  sound_Music, sound_Voice, num_Sound_Effects;
extern int  bSoundOkay, music_playing;

extern CLRFONT  *colorfont[11];
extern short     num_fonts;

/* ---------------------------------------------------------- engine API -- */
/* file.c */
FHANDLE  FileSave(const char *name);
FHANDLE  FileOpen(const char *name);
void     FileClose(FHANDLE fh);
uint32_t FileRead(FHANDLE fh, void *buffer, uint32_t size);
uint32_t FileWrite(FHANDLE fh, const void *buffer, uint32_t size);
void     FileSeek(FHANDLE fh, long size, int origin);
uint32_t FileSize(FHANDLE fh);
int      FileDelete(const char *name);

/* utils.c */
uint32_t GetLapCount(void);
void     TimeDelay(uint32_t how_long);
void     DebugStr(const char *format, ...);
void     Log(const char *format, ...);

/* sound.c */
int      SoundInit(void);
void     SoundExit(void);
uint32_t SoundSeek(const char *file, int seek, int sndnum);
void     SoundLoop(int sndnum);
void     SoundPlay(int sndnum);
void     SoundStop(int sndnum);
void     SoundFree(int sndnum);
void     SoundStopAll(void);
void     SoundWait(int sndnum);
int      SoundPoll(int sndnum);
int      SoundPosition(int sndhandle);
int      SoundGetVolume(int sndnum);
void     SoundSetVolume(int sndnum, int volume);
int      SoundGetMasterVolume(void);
void     SoundSetMasterVolume(int volume);
int      SoundGetFrequency(int sndnum);
void     SoundSetFrequency(int sndnum, int pitch);
void     SoundFade(int sndnum);
void     SoundPause(int sndnum);
void     SoundResume(int sndnum);
void     StopMusic(void);
int      MusicShutoff(void);
void     StartMusic(const char *file);
int      SoundLoad(const char *file, int sndnum);
void     StartMusicLoop(const char *file);
void     WaitMusic(void);
void     StartVoice(const char *file, int comment);

/* blit.c */
void BlitGraphic(int num, int xpos, int ypos, MGLDC *page);
void BlitGraphic2(int num, int xpos, int ypos, int height, MGLDC *page);
void BlitGraphicLayer(int num, int xpos, int ypos, int zpos);
void BlitShadow(int num, int xpos, int ypos, uchar color, MGLDC *page);
void ScaleGraphic(int num, int xpos, int ypos, int zpos, int percent, int flip);
void StretchGraphic(int num, int xpos, int ypos, int dwidth, int dheight);
void BlitDecoy(int num, int xpos, int ypos, int percent);
void BlitWorkToBack(void);
void BlitBackToFront(void);
void BlitPageToPage(MGLDC *src, MGLDC *dst, int x, int y, int w, int h);
void SaveImage(MGLDC *page, uchar *Image, int sx, int sy, int w, int h);
void RestoreImage(uchar *Image, MGLDC *page, int dx, int dy, int w, int h);
int  LoadBackground(const char *file);
int  LoadBackground2(const char *file);
int  LoadGraphics(int start, const char *file);
int  SeekGraphics(const char *file, int start, int seek, int number);
void readGraphicHeader(FHANDLE fh, GRAPHIC *g);
void FreeGraphics(int start, int num);

/* font.c */
void FontSetColor(int color);
void FontChoose(int choice);
int  FontLength(const char *str);
void FontDraw(int dx, int dy, const char *str, MGLDC *page);
void ClrFontDraw(int which, int dx, int dy, const char *str);
void ClrFontFormat(int font, int x, int y, const char *format, ...);
void ClrFontFormatRightJustify(int font, int x, int y, const char *format, ...);
void ClrFontFormatCenter(int font, int x, int y, const char *format, ...);
int  LoadFonts(const char *szName);
void RemapFonts(void);
int  FindRGB(const uchar *palette, int r, int g, int b);
void ClrFontFree(int which);
void FreeFonts(void);
int  ClrFontLength(int which, const char *str);
int  LetterWidth(int which, int letter);

/* effects.c */
void SpiralWipe(void);
void DiagonalWipe(void);
void DiagonalWipe2(void);
void FadePalette(int intensity);
void DissolveArea(int xpos, int ypos, int width, int height);
void RandomWipe(void);
void Checkerboard(void);
void CheckerboardArea(int xpos, int ypos, int wide, int high);

/* draw.c */
void RenderLine(int x1, int y1, int x2, int y2, uchar color);
void DrawRect(int x, int y, int w, int h, int color, int fill);
void DrawLine(int x1, int y1, int x2, int y2, uchar color);
void ClearScreen(void);
void ClearBackScreen(void);
void ShadowBox(int x, int y, int w, int h, uchar color);
void SunkenBox(int x, int y, int w, int h, uchar color);

/* dialog.c */
int  MouseInBox(int left, int top, int right, int bottom, int x, int y);
int  DLG_Check(OBJECT *d, int mousex, int mousey);
void DLG_Draw(OBJECT *d, int start, int stop);
void DLG_Form(OBJECT *d, int action);
void DLG_Center(OBJECT *d);

/* alert.c */
int  AlertBox(const char *str, const char *btn);
int  GetDialogInput(int *xpos, int *ypos);
int  ChooseBox(const char *title, const char *str);

/* helpers */
void *SafeMalloc(size_t n);
int   ScrollTick(void);

/* the original's Microsoft C runtime rand(): 15-bit LCG */
int  msvc_rand(void);
void msvc_srand(unsigned int seed);
#define rand()   msvc_rand()
#define srand(s) msvc_srand(s)

#include "game.h"

#endif
