/* Plays through every task of a run with the teachers switched off, using
 * the real stairs, pick-ups and buttons, and checks that the game can be
 * finished: locked floors stay locked, every item can be reached, the
 * checkpoint works. Run with a few seeds:  progress_test 1 2 3 ...
 * Build: cl /I src tools\progress_test.c src\game.c */
#include <stdio.h>
#include <stdlib.h>
#include "../src/game.h"

#define MAP_W 72
#define MAP_N 2016
extern I64 sa_state, sa_floor, sa_px, sa_py, sa_task, sa_it_n, sa_it_f[], sa_it_x[], sa_it_y[],
    sa_it_t[], sa_it_got[], sa_tvalid[], sa_tf[], sa_troam[], sa_tfollow[], sa_next_ring,
    sa_unlock[], sa_pool_open, sa_power, sa_task_item[], sa_task_floor[], sa_dist[], sa_deaths,
    sa_have[], sa_le_f;
extern U8 sa_map[];
U0 SaBfs(I64 e, I64 goal);
U0 SaCaught(CH *reason);
U0 SaLoadMap(I64 f);
U0 SaLoadFloor(I64 f);

U0 SaPlatSound(I64 id) { (void)id; }

static int fails;
static long ticks;

static void check(int ok, const char *what)
{
  if (!ok) {
    printf("  FAIL: %s (task %d, floor %d)\n", what, sa_task, sa_floor + 1);
    fails++;
  }
}

static void quiet(void)
{
  int i;
  for (i = 0; i < 2; i++) {
    if (sa_tf[i] == sa_floor)
      sa_tf[i] = (sa_floor + 3) % 6;
    sa_tvalid[i] = 0;
    sa_troam[i] = 999999;
    sa_tfollow[i] = 0;
  }
  sa_next_ring = 999999;
}

static void tick(int keys)
{
  quiet();
  SaGameTick(keys);
  ticks++;
}

static void put(int tx, int ty)
{
  sa_px = (tx * 16 + 8) << 8;
  sa_py = (ty * 16 + 10) << 8;
}

/* uses the real stairs, one floor at a time */
static int travel(int f)
{
  int i, from;
  while (sa_floor != f) {
    from = sa_floor;
    put(3, 19);
    tick(0);
    if (f > sa_floor)
      put(1, 20);
    else
      put(6, 20);
    for (i = 0; i < 60 && sa_floor == from; i++)
      tick(0);
    if (sa_floor == from) {
      check(0, "stairs did not work");
      return 0;
    }
    check(sa_floor == from + (f > from ? 1 : -1), "stairs skipped a floor");
    /* standing still after arriving must not take the stairs again */
    for (i = 0; i < 60; i++)
      tick(0);
    check(sa_floor == from + (f > from ? 1 : -1), "arrived and went on by itself");
  }
  return 1;
}

static void press(int keys)
{
  tick(keys);
  tick(0);
}

/* walk into the tile above for a few ticks (doors, the exit) */
static void bump_up(int tx, int ty)
{
  int i;
  put(tx, ty);
  for (i = 0; i < 12; i++)
    tick(K_UP);
}

int main(int argc, char **argv)
{
  int a, k, t, i, task, seed, reach, cp_task, cp_floor;
  for (a = 1; a < argc || a == 1; a++) {
    seed = argc > 1 ? atoi(argv[a]) : 1;
    fails = 0;
    ticks = 0;
    SaGameInit(seed);
    press(K_START);
    press(K_START);
    check(sa_state == 2, "not playing");

    /* every item must be reachable from the staircase */
    for (k = 0; k < sa_it_n; k++) {
      sa_pool_open = sa_it_f[k] == 0;
      SaLoadMap(sa_it_f[k]);
      SaBfs(0, 19 * MAP_W + 3);
      reach = sa_dist[sa_it_y[k] * MAP_W + sa_it_x[k]] >= 0;
      if (!reach)
        printf("  item %d (type %d) on floor %d at %d,%d unreachable\n", k, sa_it_t[k],
               sa_it_f[k] + 1, sa_it_x[k], sa_it_y[k]);
      check(reach, "item unreachable");
    }
    sa_pool_open = 0;
    SaLoadFloor(3);
    put(13, 10);

    /* locked at the start: both ways out of floor 4 */
    put(6, 20);
    for (i = 0; i < 40; i++)
      tick(0);
    check(sa_floor == 3, "went down although locked");
    put(1, 20);
    for (i = 0; i < 40; i++)
      tick(0);
    check(sa_floor == 3, "went up although locked");

    cp_task = -1;
    cp_floor = -1;
    for (task = 0; task < 10 && sa_state == 2; task++) {
      check(sa_task == task, "task order");
      if (!travel(sa_task_floor[task]))
        break;
      t = sa_task_item[task];
      if (t >= 0) {
        if (task == 6) {
          check(!sa_pool_open, "pool open too early");
          bump_up(27, 13);
          check(sa_pool_open, "pool did not open");
        }
        for (k = 0; k < sa_it_n; k++)
          if (sa_it_t[k] == t && !sa_it_got[k]) {
            check(sa_it_f[k] == sa_floor, "task item on another floor");
            put(sa_it_x[k], sa_it_y[k]);
            tick(0);
            check(sa_it_got[k], "item not picked up");
          }
      } else if (task == 2) {
        /* the piano: in the music rooms of the 4th floor */
        int px = -1, py = -1, x, y;
        for (y = 0; y < 28; y++)
          for (x = 0; x < MAP_W; x++)
            if (sa_map[y * MAP_W + x] == 'M' && px < 0) {
              px = x;
              py = y + 1;
            }
        check(px >= 0, "no piano on the task floor");
        put(px, py);
        press(K_ACT);
      } else if (task == 4) {
        put(36, 13);
        press(K_ACT);
        check(sa_power, "no power");
      } else if (task == 9) {
        bump_up(15, 1);
        check(sa_state == 5, "could not leave through the main entrance");
        break;
      }
      check(sa_task == task + 1, "task not finished");
      check(sa_le_f == sa_task_floor[sa_task], "Herr Lersch is not on the task floor");
      /* try the checkpoint once, in the middle of the run */
      if (task == 4) {
        cp_task = sa_task;
        SaCaught("TEST");
        for (i = 0; i < 90; i++)
          tick(0);
        check(sa_state == 4, "no game over screen");
        press(K_START);
        cp_floor = sa_floor;
        check(sa_state == 2 && sa_task == cp_task, "checkpoint lost the task");
        check(cp_floor == sa_task_floor[cp_task], "checkpoint on the wrong floor");
        check(sa_unlock[0] && sa_unlock[1] && sa_unlock[2], "checkpoint lost unlocked floors");
      }
    }
    printf("seed %d: %s, %ld ticks, stickers %d, deaths %d\n", seed,
           fails ? "FAILED" : "ok", ticks, sa_have[4], sa_deaths);
  }
  return fails != 0;
}
