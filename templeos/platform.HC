// ---------------------------------------------------------------------
// TempleOS platform layer (hand written, appended by make_holyc.py)
// Graphics: own 640x400 device context, blitted by the window manager.
// Keys: scan codes from key messages. Sound: PC speaker.
// ---------------------------------------------------------------------

CDC *sa_dc = NULL;
I64 sa_keys = 0;
I64 sa_latch = 0;

// The PC speaker plays one tone at a time: sound effects first (from the
// same table the synthesiser uses), otherwise the ringtone, otherwise the
// melody and bass of the music.
U0 SaSndUpdate()
{
  I64 b, seg, ms, len, w, f0, f1, f = 0, active = FALSE;
  if (sa_snd_id > 0) {
    ms = sa_snd_t * 33;
    for (seg = 0; seg < SFX_SEG && !active; seg++) {
      b = (sa_snd_id * SFX_SEG + seg) * 5;
      len = sa_sfx_def[b + 3];
      if (ms < len) {
        active = TRUE;
        w = sa_sfx_def[b];
        f0 = sa_sfx_def[b + 1];
        f1 = sa_sfx_def[b + 2];
        if (w != 5) {
          f = f0 + (f1 - f0) * ms / len;
          if (w == 3 || w == 4)
            f = f / 2 + RandU16 % (f + 1);
        }
      } else
        ms = ms - len;
    }
    if (!active)
      sa_snd_id = 0;
  }
  if (!active) {
    if (sa_ring_t > 0 && sa_vage[3] < 3)
      f = SaNoteHz(sa_vnote[3]);
    else if (sa_mus_trk != TRK_NONE && sa_vage[0] < 4)
      f = SaNoteHz(sa_vnote[0]);
    else if (sa_mus_trk != TRK_NONE && sa_vage[1] < 3)
      f = SaNoteHz(sa_vnote[1]);
  }
  if (f > 30)
    Snd(Freq2Ona(f));
  else
    Snd;
  sa_snd_t++;
}

U0 SaDrawIt(CTask *task, CDC *dc)
{
  if (sa_dc)
    GrBlot(dc, 0, 0, sa_dc);
}

U0 SaSetPalette()
{
  I64 i;
  CBGR48 c;
  for (i = 0; i < 16; i++) {
    c.r = sa_pal[i * 3] * 0x101;
    c.g = sa_pal[i * 3 + 1] * 0x101;
    c.b = sa_pal[i * 3 + 2] * 0x101;
    GrPaletteColorSet(i, c);
  }
}

// PC scan codes (set 1): arrows, WASD, E, space, Q, F, shift, enter, esc
U0 SaPollKeys()
{
  I64 a1, a2, m, sc, bit;
  while (m = ScanMsg(&a1, &a2, (1 << MSG_KEY_DOWN) | (1 << MSG_KEY_UP))) {
    sc = a2 & 0x7F;
    bit = 0;
    if (sc == 0x48 || sc == 0x11)
      bit = K_UP;
    else if (sc == 0x50 || sc == 0x1F)
      bit = K_DOWN;
    else if (sc == 0x4B || sc == 0x1E)
      bit = K_LEFT;
    else if (sc == 0x4D || sc == 0x20)
      bit = K_RIGHT;
    else if (sc == 0x12 || sc == 0x39)
      bit = K_ACT;
    else if (sc == 0x10 || sc == 0x21)
      bit = K_PHONE;
    else if (sc == 0x2A || sc == 0x36)
      bit = K_RUN;
    else if (sc == 0x1C)
      bit = K_START;
    else if (sc == 0x01)
      bit = K_ESC;
    if (m == MSG_KEY_DOWN) {
      sa_keys = sa_keys | bit;
      sa_latch = sa_latch | bit;
    } else
      sa_keys = sa_keys & ~bit;
  }
}

U0 StAnna()
{
  I64 x, y, c, w, keys;
  U8 *row, *src;
  SettingsPush;
  WinMax;
  DocCursor;
  DocClear;
  SaSetPalette;
  sa_dc = DCNew(640, 400);
  SaGameInit(GetTSC);
  Fs->draw_it = &SaDrawIt;
  try {
    while (TRUE) {
      SaPollKeys;
      keys = sa_keys | sa_latch;
      sa_latch = 0;
      if (!SaGameTick(keys))
        break;
      SaGameDraw;
      w = sa_dc->width_internal;
      for (y = 0; y < 200; y++) {
        src = sa_fb + y * 320;
        row = sa_dc->body + y * 2 * w;
        for (x = 0; x < 320; x++) {
          c = src[x];
          row[x * 2] = c;
          row[x * 2 + 1] = c;
          row[w + x * 2] = c;
          row[w + x * 2 + 1] = c;
        }
      }
      SaSndUpdate;
      Sleep(33);
    }
  } catch
    PutExcept;
  Snd;
  Fs->draw_it = NULL;
  DCDel(sa_dc);
  sa_dc = NULL;
  SettingsPop;
}

StAnna;
