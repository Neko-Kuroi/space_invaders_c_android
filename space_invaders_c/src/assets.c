/*
 * assets.c
 */
#include "assets.h"

#include <stdio.h>
#include <string.h>
#include <SDL_image.h>

static const char *const TEX_FILES[TEX_COUNT] = {
    [TEX_SHIP] = "images/ship.png",
    [TEX_SHIPEXPLOSION1] = "images/shipexplosion1.png",
    [TEX_SHIPEXPLOSION2] = "images/shipexplosion2.png",
    [TEX_SHIPEXPLOSION3] = "images/shipexplosion3.png",
    [TEX_MYSTERY] = "images/mystery.png",
    [TEX_ENEMY1_1] = "images/enemy1_1.png",
    [TEX_ENEMY1_2] = "images/enemy1_2.png",
    [TEX_ENEMY2_1] = "images/enemy2_1.png",
    [TEX_ENEMY2_2] = "images/enemy2_2.png",
    [TEX_ENEMY3_1] = "images/enemy3_1.png",
    [TEX_ENEMY3_2] = "images/enemy3_2.png",
    [TEX_LASER] = "images/laser.png",
    [TEX_LASER1] = "images/laser1.png",
    [TEX_LASER2] = "images/laser2.png",
    [TEX_ENEMYLASER] = "images/enemylaser.png",
    [TEX_ENEMYLASER1] = "images/enemylaser1.png",
    [TEX_EXPLOSIONBLUE] = "images/explosionblue.png",
    [TEX_EXPLOSIONGREEN] = "images/explosiongreen.png",
    [TEX_EXPLOSIONPURPLE] = "images/explosionpurple.png",
    [TEX_BACKGROUND] = "images/background.jpg",
};

static const char *const SND_FILES[SND_COUNT] = {
    [SND_SHOOT] = "sounds/shoot.wav",
    [SND_SHOOT2] = "sounds/shoot2.wav",
    [SND_INVADERKILLED] = "sounds/invaderkilled.wav",
    [SND_MYSTERYKILLED] = "sounds/mysterykilled.wav",
    [SND_SHIPEXPLOSION] = "sounds/shipexplosion.wav",
    [SND_MYSTERYENTERED] = "sounds/mysteryentered.wav",
    [SND_NOTE0] = "sounds/0.wav",
    [SND_NOTE1] = "sounds/1.wav",
    [SND_NOTE2] = "sounds/2.wav",
    [SND_NOTE3] = "sounds/3.wav",
};

static void join_path(char *dst, size_t cap, const char *base, const char *rel)
{
    snprintf(dst, cap, "%s%s", base, rel);
}

int assets_load(Assets *a, SDL_Renderer *renderer, const char *base_path,
                int load_sounds)
{
    char path[1024];
    memset(a, 0, sizeof *a);

    for (int i = 0; i < TEX_COUNT; i++) {
        join_path(path, sizeof path, base_path, TEX_FILES[i]);
        SDL_Surface *surf = IMG_Load(path);
        if (!surf) {
            fprintf(stderr, "image load failed: %s: %s\n", path, IMG_GetError());
            assets_free(a);
            return 0;
        }
        a->tex[i] = SDL_CreateTextureFromSurface(renderer, surf);
        SDL_FreeSurface(surf);
        if (!a->tex[i]) {
            fprintf(stderr, "texture create failed: %s\n", path);
            assets_free(a);
            return 0;
        }
    }
    for (int i = 0; load_sounds && i < SND_COUNT; i++) {
        join_path(path, sizeof path, base_path, SND_FILES[i]);
        a->snd[i] = Mix_LoadWAV(path);
        if (!a->snd[i]) {
            fprintf(stderr, "sound load failed: %s: %s\n", path, Mix_GetError());
            assets_free(a);
            return 0;
        }
        /* 原典は全ての音を set_volume(0.1) している */
        Mix_VolumeChunk(a->snd[i], (int)(MIX_MAX_VOLUME * 0.1));
    }

    join_path(path, sizeof path, base_path, "fonts/space_invaders.ttf");
    a->font20 = TTF_OpenFont(path, 20);
    a->font25 = TTF_OpenFont(path, 25);
    a->font50 = TTF_OpenFont(path, 50);
    if (!a->font20 || !a->font25 || !a->font50) {
        fprintf(stderr, "font load failed: %s: %s\n", path, TTF_GetError());
        assets_free(a);
        return 0;
    }
    return 1;
}

void assets_free(Assets *a)
{
    for (int i = 0; i < TEX_COUNT; i++)
        if (a->tex[i]) SDL_DestroyTexture(a->tex[i]);
    for (int i = 0; i < SND_COUNT; i++)
        if (a->snd[i]) Mix_FreeChunk(a->snd[i]);
    if (a->font20) TTF_CloseFont(a->font20);
    if (a->font25) TTF_CloseFont(a->font25);
    if (a->font50) TTF_CloseFont(a->font50);
    memset(a, 0, sizeof *a);
}

void assets_draw_text(Assets *a, SDL_Renderer *renderer, int size,
                      const char *msg, SDL_Color color, int x, int y)
{
    TTF_Font *font = (size == 50) ? a->font50 : (size == 25) ? a->font25 : a->font20;
    SDL_Surface *surf = TTF_RenderUTF8_Blended(font, msg, color);
    if (!surf)
        return;
    SDL_Texture *tex = SDL_CreateTextureFromSurface(renderer, surf);
    if (tex) {
        SDL_Rect dst = {x, y, surf->w, surf->h};
        SDL_RenderCopy(renderer, tex, NULL, &dst);
        SDL_DestroyTexture(tex);
    }
    SDL_FreeSurface(surf);
}

int assets_play(Assets *a, int snd)
{
    if (snd < 0 || snd >= SND_COUNT || !a->snd[snd])
        return -1;
    return Mix_PlayChannel(-1, a->snd[snd], 0);
}

int assets_play_on(Assets *a, int snd, int channel)
{
    if (snd < 0 || snd >= SND_COUNT || !a->snd[snd])
        return -1;
    return Mix_PlayChannel(channel, a->snd[snd], 0);
}
