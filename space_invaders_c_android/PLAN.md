# Space Invaders Android 移植 (C版ベース) 計画書

状態: 実装完了 (2026-11-16、報告書は IMPLEMENTATION_REPORT.md)
- 土台: C版 space_invaders_c (リポジトリ最新 958c88f = review2 適用済み、
  手元フォルダは同期済み)
- パッケージ名: C++版とは別名にする (Satoru の決定)。
  `net.example.spaceinvaders.c` とし、端末で C++版と並べて比べられるようにする。
  画面の名前も区別のため「Space Invaders (C)」とする
- タッチ操作・発射ボタン・横画面などの仕様は C++版の Android 移植と同一

## 1. 目的

C++版相当の Android 対応を、C版のコードを土台に作る。
ゲームの規則は C版のまま変えず、Android 用の違いは
`#ifdef __ANDROID__` とプロジェクト側の設定に閉じ込める
(C++版の移植と同じ方針)。

## 2. C版ソースへの変更 (場所は ~/workspace/space_invaders_c/src)

1. main.c: build_input にレンダラーを渡し、Android ではタッチを
   Input に変換する (左半分/右半分で移動、指ごとに役割を固定、
   指を離したら開始、発射ボタン範囲は発射、戻るボタン=終了)。
   素材の基準パスは Android では空文字
2. game.c: draw_play_scene の末尾に、Android のときだけ
   発射ボタン (右下の円+FIRE の文字) を描く。C には -lm を
   足さずに済むよう、円の幅は整数計算で求める (描画結果は同じ)
3. game.h: 発射ボタンの定数 (FIRE_BUTTON_CX/CY/R、C++版と同値)

デスクトップ版の動作は変えない。確認は、警告なしビルド、
logic テスト 67 件、スモーク、Android 用分岐の構文チェックで行う。

## 3. プロジェクト (場所は ~/workspace/space_invaders_c_android/project)

C++版の Android プロジェクトと同じ組み立て (SDL 公式テンプレート、
ndk-build、SDL のソース同梱、素材は assets へ複写)。
違いは次の点だけ:

- ゲームは C 言語のみなので、APP_STL などの C++ 用設定は置かない
- ゲームのソースは複写せず、C版フォルダの src を参照する
- applicationId は net.example.spaceinvaders.c

## 4. 検証の範囲

- こちらでできること: APK のビルド成功、中身 (ライブラリ・素材・
  パッケージ名・横画面・署名) の確認、デスクトップ版の回帰
- こちらではできないこと: 実機での起動・タッチ・音 (Satoru 側)。
  C++版 APK のインストールで止まっている件の原因が未確定のため、
  端末側の事情によっては同じ所で止まる可能性がある

## 5. 成果物

- ~/workspace/space_invaders_c_android/app-debug.apk
- ~/workspace/space_invaders_c_android/project/ (プロジェクト一式)
- C版側の変更分パッチ (リポジトリのツリー向け、別途作成)
- 報告書 IMPLEMENTATION_REPORT.md
