/*
 * game.c - ゲームの進行と描画。原典 spaceinvaders.py の各メソッドの写し。
 * 対応関係はコメントに原典の名前を残してある。
 */
#include "game.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL_mixer.h>

static const SDL_Color COL_WHITE  = {255, 255, 255, 255};
static const SDL_Color COL_GREEN  = {78, 255, 87, 255};
static const SDL_Color COL_BLUE   = {80, 255, 239, 255};
static const SDL_Color COL_PURPLE = {203, 0, 255, 255};
static const SDL_Color COL_RED    = {237, 28, 36, 255};

/* Enemy.load_images の対応: 行ごとの2枚（index 0 が先） */
static int enemy_texture(int row, int index)
{
    static const int table[FLEET_ROWS][2] = {
        {TEX_ENEMY1_2, TEX_ENEMY1_1},
        {TEX_ENEMY2_2, TEX_ENEMY2_1},
        {TEX_ENEMY2_2, TEX_ENEMY2_1},
        {TEX_ENEMY3_1, TEX_ENEMY3_2},
        {TEX_ENEMY3_1, TEX_ENEMY3_2},
    };
    return table[row][index & 1];
}

/* EnemyExplosion.get_image の対応: 行ごとの色 */
static int explosion_texture(int row)
{
    static const int table[FLEET_ROWS] = {
        TEX_EXPLOSIONPURPLE, TEX_EXPLOSIONBLUE, TEX_EXPLOSIONBLUE,
        TEX_EXPLOSIONGREEN, TEX_EXPLOSIONGREEN,
    };
    return table[row];
}

static bool overlap(const SDL_Rect *a, const SDL_Rect *b)
{
    return SDL_HasIntersection(a, b) == SDL_TRUE;
}

static Enemy *enemy_at(Game *g, int row, int col)
{
    return &g->enemies[row * FLEET_COLS + col];
}

static int pbullet_count(const Game *g)
{
    int n = 0;
    for (int i = 0; i < MAX_PBULLETS; i++)
        if (g->pbullets[i].active) n++;
    return n;
}

static int explosion_count(const Game *g)
{
    int n = 0;
    for (int i = 0; i < MAX_EXPLOSIONS; i++)
        if (g->explosions[i].active) n++;
    return n;
}

static void spawn_explosion(Game *g, ExplosionType type, SDL_Rect r, int row,
                            int score, Uint32 now)
{
    for (int i = 0; i < MAX_EXPLOSIONS; i++) {
        if (!g->explosions[i].active) {
            g->explosions[i].type = type;
            g->explosions[i].r = r;
            g->explosions[i].row = row;
            g->explosions[i].score = score;
            g->explosions[i].born = now;
            g->explosions[i].active = true;
            return;
        }
    }
}

static void spawn_bullet(Game *g, bool enemy, int x, int y, int direction,
                         int speed, Uint32 now)
{
    Bullet *arr = enemy ? g->ebullets : g->pbullets;
    int cap = enemy ? MAX_EBULLETS : MAX_PBULLETS;
    for (int i = 0; i < cap; i++) {
        if (!arr[i].active) {
            arr[i].r.x = x;
            arr[i].r.y = y;
            arr[i].r.w = 5;
            arr[i].r.h = 15;
            arr[i].direction = direction;
            arr[i].speed = speed;
            arr[i].born = now;
            arr[i].enemy = enemy;
            arr[i].flashing = false;
            arr[i].active = true;
            return;
        }
    }
}

/* 原典 make_blockers1（4基分）＋地面。地面は原典では4基分が同じ位置に
 * 重なって生成されるため、見た目と判定が等価な1本にまとめた。 */
static void build_blockers(Game *g)
{
    for (int b = 0; b < BLOCKER_COUNT; b++)
        for (int r = 0; r < BARRIER_ROWS; r++)
            for (int c = 0; c < BARRIER_COLS; c++)
                g->blockers[b][r][c] = BARRIER_DESIGN[r][c] != 0;
    for (int i = 0; i < GROUND_CELLS; i++)
        g->ground[i] = true;
}

static SDL_Rect blocker_cell_rect(int base, int row, int col)
{
    SDL_Rect r = {BLOCKER_X0 + BLOCKER_PITCH * base + col * BLOCKER_CELL,
                  BLOCKERS_POSITION + BLOCKER_Y_OFFSET + row * BLOCKER_CELL,
                  BLOCKER_CELL, BLOCKER_CELL};
    return r;
}

/* 1基ぶんのセル全体を囲む矩形。弾や敵がここに触れないなら、その基の
 * セルは1つずつ調べなくてよい（早期除外）。 */
static SDL_Rect blocker_base_area(int base)
{
    SDL_Rect r = {BLOCKER_X0 + BLOCKER_PITCH * base,
                  BLOCKERS_POSITION + BLOCKER_Y_OFFSET,
                  BARRIER_COLS * BLOCKER_CELL, BARRIER_ROWS * BLOCKER_CELL};
    return r;
}

/* r に重なるトーチカのセルを全部消す（原典の groupcollide の dokill）。
 * 1つでも消したら true。 */
static bool blockers_erase_overlap(Game *g, const SDL_Rect *r)
{
    bool hit = false;
    for (int base = 0; base < BLOCKER_COUNT; base++) {
        SDL_Rect area = blocker_base_area(base);
        if (!overlap(r, &area))
            continue;
        for (int row = 0; row < BARRIER_ROWS; row++)
            for (int col = 0; col < BARRIER_COLS; col++) {
                if (!g->blockers[base][row][col]) continue;
                SDL_Rect cell = blocker_cell_rect(base, row, col);
                if (overlap(r, &cell)) {
                    g->blockers[base][row][col] = false;
                    hit = true;
                }
            }
    }
    return hit;
}

/* r に重なる地面のセルを全部消す。1つでも消したら true。 */
static bool ground_erase_overlap(Game *g, const SDL_Rect *r)
{
    if (r->y + r->h <= GROUND_Y || r->y >= GROUND_Y + GROUND_CELL_W)
        return false;
    bool hit = false;
    for (int gi = 0; gi < GROUND_CELLS; gi++) {
        if (!g->ground[gi]) continue;
        SDL_Rect cell = {gi * GROUND_CELL_W, GROUND_Y, GROUND_CELL_W, GROUND_CELL_W};
        if (overlap(r, &cell)) {
            g->ground[gi] = false;
            hit = true;
        }
    }
    return hit;
}

/* 自機を初期位置に置く（開始・ラウンド開始・再出撃で共通）。 */
static void ship_place(Ship *s)
{
    s->r.x = SHIP_START_X;
    s->r.y = SHIP_START_Y;
    s->r.w = SHIP_W;
    s->r.h = SHIP_H;
    s->vx = SHIP_VX;
}

/* 原典 reset(score)。残機（lives_spare）はここでは変えない。 */
static void reset_round(Game *g, int score, Uint32 now)
{
    g->score = score;
    ship_place(&g->ship);
    g->ship_alive = true;
    g->make_new_ship = false;
    g->ship_timer = now;

    for (int row = 0; row < FLEET_ROWS; row++) {
        for (int col = 0; col < FLEET_COLS; col++) {
            Enemy *e = enemy_at(g, row, col);
            e->r.x = 157 + col * 50;
            e->r.y = g->enemy_position + row * 45;
            e->r.w = 40;
            e->r.h = 35;
            e->row = row;
            e->col = col;
            e->image_index = 0;
            /* Enemy の timer = 生成時刻 + (行×-50 + 列×120)。
             * 描画条件は「now - timer > 200」のため、この時刻を過ぎるまで
             * その敵は描かれない（出現がずれる原典の癖）。 */
            e->draw_from = (Uint32)((Sint32)now + (row * -50 + col * 120) + 200);
            e->alive = true;
        }
    }
    ledger_init(&g->ledger); /* move_time=200, level=700 を含む初期化 */
    g->row_counter = 4; /* 原典の修正に合わせ、開始時に初期値へ戻す */
    g->fleet_direction = 1;
    g->move_number = 15;
    g->left_moves = 30;
    g->right_moves = 30;
    /* EnemiesGroup の timer は timeOffset = 22000 * ROUND_NUM だけ過去。
     * ラウンド2以降は「追いつきの速進」になる原典の挙動のまま。 */
    g->fleet_timer = (Uint32)((Sint32)now - 22000 * g->round_num);
    g->fleet_bottom = g->enemy_position + (FLEET_ROWS - 1) * 45 + 35;

    g->mystery.r.x = -80;
    g->mystery.r.y = 25;
    g->mystery.r.w = 80;
    g->mystery.r.h = 40;
    g->mystery.direction = 1;
    g->mystery.timer = now;
    g->mystery.play_sound = true;
    g->mystery.visible = false;
    g->mystery.channel = -1;

    memset(g->pbullets, 0, sizeof g->pbullets);
    memset(g->ebullets, 0, sizeof g->ebullets);
    memset(g->explosions, 0, sizeof g->explosions);

    g->shoot_timer = now;
    g->note_timer = now;
    g->note_index = 0;
    g->game_over_timer = 0;
}

/* タイトルでキーを離したときの開始処理（原典 main の mainScreen 分岐） */
static void start_game(Game *g, Uint32 now)
{
    build_blockers(g);
    g->lives_spare = 3;
    g->enemy_position = ENEMY_DEFAULT_POSITION;
    reset_round(g, 0, now);
    g->state = ST_PLAY;
}

static void enter_game_over(Game *g, Uint32 now)
{
    g->state = ST_GAMEOVER;
    g->game_over_timer = now;
    g->round_num = 0;
    g->enemy_position = ENEMY_DEFAULT_POSITION;
}

/* 原典 check_input の発射部分。武器の規則は logic の weapon_volley。 */
static void fire(Game *g, Uint32 now)
{
    int sound = 0;
    int volley = weapon_volley(g->score, g->round_num, pbullet_count(g) == 0,
                               g->ship_alive, &sound);
    if (volley & VOLLEY_CENTER)
        spawn_bullet(g, false, g->ship.r.x + 23, g->ship.r.y + 5, -1, 15, now);
    if (volley & VOLLEY_LEFT)
        spawn_bullet(g, false, g->ship.r.x + 8, g->ship.r.y + 5, -1, 15, now);
    if (volley & VOLLEY_RIGHT)
        spawn_bullet(g, false, g->ship.r.x + 38, g->ship.r.y + 5, -1, 15, now);
    /* 原典は第1ブロックと第2ブロックで別々に音を鳴らす。ここでは
     * weapon_volley が返した最後の音だけを鳴らす（聞き分けできない差）。 */
    if (sound == 1)
        assets_play(g->assets, SND_SHOOT);
    else if (sound == 2)
        assets_play(g->assets, SND_SHOOT2);
}

/* 原典 EnemiesGroup.update */
static void fleet_update(Game *g, Uint32 now)
{
    FleetLedger *l = &g->ledger;
    if ((Sint32)(now - g->fleet_timer) <= l->move_time)
        return;

    int max_move = (g->fleet_direction == 1)
                       ? g->right_moves + l->right_add_move
                       : g->left_moves + l->left_add_move;
    if (g->move_number >= max_move) {
        g->left_moves = 30 + l->right_add_move;
        g->right_moves = 30 + l->left_add_move;
        g->fleet_direction *= -1;
        g->move_number = 0;
        g->fleet_bottom = 0;
        for (int i = 0; i < FLEET_ROWS * FLEET_COLS; i++) {
            Enemy *e = &g->enemies[i];
            if (!e->alive) continue;
            e->r.y += ENEMY_MOVE_DOWN;
            e->image_index ^= 1;
            if (g->fleet_bottom < e->r.y + 35)
                g->fleet_bottom = e->r.y + 35;
        }
    } else {
        int velocity = (g->fleet_direction == 1) ? 10 : -10;
        if (l->count <= 4) {
            for (int i = 0; i < FLEET_ROWS * FLEET_COLS; i++) {
                Enemy *e = &g->enemies[i];
                if (!e->alive) continue;
                e->r.x += velocity;
                e->image_index ^= 1;
            }
            g->move_number += 1;
        } else {
            /* ROW_counter の行だけが動く（波状の動き） */
            for (int i = 0; i < FLEET_ROWS * FLEET_COLS; i++) {
                Enemy *e = &g->enemies[i];
                if (!e->alive) continue;
                if (e->row == g->row_counter) {
                    e->r.x += velocity;
                    e->image_index ^= 1;
                }
            }
            bool wrapped = false;
            g->row_counter = fleet_counter_advance(g->row_counter,
                                                   l->min_row, l->max_row,
                                                   &wrapped);
            if (wrapped)
                g->move_number += 1;
        }
    }
    g->fleet_timer += (Uint32)l->move_time;
}

/* 原典 Ship.update。加速と「差の1/3ずつ寄る」式をそのまま写した。
 * 左は Python の負数の床割り算のため切り上げ、右は切り捨てになる。 */
static void ship_update(Game *g, const Input *in)
{
    Ship *s = &g->ship;
    if (in->held_a && s->r.x > 10) {
        s->vx += 1;
        s->r.x -= (s->vx + 2) / 3;
        if (s->r.x < 10) s->r.x = 10;
    }
    if (in->held_d && s->r.x < 740) {
        s->vx += 1;
        s->r.x += s->vx / 3;
        if (s->r.x > 740) s->r.x = 740;
    }
}

/* 原典 Bullet.update（自機弾・敵弾共通の式） */
static void bullets_update(Game *g, Uint32 now)
{
    for (int pass = 0; pass < 2; pass++) {
        Bullet *arr = pass == 0 ? g->pbullets : g->ebullets;
        int cap = pass == 0 ? MAX_PBULLETS : MAX_EBULLETS;
        for (int i = 0; i < cap; i++) {
            Bullet *b = &arr[i];
            if (!b->active) continue;
            b->r.y += b->speed * b->direction;
            if (b->r.y < 35) {
                b->r.y = 35;
                b->flashing = true;
                if (now > b->born + 700)
                    b->active = false;
            }
            if (b->r.y > 585) {
                b->r.y = 580;
                b->flashing = true;
                if (now > b->born + 160)
                    b->active = false;
            }
        }
    }
}

/* 原典 Mystery.update。描画は「動いたフレームだけ」のため visible で返す。 */
static void mystery_update(Game *g, Uint32 now)
{
    Mystery *m = &g->mystery;
    m->visible = false;
    Uint32 passed = now - m->timer;
    bool reset_timer = false;
    if (passed > MYSTERY_INTERVAL_MS) {
        if ((m->r.x < 0 || m->r.x > 800) && m->play_sound) {
            /* 専用チャンネル（予約済み）で鳴らす。以後のフェードアウトや
             * 停止はこの音にだけかかる */
            m->channel = assets_play_on(g->assets, SND_MYSTERYENTERED,
                                        MYSTERY_CHANNEL);
            m->play_sound = false;
        }
        if (m->r.x < 840 && m->direction == 1) {
            if (m->channel >= 0)
                Mix_FadeOutChannel(m->channel, 4000);
            m->r.x += 2;
            m->visible = true;
        } else if (m->r.x > -90 && m->direction == -1) {
            if (m->channel >= 0)
                Mix_FadeOutChannel(m->channel, 4000);
            m->r.x -= 2;
            m->visible = true;
        }
    }
    if (m->r.x > 830) {
        m->play_sound = true;
        m->direction = -1;
        reset_timer = true;
    }
    if (m->r.x < -90) {
        m->play_sound = true;
        m->direction = 1;
        reset_timer = true;
    }
    if (passed > MYSTERY_INTERVAL_MS && reset_timer)
        m->timer = now;
}

/* 原典 play_main_music */
static void play_music(Game *g, Uint32 now)
{
    Uint32 interval = (Uint32)(g->ledger.move_time * 3);
    if (now - g->note_timer > interval) {
        assets_play(g->assets, SND_NOTE0 + g->note_index);
        g->note_index = (g->note_index < 3) ? g->note_index + 1 : 0;
        g->note_timer += interval;
    }
}

/* 原典 make_enemies_shoot */
static void make_enemies_shoot(Game *g, Uint32 now)
{
    if ((Sint32)(now - g->shoot_timer) > g->ledger.level && g->ledger.count > 0) {
        int row, col;
        int pick = rand() % g->ledger.alive_column_count;
        if (ledger_bottom_enemy(&g->ledger, pick, &row, &col)) {
            Enemy *e = enemy_at(g, row, col);
            spawn_bullet(g, true, e->r.x + 14, e->r.y + 20, 1, 5, now);
            g->shoot_timer = now;
        }
    }
}

/* 原典 create_new_ship */
static void create_new_ship(Game *g, Uint32 now)
{
    if (g->make_new_ship && (Sint32)(now - g->ship_timer) > SHIP_RESPAWN_MS) {
        ship_place(&g->ship);
        g->make_new_ship = false;
        g->ship_alive = true;
    }
}

/* ---- 原典 check_collisions（処理の順番も原典どおり） ---- */

static void kill_enemy_no_score(Game *g, Enemy *e)
{
    e->alive = false;
    ledger_kill(&g->ledger, e->row, e->col);
    if (g->ledger.count == 0)
        g->fleet_bottom = 0; /* 後段の判定が空の集団で誤作動しないように */
}

/* 弾とトーチカ／地面の衝突（原典の groupcollide 相当）。
 * 原典は重なったセルを全部消すので、自機弾・敵弾とも同じ規則で
 * 全セルを消す。敵弾のときは原典どおり bulletExplosion を置く。 */
static void bullets_vs_blockers(Game *g, bool enemy, Uint32 now)
{
    Bullet *arr = enemy ? g->ebullets : g->pbullets;
    int cap = enemy ? MAX_EBULLETS : MAX_PBULLETS;
    for (int i = 0; i < cap; i++) {
        Bullet *b = &arr[i];
        if (!b->active) continue;
        bool hit = blockers_erase_overlap(g, &b->r);
        if (ground_erase_overlap(g, &b->r))
            hit = true;
        if (hit) {
            b->active = false;
            if (enemy)
                spawn_explosion(g, EX_BULLET, b->r, 0, 0, now);
        }
    }
}

/* (a) 自機弾 × 敵弾 */
static void collide_bullets_with_bullets(Game *g)
{
    /* 両方消える */
    for (int i = 0; i < MAX_PBULLETS; i++) {
        Bullet *b = &g->pbullets[i];
        if (!b->active) continue;
        for (int j = 0; j < MAX_EBULLETS; j++) {
            Bullet *eb = &g->ebullets[j];
            if (eb->active && overlap(&b->r, &eb->r)) {
                eb->active = false;
                b->active = false;
                break;
            }
        }
    }
}

/* (b) 敵 × 自機弾 */
static void collide_enemies_with_pbullets(Game *g, Uint32 now)
{
    /* (b) 敵 × 自機弾: pygame の groupcollide は行優先の順に処理し、
     * 弾は最初に当たった敵で消える。その順番で写す。 */
    for (int i = 0; i < FLEET_ROWS * FLEET_COLS; i++) {
        Enemy *e = &g->enemies[i];
        if (!e->alive) continue;
        for (int j = 0; j < MAX_PBULLETS; j++) {
            Bullet *b = &g->pbullets[j];
            if (b->active && overlap(&e->r, &b->r)) {
                b->active = false;
                assets_play(g->assets, SND_INVADERKILLED);
                g->score += score_for_row(e->row);
                spawn_explosion(g, EX_ENEMY, e->r, e->row, 0, now);
                g->game_timer = now;
                e->alive = false;
                ledger_kill(&g->ledger, e->row, e->col);
                break;
            }
        }
    }
}

/* (c) ミステリーシップ × 自機弾 */
static void collide_mystery_with_pbullets(Game *g, Uint32 now)
{
    /* (c) ミステリーシップ × 自機弾 */
    for (int j = 0; j < MAX_PBULLETS; j++) {
        Bullet *b = &g->pbullets[j];
        if (b->active && overlap(&g->mystery.r, &b->r)) {
            b->active = false;
            if (g->mystery.channel >= 0)
                Mix_HaltChannel(g->mystery.channel);
            assets_play(g->assets, SND_MYSTERYKILLED);
            int gained = mystery_score_for_pick(rand() % 4);
            g->score += gained;
            spawn_explosion(g, EX_MYSTERY_SCORE, g->mystery.r, 5, gained, now);
            /* 原典は新しい Mystery を作り直す（次の出現は25秒後） */
            g->mystery.r.x = -80;
            g->mystery.r.y = 25;
            g->mystery.direction = 1;
            g->mystery.timer = now;
            g->mystery.play_sound = true;
            g->mystery.visible = false;
            g->mystery.channel = -1;
            break;
        }
    }
}

/* (d) 自機 × 敵弾 */
static void collide_ship_with_ebullets(Game *g, Uint32 now)
{
    /* (d) 自機 × 敵弾 */
    if (g->ship_alive) {
        for (int j = 0; j < MAX_EBULLETS; j++) {
            Bullet *eb = &g->ebullets[j];
            if (eb->active && overlap(&g->ship.r, &eb->r)) {
                eb->active = false;
                assets_play(g->assets, SND_SHIPEXPLOSION);
                spawn_explosion(g, EX_SHIP, g->ship.r, 0, 0, now);
                if (g->lives_spare > 0) {
                    g->lives_spare--;
                } else {
                    enter_game_over(g, now);
                }
                g->make_new_ship = true;
                g->ship_timer = now;
                g->ship_alive = false;
                break;
            }
        }
    }
}

/* (e) 敵集団が自機の高さまで来たとき */
static void collide_fleet_reach(Game *g, Uint32 now)
{
    /* (e) 敵集団が自機の高さまで来たとき */
    if (g->state == ST_PLAY && g->fleet_bottom >= FLEET_REACH_SHIP_Y) {
        if (g->ship_alive) {
            for (int i = 0; i < FLEET_ROWS * FLEET_COLS; i++) {
                Enemy *e = &g->enemies[i];
                if (e->alive && overlap(&e->r, &g->ship.r)) {
                    kill_enemy_no_score(g, e);
                    g->ship_alive = false; /* groupcollide で自機も消える */
                }
            }
        }
        if (!g->ship_alive || g->fleet_bottom >= FLEET_GAMEOVER_Y)
            enter_game_over(g, now);
    }
}

/* (h) 敵集団がトーチカの高さまで来たら、重なったセルが消える */
static void collide_fleet_with_blockers(Game *g)
{
    if (g->fleet_bottom < BLOCKERS_POSITION)
        return;
    for (int i = 0; i < FLEET_ROWS * FLEET_COLS; i++) {
        Enemy *e = &g->enemies[i];
        if (e->alive)
            blockers_erase_overlap(g, &e->r);
    }
}

/* 原典 check_collisions。処理の順番も原典どおり。 */
static void check_collisions(Game *g, Uint32 now)
{
    collide_bullets_with_bullets(g);                  /* (a) */
    collide_enemies_with_pbullets(g, now);            /* (b) */
    collide_mystery_with_pbullets(g, now);            /* (c) */
    collide_ship_with_ebullets(g, now);               /* (d) */
    collide_fleet_reach(g, now);                      /* (e) */
    bullets_vs_blockers(g, false, now);               /* (f) 自機弾 × トーチカ・地面 */
    bullets_vs_blockers(g, true, now);                /* (g) 敵弾 × トーチカ・地面 */
    collide_fleet_with_blockers(g);                   /* (h) */
}

/* 爆発の寿命の管理（原典の各 Explosion.update の消滅条件）。
 * 描画側では状態を書き換えないよう、消滅判定はここ（更新側）に置く。
 * ラウンド遷移の条件（explosion_count == 0）も更新側だけで決まる。 */
static void explosions_update(Game *g, Uint32 now)
{
    for (int i = 0; i < MAX_EXPLOSIONS; i++) {
        Explosion *ex = &g->explosions[i];
        if (!ex->active) continue;
        Uint32 passed = now - ex->born;
        switch (ex->type) {
        case EX_ENEMY:         if (passed > 400) ex->active = false; break;
        case EX_SHIP:          if (passed > 800) ex->active = false; break;
        case EX_BULLET:        if (passed > 300) ex->active = false; break;
        case EX_MYSTERY_SCORE: if (passed > 600) ex->active = false; break;
        }
    }
}

/* ---- 状態ごとの更新 ---- */

void game_init(Game *g, Assets *assets)
{
    memset(g, 0, sizeof *g);
    g->assets = assets;
    g->state = ST_TITLE;
    g->round_num = 0;
    g->enemy_position = ENEMY_DEFAULT_POSITION;
    g->row_counter = 4;
    g->lives_spare = 3;
    ship_place(&g->ship);
    g->ship_alive = true;
    ledger_init(&g->ledger);
    build_blockers(g);
}

int game_update(Game *g, const Input *in, Uint32 now)
{
    if (in->quit)
        return 0;

    switch (g->state) {
    case ST_TITLE:
        if (in->any_keyup)
            start_game(g, now);
        break;

    case ST_GAMEOVER:
        /* 描画は読むだけなので、爆発の寿命はここでも進める */
        explosions_update(g, now);
        if ((Sint32)(now - g->game_over_timer) > 4000)
            g->state = ST_TITLE;
        break;

    case ST_PLAY:
        if (g->ledger.count == 0 && explosion_count(g) == 0) {
            /* ラウンド間の画面（原典 main の startGame 分岐の前半） */
            if (in->keyup_move)
                g->ship.vx = SHIP_VX;
            if (in->fire_keydown)
                fire(g, now);
            if ((Sint32)(now - g->game_timer) > NEXT_ROUND_DELAY_MS) {
                g->round_num += 1;
                build_blockers(g);
                g->enemy_position = ENEMY_DEFAULT_POSITION;
                reset_round(g, g->score, now);
                g->game_timer += NEXT_ROUND_DELAY_MS;
            }
            break;
        }
        play_music(g, now);
        if (in->keyup_move)
            g->ship.vx = SHIP_VX;
        if (in->fire_keydown)
            fire(g, now);
        fleet_update(g, now);
        if (g->ship_alive)
            ship_update(g, in);
        bullets_update(g, now);
        mystery_update(g, now);
        explosions_update(g, now);
        check_collisions(g, now);
        if (g->state == ST_PLAY) {
            create_new_ship(g, now);
            make_enemies_shoot(g, now);
        }
        break;
    }
    return 1;
}

/* ---- 描画 ---- */

static void draw_tex(Game *g, SDL_Renderer *ren, int tex, int x, int y, int w, int h)
{
    SDL_Rect dst = {x, y, w, h};
    SDL_RenderCopy(ren, g->assets->tex[tex], NULL, &dst);
}

static void draw_blockers(Game *g, SDL_Renderer *ren)
{
    SDL_SetRenderDrawColor(ren, COL_RED.r, COL_RED.g, COL_RED.b, 255);
    for (int base = 0; base < BLOCKER_COUNT; base++)
        for (int r = 0; r < BARRIER_ROWS; r++)
            for (int c = 0; c < BARRIER_COLS; c++) {
                if (!g->blockers[base][r][c]) continue;
                SDL_Rect cell = blocker_cell_rect(base, r, c);
                SDL_RenderFillRect(ren, &cell);
            }
    for (int gi = 0; gi < GROUND_CELLS; gi++) {
        if (!g->ground[gi]) continue;
        SDL_Rect cell = {gi * GROUND_CELL_W, GROUND_Y, GROUND_CELL_W, GROUND_CELL_W};
        SDL_RenderFillRect(ren, &cell);
    }
}

static void draw_hud(Game *g, SDL_Renderer *ren)
{
    char buf[32];
    assets_draw_text(g->assets, ren, 20, "Score", COL_WHITE, 5, 5);
    snprintf(buf, sizeof buf, "%d", g->score);
    assets_draw_text(g->assets, ren, 20, buf, COL_GREEN, 85, 5);
    assets_draw_text(g->assets, ren, 20, "Lives", COL_WHITE, 640, 5);
    for (int i = 0; i < g->lives_spare && i < 3; i++)
        draw_tex(g, ren, TEX_SHIP, 715 + 27 * i, 3, 23, 23);
}

static void draw_explosions(Game *g, SDL_Renderer *ren, Uint32 now)
{
    static const int ship_steps[8] = {
        TEX_SHIPEXPLOSION1, TEX_SHIPEXPLOSION2, TEX_SHIPEXPLOSION3,
        TEX_SHIPEXPLOSION2, TEX_SHIPEXPLOSION1, TEX_SHIPEXPLOSION3,
        TEX_SHIPEXPLOSION2, TEX_SHIPEXPLOSION3,
    };
    for (int i = 0; i < MAX_EXPLOSIONS; i++) {
        Explosion *ex = &g->explosions[i];
        if (!ex->active) continue;
        Uint32 passed = now - ex->born;
        /* 描画は読むだけ。消滅の判定は explosions_update（更新側）にある */
        switch (ex->type) {
        case EX_ENEMY:
            if (passed <= 100)
                draw_tex(g, ren, explosion_texture(ex->row), ex->r.x, ex->r.y, 40, 35);
            else if (passed <= 200)
                draw_tex(g, ren, explosion_texture(ex->row), ex->r.x - 6, ex->r.y - 6, 50, 45);
            break;
        case EX_SHIP: {
            if (passed > 800)
                break;
            int step = (int)(passed / 100);
            if (step > 7) step = 7;
            draw_tex(g, ren, ship_steps[step], ex->r.x, ex->r.y, 50, 48);
            break;
        }
        case EX_BULLET:
            if (passed <= 300)
                draw_tex(g, ren, TEX_ENEMYLASER1, ex->r.x, ex->r.y, 5, 15);
            break;
        case EX_MYSTERY_SCORE:
            if (passed <= 200 || (passed > 400 && passed <= 600)) {
                char buf[16];
                snprintf(buf, sizeof buf, "%d", ex->score);
                assets_draw_text(g->assets, ren, 20, buf, COL_WHITE,
                                 ex->r.x + 20, ex->r.y + 6);
            }
            break;
        }
    }
}

#ifdef __ANDROID__
/* Android 用の発射ボタン（右下の円＋FIRE の文字）。デスクトップでは
 * 描かない（呼び出し側が __ANDROID__ のときだけ有効）。
 * 円の半幅は整数計算で求める（-lm を足さずに済ますため。
 * 描画結果は sqrt を使う C++版と同じ）。 */
static void draw_fire_button(Game *g, SDL_Renderer *ren)
{
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(ren, 237, 28, 36, 150);
    for (int dy = -FIRE_BUTTON_R; dy <= FIRE_BUTTON_R; dy++) {
        int rest = FIRE_BUTTON_R * FIRE_BUTTON_R - dy * dy;
        int half = 0;
        while ((half + 1) * (half + 1) <= rest)
            half++;
        SDL_RenderDrawLine(ren, FIRE_BUTTON_CX - half, FIRE_BUTTON_CY + dy,
                           FIRE_BUTTON_CX + half, FIRE_BUTTON_CY + dy);
    }
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);
    assets_draw_text(g->assets, ren, 20, "FIRE", COL_WHITE,
                     FIRE_BUTTON_CX - 20, FIRE_BUTTON_CY - 10);
}
#endif /* __ANDROID__ */

static void draw_play_scene(Game *g, SDL_Renderer *ren, Uint32 now)
{
    draw_tex(g, ren, TEX_BACKGROUND, 0, 0, SCREEN_W, SCREEN_H);
    draw_blockers(g, ren);
    draw_hud(g, ren);

    if (g->ledger.count == 0 && explosion_count(g) == 0 &&
        g->state == ST_PLAY) {
        /* ラウンド間の画面 */
        char buf[16];
        assets_draw_text(g->assets, ren, 50, "Next Round", COL_WHITE, 240, 270);
        snprintf(buf, sizeof buf, "%d", g->round_num + 2);
        assets_draw_text(g->assets, ren, 50, buf, COL_WHITE, 390, 350);
        return;
    }

    char buf[32];
    snprintf(buf, sizeof buf, "Round %d", g->round_num + 1);
    assets_draw_text(g->assets, ren, 20, buf, COL_WHITE, 325, 5);

    if (g->ship_alive)
        draw_tex(g, ren, TEX_SHIP, g->ship.r.x, g->ship.r.y, SHIP_W, SHIP_H);

    for (int i = 0; i < FLEET_ROWS * FLEET_COLS; i++) {
        Enemy *e = &g->enemies[i];
        if (!e->alive) continue;
        if ((Sint32)(now - e->draw_from) <= 0) continue;
        draw_tex(g, ren, enemy_texture(e->row, e->image_index),
                 e->r.x, e->r.y, 40, 35);
    }

    if (g->mystery.visible)
        draw_tex(g, ren, TEX_MYSTERY, g->mystery.r.x, g->mystery.r.y, 80, 40);

    for (int i = 0; i < MAX_PBULLETS; i++) {
        Bullet *b = &g->pbullets[i];
        if (b->active)
            draw_tex(g, ren, b->flashing ? TEX_LASER1 : TEX_LASER,
                     b->r.x, b->r.y, 5, 15);
    }
    for (int i = 0; i < MAX_EBULLETS; i++) {
        Bullet *b = &g->ebullets[i];
        if (b->active)
            draw_tex(g, ren, b->flashing ? TEX_ENEMYLASER1 : TEX_ENEMYLASER,
                     b->r.x, b->r.y, 5, 15);
    }

    draw_explosions(g, ren, now);

#ifdef __ANDROID__
    if (g->state == ST_PLAY)
        draw_fire_button(g, ren);
#endif
}

void game_render(Game *g, SDL_Renderer *renderer, Uint32 now)
{
    SDL_RenderClear(renderer);
    switch (g->state) {
    case ST_TITLE:
        draw_tex(g, renderer, TEX_BACKGROUND, 0, 0, SCREEN_W, SCREEN_H);
        assets_draw_text(g->assets, renderer, 50, "Space Invaders", COL_WHITE, 164, 155);
        assets_draw_text(g->assets, renderer, 25, "Press any key to continue", COL_WHITE, 201, 225);
        draw_tex(g, renderer, TEX_ENEMY3_1, 318, 270, 40, 40);
        draw_tex(g, renderer, TEX_ENEMY2_2, 318, 320, 40, 40);
        draw_tex(g, renderer, TEX_ENEMY1_2, 318, 370, 40, 40);
        draw_tex(g, renderer, TEX_MYSTERY, 299, 420, 80, 40);
        assets_draw_text(g->assets, renderer, 25, "   =   10 pts", COL_GREEN, 368, 270);
        assets_draw_text(g->assets, renderer, 25, "   =   20 pts", COL_BLUE, 368, 320);
        assets_draw_text(g->assets, renderer, 25, "   =   30 pts", COL_PURPLE, 368, 370);
        assets_draw_text(g->assets, renderer, 25, "   =   ?????", COL_RED, 368, 420);
        break;
    case ST_PLAY:
        draw_play_scene(g, renderer, now);
        break;
    case ST_GAMEOVER: {
        draw_play_scene(g, renderer, now);
        Uint32 passed = now - g->game_over_timer;
        const char *msg = NULL;
        if (passed > 900 && passed < 1000) msg = "G";
        else if (passed > 1000 && passed < 1100) msg = "Ga";
        else if (passed > 1100 && passed < 1200) msg = "Gam";
        else if (passed > 1200 && passed < 1300) msg = "Game";
        else if (passed > 1300 && passed < 1400) msg = "Game O";
        else if (passed > 1500 && passed < 1600) msg = "Game Ov";
        else if (passed > 1600 && passed < 1700) msg = "Game Ove";
        else if (passed > 1700 && passed < 1800) msg = "Game Over";
        if (msg)
            assets_draw_text(g->assets, renderer, 50, msg, COL_RED, 250, 270);
        break;
    }
    }
    SDL_RenderPresent(renderer);
}
