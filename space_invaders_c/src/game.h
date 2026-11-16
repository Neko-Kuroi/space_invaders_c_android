/*
 * game.h - ゲームの状態と進行（原典 SpaceInvaders クラスの写し）
 *
 * 原典の全域 game と各スプライトの相互参照は、Game 構造体にまとめて
 * ポインタで渡す形に置き換えた（計画書の第4節）。
 * 入力は Input 構造体に分離してあるので、main.c は SDL イベントから
 * Input を作るだけになり、スモークテストでは入力を合成できる。
 */
#ifndef SI_GAME_H
#define SI_GAME_H

#include <SDL.h>
#include <stdbool.h>
#include "assets.h"
#include "logic.h"

#define SCREEN_W 800
#define SCREEN_H 600

/* Android 用の発射ボタン（画面右下の円）。main.c のタッチ判定と
 * game.c の描画で共有する（C++版と同値）。 */
#define FIRE_BUTTON_CX 738
#define FIRE_BUTTON_CY 548
#define FIRE_BUTTON_R 40

#define SHIP_VX 10
#define ENEMY_DEFAULT_POSITION 65
#define ENEMY_MOVE_DOWN 35
#define BLOCKERS_POSITION 450

/* 自機 */
#define SHIP_START_X 375
#define SHIP_START_Y 540
#define SHIP_W 50
#define SHIP_H 50
#define SHIP_RESPAWN_MS 1600        /* 被弾から再出撃まで */

/* 進行の時間 */
#define NEXT_ROUND_DELAY_MS 3000    /* 全滅から次のラウンドまで */
#define MYSTERY_INTERVAL_MS 25000   /* ミステリーシップの出現周期 */

/* 敵集団の到達ライン（fleet_bottom との比較） */
#define FLEET_REACH_SHIP_Y SHIP_START_Y  /* 自機の高さ。重なれば自機が消える */
#define FLEET_GAMEOVER_Y 600             /* ここまで来たらゲームオーバー */

/* トーチカ（4基。1セル 3x3、BARRIER_COLS x BARRIER_ROWS のセル） */
#define BLOCKER_COUNT 4
#define BLOCKER_X0 60
#define BLOCKER_PITCH 200
#define BLOCKER_Y_OFFSET 25         /* BLOCKERS_POSITION からの下がり */
#define BLOCKER_CELL 3

/* 地面（y=GROUND_Y に 4x4 のセルを横一列） */
#define GROUND_Y 595
#define GROUND_CELLS 200
#define GROUND_CELL_W 4

/* ミステリーシップ出現音の専用チャンネル。main.c が起動時に
 * Mix_ReserveChannels(1) で予約する。通常の効果音（-1 指定）は
 * このチャンネルを使わないので、後からのフェードアウトや停止が
 * 別の音にかかることはない。 */
#define MYSTERY_CHANNEL 0

#define MAX_PBULLETS 24
#define MAX_EBULLETS 64
#define MAX_EXPLOSIONS 80

typedef enum { ST_TITLE, ST_PLAY, ST_GAMEOVER } GameState;

/* 1フレーム分の入力。main.c が SDL イベントとキー状態から作る。 */
typedef struct {
    bool quit;          /* ウィンドウを閉じる / ESC の KEYUP（原典 should_exit） */
    bool any_keyup;     /* タイトルで何かキーを離した（原典の開始条件） */
    bool fire_keydown;  /* S の KEYDOWN（発射はイベント駆動） */
    bool keyup_move;    /* A か D の KEYUP（vx を SHIP_VX に戻す） */
    bool held_a;
    bool held_d;
} Input;

typedef struct {
    SDL_Rect r;         /* 50x50, 初期 (375, 540) */
    int vx;
} Ship;

typedef struct {
    SDL_Rect r;         /* 弾の画像は 5x15 */
    int direction;      /* -1 上 / +1 下 */
    int speed;
    Uint32 born;
    bool active;
    bool enemy;         /* 敵弾か */
    bool flashing;      /* 端で image2 を出している間 */
} Bullet;

typedef struct {
    SDL_Rect r;         /* 40x35 */
    int row, col;
    int image_index;    /* アニメーション 0/1 */
    Uint32 draw_from;   /* この時刻までは描かない（原典の timeOffset の癖） */
    bool alive;
} Enemy;

typedef enum { EX_ENEMY, EX_SHIP, EX_BULLET, EX_MYSTERY_SCORE } ExplosionType;

typedef struct {
    ExplosionType type;
    SDL_Rect r;
    int row;            /* EX_ENEMY: 色の決定に使う */
    int score;          /* EX_MYSTERY_SCORE: 表示する得点 */
    Uint32 born;
    bool active;
} Explosion;

typedef struct {
    SDL_Rect r;         /* 80x40 */
    int direction;
    Uint32 timer;
    bool play_sound;
    bool visible;       /* このフレームで動いた＝描く（原典の blit 条件） */
    int channel;        /* mysteryentered の再生チャンネル */
} Mystery;

typedef struct {
    GameState state;
    Assets *assets;

    int score;
    int round_num;          /* 原典の全域 ROUND_NUM */
    int enemy_position;     /* 原典 enemyPosition */
    int lives_spare;        /* 残機アイコンの数（初期3、被弾で減る） */

    Ship ship;
    bool ship_alive;
    bool make_new_ship;
    Uint32 ship_timer;

    Enemy enemies[FLEET_ROWS * FLEET_COLS];
    FleetLedger ledger;
    int fleet_direction;
    int move_number;        /* 初期値 15（原典の癖） */
    int left_moves, right_moves;
    int row_counter;        /* 原典の全域 ROW_counter、初期値 4 */
    Uint32 fleet_timer;
    int fleet_bottom;

    Bullet pbullets[MAX_PBULLETS];
    Bullet ebullets[MAX_EBULLETS];
    Mystery mystery;
    Explosion explosions[MAX_EXPLOSIONS];

    bool blockers[BLOCKER_COUNT][BARRIER_ROWS][BARRIER_COLS];
    bool ground[GROUND_CELLS]; /* 地面（1セル GROUND_CELL_W px） */

    Uint32 shoot_timer;     /* 原典 self.timer（敵の発射間隔の基準） */
    Uint32 note_timer;
    int note_index;
    Uint32 game_timer;      /* 原典 gameTimer（最後の撃墜時刻、遷移の基準） */
    Uint32 game_over_timer; /* 原典 gameOverTimer（F-5 の専用化を踏襲） */
} Game;

void game_init(Game *g, Assets *assets);
/* 1フレーム分の更新。now は SDL_GetTicks()。戻り値 0 で終了。 */
int game_update(Game *g, const Input *in, Uint32 now);
/* 現在の状態を描く。now は game_update に渡したものと同じ時刻を使う
 * （更新と描画で時刻がずれないようにするため）。 */
void game_render(Game *g, SDL_Renderer *renderer, Uint32 now);

#endif /* SI_GAME_H */
