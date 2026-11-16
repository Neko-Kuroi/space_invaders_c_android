# Space Invaders C 版（SDL2）
# 素材（images/ sounds/ fonts/）は実行ファイルと同じ場所から読む。

CC      ?= gcc
CFLAGS  ?= -std=c11 -O2 -Wall -Wextra
SDL_CFLAGS := $(shell pkg-config --cflags sdl2 SDL2_image SDL2_mixer SDL2_ttf)
SDL_LIBS   := $(shell pkg-config --libs sdl2 SDL2_image SDL2_mixer SDL2_ttf)

SRC := src/main.c src/game.c src/assets.c src/logic.c
OBJ := $(SRC:.c=.o)

all: space_invaders_c

space_invaders_c: $(OBJ)
	$(CC) $(CFLAGS) -o $@ $(OBJ) $(SDL_LIBS)

%.o: %.c
	$(CC) $(CFLAGS) $(SDL_CFLAGS) -Isrc -c -o $@ $<

test: tests/test_logic
	./tests/test_logic

tests/test_logic: tests/test_logic.c src/logic.c src/logic.h
	$(CC) $(CFLAGS) -Isrc -o $@ tests/test_logic.c src/logic.c

clean:
	rm -f $(OBJ) space_invaders_c tests/test_logic

.PHONY: all test clean
