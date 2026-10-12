/* St. Anna Schule - "Herr Thüner kommt"
 * Interface between the platform independent core (game.c) and the
 * platform layers (plat_win32.c, plat_x11.c, plat_android.c,
 * plat_switch.c, TempleOS).
 */
#ifndef STANNA_GAME_H
#define STANNA_GAME_H

/* Type names follow HolyC so game.c can be translated to TempleOS
 * mechanically. "I64" is only 32 bits in the C build, which is plenty
 * for every value the game uses and keeps 16/32-bit compilers happy. */
typedef unsigned char U8;
typedef unsigned short U16;
typedef unsigned int U32;
typedef short I16;
typedef int I64;
typedef void U0;
typedef char CH;

#define TRUE 1
#define FALSE 0

#define SCR_W 320
#define SCR_H 200
#define TICKS_PER_SEC 30

/* input bits passed to SaGameTick */
#define K_UP 1
#define K_DOWN 2
#define K_LEFT 4
#define K_RIGHT 8
#define K_ACT 16
#define K_RUN 32
#define K_START 64
#define K_ESC 128
#define K_PHONE 256

/* sound ids (SaPlatSound gets them too, for the TempleOS PC speaker) */
#define SND_HEART 1
#define SND_SPOT 2
#define SND_KEY 3
#define SND_SCARE 4
#define SND_WIN 5
#define SND_LOCKER 6
#define SND_LOCKED 7
#define SND_STEPS 8
#define SND_CALL 9
#define SND_FIGHT 10
#define SND_TALK 11
#define SND_TAKE 12
#define SND_STAIRS 13
#define SND_PICKUP 14
#define SND_PIANO 15
#define SND_UNLOCK 16
#define SND_POWER 17
#define SND_BOMB 18
#define SND_DRINK 19
#define SND_SLIP 20
#define SND_NIEHAUS 21
#define SND_ALARM 22
#define SND_BROKEN 23
#define SND_SHOUT 24

extern U8 sa_fb[SCR_W * SCR_H]; /* 320x200, one palette index per pixel */
extern U8 sa_pal[48];           /* 16 colours, RGB */
extern U8 sa_world[];           /* the current floor, 896x448 */
extern I64 sa_touch_ui;         /* platform sets 1 for touch hints */

U0 SaGameInit(U32 seed);
I64 SaGameTick(I64 keys);       /* one 1/30 s step, returns 0 to quit */
U0 SaGameDraw(void);

/* audio: the core synthesises music and effects itself; platforms with
 * a sound device pull mono 16-bit samples from it */
U0 SaAudioSetRate(I64 rate);
U0 SaAudioRender(I16 *out, I64 n);

/* screenshot helpers */
U0 SaDebugScene(I64 n);         /* jump to a fixed scene */
U0 SaDebugWorld(I64 floor);     /* render floor 0..5 into sa_world */

/* drawing helpers the platform layers may reuse */
U0 SaFillRect(U8 *dst, I64 dw, I64 dh, I64 x, I64 y, I64 w, I64 h, I64 c);
U0 SaDrawText(U8 *dst, I64 dw, I64 dh, I64 x, I64 y, CH *s, I64 c, I64 scale);
I64 SaTextW(CH *s, I64 scale);
U0 SaDrawFaceIcon(U8 *dst, I64 dw, I64 dh, I64 x, I64 y, I64 s);

/* provided by the platform layer (may do nothing) */
U0 SaPlatSound(I64 id);

#endif
