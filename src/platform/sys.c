/*
 * sys.c - SDL2 window, display presentation and MGL-compatible event queue.
 *
 * The original game ran in a 640x480 8-bit fullscreen DirectDraw/WinDirect
 * mode.  Here the display DC is converted through its palette into a
 * 32-bit texture and scaled (keeping 4:3) into a resizable window.
 */
#include <SDL.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "mgl.h"

MGLDC *sys_display;
int sys_quit_requested;
void (*sys_idle_hook)(void);
void (*sys_quit_hook)(void);

static volatile sig_atomic_t got_signal;

/* SIGTERM / SIGINT / SIGHUP (logout, "End process" in System Monitor,
 * Ctrl+C): ask the main loop to quit, and make sure the process goes away
 * a few seconds later even if the game is stuck somewhere. */
static void on_signal(int sig)
{
    (void)sig;
    got_signal = 1;
    alarm(4);   /* default SIGALRM action terminates the process */
}

/* Window closed (Alt+F4, the close button) or a termination signal:
 * save whatever needs saving and leave immediately.  This is called from
 * the event pump, so it works no matter which screen or dialog is up. */
static void sys_quit_now(void)
{
    static int quitting;
    if (quitting) return;
    quitting = 1;
    sys_quit_requested = 1;
    alarm(5);   /* never hang on the way out */
    if (sys_quit_hook) sys_quit_hook();
    sys_shutdown();
    _exit(0);
}

static SDL_Window   *win;
static SDL_Renderer *ren;
static SDL_Texture  *tex;
static uint32_t      rgbaBuf[640 * 480];
static int           dirty = 1;
static uint32_t      lastPresent;
static uint32_t      lastPump;

/* --- event queue ------------------------------------------------------- */
#define QSIZE 256
static event_t queue[QSIZE];
static int qhead, qtail;
static int mouseX = 320, mouseY = 240;
static uint32_t buttons;   /* EVT_LEFTBUT / EVT_RIGHTBUT */

static uint32_t modState(void)
{
    SDL_Keymod m = SDL_GetModState();
    uint32_t r = buttons;
    if (m & KMOD_SHIFT) r |= EVT_SHIFTKEY;
    if (m & KMOD_CTRL)  r |= EVT_CTRLSTATE;
    if (m & KMOD_ALT)   r |= EVT_ALTSTATE;
    return r;
}

static void push(uint32_t what, uint32_t message)
{
    int next = (qtail + 1) % QSIZE;
    if (what == EVT_MOUSEMOVE) {
        /* collapse consecutive mouse moves like MGL does */
        int last = (qtail + QSIZE - 1) % QSIZE;
        if (qhead != qtail && queue[last].what == EVT_MOUSEMOVE) {
            queue[last].where_x = mouseX;
            queue[last].where_y = mouseY;
            queue[last].modifiers = modState();
            return;
        }
    }
    if (next == qhead) return; /* full: drop */
    event_t *e = &queue[qtail];
    e->which = 0;
    e->what = what;
    e->when = SDL_GetTicks();
    e->where_x = mouseX;
    e->where_y = mouseY;
    e->message = message;
    e->modifiers = modState();
    qtail = next;
}

/* PC/AT scan code set 1 for the SDL scancodes the game cares about */
static int pcScan(SDL_Scancode sc)
{
    switch (sc) {
    case SDL_SCANCODE_ESCAPE: return 0x01;
    case SDL_SCANCODE_1: return 0x02; case SDL_SCANCODE_2: return 0x03;
    case SDL_SCANCODE_3: return 0x04; case SDL_SCANCODE_4: return 0x05;
    case SDL_SCANCODE_5: return 0x06; case SDL_SCANCODE_6: return 0x07;
    case SDL_SCANCODE_7: return 0x08; case SDL_SCANCODE_8: return 0x09;
    case SDL_SCANCODE_9: return 0x0A; case SDL_SCANCODE_0: return 0x0B;
    case SDL_SCANCODE_MINUS: return 0x0C; case SDL_SCANCODE_EQUALS: return 0x0D;
    case SDL_SCANCODE_BACKSPACE: return 0x0E; case SDL_SCANCODE_TAB: return 0x0F;
    case SDL_SCANCODE_Q: return 0x10; case SDL_SCANCODE_W: return 0x11;
    case SDL_SCANCODE_E: return 0x12; case SDL_SCANCODE_R: return 0x13;
    case SDL_SCANCODE_T: return 0x14; case SDL_SCANCODE_Y: return 0x15;
    case SDL_SCANCODE_U: return 0x16; case SDL_SCANCODE_I: return 0x17;
    case SDL_SCANCODE_O: return 0x18; case SDL_SCANCODE_P: return 0x19;
    case SDL_SCANCODE_LEFTBRACKET: return 0x1A; case SDL_SCANCODE_RIGHTBRACKET: return 0x1B;
    case SDL_SCANCODE_RETURN: case SDL_SCANCODE_KP_ENTER: return 0x1C;
    case SDL_SCANCODE_LCTRL: case SDL_SCANCODE_RCTRL: return 0x1D;
    case SDL_SCANCODE_A: return 0x1E; case SDL_SCANCODE_S: return 0x1F;
    case SDL_SCANCODE_D: return 0x20; case SDL_SCANCODE_F: return 0x21;
    case SDL_SCANCODE_G: return 0x22; case SDL_SCANCODE_H: return 0x23;
    case SDL_SCANCODE_J: return 0x24; case SDL_SCANCODE_K: return 0x25;
    case SDL_SCANCODE_L: return 0x26; case SDL_SCANCODE_SEMICOLON: return 0x27;
    case SDL_SCANCODE_APOSTROPHE: return 0x28; case SDL_SCANCODE_GRAVE: return 0x29;
    case SDL_SCANCODE_LSHIFT: return 0x2A; case SDL_SCANCODE_BACKSLASH: return 0x2B;
    case SDL_SCANCODE_Z: return 0x2C; case SDL_SCANCODE_X: return 0x2D;
    case SDL_SCANCODE_C: return 0x2E; case SDL_SCANCODE_V: return 0x2F;
    case SDL_SCANCODE_B: return 0x30; case SDL_SCANCODE_N: return 0x31;
    case SDL_SCANCODE_M: return 0x32; case SDL_SCANCODE_COMMA: return 0x33;
    case SDL_SCANCODE_PERIOD: return 0x34; case SDL_SCANCODE_SLASH: return 0x35;
    case SDL_SCANCODE_RSHIFT: return 0x36; case SDL_SCANCODE_KP_MULTIPLY: return 0x37;
    case SDL_SCANCODE_LALT: case SDL_SCANCODE_RALT: return 0x38;
    case SDL_SCANCODE_SPACE: return 0x39; case SDL_SCANCODE_CAPSLOCK: return 0x3A;
    case SDL_SCANCODE_F1: return 0x3B; case SDL_SCANCODE_F2: return 0x3C;
    case SDL_SCANCODE_F3: return 0x3D; case SDL_SCANCODE_F4: return 0x3E;
    case SDL_SCANCODE_F5: return 0x3F; case SDL_SCANCODE_F6: return 0x40;
    case SDL_SCANCODE_F7: return 0x41; case SDL_SCANCODE_F8: return 0x42;
    case SDL_SCANCODE_F9: return 0x43; case SDL_SCANCODE_F10: return 0x44;
    case SDL_SCANCODE_HOME: return 0x47; case SDL_SCANCODE_UP: return 0x48;
    case SDL_SCANCODE_PAGEUP: return 0x49; case SDL_SCANCODE_LEFT: return 0x4B;
    case SDL_SCANCODE_RIGHT: return 0x4D; case SDL_SCANCODE_END: return 0x4F;
    case SDL_SCANCODE_DOWN: return 0x50; case SDL_SCANCODE_PAGEDOWN: return 0x51;
    case SDL_SCANCODE_INSERT: return 0x52; case SDL_SCANCODE_DELETE: return 0x53;
    case SDL_SCANCODE_F11: return 0x57; case SDL_SCANCODE_F12: return 0x58;
    default: return 0;
    }
}

static int asciiFor(SDL_Keycode k, SDL_Keymod m)
{
    if (k == SDLK_RETURN || k == SDLK_KP_ENTER) return '\r';
    if (k == SDLK_BACKSPACE) return 8;
    if (k == SDLK_ESCAPE) return 27;
    if (k == SDLK_TAB) return 9;
    if (k >= 32 && k < 127) {
        int c = (int)k;
        if (m & KMOD_SHIFT) {
            if (c >= 'a' && c <= 'z') return c - 32;
            const char *a = "1234567890-=[]\\;',./`", *b = "!@#$%^&*()_+{}|:\"<>?~";
            const char *p = strchr(a, c);
            if (p) return b[p - a];
        } else if ((m & KMOD_CAPS) && c >= 'a' && c <= 'z') {
            return c - 32;
        }
        return c;
    }
    return 0;
}

static void winToGame(int wx, int wy, int *gx, int *gy)
{
    /* SDL's renderer already maps mouse events into the 640x480 logical
     * space (SDL_RenderSetLogicalSize), so only clamp here */
    int x = wx, y = wy;
    if (x < 0) x = 0;
    if (x > 639) x = 639;
    if (y < 0) y = 0;
    if (y > 479) y = 479;
    *gx = x; *gy = y;
}

static void toggleFullscreen(void)
{
    uint32_t f = SDL_GetWindowFlags(win);
    SDL_SetWindowFullscreen(win, (f & SDL_WINDOW_FULLSCREEN_DESKTOP) ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP);
    dirty = 1;
}

void sys_pump(void)
{
    uint32_t now = SDL_GetTicks();
    /* the original game busy-waits on the timer; don't burn a whole core */
    if (now == lastPump) SDL_Delay(1);
    lastPump = SDL_GetTicks();

    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
        switch (ev.type) {
        case SDL_QUIT:
            sys_quit_now();
            break;
        case SDL_WINDOWEVENT:
            dirty = 1;
            break;
        case SDL_MOUSEMOTION:
            winToGame(ev.motion.x, ev.motion.y, &mouseX, &mouseY);
            push(EVT_MOUSEMOVE, 0);
            break;
        case SDL_MOUSEBUTTONDOWN:
            winToGame(ev.button.x, ev.button.y, &mouseX, &mouseY);
            if (ev.button.button == SDL_BUTTON_LEFT) { buttons |= EVT_LEFTBUT; push(EVT_MOUSEDOWN, EVT_LEFTBMASK); }
            else if (ev.button.button == SDL_BUTTON_RIGHT) { buttons |= EVT_RIGHTBUT; push(EVT_MOUSEDOWN, EVT_RIGHTBMASK); }
            break;
        case SDL_MOUSEBUTTONUP:
            winToGame(ev.button.x, ev.button.y, &mouseX, &mouseY);
            if (ev.button.button == SDL_BUTTON_LEFT) { buttons &= ~EVT_LEFTBUT; push(EVT_MOUSEUP, EVT_LEFTBMASK); }
            else if (ev.button.button == SDL_BUTTON_RIGHT) { buttons &= ~EVT_RIGHTBUT; push(EVT_MOUSEUP, EVT_RIGHTBMASK); }
            break;
        case SDL_KEYDOWN: {
            SDL_Keymod m = (SDL_Keymod)ev.key.keysym.mod;
            if (ev.key.keysym.sym == SDLK_RETURN && (m & KMOD_ALT)) { if (!ev.key.repeat) toggleFullscreen(); break; }
            if (ev.key.keysym.sym == SDLK_F11 && !(m & KMOD_CTRL)) { if (!ev.key.repeat) toggleFullscreen(); break; }
            int sc = pcScan(ev.key.keysym.scancode);
            int a = asciiFor(ev.key.keysym.sym, m);
            push(ev.key.repeat ? EVT_KEYREPEAT : EVT_KEYDOWN, (uint32_t)(a & 0xFF) | ((uint32_t)sc << 8));
            break;
        }
        case SDL_KEYUP: {
            int sc = pcScan(ev.key.keysym.scancode);
            int a = asciiFor(ev.key.keysym.sym, (SDL_Keymod)ev.key.keysym.mod);
            push(EVT_KEYUP, (uint32_t)(a & 0xFF) | ((uint32_t)sc << 8));
            break;
        }
        default:
            break;
        }
    }
    if (got_signal) sys_quit_now();
    if (sys_idle_hook) sys_idle_hook();
    sys_present(0);
}

int EVT_getNext(event_t *evt, uint32_t mask)
{
    sys_pump();
    evt->what = EVT_NULLEVT;
    for (int i = qhead; i != qtail; i = (i + 1) % QSIZE) {
        if (queue[i].what & mask) {
            *evt = queue[i];
            /* remove entry i, shifting the rest down */
            for (int j = i; j != qtail; j = (j + 1) % QSIZE) {
                int n = (j + 1) % QSIZE;
                if (n == qtail) break;
                queue[j] = queue[n];
            }
            qtail = (qtail + QSIZE - 1) % QSIZE;
            return 1;
        }
    }
    evt->where_x = mouseX;
    evt->where_y = mouseY;
    evt->modifiers = modState();
    evt->message = 0;
    return 0;
}

int EVT_peekNext(event_t *evt, uint32_t mask)
{
    sys_pump();
    evt->what = EVT_NULLEVT;
    for (int i = qhead; i != qtail; i = (i + 1) % QSIZE)
        if (queue[i].what & mask) { *evt = queue[i]; return 1; }
    return 0;
}

void EVT_flush(uint32_t mask)
{
    event_t e;
    while (EVT_getNext(&e, mask)) {}
}

void EVT_halt(event_t *evt, uint32_t mask)
{
    do {
        EVT_getNext(evt, EVT_EVERYEVT);
    } while (!(evt->what & mask));
}

void MS_moveTo(int x, int y)
{
    mouseX = x; mouseY = y;
    int ww, wh;
    SDL_GetWindowSize(win, &ww, &wh);
    /* map logical -> window coordinates through the 4:3 letterbox */
    float scale = (float)ww / 640.0f;
    if ((float)wh / 480.0f < scale) scale = (float)wh / 480.0f;
    int ox = (int)((ww - 640 * scale) / 2), oy = (int)((wh - 480 * scale) / 2);
    if (SDL_GetWindowFlags(win) & SDL_WINDOW_INPUT_FOCUS)
        SDL_WarpMouseInWindow(win, ox + (int)(x * scale), oy + (int)(y * scale));
}

/* --- display ----------------------------------------------------------- */
void sys_mark_dirty(void) { dirty = 1; }

void sys_present(int force)
{
    uint32_t now = SDL_GetTicks();
    if (!sys_display) return;
    if (!force && (!dirty || now - lastPresent < 16)) return;   /* ~60 fps */
    uint32_t lut[256];
    for (int i = 0; i < 256; i++) {
        palette_t p = sys_display->pal[i];
        lut[i] = 0xFF000000u | ((uint32_t)p.red << 16) | ((uint32_t)p.green << 8) | p.blue;
    }
    const uint8_t *s = sys_display->surface;
    for (int i = 0; i < 640 * 480; i++) rgbaBuf[i] = lut[s[i]];
    SDL_UpdateTexture(tex, NULL, rgbaBuf, 640 * 4);
    SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
    SDL_RenderClear(ren);
    SDL_RenderCopy(ren, tex, NULL, NULL);
    SDL_RenderPresent(ren);
    dirty = 0;
    lastPresent = now;
}

uint32_t sys_ms(void) { return SDL_GetTicks(); }

/* PORT: the original's main loop spun flat out.  Give each pass at least
 * a few milliseconds so the game doesn't keep a CPU core at 100%.  All game
 * timing is clock based, so this doesn't change how it plays. */
void sys_throttle(uint32_t ms)
{
    static uint32_t last;
    uint32_t now = SDL_GetTicks();
    if (now - last < ms) SDL_Delay(ms - (now - last));
    last = SDL_GetTicks();
}

/* debugging aid: SAFARI_WATCHDOG=n aborts (with a backtrace under a
 * sanitizer build) if the event pump hasn't run for n seconds */
static int watchdog(void *arg)
{
    uint32_t limit = (uint32_t)(uintptr_t)arg * 1000;
    for (;;) {
        SDL_Delay(500);
        if (lastPump && SDL_GetTicks() - lastPump > limit) {
            fprintf(stderr, "WATCHDOG: event pump stalled for %u ms\n", SDL_GetTicks() - lastPump);
            abort();
        }
    }
    return 0;
}

int sys_init(void)
{
    /* let the desktop match the window to browning-safari.desktop */
    setenv("SDL_VIDEO_X11_WMCLASS", "browning-safari", 0);
    setenv("SDL_VIDEO_WAYLAND_WMCLASS", "browning-safari", 0);
    setenv("SDL_APP_ID", "browning-safari", 0);
    /* handle SIGTERM/SIGINT ourselves (see on_signal) */
    SDL_SetHint(SDL_HINT_NO_SIGNAL_HANDLERS, "1");
    struct sigaction sa;
    memset(&sa, 0, sizeof sa);
    sa.sa_handler = on_signal;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGHUP, &sa, NULL);
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 0;
    }
    /* no sound card is not fatal: the game simply runs silent */
    if (!getenv("SAFARI_NOSOUND") && SDL_InitSubSystem(SDL_INIT_AUDIO) != 0)
        SDL_Log("no audio: %s", SDL_GetError());
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
    SDL_SetHint(SDL_HINT_VIDEO_MINIMIZE_ON_FOCUS_LOSS, "0");
    uint32_t flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI;
    const char *fs = SDL_getenv("SAFARI_FULLSCREEN");
    if (fs && *fs == '1') flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
    win = SDL_CreateWindow("The Browning African Safari", SDL_WINDOWPOS_CENTERED,
                           SDL_WINDOWPOS_CENTERED, 1280, 960, flags);
    if (!win) { SDL_Log("window: %s", SDL_GetError()); return 0; }
    ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!ren) ren = SDL_CreateRenderer(win, -1, 0);
    if (!ren) { SDL_Log("renderer: %s", SDL_GetError()); return 0; }
    SDL_RenderSetLogicalSize(ren, 640, 480);
    tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, 640, 480);
    if (!tex) { SDL_Log("texture: %s", SDL_GetError()); return 0; }
    SDL_ShowCursor(SDL_DISABLE);   /* the game draws its own cursor */
    sys_display = MGL_createDisplayDC();
    const char *wd = getenv("SAFARI_WATCHDOG");
    if (wd && atoi(wd) > 0) SDL_DetachThread(SDL_CreateThread(watchdog, "watchdog", (void *)(uintptr_t)atoi(wd)));
    return sys_display != NULL;
}

void sys_shutdown(void)
{
    if (tex) SDL_DestroyTexture(tex);
    if (ren) SDL_DestroyRenderer(ren);
    if (win) SDL_DestroyWindow(win);
    SDL_Quit();
}
