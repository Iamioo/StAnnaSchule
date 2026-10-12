/* St. Anna Schule - "Herr Thüner kommt"
 * Platform independent game core: six floors, AI, phone, music, rendering.
 *
 * Written in a deliberately plain subset of C so tools/make_holyc.py can
 * turn it into HolyC for the TempleOS port: C89 declarations at the top of
 * each function, no structs/enums/ternaries/casts/continue, every function
 * defined before it is used, and every bit operation fully parenthesised
 * (HolyC gives & | ^ << >> a different precedence than C).
 */
/*C_ONLY*/
#include "game.h"
#include "game_data.inc"
/*END_C_ONLY*/
/*HOLYC_INCLUDE game_data.inc*/

#define ST_TITLE 0
#define ST_INTRO 1
#define ST_PLAY 2
#define ST_CAUGHT 3
#define ST_OVER 4
#define ST_WIN 5
#define ST_PAUSE 6

/* difficulty: 0 easy, 1 medium, 2 hard (no checkpoints) */
#define MODE_EASY 0
#define MODE_MEDIUM 1
#define MODE_HARD 2
#define SA_SAVE_N 4096

/* Herr Thüner (teacher 0) */
#define TH_SIT 0
#define TH_CHASE 1
#define TH_SEARCH 2
#define TH_WANDER 3
#define TH_GRAB 4
/* Herr Kirschbaum (teacher 1) */
#define KB_PATROL 10
#define KB_RUSH 11
#define KB_LOOK 12
#define KB_WRITE 13
/* Conjoke */
#define CJ_OFF 0
#define CJ_WAIT 1
#define CJ_RUN 2
#define CJ_FIGHT 3
#define CJ_LEAVE 4
#define CJ_GUARD 5
/* the cleaners (entities 3 and 4) */
#define CL_WALK 0
#define CL_MOP 1
#define CL_WATCH 2
/* Frau Niehaus (entity 5), only ever on the 3rd floor */
#define NI_WANDER 0
#define NI_SING 1
#define NI_COOL 2
#define F_NIEHAUS 2

#define NENT 6
#define E_CL 3
#define E_NI 5
#define NVEH 3
#define VH_SCOOTER 0
#define VH_BIKE 1

#define DIR_DOWN 0
#define DIR_UP 1
#define DIR_RIGHT 2
#define DIR_LEFT 3

#define NITEMS 48
#define F_OUTSIDE 4
#define F_TOP 5

/* balance (speeds in 1/256 pixel per tick; the player walks at 300 and
 * sprints at 540) */
#define SA_CHASE_BASE 265
#define SA_CHASE_RAMP 110
#define SA_CHASE_MAX 335
#define SA_LOSE_TICKS 45
#define SA_SEE_DIST2 82
#define SA_KB_RUSH 400
#define SA_CJ_SPEED 520
#define SA_CALL_CD 1200
#define SA_CL_SPEED 110
#define SA_NI_SPEED 140
#define SA_WET 1500

#define ENV_MAX 1048576

/* ------------------------------------------------------------------ */
/* state                                                               */
/* ------------------------------------------------------------------ */

U8 sa_fb[64000];
U8 sa_world[WORLD_N];
U8 sa_map[MAP_N];
U8 sa_vis[MAP_N];
I64 sa_dist[12096];
I64 sa_wet[MAP_N];
I64 sa_queue[MAP_N];
U8 sa_ltab[64];
U8 sa_font_map[256];
CH sa_tbuf[48];
CH sa_le_buf[64];
CH sa_obj_buf[64];
CH sa_num_buf[48];
CH sa_mb1[64];
CH sa_mb2[64];
CH sa_hint_buf[64];
I64 sa_touch_ui = 0;

U32 sa_seed = 1;
I64 sa_state = 0;
I64 sa_timer = 0;
I64 sa_tick = 0;
I64 sa_keys_prev = 0;
I64 sa_floor = 3;

I64 sa_p0x = 13;
I64 sa_p0y = 10;
I64 sa_h0x = 8;
I64 sa_h0y = 2;

/* player (feet position, 8 bits fixed point) */
I64 sa_px = 0;
I64 sa_py = 0;
I64 sa_pdir = 0;
I64 sa_panim = 0;
I64 sa_pmoving = 0;
I64 sa_crouch = 0;
I64 sa_hidden = 0;
I64 sa_stamina = 1000;
I64 sa_tired = 0;
I64 sa_running = 0;
I64 sa_hit_tile = 0;
I64 sa_locked_t = 0;
I64 sa_room = -1;
I64 sa_room_t = 0;
I64 sa_stair_cd = 0;
I64 sa_stair_arm = 1;
I64 sa_sign_t = 0;
I64 sa_boost = 0;
I64 sa_near_obj = 0;

/* things lying around, the tasks and what you carry */
I64 sa_it_f[NITEMS];
I64 sa_it_x[NITEMS];
I64 sa_it_y[NITEMS];
I64 sa_it_t[NITEMS];
I64 sa_it_got[NITEMS];
I64 sa_it_n = 0;
I64 sa_have[NITYPES];
I64 sa_task = 0;
I64 sa_unlock[NFLOORS];
I64 sa_power = 0;
I64 sa_pool_open = 0;
I64 sa_hint = -1;
I64 sa_code[3];
I64 sa_deaths = 0;
I64 sa_cj_met = 0;
I64 sa_bomb_t = 0;

/* checkpoint: the state after the last finished task */
I64 sa_cp_task = 0;
I64 sa_cp_power = 0;
I64 sa_cp_pool = 0;
I64 sa_cp_unlock[NFLOORS];
I64 sa_cp_got[NITEMS];
I64 sa_cp_have[NITYPES];

/* entities: 0 Herr Thüner, 1 Herr Kirschbaum, 2 Conjoke, 3 and 4 the
 * cleaners, 5 Frau Niehaus */
I64 sa_ex[NENT];
I64 sa_ey[NENT];
I64 sa_edir[NENT];
I64 sa_eanim[NENT];
I64 sa_emov[NENT];
I64 sa_egoal[NENT];
I64 sa_ebfs[NENT];

/* teachers 0 and 1 */
I64 sa_tst[2];
I64 sa_ttimer[2];
I64 sa_tlast[2];
I64 sa_tlost[2];
I64 sa_tstun[2];
I64 sa_tslow[2];
I64 sa_tf[2];
I64 sa_tvalid[2];
I64 sa_tfollow[2];
I64 sa_tfollow_f[2];
I64 sa_troam[2];
I64 sa_kb_alert = 0;
I64 sa_heard_t = 0;
I64 sa_tslipcd[2];
I64 sa_alarm_t = 0;

/* the cleaners: floor, on this floor, state, timer, how long they have
 * seen you, cooldown after calling Herr Thüner, roaming timer and their
 * yellow "wet floor" sign */
I64 sa_cf[2];
I64 sa_cvalid[2];
I64 sa_cst[2];
I64 sa_ct[2];
I64 sa_csee[2];
I64 sa_ccd[2];
I64 sa_croam[2];
I64 sa_csx[2];
I64 sa_csy[2];

/* Frau Niehaus and the singing */
I64 sa_ni_valid = 0;
I64 sa_ni_st = 0;
I64 sa_ni_t = 0;
I64 sa_ni_meter = 0;
I64 sa_ni_said = 0;
I64 sa_ni_hum = 0;
I64 sa_ni_idle = 0;
I64 sa_sing_t = 0;
I64 sa_sing_step = 0;

/* slipping on a wet floor */
I64 sa_slip_t = 0;

/* e-scooters and bikes on the schoolyard (pixel position, 8 bits fixed) */
I64 sa_vh_n = 0;
I64 sa_vh_x[NVEH];
I64 sa_vh_y[NVEH];
I64 sa_vh_t[NVEH];
I64 sa_vh_charge[NVEH];
I64 sa_vh_dir[NVEH];
I64 sa_ride = -1;
I64 sa_vh_near = -1;

/* who is talking in the message box: -1 nobody, 0 Herr Lersch,
 * 1 Frau Niehaus, 2 a cleaner */
I64 sa_msg_face = -1;
I64 sa_start_room = -1;

/* difficulty and the menus */
I64 sa_mode = MODE_MEDIUM;
I64 sa_menu = 0;
I64 sa_pause_sel = 0;
CH *sa_pause_msg = "";
I64 sa_pause_msg_t = 0;
/* how long a cleaner needs to recognise you, how far she sees (tiles
 * squared), and how fast Frau Niehaus gets you singing - per mode */
I64 sa_md_clsee[3] = { 210, 150, 120 };
I64 sa_md_clrange[3] = { 36, 64, 81 };
I64 sa_md_sing[3] = { 70, 45, 35 };

/* the parts of every floor you have already seen, for the map */
U8 sa_seen[12096];

/* the saved game: the platform keeps it (the browser in localStorage),
 * sa_save_seq goes up every time it changes */
U8 sa_save[SA_SAVE_N];
I64 sa_save_ok = 0;
I64 sa_save_seq = 0;
I64 sa_save_pos = 0;

/* Conjoke */
I64 sa_cj_st = 0;
I64 sa_cj_t = 0;
I64 sa_cj_tgt = -1;

/* phone */
I64 sa_phone = 1;
I64 sa_call_cd = 0;
I64 sa_ring_t = 0;
I64 sa_ring_step = 0;
I64 sa_next_ring = 1800;
I64 sa_write_t = 0;

/* Herr Lersch: always on the floor of the current task */
I64 sa_le_f = 3;
I64 sa_le_tx = 20;
I64 sa_le_ty = 3;
I64 sa_le_sx[NFLOORS];
I64 sa_le_sy[NFLOORS];
I64 sa_le_near = 0;

I64 sa_play_ticks = 0;
I64 sa_end_ticks = 0;
CH *sa_msg = "";
CH *sa_msg2 = "";
I64 sa_msg_t = 0;
CH *sa_over_reason = "";
I64 sa_spot_t = 0;
I64 sa_shake = 0;
I64 sa_pulse = 0;
I64 sa_heart_t = 0;
I64 sa_light_r = 92;
I64 sa_camx = 0;
I64 sa_camy = 0;

/* audio */
I64 sa_arate = 22050;
I64 sa_mus_trk = 0;
I64 sa_mus_step = 0;
I64 sa_mus_tick = 0;
I64 sa_vnote[4];
I64 sa_vtrig[4];
I64 sa_vage[4];
I64 sa_rseen[4];
I64 sa_rph[4];
I64 sa_rinc[4];
I64 sa_renv[4];
I64 sa_rdec[4];
I64 sa_rwave[4];
I64 sa_rsweep[4];
I64 sa_rvol[4];
I64 sa_sfx_req = 0;
I64 sa_sfx_n = 0;
I64 sa_sfx_seen = 0;
I64 sa_sfx_id = 0;
I64 sa_sfx_seg = 0;
I64 sa_sfx_pos = 0;
I64 sa_sfx_len = 0;
I64 sa_sfx_ph = 0;
I64 sa_sfx_inc = 0;
I64 sa_asamp = 0;
U32 sa_anoise = 7;

/* ------------------------------------------------------------------ */
/* small helpers                                                       */
/* ------------------------------------------------------------------ */

I64 SaRand(I64 n)
{
  sa_seed = sa_seed * 1103515245 + 12345;
  return ((sa_seed >> 16) & 32767) % n;
}

I64 SaHash(I64 x, I64 y)
{
  U32 h, t;
  h = x;
  h = h * 374761393;
  t = y;
  t = t * 668265263;
  h = h + t;
  h = h ^ (h >> 13);
  h = h * 1274126177;
  h = h ^ (h >> 16);
  return h & 255;
}

I64 SaAbs(I64 x)
{
  if (x < 0)
    return -x;
  return x;
}

I64 SaIsqrt(I64 n)
{
  I64 r, b;
  if (n <= 0)
    return 0;
  r = 0;
  b = 1 << 30;
  while (b > n)
    b = b >> 2;
  while (b != 0) {
    if (n >= r + b) {
      n = n - r - b;
      r = (r >> 1) + b;
    } else
      r = r >> 1;
    b = b >> 2;
  }
  return r;
}

U0 SaMemSet(U8 *d, I64 v, I64 n)
{
  I64 i;
  for (i = 0; i < n; i++)
    d[i] = v;
}

U0 SaMemCpy(U8 *d, U8 *s, I64 n)
{
  I64 i;
  for (i = 0; i < n; i++)
    d[i] = s[i];
}

U0 SaStrCpy(CH *d, CH *s)
{
  I64 i;
  i = 0;
  while (s[i] != 0 && i < 46) {
    d[i] = s[i];
    i++;
  }
  d[i] = 0;
}

U0 SaStrCat(CH *d, CH *s)
{
  I64 i, j;
  i = 0;
  while (d[i] != 0)
    i++;
  j = 0;
  while (s[j] != 0 && i < 62) {
    d[i] = s[j];
    i++;
    j++;
  }
  d[i] = 0;
}

U0 SaStrNum(CH *d, I64 n)
{
  I64 i;
  i = 0;
  while (d[i] != 0)
    i++;
  if (n >= 10) {
    d[i] = '0' + (n / 10) % 10;
    i++;
  }
  d[i] = '0' + n % 10;
  d[i + 1] = 0;
}

/* picks the hint for the input method: keyboard, touch or gamepad */
CH *SaHint(CH *kbd, CH *touch, CH *pad)
{
  if (sa_touch_ui == 1)
    return touch;
  if (sa_touch_ui == 2)
    return pad;
  return kbd;
}

U0 SaMsg(CH *s, I64 t)
{
  sa_msg = s;
  sa_msg2 = "";
  sa_msg_t = t;
  sa_msg_face = -1;
}

U0 SaMsg2(CH *s, CH *s2, I64 t)
{
  sa_msg = s;
  sa_msg2 = s2;
  sa_msg_t = t;
  sa_msg_face = -1;
}

/* a message with the portrait of who is speaking */
U0 SaSay(I64 face, CH *s, CH *s2, I64 t)
{
  SaMsg2(s, s2, t);
  sa_msg_face = face;
}

/* ------------------------------------------------------------------ */
/* audio: a tiny chiptune synth (lead, bass, drums, ringtone, effects) */
/* ------------------------------------------------------------------ */

I64 SaNoteHz256(I64 n)
{
  I64 o;
  if (n < 12)
    n = 12;
  o = n / 12 - 1;
  return sa_note_base[n % 12] << o;
}

I64 SaNoteHz(I64 n)
{
  return SaNoteHz256(n) / 256;
}

U0 SaSound(I64 id)
{
  sa_sfx_req = id;
  sa_sfx_n++;
  SaPlatSound(id);
}

U0 SaAudioSetRate(I64 rate)
{
  sa_arate = rate;
}

I64 SaWave(I64 w, I64 ph)
{
  I64 v;
  v = ph & 65535;
  if (w == 0) {
    if (v < 32768)
      return 90;
    return -90;
  }
  if (w == 1) {
    if (v < 32768)
      return (v >> 7) - 128;
    return ((65535 - v) >> 7) - 128;
  }
  if (w == 2)
    return (v >> 8) - 128;
  sa_anoise = sa_anoise * 1103515245 + 12345;
  if (w == 3)
    return ((sa_anoise >> 16) & 255) - 128;
  if (w == 4)
    return ((v >> 8) - 128) / 2 + (((sa_anoise >> 16) & 255) - 128) / 3;
  return 0;
}

U0 SaVoiceStart(I64 ch, I64 note)
{
  I64 ms;
  ms = 300;
  sa_rsweep[ch] = 0;
  if (ch == 2) {
    if (note == 1) {
      sa_rwave[2] = 1;
      sa_rinc[2] = (95 << 16) / sa_arate;
      sa_rsweep[2] = 1;
      ms = 160;
      sa_rvol[2] = 5;
    } else if (note == 2) {
      sa_rwave[2] = 3;
      sa_rinc[2] = 0;
      ms = 130;
      sa_rvol[2] = 3;
    } else {
      sa_rwave[2] = 3;
      sa_rinc[2] = 0;
      ms = 25;
      sa_rvol[2] = 2;
    }
  } else {
    sa_rinc[ch] = (SaNoteHz256(note) << 8) / sa_arate;
    if (ch == 0) {
      sa_rwave[0] = 0;
      ms = 320;
      sa_rvol[0] = 3;
    } else if (ch == 1) {
      sa_rwave[1] = 1;
      ms = 520;
      sa_rvol[1] = 3;
    } else {
      sa_rwave[3] = 0;
      ms = 110;
      sa_rvol[3] = 4;
    }
  }
  sa_renv[ch] = ENV_MAX;
  sa_rdec[ch] = ENV_MAX / (sa_arate * ms / 1000 + 1);
}

U0 SaSfxSeg()
{
  I64 b;
  b = (sa_sfx_id * SFX_SEG + sa_sfx_seg) * 5;
  sa_sfx_len = sa_sfx_def[b + 3] * sa_arate / 1000;
  sa_sfx_pos = 0;
}

U0 SaSfxStart(I64 id)
{
  sa_sfx_id = id;
  sa_sfx_seg = 0;
  SaSfxSeg();
  if (sa_sfx_len <= 0)
    sa_sfx_id = 0;
}

I64 SaSfxSample()
{
  I64 b, w, f0, f1, vol, f, env;
  while (sa_sfx_pos >= sa_sfx_len) {
    sa_sfx_seg++;
    if (sa_sfx_seg >= SFX_SEG) {
      sa_sfx_id = 0;
      return 0;
    }
    SaSfxSeg();
    if (sa_sfx_len <= 0) {
      sa_sfx_id = 0;
      return 0;
    }
  }
  b = (sa_sfx_id * SFX_SEG + sa_sfx_seg) * 5;
  w = sa_sfx_def[b];
  f0 = sa_sfx_def[b + 1];
  f1 = sa_sfx_def[b + 2];
  vol = sa_sfx_def[b + 4];
  if ((sa_sfx_pos & 63) == 0) {
    f = f0 + (f1 - f0) * sa_sfx_pos / sa_sfx_len;
    sa_sfx_inc = (f << 16) / sa_arate;
  }
  sa_sfx_ph = (sa_sfx_ph + sa_sfx_inc) & 65535;
  sa_sfx_pos++;
  if (w == 5)
    return 0;
  env = vol * (sa_sfx_len - sa_sfx_pos) / sa_sfx_len;
  return SaWave(w, sa_sfx_ph) * env / 32;
}

/* Fills n mono 16-bit samples at sa_arate. Called by the platform. */
U0 SaAudioRender(I16 *out, I64 n)
{
  I64 i, ch, s, m;
  for (ch = 0; ch < 4; ch++)
    if (sa_vtrig[ch] != sa_rseen[ch]) {
      sa_rseen[ch] = sa_vtrig[ch];
      SaVoiceStart(ch, sa_vnote[ch]);
    }
  if (sa_sfx_n != sa_sfx_seen) {
    sa_sfx_seen = sa_sfx_n;
    SaSfxStart(sa_sfx_req);
  }
  for (i = 0; i < n; i++) {
    m = 0;
    for (ch = 0; ch < 4; ch++)
      if (sa_renv[ch] > 0) {
        m = m + SaWave(sa_rwave[ch], sa_rph[ch]) * (sa_renv[ch] >> 12) / 256 * sa_rvol[ch];
        sa_rph[ch] = (sa_rph[ch] + sa_rinc[ch]) & 65535;
        sa_renv[ch] = sa_renv[ch] - sa_rdec[ch];
        if (sa_rsweep[ch] && (sa_asamp & 31) == 0)
          sa_rinc[ch] = sa_rinc[ch] * 31 / 32;
      }
    s = m * 18;
    if (sa_sfx_id > 0)
      s = s + SaSfxSample() * 22;
    /* soft limiter instead of hard clipping */
    if (s > 20000)
      s = 20000 + (s - 20000) / 4;
    if (s < -20000)
      s = -20000 + (s + 20000) / 4;
    if (s > 32000)
      s = 32000;
    if (s < -32000)
      s = -32000;
    out[i] = s;
    sa_asamp++;
  }
}

/* ------------------------------------------------------------------ */
/* drawing primitives                                                  */
/* ------------------------------------------------------------------ */

U0 SaPut(U8 *dst, I64 dw, I64 dh, I64 x, I64 y, I64 c)
{
  if (x >= 0 && y >= 0 && x < dw && y < dh)
    dst[y * dw + x] = c;
}

U0 SaPutFb(I64 x, I64 y, I64 c)
{
  SaPut(sa_fb, SCR_W, SCR_H, x, y, c);
}

I64 SaGetFb(I64 x, I64 y)
{
  if (x >= 0 && y >= 0 && x < SCR_W && y < SCR_H)
    return sa_fb[y * SCR_W + x];
  return 0;
}

U0 SaFillRect(U8 *dst, I64 dw, I64 dh, I64 x, I64 y, I64 w, I64 h, I64 c)
{
  I64 i, j, x1, y1;
  x1 = x + w;
  y1 = y + h;
  if (x < 0)
    x = 0;
  if (y < 0)
    y = 0;
  if (x1 > dw)
    x1 = dw;
  if (y1 > dh)
    y1 = dh;
  for (j = y; j < y1; j++)
    for (i = x; i < x1; i++)
      dst[j * dw + i] = c;
}

U0 SaRectFb(I64 x, I64 y, I64 w, I64 h, I64 c)
{
  SaFillRect(sa_fb, SCR_W, SCR_H, x, y, w, h, c);
}

U0 SaDisc(I64 cx, I64 cy, I64 r, I64 c)
{
  I64 x, y;
  for (y = -r; y <= r; y++)
    for (x = -r; x <= r; x++)
      if (x * x + y * y <= r * r + r)
        SaPutFb(cx + x, cy + y, c);
}

U0 SaDrawArt(U8 *dst, I64 dw, I64 dh, I64 x, I64 y, I64 row0, I64 w, I64 h,
             I64 flip, I64 s, I64 eyec)
{
  I64 r, c, ch, col, xx;
  CH *row;
  for (r = 0; r < h; r++) {
    row = sa_art[row0 + r];
    for (c = 0; c < w; c++) {
      ch = row[c];
      col = -1;
      if (ch >= '0' && ch <= '9')
        col = ch - '0';
      else if (ch >= 'a' && ch <= 'f')
        col = ch - 'a' + 10;
      else if (ch == 'x')
        col = eyec;
      if (col >= 0) {
        xx = c;
        if (flip)
          xx = w - 1 - c;
        if (s == 1)
          SaPut(dst, dw, dh, x + xx, y + r, col);
        else
          SaFillRect(dst, dw, dh, x + xx * s, y + r * s, s, s, col);
      }
    }
  }
}

U0 SaDrawFaceIcon(U8 *dst, I64 dw, I64 dh, I64 x, I64 y, I64 s)
{
  SaDrawArt(dst, dw, dh, x, y, ART_FACE, 32, 32, FALSE, s, 12);
}

/* white silhouette art with an automatic black outline (floor signs) */
U0 SaDrawIcon(I64 x, I64 y, I64 row0, I64 s)
{
  I64 r, c, k, ox, oy;
  CH *row;
  for (k = 0; k < 2; k++)
    for (r = 0; r < 18; r++) {
      row = sa_art[row0 + r];
      for (c = 0; c < 20; c++)
        if (row[c] == '6') {
          if (k == 0) {
            for (oy = -1; oy <= 1; oy++)
              for (ox = -1; ox <= 1; ox++)
                SaRectFb(x + (c + ox) * s, y + (r + oy) * s, s, s, 1);
          } else
            SaRectFb(x + c * s, y + r * s, s, s, 6);
        }
    }
}

/* ------------------------------------------------------------------ */
/* text                                                                */
/* ------------------------------------------------------------------ */

U0 SaFontInit()
{
  I64 i, ch;
  for (i = 0; i < 256; i++)
    sa_font_map[i] = 0;
  i = 0;
  while (sa_font_chars[i] != 0) {
    ch = sa_font_chars[i] & 255;
    sa_font_map[ch] = i + 1;
    i++;
  }
  for (i = 0; i < 26; i++)
    sa_font_map['a' + i] = i + 1;
  sa_font_map[200] = 54;
  sa_font_map[201] = 55;
  sa_font_map[202] = 56;
  sa_font_map[203] = 57;
}

/* Reads one character, decoding the UTF-8 umlauts used in the strings. */
I64 SaGetCh(CH *s, I64 *pos)
{
  I64 b, n;
  b = s[*pos] & 255;
  if (b == 0)
    return 0;
  *pos = *pos + 1;
  if (b == 195) {
    n = s[*pos] & 255;
    if (n != 0)
      *pos = *pos + 1;
    if (n == 132 || n == 164)
      return 200;
    if (n == 150 || n == 182)
      return 201;
    if (n == 156 || n == 188)
      return 202;
    if (n == 159)
      return 203;
    return '?';
  }
  return b;
}

U0 SaDrawTextN(U8 *dst, I64 dw, I64 dh, I64 x, I64 y, CH *s, I64 c, I64 scale,
               I64 maxn)
{
  I64 i, ch, g, r, k, bits, n;
  i = 0;
  n = 0;
  while (n < maxn) {
    ch = SaGetCh(s, &i);
    if (ch == 0)
      return;
    g = sa_font_map[ch];
    if (g > 0) {
      g = (g - 1) * 7;
      for (r = 0; r < 7; r++) {
        bits = sa_font[g + r];
        for (k = 0; k < 5; k++)
          if ((bits & (16 >> k)) != 0)
            SaFillRect(dst, dw, dh, x + k * scale, y + r * scale, scale, scale, c);
      }
    }
    x = x + 6 * scale;
    n++;
  }
}

U0 SaDrawText(U8 *dst, I64 dw, I64 dh, I64 x, I64 y, CH *s, I64 c, I64 scale)
{
  SaDrawTextN(dst, dw, dh, x, y, s, c, scale, 9999);
}

I64 SaTextLen(CH *s)
{
  I64 i, n;
  i = 0;
  n = 0;
  while (SaGetCh(s, &i) != 0)
    n++;
  return n;
}

I64 SaTextW(CH *s, I64 scale)
{
  I64 n;
  n = SaTextLen(s);
  if (n == 0)
    return 0;
  return n * 6 * scale - scale;
}

U0 SaTextSh(I64 x, I64 y, CH *s, I64 c, I64 sc, I64 scale)
{
  SaDrawText(sa_fb, SCR_W, SCR_H, x + scale, y + scale, s, sc, scale);
  SaDrawText(sa_fb, SCR_W, SCR_H, x, y, s, c, scale);
}

U0 SaTextCenter(I64 y, CH *s, I64 c, I64 scale)
{
  SaTextSh((SCR_W - SaTextW(s, scale)) / 2, y, s, c, 1, scale);
}

CH *SaFmtTime(CH *prefix, I64 ticks)
{
  I64 s, m, i, j;
  s = ticks / TICKS_PER_SEC;
  m = s / 60;
  s = s % 60;
  i = 0;
  j = 0;
  while (prefix[j] != 0 && i < 30) {
    sa_tbuf[i] = prefix[j];
    i++;
    j++;
  }
  if (m >= 10) {
    sa_tbuf[i] = '0' + (m / 10) % 10;
    i++;
  }
  sa_tbuf[i] = '0' + m % 10;
  sa_tbuf[i + 1] = ':';
  sa_tbuf[i + 2] = '0' + s / 10;
  sa_tbuf[i + 3] = '0' + s % 10;
  sa_tbuf[i + 4] = 0;
  return sa_tbuf;
}

/* ------------------------------------------------------------------ */
/* map                                                                 */
/* ------------------------------------------------------------------ */

I64 SaIsWallCh(I64 ch)
{
  if (ch == '#' || ch == 'b' || ch == 'w' || ch == 'p' || ch == 'n' || ch == 'y' ||
      ch == 'j' || ch == 'q' || ch == 'z' || ch == 'F' || ch == 'u')
    return TRUE;
  return FALSE;
}

I64 SaIsFloorCh(I64 ch)
{
  if (ch == '.' || ch == ',' || ch == 'c' || ch == 't' || ch == 'h' || ch == 'f' ||
      ch == 'm' || ch == '_' || ch == ';' || ch == '-' || ch == 'r')
    return TRUE;
  return FALSE;
}

I64 SaWalkCh(I64 ch)
{
  if (SaIsFloorCh(ch) || ch == 'D' || ch == '^' || ch == 'v')
    return TRUE;
  return FALSE;
}

I64 SaTile(I64 tx, I64 ty)
{
  if (tx < 0 || ty < 0 || tx >= MAP_W || ty >= MAP_H)
    return '#';
  return sa_map[ty * MAP_W + tx];
}

/* glass, fences, gates and water let light and looks through */
I64 SaOpaque(I64 tx, I64 ty)
{
  I64 ch;
  ch = SaTile(tx, ty);
  if (ch == 'q' || ch == 'F')
    return FALSE;
  if (SaIsWallCh(ch) || ch == 'E' || ch == 'X')
    return TRUE;
  return FALSE;
}

I64 SaFloorOf(I64 tx, I64 ty)
{
  I64 ch;
  ch = SaTile(tx, ty + 1);
  if (SaIsFloorCh(ch))
    return ch;
  ch = SaTile(tx, ty - 1);
  if (SaIsFloorCh(ch))
    return ch;
  ch = SaTile(tx - 1, ty);
  if (SaIsFloorCh(ch))
    return ch;
  ch = SaTile(tx + 1, ty);
  if (SaIsFloorCh(ch))
    return ch;
  if (sa_floor == F_OUTSIDE)
    return ';';
  return '.';
}

/* TRUE when the stairs up (or down) lead to a floor you may enter */
I64 SaStairOpen(I64 up)
{
  if (up) {
    if (sa_floor >= F_TOP)
      return FALSE;
    return sa_unlock[sa_floor + 1];
  }
  if (sa_floor <= 0)
    return FALSE;
  return sa_unlock[sa_floor - 1];
}

/* finds the start positions of the player and Herr Thüner */
U0 SaScanFloors()
{
  I64 f, tx, ty, ch;
  CH *row;
  for (f = 0; f < NFLOORS; f++)
    for (ty = 0; ty < MAP_H; ty++) {
      row = sa_floor_src[f * MAP_H + ty];
      for (tx = 0; tx < MAP_W; tx++) {
        ch = row[tx];
        if (ch == 'P') {
          sa_p0x = tx;
          sa_p0y = ty;
        }
        if (ch == 'H') {
          sa_h0x = tx;
          sa_h0y = ty;
        }
      }
    }
}

/* ------------------------------------------------------------------ */
/* pre-rendering the current floor into one big bitmap                 */
/* ------------------------------------------------------------------ */

U0 SaWRect(I64 x, I64 y, I64 w, I64 h, I64 c)
{
  SaFillRect(sa_world, WORLD_W, WORLD_H, x, y, w, h, c);
}

U0 SaDrawFloor(I64 ch, I64 px, I64 py)
{
  I64 x, y, c, h, wx, wy;
  for (y = 0; y < 16; y++)
    for (x = 0; x < 16; x++) {
      wx = px + x;
      wy = py + y;
      h = SaHash(wx, wy);
      if (ch == ',') {
        c = 8;
        if ((y & 3) == 3)
          c = 7;
        else if (((wx + (wy >> 2) * 5) & 15) == 0)
          c = 7;
        else if (h < 18)
          c = 7;
      } else if (ch == 'f') {
        c = 8;
        if ((wx & 7) == 0)
          c = 7;
        else if (((wy + (wx >> 3) * 11) & 31) == 0)
          c = 7;
        else if (h < 14)
          c = 10;
      } else if (ch == 'm') {
        c = 13;
        if (x == 0 || y == 0)
          c = 2;
        else if (h < 20)
          c = 3;
      } else if (ch == 'c') {
        c = 4;
        if ((x & 7) == 0 || (y & 7) == 0)
          c = 3;
        else if (h < 10)
          c = 5;
      } else if (ch == 't') {
        c = 5;
        if ((x & 3) == 0 || (y & 3) == 0)
          c = 4;
      } else if (ch == 'h') {
        c = 3;
        if ((((wx >> 4) + (wy >> 4)) & 1) == 0)
          c = 4;
        if (x == 0 || y == 0)
          c = 2;
      } else if (ch == '_') {
        c = 2;
        if (h < 30)
          c = 3;
        else if (h > 240)
          c = 1;
      } else if (ch == ';') {
        c = 14;
        if (h < 40)
          c = 13;
        else if (h > 252)
          c = 15;
      } else if (ch == '-') {
        /* the glossy orange floor of the real corridors */
        c = 12;
        if (((wx + wy) & 1) == 0)
          c = 15;
        if (h > 250)
          c = 6;
        if (x == 0 && (wy & 63) < 2)
          c = 10;
      } else if (ch == 'r') {
        /* red brick paving */
        c = 10;
        if ((y & 3) == 3)
          c = 7;
        else if (((wx + ((wy >> 2) & 1) * 4) & 7) == 0)
          c = 7;
        else if (h < 20)
          c = 12;
        else if (h > 240)
          c = 8;
      } else {
        c = 3;
        if (h < 24)
          c = 2;
        else if (h > 245)
          c = 4;
        if (x == 0 || y == 0)
          c = 2;
      }
      sa_world[wy * WORLD_W + wx] = c;
    }
}

U0 SaDrawWater(I64 px, I64 py, I64 ty)
{
  I64 x, y, c, h, wx, wy;
  for (y = 0; y < 16; y++)
    for (x = 0; x < 16; x++) {
      wx = px + x;
      wy = py + y;
      h = SaHash(wx, wy * 3);
      c = 13;
      if (((wy + (wx >> 3) * 3) % 9) == 0)
        c = 4;
      else if (h < 10)
        c = 5;
      else if (h < 36)
        c = 3;
      if ((ty == 4 || ty == 7) && y == 8) {
        c = 6;
        if (((wx >> 1) & 1) == 0)
          c = 12;
      }
      sa_world[wy * WORLD_W + wx] = c;
    }
}

/* Luftraum Forum: you look down through the glass into the forum with
 * its rows of orange seats - the deeper, the darker. */
U0 SaDrawVoid(I64 px, I64 py)
{
  I64 x, y, c, wx, wy, seat, deep;
  deep = 0;
  if (sa_floor == 3)
    deep = 1;
  if (sa_floor >= 4)
    deep = 2;
  for (y = 0; y < 16; y++)
    for (x = 0; x < 16; x++) {
      wx = px + x;
      wy = py + y;
      c = 1;
      if (((wy / 3) % 3) == 0) {
        seat = (wx / 3) % 5;
        if ((wx % 112) < 12)
          c = 3;
        else if (seat != 4) {
          c = 10;
          if (deep == 1)
            c = 8;
          if (deep == 2)
            c = 7;
        }
      } else if (((wy / 3) % 3) == 1 && (wx % 112) >= 12)
        c = 2;
      if (deep == 2 && ((wx + wy * 2) % 61) == 0)
        c = 11;
      sa_world[wy * WORLD_W + wx] = c;
    }
}

/* The stairs run along the side walls of the staircase: up on the left,
 * down on the right, so you only take them when you walk into them. A
 * floor that is still locked gets a grille with a padlock. */
U0 SaDrawStairs(I64 ch, I64 px, I64 py, I64 tx, I64 ty)
{
  I64 x, c, mid, open;
  for (x = 0; x < 16; x++) {
    c = 5 - x / 4;
    if ((x & 3) == 3)
      c = 1;
    SaWRect(px + x, py, 1, 16, c);
  }
  if (SaTile(tx, ty - 1) != ch)
    SaWRect(px, py, 16, 2, 7);
  if (SaTile(tx, ty + 1) != ch)
    SaWRect(px, py + 14, 16, 2, 7);
  mid = FALSE;
  if (SaTile(tx, ty - 1) == ch && SaTile(tx, ty + 1) == ch)
    mid = TRUE;
  if (ch == '^')
    open = SaStairOpen(TRUE);
  else
    open = SaStairOpen(FALSE);
  if (mid && open) {
    /* a painted arrow pointing into the stairs */
    for (x = 0; x < 4; x++)
      if (ch == '^')
        SaWRect(px + 4 + x, py + 7 - x, 1, 2 + x * 2, 6);
      else
        SaWRect(px + 11 - x, py + 7 - x, 1, 2 + x * 2, 6);
  }
  if (!open) {
    for (x = 1; x < 16; x = x + 3)
      SaWRect(px + x, py, 1, 16, 4);
    SaWRect(px, py + 3, 16, 1, 5);
    SaWRect(px, py + 12, 16, 1, 5);
    if (mid) {
      SaWRect(px + 6, py + 3, 4, 1, 4);
      SaWRect(px + 5, py + 4, 1, 3, 4);
      SaWRect(px + 10, py + 4, 1, 3, 4);
      SaWRect(px + 4, py + 7, 8, 6, 15);
      SaWRect(px + 4, py + 12, 8, 1, 8);
      SaWRect(px + 7, py + 9, 2, 2, 1);
    }
  }
}

U0 SaDrawWallFace(I64 ch, I64 px, I64 py, I64 tx, I64 ty)
{
  I64 x, y, c;
  for (y = 0; y < 16; y++)
    for (x = 0; x < 16; x++) {
      c = 5;
      if (sa_floor == F_OUTSIDE) {
        c = 10;
        if ((y % 4) == 3)
          c = 7;
        else if (((x + ((y / 4) % 2) * 4) % 8) == 0)
          c = 7;
      } else if (SaHash(px + x, py + y) < 20)
        c = 4;
      if (y == 0)
        c = 1;
      else if (y < 3)
        c = 2;
      else if (y == 3)
        c = 4;
      else if (y == 14)
        c = 8;
      else if (y == 15)
        c = 7;
      else if (y == 13 && sa_floor != F_OUTSIDE)
        c = 4;
      sa_world[(py + y) * WORLD_W + px + x] = c;
    }
  if (ch == '#' && sa_floor != F_OUTSIDE && SaTile(tx, ty + 1) == '-') {
    /* concrete boards */
    for (x = 0; x < 16; x++)
      if (((px + x) % 11) == 0)
        SaWRect(px + x, py + 4, 1, 9, 4);
    /* the orange band with waves, as painted in the real corridors */
    for (x = 0; x < 16; x++) {
      y = 5 + ((((px + x) / 5) & 3) == 1);
      if ((((px + x) / 5) & 3) == 3)
        y = 4;
      for (c = y; c < y + 4; c++)
        if (((px + x + c) & 1) == 0)
          sa_world[(py + c) * WORLD_W + px + x] = 12;
        else
          sa_world[(py + c) * WORLD_W + px + x] = 15;
    }
    if ((tx % 9) == 4) {
      SaWRect(px + 3, py + 3, 10, 9, 6);
      SaWRect(px + 3, py + 3, 10, 1, 5);
      sa_num_buf[0] = 0;
      SaStrNum(sa_num_buf, sa_floor + 1);
      SaDrawText(sa_world, WORLD_W, WORLD_H, px + 6, py + 4, sa_num_buf, 1, 1);
    }
  }
  if (ch == 'b') {
    SaWRect(px, py + 3, 16, 10, 7);
    SaWRect(px, py + 4, 16, 8, 13);
    if (SaTile(tx - 1, ty) != 'b')
      SaWRect(px, py + 3, 1, 10, 7);
    if (SaTile(tx + 1, ty) != 'b')
      SaWRect(px + 15, py + 3, 1, 10, 7);
    for (y = 5; y < 11; y++)
      for (x = 1; x < 15; x++)
        if (SaHash(px + x, py + y * 3) < 6)
          sa_world[(py + y) * WORLD_W + px + x] = 5;
    SaWRect(px, py + 12, 16, 1, 4);
  } else if (ch == 'w') {
    SaWRect(px + 2, py + 3, 12, 10, 4);
    if (sa_floor == F_OUTSIDE)
      SaWRect(px + 3, py + 4, 10, 8, 15);
    else {
      SaWRect(px + 3, py + 4, 10, 8, 1);
      for (x = 0; x < 6; x++)
        SaWRect(px + 4 + x, py + 10 - x, 1, 1, 3);
    }
    SaWRect(px + 7, py + 4, 2, 8, 4);
    SaWRect(px + 3, py + 7, 10, 1, 4);
  } else if (ch == 'p') {
    SaWRect(px + 2, py + 4, 12, 8, 6);
    for (y = 0; y < 3; y++)
      for (x = 0; x < 5; x++) {
        c = SaHash(px + x, py + y) & 3;
        if (c == 0)
          c = 12;
        else if (c == 1)
          c = 14;
        else if (c == 2)
          c = 15;
        else
          c = 4;
        if (!(y == 0 && x > 0 && x < 4))
          SaWRect(px + 3 + x * 2, py + 5 + y * 2, 1, 1, c);
      }
  } else if (ch == 'n') {
    SaWRect(px + 1, py + 4, 14, 8, 7);
    SaWRect(px + 2, py + 5, 12, 6, 8);
    SaWRect(px + 3, py + 6, 3, 4, 6);
    SaWRect(px + 7, py + 5, 4, 3, 6);
    SaWRect(px + 11, py + 7, 2, 3, 15);
    SaWRect(px + 4, py + 6, 1, 1, 12);
    SaWRect(px + 9, py + 5, 1, 1, 12);
  } else if (ch == 'y') {
    SaWRect(px + 1, py + 3, 2, 11, 8);
    SaWRect(px + 13, py + 3, 2, 11, 8);
    for (y = 4; y < 14; y = y + 3)
      SaWRect(px + 1, py + y, 14, 1, 7);
  } else if (ch == 'j') {
    SaWRect(px + 2, py + 3, 12, 8, 6);
    SaWRect(px + 3, py + 4, 10, 6, 5);
    SaWRect(px + 5, py + 6, 6, 3, 12);
    SaWRect(px + 6, py + 7, 4, 1, 5);
    SaWRect(px + 4, py + 11, 8, 1, 12);
    for (x = 0; x < 4; x++)
      SaWRect(px + 5 + x * 2, py + 12, 1, 2, 6);
  } else if (ch == 'u') {
    /* the fuse box: a lightning bolt and a lamp, red without power */
    SaWRect(px + 2, py + 3, 12, 10, 2);
    SaWRect(px + 3, py + 4, 10, 8, 4);
    SaWRect(px + 8, py + 5, 1, 1, 15);
    SaWRect(px + 7, py + 6, 1, 1, 15);
    SaWRect(px + 6, py + 7, 3, 1, 15);
    SaWRect(px + 7, py + 8, 1, 1, 15);
    SaWRect(px + 6, py + 9, 1, 1, 15);
    c = 12;
    if (sa_power)
      c = 14;
    SaWRect(px + 10, py + 10, 2, 1, c);
  } else if (ch == 'q') {
    SaWRect(px, py + 3, 16, 11, 1);
    for (y = 3; y < 14; y++)
      for (x = 1; x < 15; x++)
        if (((x - y + 16) % 9) == 0)
          sa_world[(py + y) * WORLD_W + px + x] = 3;
    SaWRect(px + 3, py + 5, 1, 1, 5);
    SaWRect(px, py + 3, 1, 11, 4);
    SaWRect(px + 15, py + 3, 1, 11, 4);
  }
}

U0 SaDrawWallTop(I64 px, I64 py, I64 tx, I64 ty)
{
  SaWRect(px, py, 16, 16, 1);
  if (!SaIsWallCh(SaTile(tx - 1, ty)))
    SaWRect(px, py, 1, 16, 3);
  if (!SaIsWallCh(SaTile(tx + 1, ty)))
    SaWRect(px + 15, py, 1, 16, 3);
  if (!SaIsWallCh(SaTile(tx, ty - 1)))
    SaWRect(px, py, 16, 1, 3);
}

U0 SaDrawFence(I64 px, I64 py, I64 tx, I64 ty)
{
  I64 x;
  SaDrawFloor(SaFloorOf(tx, ty), px, py);
  for (x = 1; x < 16; x = x + 3)
    SaWRect(px + x, py + 1, 1, 14, 2);
  SaWRect(px, py + 2, 16, 1, 3);
  SaWRect(px, py + 12, 16, 1, 3);
}

U0 SaDrawDoor(I64 px, I64 py, I64 tx, I64 ty)
{
  SaDrawFloor(SaFloorOf(tx, ty), px, py);
  if (SaIsWallCh(SaTile(tx, ty - 1)) && SaIsWallCh(SaTile(tx, ty + 1))) {
    SaWRect(px, py, 16, 2, 7);
    SaWRect(px, py + 14, 16, 2, 7);
  } else {
    SaWRect(px + 2, py + 3, 12, 3, 1);
    SaWRect(px, py, 16, 3, 7);
    SaWRect(px, py, 16, 1, 1);
    if (SaTile(tx - 1, ty) != 'D') {
      SaWRect(px, py, 2, 16, 7);
      SaWRect(px + 1, py, 1, 16, 8);
    }
    if (SaTile(tx + 1, ty) != 'D')
      SaWRect(px + 14, py, 2, 16, 7);
  }
}

U0 SaDrawObject(I64 ch, I64 px, I64 py, I64 tx, I64 ty)
{
  I64 h, art;
  SaDrawFloor(SaFloorOf(tx, ty), px, py);
  art = -1;
  if (ch == 'L')
    art = ART_LOCKER;
  else if (ch == 'B')
    art = ART_BENCH;
  else if (ch == 'S')
    art = ART_SINK;
  else if (ch == 'G')
    art = ART_CABINET;
  else if (ch == 'T')
    art = ART_DESK;
  else if (ch == 'R')
    art = ART_SHELF;
  else if (ch == 'O')
    art = ART_PLANT;
  else if (ch == 'Z')
    art = ART_SKEL;
  else if (ch == 'M')
    art = ART_PIANO;
  else if (ch == 'U')
    art = ART_TOILET;
  else if (ch == 'V')
    art = ART_VEND;
  else if (ch == 'E')
    art = ART_EXIT;
  else if (ch == 'Q')
    art = ART_DRUMS;
  else if (ch == 'J')
    art = ART_EASEL;
  else if (ch == 'o')
    art = ART_BOX;
  else if (ch == 'N')
    art = ART_BENCH2;
  else if (ch == 'Y')
    art = ART_TREE;
  else if (ch == 'k')
    art = ART_COUNTER;
  else if (ch == 'I')
    art = ART_TROPHY;
  else if (ch == 'W')
    art = ART_PHONEDESK;
  else if (ch == 'g')
    art = ART_GATE;
  else if (ch == 'X')
    art = ART_LOCKDOOR;
  else if (ch == 'e')
    art = ART_YARDGATE;
  if (art >= 0)
    SaDrawArt(sa_world, WORLD_W, WORLD_H, px, py, art, 16, 16, FALSE, 1, 1);
  h = SaHash(tx * 7, ty * 13);
  if (ch == 'B') {
    if ((h & 3) == 0)
      SaDrawArt(sa_world, WORLD_W, WORLD_H, px + 4, py, ART_FLASK, 8, 8, FALSE, 1, 1);
    else if ((h & 3) == 1)
      SaDrawArt(sa_world, WORLD_W, WORLD_H, px + 4, py, ART_TUBES, 8, 8, FALSE, 1, 1);
    else if ((h & 3) == 2)
      SaDrawArt(sa_world, WORLD_W, WORLD_H, px + 4, py, ART_BURNER, 8, 8, FALSE, 1, 1);
  }
  if (ch == 'T') {
    if (h < 110)
      SaWRect(px + 3 + (h & 7), py + 5, 4, 3, 6);
    else if (h < 160)
      SaWRect(px + 2 + (h & 7), py + 5, 5, 3, 12 + (h & 1) * 2);
  }
}

U0 SaWorldLabel(I64 x, I64 y, CH *s, I64 c, I64 bg)
{
  if (bg >= 0)
    SaWRect(x - 2, y - 2, SaTextW(s, 1) + 4, 11, bg);
  SaDrawText(sa_world, WORLD_W, WORLD_H, x, y, s, c, 1);
}

/* floor specific decorations: court lines, lamps, the floor plaque */
U0 SaFloorDecor()
{
  I64 i, c, x, y;
  if (sa_floor == 1) {
    SaWRect(40, 24, 640, 1, 6);
    SaWRect(40, 175, 640, 1, 6);
    SaWRect(40, 24, 1, 152, 6);
    SaWRect(679, 24, 1, 152, 6);
    SaWRect(359, 24, 1, 152, 6);
    for (y = -22; y <= 22; y++)
      for (x = -22; x <= 22; x++)
        if (x * x + y * y <= 484 && x * x + y * y >= 441)
          SaWRect(359 + x, 100 + y, 1, 1, 6);
  }
  if (sa_floor == F_OUTSIDE) {
    /* painted parking spaces for bikes and scooters, and the street lamp */
    for (i = 0; i < 6; i++)
      SaWRect(560 + i * 24, 404, 1, 22, 6);
    SaWRect(560, 425, 121, 1, 6);
    x = sa_lamp_x[F_OUTSIDE];
    y = sa_lamp_y[F_OUTSIDE];
    SaWRect(x - 1, y - 18, 3, 26, 2);
    SaWRect(x - 5, y - 22, 11, 4, 3);
    SaWRect(x - 3, y - 18, 7, 1, 15);
  }
  if (sa_floor == 3) {
    x = sa_lamp_x[3];
    y = sa_lamp_y[3];
    SaWRect(x - 5, y - 6, 10, 3, 14);
    SaWRect(x - 4, y - 7, 8, 1, 13);
    SaWRect(x - 4, y - 3, 8, 1, 15);
    SaWRect(x, y - 3, 1, 4, 4);
    SaWRect(x - 3, y + 1, 7, 1, 2);
  }
  /* the painted floor number at the staircase */
  SaWRect(65, 274, 30, 11, 1);
  for (y = 0; y < 9; y++)
    for (x = 0; x < 28; x++) {
      c = sa_sign_c1[sa_floor];
      if (sa_sign_dith[sa_floor] != 0 && ((x + y) & 1) == 0)
        c = sa_sign_c2[sa_floor];
      SaWRect(66 + x, 275 + y, 1, 1, c);
    }
  sa_num_buf[0] = 0;
  SaStrNum(sa_num_buf, sa_floor + 1);
  SaDrawText(sa_world, WORLD_W, WORLD_H, 77, 276, sa_num_buf, 1, 1);
  /* staircase labels */
  if (sa_floor < F_TOP)
    SaWorldLabel(19, 291, "< HOCH", 5, -1);
  if (sa_floor > 0)
    SaWorldLabel(62, 339, "RUNTER >", 5, -1);
  /* label 0 is the GESPERRT sign on the swimming hall door */
  for (i = 0; i < NLABELS; i++)
    if (sa_lb_f[i] == sa_floor && !(i == 0 && sa_pool_open))
      SaWorldLabel(sa_lb_x[i], sa_lb_y[i], sa_lb_text[i], sa_lb_c[i], sa_lb_bg[i]);
}

U0 SaRenderWorld()
{
  I64 tx, ty, ch, below, px, py;
  for (ty = 0; ty < MAP_H; ty++)
    for (tx = 0; tx < MAP_W; tx++) {
      ch = sa_map[ty * MAP_W + tx];
      px = tx * 16;
      py = ty * 16;
      if (ch == 'F')
        SaDrawFence(px, py, tx, ty);
      else if (SaIsWallCh(ch)) {
        below = SaTile(tx, ty + 1);
        if ((SaIsWallCh(below) && below != 'F') || below == 'E' || below == 'X')
          SaDrawWallTop(px, py, tx, ty);
        else
          SaDrawWallFace(ch, px, py, tx, ty);
      } else if (ch == 'D')
        SaDrawDoor(px, py, tx, ty);
      else if (ch == '^' || ch == 'v')
        SaDrawStairs(ch, px, py, tx, ty);
      else if (ch == '~')
        SaDrawWater(px, py, ty);
      else if (ch == 'A')
        SaDrawVoid(px, py);
      else if (SaIsFloorCh(ch))
        SaDrawFloor(ch, px, py);
      else
        SaDrawObject(ch, px, py, tx, ty);
    }
  SaFloorDecor();
}

/* copies floor f into sa_map (and makes it the current floor) without
 * drawing it; an opened swimming hall gets real doors */
U0 SaLoadMap(I64 f)
{
  I64 tx, ty, ch;
  CH *row;
  sa_floor = f;
  for (ty = 0; ty < MAP_H; ty++) {
    row = sa_floor_src[f * MAP_H + ty];
    for (tx = 0; tx < MAP_W; tx++) {
      ch = row[tx];
      if (ch == 'X' && sa_pool_open)
        ch = 'D';
      sa_map[ty * MAP_W + tx] = ch;
    }
  }
  for (ty = 0; ty < MAP_H; ty++)
    for (tx = 0; tx < MAP_W; tx++) {
      ch = sa_map[ty * MAP_W + tx];
      if (ch == 'K' || ch == 'P' || ch == 'H')
        sa_map[ty * MAP_W + tx] = SaFloorOf(tx, ty);
    }
  for (ty = 0; ty < NENT; ty++)
    sa_ebfs[ty] = -1;
}

U0 SaLoadFloor(I64 f)
{
  SaLoadMap(f);
  SaRenderWorld();
}

/* ------------------------------------------------------------------ */
/* characters                                                          */
/* ------------------------------------------------------------------ */

U0 SaRectS(U8 *dst, I64 dw, I64 dh, I64 x0, I64 y0, I64 rx, I64 ry, I64 w,
           I64 h, I64 c, I64 s, I64 flip)
{
  if (flip)
    rx = -rx - w;
  SaFillRect(dst, dw, dh, x0 + rx * s, y0 + ry * s, w * s, h * s, c);
}

/* Grown-ups, about 27 pixels tall. style 0 Herr Thüner (bald, lab coat),
 * 1 Herr Kirschbaum (plaid shirt, watch), 2 Herr Lersch (dark blue
 * button-up shirt), 3 Frau Niehaus (sage green turtleneck, long hair over
 * one shoulder), 4 a cleaner (blue uniform and cap). */
U0 SaDrawAdult(U8 *d, I64 dw, I64 dh, I64 x0, I64 y0, I64 dir, I64 frame,
               I64 eyec, I64 s, I64 style)
{
  I64 la, lb, aa, ab, st, ax, fl, head, body, shade, arm, legc;
  la = 0;
  lb = 0;
  aa = 0;
  ab = 0;
  st = 0;
  ax = 0;
  if (frame == 1) {
    la = 1;
    ab = 1;
    st = 2;
    ax = -1;
  }
  if (frame == 3) {
    lb = 1;
    aa = 1;
    st = -2;
    ax = 1;
  }
  fl = FALSE;
  if (dir == DIR_LEFT)
    fl = TRUE;
  body = 6;
  shade = 5;
  arm = 5;
  legc = 2;
  head = ART_THU_D;
  if (style == 1) {
    body = 11;
    shade = 13;
    arm = 11;
    head = ART_KB_D;
  }
  if (style == 2) {
    body = 13;
    shade = 2;
    arm = 13;
    legc = 3;
    head = ART_LE_D;
  }
  if (style == 3) {
    body = 14;
    shade = 13;
    arm = 14;
    legc = 2;
    head = ART_NI_D;
  }
  if (style == 4) {
    body = 13;
    shade = 2;
    arm = 13;
    legc = 13;
    head = ART_CL_D;
  }
  SaRectS(d, dw, dh, x0, y0, -5, -1, 10, 2, 1, s, FALSE);
  if (dir == DIR_DOWN || dir == DIR_UP) {
    SaRectS(d, dw, dh, x0, y0, -4, -7 - la, 3, 5, legc, s, FALSE);
    SaRectS(d, dw, dh, x0, y0, 1, -7 - lb, 3, 5, legc, s, FALSE);
    SaRectS(d, dw, dh, x0, y0, -4, -2 - la, 3, 2, 1, s, FALSE);
    SaRectS(d, dw, dh, x0, y0, 1, -2 - lb, 3, 2, 1, s, FALSE);
    SaRectS(d, dw, dh, x0, y0, -7, -16 + aa, 2, 8, arm, s, FALSE);
    SaRectS(d, dw, dh, x0, y0, 5, -16 + ab, 2, 8, arm, s, FALSE);
    SaRectS(d, dw, dh, x0, y0, -7, -8 + aa, 2, 2, 9, s, FALSE);
    SaRectS(d, dw, dh, x0, y0, 5, -8 + ab, 2, 2, 9, s, FALSE);
    SaRectS(d, dw, dh, x0, y0, -5, -17, 10, 11, body, s, FALSE);
    SaRectS(d, dw, dh, x0, y0, 4, -17, 1, 11, shade, s, FALSE);
    if (style == 0)
      SaRectS(d, dw, dh, x0, y0, -5, -7, 10, 1, 5, s, FALSE);
    else
      SaRectS(d, dw, dh, x0, y0, -5, -9, 10, 3, legc, s, FALSE);
    if (style == 1) {
      SaRectS(d, dw, dh, x0, y0, -3, -17, 1, 8, 13, s, FALSE);
      SaRectS(d, dw, dh, x0, y0, 1, -17, 1, 8, 13, s, FALSE);
      SaRectS(d, dw, dh, x0, y0, -5, -15, 10, 1, 4, s, FALSE);
      SaRectS(d, dw, dh, x0, y0, -5, -12, 10, 1, 4, s, FALSE);
      SaRectS(d, dw, dh, x0, y0, -7, -13 + aa, 2, 1, 13, s, FALSE);
      SaRectS(d, dw, dh, x0, y0, 5, -13 + ab, 2, 1, 13, s, FALSE);
    }
    if (dir == DIR_UP)
      head = head + 10;
    else {
      if (style == 0) {
        SaRectS(d, dw, dh, x0, y0, -1, -17, 2, 8, 5, s, FALSE);
        SaRectS(d, dw, dh, x0, y0, -1, -16, 1, 6, 11, s, FALSE);
        SaRectS(d, dw, dh, x0, y0, 2, -15, 1, 2, 12, s, FALSE);
      }
      if (style == 1)
        SaRectS(d, dw, dh, x0, y0, 5, -9 + ab, 2, 1, 5, s, FALSE);
      if (style == 2) {
        /* collar and the buttons of the shirt */
        SaRectS(d, dw, dh, x0, y0, -2, -17, 4, 1, 6, s, FALSE);
        SaRectS(d, dw, dh, x0, y0, 0, -15, 1, 1, 5, s, FALSE);
        SaRectS(d, dw, dh, x0, y0, 0, -13, 1, 1, 5, s, FALSE);
        SaRectS(d, dw, dh, x0, y0, 0, -11, 1, 1, 5, s, FALSE);
      }
      if (style == 4) {
        /* the cleaning company's logo */
        SaRectS(d, dw, dh, x0, y0, -3, -15, 2, 2, 6, s, FALSE);
        SaRectS(d, dw, dh, x0, y0, -5, -10, 10, 1, 2, s, FALSE);
      }
    }
    SaDrawArt(d, dw, dh, x0 - 5 * s, y0 - 27 * s, head, 10, 10, FALSE, s, eyec);
    if (style == 3) {
      /* turtleneck, and the long wavy hair */
      if (dir == DIR_UP)
        SaRectS(d, dw, dh, x0, y0, -4, -18, 8, 8, 8, s, FALSE);
      else {
        SaRectS(d, dw, dh, x0, y0, -2, -18, 4, 2, 14, s, FALSE);
        SaRectS(d, dw, dh, x0, y0, -5, -19, 2, 9, 8, s, FALSE);
        SaRectS(d, dw, dh, x0, y0, -4, -16, 1, 1, 10, s, FALSE);
        SaRectS(d, dw, dh, x0, y0, -5, -13, 1, 1, 10, s, FALSE);
        SaRectS(d, dw, dh, x0, y0, -4, -11, 1, 1, 10, s, FALSE);
      }
    }
  } else {
    SaRectS(d, dw, dh, x0, y0, -2 - st, -7, 3, 5, 1, s, fl);
    SaRectS(d, dw, dh, x0, y0, -2 - st, -2, 4, 2, 1, s, fl);
    SaRectS(d, dw, dh, x0, y0, -2 + st, -7, 3, 5, legc, s, fl);
    SaRectS(d, dw, dh, x0, y0, -2 + st, -2, 4, 2, 1, s, fl);
    SaRectS(d, dw, dh, x0, y0, -4, -17, 8, 11, body, s, fl);
    SaRectS(d, dw, dh, x0, y0, -4, -17, 1, 11, shade, s, fl);
    if (style == 0)
      SaRectS(d, dw, dh, x0, y0, -4, -7, 8, 1, 5, s, fl);
    else
      SaRectS(d, dw, dh, x0, y0, -4, -9, 8, 3, legc, s, fl);
    if (style == 1)
      SaRectS(d, dw, dh, x0, y0, -4, -14, 8, 1, 4, s, fl);
    SaRectS(d, dw, dh, x0, y0, -1 + ax, -16, 3, 8, arm, s, fl);
    SaRectS(d, dw, dh, x0, y0, -1 + ax, -8, 3, 2, 9, s, fl);
    SaDrawArt(d, dw, dh, x0 - 5 * s, y0 - 27 * s, head + 20, 10, 10, fl, s, eyec);
    if (style == 3) {
      SaRectS(d, dw, dh, x0, y0, -5, -18, 3, 7, 8, s, fl);
      SaRectS(d, dw, dh, x0, y0, -4, -15, 1, 1, 10, s, fl);
      SaRectS(d, dw, dh, x0, y0, -1, -18, 3, 2, 14, s, fl);
    }
  }
}

/* the mop (and the bucket while mopping) of a cleaner */
U0 SaDrawMop(I64 x, I64 y, I64 dir, I64 mopping, I64 frame)
{
  I64 i, hx, mx, sw;
  sw = 0;
  if (mopping)
    sw = ((frame / 5) & 1) * 4 - 2;
  hx = 6;
  if (dir == DIR_LEFT)
    hx = -7;
  mx = hx + 4 + sw;
  if (dir == DIR_LEFT)
    mx = hx - 4 + sw;
  for (i = 0; i < 11; i++)
    SaPutFb(x + hx + (mx - hx) * i / 10, y - 11 + i, 8);
  SaRectFb(x + mx - 3, y - 1, 7, 2, 5);
  SaRectFb(x + mx - 3, y + 1, 7, 1, 4);
  for (i = 0; i < 7; i = i + 2)
    SaPutFb(x + mx - 3 + i, y + 2, 4);
  if (mopping) {
    i = -14;
    if (dir == DIR_LEFT)
      i = 9;
    SaRectFb(x + i, y - 6, 6, 6, 15);
    SaRectFb(x + i, y - 6, 6, 1, 13);
    SaRectFb(x + i + 1, y - 7, 4, 1, 4);
  }
}

/* Conjoke: big, strong, shirtless swimmer, about 36 pixels tall */
U0 SaDrawConjoke(U8 *d, I64 dw, I64 dh, I64 x0, I64 y0, I64 dir, I64 frame, I64 s)
{
  I64 la, lb, aa, ab, st, ax, fl;
  la = 0;
  lb = 0;
  aa = 0;
  ab = 0;
  st = 0;
  ax = 0;
  if (frame == 1) {
    la = 2;
    ab = 2;
    st = 3;
    ax = -2;
  }
  if (frame == 3) {
    lb = 2;
    aa = 2;
    st = -3;
    ax = 2;
  }
  fl = FALSE;
  if (dir == DIR_LEFT)
    fl = TRUE;
  SaRectS(d, dw, dh, x0, y0, -7, -1, 14, 2, 1, s, FALSE);
  if (dir == DIR_DOWN || dir == DIR_UP) {
    SaRectS(d, dw, dh, x0, y0, -4, -10 - la, 3, 9, 9, s, FALSE);
    SaRectS(d, dw, dh, x0, y0, 1, -10 - lb, 3, 9, 9, s, FALSE);
    SaRectS(d, dw, dh, x0, y0, -4, -10 - la, 1, 9, 10, s, FALSE);
    SaRectS(d, dw, dh, x0, y0, 3, -10 - lb, 1, 9, 10, s, FALSE);
    SaRectS(d, dw, dh, x0, y0, -4, -2 - la, 3, 2, 10, s, FALSE);
    SaRectS(d, dw, dh, x0, y0, 1, -2 - lb, 3, 2, 10, s, FALSE);
    SaRectS(d, dw, dh, x0, y0, -5, -15, 10, 5, 1, s, FALSE);
    SaRectS(d, dw, dh, x0, y0, -5, -15, 10, 1, 13, s, FALSE);
    SaRectS(d, dw, dh, x0, y0, -10, -27 + aa, 4, 12, 9, s, FALSE);
    SaRectS(d, dw, dh, x0, y0, 6, -27 + ab, 4, 12, 9, s, FALSE);
    SaRectS(d, dw, dh, x0, y0, -10, -27 + aa, 1, 12, 10, s, FALSE);
    SaRectS(d, dw, dh, x0, y0, 9, -27 + ab, 1, 12, 10, s, FALSE);
    SaRectS(d, dw, dh, x0, y0, -10, -16 + aa, 4, 2, 10, s, FALSE);
    SaRectS(d, dw, dh, x0, y0, 6, -16 + ab, 4, 2, 10, s, FALSE);
    SaRectS(d, dw, dh, x0, y0, -6, -27, 12, 12, 9, s, FALSE);
    SaRectS(d, dw, dh, x0, y0, -6, -27, 1, 12, 10, s, FALSE);
    SaRectS(d, dw, dh, x0, y0, 5, -27, 1, 12, 10, s, FALSE);
    if (dir == DIR_DOWN) {
      SaRectS(d, dw, dh, x0, y0, -4, -23, 3, 1, 10, s, FALSE);
      SaRectS(d, dw, dh, x0, y0, 1, -23, 3, 1, 10, s, FALSE);
      SaRectS(d, dw, dh, x0, y0, -1, -21, 2, 1, 10, s, FALSE);
      SaRectS(d, dw, dh, x0, y0, -1, -19, 2, 1, 10, s, FALSE);
      SaRectS(d, dw, dh, x0, y0, -1, -17, 2, 1, 10, s, FALSE);
      SaDrawArt(d, dw, dh, x0 - 5 * s, y0 - 37 * s, ART_CJ_D, 10, 10, FALSE, s, 1);
    } else
      SaDrawArt(d, dw, dh, x0 - 5 * s, y0 - 37 * s, ART_CJ_U, 10, 10, FALSE, s, 1);
  } else {
    SaRectS(d, dw, dh, x0, y0, -2 - st, -10, 4, 9, 10, s, fl);
    SaRectS(d, dw, dh, x0, y0, -2 + st, -10, 4, 9, 9, s, fl);
    SaRectS(d, dw, dh, x0, y0, -2 - st, -2, 5, 2, 10, s, fl);
    SaRectS(d, dw, dh, x0, y0, -2 + st, -2, 5, 2, 10, s, fl);
    SaRectS(d, dw, dh, x0, y0, -4, -15, 8, 5, 1, s, fl);
    SaRectS(d, dw, dh, x0, y0, -4, -15, 8, 1, 13, s, fl);
    SaRectS(d, dw, dh, x0, y0, -4, -27, 9, 12, 9, s, fl);
    SaRectS(d, dw, dh, x0, y0, -4, -27, 1, 12, 10, s, fl);
    SaRectS(d, dw, dh, x0, y0, 3, -24, 2, 1, 10, s, fl);
    SaRectS(d, dw, dh, x0, y0, -2 + ax, -26, 4, 12, 9, s, fl);
    SaRectS(d, dw, dh, x0, y0, -2 + ax, -26, 1, 12, 10, s, fl);
    SaRectS(d, dw, dh, x0, y0, -2 + ax, -15, 4, 2, 10, s, fl);
    SaDrawArt(d, dw, dh, x0 - 5 * s, y0 - 37 * s, ART_CJ_S, 10, 10, fl, s, 1);
  }
}

/* The player: a student who happens to be 8 metres tall (47 px, the walls
 * are 16 px). crouch (0..12) squashes the legs when ducking through doors. */
U0 SaDrawGiant(U8 *d, I64 dw, I64 dh, I64 x0, I64 y0, I64 dir, I64 frame,
               I64 crouch, I64 s)
{
  I64 la, lb, aa, ab, st, ax, fl, leg, top, t0, head;
  la = 0;
  lb = 0;
  aa = 0;
  ab = 0;
  st = 0;
  ax = 0;
  if (frame == 1) {
    la = 2;
    ab = 2;
    st = 3;
    ax = -1;
  }
  if (frame == 3) {
    lb = 2;
    aa = 2;
    st = -3;
    ax = 1;
  }
  fl = FALSE;
  if (dir == DIR_LEFT)
    fl = TRUE;
  leg = 18 - crouch;
  top = -3 - leg;
  t0 = top - 15;
  SaRectS(d, dw, dh, x0, y0, -7, -1, 14, 2, 1, s, FALSE);
  if (dir == DIR_DOWN || dir == DIR_UP) {
    SaRectS(d, dw, dh, x0, y0, -5, top - la, 4, leg, 1, s, FALSE);
    SaRectS(d, dw, dh, x0, y0, -5, top - la, 1, leg, 3, s, FALSE);
    SaRectS(d, dw, dh, x0, y0, 1, top - lb, 4, leg, 1, s, FALSE);
    SaRectS(d, dw, dh, x0, y0, 4, top - lb, 1, leg, 3, s, FALSE);
    SaRectS(d, dw, dh, x0, y0, -6, -3 - la, 5, 2, 6, s, FALSE);
    SaRectS(d, dw, dh, x0, y0, -6, -1 - la, 5, 1, 4, s, FALSE);
    SaRectS(d, dw, dh, x0, y0, 1, -3 - lb, 5, 2, 6, s, FALSE);
    SaRectS(d, dw, dh, x0, y0, 1, -1 - lb, 5, 1, 4, s, FALSE);
    SaRectS(d, dw, dh, x0, y0, -9, t0 + 1 + aa, 3, 13, 12, s, FALSE);
    SaRectS(d, dw, dh, x0, y0, -9, t0 + 1 + aa, 1, 13, 11, s, FALSE);
    SaRectS(d, dw, dh, x0, y0, -9, t0 + 14 + aa, 3, 2, 9, s, FALSE);
    SaRectS(d, dw, dh, x0, y0, 6, t0 + 1 + ab, 3, 13, 12, s, FALSE);
    SaRectS(d, dw, dh, x0, y0, 8, t0 + 1 + ab, 1, 13, 11, s, FALSE);
    SaRectS(d, dw, dh, x0, y0, 6, t0 + 14 + ab, 3, 2, 9, s, FALSE);
    SaRectS(d, dw, dh, x0, y0, -6, t0, 12, 15, 12, s, FALSE);
    SaRectS(d, dw, dh, x0, y0, 5, t0, 1, 15, 11, s, FALSE);
    SaRectS(d, dw, dh, x0, y0, -6, top - 2, 12, 2, 11, s, FALSE);
    SaRectS(d, dw, dh, x0, y0, -6, t0 - 3, 12, 4, 11, s, FALSE);
    head = ART_PL_U;
    if (dir == DIR_DOWN) {
      head = ART_PL_D;
      SaRectS(d, dw, dh, x0, y0, -3, top - 7, 6, 3, 11, s, FALSE);
      SaRectS(d, dw, dh, x0, y0, -2, t0 + 1, 1, 4, 6, s, FALSE);
      SaRectS(d, dw, dh, x0, y0, 1, t0 + 1, 1, 4, 6, s, FALSE);
    } else {
      SaRectS(d, dw, dh, x0, y0, -5, t0 + 2, 10, 11, 8, s, FALSE);
      SaRectS(d, dw, dh, x0, y0, -5, t0 + 12, 10, 1, 7, s, FALSE);
      SaRectS(d, dw, dh, x0, y0, -2, t0 + 1, 4, 1, 7, s, FALSE);
    }
    SaDrawArt(d, dw, dh, x0 - 5 * s, y0 + (t0 - 11) * s, head, 10, 10, FALSE, s, 1);
  } else {
    SaRectS(d, dw, dh, x0, y0, -2 - st, top, 4, leg, 0, s, fl);
    SaRectS(d, dw, dh, x0, y0, -2 - st, -3, 6, 3, 4, s, fl);
    SaRectS(d, dw, dh, x0, y0, -8, t0 + 2, 3, 11, 8, s, fl);
    SaRectS(d, dw, dh, x0, y0, -2 + st, top, 4, leg, 1, s, fl);
    SaRectS(d, dw, dh, x0, y0, 1 + st, top, 1, leg, 3, s, fl);
    SaRectS(d, dw, dh, x0, y0, -2 + st, -3, 6, 2, 6, s, fl);
    SaRectS(d, dw, dh, x0, y0, -2 + st, -1, 6, 1, 4, s, fl);
    SaRectS(d, dw, dh, x0, y0, -5, t0, 10, 15, 12, s, fl);
    SaRectS(d, dw, dh, x0, y0, -5, t0, 1, 15, 11, s, fl);
    SaRectS(d, dw, dh, x0, y0, -5, top - 2, 10, 2, 11, s, fl);
    SaRectS(d, dw, dh, x0, y0, -6, t0 - 3, 10, 4, 11, s, fl);
    SaRectS(d, dw, dh, x0, y0, -1 + ax, t0 + 1, 3, 13, 12, s, fl);
    SaRectS(d, dw, dh, x0, y0, -1 + ax, t0 + 1, 1, 13, 11, s, fl);
    SaRectS(d, dw, dh, x0, y0, -1 + ax, t0 + 14, 3, 2, 9, s, fl);
    SaDrawArt(d, dw, dh, x0 - 5 * s, y0 + (t0 - 11) * s, ART_PL_S, 10, 10, fl, s, 1);
  }
}

/* ------------------------------------------------------------------ */
/* line of sight, field of view, path finding                          */
/* ------------------------------------------------------------------ */

/* TRUE when no opaque tile lies strictly between the two tiles. */
I64 SaLosTiles(I64 x0, I64 y0, I64 x1, I64 y1)
{
  I64 dx, dy, sx, sy, err, e2;
  dx = SaAbs(x1 - x0);
  dy = -SaAbs(y1 - y0);
  sx = 1;
  if (x0 > x1)
    sx = -1;
  sy = 1;
  if (y0 > y1)
    sy = -1;
  err = dx + dy;
  while (TRUE) {
    if (x0 == x1 && y0 == y1)
      return TRUE;
    e2 = 2 * err;
    if (e2 >= dy) {
      err = err + dy;
      x0 = x0 + sx;
    }
    if (e2 <= dx) {
      err = err + dx;
      y0 = y0 + sy;
    }
    if (x0 == x1 && y0 == y1)
      return TRUE;
    if (SaOpaque(x0, y0))
      return FALSE;
  }
  return TRUE;
}

U0 SaFov()
{
  I64 ox, oy, x, y, dx, dy;
  SaMemSet(sa_vis, 0, MAP_N);
  ox = sa_px >> 12;
  oy = sa_py >> 12;
  for (dy = -12; dy <= 12; dy++)
    for (dx = -12; dx <= 12; dx++)
      if (dx * dx + dy * dy <= 148) {
        x = ox + dx;
        y = oy + dy;
        if (x >= 0 && y >= 0 && x < MAP_W && y < MAP_H)
          if (SaLosTiles(ox, oy, x, y)) {
            sa_vis[y * MAP_W + x] = 1;
            sa_seen[sa_floor * MAP_N + y * MAP_W + x] = 1;
          }
      }
}

/* breadth first search from goal into distance slot e */
U0 SaBfs(I64 e, I64 goal)
{
  I64 head, tail, i, n, k, x, y, base;
  base = e * MAP_N;
  for (i = 0; i < MAP_N; i++)
    sa_dist[base + i] = -1;
  sa_dist[base + goal] = 0;
  sa_queue[0] = goal;
  head = 0;
  tail = 1;
  while (head < tail) {
    i = sa_queue[head];
    head++;
    x = i % MAP_W;
    y = i / MAP_W;
    for (k = 0; k < 4; k++) {
      n = -1;
      if (k == 0 && x > 0)
        n = i - 1;
      if (k == 1 && x < MAP_W - 1)
        n = i + 1;
      if (k == 2 && y > 0)
        n = i - MAP_W;
      if (k == 3 && y < MAP_H - 1)
        n = i + MAP_W;
      if (n >= 0)
        if (sa_dist[base + n] < 0)
          if (SaWalkCh(sa_map[n])) {
            sa_dist[base + n] = sa_dist[base + i] + 1;
            sa_queue[tail] = n;
            tail++;
          }
    }
  }
  sa_ebfs[e] = goal;
}

I64 SaPlayerTile()
{
  return (sa_py >> 12) * MAP_W + (sa_px >> 12);
}

I64 SaEntTile(I64 e)
{
  return (sa_ey[e] >> 12) * MAP_W + (sa_ex[e] >> 12);
}

I64 SaRandomTileNear(I64 cx, I64 cy, I64 r)
{
  I64 i, x, y;
  for (i = 0; i < 40; i++) {
    x = cx - r + SaRand(2 * r + 1);
    y = cy - r + SaRand(2 * r + 1);
    if (SaWalkCh(SaTile(x, y)) && SaTile(x, y) != '^' && SaTile(x, y) != 'v')
      return y * MAP_W + x;
  }
  return SaEntTile(0);
}

/* Moves entity e one step along its BFS field towards sa_egoal[e]. With
 * direct set it walks straight at (dpx, dpy) once next to the goal. */
U0 SaEntMove(I64 e, I64 speed, I64 direct, I64 dpx, I64 dpy)
{
  I64 hx, hy, ht, k, n, best, bestd, nx, ny, d, tx, ty, dx, dy, len, step, cur, base;
  hx = sa_ex[e] >> 8;
  hy = sa_ey[e] >> 8;
  ht = (hy >> 4) * MAP_W + (hx >> 4);
  base = e * MAP_N;
  sa_emov[e] = FALSE;
  if (sa_ebfs[e] != sa_egoal[e])
    SaBfs(e, sa_egoal[e]);
  cur = sa_dist[base + ht];
  if (cur < 0)
    return;
  if (ht == sa_egoal[e]) {
    if (direct) {
      tx = dpx;
      ty = dpy;
    } else {
      tx = (sa_egoal[e] % MAP_W) * 16 + 8;
      ty = (sa_egoal[e] / MAP_W) * 16 + 10;
    }
  } else {
    best = -1;
    bestd = 1000000;
    for (k = 0; k < 4; k++) {
      n = -1;
      if (k == 0)
        n = ht - 1;
      if (k == 1)
        n = ht + 1;
      if (k == 2)
        n = ht - MAP_W;
      if (k == 3)
        n = ht + MAP_W;
      if (n >= 0 && n < MAP_N)
        if (sa_dist[base + n] >= 0 && sa_dist[base + n] < cur) {
          nx = (n % MAP_W) * 16 + 8;
          ny = (n / MAP_W) * 16 + 10;
          d = (nx - hx) * (nx - hx) + (ny - hy) * (ny - hy);
          if (d < bestd) {
            bestd = d;
            best = n;
          }
        }
    }
    if (best < 0)
      return;
    if (direct && best == sa_egoal[e]) {
      tx = dpx;
      ty = dpy;
    } else {
      tx = (best % MAP_W) * 16 + 8;
      ty = (best / MAP_W) * 16 + 10;
    }
  }
  dx = (tx << 8) - sa_ex[e];
  dy = (ty << 8) - sa_ey[e];
  len = SaIsqrt(dx * dx + dy * dy);
  if (len <= 0)
    return;
  step = speed;
  if (step > len)
    step = len;
  sa_ex[e] = sa_ex[e] + dx * step / len;
  sa_ey[e] = sa_ey[e] + dy * step / len;
  if (SaAbs(dx) > SaAbs(dy)) {
    if (dx > 0)
      sa_edir[e] = DIR_RIGHT;
    else
      sa_edir[e] = DIR_LEFT;
  } else {
    if (dy > 0)
      sa_edir[e] = DIR_DOWN;
    else
      sa_edir[e] = DIR_UP;
  }
  sa_eanim[e] = sa_eanim[e] + step;
  sa_emov[e] = TRUE;
}

/* ------------------------------------------------------------------ */
/* camera                                                              */
/* ------------------------------------------------------------------ */

I64 SaTeacherHere(I64 i)
{
  if (sa_tf[i] == sa_floor && sa_tvalid[i])
    return TRUE;
  return FALSE;
}

/* Follows the player. While Herr Thüner still sits at his desk it frames
 * both of you. It may look a little past the top and bottom of the school
 * so the HUD lines never cover the outer rooms. */
I64 SaCamTargetX()
{
  I64 x;
  x = (sa_px >> 8) - SCR_W / 2;
  if (sa_tst[0] == TH_SIT && SaTeacherHere(0))
    x = ((sa_px >> 8) + (sa_ex[0] >> 8)) / 2 - SCR_W / 2;
  if (x < 0)
    x = 0;
  if (x > sa_floor_w[sa_floor] * 16 - SCR_W)
    x = sa_floor_w[sa_floor] * 16 - SCR_W;
  return x;
}

I64 SaCamTargetY()
{
  I64 y;
  y = (sa_py >> 8) - 112;
  if (sa_tst[0] == TH_SIT && SaTeacherHere(0))
    y = ((sa_py >> 8) + (sa_ey[0] >> 8)) / 2 - 104;
  if (y < -26)
    y = -26;
  if (y > WORLD_H - SCR_H + 30)
    y = WORLD_H - SCR_H + 30;
  return y;
}

U0 SaCamSnap()
{
  sa_camx = SaCamTargetX();
  sa_camy = SaCamTargetY();
}

U0 SaCamUpdate()
{
  sa_camx = sa_camx + (SaCamTargetX() - sa_camx) / 4;
  sa_camy = sa_camy + (SaCamTargetY() - sa_camy) / 4;
}

/* ------------------------------------------------------------------ */
/* game logic                                                          */
/* ------------------------------------------------------------------ */

I64 SaRoomIndex(I64 f, I64 tx, I64 ty)
{
  I64 i;
  for (i = 0; i < NROOMS; i++)
    if (sa_rm_f[i] == f && tx >= sa_rm_x0[i] && tx <= sa_rm_x1[i] && ty >= sa_rm_y0[i] &&
        ty <= sa_rm_y1[i])
      return i;
  return -1;
}

U0 SaPlaceEnt(I64 e, I64 tx, I64 ty)
{
  sa_ex[e] = (tx * 16 + 8) << 8;
  sa_ey[e] = (ty * 16 + 10) << 8;
  sa_egoal[e] = ty * MAP_W + tx;
  sa_emov[e] = FALSE;
}

U0 SaSpawnAtStairs(I64 e)
{
  SaPlaceEnt(e, 2 + (e % 4), 18 + (e / 4) * 2);
  sa_edir[e] = DIR_DOWN;
}

/* a random spot far away from the player on the current floor */
U0 SaSpawnFar(I64 e)
{
  I64 i, x, y, dx, dy;
  for (i = 0; i < 80; i++) {
    x = 1 + SaRand(MAP_W - 2);
    y = 1 + SaRand(MAP_H - 2);
    dx = x - (sa_px >> 12);
    dy = y - (sa_py >> 12);
    if (SaWalkCh(SaTile(x, y)) && SaTile(x, y) != '^' && SaTile(x, y) != 'v' &&
        dx * dx + dy * dy > 200) {
      SaPlaceEnt(e, x, y);
      return;
    }
  }
  SaSpawnAtStairs(e);
}

I64 SaWanderGoal(I64 e)
{
  if (e == 0 && SaRand(100) < 40)
    return SaRandomTileNear(sa_px >> 12, sa_py >> 12, 8);
  return SaRandomTileNear(1 + SaRand(MAP_W - 2), 1 + SaRand(MAP_H - 2), 3);
}

U0 SaResetTeacher(I64 i)
{
  sa_tlost[i] = 0;
  sa_tstun[i] = 0;
  sa_tslow[i] = 0;
  sa_tfollow[i] = 0;
  sa_ttimer[i] = 300;
  sa_tlast[i] = SaEntTile(i);
  if (i == 0)
    sa_tst[0] = TH_WANDER;
  else
    sa_tst[1] = KB_PATROL;
  sa_egoal[i] = SaWanderGoal(i);
}

/* ---- the cleaners, Frau Niehaus, the scooters ---- */

/* makes a floor tile wet for t ticks */
U0 SaWetTile(I64 tx, I64 ty, I64 t)
{
  I64 ch;
  ch = SaTile(tx, ty);
  if (SaIsFloorCh(ch) || ch == 'D')
    if (sa_wet[ty * MAP_W + tx] < t)
      sa_wet[ty * MAP_W + tx] = t;
}

/* cleaners mop the corridors most of the time, now and then a room */
I64 SaCleanerGoal(I64 e)
{
  I64 i, x, y, ch;
  for (i = 0; i < 60; i++) {
    x = 1 + SaRand(MAP_W - 2);
    y = 1 + SaRand(MAP_H - 2);
    ch = SaTile(x, y);
    if (SaIsFloorCh(ch) && !(x <= 7 && y >= 17))
      if (ch == '-' || ch == 'r' || ch == 'h' || SaRand(100) < 25)
        return y * MAP_W + x;
  }
  return SaRandomTileNear(sa_ex[e] >> 12, sa_ey[e] >> 12, 5);
}

/* a cleaner turns up on the current floor: by the stairs, or (arrive
 * FALSE) somewhere far away, already busy */
U0 SaPlaceCleaner(I64 c, I64 arrive)
{
  I64 e;
  e = E_CL + c;
  if (arrive)
    SaSpawnAtStairs(e);
  else
    SaSpawnFar(e);
  sa_cvalid[c] = TRUE;
  sa_cst[c] = CL_WALK;
  sa_ct[c] = 0;
  sa_csee[c] = 0;
  sa_csx[c] = -1;
  sa_ebfs[e] = -1;
  sa_egoal[e] = SaCleanerGoal(e);
}

/* the next floor for a cleaner (never the schoolyard) */
I64 SaCleanerNextFloor(I64 f)
{
  I64 nf;
  nf = f + 1;
  if (SaRand(2) == 0)
    nf = f - 1;
  if (nf == F_OUTSIDE) {
    if (nf > f)
      nf = nf + 1;
    else
      nf = nf - 1;
  }
  if (nf < 0)
    nf = 1;
  if (nf > F_TOP)
    nf = F_TOP - 1;
  return nf;
}

/* a free tile in one of the rooms of the 3rd floor (not the corridor) */
I64 SaNiehausGoal(I64 far)
{
  I64 i, x, y, r, dx, dy;
  for (i = 0; i < 300; i++) {
    x = 1 + SaRand(MAP_W - 2);
    y = 1 + SaRand(MAP_H - 2);
    r = SaRoomIndex(F_NIEHAUS, x, y);
    dx = x - (sa_px >> 12);
    dy = y - (sa_py >> 12);
    if (r >= 0 && SaIsFloorCh(SaTile(x, y)))
      if (sa_rm_hall[r] == 0 && (!far || dx * dx + dy * dy > 150))
        return y * MAP_W + x;
  }
  return 20 * MAP_W + 30;
}

U0 SaPlaceNiehaus()
{
  I64 g;
  g = SaNiehausGoal(TRUE);
  SaPlaceEnt(E_NI, g % MAP_W, g / MAP_W);
  sa_ni_valid = TRUE;
  sa_ni_st = NI_WANDER;
  sa_ni_t = 60;
  sa_ni_meter = 0;
  sa_ni_said = FALSE;
  sa_ni_hum = FALSE;
  sa_ebfs[E_NI] = -1;
  sa_egoal[E_NI] = SaNiehausGoal(FALSE);
  sa_edir[E_NI] = DIR_DOWN;
}

/* gets off the scooter or bike: it stays where you are */
U0 SaParkVehicle()
{
  if (sa_ride < 0)
    return;
  sa_vh_x[sa_ride] = sa_px;
  sa_vh_y[sa_ride] = sa_py;
  sa_vh_dir[sa_ride] = sa_pdir;
  sa_ride = -1;
}

/* ---- things to pick up, tasks, checkpoints ---- */

/* A random free floor tile of floor f (already in sa_map, with the BFS
 * from the staircase in distance slot 0) for the given zone. */
I64 SaPickSpot(I64 f, I64 zone)
{
  I64 i, k, x, y, ch, ok, r;
  for (i = 0; i < 600; i++) {
    x = 1 + SaRand(MAP_W - 2);
    y = 1 + SaRand(MAP_H - 2);
    ch = sa_map[y * MAP_W + x];
    ok = SaIsFloorCh(ch);
    if (ok && zone == 2) {
      if (y > 11 || ch != 't')
        ok = FALSE;
    } else if (ok && sa_dist[y * MAP_W + x] < 0)
      ok = FALSE;
    if (ok && x <= 7 && y >= 16)
      ok = FALSE;
    /* "in a room": not in a corridor or the staircase */
    if (ok && (zone == 1 || zone == 4)) {
      r = SaRoomIndex(f, x, y);
      if (r < 0 || ch == '-' || ch == 'r')
        ok = FALSE;
      else if (sa_rm_hall[r] != 0)
        ok = FALSE;
      else if (zone == 4 && r == sa_start_room)
        ok = FALSE;
    }
    /* scooters and bikes: out on the schoolyard */
    if (ok && zone == 5 && ((ch != '_' && ch != ';') || x < 9))
      ok = FALSE;
    if (ok && SaAbs(sa_le_sx[f] - x) <= 1 && SaAbs(sa_le_sy[f] - y) <= 1)
      ok = FALSE;
    for (k = 0; k < sa_it_n && ok; k++)
      if (sa_it_f[k] == f && SaAbs(sa_it_x[k] - x) <= 1 && SaAbs(sa_it_y[k] - y) <= 1)
        ok = FALSE;
    if (ok)
      return y * MAP_W + x;
  }
  return 18 * MAP_W + 3;
}

/* scatters every item of this run over the school, and picks a spot on
 * each floor where Herr Lersch will wait */
U0 SaPlaceItems()
{
  I64 f, k, n, s, j;
  sa_it_n = 0;
  for (f = 0; f < NFLOORS; f++) {
    SaLoadMap(f);
    SaBfs(0, 19 * MAP_W + 3);
    sa_le_sx[f] = -9;
    sa_le_sy[f] = -9;
    s = SaPickSpot(f, 4);
    sa_le_sx[f] = s % MAP_W;
    sa_le_sy[f] = s / MAP_W;
    for (k = 0; k < NPLACE; k++)
      if (sa_pl_f[k] == f)
        for (n = 0; n < sa_pl_n[k]; n++)
          if (sa_it_n < NITEMS) {
            if (sa_pl_zone[k] == 3)
              s = (sa_p0y - 1) * MAP_W + sa_p0x + 2;
            else
              s = SaPickSpot(f, sa_pl_zone[k]);
            j = sa_it_n;
            sa_it_f[j] = f;
            sa_it_x[j] = s % MAP_W;
            sa_it_y[j] = s / MAP_W;
            sa_it_t[j] = sa_pl_t[k];
            sa_it_got[j] = FALSE;
            sa_it_n++;
          }
    /* some days there are scooters and bikes parked on the schoolyard */
    if (f == F_OUTSIDE) {
      sa_vh_n = 0;
      for (k = 0; k < NVEH; k++)
        if (SaRand(100) < 60) {
          s = SaPickSpot(f, 5);
          j = sa_vh_n;
          sa_vh_x[j] = ((s % MAP_W) * 16 + 8) << 8;
          sa_vh_y[j] = ((s / MAP_W) * 16 + 10) << 8;
          sa_vh_t[j] = SaRand(2);
          sa_vh_charge[j] = 70 + SaRand(70);
          sa_vh_dir[j] = DIR_RIGHT;
          if (SaRand(2) == 0)
            sa_vh_dir[j] = DIR_LEFT;
          sa_vh_n++;
        }
    }
  }
  sa_ebfs[0] = -1;
}

U0 SaSaveCp()
{
  I64 i;
  sa_cp_task = sa_task;
  sa_cp_power = sa_power;
  sa_cp_pool = sa_pool_open;
  for (i = 0; i < NFLOORS; i++)
    sa_cp_unlock[i] = sa_unlock[i];
  for (i = 0; i < NITEMS; i++)
    sa_cp_got[i] = sa_it_got[i];
  for (i = 0; i < NITYPES; i++)
    sa_cp_have[i] = sa_have[i];
}

U0 SaLoadCp()
{
  I64 i;
  sa_task = sa_cp_task;
  sa_power = sa_cp_power;
  sa_pool_open = sa_cp_pool;
  for (i = 0; i < NFLOORS; i++)
    sa_unlock[i] = sa_cp_unlock[i];
  for (i = 0; i < NITEMS; i++)
    sa_it_got[i] = sa_cp_got[i];
  for (i = 0; i < NITYPES; i++)
    sa_have[i] = sa_cp_have[i];
}

/* Herr Lersch hurries ahead to the floor of the current task */
U0 SaLerschGo()
{
  sa_le_f = sa_task_floor[sa_task];
  sa_le_tx = sa_le_sx[sa_le_f];
  sa_le_ty = sa_le_sy[sa_le_f];
}

U0 SaNextTask()
{
  if (sa_task < TK_EXIT)
    sa_task++;
  sa_hint = -1;
  SaLerschGo();
  SaSaveCp();
}

U0 SaUnlock(I64 f)
{
  sa_unlock[f] = TRUE;
  SaSound(SND_UNLOCK);
  if (f == sa_floor + 1 || f == sa_floor - 1)
    SaRenderWorld();
}

/* the goal line of the HUD, with a counter for collecting tasks */
CH *SaGoalText()
{
  SaStrCpy(sa_obj_buf, sa_task_goal[sa_task]);
  if (sa_task_need[sa_task] > 1) {
    SaStrNum(sa_obj_buf, sa_have[sa_task_item[sa_task]]);
    SaStrCat(sa_obj_buf, "/");
    SaStrNum(sa_obj_buf, sa_task_need[sa_task]);
  }
  return sa_obj_buf;
}

/* the first item still lying around that the current task needs */
I64 SaTaskItemLeft()
{
  I64 k;
  if (sa_task_item[sa_task] < 0)
    return -1;
  for (k = 0; k < sa_it_n; k++)
    if (!sa_it_got[k] && sa_it_t[k] == sa_task_item[sa_task])
      return k;
  return -1;
}

U0 SaResetGame()
{
  I64 i;
  sa_task = 0;
  sa_power = FALSE;
  sa_pool_open = FALSE;
  for (i = 0; i < NFLOORS; i++)
    sa_unlock[i] = FALSE;
  sa_unlock[3] = TRUE;
  for (i = 0; i < NITYPES; i++)
    sa_have[i] = 0;
  for (i = 0; i < 3; i++)
    sa_code[i] = 1 + SaRand(9);
  sa_hint = -1;
  sa_deaths = 0;
  sa_cj_met = FALSE;
  sa_bomb_t = 0;
  sa_boost = 0;
  sa_start_room = SaRoomIndex(3, sa_p0x, sa_p0y);
  sa_ride = -1;
  sa_vh_near = -1;
  sa_slip_t = 0;
  sa_sing_t = 0;
  sa_alarm_t = 0;
  sa_msg_face = -1;
  sa_ni_valid = FALSE;
  for (i = 0; i < MAP_N; i++)
    sa_wet[i] = 0;
  for (i = 0; i < 12096; i++)
    sa_seen[i] = 0;
  SaPlaceItems();
  SaLoadFloor(3);
  SaLerschGo();
  SaSaveCp();
  sa_px = (sa_p0x * 16 + 8) << 8;
  sa_py = (sa_p0y * 16 + 10) << 8;
  sa_pdir = DIR_UP;
  sa_panim = 0;
  sa_pmoving = FALSE;
  sa_crouch = 0;
  sa_hidden = FALSE;
  sa_stamina = 1000;
  sa_tired = FALSE;
  sa_running = FALSE;
  sa_hit_tile = 0;
  sa_locked_t = 0;
  sa_room = -1;
  sa_room_t = 0;
  sa_stair_cd = 0;
  sa_stair_arm = TRUE;
  sa_sign_t = 0;
  /* Herr Thüner sits at his desk in the chemistry room */
  sa_tf[0] = 3;
  sa_tvalid[0] = TRUE;
  SaPlaceEnt(0, sa_h0x, sa_h0y);
  SaResetTeacher(0);
  sa_tst[0] = TH_SIT;
  sa_ttimer[0] = 90;
  sa_edir[0] = DIR_DOWN;
  sa_troam[0] = 1500;
  /* Herr Kirschbaum walks around the music floor */
  sa_tf[1] = 2;
  sa_tvalid[1] = FALSE;
  sa_tfollow[1] = 0;
  sa_tst[1] = KB_PATROL;
  sa_troam[1] = 1200 + SaRand(900);
  sa_tstun[1] = 0;
  sa_tslow[1] = 0;
  sa_kb_alert = FALSE;
  sa_heard_t = 0;
  sa_cj_st = CJ_OFF;
  sa_cj_tgt = -1;
  /* one cleaner is mopping up here already, the other one upstairs */
  sa_tslipcd[0] = 0;
  sa_tslipcd[1] = 0;
  for (i = 0; i < 2; i++) {
    sa_ccd[i] = 0;
    sa_csee[i] = 0;
    sa_csx[i] = -1;
    sa_cvalid[i] = FALSE;
    sa_croam[i] = 1800 + SaRand(1200);
  }
  sa_cf[0] = 3;
  sa_cf[1] = F_TOP;
  SaPlaceCleaner(0, FALSE);
  SaPlaceEnt(E_CL, 36, 3);
  sa_egoal[E_CL] = 3 * MAP_W + 36;
  sa_phone = TRUE;
  sa_call_cd = 0;
  sa_ring_t = 0;
  sa_next_ring = 1800 + SaRand(600);
  sa_write_t = 0;
  sa_play_ticks = 0;
  sa_msg_t = 0;
  sa_spot_t = 0;
  sa_shake = 0;
  sa_pulse = 0;
  sa_heart_t = 0;
  sa_light_r = 150;
  SaMsg2("SCHNAPP DIR DIE TASCHENLAMPE!", "DANN FINDE DEN TREPPENSCHLÜSSEL.", 120);
  sa_msg_face = -1;
  SaFov();
  SaCamSnap();
}

/* After being caught: back to the last checkpoint, in the staircase of the
 * task floor. The phone stays wherever it is now. */
U0 SaContinue()
{
  I64 f, i;
  SaLoadCp();
  if (sa_ride >= 0 && sa_floor == F_OUTSIDE)
    SaParkVehicle();
  sa_ride = -1;
  f = sa_task_floor[sa_task];
  SaLoadFloor(f);
  for (i = 0; i < MAP_N; i++)
    sa_wet[i] = 0;
  sa_slip_t = 0;
  sa_sing_t = 0;
  sa_alarm_t = 0;
  sa_ni_valid = FALSE;
  for (i = 0; i < 2; i++) {
    sa_cvalid[i] = FALSE;
    sa_csee[i] = 0;
    if (sa_cf[i] == f)
      SaPlaceCleaner(i, FALSE);
  }
  SaLerschGo();
  sa_px = (3 * 16 + 8) << 8;
  sa_py = (19 * 16 + 10) << 8;
  sa_pdir = DIR_DOWN;
  sa_pmoving = FALSE;
  sa_crouch = 0;
  sa_hidden = FALSE;
  sa_stamina = 1000;
  sa_tired = FALSE;
  sa_running = FALSE;
  sa_boost = 0;
  sa_hit_tile = 0;
  sa_room = -1;
  sa_stair_cd = 20;
  sa_stair_arm = TRUE;
  sa_sign_t = 80;
  sa_hint = -1;
  sa_bomb_t = 0;
  sa_write_t = 0;
  sa_ring_t = 0;
  sa_next_ring = 900 + SaRand(900);
  sa_spot_t = 0;
  sa_shake = 0;
  sa_pulse = 0;
  /* Herr Thüner needs a while to find you again */
  sa_tvalid[0] = FALSE;
  sa_tf[0] = f + 1;
  if (sa_tf[0] > F_TOP)
    sa_tf[0] = f - 1;
  sa_tst[0] = TH_WANDER;
  sa_tfollow[0] = 0;
  sa_troam[0] = 600;
  sa_tstun[0] = 0;
  sa_tslow[0] = 0;
  if (sa_tf[1] == f) {
    sa_tvalid[1] = FALSE;
    sa_tf[1] = f - 1;
    if (sa_tf[1] < 0)
      sa_tf[1] = 1;
  }
  sa_tst[1] = KB_PATROL;
  sa_tfollow[1] = 0;
  sa_troam[1] = 900;
  sa_kb_alert = FALSE;
  sa_cj_st = CJ_OFF;
  sa_light_r = 92;
  SaMsg2("WEITER AB DEM LETZTEN CHECKPOINT.", SaGoalText(), 120);
  SaFov();
  SaCamSnap();
}

U0 SaCaught(CH *reason)
{
  sa_state = ST_CAUGHT;
  sa_timer = 0;
  sa_over_reason = reason;
  sa_end_ticks = sa_play_ticks;
  sa_ring_t = 0;
  sa_deaths++;
  SaSound(SND_SCARE);
}

U0 SaSpot()
{
  sa_spot_t = 30;
  sa_shake = 8;
  SaSound(SND_SPOT);
}

I64 SaTeacherSees(I64 i, I64 maxd2)
{
  I64 ptx, pty, htx, hty, dx, dy;
  if (sa_hidden || !SaTeacherHere(i))
    return FALSE;
  ptx = sa_px >> 12;
  pty = sa_py >> 12;
  htx = sa_ex[i] >> 12;
  hty = sa_ey[i] >> 12;
  dx = ptx - htx;
  dy = pty - hty;
  if (dx * dx + dy * dy > maxd2)
    return FALSE;
  return SaLosTiles(htx, hty, ptx, pty);
}

I64 SaDistPx(I64 e)
{
  I64 dx, dy;
  dx = (sa_ex[e] - sa_px) / 256;
  dy = (sa_ey[e] - sa_py) / 256;
  return SaIsqrt(dx * dx + dy * dy);
}

/* 0 free, 1 blocked, or the character of a special blocking tile */
I64 SaBoxHit(I64 fx, I64 fy)
{
  I64 x0, x1, y0, y1, r, ch, k, x, y;
  x0 = (fx >> 8) - 6;
  x1 = (fx >> 8) + 5;
  y0 = (fy >> 8) - 5;
  y1 = fy >> 8;
  r = 0;
  for (k = 0; k < 4; k++) {
    x = x0;
    y = y0;
    if (k == 1 || k == 3)
      x = x1;
    if (k >= 2)
      y = y1;
    ch = SaTile(x >> 4, y >> 4);
    if (!SaWalkCh(ch)) {
      if (ch == 'E' || ch == 'X' || ch == 'g' || ch == 'e')
        r = ch;
      else if (r == 0)
        r = 1;
    } else if ((ch == '^' && !SaStairOpen(TRUE)) || (ch == 'v' && !SaStairOpen(FALSE)))
      r = ch;
  }
  return r;
}

U0 SaPlayerMove(I64 keys)
{
  I64 dx, dy, speed, nx, ny, hit, off, ch;
  dx = 0;
  dy = 0;
  if ((keys & K_LEFT) != 0)
    dx = dx - 1;
  if ((keys & K_RIGHT) != 0)
    dx = dx + 1;
  if ((keys & K_UP) != 0)
    dy = dy - 1;
  if ((keys & K_DOWN) != 0)
    dy = dy + 1;
  sa_pmoving = FALSE;
  if (dx != 0 || dy != 0)
    sa_pmoving = TRUE;
  ch = SaTile(sa_px >> 12, sa_py >> 12);
  if (ch == 'D') {
    if (sa_crouch < 12)
      sa_crouch = sa_crouch + 3;
  } else if (sa_crouch > 0)
    sa_crouch = sa_crouch - 2;
  if (sa_crouch < 0)
    sa_crouch = 0;
  sa_running = FALSE;
  if (sa_pmoving) {
    speed = 300;
    if (sa_have[IT_SHOES] > 0)
      speed = 335;
    if ((keys & K_RUN) != 0 && sa_stamina > 0 && !sa_tired) {
      speed = 540;
      if (sa_have[IT_SHOES] > 0)
        speed = 590;
      sa_running = TRUE;
    }
    if (sa_ride >= 0) {
      speed = 760;
      if (sa_vh_t[sa_ride] == VH_BIKE)
        speed = 660;
      sa_running = FALSE;
    }
    if (sa_crouch > 0)
      speed = speed * 55 / 100;
    if (dx != 0 && dy != 0)
      speed = speed * 181 / 256;
    if (dx != 0) {
      nx = sa_px + dx * speed;
      hit = SaBoxHit(nx, sa_py);
      if (hit == 0)
        sa_px = nx;
      else if (hit > 1)
        sa_hit_tile = hit;
      else if (dy == 0) {
        for (off = 1; off <= 12; off++) {
          if (SaBoxHit(nx, sa_py + off * 256) == 0 && SaBoxHit(sa_px, sa_py + 200) == 0) {
            sa_py = sa_py + 200;
            off = 99;
          } else if (SaBoxHit(nx, sa_py - off * 256) == 0 && SaBoxHit(sa_px, sa_py - 200) == 0) {
            sa_py = sa_py - 200;
            off = 99;
          }
        }
      }
    }
    if (dy != 0) {
      ny = sa_py + dy * speed;
      hit = SaBoxHit(sa_px, ny);
      if (hit == 0)
        sa_py = ny;
      else if (hit > 1)
        sa_hit_tile = hit;
      else if (dx == 0) {
        for (off = 1; off <= 12; off++) {
          if (SaBoxHit(sa_px + off * 256, ny) == 0 && SaBoxHit(sa_px + 200, sa_py) == 0) {
            sa_px = sa_px + 200;
            off = 99;
          } else if (SaBoxHit(sa_px - off * 256, ny) == 0 && SaBoxHit(sa_px - 200, sa_py) == 0) {
            sa_px = sa_px - 200;
            off = 99;
          }
        }
      }
    }
    if (dx > 0)
      sa_pdir = DIR_RIGHT;
    else if (dx < 0)
      sa_pdir = DIR_LEFT;
    else if (dy < 0)
      sa_pdir = DIR_UP;
    else
      sa_pdir = DIR_DOWN;
    sa_panim = sa_panim + speed;
  }
  if (sa_running) {
    if (sa_boost == 0)
      sa_stamina = sa_stamina - 10;
    if (sa_stamina <= 0) {
      sa_stamina = 0;
      if (sa_have[IT_DRINK] > 0) {
        /* an energy drink kicks in by itself */
        sa_have[IT_DRINK]--;
        sa_stamina = 1000;
        sa_boost = 240;
        SaSound(SND_DRINK);
        SaMsg("ENERGY-DRINK! 8 SEKUNDEN VOLLE PUSTE.", 60);
      } else {
        sa_tired = TRUE;
        SaMsg("KEINE PUSTE MEHR!", 45);
      }
    }
  } else {
    sa_stamina = sa_stamina + 4;
    if (sa_hidden)
      sa_stamina = sa_stamina + 3;
    if (sa_stamina > 1000)
      sa_stamina = 1000;
    if (sa_tired && sa_stamina > 350)
      sa_tired = FALSE;
  }
}

U0 SaToggleHide()
{
  I64 ptx, pty, k, lx, ly, saw;
  if (sa_hidden) {
    sa_hidden = FALSE;
    SaSound(SND_LOCKER);
    SaMsg("DU FALTEST DICH WIEDER AUSEINANDER.", 60);
    if (sa_tst[0] == TH_GRAB)
      sa_tst[0] = TH_CHASE;
    return;
  }
  ptx = sa_px >> 12;
  pty = sa_py >> 12;
  for (k = 0; k < 4; k++) {
    lx = ptx;
    ly = pty;
    if (k == 0)
      ly = pty - 1;
    if (k == 1)
      ly = pty + 1;
    if (k == 2)
      lx = ptx - 1;
    if (k == 3)
      lx = ptx + 1;
    if (SaTile(lx, ly) == 'L') {
      saw = FALSE;
      if (sa_tst[0] == TH_CHASE && sa_tstun[0] == 0 && SaTeacherSees(0, SA_SEE_DIST2))
        saw = TRUE;
      sa_hidden = TRUE;
      SaSound(SND_LOCKER);
      if (saw) {
        sa_tst[0] = TH_GRAB;
        SaMsg("OH NEIN... ER HAT ES GESEHEN!", 90);
      } else
        SaMsg("DU FALTEST DICH IN DEN SPIND. ALLE 8 METER.", 90);
      return;
    }
  }
  SaMsg("HIER IST KEIN SPIND ZUM VERSTECKEN.", 45);
}

/* ---- Conjoke ---- */

I64 SaConjokeTarget()
{
  I64 i, best, bd, d;
  best = -1;
  bd = 99999;
  for (i = 0; i < 2; i++)
    if (SaTeacherHere(i) && sa_tst[i] != KB_WRITE) {
      d = SaDistPx(i);
      if ((i == 0 && (sa_tst[0] == TH_CHASE || sa_tst[0] == TH_GRAB)) ||
          (i == 1 && sa_tst[1] == KB_RUSH))
        d = d / 4;
      if (d < bd) {
        bd = d;
        best = i;
      }
    }
  return best;
}

U0 SaSummonConjoke()
{
  if (sa_cj_st != CJ_OFF)
    return;
  sa_cj_st = CJ_WAIT;
  sa_cj_t = 30;
  sa_call_cd = SA_CALL_CD;
}

U0 SaConjokeTick()
{
  I64 t, dx, dy;
  if (sa_cj_st == CJ_OFF)
    return;
  if (sa_cj_st == CJ_WAIT) {
    sa_cj_t--;
    if (sa_cj_t <= 0) {
      SaSpawnAtStairs(2);
      sa_ebfs[2] = -1;
      sa_cj_tgt = SaConjokeTarget();
      if (sa_cj_tgt >= 0) {
        sa_cj_st = CJ_RUN;
        SaMsg("CONJOKE STÜRMT DURCHS TREPPENHAUS!", 60);
      } else {
        sa_cj_st = CJ_GUARD;
        sa_cj_t = 450;
        SaMsg2("CONJOKE: KEIN LEHRER HIER, BRO.", "ICH PASS AUF DICH AUF.", 90);
      }
    }
    return;
  }
  if (sa_cj_st == CJ_RUN) {
    t = sa_cj_tgt;
    if (!SaTeacherHere(t)) {
      sa_cj_st = CJ_GUARD;
      sa_cj_t = 300;
      return;
    }
    sa_egoal[2] = SaEntTile(t);
    SaEntMove(2, SA_CJ_SPEED, TRUE, sa_ex[t] >> 8, sa_ey[t] >> 8);
    dx = SaAbs(sa_ex[2] - sa_ex[t]) / 256;
    dy = SaAbs(sa_ey[2] - sa_ey[t]) / 256;
    if (dx < 12 && dy < 10) {
      sa_cj_st = CJ_FIGHT;
      sa_cj_t = 90;
      sa_tstun[t] = 90;
      sa_tslow[t] = 450;
      if (t == 0) {
        SaMsg2("RAUFEREI!", "CONJOKE HÄLT HERRN THÜNER AUF!", 90);
        if (sa_tst[0] == TH_GRAB || sa_tst[0] == TH_CHASE) {
          sa_tst[0] = TH_SEARCH;
          sa_ttimer[0] = 200;
        }
      } else {
        SaMsg2("RAUFEREI!", "CONJOKE HÄLT HERRN KIRSCHBAUM AUF!", 90);
        sa_tst[1] = KB_PATROL;
      }
    }
    return;
  }
  if (sa_cj_st == CJ_FIGHT) {
    sa_cj_t--;
    if ((sa_cj_t % 15) == 0)
      SaSound(SND_FIGHT);
    if (sa_cj_t <= 0) {
      sa_cj_st = CJ_LEAVE;
      SaMsg("CONJOKE: KEIN PROBLEM, BRO!", 60);
    }
    return;
  }
  if (sa_cj_st == CJ_GUARD) {
    sa_cj_t--;
    sa_egoal[2] = SaPlayerTile();
    if (SaDistPx(2) > 28)
      SaEntMove(2, SA_CJ_SPEED - 100, TRUE, (sa_px >> 8) + 14, sa_py >> 8);
    else
      sa_emov[2] = FALSE;
    t = SaConjokeTarget();
    if (t >= 0)
      if (SaDistPx(t) < 100) {
        sa_cj_tgt = t;
        sa_cj_st = CJ_RUN;
      }
    if (sa_cj_t <= 0)
      sa_cj_st = CJ_LEAVE;
    return;
  }
  if (sa_cj_st == CJ_LEAVE) {
    sa_egoal[2] = 18 * MAP_W + 4;
    SaEntMove(2, SA_CJ_SPEED, FALSE, 0, 0);
    if (SaEntTile(2) == sa_egoal[2])
      sa_cj_st = CJ_OFF;
  }
}

/* ---- the phone ---- */

U0 SaRingStart()
{
  I64 d;
  sa_ring_t = 210;
  sa_ring_step = 0;
  SaMsg2("DEIN HANDY KLINGELT! (CONJOKE)",
         SaHint("Q = RANGEHEN", "HANDY-KNOPF = RANGEHEN", "Y = RANGEHEN"), 90);
  if (SaTeacherHere(1)) {
    if (sa_tst[1] != KB_WRITE) {
      sa_tst[1] = KB_RUSH;
      sa_tlast[1] = SaPlayerTile();
      sa_egoal[1] = sa_tlast[1];
    }
  } else {
    /* he hears it from up to two floors away and comes at once */
    d = SaAbs(sa_tf[1] - sa_floor);
    if (d <= 2 && sa_tfollow[1] == 0) {
      sa_tfollow[1] = 45;
      sa_tfollow_f[1] = sa_floor;
      sa_kb_alert = TRUE;
      sa_tlast[1] = SaPlayerTile();
    }
  }
}

U0 SaPhoneButton()
{
  if (!sa_phone) {
    SaMsg("DEIN HANDY HAT JETZT HERR KIRSCHBAUM.", 60);
    return;
  }
  if (sa_write_t > 0)
    return;
  if (sa_ring_t > 0) {
    sa_ring_t = 0;
    sa_next_ring = 1500 + SaRand(1500);
    if (sa_cj_st == CJ_OFF) {
      SaMsg2("CONJOKE: YO BRO, BRAUCHST DU HILFE?", "ICH KOMM SOFORT!", 90);
      SaSummonConjoke();
    } else
      SaMsg("CONJOKE: ICH BIN DOCH SCHON DA, BRO!", 60);
    return;
  }
  if (sa_cj_st != CJ_OFF) {
    SaMsg("CONJOKE IST SCHON UNTERWEGS.", 45);
    return;
  }
  if (sa_call_cd > 0) {
    SaStrCpy(sa_le_buf, "CONJOKE GEHT NICHT RAN. (NOCH ");
    SaStrNum(sa_le_buf, sa_call_cd / TICKS_PER_SEC + 1);
    SaStrCat(sa_le_buf, " S)");
    SaMsg(sa_le_buf, 60);
    return;
  }
  SaSound(SND_CALL);
  SaMsg2("DU RUFST CONJOKE AN...", "CONJOKE: BIN UNTERWEGS, BRO!", 90);
  SaSummonConjoke();
  if (SaTeacherHere(1) && sa_tst[1] != KB_WRITE && SaTeacherSees(1, 64)) {
    sa_tst[1] = KB_RUSH;
    sa_tlast[1] = SaPlayerTile();
    sa_egoal[1] = sa_tlast[1];
    SaMsg2("HERR KIRSCHBAUM: EIN HANDY?!", "DAS GIBST DU MIR SOFORT!", 90);
  }
}

/* ---- Herr Lersch ---- */

I64 SaLerschDist()
{
  I64 dx, dy;
  if (sa_floor != sa_le_f)
    return 9999;
  dx = (sa_px >> 8) - (sa_le_tx * 16 + 8);
  dy = (sa_py >> 8) - (sa_le_ty * 16 + 10);
  return SaIsqrt(dx * dx + dy * dy);
}

/* Herr Lersch tells you where the next thing for your task lies */
U0 SaTalkLersch()
{
  I64 r, k;
  SaSound(SND_TALK);
  k = SaTaskItemLeft();
  if (k < 0) {
    SaSay(0, sa_le_say1[sa_task], sa_le_say2[sa_task], 150);
    return;
  }
  sa_hint = k;
  r = SaRoomIndex(sa_it_f[k], sa_it_x[k], sa_it_y[k]);
  if (sa_task_need[sa_task] > 1)
    SaStrCpy(sa_le_buf, "EINS HAB ICH GESEHEN - ORT: ");
  else
    SaStrCpy(sa_le_buf, "ORT: ");
  if (r >= 0)
    SaStrCat(sa_le_buf, sa_rm_name[r]);
  else
    SaStrCat(sa_le_buf, "IRGENDWO HIER");
  SaSay(0, sa_le_say1[sa_task], sa_le_buf, 150);
}

/* ---- teachers ---- */

U0 SaTeacherArrive(I64 i)
{
  sa_tvalid[i] = TRUE;
  SaSpawnAtStairs(i);
  sa_ebfs[i] = -1;
  sa_tstun[i] = 0;
  if (i == 0) {
    sa_tst[0] = TH_SEARCH;
    sa_tlast[0] = SaPlayerTile();
    sa_egoal[0] = sa_tlast[0];
    sa_ttimer[0] = 300;
    SaSound(SND_STEPS);
    SaMsg("DU HÖRST SCHRITTE IM TREPPENHAUS...", 75);
  } else {
    if (sa_kb_alert && sa_phone) {
      sa_tst[1] = KB_RUSH;
      sa_egoal[1] = sa_tlast[1];
      SaMsg("HERR KIRSCHBAUM KOMMT DIE TREPPE RUNTER!", 75);
    } else {
      sa_tst[1] = KB_PATROL;
      sa_egoal[1] = SaWanderGoal(1);
    }
    sa_kb_alert = FALSE;
  }
}

/* teachers on other floors move around abstractly */
U0 SaTeacherAway(I64 i)
{
  I64 nf;
  if (i == 0 && sa_tst[0] == TH_SIT) {
    sa_tst[0] = TH_WANDER;
    sa_troam[0] = 600;
  }
  if (sa_tfollow[i] > 0) {
    sa_tfollow[i]--;
    if (sa_tfollow[i] == 0) {
      if (sa_tf[i] < sa_tfollow_f[i])
        sa_tf[i]++;
      else if (sa_tf[i] > sa_tfollow_f[i])
        sa_tf[i]--;
      if (sa_tf[i] == sa_floor)
        SaTeacherArrive(i);
      else if (sa_tf[i] != sa_tfollow_f[i])
        sa_tfollow[i] = 45;
    }
    return;
  }
  sa_troam[i]--;
  if (sa_troam[i] > 0)
    return;
  if (i == 0) {
    /* Herr Thüner hunts you floor by floor, and he is quick on the stairs */
    sa_tfollow_f[0] = sa_floor;
    sa_tfollow[0] = 45;
    sa_troam[0] = 450 + SaRand(450);
  } else {
    /* Herr Kirschbaum mostly heads for the floor you are on */
    nf = sa_tf[1] + 1;
    if (SaRand(100) < 65) {
      if (sa_floor < sa_tf[1])
        nf = sa_tf[1] - 1;
    } else if (SaRand(2) == 0)
      nf = sa_tf[1] - 1;
    if (nf < 0)
      nf = 1;
    if (nf > F_TOP)
      nf = F_TOP - 1;
    sa_tf[1] = nf;
    sa_troam[1] = 450 + SaRand(450);
    if (nf == sa_floor)
      SaTeacherArrive(1);
  }
}

/* Herr Thüner got you - unless you still have a stink bomb */
U0 SaThunerCatch(CH *reason)
{
  if (sa_have[IT_BOMB] <= 0) {
    SaCaught(reason);
    return;
  }
  sa_have[IT_BOMB]--;
  sa_hidden = FALSE;
  sa_tstun[0] = 120;
  sa_tslow[0] = 300;
  sa_tst[0] = TH_SEARCH;
  sa_ttimer[0] = 150;
  sa_egoal[0] = SaEntTile(0);
  sa_bomb_t = 60;
  sa_shake = 8;
  SaSound(SND_BOMB);
  SaMsg2("STINKBOMBE!", "HERR THÜNER HUSTET - LAUF!", 90);
}

U0 SaThunerTick()
{
  I64 see, pt, ht, speed, chase, dx, dy, direct;
  pt = SaPlayerTile();
  ht = SaEntTile(0);
  see = FALSE;
  if (sa_tstun[0] == 0)
    see = SaTeacherSees(0, SA_SEE_DIST2);
  chase = SA_CHASE_BASE + sa_play_ticks / SA_CHASE_RAMP;
  if (chase > SA_CHASE_MAX)
    chase = SA_CHASE_MAX;
  if (sa_mode == MODE_EASY)
    chase = chase - 15;
  speed = 0;
  direct = FALSE;
  if (sa_heard_t > 0)
    sa_heard_t--;
  if (sa_tst[0] == TH_SIT) {
    sa_ttimer[0]--;
    if (sa_ttimer[0] == 45)
      SaMsg("HERR THÜNER STEHT LANGSAM AUF...", 50);
    if (sa_ttimer[0] <= 0) {
      sa_tst[0] = TH_CHASE;
      sa_tlast[0] = pt;
      sa_tlost[0] = 0;
      SaSpot();
      SaMsg("LAUF!!!", 60);
    }
  } else if (sa_tst[0] == TH_CHASE) {
    speed = chase;
    if (see) {
      sa_tlast[0] = pt;
      sa_tlost[0] = 0;
      direct = TRUE;
    } else
      sa_tlost[0]++;
    sa_egoal[0] = sa_tlast[0];
    if (sa_tlost[0] > SA_LOSE_TICKS) {
      sa_tst[0] = TH_SEARCH;
      sa_ttimer[0] = 270;
    }
  } else if (sa_tst[0] == TH_SEARCH) {
    speed = 230;
    if (see) {
      SaSpot();
      sa_tst[0] = TH_CHASE;
      sa_tlast[0] = pt;
      sa_tlost[0] = 0;
    } else {
      if (ht == sa_egoal[0])
        sa_egoal[0] = SaRandomTileNear(sa_egoal[0] % MAP_W, sa_egoal[0] / MAP_W, 4);
      sa_ttimer[0]--;
      if (sa_ttimer[0] <= 0) {
        sa_tst[0] = TH_WANDER;
        sa_egoal[0] = SaWanderGoal(0);
        sa_ttimer[0] = 360;
      }
    }
  } else if (sa_tst[0] == TH_WANDER) {
    speed = 190;
    if (see) {
      SaSpot();
      sa_tst[0] = TH_CHASE;
      sa_tlast[0] = pt;
      sa_tlost[0] = 0;
    } else {
      sa_ttimer[0]--;
      if (ht == sa_egoal[0] || sa_ttimer[0] <= 0) {
        sa_egoal[0] = SaWanderGoal(0);
        sa_ttimer[0] = 360;
      }
    }
  } else if (sa_tst[0] == TH_GRAB) {
    speed = chase + 40;
    sa_egoal[0] = pt;
    direct = TRUE;
  }
  /* running is loud */
  if (sa_running && sa_heard_t == 0 && (sa_tst[0] == TH_WANDER || sa_tst[0] == TH_SEARCH)) {
    dx = (sa_px >> 12) - (sa_ex[0] >> 12);
    dy = (sa_py >> 12) - (sa_ey[0] >> 12);
    if (dx * dx + dy * dy < 50) {
      sa_tst[0] = TH_SEARCH;
      sa_egoal[0] = pt;
      sa_ttimer[0] = 200;
      sa_heard_t = 150;
      SaMsg("ER HAT DICH RENNEN GEHÖRT!", 60);
    }
  }
  /* chasing over a wet floor is a bad idea, even for Herr Thüner */
  if (sa_tslipcd[0] > 0)
    sa_tslipcd[0]--;
  if (speed > 240 && sa_tstun[0] == 0 && sa_tslipcd[0] == 0 && sa_wet[ht] > 0) {
    sa_tstun[0] = 75;
    sa_tslipcd[0] = 240;
    SaSound(SND_SLIP);
    SaMsg2("HERR THÜNER RUTSCHT AUF DEM", "NASSEN BODEN AUS! LAUF!", 90);
  }
  if (sa_tstun[0] > 0) {
    sa_tstun[0]--;
    speed = 0;
  }
  if (sa_tslow[0] > 0) {
    sa_tslow[0]--;
    speed = speed / 2;
  }
  if (speed > 0)
    SaEntMove(0, speed, direct, sa_px >> 8, sa_py >> 8);
  else
    sa_emov[0] = FALSE;
  if (sa_tstun[0] > 0)
    return;
  dx = SaAbs(sa_ex[0] - sa_px) / 256;
  dy = SaAbs(sa_ey[0] - sa_py) / 256;
  if (sa_hidden) {
    if (sa_tst[0] == TH_GRAB && dx < 16 && dy < 16)
      SaThunerCatch("ER HAT DICH AUS DEM SPIND GEZOGEN.");
  } else if (sa_tst[0] != TH_SIT && dx < 8 && dy < 7)
    SaThunerCatch("UND ZWAR FÜR IMMER.");
  /* now and then he gives up on this floor (only when far away) */
  if (sa_tst[0] == TH_WANDER) {
    sa_troam[0]--;
    if (sa_troam[0] <= 0) {
      sa_troam[0] = 900 + SaRand(900);
      if (SaRand(100) < 30 && SaDistPx(0) > 180 && sa_vis[SaEntTile(0)] == 0) {
        sa_tvalid[0] = FALSE;
        sa_troam[0] = 300 + SaRand(300);
        sa_tf[0] = sa_floor + 1;
        if (sa_tf[0] > F_TOP)
          sa_tf[0] = sa_floor - 1;
      }
    }
  }
}

U0 SaKirschbaumTick()
{
  I64 pt, speed, dx, dy, direct, see;
  pt = SaPlayerTile();
  speed = 0;
  direct = FALSE;
  see = FALSE;
  if (sa_tstun[1] == 0 && sa_phone)
    see = SaTeacherSees(1, 50);
  if (sa_tst[1] == KB_PATROL) {
    speed = 160;
    if (SaEntTile(1) == sa_egoal[1] || sa_ebfs[1] < 0)
      sa_egoal[1] = SaWanderGoal(1);
    sa_troam[1]--;
    if (sa_troam[1] <= 0) {
      sa_troam[1] = 900 + SaRand(600);
      if (SaRand(100) < 60 && SaDistPx(1) > 150 && sa_vis[SaEntTile(1)] == 0) {
        sa_tvalid[1] = FALSE;
        sa_tf[1] = sa_floor - 1;
        if (sa_tf[1] < 0)
          sa_tf[1] = 1;
      }
    }
  } else if (sa_tst[1] == KB_RUSH) {
    speed = SA_KB_RUSH;
    if (see && (sa_ring_t > 0 || SaDistPx(1) < 90)) {
      sa_tlast[1] = pt;
      direct = TRUE;
    }
    sa_egoal[1] = sa_tlast[1];
    if (SaEntTile(1) == sa_tlast[1] && !direct) {
      sa_tst[1] = KB_LOOK;
      sa_ttimer[1] = 150;
    }
  } else if (sa_tst[1] == KB_LOOK) {
    speed = 200;
    if (SaEntTile(1) == sa_egoal[1])
      sa_egoal[1] = SaRandomTileNear(sa_tlast[1] % MAP_W, sa_tlast[1] / MAP_W, 4);
    if (see && sa_ring_t > 0)
      sa_tst[1] = KB_RUSH;
    sa_ttimer[1]--;
    if (sa_ttimer[1] <= 0) {
      sa_tst[1] = KB_PATROL;
      SaMsg("HERR KIRSCHBAUM: HMPF. NA GUT.", 45);
    }
  } else if (sa_tst[1] == KB_WRITE) {
    sa_emov[1] = FALSE;
    if (sa_px > sa_ex[1])
      sa_edir[1] = DIR_RIGHT;
    else
      sa_edir[1] = DIR_LEFT;
    return;
  }
  if (sa_tslipcd[1] > 0)
    sa_tslipcd[1]--;
  if (speed > 240 && sa_tstun[1] == 0 && sa_tslipcd[1] == 0 && sa_wet[SaEntTile(1)] > 0) {
    sa_tstun[1] = 75;
    sa_tslipcd[1] = 240;
    SaSound(SND_SLIP);
    SaMsg("HERR KIRSCHBAUM RUTSCHT AUS! PLATSCH!", 75);
  }
  if (sa_tstun[1] > 0) {
    sa_tstun[1]--;
    speed = 0;
  }
  if (sa_tslow[1] > 0) {
    sa_tslow[1]--;
    speed = speed / 2;
  }
  if (speed > 0)
    SaEntMove(1, speed, direct, sa_px >> 8, sa_py >> 8);
  else
    sa_emov[1] = FALSE;
  /* caught with the phone */
  if (sa_tst[1] == KB_RUSH && sa_phone && !sa_hidden && sa_tstun[1] == 0) {
    dx = SaAbs(sa_ex[1] - sa_px) / 256;
    dy = SaAbs(sa_ey[1] - sa_py) / 256;
    if (dx < 12 && dy < 9) {
      sa_phone = FALSE;
      sa_ring_t = 0;
      sa_write_t = 180;
      sa_tst[1] = KB_WRITE;
      SaSound(SND_TAKE);
      SaMsg2("HERR KIRSCHBAUM: HANDY HER!", "UND SCHREIB DEINEN NAMEN AUF.", 120);
    }
  }
}

/* ---- floors ---- */

U0 SaChangeFloor(I64 nf, I64 up)
{
  I64 i, follow, d;
  for (i = 0; i < 2; i++)
    if (SaTeacherHere(i)) {
      follow = FALSE;
      if (i == 0 && sa_tstun[0] == 0 &&
          (sa_tst[0] == TH_CHASE || sa_tst[0] == TH_GRAB ||
           (sa_tst[0] == TH_SEARCH && sa_ttimer[0] > 150)))
        follow = TRUE;
      if (i == 1 && sa_tst[1] == KB_RUSH && sa_tstun[1] == 0)
        follow = TRUE;
      if (follow) {
        d = SaAbs((sa_ex[i] >> 12) - 3) + SaAbs((sa_ey[i] >> 12) - 19);
        sa_tfollow[i] = 15 + d * 3;
        sa_tfollow_f[i] = nf;
        if (i == 1)
          sa_kb_alert = TRUE;
      }
      sa_tvalid[i] = FALSE;
      if (sa_tst[i] == KB_WRITE)
        sa_tst[i] = KB_PATROL;
    }
  /* the scooter stays down on the schoolyard */
  if (sa_ride >= 0)
    SaParkVehicle();
  SaLoadFloor(nf);
  for (i = 0; i < MAP_N; i++)
    sa_wet[i] = 0;
  sa_ni_valid = FALSE;
  sa_slip_t = 0;
  /* you come out next to the stairs you came by (up: beside the stairs
   * down on the right, and the other way round), walking away from them */
  if (up) {
    sa_px = (5 * 16 + 8) << 8;
    sa_pdir = DIR_LEFT;
  } else {
    sa_px = (2 * 16 + 8) << 8;
    sa_pdir = DIR_RIGHT;
  }
  sa_py = (20 * 16 + 10) << 8;
  sa_stair_arm = FALSE;
  for (i = 0; i < 2; i++) {
    sa_cvalid[i] = FALSE;
    sa_csee[i] = 0;
    if (sa_cf[i] == nf)
      SaPlaceCleaner(i, FALSE);
  }
  for (i = 0; i < 2; i++)
    if (sa_tf[i] == nf && sa_tfollow[i] == 0 && !sa_tvalid[i]) {
      SaSpawnFar(i);
      sa_tvalid[i] = TRUE;
      sa_ebfs[i] = -1;
      SaResetTeacher(i);
    }
  if (sa_cj_st != CJ_OFF && sa_cj_st != CJ_WAIT) {
    SaPlaceEnt(2, 4, 19);
    sa_ebfs[2] = -1;
    if (sa_cj_st == CJ_RUN || sa_cj_st == CJ_FIGHT) {
      sa_cj_st = CJ_GUARD;
      sa_cj_t = 240;
    }
  }
  sa_stair_cd = 20;
  sa_sign_t = 80;
  sa_room = -1;
  sa_hidden = FALSE;
  SaSound(SND_STAIRS);
  SaFov();
  SaCamSnap();
}

U0 SaWin()
{
  sa_state = ST_WIN;
  sa_timer = 0;
  sa_end_ticks = sa_play_ticks;
  sa_ring_t = 0;
  SaSound(SND_WIN);
}

/* noise on this floor: Herr Thüner comes to have a look */
U0 SaNoise()
{
  if (SaTeacherHere(0)) {
    if (sa_tst[0] != TH_GRAB && sa_tst[0] != TH_CHASE && sa_tst[0] != TH_SIT) {
      sa_tst[0] = TH_SEARCH;
      sa_egoal[0] = SaPlayerTile();
      sa_ttimer[0] = 300;
    }
  } else if (sa_tfollow[0] == 0) {
    sa_tfollow_f[0] = sa_floor;
    sa_tfollow[0] = 60;
  }
}

/* Somebody calls Herr Thüner: he comes straight to where you are now,
 * from another floor too */
U0 SaAlertThuner()
{
  sa_alarm_t = 120;
  SaSound(SND_ALARM);
  if (SaTeacherHere(0)) {
    if (sa_tst[0] != TH_GRAB && sa_tst[0] != TH_CHASE) {
      sa_tst[0] = TH_SEARCH;
      sa_egoal[0] = SaPlayerTile();
      sa_tlast[0] = sa_egoal[0];
      sa_ttimer[0] = 420;
    }
  } else {
    sa_tfollow_f[0] = sa_floor;
    if (sa_tfollow[0] == 0 || sa_tfollow[0] > 30)
      sa_tfollow[0] = 30;
  }
}

/* entity e turns to look at you */
U0 SaFacePlayer(I64 e)
{
  I64 dx, dy;
  dx = (sa_px - sa_ex[e]) / 256;
  dy = (sa_py - sa_ey[e]) / 256;
  if (SaAbs(dx) > SaAbs(dy)) {
    if (dx > 0)
      sa_edir[e] = DIR_RIGHT;
    else
      sa_edir[e] = DIR_LEFT;
  } else if (dy > 0)
    sa_edir[e] = DIR_DOWN;
  else
    sa_edir[e] = DIR_UP;
}

/* ---- the cleaners ---- */

U0 SaCleanerTick(I64 c)
{
  I64 e, see, dx, dy, ptx, pty, ex, ey, k, sx, sy;
  e = E_CL + c;
  if (sa_ccd[c] > 0)
    sa_ccd[c]--;
  ptx = sa_px >> 12;
  pty = sa_py >> 12;
  ex = sa_ex[e] >> 12;
  ey = sa_ey[e] >> 12;
  dx = ptx - ex;
  dy = pty - ey;
  see = FALSE;
  if (!sa_hidden && sa_ccd[c] == 0 && dx * dx + dy * dy <= sa_md_clrange[sa_mode])
    see = SaLosTiles(ex, ey, ptx, pty);
  if (see) {
    /* she stops and stares - stay too long and she calls Herr Thüner */
    sa_csee[c]++;
    sa_cst[c] = CL_WATCH;
    if (sa_csee[c] == 40 && sa_msg_t == 0)
      SaSay(2, "PUTZKRAFT: NANU? WER IST DENN DA NOCH?", "(SCHNELL WEG, BEVOR SIE PETZT!)", 70);
    if (sa_csee[c] >= sa_md_clsee[sa_mode]) {
      sa_csee[c] = 0;
      sa_ccd[c] = 900;
      sa_cst[c] = CL_WALK;
      sa_egoal[e] = SaCleanerGoal(e);
      SaSound(SND_SHOUT);
      SaAlertThuner();
      SaSay(2, "PUTZKRAFT: HEEE! HERR THÜNER!", "HIER LÄUFT NOCH EIN SCHÜLER RUM!", 120);
    }
  } else {
    sa_csee[c] = sa_csee[c] - 1;
    if (sa_csee[c] < 0)
      sa_csee[c] = 0;
    if (sa_cst[c] == CL_WATCH && sa_csee[c] == 0)
      sa_cst[c] = CL_WALK;
  }
  if (sa_cst[c] == CL_WATCH) {
    sa_emov[e] = FALSE;
    SaFacePlayer(e);
  } else if (sa_cst[c] == CL_MOP) {
    sa_emov[e] = FALSE;
    sa_ct[c]--;
    if ((sa_ct[c] % 15) == 0)
      for (dy = -1; dy <= 1; dy++)
        for (dx = -1; dx <= 1; dx++)
          SaWetTile(ex + dx, ey + dy, SA_WET);
    if (sa_ct[c] <= 0) {
      sa_cst[c] = CL_WALK;
      sa_egoal[e] = SaCleanerGoal(e);
    }
  } else {
    if (SaEntTile(e) == sa_egoal[e]) {
      /* mop this spot, and put up the yellow sign next to it */
      sa_cst[c] = CL_MOP;
      sa_ct[c] = 150 + SaRand(120);
      sa_csx[c] = -1;
      for (k = 0; k < 4; k++) {
        sx = ex + 1;
        sy = ey;
        if (k == 1)
          sx = ex - 1;
        if (k == 2) {
          sx = ex;
          sy = ey - 1;
        }
        if (k == 3) {
          sx = ex;
          sy = ey + 1;
        }
        if (sa_csx[c] < 0 && SaIsFloorCh(SaTile(sx, sy))) {
          sa_csx[c] = sx;
          sa_csy[c] = sy;
        }
      }
    } else {
      SaEntMove(e, SA_CL_SPEED, FALSE, 0, 0);
      SaWetTile(ex, ey, SA_WET * 2 / 3);
      if (!sa_emov[e])
        sa_egoal[e] = SaCleanerGoal(e);
    }
  }
  /* when her floor is done she moves on (never right in front of you) */
  sa_croam[c]--;
  if (sa_croam[c] <= 0) {
    sa_croam[c] = 1500 + SaRand(900);
    if (SaRand(100) < 40 && SaDistPx(e) > 150 && sa_vis[SaEntTile(e)] == 0) {
      sa_cvalid[c] = FALSE;
      sa_cf[c] = SaCleanerNextFloor(sa_floor);
    }
  }
}

/* cleaners on other floors move from floor to floor */
U0 SaCleanerAway(I64 c)
{
  sa_croam[c]--;
  if (sa_croam[c] > 0)
    return;
  sa_croam[c] = 1500 + SaRand(900);
  sa_cf[c] = SaCleanerNextFloor(sa_cf[c]);
  if (sa_cf[c] == sa_floor) {
    SaPlaceCleaner(c, TRUE);
    if (sa_msg_t == 0)
      SaMsg("DU HÖRST EINEN PUTZEIMER KLAPPERN...", 60);
  }
}

/* ---- Frau Niehaus (3rd floor only) ---- */

U0 SaNiehausTick()
{
  I64 rp, rn, same, d, near, e, add;
  e = E_NI;
  if (sa_floor != F_NIEHAUS) {
    sa_ni_valid = FALSE;
    return;
  }
  if (!sa_ni_valid)
    SaPlaceNiehaus();
  d = SaDistPx(e);
  if (!sa_ni_hum && d < 110) {
    sa_ni_hum = TRUE;
    if (sa_msg_t == 0)
      SaMsg("DU HÖRST JEMANDEN SUMMEN... FRAU NIEHAUS!", 75);
  }
  if (sa_ni_st == NI_SING) {
    sa_emov[e] = FALSE;
    SaFacePlayer(e);
    if (sa_sing_t <= 0) {
      sa_ni_st = NI_COOL;
      sa_ni_t = 450;
      sa_ni_idle = 0;
      sa_egoal[e] = SaNiehausGoal(FALSE);
      SaSay(1, "FRAU NIEHAUS: WUNDERBAR! DU HAST TALENT!", "BIS ZUR NÄCHSTEN CHORPROBE!", 120);
    }
    return;
  }
  if (sa_ni_st == NI_COOL) {
    sa_ni_t--;
    if (sa_ni_t <= 0)
      sa_ni_st = NI_WANDER;
  }
  /* in the same classroom and close by: she wants you to sing with her */
  rp = SaRoomIndex(sa_floor, sa_px >> 12, sa_py >> 12);
  rn = SaRoomIndex(sa_floor, sa_ex[e] >> 12, sa_ey[e] >> 12);
  same = FALSE;
  if (rp >= 0 && rp == rn)
    if (sa_rm_hall[rp] == 0)
      same = TRUE;
  near = FALSE;
  if (d < 64 && !sa_hidden && sa_write_t == 0)
    near = SaLosTiles(sa_ex[e] >> 12, sa_ey[e] >> 12, sa_px >> 12, sa_py >> 12);
  if (same && near && sa_ni_st == NI_WANDER) {
    add = 1;
    /* Herrn Lerschs sheets of music? Those she wants to sing right now */
    if (sa_have[IT_SHEET] > 0 && sa_task <= TK_PIANO)
      add = 2;
    sa_ni_meter = sa_ni_meter + add;
    if (!sa_ni_said) {
      sa_ni_said = TRUE;
      SaSound(SND_NIEHAUS);
      if (add > 1)
        SaSay(1, "FRAU NIEHAUS: OH! SIND DAS NOTENBLÄTTER?", "DIE SINGEN WIR SOFORT! (LAUF WEG!)", 90);
      else
        SaSay(1, "FRAU NIEHAUS: OH, HALLO! SINGST DU MIT?", "SCHNELL RAUS HIER - SONST MUSST DU!", 90);
    }
    sa_emov[e] = FALSE;
    SaFacePlayer(e);
    if (sa_ni_meter >= sa_md_sing[sa_mode]) {
      sa_ni_meter = 0;
      sa_ni_st = NI_SING;
      sa_sing_t = 150;
      sa_sing_step = 0;
      SaSound(SND_NIEHAUS);
      SaAlertThuner();
      SaSay(1, "FRAU NIEHAUS: UND ALLE ZUSAMMEN! LA-LA-LAAA!", "DU MUSST MITSINGEN. HERR THÜNER HÖRT ES!", 150);
    }
    return;
  }
  if (sa_ni_meter > 0) {
    sa_ni_meter = sa_ni_meter - 2;
    if (sa_ni_meter <= 0) {
      sa_ni_meter = 0;
      sa_ni_said = FALSE;
    }
  }
  /* she walks from classroom to classroom, humming */
  if (SaEntTile(e) == sa_egoal[e]) {
    sa_emov[e] = FALSE;
    sa_ni_idle--;
    if (sa_ni_idle <= 0) {
      sa_egoal[e] = SaNiehausGoal(FALSE);
      sa_ni_idle = 90 + SaRand(150);
    }
  } else {
    SaEntMove(e, SA_NI_SPEED, FALSE, 0, 0);
    if (!sa_emov[e])
      sa_egoal[e] = SaNiehausGoal(FALSE);
  }
}

/* ---- wet floors ---- */

U0 SaSlip()
{
  sa_slip_t = 50;
  sa_stamina = sa_stamina - 250;
  if (sa_stamina < 0)
    sa_stamina = 0;
  sa_shake = 10;
  sa_running = FALSE;
  SaSound(SND_SLIP);
  SaMsg2("AUSGERUTSCHT! PLATSCH!", "NASSER BODEN - HIER NICHT RENNEN!", 90);
  SaNoise();
}

/* ---- e-scooters and bikes ---- */

I64 SaNearVehicle()
{
  I64 k, best, bd, dx, dy;
  best = -1;
  bd = 9999;
  if (sa_floor != F_OUTSIDE || sa_ride >= 0)
    return -1;
  for (k = 0; k < sa_vh_n; k++) {
    dx = SaAbs(sa_vh_x[k] - sa_px) / 256;
    dy = SaAbs(sa_vh_y[k] - sa_py) / 256;
    if (dx < 20 && dy < 14 && dx + dy < bd) {
      bd = dx + dy;
      best = k;
    }
  }
  return best;
}

U0 SaRideVehicle(I64 k)
{
  if (sa_vh_charge[k] <= 0) {
    if (sa_vh_t[k] == VH_SCOOTER)
      SaMsg("DER AKKU VON DEM E-SCOOTER IST LEER.", 60);
    else
      SaMsg("DIE KETTE VON DEM FAHRRAD IST KAPUTT.", 60);
    return;
  }
  sa_ride = k;
  sa_px = sa_vh_x[k];
  sa_py = sa_vh_y[k];
  sa_pdir = sa_vh_dir[k];
  if (sa_vh_t[k] == VH_SCOOTER)
    SaMsg2("DU STEIGST AUF DEN E-SCOOTER. SSSSSST!", "(DER AKKU SIEHT NICHT SO VOLL AUS...)", 75);
  else
    SaMsg2("DU SCHWINGST DICH AUFS FAHRRAD!", "(DIE KETTE KLAPPERT KOMISCH...)", 75);
}

U0 SaVehicleTick()
{
  I64 k;
  if (sa_ride < 0)
    return;
  if (sa_pmoving)
    sa_vh_charge[sa_ride]--;
  if (sa_vh_charge[sa_ride] > 0)
    return;
  k = sa_ride;
  sa_vh_charge[k] = 0;
  SaParkVehicle();
  SaSound(SND_BROKEN);
  sa_shake = 6;
  if (sa_vh_t[k] == VH_SCOOTER)
    SaMsg2("PIEP PIEP... AKKU LEER!", "DER E-SCOOTER BLEIBT EINFACH STEHEN.", 90);
  else
    SaMsg2("KNACK! DIE KETTE IST GERISSEN.", "DAS FAHRRAD KANNST DU VERGESSEN.", 90);
}

/* finishes every task whose items are all collected */
U0 SaCheckTask()
{
  I64 t;
  t = sa_task_item[sa_task];
  if (t < 0 || sa_have[t] < sa_task_need[sa_task])
    return;
  if (sa_task == TK_STAIRKEY) {
    SaUnlock(2);
    SaMsg2("DER TREPPENSCHLÜSSEL!", "DAS GITTER ZUR 3. ETAGE IST JETZT OFFEN.", 150);
  } else if (sa_task == TK_SHEETS)
    SaMsg2("ALLE 4 NOTENBLÄTTER!", "AB ZUM KLAVIER: MUSIKRAUM 4.25 (4. ETAGE).", 150);
  else if (sa_task == TK_FUSES)
    SaMsg2("ALLE 3 SICHERUNGEN!", "AB ZUM SICHERUNGSKASTEN IM FLUR.", 150);
  else if (sa_task == TK_POOLKEY)
    SaMsg2("DER BADEMEISTER-SCHLÜSSEL!", "JETZT KOMMST DU IN DIE SCHWIMMHALLE.", 150);
  else if (sa_task == TK_YARDKEY) {
    SaUnlock(4);
    SaMsg2("DER SCHULHOF-SCHLÜSSEL!", "DER WEG ZUR 5. ETAGE (SCHULHOF) IST FREI.", 150);
  } else if (sa_task == TK_NOTES) {
    SaUnlock(5);
    SaStrCpy(sa_mb1, "DER CODE IST ");
    SaStrNum(sa_mb1, sa_code[0]);
    SaStrNum(sa_mb1, sa_code[1]);
    SaStrNum(sa_mb1, sa_code[2]);
    SaStrCat(sa_mb1, "! KLICK.");
    SaMsg2(sa_mb1, "DAS TREPPENHAUS ZUR 6. ETAGE IST OFFEN.", 150);
  } else if (sa_task == TK_MAINKEY) {
    SaMsg2("DER HAUSMEISTERSCHLÜSSEL!", "JETZT RAUS DURCH DEN HAUPTEINGANG!", 150);
    if (SaTeacherHere(0)) {
      if (sa_tst[0] != TH_GRAB && sa_tst[0] != TH_SIT) {
        sa_tst[0] = TH_SEARCH;
        sa_egoal[0] = SaPlayerTile();
        sa_ttimer[0] = 300;
      }
    } else if (sa_tfollow[0] == 0) {
      sa_tfollow_f[0] = sa_floor;
      sa_tfollow[0] = 90;
    }
  }
  SaNextTask();
}

U0 SaPickup(I64 k)
{
  I64 t, n;
  t = sa_it_t[k];
  sa_it_got[k] = TRUE;
  sa_have[t]++;
  if (sa_it_art[t] == ART_KEY || sa_it_art[t] == ART_KEY2)
    SaSound(SND_KEY);
  else
    SaSound(SND_PICKUP);
  SaStrCpy(sa_mb1, sa_it_name[t]);
  SaStrCpy(sa_mb2, sa_it_msg[t]);
  if (t == IT_STICKER) {
    SaStrCpy(sa_mb2, "ST.-ANNA-STICKER ");
    SaStrNum(sa_mb2, sa_have[t]);
    SaStrCat(sa_mb2, "/");
    SaStrNum(sa_mb2, NSTICKERS);
  } else if (t == IT_SHEET || t == IT_FUSE) {
    SaStrCpy(sa_mb2, "SCHON ");
    SaStrNum(sa_mb2, sa_have[t]);
    SaStrCat(sa_mb2, " VON ");
    SaStrNum(sa_mb2, sa_task_need[sa_task]);
  } else if (t == IT_NOTE) {
    n = sa_have[t];
    if (n > 3)
      n = 3;
    SaStrCpy(sa_mb2, "DARAUF STEHT: ");
    SaStrNum(sa_mb2, n);
    SaStrCat(sa_mb2, ". ZIFFER = ");
    SaStrNum(sa_mb2, sa_code[n - 1]);
  } else if (t == IT_DRINK || t == IT_BOMB) {
    SaStrCat(sa_mb1, " (");
    SaStrNum(sa_mb1, sa_have[t]);
    SaStrCat(sa_mb1, "X)");
  }
  SaMsg2(sa_mb1, sa_mb2, 110);
  if (sa_hint == k)
    sa_hint = -1;
  SaCheckTask();
  /* what you picked up stays yours, even if Herr Thüner gets you */
  SaSaveCp();
}

/* TRUE when tile ch is next to the player (8 neighbours) */
I64 SaNearTile(I64 ch)
{
  I64 x, y, ptx, pty;
  ptx = sa_px >> 12;
  pty = sa_py >> 12;
  for (y = pty - 1; y <= pty + 1; y++)
    for (x = ptx - 1; x <= ptx + 1; x++)
      if (SaTile(x, y) == ch)
        return TRUE;
  return FALSE;
}

U0 SaPlayPiano()
{
  if (sa_task < TK_PIANO) {
    SaMsg("EIN KLAVIER. OHNE NOTEN KANNST DU NICHTS SPIELEN.", 75);
    return;
  }
  if (sa_task > TK_PIANO) {
    SaSound(SND_PIANO);
    SaMsg("PLING. (DAS GEHEIMFACH IST SCHON LEER.)", 60);
    return;
  }
  SaSound(SND_PIANO);
  SaNoise();
  SaUnlock(1);
  SaMsg2("DU SPIELST DAS LIED... KLACK! EIN GEHEIMFACH:",
         "DER HALLENSCHLÜSSEL! DIE 2. ETAGE IST OFFEN.", 150);
  SaNextTask();
}

U0 SaFuseBox()
{
  if (sa_task < TK_FUSEBOX) {
    SaMsg2("DER SICHERUNGSKASTEN. ALLES DUNKEL.", "ES FEHLEN 3 SICHERUNGEN.", 90);
    return;
  }
  if (sa_task > TK_FUSEBOX) {
    SaMsg("DER STROM LÄUFT WIEDER.", 45);
    return;
  }
  sa_power = TRUE;
  SaSound(SND_POWER);
  sa_unlock[0] = TRUE;
  SaRenderWorld();
  SaMsg2("ZZZT... DAS LICHT GEHT AN!", "DAS GITTER ZUR 1. ETAGE IST OFFEN.", 150);
  SaNextTask();
}

/* the action button: hide, talk, play, repair */
U0 SaAction()
{
  if (sa_hidden) {
    SaToggleHide();
    return;
  }
  if (sa_ride >= 0) {
    SaParkVehicle();
    SaMsg("DU STEIGST AB.", 40);
    return;
  }
  if (sa_vh_near >= 0) {
    SaRideVehicle(sa_vh_near);
    return;
  }
  if (SaLerschDist() < 40) {
    SaTalkLersch();
    return;
  }
  if (sa_near_obj == 'M') {
    SaPlayPiano();
    return;
  }
  if (sa_near_obj == 'u') {
    SaFuseBox();
    return;
  }
  SaToggleHide();
}

/* message for walking into something locked */
U0 SaLockedMsg(I64 ch)
{
  I64 f;
  if (ch == '^' || ch == 'v') {
    f = sa_floor + 1;
    if (ch == 'v')
      f = sa_floor - 1;
    SaStrCpy(sa_mb1, "GITTER ZU! DIE ");
    SaStrNum(sa_mb1, f + 1);
    SaStrCat(sa_mb1, ". ETAGE IST NOCH GESPERRT.");
    SaMsg2(sa_mb1, SaGoalText(), 90);
  } else if (ch == 'E')
    SaMsg2("ABGESCHLOSSEN!", "DU BRAUCHST DEN HAUSMEISTERSCHLÜSSEL.", 90);
  else if (ch == 'X')
    SaMsg2("DIE SCHWIMMHALLE IST ABGESCHLOSSEN.", "DEN SCHLÜSSEL HAT DER BADEMEISTER.", 90);
  else if (ch == 'g')
    SaMsg("7. BIS 9. ETAGE: GESPERRT.", 75);
  else
    SaMsg2("DAS SCHULTOR IST ZU.", "DER HAUPTEINGANG IST IN DER 6. ETAGE.", 90);
  SaSound(SND_LOCKED);
  sa_locked_t = 60;
}

U0 SaPlayTick(I64 keys, I64 pressed)
{
  I64 dx, dy, d, r, ch, i, slip, frozen;
  sa_play_ticks++;
  if (sa_msg_t > 0)
    sa_msg_t--;
  if (sa_spot_t > 0)
    sa_spot_t--;
  if (sa_pulse > 0)
    sa_pulse--;
  if (sa_shake > 0)
    sa_shake--;
  if (sa_room_t > 0)
    sa_room_t--;
  if (sa_locked_t > 0)
    sa_locked_t--;
  if (sa_stair_cd > 0)
    sa_stair_cd--;
  if (sa_sign_t > 0)
    sa_sign_t--;
  if (sa_call_cd > 0)
    sa_call_cd--;
  if (sa_boost > 0)
    sa_boost--;
  if (sa_bomb_t > 0)
    sa_bomb_t--;
  if (sa_alarm_t > 0)
    sa_alarm_t--;
  if (sa_sing_t > 0) {
    sa_sing_t--;
    sa_sing_step++;
  }
  for (i = 0; i < MAP_N; i++)
    if (sa_wet[i] > 0)
      sa_wet[i]--;

  if ((pressed & K_ESC) != 0) {
    sa_state = ST_PAUSE;
    sa_pause_sel = 0;
    sa_pause_msg_t = 0;
    return;
  }

  /* writing your name for Herr Kirschbaum: you cannot move */
  if (sa_write_t > 0) {
    sa_write_t--;
    if (sa_write_t == 0) {
      if (sa_tst[1] == KB_WRITE)
        sa_tst[1] = KB_PATROL;
      SaMsg("HERR KIRSCHBAUM: SO. UND JETZT AB MIT DIR.", 75);
    }
  }

  sa_le_near = FALSE;
  if (SaLerschDist() < 40)
    sa_le_near = TRUE;
  sa_near_obj = 0;
  if (SaNearTile('M'))
    sa_near_obj = 'M';
  if (sa_floor == 1 && SaNearTile('u'))
    sa_near_obj = 'u';
  if ((pressed & K_PHONE) != 0)
    SaPhoneButton();
  sa_vh_near = SaNearVehicle();
  /* while you slip or have to sing you cannot do anything */
  frozen = FALSE;
  if (sa_write_t > 0 || sa_slip_t > 0 || sa_sing_t > 0)
    frozen = TRUE;
  if ((pressed & K_ACT) != 0 && !frozen)
    SaAction();
  if (sa_state != ST_PLAY)
    return;
  if (!sa_hidden && !frozen)
    SaPlayerMove(keys);
  else
    SaPlayerMove(0);
  SaVehicleTick();
  /* running on a wet floor, or right past a cleaner's mop */
  if (sa_slip_t > 0) {
    sa_slip_t--;
    sa_crouch = 12;
  } else if (sa_running && sa_ride < 0) {
    slip = FALSE;
    if (sa_wet[SaPlayerTile()] > 0)
      slip = TRUE;
    for (i = 0; i < 2; i++)
      if (sa_cvalid[i] && sa_cf[i] == sa_floor && SaDistPx(E_CL + i) < 22)
        slip = TRUE;
    if (slip)
      SaSlip();
  }

  /* stairs: only once you have stepped off them after arriving */
  ch = SaTile(sa_px >> 12, sa_py >> 12);
  if (ch != '^' && ch != 'v')
    sa_stair_arm = TRUE;
  if (sa_stair_cd == 0 && !frozen && sa_stair_arm) {
    if (ch == '^' && SaStairOpen(TRUE)) {
      SaChangeFloor(sa_floor + 1, TRUE);
      return;
    }
    if (ch == 'v' && SaStairOpen(FALSE)) {
      SaChangeFloor(sa_floor - 1, FALSE);
      return;
    }
  }

  if (sa_hit_tile != 0) {
    ch = sa_hit_tile;
    sa_hit_tile = 0;
    if (ch == 'E' && sa_task == TK_EXIT) {
      SaWin();
      return;
    }
    if (ch == 'X' && sa_task >= TK_YARDKEY && !sa_pool_open) {
      sa_pool_open = TRUE;
      SaSaveCp();
      SaLoadFloor(0);
      SaSound(SND_UNLOCK);
      SaMsg2("DU SCHLIESST DIE SCHWIMMHALLE AUF.", "IRGENDWO AM BECKEN LIEGT DER SCHLÜSSEL.", 120);
      sa_locked_t = 30;
    } else if (sa_locked_t == 0)
      SaLockedMsg(ch);
  }

  /* picking things up */
  for (i = 0; i < sa_it_n; i++)
    if (!sa_it_got[i] && sa_it_f[i] == sa_floor) {
      dx = SaAbs((sa_px >> 8) - (sa_it_x[i] * 16 + 8));
      dy = SaAbs((sa_py >> 8) - (sa_it_y[i] * 16 + 10));
      if (dx < 11 && dy < 11)
        SaPickup(i);
    }

  r = SaRoomIndex(sa_floor, sa_px >> 12, sa_py >> 12);
  if (r >= 0 && r != sa_room) {
    sa_room = r;
    sa_room_t = 60;
    /* room 0 is the swimming hall, where Conjoke does his lengths */
    if (r == 0 && !sa_cj_met && sa_cj_st == CJ_OFF) {
      sa_cj_met = TRUE;
      SaMsg2("CONJOKE (IM BECKEN): YO BRO, WAS GEHT?", "ICH SCHWIMM NOCH 'NE RUNDE. RUF MICH AN!", 120);
    }
  }

  /* the phone rings now and then (Conjoke) - Herr Kirschbaum hears it */
  if (sa_phone && sa_write_t == 0 && sa_ring_t == 0) {
    sa_next_ring--;
    if (sa_next_ring <= 0)
      SaRingStart();
  }
  if (sa_ring_t > 0) {
    sa_ring_t--;
    sa_ring_step++;
    if (sa_ring_t == 0 && sa_phone) {
      SaMsg("VERPASSTER ANRUF: CONJOKE", 60);
      sa_next_ring = 1500 + SaRand(1500);
    }
  }

  SaFov();
  for (i = 0; i < 2; i++)
    if (!SaTeacherHere(i))
      SaTeacherAway(i);
  if (SaTeacherHere(0))
    SaThunerTick();
  if (sa_state != ST_PLAY)
    return;
  if (SaTeacherHere(1))
    SaKirschbaumTick();
  for (i = 0; i < 2; i++)
    if (sa_cvalid[i] && sa_cf[i] == sa_floor)
      SaCleanerTick(i);
    else
      SaCleanerAway(i);
  SaNiehausTick();
  if (sa_state != ST_PLAY)
    return;
  SaConjokeTick();
  SaCamUpdate();

  /* without the flashlight you only have the glow of your phone */
  sa_light_r = 66 + SaRand(3);
  if (sa_have[IT_LAMP] > 0)
    sa_light_r = 92 + SaRand(3);
  if (sa_floor == F_OUTSIDE)
    sa_light_r = sa_light_r + 20;
  if (SaTeacherHere(0)) {
    d = SaDistPx(0);
    if (sa_tst[0] == TH_SIT)
      sa_light_r = 150 - SaRand(4);
    else if (d < 110) {
      sa_light_r = 66 + SaRand(30);
      if (SaRand(16) == 0)
        sa_light_r = 24;
    }
    if (sa_tst[0] != TH_SIT && d < 170) {
      sa_heart_t--;
      if (sa_heart_t <= 0) {
        SaSound(SND_HEART);
        sa_pulse = 6;
        sa_heart_t = 8 + d / 7;
      }
    }
  }
}

/* ------------------------------------------------------------------ */
/* music sequencer, driven by game ticks                               */
/* ------------------------------------------------------------------ */

U0 SaMusicTick()
{
  I64 want, i, n, off;
  want = TRK_NONE;
  if (sa_state == ST_TITLE)
    want = TRK_TITLE;
  else if (sa_state == ST_INTRO)
    want = TRK_EXPLORE;
  else if (sa_state == ST_PLAY) {
    want = TRK_EXPLORE;
    if (SaTeacherHere(0) && (sa_tst[0] == TH_CHASE || sa_tst[0] == TH_GRAB) && sa_tstun[0] == 0)
      want = TRK_CHASE;
  } else if (sa_state == ST_OVER)
    want = TRK_OVER;
  else if (sa_state == ST_WIN)
    want = TRK_WIN;
  if (want != sa_mus_trk) {
    sa_mus_trk = want;
    sa_mus_step = 0;
    sa_mus_tick = 0;
  }
  for (i = 0; i < 4; i++)
    sa_vage[i]++;
  if (sa_mus_trk != TRK_NONE) {
    if (sa_mus_tick == 0) {
      off = sa_trk_off[sa_mus_trk] + sa_mus_step;
      n = sa_mus_lead[off];
      if (n != 0) {
        sa_vnote[0] = n;
        sa_vtrig[0]++;
        sa_vage[0] = 0;
      }
      n = sa_mus_bass[off];
      if (n != 0) {
        sa_vnote[1] = n;
        sa_vtrig[1]++;
        sa_vage[1] = 0;
      }
      n = sa_mus_drum[off];
      if (n != 0) {
        sa_vnote[2] = n;
        sa_vtrig[2]++;
        sa_vage[2] = 0;
      }
      sa_mus_step++;
      if (sa_mus_step >= sa_trk_len[sa_mus_trk])
        sa_mus_step = 0;
    }
    sa_mus_tick++;
    if (sa_mus_tick >= sa_trk_tps[sa_mus_trk])
      sa_mus_tick = 0;
  }
  if (sa_ring_t > 0 && (sa_ring_step % 3) == 0) {
    n = sa_ring_mel[(sa_ring_step / 3) % 12];
    if (n != 0) {
      sa_vnote[3] = n;
      sa_vtrig[3]++;
      sa_vage[3] = 0;
    }
  } else if (sa_ring_t == 0 && sa_sing_t > 0 && (sa_sing_step % 4) == 0) {
    /* Frau Niehaus' song - and you sing along */
    n = sa_sing_mel[(sa_sing_step / 4) % 32];
    if (n != 0) {
      sa_vnote[3] = n;
      sa_vtrig[3]++;
      sa_vage[3] = 0;
    }
  }
}

/* ------------------------------------------------------------------ */
/* rendering the play view                                             */
/* ------------------------------------------------------------------ */

/* Flashlight (lx,ly,r) plus an optional lamp (lx2,ly2,r2 > 0) and a
 * minimum brightness amb (moonlight outside). Four levels, dithered. */
U0 SaLight(I64 camx, I64 camy, I64 lx, I64 ly, I64 r, I64 lx2, I64 ly2, I64 r2, I64 amb)
{
  I64 x, y, wy, row, dy2, dyb, dx, d, b, e1, e2, e3, f1, f2, f3, dith, dithb, i, lvl, lvl2;
  e1 = r * r * 36 / 100;
  e2 = r * r * 64 / 100;
  e3 = r * r;
  dith = (e2 - e1) / 16;
  f1 = r2 * r2 * 36 / 100;
  f2 = r2 * r2 * 64 / 100;
  f3 = r2 * r2;
  dithb = (f2 - f1) / 16;
  i = 0;
  for (y = 0; y < SCR_H; y++) {
    wy = camy + y;
    if (wy < 0 || wy >= WORLD_H) {
      SaMemSet(sa_fb + i, 0, SCR_W);
      i = i + SCR_W;
    } else {
      row = (wy >> 4) * MAP_W;
      dy2 = (wy - ly) * (wy - ly);
      dyb = (wy - ly2) * (wy - ly2);
      for (x = 0; x < SCR_W; x++) {
        if (sa_vis[row + ((camx + x) >> 4)] == 0)
          sa_fb[i] = 0;
        else {
          dx = camx + x - lx;
          b = sa_bayer[((y & 3) << 2) + (x & 3)];
          d = dx * dx + dy2 + b * dith;
          lvl = 3;
          if (d >= e1) {
            lvl = 0;
            if (d < e2)
              lvl = 2;
            else if (d < e3)
              lvl = 1;
          }
          if (lvl < 3 && r2 > 0) {
            dx = camx + x - lx2;
            d = dx * dx + dyb + b * dithb;
            lvl2 = 3;
            if (d >= f1) {
              lvl2 = 0;
              if (d < f2)
                lvl2 = 2;
              else if (d < f3)
                lvl2 = 1;
            }
            if (lvl2 > lvl)
              lvl = lvl2;
          }
          if (lvl < amb)
            lvl = amb;
          if (lvl < 3)
            sa_fb[i] = sa_ltab[(lvl << 4) + sa_fb[i]];
        }
        i++;
      }
    }
  }
}

/* Things to pick up, drawn after the lighting so they never drown in
 * shadow. Out of the light only a sparkle shows (stickers stay hidden). */
U0 SaDrawItems(I64 camx, I64 camy, I64 amb)
{
  I64 k, t, ix, iy, dx, dy;
  for (k = 0; k < sa_it_n; k++)
    if (!sa_it_got[k] && sa_it_f[k] == sa_floor &&
        sa_vis[sa_it_y[k] * MAP_W + sa_it_x[k]] != 0) {
      t = sa_it_t[k];
      ix = sa_it_x[k] * 16 + 4;
      iy = sa_it_y[k] * 16 + 5;
      dx = ix + 4 - (sa_px >> 8);
      dy = iy + 4 - (sa_py >> 8) + 8;
      if (amb == 0 && dx * dx + dy * dy > sa_light_r * sa_light_r) {
        if (sa_it_glint[t] && ((sa_tick + k * 7) % 24) < 3)
          SaPutFb(ix + 3 - camx, iy + 2 - camy, 15);
      } else {
        if (sa_it_glint[t] && ((sa_tick / 10 + k) % 2) == 0)
          iy = iy - 1;
        SaDrawArt(sa_fb, SCR_W, SCR_H, ix - camx, iy - camy, sa_it_art[t], 8, 8, FALSE, 1, 1);
        if (((sa_tick + k * 5) % 24) < 4)
          SaPutFb(ix + 2 - camx, iy + 1 - camy, 6);
      }
    }
}

/* the green cloud of a stink bomb */
U0 SaDrawStink(I64 x, I64 y)
{
  I64 i;
  for (i = 0; i < 6; i++)
    SaDisc(x + SaRand(29) - 14, y - 16 + SaRand(21) - 10, 4 + SaRand(4), 13 + (i & 1));
  SaTextSh(x - 15, y - 44, "HUST!", 14, 1, 1);
}

/* What you still see of Herr Thüner in the dark: his shiny head and,
 * when he is after you, his eyes. */
U0 SaGlint(I64 hx, I64 hy)
{
  I64 d, c;
  d = SaDistPx(0);
  if (d > 210)
    return;
  if (SaGetFb(hx - 1, hy - 26) == 0 && (sa_tick % 40) < 32) {
    c = 4;
    if (d < 120)
      c = 5;
    SaPutFb(hx - 1, hy - 26, c);
    SaPutFb(hx, hy - 26, c);
    SaPutFb(hx - 1, hy - 25, 3);
  }
  if ((sa_tst[0] == TH_CHASE || sa_tst[0] == TH_GRAB) && sa_tstun[0] == 0) {
    if (sa_edir[0] == DIR_DOWN) {
      SaPutFb(hx - 3, hy - 22, 12);
      SaPutFb(hx - 2, hy - 22, 12);
      SaPutFb(hx + 1, hy - 22, 12);
      SaPutFb(hx + 2, hy - 22, 12);
    } else if (sa_edir[0] == DIR_RIGHT)
      SaPutFb(hx + 2, hy - 22, 12);
    else if (sa_edir[0] == DIR_LEFT)
      SaPutFb(hx - 3, hy - 22, 12);
  }
}

/* Herr Kirschbaum's glasses flash in the dark when he comes running */
U0 SaKbGlint(I64 x, I64 y)
{
  if (SaGetFb(x - 2, y - 22) != 0 || (sa_tick % 20) > 14)
    return;
  SaPutFb(x - 3, y - 22, 6);
  SaPutFb(x + 2, y - 22, 6);
  SaPutFb(x - 4, y - 23, 5);
  SaPutFb(x + 3, y - 23, 5);
}

/* a cartoon dust cloud, nobody gets hurt */
U0 SaDrawFight(I64 x, I64 y)
{
  I64 i;
  for (i = 0; i < 8; i++)
    SaDisc(x + SaRand(25) - 12, y - 14 + SaRand(17) - 8, 5 + SaRand(4), 4 + (i & 1));
  for (i = 0; i < 3; i++)
    SaRectFb(x + SaRand(30) - 15, y - 14 + SaRand(20) - 10, 2, 6, 9);
  for (i = 0; i < 4; i++)
    SaPutFb(x + SaRand(36) - 18, y - 30 + SaRand(12), 15);
  SaTextSh(x - 26, y - 46, "RAUFEREI!", 12, 1, 1);
}

U0 SaDrawEnt(I64 e, I64 camx, I64 camy)
{
  I64 x, y, frame, eyec;
  x = (sa_ex[e] >> 8) - camx;
  y = (sa_ey[e] >> 8) - camy;
  frame = 0;
  if (sa_emov[e])
    frame = (sa_eanim[e] / 1300) % 4;
  if (e == 2) {
    if (sa_cj_st == CJ_FIGHT)
      return;
    SaDrawConjoke(sa_fb, SCR_W, SCR_H, x, y, sa_edir[2], frame, 1);
    return;
  }
  if (e >= E_CL && e < E_NI) {
    SaDrawAdult(sa_fb, SCR_W, SCR_H, x, y, sa_edir[e], frame, 1, 1, 4);
    if (sa_edir[e] != DIR_UP)
      SaDrawMop(x, y, sa_edir[e], sa_cst[e - E_CL] == CL_MOP, sa_tick);
    return;
  }
  if (e == E_NI) {
    SaDrawAdult(sa_fb, SCR_W, SCR_H, x, y, sa_edir[e], frame, 1, 1, 3);
    return;
  }
  if (sa_tstun[e] > 0 && sa_cj_st == CJ_FIGHT && sa_cj_tgt == e)
    return;
  eyec = 1;
  if (e == 0 && (sa_tst[0] == TH_CHASE || sa_tst[0] == TH_GRAB))
    eyec = 12;
  SaDrawAdult(sa_fb, SCR_W, SCR_H, x, y, sa_edir[e], frame, eyec, 1, e);
}

/* a scooter or bike (side view) with its front wheel at x, ground at y */
U0 SaDrawVehicle(I64 x, I64 y, I64 t, I64 dir, I64 broken)
{
  I64 art, fl;
  art = ART_SCOOTER;
  if (t == VH_BIKE)
    art = ART_BIKE;
  fl = FALSE;
  if (dir == DIR_LEFT)
    fl = TRUE;
  SaDrawArt(sa_fb, SCR_W, SCR_H, x - 16, y - 19, art, 16, 10, fl, 2, 1);
  if (broken && ((sa_tick / 12) & 1) == 0) {
    if (t == VH_SCOOTER) {
      /* empty battery */
      SaRectFb(x - 4, y - 30, 9, 5, 12);
      SaRectFb(x - 3, y - 29, 7, 3, 1);
      SaRectFb(x + 5, y - 29, 1, 3, 12);
    } else {
      /* the chain hangs down */
      SaPutFb(x - 4, y + 1, 4);
      SaPutFb(x - 1, y + 2, 4);
      SaPutFb(x + 2, y + 1, 4);
    }
  }
}

U0 SaDrawPlayerAt(I64 x, I64 y)
{
  I64 frame, crouch;
  frame = 0;
  if (sa_pmoving)
    frame = (sa_panim / 1700) % 4;
  if (sa_hidden)
    return;
  crouch = sa_crouch;
  if (sa_ride >= 0) {
    /* standing on the scooter / sitting on the bike */
    frame = 0;
    if (crouch < 4)
      crouch = 4;
    SaDrawGiant(sa_fb, SCR_W, SCR_H, x, y - 3, sa_pdir, frame, crouch, 1);
    SaDrawVehicle(x, y + 1, sa_vh_t[sa_ride], sa_pdir, FALSE);
    return;
  }
  SaDrawGiant(sa_fb, SCR_W, SCR_H, x, y, sa_pdir, frame, crouch, 1);
  if (sa_slip_t > 0) {
    /* stars around the head */
    SaPutFb(x - 10 + (sa_tick % 20), y - 50, 15);
    SaPutFb(x + 9 - (sa_tick % 20), y - 46, 15);
    SaPutFb(x - 4 + ((sa_tick / 2) % 9), y - 54, 6);
  }
}

/* a little note, for the singing */
U0 SaDrawNote(I64 x, I64 y, I64 c)
{
  SaRectFb(x, y + 4, 3, 2, c);
  SaRectFb(x + 2, y, 1, 5, c);
  SaRectFb(x + 3, y, 2, 1, c);
}

/* the yellow "Vorsicht, Rutschgefahr" sign of a cleaner */
U0 SaDrawWetSign(I64 x, I64 y)
{
  I64 i;
  for (i = 0; i < 9; i++)
    SaRectFb(x - i / 3 - 1, y - 12 + i, 3 + (i / 3) * 2, 1, 15);
  SaRectFb(x, y - 9, 1, 3, 1);
  SaPutFb(x, y - 5, 1);
  SaRectFb(x - 4, y - 3, 1, 3, 8);
  SaRectFb(x + 4, y - 3, 1, 3, 8);
}

/* Wet tiles: a blue puddle that shrinks while it dries, with a bright
 * edge. Neighbouring puddles flow into each other. */
U0 SaDrawWet(I64 camx, I64 camy)
{
  I64 tx, ty, x, y, wx, wy, w, r, dx, dy, d, cx, cy;
  for (ty = camy / 16; ty <= (camy + SCR_H) / 16; ty++)
    for (tx = camx / 16; tx <= (camx + SCR_W) / 16; tx++)
      if (tx >= 0 && ty >= 0 && tx < MAP_W && ty < MAP_H) {
        w = sa_wet[ty * MAP_W + tx];
        if (w > 0) {
          r = 5 + w / 180;
          if (r > 11)
            r = 11;
          cx = 6 + (SaHash(tx, ty) & 3);
          cy = 7 + (SaHash(ty, tx) & 1);
          for (y = 0; y < 16; y++)
            for (x = 0; x < 16; x++) {
              dx = x - cx;
              dy = (y - cy) * 3 / 2;
              d = dx * dx + dy * dy + (SaHash(tx * 16 + x, ty * 16 + y) & 7);
              if (d < r * r) {
                wx = tx * 16 + x - camx;
                wy = ty * 16 + y - camy;
                if (d > (r - 2) * (r - 2) && dy < 0)
                  SaPutFb(wx, wy, 6);
                else if (((x + y * 3) % 5) == 0)
                  SaPutFb(wx, wy, 5);
                else
                  SaPutFb(wx, wy, 13);
              }
            }
        }
      }
}

/* puddles glint in the dark, like the items, so you see them coming */
U0 SaWetGlint(I64 camx, I64 camy)
{
  I64 tx, ty, x, y;
  for (ty = camy / 16; ty <= (camy + SCR_H) / 16; ty++)
    for (tx = camx / 16; tx <= (camx + SCR_W) / 16; tx++)
      if (tx >= 0 && ty >= 0 && tx < MAP_W && ty < MAP_H)
        if (sa_wet[ty * MAP_W + tx] > 0 && sa_vis[ty * MAP_W + tx] != 0)
          if (((sa_tick / 4 + tx * 3 + ty * 5) % 10) < 3) {
            x = tx * 16 + 4 + (SaHash(tx, ty) & 7) - camx;
            y = ty * 16 + 5 + (SaHash(ty, tx) & 3) - camy;
            if (SaGetFb(x, y) != 6) {
              SaPutFb(x, y, 6);
              SaPutFb(x + 1, y, 13);
              SaPutFb(x, y + 1, 13);
            }
          }
}

/* Conjoke doing his lengths in the pool, seen through the glass */
U0 SaDrawSwimmer(I64 camx, I64 camy)
{
  I64 t, x, y, flip;
  if (sa_floor != 0 || sa_cj_st != CJ_OFF)
    return;
  t = sa_tick % 600;
  flip = FALSE;
  if (t < 300)
    x = 80 + t * 2;
  else {
    x = 680 - (t - 300) * 2;
    flip = TRUE;
  }
  y = 120;
  x = x - camx;
  y = y - camy;
  SaDrawArt(sa_fb, SCR_W, SCR_H, x - 5, y - 6, ART_CJ_S, 10, 10, flip, 1, 1);
  if (((t / 8) % 2) == 0) {
    SaRectFb(x - 8, y + 2, 4, 2, 9);
    SaPutFb(x - 9, y, 6);
    SaPutFb(x + 7, y + 1, 6);
  } else {
    SaRectFb(x + 4, y + 2, 4, 2, 9);
    SaPutFb(x + 9, y, 6);
    SaPutFb(x - 7, y + 1, 6);
  }
  SaRectFb(x - 6, y + 4, 12, 1, 6);
}

/* Herr Lersch with his tablet, talking with his hands */
U0 SaDrawLersch(I64 camx, I64 camy)
{
  I64 dir, x, y, f;
  dir = DIR_LEFT;
  f = 1;
  if (sa_px > ((sa_le_tx * 16 + 8) << 8)) {
    dir = DIR_RIGHT;
    f = -1;
  }
  x = sa_le_tx * 16 + 8 - camx;
  y = sa_le_ty * 16 + 10 - camy;
  SaDrawAdult(sa_fb, SCR_W, SCR_H, x, y, dir, 0, 1, 1, 2);
  /* the tablet in front of him */
  SaRectFb(x - f * 6 - 3, y - 14, 6, 5, 1);
  SaRectFb(x - f * 6 - 2, y - 13, 4, 3, 13);
  SaPutFb(x - f * 6 - 1, y - 13, 6);
  /* and the other hand gestures */
  if (((sa_tick / 14) & 1) == 0 || sa_le_near)
    SaRectFb(x + f * 4 - 1, y - 19 - ((sa_tick / 7) & 1) * 2, 2, 2, 9);
}

U0 SaLockerView()
{
  I64 x, y, i, v, k, slit, c;
  i = 0;
  for (y = 0; y < SCR_H; y++)
    for (x = 0; x < SCR_W; x++) {
      slit = FALSE;
      if (x >= 70 && x < 250 && y >= 50 && y < 140) {
        v = (y - 50) % 18;
        if (v >= 4 && v < 11)
          slit = TRUE;
      }
      if (!slit) {
        c = 2;
        if (x < 8 || x >= SCR_W - 8 || x == 40 || x == 280)
          c = 1;
        else if (((x * 7 + y * 13) % 97) == 0)
          c = 3;
        sa_fb[i] = c;
      }
      i++;
    }
  for (k = 0; k < 5; k++) {
    SaRectFb(70, 50 + k * 18 + 3, 180, 1, 1);
    SaRectFb(70, 50 + k * 18 + 11, 180, 1, 3);
  }
  SaRectFb(290, 96, 6, 18, 4);
  SaRectFb(291, 97, 4, 16, 5);
}

U0 SaVignette(I64 p, I64 c)
{
  I64 x, y, t, e;
  t = p * 3;
  for (y = 0; y < SCR_H; y++)
    for (x = 0; x < SCR_W; x++) {
      e = x;
      if (SCR_W - 1 - x < e)
        e = SCR_W - 1 - x;
      if (y < e)
        e = y;
      if (SCR_H - 1 - y < e)
        e = SCR_H - 1 - y;
      if (e < t)
        if (sa_bayer[((y & 3) << 2) + (x & 3)] * t >= e * 16)
          sa_fb[y * SCR_W + x] = c;
    }
}

/* the painted floor sign from the staircase, big */
U0 SaDrawFloorSign(I64 x, I64 y, I64 f)
{
  I64 i, j, c, w, h, cx, cy;
  w = 84;
  h = 46;
  for (j = 0; j < h; j++)
    for (i = 0; i < w; i++) {
      /* rounded corners with radius 5 */
      cx = 0;
      cy = 0;
      if (i < 5)
        cx = 5 - i;
      if (i > w - 6)
        cx = i - (w - 6);
      if (j < 5)
        cy = 5 - j;
      if (j > h - 6)
        cy = j - (h - 6);
      if (cx * cx + cy * cy <= 25) {
        c = sa_sign_c1[f];
        if (sa_sign_dith[f] == 1 && ((i + j) & 1) == 0)
          c = sa_sign_c2[f];
        if (sa_sign_dith[f] == 3 && ((i + j * 2) & 3) == 0)
          c = sa_sign_c2[f];
        if (cx * cx + cy * cy > 16 || i == 0 || j == 0 || i == w - 1 || j == h - 1)
          c = 1;
        SaPutFb(x + i, y + j, c);
      }
    }
  SaDrawIcon(x + 4, y + 5, ART_ICON1 + f * 18, 2);
  sa_num_buf[0] = 0;
  SaStrNum(sa_num_buf, f + 1);
  SaDrawText(sa_fb, SCR_W, SCR_H, x + 58, y + 9, sa_num_buf, 1, 4);
}

/* the second goal line: where to go next */
CH *SaWhereText()
{
  I64 r, f;
  f = sa_task_floor[sa_task];
  if (sa_floor != f) {
    SaStrCpy(sa_hint_buf, "> ZUR ");
    SaStrNum(sa_hint_buf, f + 1);
    SaStrCat(sa_hint_buf, ". ETAGE");
    return sa_hint_buf;
  }
  if (sa_hint >= 0 && !sa_it_got[sa_hint]) {
    r = SaRoomIndex(sa_it_f[sa_hint], sa_it_x[sa_hint], sa_it_y[sa_hint]);
    if (r >= 0) {
      SaStrCpy(sa_hint_buf, "> ");
      SaStrCat(sa_hint_buf, sa_rm_name[r]);
      return sa_hint_buf;
    }
  }
  if (sa_task_where[sa_task][0] != 0)
    return sa_task_where[sa_task];
  if (sa_le_f == sa_floor)
    return "> FRAG HERRN LERSCH (AUF DIESER ETAGE)";
  return "";
}

/* one inventory slot: icon and count */
U0 SaHudItem(I64 x, I64 t, I64 showcount)
{
  SaDrawArt(sa_fb, SCR_W, SCR_H, x, 190, sa_it_art[t], 8, 8, FALSE, 1, 1);
  if (showcount) {
    sa_num_buf[0] = 0;
    SaStrNum(sa_num_buf, sa_have[t]);
    SaTextSh(x + 10, 191, sa_num_buf, 6, 1, 1);
  }
}

U0 SaHud()
{
  I64 w, c, y, x, t;
  t = sa_task_item[sa_task];
  x = 4;
  if (t >= 0) {
    SaDrawArt(sa_fb, SCR_W, SCR_H, 4, 3, sa_it_art[t], 8, 8, FALSE, 1, 1);
    x = 14;
  }
  SaTextSh(x, 4, SaGoalText(), 15, 1, 1);
  SaTextSh(x, 13, SaWhereText(), 5, 1, 1);
  sa_num_buf[0] = 0;
  SaStrNum(sa_num_buf, sa_floor + 1);
  SaStrCat(sa_num_buf, ". ETAGE");
  SaTextSh(SCR_W - 4 - SaTextW(sa_num_buf, 1), 4, sa_num_buf, sa_sign_c1[sa_floor], 1, 1);
  SaTextSh(SCR_W - 4 - SaTextW("8,00 M", 1), 13, "8,00 M", 3, 1, 1);
  if (sa_room_t > 0 && sa_room >= 0 && sa_sign_t == 0)
    SaTextCenter(26, sa_rm_name[sa_room], 6, 1);
  /* inventory */
  x = 98;
  if (sa_have[IT_LAMP] > 0) {
    SaHudItem(x, IT_LAMP, FALSE);
    x = x + 11;
  }
  if (sa_have[IT_SHOES] > 0) {
    SaHudItem(x, IT_SHOES, FALSE);
    x = x + 11;
  }
  if (sa_have[IT_DRINK] > 0) {
    SaHudItem(x, IT_DRINK, TRUE);
    x = x + 19;
  }
  if (sa_have[IT_BOMB] > 0) {
    SaHudItem(x, IT_BOMB, TRUE);
    x = x + 19;
  }
  SaDrawArt(sa_fb, SCR_W, SCR_H, x, 190, ART_STICKER, 8, 8, FALSE, 1, 1);
  sa_num_buf[0] = 0;
  SaStrNum(sa_num_buf, sa_have[IT_STICKER]);
  SaStrCat(sa_num_buf, "/");
  SaStrNum(sa_num_buf, NSTICKERS);
  SaTextSh(x + 10, 191, sa_num_buf, 15, 1, 1);
  if (sa_boost > 0 && ((sa_tick / 4) % 2) == 0)
    SaTextSh(60, 182, "BOOST!", 14, 1, 1);
  /* stamina */
  SaRectFb(4, 192, 52, 5, 1);
  c = 14;
  if (sa_tired)
    c = 11;
  SaRectFb(5, 193, sa_stamina * 50 / 1000, 3, c);
  SaTextSh(60, 191, "PUSTE", 4, 1, 1);
  /* phone */
  if (sa_phone) {
    SaDrawArt(sa_fb, SCR_W, SCR_H, 250, 189, ART_PHONE, 8, 8, FALSE, 1, 1);
    if (sa_ring_t > 0) {
      if (((sa_tick / 4) % 2) == 0)
        SaTextSh(261, 191, "KLINGELT!", 12, 1, 1);
    } else if (sa_cj_st != CJ_OFF)
      SaTextSh(261, 191, "CONJOKE", 14, 1, 1);
    else if (sa_call_cd > 0) {
      sa_num_buf[0] = 0;
      SaStrNum(sa_num_buf, sa_call_cd / TICKS_PER_SEC + 1);
      SaStrCat(sa_num_buf, " S");
      SaTextSh(261, 191, sa_num_buf, 4, 1, 1);
    } else
      SaTextSh(261, 191, "BEREIT", 14, 1, 1);
  } else
    SaTextSh(250, 191, "KEIN HANDY", 3, 1, 1);
  if (sa_hidden)
    SaTextCenter(152, SaHint("VERSTECKT  -  E = RAUS", "VERSTECKT  -  AKTION = RAUS",
                             "VERSTECKT  -  A = RAUS"), 5, 1);
  if (sa_write_t > 0) {
    sa_num_buf[0] = 0;
    SaStrCat(sa_num_buf, "NAME AUFSCHREIBEN... ");
    SaStrNum(sa_num_buf, sa_write_t / TICKS_PER_SEC + 1);
    SaTextCenter(140, sa_num_buf, 15, 1);
  } else if (sa_le_near && sa_msg_t == 0)
    SaTextCenter(156, SaHint("E = MIT HERRN LERSCH REDEN", "AKTION = MIT HERRN LERSCH REDEN",
                             "A = MIT HERRN LERSCH REDEN"), 5, 1);
  else if (sa_near_obj == 'M' && sa_task == TK_PIANO && sa_msg_t == 0)
    SaTextCenter(156, SaHint("E = DAS LIED SPIELEN", "AKTION = DAS LIED SPIELEN",
                             "A = DAS LIED SPIELEN"), 15, 1);
  else if (sa_near_obj == 'u' && sa_task == TK_FUSEBOX && sa_msg_t == 0)
    SaTextCenter(156, SaHint("E = SICHERUNGEN EINSETZEN", "AKTION = SICHERUNGEN EINSETZEN",
                             "A = SICHERUNGEN EINSETZEN"), 15, 1);
  else if (sa_vh_near >= 0 && sa_msg_t == 0) {
    if (sa_vh_charge[sa_vh_near] <= 0)
      SaTextCenter(156, "KAPUTT.", 4, 1);
    else if (sa_vh_t[sa_vh_near] == VH_SCOOTER)
      SaTextCenter(156, SaHint("E = AUF DEN E-SCOOTER", "AKTION = AUF DEN E-SCOOTER",
                               "A = AUF DEN E-SCOOTER"), 15, 1);
    else
      SaTextCenter(156, SaHint("E = AUFS FAHRRAD", "AKTION = AUFS FAHRRAD", "A = AUFS FAHRRAD"),
                   15, 1);
  }
  /* riding: how much battery / chain is left */
  if (sa_ride >= 0) {
    c = 14;
    if (sa_vh_charge[sa_ride] < 40)
      c = 12;
    if (sa_vh_t[sa_ride] == VH_SCOOTER)
      SaTextSh(110, 40, "AKKU", 5, 1, 1);
    else
      SaTextSh(110, 40, "KETTE", 5, 1, 1);
    SaRectFb(146, 40, 64, 7, 1);
    SaRectFb(147, 41, sa_vh_charge[sa_ride] * 62 / 140, 5, c);
  }
  /* Frau Niehaus wants to sing */
  if (sa_ni_meter > 0 && sa_sing_t == 0) {
    SaTextCenter(40, "FRAU NIEHAUS WILL MIT DIR SINGEN!", 15, 1);
    SaRectFb(110, 50, 100, 5, 1);
    SaRectFb(111, 51, sa_ni_meter * 98 / sa_md_sing[sa_mode], 3, 15);
  }
  if (sa_alarm_t > 0 && ((sa_tick / 6) & 1) == 0)
    SaTextCenter(60, "! HERR THÜNER IST ALARMIERT !", 12, 1);
  if (sa_msg_t > 0) {
    y = 177;
    if (sa_msg2[0] != 0) {
      y = 166;
      w = SaTextW(sa_msg2, 1);
      if (SaTextW(sa_msg, 1) > w)
        w = SaTextW(sa_msg, 1);
      SaRectFb((SCR_W - w) / 2 - 3, y - 2, w + 6, 22, 1);
      SaTextCenter(y + 11, sa_msg2, 6, 1);
    } else {
      w = SaTextW(sa_msg, 1);
      SaRectFb((SCR_W - w) / 2 - 3, y, w + 6, 11, 1);
      y = y + 2;
    }
    SaTextCenter(y, sa_msg, 6, 1);
    /* the face of who is talking, above the box */
    if (sa_msg_face >= 0) {
      SaRectFb(4, y - 54, 52, 52, 1);
      SaRectFb(5, y - 53, 50, 50, 6);
      SaDrawArt(sa_fb, SCR_W, SCR_H, 6, y - 52, ART_POR_LE + sa_msg_face * 24, 24, 24, FALSE, 2, 1);
    }
  }
}

U0 SaDrawPlay()
{
  I64 camx, camy, y, px, py, n, i, j, t, amb, lr, x, e;
  I64 sy[9];
  I64 st[9];
  px = sa_px >> 8;
  py = sa_py >> 8;
  camx = sa_camx;
  camy = sa_camy;
  if (sa_shake > 0) {
    camx = camx + SaRand(5) - 2;
    camy = camy + SaRand(5) - 2;
  }
  if (camx < 0)
    camx = 0;
  if (camy < -26)
    camy = -26;
  if (camx > sa_floor_w[sa_floor] * 16 - SCR_W)
    camx = sa_floor_w[sa_floor] * 16 - SCR_W;
  if (camy > WORLD_H - SCR_H + 30)
    camy = WORLD_H - SCR_H + 30;
  for (y = 0; y < SCR_H; y++) {
    if (camy + y < 0 || camy + y >= WORLD_H)
      SaMemSet(sa_fb + y * SCR_W, 0, SCR_W);
    else
      SaMemCpy(sa_fb + y * SCR_W, sa_world + (camy + y) * WORLD_W + camx, SCR_W);
  }
  SaDrawSwimmer(camx, camy);
  SaDrawWet(camx, camy);
  for (i = 0; i < 2; i++)
    if (sa_csx[i] >= 0 && sa_cvalid[i] && sa_cf[i] == sa_floor)
      if (sa_wet[sa_csy[i] * MAP_W + sa_csx[i]] > 0 || sa_cst[i] == CL_MOP)
        SaDrawWetSign(sa_csx[i] * 16 + 8 - camx, sa_csy[i] * 16 + 13 - camy);
  if (sa_floor == F_OUTSIDE)
    for (i = 0; i < sa_vh_n; i++)
      if (i != sa_ride)
        SaDrawVehicle((sa_vh_x[i] >> 8) - camx, (sa_vh_y[i] >> 8) - camy + 1, sa_vh_t[i],
                      sa_vh_dir[i], sa_vh_charge[i] <= 0);

  /* everybody on this floor, sorted by their feet */
  n = 0;
  sy[n] = py;
  st[n] = 9;
  n++;
  for (i = 0; i < 2; i++)
    if (SaTeacherHere(i)) {
      sy[n] = sa_ey[i] >> 8;
      st[n] = i;
      n++;
    }
  if (sa_cj_st != CJ_OFF && sa_cj_st != CJ_WAIT) {
    sy[n] = sa_ey[2] >> 8;
    st[n] = 2;
    n++;
  }
  if (sa_floor == sa_le_f) {
    sy[n] = sa_le_ty * 16 + 10;
    st[n] = 7;
    n++;
  }
  for (i = 0; i < 2; i++)
    if (sa_cvalid[i] && sa_cf[i] == sa_floor) {
      sy[n] = sa_ey[E_CL + i] >> 8;
      st[n] = E_CL + i;
      n++;
    }
  if (sa_ni_valid && sa_floor == F_NIEHAUS) {
    sy[n] = sa_ey[E_NI] >> 8;
    st[n] = E_NI;
    n++;
  }
  for (i = 1; i < n; i++)
    for (j = i; j > 0; j--)
      if (sy[j] < sy[j - 1]) {
        t = sy[j];
        sy[j] = sy[j - 1];
        sy[j - 1] = t;
        t = st[j];
        st[j] = st[j - 1];
        st[j - 1] = t;
      }
  for (i = 0; i < n; i++) {
    if (st[i] == 9)
      SaDrawPlayerAt(px - camx, py - camy);
    else if (st[i] == 7)
      SaDrawLersch(camx, camy);
    else
      SaDrawEnt(st[i], camx, camy);
  }

  /* outside there is moonlight; the gym floor stays dark until the
   * fuses are back in */
  amb = 0;
  lr = sa_lamp_r[sa_floor];
  if (sa_floor == F_OUTSIDE)
    amb = 1;
  if (sa_floor == 1) {
    lr = 0;
    if (sa_power) {
      amb = 1;
      lr = sa_lamp_r[1];
    }
  }
  SaLight(camx, camy, px, py - 8, sa_light_r, sa_lamp_x[sa_floor], sa_lamp_y[sa_floor], lr, amb);
  if (SaTeacherHere(0))
    SaGlint((sa_ex[0] >> 8) - camx, (sa_ey[0] >> 8) - camy);
  SaDrawItems(camx, camy, amb);
  SaWetGlint(camx, camy);
  if (sa_bomb_t > 0 && SaTeacherHere(0))
    SaDrawStink((sa_ex[0] >> 8) - camx, (sa_ey[0] >> 8) - camy);
  if (sa_cj_st == CJ_FIGHT && sa_cj_tgt >= 0)
    if (SaTeacherHere(sa_cj_tgt))
      SaDrawFight((sa_ex[sa_cj_tgt] >> 8) - camx, (sa_ey[sa_cj_tgt] >> 8) - camy);
  if (SaTeacherHere(1) && sa_tst[1] == KB_RUSH && sa_tstun[1] == 0)
    SaKbGlint((sa_ex[1] >> 8) - camx, (sa_ey[1] >> 8) - camy);
  if (sa_spot_t > 0 && SaTeacherHere(0))
    SaTextSh((sa_ex[0] >> 8) - camx - 4, (sa_ey[0] >> 8) - camy - 46, "!", 12, 1, 2);
  /* the cleaners: "?" while they wonder who you are, "!" just before
   * they call Herr Thüner */
  for (i = 0; i < 2; i++)
    if (sa_cvalid[i] && sa_cf[i] == sa_floor && sa_csee[i] > 0) {
      e = E_CL + i;
      if (sa_csee[i] > sa_md_clsee[sa_mode] * 2 / 3)
        SaTextSh((sa_ex[e] >> 8) - camx - 2, (sa_ey[e] >> 8) - camy - 40, "!", 12, 1, 1);
      else
        SaTextSh((sa_ex[e] >> 8) - camx - 2, (sa_ey[e] >> 8) - camy - 40, "?", 15, 1, 1);
    }
  /* Frau Niehaus hums - her notes show even in the dark */
  if (sa_ni_valid && sa_floor == F_NIEHAUS) {
    x = (sa_ex[E_NI] >> 8) - camx;
    y = (sa_ey[E_NI] >> 8) - camy;
    if (SaDistPx(E_NI) < 200 && ((sa_tick / 20) % 3) != 0)
      SaDrawNote(x + 5 + (sa_tick / 6) % 4, y - 36 - (sa_tick / 4) % 6, 15);
    if (sa_ni_meter > 0)
      SaTextSh(x - 2, y - 42, "?", 15, 1, 1);
  }
  if (sa_sing_t > 0) {
    for (i = 0; i < 4; i++)
      SaDrawNote(px - camx - 16 + i * 9, py - camy - 56 - ((sa_tick / 4 + i * 3) % 8), 6 + (i & 1) * 9);
    if (((sa_tick / 8) & 1) == 0)
      SaTextSh(px - camx - 20, py - camy - 68, "LA LA LAAA!", 15, 1, 1);
  }
  if (sa_hidden)
    SaLockerView();
  if (sa_pulse > 0)
    SaVignette(sa_pulse, 11);
  if (sa_ring_t > 0 && ((sa_tick / 4) % 2) == 0)
    SaVignette(2, 3);
  if (sa_sign_t > 0) {
    SaDrawFloorSign(118, 26, sa_floor);
    sa_num_buf[0] = 0;
    SaStrNum(sa_num_buf, sa_floor + 1);
    SaStrCat(sa_num_buf, ". ETAGE");
    SaTextCenter(76, sa_num_buf, 6, 2);
    SaTextCenter(94, sa_floor_name[sa_floor], 5, 1);
  }
  SaHud();
}

/* ------------------------------------------------------------------ */
/* saving and loading                                                  */
/* ------------------------------------------------------------------ */

U0 SaPut32(I64 v)
{
  if (sa_save_pos + 4 > SA_SAVE_N)
    return;
  sa_save[sa_save_pos] = (v & 255);
  sa_save[sa_save_pos + 1] = ((v >> 8) & 255);
  sa_save[sa_save_pos + 2] = ((v >> 16) & 255);
  sa_save[sa_save_pos + 3] = ((v >> 24) & 255);
  sa_save_pos = sa_save_pos + 4;
}

I64 SaGet32()
{
  I64 v;
  if (sa_save_pos + 4 > SA_SAVE_N)
    return 0;
  v = sa_save[sa_save_pos] | (sa_save[sa_save_pos + 1] << 8) |
      (sa_save[sa_save_pos + 2] << 16) | (sa_save[sa_save_pos + 3] << 24);
  sa_save_pos = sa_save_pos + 4;
  return v;
}

/* the 0/1 values of the map, 8 per byte */
U0 SaPutSeen()
{
  I64 i, b, k;
  for (i = 0; i < 12096; i = i + 8) {
    b = 0;
    for (k = 0; k < 8; k++)
      if (sa_seen[i + k] != 0)
        b = (b | (1 << k));
    if (sa_save_pos < SA_SAVE_N)
      sa_save[sa_save_pos] = b;
    sa_save_pos++;
  }
}

U0 SaGetSeen()
{
  I64 i, b, k;
  for (i = 0; i < 12096; i = i + 8) {
    b = 0;
    if (sa_save_pos < SA_SAVE_N)
      b = sa_save[sa_save_pos];
    sa_save_pos++;
    for (k = 0; k < 8; k++)
      sa_seen[i + k] = ((b >> k) & 1);
  }
}

/* writes the whole run into sa_save: progress, where everything lies,
 * where you stand, what you have seen of the map */
U0 SaSaveGame()
{
  I64 i;
  sa_save_pos = 0;
  SaPut32(5395265);
  SaPut32(sa_mode);
  SaPut32(sa_task);
  SaPut32(sa_power);
  SaPut32(sa_pool_open);
  for (i = 0; i < NFLOORS; i++)
    SaPut32(sa_unlock[i]);
  for (i = 0; i < 3; i++)
    SaPut32(sa_code[i]);
  SaPut32(sa_deaths);
  SaPut32(sa_play_ticks);
  SaPut32(sa_floor);
  SaPut32(sa_px);
  SaPut32(sa_py);
  SaPut32(sa_phone);
  SaPut32(sa_cj_met);
  SaPut32(sa_it_n);
  for (i = 0; i < NITEMS; i++) {
    SaPut32(sa_it_f[i]);
    SaPut32(sa_it_x[i]);
    SaPut32(sa_it_y[i]);
    SaPut32(sa_it_t[i]);
    SaPut32(sa_it_got[i]);
  }
  for (i = 0; i < NITYPES; i++)
    SaPut32(sa_have[i]);
  for (i = 0; i < NFLOORS; i++) {
    SaPut32(sa_le_sx[i]);
    SaPut32(sa_le_sy[i]);
  }
  SaPut32(sa_vh_n);
  for (i = 0; i < NVEH; i++) {
    SaPut32(sa_vh_x[i]);
    SaPut32(sa_vh_y[i]);
    SaPut32(sa_vh_t[i]);
    SaPut32(sa_vh_charge[i]);
    SaPut32(sa_vh_dir[i]);
  }
  SaPut32(sa_cf[0]);
  SaPut32(sa_cf[1]);
  SaPutSeen();
  sa_save_ok = TRUE;
  sa_save_seq++;
}

/* starts the saved run again where it was saved; the teachers are out of
 * sight for a moment, like after a checkpoint */
I64 SaLoadGame()
{
  I64 i, f, x, y;
  if (!sa_save_ok)
    return FALSE;
  sa_save_pos = 0;
  if (SaGet32() != 5395265)
    return FALSE;
  sa_mode = SaGet32();
  if (sa_mode < 0 || sa_mode > 2)
    sa_mode = MODE_MEDIUM;
  SaResetGame();
  sa_task = SaGet32();
  sa_power = SaGet32();
  sa_pool_open = SaGet32();
  for (i = 0; i < NFLOORS; i++)
    sa_unlock[i] = SaGet32();
  for (i = 0; i < 3; i++)
    sa_code[i] = SaGet32();
  sa_deaths = SaGet32();
  sa_play_ticks = SaGet32();
  f = SaGet32();
  x = SaGet32();
  y = SaGet32();
  sa_phone = SaGet32();
  sa_cj_met = SaGet32();
  sa_it_n = SaGet32();
  for (i = 0; i < NITEMS; i++) {
    sa_it_f[i] = SaGet32();
    sa_it_x[i] = SaGet32();
    sa_it_y[i] = SaGet32();
    sa_it_t[i] = SaGet32();
    sa_it_got[i] = SaGet32();
  }
  for (i = 0; i < NITYPES; i++)
    sa_have[i] = SaGet32();
  for (i = 0; i < NFLOORS; i++) {
    sa_le_sx[i] = SaGet32();
    sa_le_sy[i] = SaGet32();
  }
  sa_vh_n = SaGet32();
  for (i = 0; i < NVEH; i++) {
    sa_vh_x[i] = SaGet32();
    sa_vh_y[i] = SaGet32();
    sa_vh_t[i] = SaGet32();
    sa_vh_charge[i] = SaGet32();
    sa_vh_dir[i] = SaGet32();
  }
  sa_cf[0] = SaGet32();
  sa_cf[1] = SaGet32();
  SaGetSeen();
  if (f < 0 || f >= NFLOORS)
    f = sa_task_floor[sa_task];
  SaLoadFloor(f);
  sa_px = x;
  sa_py = y;
  if (!SaWalkCh(SaTile(sa_px >> 12, sa_py >> 12))) {
    sa_px = (3 * 16 + 8) << 8;
    sa_py = (19 * 16 + 10) << 8;
  }
  SaLerschGo();
  SaSaveCp();
  /* Herr Thüner and Herr Kirschbaum are somewhere else for now */
  sa_tvalid[0] = FALSE;
  sa_tf[0] = f + 1;
  if (sa_tf[0] > F_TOP)
    sa_tf[0] = f - 1;
  sa_tst[0] = TH_WANDER;
  sa_troam[0] = 600;
  sa_tfollow[0] = 0;
  sa_tvalid[1] = FALSE;
  sa_tf[1] = f - 1;
  if (sa_tf[1] < 0)
    sa_tf[1] = f + 1;
  sa_tst[1] = KB_PATROL;
  sa_tfollow[1] = 0;
  for (i = 0; i < 2; i++) {
    sa_cvalid[i] = FALSE;
    if (sa_cf[i] == f)
      SaPlaceCleaner(i, FALSE);
  }
  sa_stair_cd = 20;
  sa_stair_arm = TRUE;
  sa_sign_t = 80;
  sa_light_r = 92;
  SaMsg2("SPIELSTAND GELADEN.", SaGoalText(), 100);
  SaFov();
  SaCamSnap();
  sa_state = ST_PLAY;
  sa_timer = 0;
  return TRUE;
}

/* ------------------------------------------------------------------ */
/* other screens                                                       */
/* ------------------------------------------------------------------ */

/* the pause menu with the map of this floor (only what you have seen) */
U0 SaDrawMap(I64 ox, I64 oy, I64 cs)
{
  I64 tx, ty, ch, c, w, k;
  w = sa_floor_w[sa_floor];
  SaRectFb(ox - 2, oy - 2, w * cs + 4, MAP_H * cs + 4, 3);
  SaRectFb(ox - 1, oy - 1, w * cs + 2, MAP_H * cs + 2, 0);
  for (ty = 0; ty < MAP_H; ty++)
    for (tx = 0; tx < w; tx++)
      if (sa_seen[sa_floor * MAP_N + ty * MAP_W + tx] != 0) {
        ch = SaTile(tx, ty);
        c = 4;
        if (SaIsWallCh(ch))
          c = 2;
        else if (ch == '-')
          c = 12;
        else if (ch == 'r')
          c = 10;
        else if (ch == 'A')
          c = 1;
        else if (ch == '~')
          c = 13;
        else if (ch == ';')
          c = 14;
        else if (ch == '_')
          c = 3;
        else if (ch == '^' || ch == 'v')
          c = 15;
        else if (ch == 'D')
          c = 6;
        else if (ch == 'E' || ch == 'e' || ch == 'X' || ch == 'g')
          c = 11;
        else if (SaIsFloorCh(ch))
          c = 5;
        if (ch == 'q' || ch == 'F')
          c = 13;
        SaRectFb(ox + tx * cs, oy + ty * cs, cs, cs, c);
      }
  /* Herr Lersch, what you are looking for, and you */
  if (sa_le_f == sa_floor)
    SaRectFb(ox + sa_le_tx * cs - 1, oy + sa_le_ty * cs - 1, cs + 2, cs + 2, 13);
  k = sa_hint;
  if (k >= 0 && !sa_it_got[k] && sa_it_f[k] == sa_floor && ((sa_tick / 8) & 1) == 0)
    SaRectFb(ox + sa_it_x[k] * cs - 1, oy + sa_it_y[k] * cs - 1, cs + 2, cs + 2, 15);
  if (((sa_tick / 6) & 1) == 0)
    SaRectFb(ox + (sa_px >> 12) * cs - 1, oy + (sa_py >> 12) * cs - 1, cs + 2, cs + 2, 12);
  else
    SaRectFb(ox + (sa_px >> 12) * cs, oy + (sa_py >> 12) * cs, cs, cs, 6);
}

CH *sa_pause_items[4] = { "WEITERSPIELEN", "SPIEL SPEICHERN", "SPIELSTAND LADEN", "ZUM HAUPTMENÜ" };

U0 SaDrawPause()
{
  I64 x, y, w, i, c;
  SaDrawPlay();
  /* darken the frozen game */
  for (y = 0; y < SCR_H; y++)
    for (x = 0; x < SCR_W; x++)
      if (((x + y) & 1) == 0)
        sa_fb[y * SCR_W + x] = 0;
  SaRectFb(0, 0, SCR_W, 12, 1);
  sa_num_buf[0] = 0;
  SaStrCat(sa_num_buf, "PAUSE - KARTE ");
  SaStrNum(sa_num_buf, sa_floor + 1);
  SaStrCat(sa_num_buf, ". ETAGE");
  SaTextSh(4, 3, sa_num_buf, 15, 1, 1);
  SaTextSh(SCR_W - 4 - SaTextW(sa_mode_name[sa_mode], 1), 3, sa_mode_name[sa_mode], 4, 1, 1);
  w = sa_floor_w[sa_floor] * 4;
  SaDrawMap((SCR_W - w) / 2, 18, 4);
  SaTextCenter(134, sa_floor_name[sa_floor], 5, 1);
  /* legend */
  SaRectFb(40, 145, 4, 4, 12);
  SaTextSh(47, 144, "DU", 5, 1, 1);
  SaRectFb(72, 145, 4, 4, 13);
  SaTextSh(79, 144, "HERR LERSCH", 5, 1, 1);
  SaRectFb(152, 145, 4, 4, 15);
  SaTextSh(159, 144, "ZIEL / TREPPE", 5, 1, 1);
  SaTextSh(246, 144, "UNERFORSCHT", 3, 1, 1);
  /* the menu */
  SaRectFb(70, 154, 180, 44, 1);
  for (i = 0; i < 4; i++) {
    c = 4;
    if (i == sa_pause_sel)
      c = 15;
    if ((i == 1 && sa_mode == MODE_HARD) || (i == 2 && !sa_save_ok))
      c = 3;
    if (i == sa_pause_sel) {
      SaTextSh(78, 156 + i * 10, ">", 15, 1, 1);
      SaTextSh(236, 156 + i * 10, "<", 15, 1, 1);
    }
    SaTextCenter(156 + i * 10, sa_pause_items[i], c, 1);
  }
  if (sa_pause_msg_t > 0)
    SaTextCenter(124, sa_pause_msg, 14, 1);
}

U0 SaDrawSky(I64 flash)
{
  I64 i, x, y, c;
  c = 1;
  if (flash)
    c = 3;
  SaRectFb(0, 0, SCR_W, SCR_H, c);
  for (i = 0; i < 60; i++) {
    x = (SaHash(i, 7) + SaHash(i, 9) * 2) % SCR_W;
    y = SaHash(i, 3) * 64 / 256;
    c = 4;
    if ((i & 3) == 0)
      c = 5;
    if ((i % 7) == 0 && ((sa_tick / 6 + i) % 9) == 0)
      c = 6;
    SaPutFb(x, y, c);
  }
  SaDisc(272, 28, 11, 6);
  SaDisc(268, 25, 2, 5);
  SaDisc(276, 32, 2, 5);
}

U0 SaWindow(I64 x, I64 y, I64 lit)
{
  I64 g;
  SaRectFb(x - 1, y - 1, 20, 20, 5);
  g = 1;
  if (lit == 1)
    g = 15;
  if (lit == 2)
    g = 8;
  SaRectFb(x, y, 18, 18, g);
  if (lit != 0) {
    SaDisc(x + 6, y + 12, 4, 1);
    SaRectFb(x + 1, y + 15, 11, 3, 1);
    SaPutFb(x + 4, y + 9, 6);
    SaPutFb(x + 5, y + 9, 6);
  }
  SaRectFb(x + 8, y, 2, 18, 4);
  SaRectFb(x, y + 6, 18, 1, 4);
}

U0 SaDrawSchool(I64 flash, I64 lit)
{
  I64 x, y, c, i, half;
  SaRectFb(0, 168, SCR_W, 32, 2);
  SaRectFb(0, 168, SCR_W, 2, 14);
  for (y = 170; y < SCR_H; y++) {
    half = 12 + (y - 170);
    SaRectFb(160 - half, y, half * 2, 1, 3);
  }
  SaDisc(18, 142, 16, 13);
  SaDisc(16, 139, 14, 14);
  SaDisc(30, 154, 13, 14);
  SaRectFb(20, 150, 4, 20, 7);
  SaDisc(300, 140, 17, 13);
  SaDisc(302, 137, 15, 14);
  SaDisc(290, 153, 12, 14);
  SaRectFb(298, 150, 4, 20, 7);
  for (y = 70; y < 170; y++)
    for (x = 40; x < 280; x++) {
      c = 10;
      if (flash)
        c = 9;
      if ((y % 5) == 4)
        c = 7;
      else if (((x + ((y / 5) % 2) * 5) % 10) == 0)
        c = 7;
      SaPutFb(x, y, c);
    }
  SaRectFb(36, 64, 248, 7, 2);
  SaRectFb(36, 64, 248, 1, 4);
  for (i = 0; i < 9; i++) {
    SaWindow(48 + i * 26, 80, 0);
    if (i == 1)
      SaWindow(48 + i * 26, 120, lit);
    else if (i != 4)
      SaWindow(48 + i * 26, 120, 0);
  }
  SaRectFb(146, 136, 28, 34, 2);
  SaRectFb(148, 138, 11, 32, 7);
  SaRectFb(161, 138, 11, 32, 7);
  SaRectFb(150, 141, 7, 12, 1);
  SaRectFb(163, 141, 7, 12, 1);
  SaRectFb(156, 156, 2, 3, 15);
  SaRectFb(162, 156, 2, 3, 15);
  SaRectFb(140, 168, 40, 2, 4);
  SaRectFb(111, 103, 98, 14, 4);
  SaRectFb(112, 104, 96, 12, 1);
  SaDrawText(sa_fb, SCR_W, SCR_H, 116, 107, "ST. ANNA SCHULE", 6, 1);
}

U0 SaDrawTitle()
{
  I64 t, flash, lit, c;
  t = sa_timer % 170;
  flash = FALSE;
  if (t < 3 || (t > 6 && t < 9))
    flash = TRUE;
  lit = 1;
  if (SaHash(sa_timer / 3, 1) < 50)
    lit = 2;
  SaDrawSky(flash);
  SaDrawSchool(flash, lit);
  SaTextSh((SCR_W - SaTextW("ST. ANNA SCHULE", 2)) / 2, 6, "ST. ANNA SCHULE", 12, 11, 2);
  SaTextCenter(26, "HERR THÜNER IST NOCH IM HAUS", 6, 1);
  SaTextCenter(37, "6 ETAGEN - 10 AUFGABEN - 18 STICKER", 4, 1);
  /* the menu: new game with the difficulty, or the saved game */
  SaRectFb(30, 140, 260, 58, 1);
  SaRectFb(30, 140, 260, 1, 3);
  c = 5;
  if (sa_menu == 0)
    c = 15;
  SaStrCpy(sa_mb1, "NEUES SPIEL:  < ");
  SaStrCat(sa_mb1, sa_mode_name[sa_mode]);
  SaStrCat(sa_mb1, " >");
  SaTextCenter(145, sa_mb1, c, 1);
  c = 3;
  if (sa_save_ok)
    c = 5;
  if (sa_menu == 1)
    c = 15;
  if (sa_save_ok)
    SaTextCenter(157, "SPIELSTAND LADEN", c, 1);
  else
    SaTextCenter(157, "(NOCH KEIN SPIELSTAND)", c, 1);
  SaTextCenter(170, sa_mode_desc[sa_mode], 4, 1);
  if (((sa_timer / 15) % 2) == 0)
    SaTextCenter(181, SaHint("PFEILE WÄHLEN - ENTER STARTEN", "STEUERKREUZ WÄHLEN - TIPPEN STARTEN",
                             "STICK WÄHLEN - + STARTEN"), 15, 1);
  SaTextCenter(191, SaHint("WASD/SHIFT LAUFEN - E AKTION - Q HANDY - ESC PAUSE",
                           "LINKS LAUFEN - RECHTS AKTION/RENNEN/HANDY",
                           "STICK LAUFEN - B RENNEN - A AKTION - - PAUSE"), 4, 1);
}

U0 SaDrawIntro()
{
  I64 i, y, budget, n, c;
  SaRectFb(0, 0, SCR_W, SCR_H, 0);
  budget = sa_timer;
  y = 34;
  for (i = 0; i < 9; i++) {
    n = SaTextLen(sa_intro[i]);
    if (budget > 0) {
      c = 5;
      if (i == 0)
        c = 15;
      if (i == 5 || i == 8)
        c = 12;
      SaDrawTextN(sa_fb, SCR_W, SCR_H, (SCR_W - SaTextW(sa_intro[i], 1)) / 2, y,
                  sa_intro[i], c, 1, budget);
    }
    budget = budget - n - 8;
    y = y + 14;
  }
  SaTextCenter(186, SaHint("ENTER = WEITER", "TIPPEN = WEITER", "A = WEITER"), 3, 1);
}

U0 SaDrawCaught()
{
  I64 s, ox, oy;
  if (((sa_timer / 2) % 2) == 0)
    SaRectFb(0, 0, SCR_W, SCR_H, 11);
  else
    SaRectFb(0, 0, SCR_W, SCR_H, 0);
  s = 3 + sa_timer / 3;
  if (s > 6)
    s = 6;
  ox = (SCR_W - 32 * s) / 2 + SaRand(9) - 4;
  oy = (SCR_H - 32 * s) / 2 + SaRand(9) - 4;
  SaDrawArt(sa_fb, SCR_W, SCR_H, ox, oy, ART_FACE, 32, 32, FALSE, s, 12);
}

U0 SaDrawOver()
{
  SaRectFb(0, 0, SCR_W, SCR_H, 0);
  SaDrawArt(sa_fb, SCR_W, SCR_H, 112, 6, ART_FACE, 32, 32, FALSE, 3, 12);
  SaTextCenter(110, "NACHSITZEN!", 12, 3);
  SaTextCenter(140, "HERR THÜNER HAT DICH ERWISCHT.", 6, 1);
  SaTextCenter(152, sa_over_reason, 4, 1);
  SaTextCenter(164, SaFmtTime("ZEIT BISHER: ", sa_end_ticks), 5, 1);
  if (sa_mode == MODE_HARD) {
    if (((sa_timer / 15) % 2) == 0)
      SaTextCenter(178, SaHint("SCHWER: KEIN CHECKPOINT - ENTER = NEU", "SCHWER: KEIN CHECKPOINT - TIPPEN = NEU",
                               "SCHWER: KEIN CHECKPOINT - A = NEU"), 12, 1);
  } else if (((sa_timer / 15) % 2) == 0)
    SaTextCenter(178, SaHint("ENTER = WEITER AB CHECKPOINT", "TIPPEN = WEITER AB CHECKPOINT",
                             "A = WEITER AB CHECKPOINT"), 15, 1);
  SaTextCenter(189, SaHint("ESC = AUFGEBEN (MENÜ)", "ZURÜCK = MENÜ", "- = AUFGEBEN (MENÜ)"), 3, 1);
}

U0 SaDrawWin()
{
  I64 cr, i, y;
  SaDrawSky(FALSE);
  SaDrawSchool(FALSE, 0);
  SaDrawAdult(sa_fb, SCR_W, SCR_H, 160, 169, DIR_DOWN, 0, 12, 1, 0);
  SaDrawConjoke(sa_fb, SCR_W, SCR_H, 252, 194, DIR_DOWN, ((sa_timer / 8) % 2) * 2 + 1, 1);
  cr = 12 - sa_timer / 4;
  if (cr < 0)
    cr = 0;
  SaDrawGiant(sa_fb, SCR_W, SCR_H, 66, 194, DIR_DOWN, 0, cr, 2);
  if (cr == 0) {
    SaRectFb(96, 100, 1, 94, 15);
    for (i = 0; i <= 8; i++) {
      y = 194 - i * 94 / 8;
      SaRectFb(96, y, 4, 1, 15);
    }
    SaTextSh(102, 97, "8 M", 15, 1, 1);
  }
  SaTextSh((SCR_W - SaTextW("DU BIST ENTKOMMEN!", 2)) / 2, 6, "DU BIST ENTKOMMEN!", 15, 7, 2);
  SaTextCenter(26, SaFmtTime("ZEIT: ", sa_end_ticks), 6, 1);
  if (sa_timer > 50)
    SaTextCenter(38, "DU RICHTEST DICH AUF: 8,00 METER.", 5, 1);
  if (sa_timer > 30) {
    SaStrCpy(sa_le_buf, "STICKER: ");
    SaStrNum(sa_le_buf, sa_have[IT_STICKER]);
    SaStrCat(sa_le_buf, "/");
    SaStrNum(sa_le_buf, NSTICKERS);
    SaStrCat(sa_le_buf, "   ERWISCHT: ");
    SaStrNum(sa_le_buf, sa_deaths);
    SaStrCat(sa_le_buf, "X");
    SaTextCenter(50, sa_le_buf, 15, 1);
  }
  if (sa_timer > 60)
    SaTextSh(226, 146, "GG, BRO!", 14, 1, 1);
  if (sa_timer > 70 && ((sa_timer / 15) % 2) == 0)
    SaTextCenter(186, SaHint("ENTER = MENÜ", "TIPPEN = MENÜ", "A = MENÜ"), 15, 1);
}

/* ------------------------------------------------------------------ */
/* entry points                                                        */
/* ------------------------------------------------------------------ */

U0 SaGameInit(U32 seed)
{
  I64 i;
  sa_seed = seed;
  if (sa_seed == 0)
    sa_seed = 1;
  SaFontInit();
  for (i = 0; i < 16; i++) {
    sa_ltab[48 + i] = i;
    sa_ltab[32 + i] = sa_shade[i];
    sa_ltab[16 + i] = sa_shade[sa_shade[i]];
    sa_ltab[i] = 0;
  }
  for (i = 0; i < 4; i++) {
    sa_rvol[i] = 3;
    sa_renv[i] = 0;
  }
  SaScanFloors();
  SaResetGame();
  sa_state = ST_TITLE;
  sa_timer = 0;
}

I64 SaGameTick(I64 keys)
{
  I64 pressed;
  pressed = keys & ~sa_keys_prev;
  sa_keys_prev = keys;
  sa_tick++;
  sa_timer++;
  if (sa_state == ST_TITLE) {
    if ((pressed & K_ESC) != 0)
      return FALSE;
    if ((pressed & K_UP) != 0)
      sa_menu = 0;
    if ((pressed & K_DOWN) != 0 && sa_save_ok)
      sa_menu = 1;
    if (sa_menu == 0) {
      if ((pressed & K_LEFT) != 0 && sa_mode > 0)
        sa_mode--;
      if ((pressed & K_RIGHT) != 0 && sa_mode < 2)
        sa_mode++;
    }
    if ((pressed & (K_START | K_ACT)) != 0) {
      if (sa_menu == 1 && sa_save_ok)
        SaLoadGame();
      else {
        SaResetGame();
        sa_state = ST_INTRO;
        sa_timer = 0;
      }
    }
  } else if (sa_state == ST_INTRO) {
    if ((pressed & (K_START | K_ACT | K_ESC)) != 0 || sa_timer > 330) {
      sa_state = ST_PLAY;
      sa_timer = 0;
    }
  } else if (sa_state == ST_PLAY)
    SaPlayTick(keys, pressed);
  else if (sa_state == ST_CAUGHT) {
    if (sa_timer >= 50) {
      sa_state = ST_OVER;
      sa_timer = 0;
    }
  } else if (sa_state == ST_PAUSE) {
    if (sa_pause_msg_t > 0)
      sa_pause_msg_t--;
    if ((pressed & K_UP) != 0 && sa_pause_sel > 0)
      sa_pause_sel--;
    if ((pressed & K_DOWN) != 0 && sa_pause_sel < 3)
      sa_pause_sel++;
    if ((pressed & K_ESC) != 0)
      sa_state = ST_PLAY;
    else if ((pressed & (K_START | K_ACT)) != 0) {
      if (sa_pause_sel == 0)
        sa_state = ST_PLAY;
      else if (sa_pause_sel == 1) {
        if (sa_mode == MODE_HARD)
          sa_pause_msg = "IM SCHWEREN MODUS KANN MAN NICHT SPEICHERN.";
        else {
          SaSaveGame();
          sa_pause_msg = "GESPEICHERT!";
        }
        sa_pause_msg_t = 60;
      } else if (sa_pause_sel == 2) {
        if (!SaLoadGame()) {
          sa_pause_msg = "ES GIBT NOCH KEINEN SPIELSTAND.";
          sa_pause_msg_t = 60;
        }
      } else {
        sa_state = ST_TITLE;
        sa_timer = 0;
        sa_ring_t = 0;
        sa_sing_t = 0;
      }
    }
  } else if (sa_state == ST_OVER) {
    if ((pressed & (K_START | K_ACT)) != 0 && sa_timer > 20) {
      if (sa_mode == MODE_HARD) {
        /* no checkpoints: the whole school again */
        SaResetGame();
        sa_state = ST_INTRO;
      } else {
        SaContinue();
        sa_state = ST_PLAY;
      }
      sa_timer = 0;
    } else if ((pressed & K_ESC) != 0) {
      sa_state = ST_TITLE;
      sa_timer = 0;
    }
  } else if (sa_state == ST_WIN) {
    if ((pressed & (K_START | K_ACT | K_ESC)) != 0 && sa_timer > 60) {
      sa_state = ST_TITLE;
      sa_timer = 0;
    }
  }
  SaMusicTick();
  return TRUE;
}

U0 SaGameDraw()
{
  if (sa_state == ST_TITLE)
    SaDrawTitle();
  else if (sa_state == ST_INTRO)
    SaDrawIntro();
  else if (sa_state == ST_PLAY)
    SaDrawPlay();
  else if (sa_state == ST_CAUGHT)
    SaDrawCaught();
  else if (sa_state == ST_OVER)
    SaDrawOver();
  else if (sa_state == ST_PAUSE)
    SaDrawPause();
  else
    SaDrawWin();
}

/* ---- for the screenshot tool ---- */

U0 SaDebugWorld(I64 f)
{
  SaLoadFloor(f);
}

U0 SaGoFloor(I64 f)
{
  I64 i;
  /* every floor up to the task floor is open in these scenes */
  for (i = 0; i < NFLOORS; i++)
    sa_unlock[i] = FALSE;
  for (i = 0; i <= sa_task; i++)
    sa_unlock[sa_task_floor[i]] = TRUE;
  sa_unlock[f] = TRUE;
  SaLoadFloor(f);
  sa_tvalid[0] = FALSE;
  sa_tvalid[1] = FALSE;
  sa_tf[0] = f;
  sa_tf[1] = f + 2;
}

/* moves item slot k to a fixed spot for a screenshot */
U0 SaDebugItem(I64 k, I64 f, I64 x, I64 y, I64 t)
{
  sa_it_f[k] = f;
  sa_it_x[k] = x;
  sa_it_y[k] = y;
  sa_it_t[k] = t;
  sa_it_got[k] = FALSE;
}

U0 SaPlaceAt(I64 ptx, I64 pty, I64 htx, I64 hty)
{
  sa_px = (ptx * 16 + 8) << 8;
  sa_py = (pty * 16 + 10) << 8;
  sa_tvalid[0] = TRUE;
  sa_tf[0] = sa_floor;
  SaPlaceEnt(0, htx, hty);
  sa_ebfs[0] = -1;
  SaCamSnap();
}

U0 SaPlaceKb(I64 tx, I64 ty, I64 st)
{
  sa_tvalid[1] = TRUE;
  sa_tf[1] = sa_floor;
  SaPlaceEnt(1, tx, ty);
  sa_ebfs[1] = -1;
  sa_tst[1] = st;
}

U0 SaNoThuner()
{
  sa_tvalid[0] = FALSE;
  sa_tf[0] = (sa_floor + 3) % NFLOORS;
  sa_tfollow[0] = 0;
  sa_troam[0] = 99999;
}

/* Fixed scenes for the screenshot tool (stanna -shot N file.bmp). */
U0 SaDebugScene(I64 n)
{
  I64 i;
  SaResetGame();
  sa_state = ST_PLAY;
  sa_timer = 0;
  sa_msg_t = 0;
  sa_next_ring = 99999;
  if (n == 0) {
    sa_state = ST_TITLE;
    sa_timer = 40;
  } else if (n == 1) {
    SaMsg2("SCHNAPP DIR DIE TASCHENLAMPE!", "DANN FINDE DEN TREPPENSCHLÜSSEL.", 120);
    for (i = 0; i < 20; i++)
      SaGameTick(0);
  } else if (n == 2) {
    SaPlaceAt(24, 15, 18, 15);
    sa_tst[0] = TH_CHASE;
    sa_play_ticks = 900;
    sa_pdir = DIR_RIGHT;
    for (i = 0; i < 6; i++)
      SaGameTick(K_RIGHT);
    SaSpot();
    sa_spot_t = 20;
  } else if (n == 3) {
    sa_state = ST_CAUGHT;
    sa_timer = 12;
  } else if (n == 4) {
    sa_state = ST_OVER;
    sa_timer = 30;
    sa_over_reason = "UND ZWAR FÜR IMMER.";
    sa_end_ticks = 2 * 60 * 30 + 17 * 30;
  } else if (n == 5) {
    sa_state = ST_WIN;
    sa_timer = 120;
    sa_end_ticks = 3 * 60 * 30 + 5 * 30;
  } else if (n == 6) {
    SaPlaceAt(13, 15, 40, 15);
    sa_tst[0] = TH_WANDER;
    SaGameTick(0);
    SaGameTick(K_ACT);
    SaPlaceAt(13, 15, 8, 15);
    sa_tst[0] = TH_WANDER;
    sa_egoal[0] = 15 * MAP_W + 2;
    SaGameTick(0);
  } else if (n == 7) {
    SaGoFloor(5);
    SaPlaceAt(15, 3, 20, 14);
    sa_task = TK_EXIT;
    sa_tst[0] = TH_CHASE;
    sa_play_ticks = 3000;
    for (i = 0; i < 4; i++)
      SaGameTick(K_UP);
  } else if (n == 8) {
    sa_state = ST_INTRO;
    sa_timer = 290;
  } else if (n == 9) {
    SaGoFloor(2);
    SaNoThuner();
    sa_px = (36 * 16 + 8) << 8;
    sa_py = (5 * 16 + 10) << 8;
    SaPlaceKb(42, 5, KB_PATROL);
    sa_pdir = DIR_RIGHT;
    for (i = 0; i < 3; i++)
      SaGameTick(K_RIGHT);
  } else if (n == 10) {
    SaGoFloor(1);
    SaNoThuner();
    sa_px = (20 * 16 + 8) << 8;
    sa_py = (7 * 16 + 10) << 8;
    sa_pdir = DIR_DOWN;
    SaGameTick(K_DOWN);
  } else if (n == 11) {
    SaGoFloor(0);
    SaNoThuner();
    sa_px = (22 * 16 + 8) << 8;
    sa_py = (13 * 16 + 12) << 8;
    sa_pdir = DIR_UP;
    sa_tick = 130;
    SaGameTick(K_UP);
  } else if (n == 12) {
    SaGoFloor(4);
    SaNoThuner();
    sa_px = (20 * 16 + 8) << 8;
    sa_py = (21 * 16 + 10) << 8;
    sa_pdir = DIR_DOWN;
    SaGameTick(K_DOWN);
  } else if (n == 13) {
    sa_task = TK_SHEETS;
    SaLerschGo();
    SaGoFloor(sa_le_f);
    SaNoThuner();
    sa_px = ((sa_le_tx * 16 + 8) << 8) - 20 * 256;
    sa_py = (sa_le_ty * 16 + 10) << 8;
    sa_pdir = DIR_RIGHT;
    SaGameTick(0);
    SaGameTick(K_ACT);
  } else if (n == 14) {
    SaPlaceAt(20, 15, 26, 15);
    sa_tst[0] = TH_CHASE;
    sa_cj_st = CJ_FIGHT;
    sa_cj_tgt = 0;
    sa_cj_t = 60;
    sa_tstun[0] = 60;
    SaPlaceEnt(2, 25, 15);
    SaMsg2("RAUFEREI!", "CONJOKE HÄLT HERRN THÜNER AUF!", 90);
    SaGameTick(0);
  } else if (n == 15) {
    SaGoFloor(2);
    SaNoThuner();
    sa_px = (20 * 16 + 8) << 8;
    sa_py = (15 * 16 + 10) << 8;
    SaPlaceKb(26, 15, KB_PATROL);
    sa_next_ring = 1;
    for (i = 0; i < 7; i++)
      SaGameTick(0);
  } else if (n == 16) {
    SaGoFloor(2);
    SaNoThuner();
    sa_px = (20 * 16 + 8) << 8;
    sa_py = (15 * 16 + 10) << 8;
    SaPlaceKb(22, 15, KB_WRITE);
    sa_phone = FALSE;
    sa_write_t = 120;
    SaMsg2("HERR KIRSCHBAUM: HANDY HER!", "UND SCHREIB DEINEN NAMEN AUF.", 120);
    SaGameTick(0);
  } else if (n == 17) {
    SaGoFloor(0);
    SaNoThuner();
    SaChangeFloor(0, FALSE);
    sa_tick = 200;
    SaGameTick(0);
  } else if (n == 18) {
    /* items in the light: put a few of them around the player */
    SaGoFloor(1);
    SaNoThuner();
    sa_task = TK_FUSES;
    sa_have[IT_LAMP] = 1;
    SaDebugItem(0, 1, 46, 7, IT_FUSE);
    SaDebugItem(1, 1, 49, 8, IT_SHOES);
    SaDebugItem(2, 1, 50, 6, IT_STICKER);
    SaDebugItem(3, 1, 45, 9, IT_DRINK);
    sa_px = (47 * 16 + 8) << 8;
    sa_py = (9 * 16 + 10) << 8;
    sa_pdir = DIR_UP;
    SaGameTick(K_UP);
  } else if (n == 19) {
    /* the new staircase, the way down still locked */
    SaNoThuner();
    sa_have[IT_LAMP] = 1;
    sa_px = (3 * 16 + 8) << 8;
    sa_py = (18 * 16 + 12) << 8;
    sa_pdir = DIR_DOWN;
    SaGameTick(K_DOWN);
    SaGameTick(K_RIGHT);
  } else if (n == 20) {
    /* music floor: all sheets, next to a piano */
    sa_task = TK_PIANO;
    sa_have[IT_SHEET] = 4;
    sa_have[IT_LAMP] = 1;
    SaLerschGo();
    SaGoFloor(3);
    SaNoThuner();
    sa_px = (40 * 16 + 8) << 8;
    sa_py = (2 * 16 + 12) << 8;
    sa_pdir = DIR_UP;
    SaGameTick(K_UP);
  } else if (n == 21) {
    /* the fuse box, lights still off */
    sa_task = TK_FUSEBOX;
    sa_have[IT_FUSE] = 3;
    sa_have[IT_LAMP] = 1;
    sa_have[IT_SHOES] = 1;
    SaLerschGo();
    SaGoFloor(1);
    SaNoThuner();
    sa_px = (37 * 16 + 4) << 8;
    sa_py = (13 * 16 + 12) << 8;
    sa_pdir = DIR_UP;
    SaGameTick(K_UP);
  } else if (n == 22) {
    /* the swimming hall is open, the schoolyard key lies at the pool */
    sa_task = TK_YARDKEY;
    sa_pool_open = TRUE;
    sa_power = TRUE;
    sa_have[IT_LAMP] = 1;
    sa_have[IT_STICKER] = 9;
    sa_have[IT_BOMB] = 1;
    SaLerschGo();
    SaGoFloor(0);
    SaNoThuner();
    SaDebugItem(0, 0, 30, 10, IT_KYARD);
    sa_px = (27 * 16 + 8) << 8;
    sa_py = (11 * 16 + 10) << 8;
    sa_pdir = DIR_RIGHT;
    sa_tick = 300;
    SaGameTick(K_RIGHT);
  } else if (n == 23) {
    sa_state = ST_OVER;
    sa_timer = 30;
    sa_over_reason = "UND ZWAR FÜR IMMER.";
    sa_end_ticks = 9 * 60 * 30 + 41 * 30;
  } else if (n == 24) {
    sa_state = ST_WIN;
    sa_timer = 120;
    sa_have[IT_STICKER] = 14;
    sa_deaths = 2;
    sa_end_ticks = 17 * 60 * 30 + 12 * 30;
  } else if (n == 25) {
    /* schoolyard at night with a full inventory and two code notes */
    sa_task = TK_NOTES;
    sa_have[IT_NOTE] = 2;
    sa_have[IT_LAMP] = 1;
    sa_have[IT_SHOES] = 1;
    sa_have[IT_DRINK] = 2;
    sa_have[IT_BOMB] = 1;
    sa_have[IT_STICKER] = 11;
    SaLerschGo();
    SaGoFloor(4);
    SaNoThuner();
    SaDebugItem(0, 4, 22, 21, IT_NOTE);
    sa_px = (18 * 16 + 8) << 8;
    sa_py = (21 * 16 + 10) << 8;
    sa_pdir = DIR_RIGHT;
    SaGameTick(K_RIGHT);
  } else if (n == 26 || n == 27) {
    /* Frau Niehaus in the art room: 26 she wants to sing, 27 you sing */
    sa_have[IT_LAMP] = 1;
    SaGoFloor(2);
    SaNoThuner();
    sa_tf[1] = 5;
    sa_px = (31 * 16 + 8) << 8;
    sa_py = (22 * 16 + 10) << 8;
    sa_pdir = DIR_RIGHT;
    SaGameTick(0);
    SaPlaceEnt(E_NI, 34, 22);
    sa_egoal[E_NI] = 22 * MAP_W + 34;
    sa_ni_said = FALSE;
    for (i = 0; i < 25; i++)
      SaGameTick(0);
    if (n == 27)
      for (i = 0; i < 40; i++)
        SaGameTick(0);
  } else if (n == 28 || n == 29) {
    /* a cleaner mopping the corridor; 29: you ran and slipped */
    sa_have[IT_LAMP] = 1;
    SaGoFloor(3);
    SaNoThuner();
    SaPlaceEnt(E_CL, 18, 15);
    sa_egoal[E_CL] = 15 * MAP_W + 18;
    sa_cst[0] = CL_WALK;
    for (i = 13; i < 21; i++) {
      SaWetTile(i, 15, SA_WET);
      SaWetTile(i, 16, SA_WET - i * 40);
    }
    sa_px = (11 * 16 + 8) << 8;
    sa_py = (16 * 16 + 4) << 8;
    sa_pdir = DIR_RIGHT;
    for (i = 0; i < 40; i++)
      SaGameTick(0);
    if (n == 29) {
      sa_px = (16 * 16 + 8) << 8;
      sa_py = (15 * 16 + 10) << 8;
      for (i = 0; i < 5; i++)
        SaGameTick(K_RIGHT | K_RUN);
      sa_ccd[0] = 999;
      SaGameTick(0);
    } else {
      sa_ccd[0] = 0;
      for (i = 0; i < 30; i++)
        SaGameTick(0);
    }
  } else if (n == 31) {
    /* the pause menu with the map of the 3rd floor, half explored */
    sa_task = TK_SHEETS;
    sa_hint = -1;
    SaLerschGo();
    SaGoFloor(2);
    SaNoThuner();
    for (i = 0; i < 40; i++) {
      sa_px = ((2 + i) * 16 + 8) << 8;
      sa_py = (15 * 16 + 10) << 8;
      if (i > 26) {
        sa_px = (30 * 16 + 8) << 8;
        sa_py = ((10 - (i - 27) / 2) * 16 + 10) << 8;
      }
      SaFov();
    }
    for (i = 30; i < 60; i++) {
      sa_px = (i * 16 + 8) << 8;
      sa_py = (5 * 16 + 10) << 8;
      SaFov();
    }
    SaGameTick(0);
    sa_state = ST_PAUSE;
    sa_pause_sel = 1;
  } else if (n == 32 || n == 33) {
    /* 32: the new Physik corridor (3rd floor), 33: the Bio corridor (4th) */
    sa_have[IT_LAMP] = 1;
    if (n == 32) {
      SaGoFloor(2);
      sa_px = (50 * 16 + 8) << 8;
      sa_py = (6 * 16 + 4) << 8;
    } else {
      SaGoFloor(3);
      sa_px = (58 * 16 + 8) << 8;
      sa_py = (9 * 16 + 4) << 8;
    }
    SaNoThuner();
    sa_pdir = DIR_RIGHT;
    SaGameTick(K_RIGHT);
  } else if (n == 30) {
    /* riding an e-scooter over the schoolyard, a bike parked nearby */
    sa_task = TK_NOTES;
    sa_have[IT_LAMP] = 1;
    SaLerschGo();
    SaGoFloor(4);
    SaNoThuner();
    sa_vh_n = 2;
    sa_vh_t[0] = VH_SCOOTER;
    sa_vh_t[1] = VH_BIKE;
    sa_vh_charge[0] = 120;
    sa_vh_charge[1] = 0;
    sa_vh_x[1] = (24 * 16 + 8) << 8;
    sa_vh_y[1] = (23 * 16 + 10) << 8;
    sa_vh_dir[1] = DIR_LEFT;
    sa_px = (14 * 16 + 8) << 8;
    sa_py = (22 * 16 + 10) << 8;
    sa_vh_x[0] = sa_px;
    sa_vh_y[0] = sa_py;
    sa_vh_dir[0] = DIR_RIGHT;
    SaGameTick(0);
    SaGameTick(K_ACT);
    for (i = 0; i < 8; i++)
      SaGameTick(K_RIGHT);
  }
  SaCamSnap();
}
