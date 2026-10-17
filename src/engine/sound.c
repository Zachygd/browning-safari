/*
 * sound.c - Sound.obj ported from Miles Sound System (MSS32) to SDL2 audio.
 *
 * The game keeps whole .WAV images in memory (sndptr[]) and plays them on
 * up to 16 MSS "sample handles".  This file contains a tiny MSS-like mixer
 * (voices with volume, playback rate, loop count, status and an
 * end-of-sample callback) and the original game-facing Sound* API on top.
 */
#include <SDL.h>
#include <string.h>
#include <stdlib.h>
#include "safari.h"

/* ------------------------------------------------------------ mini MSS -- */
enum { SMP_FREE = 1, SMP_DONE = 2, SMP_PLAYING = 4, SMP_STOPPED = 8 };
#define NUM_SAMPLES 16
#define OUT_RATE 44100

typedef struct SAMPLE {
    int           status;
    const uint8_t *data;    /* PCM data */
    const uint8_t *owner;   /* sndptr buffer the data lives in */
    uint32_t      len;      /* bytes */
    int           bits;     /* 8 or 16 */
    int           rate;
    double        pos;      /* in frames */
    int           volume;   /* 0..127 */
    int           loops;    /* 0 = forever, n = play n times */
    int           eos_pending;
    int         (*eos)(struct SAMPLE *);
} SAMPLE;

static SDL_AudioDeviceID dev;
static SAMPLE samples[NUM_SAMPLES];
static int masterVolume = 127;

static void lock(void)   { if (dev) SDL_LockAudioDevice(dev); }
static void unlock(void) { if (dev) SDL_UnlockAudioDevice(dev); }

/* silence any voice still playing from a buffer that is about to be freed */
static void release_owner(const uint8_t *buf)
{
    if (!buf) return;
    lock();
    for (int i = 0; i < NUM_SAMPLES; i++)
        if (samples[i].owner == buf) {
            samples[i].status = SMP_DONE;
            samples[i].data = NULL;
            samples[i].owner = NULL;
            samples[i].eos_pending = 0;
        }
    unlock();
}

static void mix(void *ud, Uint8 *stream, int len)
{
    (void)ud;
    int16_t *out = (int16_t *)stream;
    int frames = len / 4;
    static int32_t acc[4096];
    if (frames > 4096) frames = 4096;
    memset(acc, 0, sizeof(int32_t) * frames);
    for (int v = 0; v < NUM_SAMPLES; v++) {
        SAMPLE *s = &samples[v];
        if (s->status != SMP_PLAYING || !s->data) continue;
        int bps = s->bits / 8;
        uint32_t total = s->len / bps;
        if (total == 0) { s->status = SMP_DONE; s->eos_pending = 1; continue; }
        double step = (double)s->rate / OUT_RATE;
        int32_t vol = s->volume * masterVolume;   /* 0..16129 */
        for (int i = 0; i < frames; i++) {
            uint32_t ip = (uint32_t)s->pos;
            if (ip >= total) {
                if (s->loops == 0 || --s->loops > 0) { s->pos -= total; ip = (uint32_t)s->pos; if (ip >= total) ip = 0, s->pos = 0; }
                else { s->status = SMP_DONE; s->eos_pending = 1; break; }
            }
            int32_t smp;
            if (bps == 1) smp = ((int32_t)s->data[ip] - 128) * 256;
            else smp = (int16_t)(s->data[ip * 2] | (s->data[ip * 2 + 1] << 8));
            acc[i] += (smp * vol) / 16129;
            s->pos += step;
        }
    }
    for (int i = 0; i < frames; i++) {
        int32_t v = acc[i];
        if (v > 32767) v = 32767;
        if (v < -32768) v = -32768;
        out[i * 2] = out[i * 2 + 1] = (int16_t)v;
    }
}

/* parse a RIFF/WAVE image into a sample */
static int set_sample_file(SAMPLE *s, const uint8_t *img, uint32_t size)
{
    if (!img || size < 12 || memcmp(img, "RIFF", 4) || memcmp(img + 8, "WAVE", 4)) return 0;
    uint32_t riffLen = img[4] | img[5] << 8 | img[6] << 16 | (uint32_t)img[7] << 24;
    uint32_t o = 12, end = riffLen + 8;
    if (end > size || end < riffLen) end = size;   /* never read past the buffer */
    s->data = NULL;
    s->rate = 0;
    s->bits = 0;
    while (o + 8 <= end) {
        uint32_t clen = img[o + 4] | img[o + 5] << 8 | img[o + 6] << 16 | (uint32_t)img[o + 7] << 24;
        if (!memcmp(img + o, "fmt ", 4) && o + 24 <= end) {
            s->rate = img[o + 12] | img[o + 13] << 8 | img[o + 14] << 16 | (uint32_t)img[o + 15] << 24;
            s->bits = img[o + 22] | img[o + 23] << 8;
        } else if (!memcmp(img + o, "data", 4)) {
            s->data = img + o + 8;
            s->len = clen <= end - (o + 8) ? clen : end - (o + 8);
        }
        if (clen > end) break;
        o += 8 + clen + (clen & 1);
    }
    if (s->bits != 8 && s->bits != 16) s->bits = 8;
    if (s->rate < 1000 || s->rate > 192000) s->rate = 22050;   /* a 0 rate would never finish */
    return s->data != NULL;
}

/* run end-of-sample callbacks on the main thread */
static void service_eos(void)
{
    for (int i = 0; i < NUM_SAMPLES; i++) {
        lock();
        int p = samples[i].eos_pending;
        int (*cb)(SAMPLE *) = samples[i].eos;
        samples[i].eos_pending = 0;
        unlock();
        if (p && cb) cb(&samples[i]);
    }
}
extern void (*sys_idle_hook)(void);

/* ------------------------------------------------------ game sound API -- */
int  bSoundOkay;
int  music_playing;
int  sound_volume, music_volume;
int  sound_Music = 16, sound_Voice = 17, num_Sound_Effects = 39;
static SAMPLE  *smp[NUM_SAMPLES];
static int      num_voices;
static uint8_t *sndptr[64];
static uint32_t sndsize[64];
static int      smp_handles[64];

static int SoundShutoff(SAMPLE *s);
/* sound number valid and sound running */
#define SND_OK(n) (bSoundOkay && (n) >= 0 && (n) < 64)
static int MusicShutoffCB(SAMPLE *s) { (void)s; return MusicShutoff(); }

int SoundInit(void)
{
    for (int i = 0; i < 64; i++) smp_handles[i] = -1;
    SDL_AudioSpec want, have;
    SDL_zero(want);
    want.freq = OUT_RATE;
    want.format = AUDIO_S16SYS;
    want.channels = 2;
    want.samples = 1024;
    want.callback = mix;
    if (!getenv("SAFARI_NOSOUND"))
        dev = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    if (!dev) {
        bSoundOkay = 0;
        Log("ERROR: wave_init_driver failed");
        return 0;
    }
    for (int i = 0; i < NUM_SAMPLES; i++) {
        samples[i].status = SMP_DONE;
        samples[i].volume = 127;
        smp[i] = &samples[i];
    }
    num_voices = NUM_SAMPLES;
    sys_idle_hook = service_eos;
    SDL_PauseAudioDevice(dev, 0);
    bSoundOkay = 1;
    return 1;
}

void SoundExit(void)
{
    if (bSoundOkay) {
        lock();
        for (int i = 0; i < num_voices; i++) smp[i]->status = SMP_DONE;
        unlock();
        for (int i = 0; i < num_Sound_Effects; i++)
            if (sndptr[i]) { free(sndptr[i]); sndptr[i] = NULL; }
    }
    if (dev) SDL_CloseAudioDevice(dev);
    dev = 0;
}

static void soundPath(char *out, size_t n, const char *file)
{
    if (init_8bitsound == 0) snprintf(out, n, "%s%s.16b", cdPath, file);
    else snprintf(out, n, "%s%s.8b", cdPath, file);
}

uint32_t SoundSeek(const char *file, int seek, int sndnum)
{
    uint32_t seektable[128];
    short numentries = 0;
    uint8_t pad[4];
    uint32_t filesize;
    if (debug) Log("SoundSeek(%s, %d, %d)", file, seek, sndnum);
    if (!bSoundOkay) return 0;
    if (*file == 0) return 0;
    if (num_Sound_Effects < sndnum || sndnum < 0) { Log("ERROR: SoundSeek -- too many sounds"); return 0; }
    soundPath(filename, sizeof filename, file);
    FHANDLE fh = FileOpen(filename);
    if (fh == BAD_HANDLE) { Log("unable to open %s", filename); return 0; }
    if (sndptr[sndnum]) { release_owner(sndptr[sndnum]); free(sndptr[sndnum]); sndptr[sndnum] = NULL; }
    FileRead(fh, &numentries, 2);
    FileRead(fh, pad, 2);
    FileRead(fh, seektable, 0x200);
    if (numentries - 1 < seek) {
        FileClose(fh);
        Log("ERROR: SoundSeek: file overrun %s, %d", file, seek);
        return 0;
    }
    if (seek == 0) {
        filesize = seektable[0];
    } else {
        filesize = seektable[seek] - seektable[seek - 1];
        FileSeek(fh, (long)seektable[seek - 1], 1);
    }
    sndptr[sndnum] = calloc(1, filesize ? filesize : 1);
    sndsize[sndnum] = filesize;
    if (!sndptr[sndnum]) {
        Log("ERROR: Out of memory: SoundSeek");
        FileClose(fh);
        return 0;
    }
    FileRead(fh, sndptr[sndnum], filesize);
    FileClose(fh);
    if (debug) Log("SoundSeek: filesize %d", filesize);
    return filesize;
}

int SoundLoad(const char *file, int sndnum)
{
    if (debug) Log("SoundLoad(%s, %d)", file, sndnum);
    if (!bSoundOkay) return 0;
    if (num_Sound_Effects < sndnum || sndnum < 0) { Log("ERROR: SoundLoad -- too many sounds"); return 0; }
    soundPath(filename, sizeof filename, file);
    FHANDLE fh = FileOpen(filename);
    if (fh == BAD_HANDLE) { Log("SoundLoad: unable to load %s", filename); return 0; }
    if (sndptr[sndnum]) { release_owner(sndptr[sndnum]); free(sndptr[sndnum]); sndptr[sndnum] = NULL; }
    uint32_t size = FileSize(fh);
    sndptr[sndnum] = calloc(1, size ? size : 1);
    sndsize[sndnum] = size;
    if (!sndptr[sndnum]) {
        Log("ERROR: Out of memory: SoundLoad");
        FileClose(fh);
        return 0;
    }
    FileRead(fh, sndptr[sndnum], size);
    FileClose(fh);
    return 1;
}

static void start_on(int i, int sndnum, int loopForever)
{
    lock();
    SAMPLE *s = smp[i];
    s->status = SMP_DONE;
    s->pos = 0;
    s->volume = 127;
    s->loops = 1;
    s->eos = NULL;
    s->eos_pending = 0;
    s->owner = sndptr[sndnum];
    if (set_sample_file(s, sndptr[sndnum], sndsize[sndnum])) {
        if (loopForever) s->loops = 0;
        s->status = SMP_PLAYING;
        s->eos = SoundShutoff;
    }
    unlock();
    /* PORT: forget any other sound still pointing at this voice, so that
     * polling or stopping a finished sound can't touch the new one (the
     * original could, e.g. wait forever on somebody else's looping sound) */
    for (int j = 0; j < 64; j++)
        if (j != sndnum && smp_handles[j] == i) smp_handles[j] = -1;
    smp_handles[sndnum] = i;
}

void SoundLoop(int sndnum)
{
    if (debug) Log("SoundLoop(%d)", sndnum);
    if (!SND_OK(sndnum) || !sndptr[sndnum]) return;
    int i;
    for (i = 0; i < num_voices && smp[i]->status != SMP_DONE; i++) {}
    if (i != num_voices) start_on(i, sndnum, 1);
    else smp_handles[sndnum] = -1;
}

/* EOS callback: the sound that was playing on this voice has finished.
 * (PORT FIX: the original cleared smp_handles[voice] instead of
 * smp_handles[sound], leaving stale handles behind.) */
static int SoundShutoff(SAMPLE *s)
{
    for (int i = 0; i < num_voices; i++) {
        if (smp[i] == s && s->status != SMP_PLAYING) {
            for (int j = 0; j < 64; j++)
                if (smp_handles[j] == i) smp_handles[j] = -1;
            return 0;
        }
    }
    return 0;
}

void SoundPlay(int sndnum)
{
    if (debug) Log("SoundPlay(%d)", sndnum);
    if (!SND_OK(sndnum) || !sndptr[sndnum]) return;
    int i;
    for (i = 0; i < num_voices && smp[i]->status == SMP_PLAYING; i++) {}
    if (i == num_voices) { Log("no slot available for %d", sndnum); smp_handles[sndnum] = -1; return; }
    start_on(i, sndnum, 0);
    SoundSetVolume(sndnum, sound_volume);
}

void SoundStop(int sndnum)
{
    if (debug) Log("SoundStop(%d)", sndnum);
    if (SND_OK(sndnum) && smp_handles[sndnum] != -1) {
        lock();
        SAMPLE *s = smp[smp_handles[sndnum]];
        s->status = SMP_DONE;
        s->eos_pending = 0;
        s->data = NULL;
        s->owner = NULL;
        unlock();
        smp_handles[sndnum] = -1;
    }
}

void SoundFree(int sndnum)
{
    if (debug) Log("SoundFree(%d)", sndnum);
    if (SND_OK(sndnum) && sndptr[sndnum]) {
        if (smp_handles[sndnum] != -1) SoundStop(sndnum);
        if (smp_handles[sndnum] == -1 && sndptr[sndnum]) {
            release_owner(sndptr[sndnum]);
            free(sndptr[sndnum]);
            sndptr[sndnum] = NULL;
        }
    }
}

void SoundStopAll(void)
{
    if (bSoundOkay)
        for (int i = 0; i < num_Sound_Effects; i++) SoundStop(i);
}

void SoundWait(int sndnum)
{
    /* PORT FIX: GetLapCount() runs the end-of-sound callbacks, which can
     * clear this sound's handle while we wait. Re-read it every time (the
     * old loop then indexed voice -1 and crashed, e.g. in the trophy fee
     * tally at the airport). */
    if (!SND_OK(sndnum)) return;
    int h;
    while ((h = smp_handles[sndnum]) != -1 && smp[h]->status == SMP_PLAYING) GetLapCount();
    smp_handles[sndnum] = -1;
}

int SoundPoll(int sndnum)
{
    sys_pump();
    if (!SND_OK(sndnum) || smp_handles[sndnum] == -1) return 0;
    return smp[smp_handles[sndnum]]->status == SMP_PLAYING;
}

int SoundPosition(int sndhandle)
{
    SAMPLE *s = smp[sndhandle];
    return (int)(s->pos * (s->bits / 8));
}

int SoundGetVolume(int sndnum)
{
    if (!SND_OK(sndnum) || smp_handles[sndnum] == -1) return 0;
    return (int)(smp[smp_handles[sndnum]]->volume / 1.27);
}

void SoundSetVolume(int sndnum, int volume)
{
    if (SND_OK(sndnum) && smp_handles[sndnum] != -1) {
        int v = (int)(volume * 1.27);
        if (v < 0) v = 0;
        if (v > 127) v = 127;
        lock();
        smp[smp_handles[sndnum]]->volume = v;
        unlock();
    }
}

int SoundGetMasterVolume(void)
{
    if (!bSoundOkay) return 0;
    return (int)(masterVolume / 1.27);
}

void SoundSetMasterVolume(int volume)
{
    if (bSoundOkay) {
        int v = (int)(volume * 1.27);
        if (v < 0) v = 0;
        if (v > 127) v = 127;
        lock();
        masterVolume = v;
        unlock();
    }
}

int SoundGetFrequency(int sndnum)
{
    if (!SND_OK(sndnum) || smp_handles[sndnum] == -1) return 0;
    return smp[smp_handles[sndnum]]->rate;
}

void SoundSetFrequency(int sndnum, int pitch)
{
    if (SND_OK(sndnum) && smp_handles[sndnum] != -1) {
        lock();
        smp[smp_handles[sndnum]]->rate = pitch;
        unlock();
    }
}

void SoundFade(int sndnum)
{
    short volume = 100;
    if (bSoundOkay) {
        do {
            TimeDelay(0);
            volume -= 2;
            SoundSetVolume(sndnum, volume);
            TimeDelay(5);
        } while (volume > 0);
    }
}

void SoundPause(int sndnum)
{
    if (SND_OK(sndnum) && smp_handles[sndnum] != -1) {
        lock();
        SAMPLE *s = smp[smp_handles[sndnum]];
        if (s->status == SMP_PLAYING) s->status = SMP_STOPPED;
        unlock();
    }
}

void SoundResume(int sndnum)
{
    if (SND_OK(sndnum) && smp_handles[sndnum] != -1) {
        lock();
        SAMPLE *s = smp[smp_handles[sndnum]];
        if (s->status == SMP_STOPPED) s->status = SMP_PLAYING;
        unlock();
    }
}

void StopMusic(void)
{
    if (debug) Log("StopMusic");
    if (music_playing) MusicShutoff();
}

int MusicShutoff(void)
{
    if (music_playing) SoundStop(sound_Music);
    SoundFree(sound_Music);
    music_playing = 0;
    return 0;
}

static void registerMusicEOS(void)
{
    if (smp_handles[sound_Music] != -1) {
        lock();
        smp[smp_handles[sound_Music]]->eos = MusicShutoffCB;
        unlock();
    }
}

void StartMusic(const char *file)
{
    if (music_playing == 0) {
        if (debug) Log("music_playing = %d", 0);
        SoundLoad(file, sound_Music);
        SoundPlay(sound_Music);
        SoundSetVolume(sound_Music, music_volume);
        registerMusicEOS();
        music_playing = 1;
    }
}

void StartMusicLoop(const char *file)
{
    if (music_playing == 0) {
        SoundLoad(file, sound_Music);
        SoundLoop(sound_Music);
        SoundSetVolume(sound_Music, music_volume);
        registerMusicEOS();
        music_playing = 1;
    }
}

void WaitMusic(void)
{
    /* a looping track never ends, so don't wait for one */
    while (SoundPoll(sound_Music) && smp[smp_handles[sound_Music]]->loops != 0) {}
}

void StartVoice(const char *file, int comment)
{
    if (SoundPoll(sound_Voice) == 0) {
        SoundStop(sound_Voice);
        SoundFree(sound_Voice);
        if (SoundSeek(file, comment, sound_Voice)) SoundPlay(sound_Voice);
    }
}

/* raw image of a loaded sound (used for the talking-head lip sync) */
const uint8_t *SoundData(int sndnum)
{
    if (sndnum < 0 || sndnum >= 64) return NULL;
    return sndptr[sndnum];
}
