/*
 * assets.h - 画像・音・フォントの読み込みと解放。
 * 素材は原典リポジトリのものをそのまま使う（プロジェクト直下の
 * images/ sounds/ fonts/ を、実行ファイルの場所から相対で読む）。
 */
#ifndef SI_ASSETS_H
#define SI_ASSETS_H

#include <SDL.h>
#include <SDL_mixer.h>
#include <SDL_ttf.h>

/* 画像の番号。原典の IMAGES 辞書のキーに対応する。 */
enum {
    TEX_SHIP,
    TEX_SHIPEXPLOSION1, TEX_SHIPEXPLOSION2, TEX_SHIPEXPLOSION3,
    TEX_MYSTERY,
    TEX_ENEMY1_1, TEX_ENEMY1_2, TEX_ENEMY2_1, TEX_ENEMY2_2,
    TEX_ENEMY3_1, TEX_ENEMY3_2,
    TEX_LASER, TEX_LASER1, TEX_LASER2,
    TEX_ENEMYLASER, TEX_ENEMYLASER1,
    TEX_EXPLOSIONBLUE, TEX_EXPLOSIONGREEN, TEX_EXPLOSIONPURPLE,
    TEX_BACKGROUND,
    TEX_COUNT
};

/* 音の番号。原典の sounds 辞書と musicNotes、Mystery の音に対応する。 */
enum {
    SND_SHOOT, SND_SHOOT2, SND_INVADERKILLED, SND_MYSTERYKILLED,
    SND_SHIPEXPLOSION, SND_MYSTERYENTERED,
    SND_NOTE0, SND_NOTE1, SND_NOTE2, SND_NOTE3,
    SND_COUNT
};

typedef struct {
    SDL_Texture *tex[TEX_COUNT];
    Mix_Chunk *snd[SND_COUNT];
    TTF_Font *font20;
    TTF_Font *font25;
    TTF_Font *font50;
} Assets;

/* base_path は素材フォルダのあるディレクトリ（末尾に '/' を含む想定）。
 * load_sounds が 0 のときは音を読まない（音声デバイスが使えない環境用）。
 * その場合 snd[] は NULL のままで、assets_play 系は何もせず -1 を返す。
 * 失敗したら stderr に理由を出し、読み込み済みの分を解放して 0 を返す。 */
int assets_load(Assets *a, SDL_Renderer *renderer, const char *base_path,
                int load_sounds);
void assets_free(Assets *a);

/* 文字をその場で描く（原典の Text.draw 相当）。size は 20/25/50 のみ。 */
void assets_draw_text(Assets *a, SDL_Renderer *renderer, int size,
                      const char *msg, SDL_Color color, int x, int y);

/* 音量は原典どおり全て 0.1。channel を返す（-1 は失敗）。 */
int assets_play(Assets *a, int snd);

/* チャンネルを指定して再生する。ミステリーシップの出現音のように、
 * 後からフェードアウトや停止をかける音は、Mix_ReserveChannels で
 * 予約した専用チャンネルで鳴らす（番号の使い回しで別の音を切らない）。 */
int assets_play_on(Assets *a, int snd, int channel);

#endif /* SI_ASSETS_H */
