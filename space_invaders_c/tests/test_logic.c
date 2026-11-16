/*
 * test_logic.c - logic.c の単体テスト。
 * 期待値は原典 spaceinvaders.py の実装と実測（トーチカ 318 セル/基など）から。
 */
#include <stdio.h>
#include "../src/logic.h"

static int failures = 0;
static int checks = 0;

#define CHECK(cond) do { \
    checks++; \
    if (!(cond)) { \
        failures++; \
        printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
    } \
} while (0)

static void kill_column(FleetLedger *l, int col)
{
    for (int r = 0; r < FLEET_ROWS; r++)
        ledger_kill(l, r, col);
}

static void kill_down_to(FleetLedger *l, int target)
{
    for (int c = 0; c < FLEET_COLS && l->count > target; c++)
        for (int r = 0; r < FLEET_ROWS && l->count > target; r++)
            ledger_kill(l, r, c);
}

static void test_init_and_scores(void)
{
    FleetLedger l;
    ledger_init(&l);
    CHECK(l.count == 50);
    CHECK(l.move_time == 200);
    CHECK(l.level == 700);
    CHECK(l.left_alive_column == 0);
    CHECK(l.right_alive_column == 9);
    CHECK(l.alive_column_count == 10);
    CHECK(l.min_row == 0 && l.max_row == 4);

    CHECK(score_for_row(0) == 30);
    CHECK(score_for_row(1) == 20);
    CHECK(score_for_row(2) == 20);
    CHECK(score_for_row(3) == 10);
    CHECK(score_for_row(4) == 10);
    CHECK(mystery_score_for_pick(0) == 50);
    CHECK(mystery_score_for_pick(1) == 100);
    CHECK(mystery_score_for_pick(2) == 150);
    CHECK(mystery_score_for_pick(3) == 300);

    CHECK(barrier_cell_count() == 318);
}

static void test_kill_bookkeeping(void)
{
    FleetLedger l;
    ledger_init(&l);

    /* 中央の1体: 台帳の端や速度は変わらない */
    ledger_kill(&l, 2, 5);
    CHECK(l.count == 49);
    CHECK(l.move_time == 200);   /* 41体以上は表に無い */
    CHECK(l.level == 700);
    CHECK(l.alive_column_count == 10);

    /* 二重に倒しても数えない */
    ledger_kill(&l, 2, 5);
    CHECK(l.count == 49);

    /* 右端の列を全滅: 右端がずれ、加算が入る */
    FleetLedger l2;
    ledger_init(&l2);
    kill_column(&l2, 9);
    CHECK(l2.count == 45);
    CHECK(l2.right_alive_column == 8);
    CHECK(l2.right_add_move == 5);
    CHECK(l2.alive_column_count == 9);
    CHECK(ledger_column_dead(&l2, 9));
    CHECK(!ledger_column_dead(&l2, 8));
    kill_column(&l2, 8);
    CHECK(l2.right_alive_column == 7);
    CHECK(l2.right_add_move == 10);

    /* 左端の列を全滅 */
    FleetLedger l3;
    ledger_init(&l3);
    kill_column(&l3, 0);
    CHECK(l3.left_alive_column == 1);
    CHECK(l3.left_add_move == 5);
    CHECK(l3.alive_column_count == 9);
}

static void test_speed_table(void)
{
    FleetLedger l;
    ledger_init(&l);
    kill_down_to(&l, 41);
    CHECK(l.move_time == 200 && l.level == 700);
    kill_down_to(&l, 40);
    CHECK(l.move_time == 110 && l.level == 400);
    kill_down_to(&l, 30);
    CHECK(l.move_time == 100);
    kill_down_to(&l, 20);
    CHECK(l.move_time == 90);
    kill_down_to(&l, 15);
    CHECK(l.move_time == 80);
    kill_down_to(&l, 10);
    CHECK(l.move_time == 40);
    kill_down_to(&l, 4);
    CHECK(l.move_time == 60);
    kill_down_to(&l, 3);
    CHECK(l.move_time == 60);
    kill_down_to(&l, 2);
    CHECK(l.move_time == 40);
    kill_down_to(&l, 1);
    CHECK(l.move_time == 2 && l.level == 350);
    kill_down_to(&l, 0);
    CHECK(l.count == 0);
}

static void test_min_max_row(void)
{
    FleetLedger l;
    ledger_init(&l);
    for (int c = 0; c < FLEET_COLS; c++)
        ledger_kill(&l, 0, c);
    CHECK(l.min_row == 1);
    CHECK(l.max_row == 4);
    for (int c = 0; c < FLEET_COLS; c++)
        ledger_kill(&l, 4, c);
    CHECK(l.max_row == 3);
}

static void test_bottom_enemy(void)
{
    FleetLedger l;
    int row, col;
    ledger_init(&l);
    CHECK(ledger_bottom_enemy(&l, 0, &row, &col) && row == 4 && col == 0);
    CHECK(ledger_bottom_enemy(&l, 9, &row, &col) && row == 4 && col == 9);
    ledger_kill(&l, 4, 0);
    CHECK(ledger_bottom_enemy(&l, 0, &row, &col) && row == 3 && col == 0);
    kill_column(&l, 0);
    CHECK(ledger_bottom_enemy(&l, 0, &row, &col) && row == 4 && col == 1);
    kill_down_to(&l, 0);
    CHECK(!ledger_bottom_enemy(&l, 0, &row, &col));
}

static void test_weapon_volley(void)
{
    int sound = -1;
    CHECK(weapon_volley(0, 0, true, true, &sound) == VOLLEY_CENTER && sound == 1);
    CHECK(weapon_volley(1990, 0, true, true, &sound) == VOLLEY_CENTER && sound == 1);
    CHECK(weapon_volley(2000, 0, true, true, &sound) == (VOLLEY_LEFT | VOLLEY_RIGHT) && sound == 2);
    CHECK(weapon_volley(3499, 0, true, true, &sound) == (VOLLEY_LEFT | VOLLEY_RIGHT) && sound == 2);
    CHECK(weapon_volley(3500, 0, true, true, &sound) ==
          (VOLLEY_CENTER | VOLLEY_LEFT | VOLLEY_RIGHT) && sound == 2);
    /* 弾が残っていると通常の発射はしない */
    CHECK(weapon_volley(0, 0, false, true, &sound) == 0 && sound == 0);
    /* ラウンド6以上は弾が残っていても3発追加（原典の癖） */
    CHECK(weapon_volley(0, 6, false, true, &sound) ==
          (VOLLEY_CENTER | VOLLEY_LEFT | VOLLEY_RIGHT) && sound == 2);
    /* 自機が居なければ撃てない */
    CHECK(weapon_volley(5000, 6, true, false, &sound) == 0 && sound == 0);
}

static void test_counter_advance(void)
{
    bool w = false;
    /* 通常の巡回: 4→3 は包まない */
    CHECK(fleet_counter_advance(4, 0, 4, &w) == 3 && !w);
    /* 一番下の行が動いたあと包む（min_row=0 で 0→-1） */
    CHECK(fleet_counter_advance(0, 0, 4, &w) == 4 && w);
    /* 上の行が全滅した直後（min_row=2）: 一番上の行が動いたら包む */
    CHECK(fleet_counter_advance(2, 2, 4, &w) == 4 && w);
    CHECK(fleet_counter_advance(3, 2, 4, &w) == 2 && !w);
    /* 止まるパターンの回帰: カウンタが min_row-1 ちょうど／それ以下でも
     * 包み直して復帰する（旧い == 判定では両方とも止まった） */
    CHECK(fleet_counter_advance(1, 2, 4, &w) == 4 && w);
    CHECK(fleet_counter_advance(0, 2, 4, &w) == 4 && w);
    /* 大きく負に沈んだ持ち越し状態からも1回で復帰する */
    CHECK(fleet_counter_advance(-50, 0, 4, &w) == 4 && w);
}

int main(void)
{
    test_init_and_scores();
    test_counter_advance();
    test_kill_bookkeeping();
    test_speed_table();
    test_min_max_row();
    test_bottom_enemy();
    test_weapon_volley();
    printf("logic tests: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
