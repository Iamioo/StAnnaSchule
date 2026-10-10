/* St. Anna Schule - Android port.
 *
 * A bare NativeActivity: no Java code, software rendering straight into the
 * ANativeWindow buffer, raw touch/key events. The handful of NDK functions
 * used are declared below, so this builds with a freestanding ARM GCC and
 * the SDK's linker (see android/build_android.ps1) - no NDK required.
 *
 * Touch layout: d-pad on the left, HANDY (phone), AKTION (hide / talk /
 * use) and RENNEN (run) on the right, tap the picture for "Enter".
 * Back = Esc. Gamepads and keyboards work too. Sound goes through AAudio,
 * opened with dlopen so the app still starts on Android < 8 (silently).
 */
#include <stddef.h>
#include <stdint.h>
#include "game.h"

/* ---- NDK declarations (from native_activity.h, native_window.h, input.h) ---- */
typedef struct ANativeWindow ANativeWindow;
typedef struct AInputQueue AInputQueue;
typedef struct AInputEvent AInputEvent;
typedef struct ARect { int32_t left, top, right, bottom; } ARect;
typedef struct ANativeWindow_Buffer {
  int32_t width, height, stride, format;
  void *bits;
  uint32_t reserved[6];
} ANativeWindow_Buffer;
typedef struct ANativeActivity ANativeActivity;
typedef struct ANativeActivityCallbacks {
  void (*onStart)(ANativeActivity *);
  void (*onResume)(ANativeActivity *);
  void *(*onSaveInstanceState)(ANativeActivity *, size_t *);
  void (*onPause)(ANativeActivity *);
  void (*onStop)(ANativeActivity *);
  void (*onDestroy)(ANativeActivity *);
  void (*onWindowFocusChanged)(ANativeActivity *, int);
  void (*onNativeWindowCreated)(ANativeActivity *, ANativeWindow *);
  void (*onNativeWindowResized)(ANativeActivity *, ANativeWindow *);
  void (*onNativeWindowRedrawNeeded)(ANativeActivity *, ANativeWindow *);
  void (*onNativeWindowDestroyed)(ANativeActivity *, ANativeWindow *);
  void (*onInputQueueCreated)(ANativeActivity *, AInputQueue *);
  void (*onInputQueueDestroyed)(ANativeActivity *, AInputQueue *);
  void (*onContentRectChanged)(ANativeActivity *, const ARect *);
  void (*onConfigurationChanged)(ANativeActivity *);
  void (*onLowMemory)(ANativeActivity *);
} ANativeActivityCallbacks;
struct ANativeActivity {
  ANativeActivityCallbacks *callbacks;
  void *vm, *env, *clazz;
  const char *internalDataPath, *externalDataPath;
  int32_t sdkVersion;
  void *instance, *assetManager;
  const char *obbPath;
};

#define WINDOW_FORMAT_RGBX_8888 2
#define AWINDOW_FLAG_KEEP_SCREEN_ON 0x80
#define AWINDOW_FLAG_FULLSCREEN 0x400
#define AINPUT_EVENT_TYPE_KEY 1
#define AINPUT_EVENT_TYPE_MOTION 2
#define AMOTION_EVENT_ACTION_UP 1
#define AMOTION_EVENT_ACTION_CANCEL 3
#define AMOTION_EVENT_ACTION_POINTER_UP 6

int32_t ANativeWindow_setBuffersGeometry(ANativeWindow *, int32_t, int32_t, int32_t);
int32_t ANativeWindow_lock(ANativeWindow *, ANativeWindow_Buffer *, ARect *);
int32_t ANativeWindow_unlockAndPost(ANativeWindow *);
int32_t ANativeWindow_getWidth(ANativeWindow *);
int32_t ANativeWindow_getHeight(ANativeWindow *);
int32_t AInputQueue_getEvent(AInputQueue *, AInputEvent **);
int32_t AInputQueue_preDispatchEvent(AInputQueue *, AInputEvent *);
void AInputQueue_finishEvent(AInputQueue *, AInputEvent *, int);
int32_t AInputEvent_getType(const AInputEvent *);
int32_t AKeyEvent_getAction(const AInputEvent *);
int32_t AKeyEvent_getKeyCode(const AInputEvent *);
int32_t AMotionEvent_getAction(const AInputEvent *);
size_t AMotionEvent_getPointerCount(const AInputEvent *);
float AMotionEvent_getX(const AInputEvent *, size_t);
float AMotionEvent_getY(const AInputEvent *, size_t);
void ANativeActivity_finish(ANativeActivity *);
void ANativeActivity_setWindowFlags(ANativeActivity *, uint32_t, uint32_t);
int __android_log_print(int prio, const char *tag, const char *fmt, ...);

struct timespec_ { long tv_sec, tv_nsec; };
int clock_gettime(int clk, struct timespec_ *ts);
int pthread_create(long *thread, const void *attr, void *(*fn)(void *), void *arg);
int pthread_join(long thread, void **ret);
int usleep(unsigned us);
void *dlopen(const char *name, int flags);
void *dlsym(void *handle, const char *name);

/* ---- AAudio (aaudio/AAudio.h), looked up at run time ---- */
typedef struct AAudioStreamStruct AAudioStream;
typedef struct AAudioStreamBuilderStruct AAudioStreamBuilder;
typedef int32_t (*aaudio_cb)(AAudioStream *, void *, void *, int32_t);
static int32_t (*pCreateBuilder)(AAudioStreamBuilder **);
static void (*pSetFormat)(AAudioStreamBuilder *, int32_t);
static void (*pSetChannels)(AAudioStreamBuilder *, int32_t);
static void (*pSetRate)(AAudioStreamBuilder *, int32_t);
static void (*pSetCallback)(AAudioStreamBuilder *, aaudio_cb, void *);
static int32_t (*pOpen)(AAudioStreamBuilder *, AAudioStream **);
static int32_t (*pDeleteBuilder)(AAudioStreamBuilder *);
static int32_t (*pStart)(AAudioStream *);
static int32_t (*pPause)(AAudioStream *);
static int32_t (*pGetRate)(AAudioStream *);

#define LOG(...) __android_log_print(4, "StAnna", __VA_ARGS__)
#define EXPORT __attribute__((visibility("default")))

U0 SaPlatSound(I64 id) { (void)id; }

/* ---- shared state between the UI thread callbacks and the game thread ---- */
static ANativeActivity *g_act;
static ANativeWindow *volatile g_new_win;
static AInputQueue *volatile g_new_queue;
static volatile int g_win_gone, g_queue_gone, g_running, g_paused, g_inited;
static long g_thread;

/* ---- game thread ---- */
#define MAXW 2048
#define MAXH 1100
static U8 canvas[MAXW * MAXH];
static uint32_t pal32[16];
static ANativeWindow *win;
static AInputQueue *queue;
static int win_w, win_h, bw, bh, scale, gx, gy;
static int pad_x, pad_y, pad_r, btn_x, act_y, run_y, btn_r;
/* latch remembers presses shorter than one tick (quick taps) */
static int key_keys, touch_keys, latch;
static int ph_y;
static AAudioStream *g_stream;
static int g_audio_paused;

static int ms_now(void)
{
  struct timespec_ ts;
  clock_gettime(1 /* CLOCK_MONOTONIC */, &ts);
  return (int)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

static int iabs(int v) { return v < 0 ? -v : v; }

static void layout(void)
{
  int d, side;
  win_w = ANativeWindow_getWidth(win);
  win_h = ANativeWindow_getHeight(win);
  d = win_h / 400;
  if (d < 1)
    d = 1;
  bw = win_w / d;
  bh = win_h / d;
  while (bw > MAXW || bh > MAXH) {
    d++;
    bw = win_w / d;
    bh = win_h / d;
  }
  ANativeWindow_setBuffersGeometry(win, bw, bh, WINDOW_FORMAT_RGBX_8888);
  scale = bh / SCR_H;
  if (bw / SCR_W < scale)
    scale = bw / SCR_W;
  if (scale < 1)
    scale = 1;
  gx = (bw - SCR_W * scale) / 2;
  gy = (bh - SCR_H * scale) / 2;
  side = gx;
  pad_r = bh / 5;
  if (side >= pad_r * 2 + 16)
    pad_x = side / 2;
  else
    pad_x = pad_r + 10;
  pad_y = bh - pad_r - bh / 8;
  btn_r = pad_r * 6 / 10;
  btn_x = bw - pad_x;
  act_y = bh / 2 - btn_r / 2;
  run_y = bh - btn_r - bh / 8;
  ph_y = act_y - (run_y - act_y);
  if (ph_y < btn_r + 2)
    ph_y = btn_r + 2;
  LOG("window %dx%d buffer %dx%d scale %d", win_w, win_h, bw, bh, scale);
}

static void disc(int cx, int cy, int r, int ring, int c)
{
  int x, y, d2, r2 = r * r, i2 = (r - ring) * (r - ring);
  for (y = -r; y <= r; y++)
    for (x = -r; x <= r; x++) {
      d2 = x * x + y * y;
      if (d2 <= r2 && d2 >= i2 && cx + x >= 0 && cx + x < bw && cy + y >= 0 && cy + y < bh)
        canvas[(cy + y) * bw + cx + x] = (U8)c;
    }
}

static void label(int cx, int cy, const char *s, int c)
{
  int sc = btn_r >= 40 ? 2 : 1;
  SaDrawText(canvas, bw, bh, cx - SaTextW((CH *)s, sc) / 2, cy - 3 * sc, (CH *)s, c, sc);
}

static void draw_controls(void)
{
  int k = touch_keys | key_keys, a = pad_r / 3, w = pad_r / 5;
  disc(pad_x, pad_y, pad_r, 3, 3);
  disc(pad_x, pad_y, pad_r / 4, pad_r / 4, 2);
  SaFillRect(canvas, bw, bh, pad_x - w / 2, pad_y - pad_r + 10, w, a, k & K_UP ? 6 : 4);
  SaFillRect(canvas, bw, bh, pad_x - w / 2, pad_y + pad_r - 10 - a, w, a, k & K_DOWN ? 6 : 4);
  SaFillRect(canvas, bw, bh, pad_x - pad_r + 10, pad_y - w / 2, a, w, k & K_LEFT ? 6 : 4);
  SaFillRect(canvas, bw, bh, pad_x + pad_r - 10 - a, pad_y - w / 2, a, w, k & K_RIGHT ? 6 : 4);
  disc(btn_x, ph_y, btn_r, k & K_PHONE ? btn_r : 3, k & K_PHONE ? 4 : 5);
  label(btn_x, ph_y, "HANDY", k & K_PHONE ? 1 : 5);
  disc(btn_x, act_y, btn_r, k & K_ACT ? btn_r : 3, k & K_ACT ? 13 : 14);
  label(btn_x, act_y, "AKTION", k & K_ACT ? 6 : 14);
  disc(btn_x, run_y, btn_r, k & K_RUN ? btn_r : 3, k & K_RUN ? 11 : 12);
  label(btn_x, run_y, "RENNEN", k & K_RUN ? 6 : 12);
}

static void render(void)
{
  ANativeWindow_Buffer buf;
  int x, y, sx, sy;
  uint32_t *dst;
  U8 *src;
  for (y = 0; y < bh * bw; y++)
    canvas[y] = 0;
  for (y = 0; y < SCR_H * scale; y++) {
    src = sa_fb + (y / scale) * SCR_W;
    for (x = 0; x < SCR_W * scale; x++)
      canvas[(gy + y) * bw + gx + x] = src[x / scale];
  }
  draw_controls();
  if (ANativeWindow_lock(win, &buf, 0) < 0)
    return;
  sx = buf.width < bw ? buf.width : bw;
  sy = buf.height < bh ? buf.height : bh;
  for (y = 0; y < sy; y++) {
    dst = (uint32_t *)buf.bits + y * buf.stride;
    src = canvas + y * bw;
    for (x = 0; x < sx; x++)
      dst[x] = pal32[src[x]];
  }
  ANativeWindow_unlockAndPost(win);
}

static int key_bit(int code)
{
  switch (code) {
  case 19: case 51: return K_UP;          /* DPAD_UP, W */
  case 20: case 47: return K_DOWN;        /* DPAD_DOWN, S */
  case 21: case 29: return K_LEFT;        /* DPAD_LEFT, A */
  case 22: case 32: return K_RIGHT;       /* DPAD_RIGHT, D */
  case 33: case 62: case 96: return K_ACT;           /* E, SPACE, BUTTON_A */
  case 45: case 34: case 100: return K_PHONE;        /* Q, F, BUTTON_Y */
  case 59: case 60: case 97: case 99: return K_RUN;  /* SHIFT, BUTTON_B/X */
  case 23: case 66: case 108: return K_START;        /* DPAD_CENTER, ENTER, START */
  case 4: case 111: return K_ESC;                    /* BACK, ESCAPE */
  }
  return 0;
}

static int touch_bits(int x, int y)
{
  int dx, dy, ax, ay, dead;
  if (x >= gx && x < gx + SCR_W * scale && y >= gy && y < gy + SCR_H * scale)
    return K_START;
  if (x < bw / 2) {
    dx = x - pad_x;
    dy = y - pad_y;
    ax = iabs(dx);
    ay = iabs(dy);
    dead = pad_r / 4;
    if (ax > pad_r * 2 || ay > pad_r * 2)
      return 0;
    return (ax > dead && ax * 2 > ay ? (dx < 0 ? K_LEFT : K_RIGHT) : 0) |
           (ay > dead && ay * 2 > ax ? (dy < 0 ? K_UP : K_DOWN) : 0);
  }
  dx = x - btn_x;
  dy = y - act_y;
  if (dx * dx + dy * dy < btn_r * btn_r * 2)
    return K_ACT;
  dy = y - run_y;
  if (dx * dx + dy * dy < btn_r * btn_r * 2)
    return K_RUN;
  dy = y - ph_y;
  if (dx * dx + dy * dy < btn_r * btn_r * 2)
    return K_PHONE;
  return 0;
}

/* ---- sound ---- */
static int32_t audio_cb(AAudioStream *st, void *user, void *data, int32_t frames)
{
  (void)st;
  (void)user;
  SaAudioRender((I16 *)data, frames);
  return 0; /* AAUDIO_CALLBACK_RESULT_CONTINUE */
}

static void audio_init(void)
{
  void *lib = dlopen("libaaudio.so", 2 /* RTLD_NOW */);
  AAudioStreamBuilder *b;
  if (!lib)
    return;
  pCreateBuilder = (int32_t (*)(AAudioStreamBuilder **))dlsym(lib, "AAudio_createStreamBuilder");
  pSetFormat = (void (*)(AAudioStreamBuilder *, int32_t))dlsym(lib, "AAudioStreamBuilder_setFormat");
  pSetChannels = (void (*)(AAudioStreamBuilder *, int32_t))dlsym(lib, "AAudioStreamBuilder_setChannelCount");
  pSetRate = (void (*)(AAudioStreamBuilder *, int32_t))dlsym(lib, "AAudioStreamBuilder_setSampleRate");
  pSetCallback = (void (*)(AAudioStreamBuilder *, aaudio_cb, void *))dlsym(lib, "AAudioStreamBuilder_setDataCallback");
  pOpen = (int32_t (*)(AAudioStreamBuilder *, AAudioStream **))dlsym(lib, "AAudioStreamBuilder_openStream");
  pDeleteBuilder = (int32_t (*)(AAudioStreamBuilder *))dlsym(lib, "AAudioStreamBuilder_delete");
  pStart = (int32_t (*)(AAudioStream *))dlsym(lib, "AAudioStream_requestStart");
  pPause = (int32_t (*)(AAudioStream *))dlsym(lib, "AAudioStream_requestPause");
  pGetRate = (int32_t (*)(AAudioStream *))dlsym(lib, "AAudioStream_getSampleRate");
  if (!pCreateBuilder || !pSetFormat || !pSetChannels || !pSetRate || !pSetCallback || !pOpen ||
      !pDeleteBuilder || !pStart || !pPause || !pGetRate)
    return;
  if (pCreateBuilder(&b) != 0)
    return;
  pSetFormat(b, 1); /* AAUDIO_FORMAT_PCM_I16 */
  pSetChannels(b, 1);
  pSetRate(b, 22050);
  pSetCallback(b, audio_cb, 0);
  if (pOpen(b, &g_stream) != 0)
    g_stream = 0;
  pDeleteBuilder(b);
  if (!g_stream)
    return;
  /* the device may pick another rate, the synth follows it */
  SaAudioSetRate(pGetRate(g_stream));
  pStart(g_stream);
  LOG("AAudio: %d Hz", pGetRate(g_stream));
}

static void audio_pause(int pause)
{
  if (!g_stream || pause == g_audio_paused)
    return;
  g_audio_paused = pause;
  if (pause)
    pPause(g_stream);
  else
    pStart(g_stream);
}

static int handle_event(AInputEvent *ev)
{
  int type = AInputEvent_getType(ev), action, idx, i, n, up, k;
  if (type == AINPUT_EVENT_TYPE_KEY) {
    k = key_bit(AKeyEvent_getKeyCode(ev));
    if (!k)
      return 0;
    if (AKeyEvent_getAction(ev) == 0) {
      key_keys |= k;
      latch |= k;
    }
    else if (AKeyEvent_getAction(ev) == 1)
      key_keys &= ~k;
    return 1;
  }
  if (type == AINPUT_EVENT_TYPE_MOTION && win_w > 0) {
    /* every motion event carries all pointers: rebuild the touch state */
    action = AMotionEvent_getAction(ev);
    idx = (action >> 8) & 0xFF;
    action &= 0xFF;
    n = (int)AMotionEvent_getPointerCount(ev);
    touch_keys = 0;
    if (action != AMOTION_EVENT_ACTION_UP && action != AMOTION_EVENT_ACTION_CANCEL)
      for (i = 0; i < n; i++) {
        up = action == AMOTION_EVENT_ACTION_POINTER_UP && i == idx;
        if (!up)
          touch_keys |= touch_bits((int)AMotionEvent_getX(ev, i) * bw / win_w,
                                   (int)AMotionEvent_getY(ev, i) * bh / win_h);
      }
    latch |= touch_keys;
    return 1;
  }
  return 0;
}

static void poll_input(void)
{
  AInputEvent *ev;
  while (AInputQueue_getEvent(queue, &ev) >= 0) {
    if (AInputQueue_preDispatchEvent(queue, ev))
      continue;
    AInputQueue_finishEvent(queue, ev, handle_event(ev));
  }
}

static void *game_thread(void *arg)
{
  int i, ticks = 0, t0 = ms_now(), now, steps;
  (void)arg;
  if (!g_inited) {
    SaGameInit((U32)ms_now());
    sa_touch_ui = 1;
    g_inited = 1;
    audio_init();
  }
  for (i = 0; i < 16; i++)
    pal32[i] = 0xFF000000u | sa_pal[i * 3] | (sa_pal[i * 3 + 1] << 8) | ((uint32_t)sa_pal[i * 3 + 2] << 16);
  while (g_running) {
    __sync_synchronize();
    if (g_win_gone) {
      win = 0;
      g_win_gone = 0;
    }
    if (g_new_win) {
      win = g_new_win;
      g_new_win = 0;
      layout();
    }
    if (g_queue_gone) {
      queue = 0;
      g_queue_gone = 0;
    }
    if (g_new_queue) {
      queue = g_new_queue;
      g_new_queue = 0;
    }
    __sync_synchronize();
    if (queue)
      poll_input();
    now = ms_now();
    audio_pause(!win || g_paused);
    if (!win || g_paused) {
      t0 = now;
      ticks = 0;
      usleep(20000);
      continue;
    }
    if (now - (t0 + ticks * 1000 / TICKS_PER_SEC) >= 0) {
      steps = 0;
      while (now - (t0 + ticks * 1000 / TICKS_PER_SEC) >= 0 && steps < 4) {
        i = key_keys | touch_keys | latch;
        latch = 0;
        if (!SaGameTick(i)) {
          ANativeActivity_finish(g_act);
          SaGameInit((U32)now); /* fresh title screen if we come back */
          sa_touch_ui = 1;
        }
        ticks++;
        steps++;
      }
      if (steps == 4) {
        t0 = now;
        ticks = 0;
      }
      SaGameDraw();
      render();
    } else
      usleep(2000);
  }
  return 0;
}

/* ---- UI thread callbacks ---- */
static void wait_for(volatile int *flag)
{
  int i;
  for (i = 0; i < 500 && *flag && g_running; i++)
    usleep(1000);
}

static void on_win_created(ANativeActivity *a, ANativeWindow *w)
{
  (void)a;
  g_new_win = w;
  __sync_synchronize();
}

static void on_win_destroyed(ANativeActivity *a, ANativeWindow *w)
{
  (void)a;
  if (g_new_win == w)
    g_new_win = 0;
  g_win_gone = 1;
  __sync_synchronize();
  wait_for(&g_win_gone);
}

static void on_queue_created(ANativeActivity *a, AInputQueue *q)
{
  (void)a;
  g_new_queue = q;
  __sync_synchronize();
}

static void on_queue_destroyed(ANativeActivity *a, AInputQueue *q)
{
  (void)a;
  (void)q;
  if (g_new_queue == q)
    g_new_queue = 0;
  g_queue_gone = 1;
  __sync_synchronize();
  wait_for(&g_queue_gone);
}

static void on_pause(ANativeActivity *a) { (void)a; g_paused = 1; }
static void on_resume(ANativeActivity *a) { (void)a; g_paused = 0; }

static void on_destroy(ANativeActivity *a)
{
  (void)a;
  g_running = 0;
  __sync_synchronize();
  pthread_join(g_thread, 0);
}

EXPORT void ANativeActivity_onCreate(ANativeActivity *a, void *saved, size_t saved_size)
{
  (void)saved;
  (void)saved_size;
  g_act = a;
  a->callbacks->onNativeWindowCreated = on_win_created;
  a->callbacks->onNativeWindowDestroyed = on_win_destroyed;
  a->callbacks->onInputQueueCreated = on_queue_created;
  a->callbacks->onInputQueueDestroyed = on_queue_destroyed;
  a->callbacks->onPause = on_pause;
  a->callbacks->onResume = on_resume;
  a->callbacks->onDestroy = on_destroy;
  ANativeActivity_setWindowFlags(a, AWINDOW_FLAG_KEEP_SCREEN_ON | AWINDOW_FLAG_FULLSCREEN, 0);
  g_new_win = 0;
  g_new_queue = 0;
  g_win_gone = g_queue_gone = g_paused = 0;
  g_running = 1;
  __sync_synchronize();
  pthread_create(&g_thread, 0, game_thread, 0);
  LOG("St. Anna Schule gestartet (SDK %d)", a->sdkVersion);
}
