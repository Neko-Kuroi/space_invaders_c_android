/*
 * logic.h - 画面に依存しない純ロジック（原典 spaceinvaders.py の写し）
 *
 * 敵の台帳（EnemiesGroup の kill / update_speed / random_bottom）、
 * 得点表（calculate_score）、発射の規則（check_input の分岐）を、
 * SDL に触れず単体テストできる形に切り出したもの。
 */
#ifndef SI_LOGIC_H
#define SI_LOGIC_H

#include <stdbool.h>

#define FLEET_ROWS 5
#define FLEET_COLS 10

typedef struct {
    bool alive[FLEET_ROWS][FLEET_COLS];
    int count;                       /* 生きている敵の総数 */
    bool column_alive[FLEET_COLS];   /* 列に1体でも残っているか */
    int alive_column_count;
    int left_alive_column;           /* 原典 _leftAliveColumn */
    int right_alive_column;          /* 原典 _rightAliveColumn */
    int left_add_move;               /* 原典 leftAddMove */
    int right_add_move;              /* 原典 rightAddMove */
    int min_row;                     /* 原典 minRow */
    int max_row;                     /* 原典 maxRow */
    int move_time;                   /* 敵集団の移動間隔 ms（原典 moveTime） */
    int level;                       /* 敵の発射間隔 ms（原典の全域 LEVEL） */
} FleetLedger;

/* 原典の EnemiesGroup 生成直後の状態にする。move_time=200, level=700 は
 * 原典の初期値（Enemy の生成で LEVEL=700 に戻される癖を含む）。 */
void ledger_init(FleetLedger *l);

/* 敵を1体倒したときの台帳更新。原典 EnemiesGroup.kill() + remove_internal
 * 経由の update_speed() を合わせたもの。既に死んでいるセルでも安全。 */
void ledger_kill(FleetLedger *l, int row, int col);

bool ledger_column_dead(const FleetLedger *l, int col);

/* 原典 random_bottom() の選択部分。
 * pick は [0, alive_column_count) の乱数。生きている列を小さい順に数えて
 * pick 番目の列を選び、その列の一番下（最大の行）の敵を返す。
 * 敵が1体もいなければ false。 */
bool ledger_bottom_enemy(const FleetLedger *l, int pick, int *out_row, int *out_col);

/* 得点表。row は 0..4（ミステリーシップは mystery_score_for_pick で別扱い）。 */
int score_for_row(int row);

/* ミステリーシップの得点 {50, 100, 150, 300}。pick は [0,4) の乱数。 */
int mystery_score_for_pick(int pick);

/* 発射の規則（原典 check_input の分岐そのまま）。
 * 戻り値は撃つ弾のビットマス（VOLLEY_CENTER/LEFT/RIGHT）。
 * sound は鳴らす音: 0=なし, 1=shoot, 2=shoot2。
 * 原典の癖: ラウンド6以上では、弾が残っていても3発が追加で撃たれる。 */
#define VOLLEY_CENTER 1
#define VOLLEY_LEFT   2
#define VOLLEY_RIGHT  4
int weapon_volley(int score, int round_num, bool bullets_empty, bool ship_alive,
                  int *out_sound);

/* 敵集団の波状移動で ROW_counter を1つ進める規則。
 * counter を -1 し、min_row-1 以下になったら max_row に包み直して
 * *wrapped を真にする（moveNumber の加算は呼び出し側の仕事）。
 * 包み直しの条件は、原典の修正（一致判定 == から <= へ）を反映したもの。
 * 一致だけだと、上の行が全滅して min_row が上がったときにカウンタが
 * 条件を飛び越え、敵集団が止まってしまう。 */
int fleet_counter_advance(int counter, int min_row, int max_row, bool *wrapped);

/* トーチカの設計表（原典 make_blockers1 の barrierDesign）。
 * 16行×26列、先頭行は空。1 のセルに 3x3 のブロックが置かれる。
 * 1基あたり318セル（原典で実測）。 */
#define BARRIER_ROWS 16
#define BARRIER_COLS 26
extern const unsigned char BARRIER_DESIGN[BARRIER_ROWS][BARRIER_COLS];

/* 1基あたりのブロック数（原典では4基で 1272 個、地面 800 個と合わせて 2072）。 */
int barrier_cell_count(void);

#endif /* SI_LOGIC_H */
