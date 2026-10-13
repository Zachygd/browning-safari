/*
 * mgl.h - minimal reimplementation of the parts of SciTech MGL 3.0 that
 * The Browning African Safari (1998) uses, on top of SDL2.
 *
 * Every device context (DC) is an 8-bit indexed surface with its own
 * 256-entry palette, exactly like MGL memory DCs.  The display DC's pixels
 * are what gets shown on screen, through the "hardware" palette that is
 * set with MGL_realizePalette().
 */
#ifndef MGL_H
#define MGL_H

#include <stdint.h>

/* MGL palette entry layout: blue, green, red, alpha */
typedef struct {
    uint8_t blue, green, red, alpha;
} palette_t;

typedef struct MGLDC {
    uint8_t  *surface;       /* pixel data */
    int       bytesPerLine;  /* pitch */
    int       width, height;
    int       maxx, maxy;    /* width-1, height-1 */
    int       clipL, clipT, clipR, clipB;
    palette_t pal[256];
    int       isDisplay;
} MGLDC;

/* events (MGL EVT_* values) */
enum {
    EVT_NULLEVT   = 0x00,
    EVT_KEYDOWN   = 0x01,
    EVT_KEYREPEAT = 0x02,
    EVT_KEYUP     = 0x04,
    EVT_MOUSEDOWN = 0x08,
    EVT_MOUSEUP   = 0x10,
    EVT_MOUSEMOVE = 0x20,
    EVT_EVERYEVT  = 0xFFFF
};

/* message masks for mouse buttons */
#define EVT_LEFTBMASK   0x0001
#define EVT_RIGHTBMASK  0x0004
/* modifier flags */
#define EVT_LEFTBUT     0x0001
#define EVT_RIGHTBUT    0x0002
#define EVT_SHIFTKEY    0x0018
#define EVT_CTRLSTATE   0x0020
#define EVT_ALTSTATE    0x0040

typedef struct {
    uint32_t which;
    uint32_t what;
    uint32_t when;
    int32_t  where_x;
    int32_t  where_y;
    uint32_t message;    /* key: ascii | scancode<<8 ; mouse: button mask */
    uint32_t modifiers;
} event_t;

MGLDC *MGL_createDisplayDC(void);
MGLDC *MGL_createMemoryDC(int xres, int yres);
void   MGL_destroyDC(MGLDC *dc);
MGLDC *MGL_makeCurrentDC(MGLDC *dc);

void MGL_bitBltCoord(MGLDC *dst, MGLDC *src, int left, int top, int right, int bottom,
                     int dstLeft, int dstTop, int op);
void MGL_transBltCoord(MGLDC *dst, MGLDC *src, int left, int top, int right, int bottom,
                       int dstLeft, int dstTop, uint32_t transparent, int sourceTrans);
void MGL_stretchBltCoord(MGLDC *dst, MGLDC *src, int left, int top, int right, int bottom,
                         int dstLeft, int dstTop, int dstRight, int dstBottom);

void MGL_setColor(uint32_t color);
void MGL_fillRectCoord(int left, int top, int right, int bottom);
void MGL_rectCoord(int left, int top, int right, int bottom);
void MGL_lineCoord(int x1, int y1, int x2, int y2);
void MGL_clearDevice(void);
void MGL_setClipRectDC(MGLDC *dc, int l, int t, int r, int b);

void MGL_setPalette(MGLDC *dc, const palette_t *pal, int numColors, int startIndex);
void MGL_realizePalette(MGLDC *dc, int numColors, int startIndex, int waitVRT);
int  MGL_fadePalette(MGLDC *dc, const palette_t *fullIntensity, int numColors, int startIndex,
                     uint8_t intensity);

/* screen region save/restore ("divots") */
typedef struct {
    int l, t, r, b;
    uint8_t data[];
} divot_t;
long MGL_divotSize(MGLDC *dc, int l, int t, int r, int b);
void MGL_getDivot(MGLDC *dc, int l, int t, int r, int b, divot_t *d);
void MGL_putDivot(MGLDC *dc, divot_t *d);

/* events */
int  EVT_getNext(event_t *evt, uint32_t mask);
int  EVT_peekNext(event_t *evt, uint32_t mask);
void EVT_flush(uint32_t mask);
void EVT_halt(event_t *evt, uint32_t mask);
void MS_moveTo(int x, int y);

/* platform services (sys.c) */
int      sys_init(void);
void     sys_shutdown(void);
void     sys_pump(void);
void     sys_present(int force);
uint32_t sys_ms(void);
void     sys_mark_dirty(void);
void     sys_throttle(uint32_t ms);
extern int sys_quit_requested;
extern void (*sys_quit_hook)(void);   /* runs once when the window is closed */
extern MGLDC *sys_display;

#endif
