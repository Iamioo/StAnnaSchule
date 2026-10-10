/* Headless balance test: a bot plays whole runs against Herr Thüner and
 * Herr Kirschbaum. It walks the shortest path to whatever the current task
 * needs, takes the stairs, sprints when Herr Thüner is after it, answers
 * the phone and calls Conjoke, but never hides. When it is caught it goes
 * on from the checkpoint. Build and run with build_sim.bat [runs]. */
#include <stdio.h>
#include <stdlib.h>
#include "../src/game.h"

#define MAP_W 56
#define MAP_H 28
#define MAP_N 1568
#define LIMIT (40 * 60 * 30)

extern U8 sa_map[MAP_N];
extern I64 sa_state, sa_floor, sa_px, sa_py, sa_ex[], sa_ey[], sa_tst[], sa_tf[], sa_tvalid[],
    sa_task, sa_it_n, sa_it_f[], sa_it_x[], sa_it_y[], sa_it_t[], sa_it_got[], sa_task_item[],
    sa_task_floor[], sa_pool_open, sa_play_ticks, sa_stamina, sa_tired, sa_deaths, sa_ring_t,
    sa_call_cd, sa_cj_st, sa_phone, sa_have[];

U0 SaPlatSound(I64 id) { (void)id; }

static int dist[MAP_N], queue[MAP_N], cur_item = -1;

static int walk(int ch)
{
  return ch == '.' || ch == ',' || ch == 'c' || ch == 't' || ch == 'h' || ch == 'f' ||
         ch == 'm' || ch == '_' || ch == ';' || ch == 'D' || ch == '^' || ch == 'v';
}

static void bfs(int goal)
{
  int head = 0, tail = 0, i, k, n;
  for (i = 0; i < MAP_N; i++)
    dist[i] = -1;
  dist[goal] = 0;
  queue[tail++] = goal;
  while (head < tail) {
    i = queue[head++];
    for (k = 0; k < 4; k++) {
      n = k == 0 ? i - 1 : k == 1 ? i + 1 : k == 2 ? i - MAP_W : i + MAP_W;
      if (n >= 0 && n < MAP_N && dist[n] < 0 && walk(sa_map[n])) {
        dist[n] = dist[i] + 1;
        queue[tail++] = n;
      }
    }
  }
}

/* the tile the bot wants to reach, and what to press once there */
static int target(int *action)
{
  int k, best = -1, bd = 1 << 30, d, pt, t;
  *action = 0;
  if (sa_floor != sa_task_floor[sa_task])
    return 20 * MAP_W + (sa_task_floor[sa_task] > sa_floor ? 1 : 6);
  t = sa_task_item[sa_task];
  if (sa_task == 2) {
    *action = K_ACT;
    return 3 * MAP_W + 1;
  }
  if (sa_task == 4) {
    *action = K_ACT;
    return 13 * MAP_W + 36;
  }
  if (sa_task == 9) {
    *action = K_UP;
    return 1 * MAP_W + 15;
  }
  if (sa_task == 6 && !sa_pool_open) {
    *action = K_UP;
    return 13 * MAP_W + 27;
  }
  pt = (sa_py >> 12) * MAP_W + (sa_px >> 12);
  /* stick to one item until it is picked up; choose by walking distance */
  if (cur_item >= 0 && !sa_it_got[cur_item] && sa_it_f[cur_item] == sa_floor && sa_it_t[cur_item] == t)
    return sa_it_y[cur_item] * MAP_W + sa_it_x[cur_item];
  bfs(pt);
  cur_item = -1;
  for (k = 0; k < sa_it_n; k++)
    if (!sa_it_got[k] && sa_it_t[k] == t && sa_it_f[k] == sa_floor) {
      d = dist[sa_it_y[k] * MAP_W + sa_it_x[k]];
      if (d >= 0 && d < bd) {
        bd = d;
        best = sa_it_y[k] * MAP_W + sa_it_x[k];
        cur_item = k;
      }
    }
  return best < 0 ? pt : best;
}

static int bot_keys(void)
{
  int goal, action, pt, k, n, best = -1, bd = 1 << 30, tx, ty, px, py, keys = 0, chased;
  goal = target(&action);
  bfs(goal);
  px = sa_px >> 8;
  py = sa_py >> 8;
  pt = (py >> 4) * MAP_W + (px >> 4);
  if (pt == goal) {
    tx = (goal % MAP_W) * 16 + 8;
    ty = (goal / MAP_W) * 16 + 10;
    if (action == K_UP)
      return K_UP;
    if (abs(tx - px) <= 3 && abs(ty - py) <= 3)
      return action;
  } else {
    for (k = 0; k < 4; k++) {
      n = k == 0 ? pt - 1 : k == 1 ? pt + 1 : k == 2 ? pt - MAP_W : pt + MAP_W;
      if (n >= 0 && n < MAP_N && dist[n] >= 0 && dist[n] < bd) {
        bd = dist[n];
        best = n;
      }
    }
    if (best < 0)
      return 0;
    tx = (best % MAP_W) * 16 + 8;
    ty = (best / MAP_W) * 16 + 10;
  }
  if (tx > px + 2) keys |= K_RIGHT;
  if (tx < px - 2) keys |= K_LEFT;
  if (ty > py + 2) keys |= K_DOWN;
  if (ty < py - 2) keys |= K_UP;
  chased = sa_tf[0] == sa_floor && sa_tvalid[0] && (sa_tst[0] == 1 || sa_tst[0] == 4);
  if (chased && !sa_tired)
    keys |= K_RUN;
  /* answer Conjoke, or call him when Herr Thüner is close behind */
  if (sa_phone && (sa_ring_t > 0 || (chased && sa_call_cd == 0 && sa_cj_st == 0)) && (sa_play_ticks & 1))
    keys |= K_PHONE;
  return keys;
}

int main(int argc, char **argv)
{
  int runs = argc > 1 ? atoi(argv[1]) : 30, r, wins = 0, keys, prev = 0, st0;
  int only = argc > 2 ? atoi(argv[2]) : -1;
  long total_t = 0, total_d = 0, ticks, last_cont = 0;
  for (r = 0; r < runs; r++) {
    if (only >= 0 && r != only)
      continue;
    SaGameInit(1000 + r * 7919);
    SaGameTick(K_START);
    SaGameTick(0);
    SaGameTick(K_START);
    SaGameTick(0);
    for (ticks = 0; ticks < LIMIT; ticks++) {
      if (sa_state == 2)
        keys = bot_keys();
      else if (sa_state == 4)
        keys = (ticks & 8) ? K_START : 0;
      else
        keys = 0;
      /* a held button only counts once, so let go now and then */
      if ((keys & (K_ACT | K_PHONE)) && (prev & (K_ACT | K_PHONE)))
        keys &= ~(K_ACT | K_PHONE);
      prev = keys;
      st0 = sa_state;
      SaGameTick(keys);
      if (only >= 0 && st0 == 2 && sa_state == 3)
        printf("  caught t%5ld f%d task%d p(%d,%d) thuner (%d,%d), %ld ticks after the last restart\n",
               ticks, sa_floor + 1, sa_task, sa_px >> 12, sa_py >> 12, sa_ex[0] >> 12,
               sa_ey[0] >> 12, ticks - last_cont);
      if (st0 == 4 && sa_state == 2)
        last_cont = ticks;
      if (sa_state == 5)
        break;
    }
    printf("run %2d: %s  %5.1f min  task %d  caught %d  stickers %d\n", r, sa_state == 5 ? "WIN " : "----",
           sa_play_ticks / 1800.0, sa_task, sa_deaths, sa_have[4]);
    if (sa_state == 5) {
      wins++;
      total_t += sa_play_ticks;
      total_d += sa_deaths;
    }
  }
  printf("%d/%d finished", wins, runs);
  if (wins)
    printf(", average %.1f min, caught %.1f times", total_t / 1800.0 / wins, (double)total_d / wins);
  printf("\n");
  return 0;
}
