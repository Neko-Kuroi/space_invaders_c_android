# Space Invaders Android 移植 (C版ベース) 実施報告書

実施日: 2026-11-16
計画書: PLAN.md（決定事項はそちらに記録）

## 1. 成果物

- `app-debug.apk`（3,175,087 バイト、MD5 431add48161da47e149becf609b4094a）
  - パッケージ `net.example.spaceinvaders.c`（C++版とは別名。端末で並べて比べられる）
  - 画面の名前「Space Invaders (C)」
  - minSdk 21 / targetSdk 34 / arm64-v8a のみ / 横画面
  - 中身: libmain.so ＋ SDL 4ライブラリ、素材31件、デバッグ署名（apksigner で確認）
  - zipalign の確認も合格（`zipalign -c 4` 成功）
- `project/`（SDL 公式テンプレート＋ndk-build、SDL のソース同梱）
- C版側の変更パッチ `~/workspace/space_invaders_c_android.patch`（219行、3ファイル）

## 2. C版ソースへの変更（`~/workspace/space_invaders_c/src`、全て `#ifdef __ANDROID__` 内）

- `main.c`: build_input がレンダラーを受け取り、タッチを Input に変換する
  （左半分/右半分で移動、指ごとに役割を固定、指を離したら開始、
  発射ボタン範囲は発射、戻るボタン＝終了）。素材の基準パスは空文字
- `game.c`: draw_play_scene の末尾で、プレイ中だけ発射ボタン
  （右下の円＋FIRE の文字）を描く。円の半幅は整数計算
  （Makefile に -lm を足さないため。描画結果は C++版と同じ）
- `game.h`: 発射ボタンの定数 FIRE_BUTTON_CX/CY/R（C++版と同値）

## 3. プロジェクトの C++版との違い

- ゲームは C 言語のみなので Application.mk に STL の設定を置かない
- Android.mk の参照先は C版フォルダの src（複写せず参照、版ずれ防止）
- 素材は C版フォルダから複写（31件、diff で同一を確認）

## 4. 検証（こちらで実施）

- デスクトップ回帰: 警告なしビルド（-Wall -Wextra -Werror）、
  logic テスト 67/67、スモーク 600／7,200 フレーム正常、
  音声なし起動でも完走、Android 用分岐の構文チェック合格
- パッチ: まっさらな C リポジトリへの `git apply` と `patch -p1` の
  両経路で、現行フォルダと完全一致を確認
- APK: ビルド成功（Gradle 8.1.1 / AGP 8.1.1 / NDK 26.1.10909125）、
  中身・署名・パッケージ名・横画面を aapt / apksigner / zipalign で確認

## 5. 未確認（Satoru 側）

- 実機でのインストール・起動・タッチ・音。
  C++版 APK のインストールで止まっている件の原因が未確定のため、
  端末側の事情によっては、この APK も同じ所で止まる可能性がある
