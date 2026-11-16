LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := main

SDL_PATH := ../SDL

# ゲーム本体（C版）のソースは、版のずれを防ぐため複写せず参照する
# （PLAN.md 第2節）。このファイルの場所から 5 つ上が workspace 直下。
GAME_PATH := $(abspath $(LOCAL_PATH)/../../../../../space_invaders_c/src)

LOCAL_C_INCLUDES := $(LOCAL_PATH)/$(SDL_PATH)/include \
    $(LOCAL_PATH)/../SDL_image \
    $(LOCAL_PATH)/../SDL_mixer \
    $(LOCAL_PATH)/../SDL_ttf \
    $(GAME_PATH)

LOCAL_SRC_FILES := $(GAME_PATH)/main.c \
    $(GAME_PATH)/game.c \
    $(GAME_PATH)/assets.c \
    $(GAME_PATH)/logic.c

LOCAL_SHARED_LIBRARIES := SDL2 SDL2_image SDL2_mixer SDL2_ttf

LOCAL_LDLIBS := -lGLESv1_CM -lGLESv2 -lOpenSLES -llog -landroid

include $(BUILD_SHARED_LIBRARY)
