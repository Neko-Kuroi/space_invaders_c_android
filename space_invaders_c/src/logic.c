/*
 * logic.c - logic.h の実装。規則の出典は原典 spaceinvaders.py の各関数。
 */
#include "logic.h"

void ledger_init(FleetLedger *l)
{
    for (int r = 0; r < FLEET_ROWS; r++)
        for (int c = 0; c < FLEET_COLS; c++)
            l->alive[r][c] = true;
    l->count = FLEET_ROWS * FLEET_COLS;
    for (int c = 0; c < FLEET_COLS; c++)
        l->column_alive[c] = true;
    l->alive_column_count = FLEET_COLS;
    l->left_alive_column = 0;
    l->right_alive_column = FLEET_COLS - 1;
    l->left_add_move = 0;
    l->right_add_move = 0;
    l->min_row = 0;
    l->max_row = 4;
    l->move_time = 200;
    l->level = 700;
}

bool ledger_column_dead(const FleetLedger *l, int col)
{
    if (col < 0 || col >= FLEET_COLS)
        return true; /* 原典では到達しない経路の安全側 */
    return !l->column_alive[col];
}

/* 原典 update_speed()。残数で move_time と level が変わる表と、
 * minRow / maxRow の再計算。0体では早期 return（修正済み原典の形）。 */
static void ledger_update_speed(FleetLedger *l)
{
    if (l->count == 0)
        return;

    int minimum = 5, maximum = 0;
    for (int r = 0; r < FLEET_ROWS; r++)
        for (int c = 0; c < FLEET_COLS; c++)
            if (l->alive[r][c]) {
                if (r < minimum) minimum = r;
                if (r > maximum) maximum = r;
            }
    l->min_row = minimum;
    l->max_row = maximum;

    if (l->count == 1) {
        l->move_time = 2;
        l->level = 350;
    } else if (l->count == 2) {
        l->move_time = 40;
    } else if (l->count == 3) {
        l->move_time = 60;
    } else if (l->count == 4) {
        l->move_time = 60;
    } else if (l->count <= 10) {
        l->move_time = 40;
    } else if (l->count <= 15) {
        l->move_time = 80;
    } else if (l->count <= 20) {
        l->move_time = 90;
    } else if (l->count <= 30) {
        l->move_time = 100;
    } else if (l->count <= 40) {
        l->move_time = 110;
        l->level = 400;
    }
    /* 41体以上では原典も何も変えない（初期値 200 / 700 が残る） */
}

void ledger_kill(FleetLedger *l, int row, int col)
{
    if (row < 0 || row >= FLEET_ROWS || col < 0 || col >= FLEET_COLS)
        return;
    if (!l->alive[row][col])
        return;
    l->alive[row][col] = false;
    l->count--;

    /* 列が全滅したか */
    bool col_dead = true;
    for (int r = 0; r < FLEET_ROWS; r++)
        if (l->alive[r][col]) { col_dead = false; break; }
    if (col_dead && l->column_alive[col]) {
        l->column_alive[col] = false;
        l->alive_column_count--;
    }

    /* 原典 kill(): 端の列が全滅したら、生きている端までずらしながら
     * 反対側の移動回数に 5 ずつ加える。右端の判定が先（elif でない方）。 */
    if (col == l->right_alive_column) {
        bool dead = col_dead;
        while (l->right_alive_column > 0 && dead) {
            l->right_alive_column--;
            l->right_add_move += 5;
            dead = ledger_column_dead(l, l->right_alive_column);
        }
    } else if (col == l->left_alive_column) {
        bool dead = col_dead;
        while (l->left_alive_column < FLEET_COLS && dead) {
            l->left_alive_column++;
            l->left_add_move += 5;
            dead = ledger_column_dead(l, l->left_alive_column);
        }
    }

    ledger_update_speed(l);
}

bool ledger_bottom_enemy(const FleetLedger *l, int pick, int *out_row, int *out_col)
{
    if (l->alive_column_count == 0 || pick < 0 || pick >= l->alive_column_count)
        return false;
    int seen = -1;
    for (int c = 0; c < FLEET_COLS; c++) {
        if (!l->column_alive[c])
            continue;
        seen++;
        if (seen == pick) {
            for (int r = FLEET_ROWS - 1; r >= 0; r--) {
                if (l->alive[r][c]) {
                    *out_row = r;
                    *out_col = c;
                    return true;
                }
            }
        }
    }
    return false;
}

int fleet_counter_advance(int counter, int min_row, int max_row, bool *wrapped)
{
    counter -= 1;
    if (counter <= min_row - 1) {
        *wrapped = true;
        return max_row;
    }
    *wrapped = false;
    return counter;
}

int score_for_row(int row)
{
    static const int scores[FLEET_ROWS] = {30, 20, 20, 10, 10};
    if (row < 0 || row >= FLEET_ROWS)
        return 0;
    return scores[row];
}

int mystery_score_for_pick(int pick)
{
    static const int scores[4] = {50, 100, 150, 300};
    if (pick < 0 || pick >= 4)
        return 0;
    return scores[pick];
}

int weapon_volley(int score, int round_num, bool bullets_empty, bool ship_alive,
                  int *out_sound)
{
    int volley = 0;
    int sound = 0;
    if (bullets_empty && ship_alive) {
        if (score <= 1990) {
            volley = VOLLEY_CENTER;
            sound = 1;
        } else if (score < 3500) {
            volley = VOLLEY_LEFT | VOLLEY_RIGHT;
            sound = 2;
        } else {
            volley = VOLLEY_CENTER | VOLLEY_LEFT | VOLLEY_RIGHT;
            sound = 2;
        }
    }
    if (round_num >= 6 && ship_alive) {
        volley |= VOLLEY_CENTER | VOLLEY_LEFT | VOLLEY_RIGHT;
        sound = 2;
    }
    if (out_sound)
        *out_sound = sound;
    return volley;
}

const unsigned char BARRIER_DESIGN[BARRIER_ROWS][BARRIER_COLS] = {
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0},
    {0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0},
    {0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0},
    {0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0},
    {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
    {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
    {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
    {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
    {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
    {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
    {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
    {1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1},
    {1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1},
    {1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1},
    {1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1},
};

int barrier_cell_count(void)
{
    int n = 0;
    for (int r = 0; r < BARRIER_ROWS; r++)
        for (int c = 0; c < BARRIER_COLS; c++)
            if (BARRIER_DESIGN[r][c])
                n++;
    return n;
}
