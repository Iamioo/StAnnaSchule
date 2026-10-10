/* Renders the game's music and sound effects into a WAV file and prints
 * levels, so the synthesiser can be checked without listening.
 *   audio_test out.wav */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "../src/game.h"

extern I64 sa_state, sa_timer;
U0 SaSound(I64 id);

U0 SaPlatSound(I64 id) { (void)id; }

#define RATE 22050
#define PER_TICK (RATE / TICKS_PER_SEC)
static I16 buf[RATE * 40];

static void put32(FILE *f, unsigned v) { fputc(v & 255, f); fputc((v >> 8) & 255, f); fputc((v >> 16) & 255, f); fputc(v >> 24, f); }
static void put16(FILE *f, unsigned v) { fputc(v & 255, f); fputc((v >> 8) & 255, f); }

int main(int argc, char **argv)
{
  int t, n = 0, i, peak = 0, clip = 0;
  double sum = 0;
  FILE *f;
  SaGameInit(1);
  SaAudioSetRate(RATE);
  /* 8 s title music, 8 s exploring, 8 s chase, then effects, 6 s win */
  for (t = 0; t < 30 * 36 && n + PER_TICK < (int)(sizeof(buf) / 2); t++) {
    int keys = 0;
    if (t == 240) keys = K_START;           /* title -> intro (explore music) */
    if (t == 480) keys = K_START;           /* intro -> play, Herr Thüner sits */
    if (t == 870) SaSound(SND_HEART);
    if (t == 900) SaSound(SND_SPOT);
    if (t == 930) SaSound(SND_KEY);
    if (t == 960) SaSound(SND_CALL);
    if (t == 1000) SaSound(SND_FIGHT);
    if (t == 1040) SaSound(SND_SCARE);
    SaGameTick(keys);
    SaAudioRender(buf + n, PER_TICK);
    n += PER_TICK;
  }
  for (i = 0; i < n; i++) {
    int v = buf[i] < 0 ? -buf[i] : buf[i];
    if (v > peak) peak = v;
    if (v >= 32000) clip++;
    sum += (double)buf[i] * buf[i];
  }
  printf("%d samples, peak %d, clipped %d, rms %.0f\n", n, peak, clip, n ? sqrt(sum / n) : 0);
  for (t = 0; t < n; t += RATE * 4) {
    double s = 0;
    for (i = t; i < t + RATE * 4 && i < n; i++) s += (double)buf[i] * buf[i];
    printf("  %2d-%2d s: rms %.0f\n", t / RATE, t / RATE + 4, sqrt(s / (RATE * 4)));
  }
  if (argc > 1 && (f = fopen(argv[1], "wb")) != NULL) {
    fwrite("RIFF", 1, 4, f); put32(f, 36 + n * 2); fwrite("WAVEfmt ", 1, 8, f);
    put32(f, 16); put16(f, 1); put16(f, 1); put32(f, RATE); put32(f, RATE * 2); put16(f, 2); put16(f, 16);
    fwrite("data", 1, 4, f); put32(f, n * 2); fwrite(buf, 2, n, f); fclose(f);
  }
  return 0;
}
