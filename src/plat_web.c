/* St. Anna Schule - browser port (WebAssembly).
 *
 * Built with plain clang, no Emscripten and no C library:
 *   clang --target=wasm32 -O2 -nostdlib -Wl,--no-entry -o stanna.wasm \
 *         src/game.c src/plat_web.c
 * web/stanna.js does the rest: canvas, keyboard, touch, gamepad, WebAudio.
 */
#include "game.h"

#define EXPORT(name) __attribute__((export_name(name)))

extern I64 sa_state, sa_end_ticks, sa_deaths, sa_have[], sa_save_ok, sa_save_seq;
extern U8 sa_save[];

static unsigned int rgba[SCR_W * SCR_H];
static unsigned int pal32[16];
static I16 audio[8192];

U0 SaPlatSound(I64 id) { (void)id; }

/* the compiler may call these for loops over arrays */
void *memset(void *d, int v, unsigned long n)
{
  unsigned char *p = (unsigned char *)d;
  while (n--)
    *p++ = (unsigned char)v;
  return d;
}

void *memcpy(void *d, const void *s, unsigned long n)
{
  unsigned char *p = (unsigned char *)d;
  const unsigned char *q = (const unsigned char *)s;
  while (n--)
    *p++ = *q++;
  return d;
}

EXPORT("init") void web_init(unsigned int seed)
{
  int i;
  for (i = 0; i < 16; i++)
    pal32[i] = 0xFF000000u | sa_pal[i * 3] | (sa_pal[i * 3 + 1] << 8) | ((unsigned int)sa_pal[i * 3 + 2] << 16);
  SaGameInit(seed);
}

EXPORT("touch_ui") void web_touch_ui(int t) { sa_touch_ui = t; }
EXPORT("tick") int web_tick(int keys) { return SaGameTick(keys); }

/* draws a frame and returns the RGBA pixels (320x200) */
EXPORT("draw") unsigned int *web_draw(void)
{
  int i;
  SaGameDraw();
  for (i = 0; i < SCR_W * SCR_H; i++)
    rgba[i] = pal32[sa_fb[i]];
  return rgba;
}

EXPORT("audio_rate") void web_audio_rate(int rate) { SaAudioSetRate(rate); }

/* renders n (at most 8192) mono samples and returns them */
EXPORT("audio") I16 *web_audio(int n)
{
  if (n > 8192)
    n = 8192;
  SaAudioRender(audio, n);
  return audio;
}

/* for the high score: 5 = escaped */
EXPORT("state") int web_state(void) { return sa_state; }
EXPORT("end_ticks") int web_end_ticks(void) { return sa_end_ticks; }
EXPORT("deaths") int web_deaths(void) { return sa_deaths; }
EXPORT("stickers") int web_stickers(void) { return sa_have[4]; }

/* the saved game: the page keeps these bytes in localStorage */
EXPORT("save_ptr") U8 *web_save_ptr(void) { return sa_save; }
EXPORT("save_len") int web_save_len(void) { return 4096; }
EXPORT("save_seq") int web_save_seq(void) { return sa_save_seq; }
EXPORT("save_set") void web_save_set(int ok) { sa_save_ok = ok; }
