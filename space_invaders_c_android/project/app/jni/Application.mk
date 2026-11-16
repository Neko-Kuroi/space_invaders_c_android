# Space Invaders (Android, C版) - ndk-build の全体設定
# 対象は arm64 のみ、API 21 以上（PLAN.md の決定どおり）

APP_ABI := arm64-v8a
APP_PLATFORM := android-21

# ゲーム本体は C 言語のみなので、C++ 用の STL 設定は置かない

# SDL2_mixer は wavpack と gme を外部モジュールとして要求するが、
# このゲームで使うのは WAV のみなので無効化する
SUPPORT_WAVPACK := false
SUPPORT_GME := false
