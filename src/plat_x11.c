/* St. Anna Schule - Linux/Unix port (plain Xlib).
 *
 * Works with any X server back to the 1990s: 8-bit palette, 15/16-bit and
 * 24/32-bit visuals. Also picks up a joystick on /dev/input/js0.
 * Sound is piped into "aplay" (ALSA utils, on nearly every distribution);
 * without it the game simply stays silent.
 *
 *   cc -O2 -o stanna src/game.c src/plat_x11.c -lX11
 */
#define _DEFAULT_SOURCE
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/XKBlib.h>
#include <X11/keysym.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>
#include "game.h"

U0 SaPlatSound(I64 id) { (void)id; }

static Display *dpy;
static Window win;
static GC gc;
static Visual *vis;
static int depth, win_w, win_h, keys, latch, running = 1, joy_fd = -1, joy_keys;
static XImage *img;
static char *img_mem;
static unsigned long pix[16];

/* ---- sound through a pipe to aplay ---- */
#define AUD_RATE 22050
static FILE *snd;
static int snd_fd = -1;
static long snd_t0, snd_written;
static I16 snd_buf[2048];

static long now_ms(void)
{
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return tv.tv_sec * 1000L + tv.tv_usec / 1000;
}

static void audio_init(void)
{
  signal(SIGPIPE, SIG_IGN);
  snd = popen("exec aplay -q -t raw -f S16_LE -r 22050 -c 1 --buffer-time=120000 - 2>/dev/null", "w");
  if (!snd)
    return;
  snd_fd = fileno(snd);
  fcntl(snd_fd, F_SETFL, fcntl(snd_fd, F_GETFL) | O_NONBLOCK);
  SaAudioSetRate(AUD_RATE);
  snd_t0 = now_ms();
  snd_written = 0;
}

/* keeps about 0.1 s of sound queued; chunks of 4 KB are written atomically */
static void audio_pump(long now)
{
  long want, n;
  int k;
  if (snd_fd < 0)
    return;
  want = (now - snd_t0) * AUD_RATE / 1000 + AUD_RATE / 10;
  n = want - snd_written;
  if (n > AUD_RATE / 2) {
    snd_written = want - AUD_RATE / 10;
    n = AUD_RATE / 10;
  }
  while (n > 0) {
    k = n > 2048 ? 2048 : (int)n;
    SaAudioRender(snd_buf, k);
    if (write(snd_fd, snd_buf, (size_t)k * 2) < 0) {
      if (errno != EAGAIN) {
        pclose(snd);
        snd = NULL;
        snd_fd = -1;
      }
      return;
    }
    snd_written += k;
    n -= k;
  }
}

static int mask_shift(unsigned long m, int *bits)
{
  int s = 0, b = 0;
  if (!m)
    return *bits = 0, 0;
  while (!(m & 1)) m >>= 1, s++;
  while (m & 1) m >>= 1, b++;
  *bits = b;
  return s;
}

static void init_colors(void)
{
  int i, rs, gs, bs, rb, gb, bb;
  if (vis->class == TrueColor || vis->class == DirectColor) {
    rs = mask_shift(vis->red_mask, &rb);
    gs = mask_shift(vis->green_mask, &gb);
    bs = mask_shift(vis->blue_mask, &bb);
    for (i = 0; i < 16; i++)
      pix[i] = ((unsigned long)(sa_pal[i * 3] >> (8 - rb)) << rs) |
               ((unsigned long)(sa_pal[i * 3 + 1] >> (8 - gb)) << gs) |
               ((unsigned long)(sa_pal[i * 3 + 2] >> (8 - bb)) << bs);
  } else {
    Colormap cm = DefaultColormap(dpy, DefaultScreen(dpy));
    for (i = 0; i < 16; i++) {
      XColor c;
      c.red = sa_pal[i * 3] * 257;
      c.green = sa_pal[i * 3 + 1] * 257;
      c.blue = sa_pal[i * 3 + 2] * 257;
      c.flags = DoRed | DoGreen | DoBlue;
      pix[i] = XAllocColor(dpy, cm, &c) ? c.pixel : (i ? WhitePixel(dpy, 0) : BlackPixel(dpy, 0));
    }
  }
}

static void make_image(void)
{
  int bpp_bytes;
  if (img) {
    img->data = NULL;
    XDestroyImage(img);
    free(img_mem);
  }
  img = XCreateImage(dpy, vis, depth, ZPixmap, 0, NULL, win_w, win_h, 32, 0);
  bpp_bytes = (img->bits_per_pixel + 7) / 8;
  img_mem = calloc(1, (size_t)img->bytes_per_line * win_h + bpp_bytes);
  img->data = img_mem;
}

static void present(void)
{
  int s, gw, gh, ox, oy, x, y;
  s = win_w / SCR_W < win_h / SCR_H ? win_w / SCR_W : win_h / SCR_H;
  if (s < 1)
    s = 1;
  gw = SCR_W * s;
  gh = SCR_H * s;
  ox = (win_w - gw) / 2;
  oy = (win_h - gh) / 2;
  if (gw > win_w) gw = win_w, ox = 0;
  if (gh > win_h) gh = win_h, oy = 0;
  for (y = 0; y < gh; y++) {
    const U8 *src = sa_fb + (y / s) * SCR_W;
    char *row = img->data + (size_t)y * img->bytes_per_line;
    switch (img->bits_per_pixel) {
    case 32: {
      unsigned int *d = (unsigned int *)row;
      for (x = 0; x < gw; x++)
        d[x] = (unsigned int)pix[src[x / s]];
      break;
    }
    case 16: {
      unsigned short *d = (unsigned short *)row;
      for (x = 0; x < gw; x++)
        d[x] = (unsigned short)pix[src[x / s]];
      break;
    }
    case 8:
      for (x = 0; x < gw; x++)
        row[x] = (char)pix[src[x / s]];
      break;
    default:
      for (x = 0; x < gw; x++)
        XPutPixel(img, x, y, pix[src[x / s]]);
    }
  }
  XSetForeground(dpy, gc, BlackPixel(dpy, DefaultScreen(dpy)));
  if (oy > 0) {
    XFillRectangle(dpy, win, gc, 0, 0, win_w, oy);
    XFillRectangle(dpy, win, gc, 0, oy + gh, win_w, win_h - oy - gh);
  }
  if (ox > 0) {
    XFillRectangle(dpy, win, gc, 0, 0, ox, win_h);
    XFillRectangle(dpy, win, gc, ox + gw, 0, win_w - ox - gw, win_h);
  }
  XPutImage(dpy, win, gc, img, 0, 0, ox, oy, gw, gh);
  XFlush(dpy);
}

static int key_bit(KeySym k)
{
  switch (k) {
  case XK_Up: case XK_w: case XK_KP_Up: return K_UP;
  case XK_Down: case XK_s: case XK_KP_Down: return K_DOWN;
  case XK_Left: case XK_a: case XK_KP_Left: return K_LEFT;
  case XK_Right: case XK_d: case XK_KP_Right: return K_RIGHT;
  case XK_e: case XK_space: return K_ACT;
  case XK_q: case XK_f: return K_PHONE;
  case XK_Shift_L: case XK_Shift_R: return K_RUN;
  case XK_Return: case XK_KP_Enter: return K_START;
  case XK_Escape: return K_ESC;
  }
  return 0;
}

/* Linux joystick API: struct js_event { u32 time; s16 value; u8 type, number; } */
static void poll_joystick(void)
{
  unsigned char e[8];
  short v;
  int bit;
  if (joy_fd < 0)
    return;
  while (read(joy_fd, e, 8) == 8) {
    v = (short)(e[4] | (e[5] << 8));
    if ((e[6] & 0x7F) == 2) { /* axis */
      if (e[7] == 0)
        joy_keys = (joy_keys & ~(K_LEFT | K_RIGHT)) | (v < -12000 ? K_LEFT : v > 12000 ? K_RIGHT : 0);
      if (e[7] == 1)
        joy_keys = (joy_keys & ~(K_UP | K_DOWN)) | (v < -12000 ? K_UP : v > 12000 ? K_DOWN : 0);
    } else if ((e[6] & 0x7F) == 1) { /* button */
      /* A, B/X, Y, Back, Start on an Xbox style pad */
      bit = e[7] == 0 ? K_ACT : e[7] <= 2 ? K_RUN : e[7] == 3 ? K_PHONE : e[7] == 6 ? K_ESC :
            e[7] >= 7 ? K_START : 0;
      joy_keys = v ? joy_keys | bit : joy_keys & ~bit;
    }
  }
}

int main(void)
{
  XSetWindowAttributes wa;
  XSizeHints hints;
  Atom wm_delete;
  XEvent ev;
  long t0, now, ticks = 0;
  int scr, s = 3, steps;

  dpy = XOpenDisplay(NULL);
  if (!dpy) {
    fprintf(stderr, "stanna: kann den X-Server nicht öffnen (DISPLAY gesetzt?)\n");
    return 1;
  }
  scr = DefaultScreen(dpy);
  vis = DefaultVisual(dpy, scr);
  depth = DefaultDepth(dpy, scr);
  while (s > 1 && (SCR_W * s > DisplayWidth(dpy, scr) - 40 || SCR_H * s > DisplayHeight(dpy, scr) - 80))
    s--;
  win_w = SCR_W * s;
  win_h = SCR_H * s;

  SaGameInit((U32)now_ms());
  init_colors();

  wa.background_pixel = BlackPixel(dpy, scr);
  wa.event_mask = KeyPressMask | KeyReleaseMask | ExposureMask | StructureNotifyMask | FocusChangeMask;
  win = XCreateWindow(dpy, RootWindow(dpy, scr), 0, 0, win_w, win_h, 0, depth, InputOutput, vis,
                      CWBackPixel | CWEventMask, &wa);
  XStoreName(dpy, win, "St. Anna Schule - Herr Th\xFCner kommt");
  hints.flags = PMinSize;
  hints.min_width = SCR_W;
  hints.min_height = SCR_H;
  XSetWMNormalHints(dpy, win, &hints);
  wm_delete = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
  XSetWMProtocols(dpy, win, &wm_delete, 1);
  XkbSetDetectableAutoRepeat(dpy, True, NULL);
  gc = XCreateGC(dpy, win, 0, NULL);
  XMapWindow(dpy, win);
  make_image();
  joy_fd = open("/dev/input/js0", O_RDONLY | O_NONBLOCK);
  audio_init();

  t0 = now_ms();
  while (running) {
    while (XPending(dpy)) {
      XNextEvent(dpy, &ev);
      switch (ev.type) {
      case KeyPress:
        keys |= key_bit(XLookupKeysym(&ev.xkey, 0));
        latch |= key_bit(XLookupKeysym(&ev.xkey, 0)); /* presses shorter than a tick */
        break;
      case KeyRelease:
        keys &= ~key_bit(XLookupKeysym(&ev.xkey, 0));
        break;
      case FocusOut:
        keys = 0;
        break;
      case ConfigureNotify:
        if (ev.xconfigure.width != win_w || ev.xconfigure.height != win_h) {
          win_w = ev.xconfigure.width;
          win_h = ev.xconfigure.height;
          make_image();
        }
        break;
      case ClientMessage:
        if ((Atom)ev.xclient.data.l[0] == wm_delete)
          running = 0;
        break;
      }
    }
    poll_joystick();
    now = now_ms();
    if (now - (t0 + ticks * 1000 / TICKS_PER_SEC) >= 0) {
      steps = 0;
      while (now - (t0 + ticks * 1000 / TICKS_PER_SEC) >= 0 && steps < 4) {
        if (!SaGameTick(keys | joy_keys | latch))
          running = 0;
        latch = 0;
        ticks++;
        steps++;
      }
      if (steps == 4) {
        t0 = now;
        ticks = 0;
      }
      SaGameDraw();
      present();
      audio_pump(now);
    } else
      usleep(2000);
  }
  if (snd)
    pclose(snd);
  XCloseDisplay(dpy);
  return 0;
}
