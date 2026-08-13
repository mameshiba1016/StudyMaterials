# 2Dゲーム完成チェックリスト

この章は知識の一覧ではなく、一本の2Dゲームを配布可能にするための完了条件です。

## Core Loop

- [ ] 起動、通常プレイ、終了が安定する。
- [ ] 最小化、フォーカス喪失、Controller切断から復帰する。
- [ ] Delta上限と固定更新がある。
- [ ] Pause中に進むSystemと止まるSystemが明確。
- [ ] Scene往復で状態やMemoryが漏れない。

## Gameplay

- [ ] KeyboardとGamepadで全必須操作ができる。
- [ ] 入力バッファ、Coyote Time等が仕様化されている。
- [ ] Collision Layer、Hit/Hurt/Bodyが分離される。
- [ ] 高速移動で壁抜けしない。
- [ ] AttackのStartup/Active/Recoveryがデータ化される。
- [ ] Combo、Cancel、Damage順序が再現可能。
- [ ] Death、Respawn、Scene遷移で全状態がResetされる。

## Enemy/Boss

- [ ] 全攻撃に視覚・音のTelegraphがある。
- [ ] 攻撃後に対処可能なRecoveryがある。
- [ ] 画面外から理不尽な攻撃をしない。
- [ ] 複数敵の同時攻撃を制御する。
- [ ] Boss Phase境界と演出中Damage規則がある。
- [ ] AI状態・経路・選択理由をDebug表示できる。

## Visual/Audio

- [ ] Sprite PivotとColliderが全Animationで一致する。
- [ ] 解像度とAspect比が変わっても画面が破綻しない。
- [ ] UIはWorld Camera Shakeを受けない。
- [ ] Particle上限、Draw Call、Overdrawを計測する。
- [ ] BGM/SFX/Voice/UI音量を別設定できる。
- [ ] Voice上限と重要音Priorityがある。

## UI/Accessibility

- [ ] Mouse、Keyboard、GamepadでMenu操作可能。
- [ ] 初期Focus、戻る、Dialog Focus Trapが正しい。
- [ ] Key Rebindと必須Action検証がある。
- [ ] UI Scale、字幕、揺れ・点滅・振動軽減設定がある。
- [ ] 重要情報を色だけで示さない。
- [ ] 長い翻訳文とCJK Fontをテストする。

## Save/Settings

- [ ] Save SchemaにVersionがある。
- [ ] 一時Fileと置換で安全に書き込む。
- [ ] 壊れたSaveを検出しBackupから復旧できる。
- [ ] Auto Save失敗を通知する。
- [ ] 古いVersionの移行テストがある。
- [ ] Settingsは次回起動直後から適用される。

## Performance

- [ ] 対象Hardwareと目標FPSを明記する。
- [ ] CPU/GPU Frame Captureを保存する。
- [ ] P95/P99 Frame TimeとStutterを調査する。
- [ ] Release相当Buildで警告・Sanitizer・Testを実行する。
- [ ] Asset初回使用によるStutterをPrewarmする。
- [ ] 長時間PlayでMemory増加を確認する。

## Debug/Quality

- [ ] Hit Box、Collider、Camera Boundsを可視化できる。
- [ ] 状態、tick、入力、FPS、Entity数を表示できる。
- [ ] Crash LogとBuild識別子がある。
- [ ] 主要Ruleに単体・統合・回帰Testがある。
- [ ] 固定seedと入力記録でBugを再現できる。
- [ ] Third-party LicenseとAsset権利を記載する。

## Portfolio

- [ ] READMEに概要、操作、Build方法、対応環境を書く。
- [ ] 自作範囲と外部Library/Assetを明記する。
- [ ] Architecture図と主要Systemの説明がある。
- [ ] 難しかった問題、候補案、選択理由、結果を書く。
- [ ] ProfilerのBefore/Afterを載せる。
- [ ] 1～3分のGameplay/技術解説動画を用意する。
- [ ] 初見の採用担当者が短時間で遊べるBuildを用意する。

## 初稿後の学習方法

このノート群を読むだけでは実装力になりません。`90_examples`で一概念一`.cpp`の小例を作り、Pong、Breakout、Shooting、Platformer、戦闘Prototypeの順に統合します。各作品で設計、実装、Test、計測、振り返りを残します。
