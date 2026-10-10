/* St. Anna Schule - Windows port.
 *
 * Plain Win32 + GDI + winmm and no C runtime at all, so the same 32-bit
 * exe only imports functions that already existed in Windows 95.
 *
 *   StAnna.exe                    play
 *   StAnna.exe -shot N file.bmp   render debug scene N to a BMP and exit
 *   StAnna.exe -icon S file.bmp   render the 32x32 face at scale S
 *   StAnna.exe -world F file.bmp  save the whole floor F (1-6) as a BMP
 *   StAnna.exe -autoplay file.bmp play a scripted run in the window,
 *                                 save the last frame and exit
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>
#include "game.h"

/* ---- the few CRT pieces the compiler may still ask for ---- */
#pragma function(memset, memcpy)
#pragma optimize("", off)
void *__cdecl memset(void *d, int c, size_t n)
{
  unsigned char *p = (unsigned char *)d;
  while (n--)
    *p++ = (unsigned char)c;
  return d;
}
void *__cdecl memcpy(void *d, const void *s, size_t n)
{
  unsigned char *p = (unsigned char *)d;
  const unsigned char *q = (const unsigned char *)s;
  while (n--)
    *p++ = *q++;
  return d;
}
#pragma optimize("", on)
int _fltused = 0;

static void put32(unsigned char *p, unsigned v)
{
  p[0] = (unsigned char)v;
  p[1] = (unsigned char)(v >> 8);
  p[2] = (unsigned char)(v >> 16);
  p[3] = (unsigned char)(v >> 24);
}

/* ---- sound: the core's synthesiser streamed through waveOut ---- */
#define AUD_RATE 22050
#define AUD_BUFS 4
#define AUD_LEN 768
static HWAVEOUT wo;
static WAVEHDR wh[AUD_BUFS];
static I16 abuf[AUD_BUFS][AUD_LEN];
static int audio_ok;

static void audio_init(void)
{
  WAVEFORMATEX wf;
  int i;
  memset(&wf, 0, sizeof(wf));
  wf.wFormatTag = WAVE_FORMAT_PCM;
  wf.nChannels = 1;
  wf.nSamplesPerSec = AUD_RATE;
  wf.wBitsPerSample = 16;
  wf.nBlockAlign = 2;
  wf.nAvgBytesPerSec = AUD_RATE * 2;
  if (waveOutOpen(&wo, WAVE_MAPPER, &wf, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR)
    return;
  SaAudioSetRate(AUD_RATE);
  for (i = 0; i < AUD_BUFS; i++) {
    memset(&wh[i], 0, sizeof(WAVEHDR));
    wh[i].lpData = (LPSTR)abuf[i];
    wh[i].dwBufferLength = AUD_LEN * 2;
    waveOutPrepareHeader(wo, &wh[i], sizeof(WAVEHDR));
    SaAudioRender(abuf[i], AUD_LEN);
    waveOutWrite(wo, &wh[i], sizeof(WAVEHDR));
  }
  audio_ok = 1;
}

/* refill every buffer the driver has finished with */
static void audio_pump(void)
{
  int i;
  if (!audio_ok)
    return;
  for (i = 0; i < AUD_BUFS; i++)
    if (wh[i].dwFlags & WHDR_DONE) {
      SaAudioRender(abuf[i], AUD_LEN);
      waveOutWrite(wo, &wh[i], sizeof(WAVEHDR));
    }
}

static void audio_close(void)
{
  if (audio_ok) {
    waveOutReset(wo);
    waveOutClose(wo);
  }
}

U0 SaPlatSound(I64 id)
{
  (void)id;
}

/* ---- BMP output (screenshots, icons) ---- */
static unsigned char bmp_row[4096];

static int save_bmp(const char *path, const U8 *pix, int w, int h, int scale)
{
  unsigned char hdr[54 + 64];
  int ow = w * scale, oh = h * scale, stride = (ow + 3) & ~3, x, y, i;
  DWORD written;
  HANDLE f = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
  if (f == INVALID_HANDLE_VALUE)
    return 0;
  memset(hdr, 0, sizeof(hdr));
  hdr[0] = 'B';
  hdr[1] = 'M';
  put32(hdr + 2, sizeof(hdr) + stride * oh);
  put32(hdr + 10, sizeof(hdr));
  put32(hdr + 14, 40);
  put32(hdr + 18, ow);
  put32(hdr + 22, oh);
  hdr[26] = 1;
  hdr[28] = 8;
  put32(hdr + 46, 16);
  for (i = 0; i < 16; i++) {
    hdr[54 + i * 4] = sa_pal[i * 3 + 2];
    hdr[55 + i * 4] = sa_pal[i * 3 + 1];
    hdr[56 + i * 4] = sa_pal[i * 3];
  }
  WriteFile(f, hdr, sizeof(hdr), &written, NULL);
  for (y = oh - 1; y >= 0; y--) {
    memset(bmp_row, 0, stride);
    for (x = 0; x < ow; x++)
      bmp_row[x] = pix[(y / scale) * w + x / scale];
    WriteFile(f, bmp_row, stride, &written, NULL);
  }
  CloseHandle(f);
  return 1;
}

/* ---- command line ---- */
static char *next_arg(char **pp)
{
  char *p = *pp, *start;
  while (*p == ' ' || *p == '\t')
    p++;
  if (!*p)
    return NULL;
  if (*p == '"') {
    start = ++p;
    while (*p && *p != '"')
      p++;
  } else {
    start = p;
    while (*p && *p != ' ' && *p != '\t')
      p++;
  }
  if (*p)
    *p++ = 0;
  *pp = p;
  return start;
}

static int str_eq(const char *a, const char *b)
{
  while (*a && *a == *b)
    a++, b++;
  return *a == *b;
}

static int to_int(const char *s)
{
  int v = 0;
  while (s && *s >= '0' && *s <= '9')
    v = v * 10 + (*s++ - '0');
  return v;
}

/* ---- window ---- */
static struct {
  BITMAPINFOHEADER h;
  RGBQUAD pal[256];
} bmi;
static U8 flipped[SCR_W * SCR_H];
static HWND hwnd;
static int keys, latch, running = 1, shot_no; /* latch: presses shorter than a tick */
static const char *autoplay_out;

static void present(HDC dc)
{
  RECT rc;
  int cw, ch, w, h, x, y;
  for (y = 0; y < SCR_H; y++)
    memcpy(flipped + (SCR_H - 1 - y) * SCR_W, sa_fb + y * SCR_W, SCR_W);
  GetClientRect(hwnd, &rc);
  cw = rc.right;
  ch = rc.bottom;
  w = cw;
  h = cw * SCR_H / SCR_W;
  if (h > ch) {
    h = ch;
    w = ch * SCR_W / SCR_H;
  }
  x = (cw - w) / 2;
  y = (ch - h) / 2;
  if (x > 0) {
    PatBlt(dc, 0, 0, x, ch, BLACKNESS);
    PatBlt(dc, x + w, 0, cw - x - w, ch, BLACKNESS);
  }
  if (y > 0) {
    PatBlt(dc, 0, 0, cw, y, BLACKNESS);
    PatBlt(dc, 0, y + h, cw, ch - y - h, BLACKNESS);
  }
  StretchDIBits(dc, x, y, w, h, 0, 0, SCR_W, SCR_H, flipped, (BITMAPINFO *)&bmi,
                DIB_RGB_COLORS, SRCCOPY);
}

static int key_bit(WPARAM vk)
{
  switch (vk) {
  case VK_UP: case 'W': return K_UP;
  case VK_DOWN: case 'S': return K_DOWN;
  case VK_LEFT: case 'A': return K_LEFT;
  case VK_RIGHT: case 'D': return K_RIGHT;
  case 'E': case VK_SPACE: return K_ACT;
  case 'Q': case 'F': return K_PHONE;
  case VK_SHIFT: return K_RUN;
  case VK_RETURN: return K_START;
  case VK_ESCAPE: return K_ESC;
  }
  return 0;
}

static void screenshot(void)
{
  char name[32] = "stanna_00.bmp";
  name[7] = (char)('0' + (shot_no / 10) % 10);
  name[8] = (char)('0' + shot_no % 10);
  shot_no++;
  save_bmp(name, sa_fb, SCR_W, SCR_H, 3);
}

static LRESULT CALLBACK wnd_proc(HWND h, UINT msg, WPARAM wp, LPARAM lp)
{
  PAINTSTRUCT ps;
  switch (msg) {
  case WM_KEYDOWN:
  case WM_SYSKEYDOWN:
    if (wp == VK_F12)
      screenshot();
    keys |= key_bit(wp);
    latch |= key_bit(wp);
    if (msg == WM_SYSKEYDOWN)
      break;
    return 0;
  case WM_KEYUP:
  case WM_SYSKEYUP:
    keys &= ~key_bit(wp);
    if (msg == WM_SYSKEYUP)
      break;
    return 0;
  case WM_KILLFOCUS:
    keys = 0;
    return 0;
  case WM_ERASEBKGND:
    return 1;
  case WM_PAINT:
    BeginPaint(h, &ps);
    present(ps.hdc);
    EndPaint(h, &ps);
    return 0;
  case WM_DESTROY:
    running = 0;
    PostQuitMessage(0);
    return 0;
  }
  return DefWindowProcA(h, msg, wp, lp);
}

static int joystick_keys(void)
{
  JOYINFOEX ji;
  int k = 0;
  memset(&ji, 0, sizeof(ji));
  ji.dwSize = sizeof(ji);
  ji.dwFlags = JOY_RETURNALL;
  if (joyGetPosEx(JOYSTICKID1, &ji) != JOYERR_NOERROR)
    return 0;
  if (ji.dwXpos < 16000) k |= K_LEFT;
  if (ji.dwXpos > 49000) k |= K_RIGHT;
  if (ji.dwYpos < 16000) k |= K_UP;
  if (ji.dwYpos > 49000) k |= K_DOWN;
  if (ji.dwButtons & 1) k |= K_ACT;
  if (ji.dwButtons & 6) k |= K_RUN;
  if (ji.dwButtons & 8) k |= K_PHONE;
  if (ji.dwButtons & 0x300) k |= K_START;
  return k;
}

/* scripted input for -autoplay: leave the chemistry room by the back door */
static int autoplay_keys(int t)
{
  if (t == 5 || t == 12) return K_START;
  if (t >= 20 && t < 32) return K_RIGHT;
  if (t >= 32 && t < 100) return K_DOWN;
  if (t >= 100 && t < 150) return K_RIGHT | K_RUN;
  return 0;
}

static void run_window(void)
{
  WNDCLASSA wc;
  MSG m;
  RECT r;
  int scale = 3, sw, sh, i, joy_ok;
  DWORD t0, ticks = 0, now;
  HDC dc;

  memset(&wc, 0, sizeof(wc));
  wc.lpfnWndProc = wnd_proc;
  wc.hInstance = GetModuleHandleA(NULL);
  wc.hIcon = LoadIconA(wc.hInstance, MAKEINTRESOURCEA(1));
  if (!wc.hIcon)
    wc.hIcon = LoadIconA(NULL, (LPCSTR)IDI_APPLICATION);
  wc.hCursor = LoadCursorA(NULL, (LPCSTR)IDC_ARROW);
  wc.lpszClassName = "StAnnaSchule";
  RegisterClassA(&wc);

  sw = GetSystemMetrics(SM_CXSCREEN);
  sh = GetSystemMetrics(SM_CYSCREEN);
  while (scale > 1 && (SCR_W * scale > sw - 40 || SCR_H * scale > sh - 80))
    scale--;
  r.left = 0;
  r.top = 0;
  r.right = SCR_W * scale;
  r.bottom = SCR_H * scale;
  AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW, FALSE);
  hwnd = CreateWindowExA(0, wc.lpszClassName, "St. Anna Schule - Herr Th\xFCner kommt",
                         WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT,
                         r.right - r.left, r.bottom - r.top, NULL, NULL, wc.hInstance, NULL);

  for (i = 0; i < 16; i++) {
    bmi.pal[i].rgbRed = sa_pal[i * 3];
    bmi.pal[i].rgbGreen = sa_pal[i * 3 + 1];
    bmi.pal[i].rgbBlue = sa_pal[i * 3 + 2];
  }
  bmi.h.biSize = sizeof(BITMAPINFOHEADER);
  bmi.h.biWidth = SCR_W;
  bmi.h.biHeight = SCR_H;
  bmi.h.biPlanes = 1;
  bmi.h.biBitCount = 8;
  bmi.h.biCompression = BI_RGB;
  bmi.h.biClrUsed = 16;

  {
    JOYINFOEX ji;
    memset(&ji, 0, sizeof(ji));
    ji.dwSize = sizeof(ji);
    ji.dwFlags = JOY_RETURNALL;
    joy_ok = joyGetPosEx(JOYSTICKID1, &ji) == JOYERR_NOERROR;
  }

  timeBeginPeriod(1);
  t0 = timeGetTime();
  while (running) {
    while (PeekMessageA(&m, NULL, 0, 0, PM_REMOVE)) {
      if (m.message == WM_QUIT)
        running = 0;
      TranslateMessage(&m);
      DispatchMessageA(&m);
    }
    if (!running)
      break;
    now = timeGetTime();
    if ((int)(now - (t0 + ticks * 1000 / TICKS_PER_SEC)) >= 0) {
      int steps = 0, k = keys | latch;
      latch = 0;
      if (joy_ok)
        k |= joystick_keys();
      while ((int)(now - (t0 + ticks * 1000 / TICKS_PER_SEC)) >= 0 && steps < 4) {
        if (autoplay_out)
          k = autoplay_keys((int)ticks);
        if (!SaGameTick(k))
          running = 0;
        ticks++;
        steps++;
      }
      if (steps == 4) /* far behind (e.g. window dragged): resync */
        t0 = now - ticks * 1000 / TICKS_PER_SEC;
      SaGameDraw();
      audio_pump();
      dc = GetDC(hwnd);
      present(dc);
      ReleaseDC(hwnd, dc);
      if (autoplay_out && ticks >= 150) {
        save_bmp(autoplay_out, sa_fb, SCR_W, SCR_H, 3);
        running = 0;
      }
    } else {
      audio_pump();
      Sleep(1);
    }
  }
  timeEndPeriod(1);
  audio_close();
}

void __cdecl SaEntry(void)
{
  char cmd[512], *p, *a;
  int i;
  const char *src = GetCommandLineA();
  for (i = 0; i < 511 && src[i]; i++)
    cmd[i] = src[i];
  cmd[i] = 0;
  p = cmd;
  next_arg(&p); /* program name */

  SaGameInit(GetTickCount());

  a = next_arg(&p);
  if (a && str_eq(a, "-shot")) {
    int n = to_int(next_arg(&p));
    char *out = next_arg(&p);
    SaDebugScene(n);
    SaGameDraw();
    ExitProcess(out && save_bmp(out, sa_fb, SCR_W, SCR_H, 3) ? 0 : 1);
  }
  if (a && str_eq(a, "-icon")) {
    static U8 icon[256 * 256];
    int s = to_int(next_arg(&p));
    char *out = next_arg(&p);
    if (s < 1 || s > 8)
      s = 1;
    SaDrawFaceIcon(icon, 32 * s, 32 * s, 0, 0, s);
    ExitProcess(out && save_bmp(out, icon, 32 * s, 32 * s, 1) ? 0 : 1);
  }
  if (a && str_eq(a, "-world")) {
    int f = to_int(next_arg(&p));
    char *out = next_arg(&p);
    if (f < 1 || f > 6)
      f = 4;
    SaDebugWorld(f - 1);
    ExitProcess(out && save_bmp(out, sa_world, 896, 448, 1) ? 0 : 1);
  }
  if (a && str_eq(a, "-autoplay"))
    autoplay_out = next_arg(&p);

  audio_init();

  run_window();
  ExitProcess(0);
}
