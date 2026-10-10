/* St. Anna Schule - Nintendo Switch port (homebrew, libnx).
 *
 * Software rendering into a 1280x720 framebuffer (the game at 3x in the
 * middle), Joy-Con / Pro Controller input, music through audout.
 * Start it from the Homebrew Menu (StAnna.nro).
 *
 * Controls: stick / d-pad walk, B or ZR run, A hide / talk, Y phone,
 * + start, - back to the menu (and out of the game on the title screen).
 */
#include <switch.h>
#include <malloc.h>
#include <string.h>
#include "game.h"

U0 SaPlatSound(I64 id) { (void)id; }

#define FB_W 1280
#define FB_H 720
#define SCALE 3
#define OX ((FB_W - SCR_W * SCALE) / 2)
#define OY ((FB_H - SCR_H * SCALE) / 2)

#define AUD_RATE 48000
#define AUD_FRAMES 1600
#define AUD_BUFS 4

static AudioOutBuffer aob[AUD_BUFS];
static I16 *abuf[AUD_BUFS];
static I16 mono[AUD_FRAMES];
static int audio_ok;
static u32 pal[16];

static void audio_fill(AudioOutBuffer *b)
{
  I16 *d = (I16 *)b->buffer;
  int i;
  SaAudioRender(mono, AUD_FRAMES);
  for (i = 0; i < AUD_FRAMES; i++) {
    d[i * 2] = mono[i];
    d[i * 2 + 1] = mono[i];
  }
}

static void audio_init(void)
{
  size_t size = AUD_FRAMES * 2 * sizeof(I16);
  size_t aligned = (size + 0xFFF) & ~(size_t)0xFFF;
  int i;
  if (R_FAILED(audoutInitialize()))
    return;
  if (R_FAILED(audoutStartAudioOut())) {
    audoutExit();
    return;
  }
  SaAudioSetRate(AUD_RATE);
  for (i = 0; i < AUD_BUFS; i++) {
    abuf[i] = (I16 *)memalign(0x1000, aligned);
    memset(abuf[i], 0, aligned);
    aob[i].next = NULL;
    aob[i].buffer = abuf[i];
    aob[i].buffer_size = aligned;
    aob[i].data_size = size;
    aob[i].data_offset = 0;
    audio_fill(&aob[i]);
    audoutAppendAudioOutBuffer(&aob[i]);
  }
  audio_ok = 1;
}

static void audio_pump(void)
{
  AudioOutBuffer *rel = NULL;
  u32 count = 0;
  int k;
  if (!audio_ok)
    return;
  for (k = 0; k < AUD_BUFS; k++) {
    if (R_FAILED(audoutGetReleasedAudioOutBuffer(&rel, &count)) || count == 0 || !rel)
      break;
    audio_fill(rel);
    audoutAppendAudioOutBuffer(rel);
  }
}

static int map_buttons(u64 b)
{
  int k = 0;
  if (b & (HidNpadButton_Up | HidNpadButton_StickLUp)) k |= K_UP;
  if (b & (HidNpadButton_Down | HidNpadButton_StickLDown)) k |= K_DOWN;
  if (b & (HidNpadButton_Left | HidNpadButton_StickLLeft)) k |= K_LEFT;
  if (b & (HidNpadButton_Right | HidNpadButton_StickLRight)) k |= K_RIGHT;
  if (b & HidNpadButton_A) k |= K_ACT;
  if (b & (HidNpadButton_B | HidNpadButton_ZR | HidNpadButton_R)) k |= K_RUN;
  if (b & (HidNpadButton_Y | HidNpadButton_X)) k |= K_PHONE;
  if (b & HidNpadButton_Plus) k |= K_START;
  if (b & HidNpadButton_Minus) k |= K_ESC;
  return k;
}

static void present(Framebuffer *fb)
{
  u32 stride, x, y, s, c;
  u32 *px = (u32 *)framebufferBegin(fb, &stride);
  u32 *row;
  stride /= sizeof(u32);
  for (y = 0; y < FB_H; y++) {
    row = px + y * stride;
    if (y < OY || y >= OY + SCR_H * SCALE) {
      memset(row, 0, FB_W * sizeof(u32));
      continue;
    }
    memset(row, 0, OX * sizeof(u32));
    memset(row + OX + SCR_W * SCALE, 0, (FB_W - OX - SCR_W * SCALE) * sizeof(u32));
    {
      const U8 *src = sa_fb + ((y - OY) / SCALE) * SCR_W;
      u32 *d = row + OX;
      for (x = 0; x < SCR_W; x++) {
        c = pal[src[x]];
        for (s = 0; s < SCALE; s++)
          *d++ = c;
      }
    }
  }
  framebufferEnd(fb);
}

int main(int argc, char **argv)
{
  Framebuffer fb;
  PadState pad;
  int i, frame = 0, keys, latch = 0;
  (void)argc;
  (void)argv;

  framebufferCreate(&fb, nwindowGetDefault(), FB_W, FB_H, PIXEL_FORMAT_RGBA_8888, 2);
  framebufferMakeLinear(&fb);
  padConfigureInput(1, HidNpadStyleSet_NpadStandard);
  padInitializeDefault(&pad);

  for (i = 0; i < 16; i++)
    pal[i] = 0xFF000000u | ((u32)sa_pal[i * 3 + 2] << 16) | ((u32)sa_pal[i * 3 + 1] << 8) |
             sa_pal[i * 3];
  SaGameInit((U32)armGetSystemTick());
  sa_touch_ui = 2;
  audio_init();

  /* the screen refreshes at 60 Hz, the game runs at 30 */
  while (appletMainLoop()) {
    padUpdate(&pad);
    keys = map_buttons(padGetButtons(&pad));
    latch |= (int)map_buttons(padGetButtonsDown(&pad));
    if ((frame & 1) == 0) {
      if (!SaGameTick(keys | latch))
        break;
      latch = 0;
      SaGameDraw();
    }
    present(&fb);
    audio_pump();
    frame++;
  }

  if (audio_ok) {
    audoutStopAudioOut();
    audoutExit();
  }
  framebufferClose(&fb);
  return 0;
}
