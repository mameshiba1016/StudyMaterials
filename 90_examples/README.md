# 90 実行可能な実習編

知識ノートの整備完了後、各ノート項目に対応する「そのままBuild・実行して挙動を確認できるSample」を一項目ずつ追加します。巨大な一Projectへ詰め込まず、概念ごとに独立して実行・変更・破棄できる構成にします。

## 実習編の原則

- 標準C++は、必要に応じて`.h`と`.cpp`へ責務を分割します。
- 各Sampleへ`README.md`、Source、Build設定、実行手順、期待結果を付けます。
- コードには「何をするか」だけでなく、呼出し順序、Memory、寿命、所有権、失敗条件をコメントします。
- 初心者向けの最小実装と、実務的に整理した実装の違いを説明します。
- Sample単体で完結させ、別Sampleの隠れたFileや設定へ依存させません。
- 外部SDK、Asset、Engine Version、Package、Pluginが必要なら冒頭に明記します。
- 実行時の操作方法、正常時の画面・Console出力、終了方法を明記します。
- Build確認、起動確認、主要処理のTestを行ってから追加します。
- 意図的な失敗例は正常版と分離し、危険な未定義動作を通常実行させません。
- Originalの図形・仮Assetを使い、特定作品のDataを複製しません。

## 予定ディレクトリ

```text
90_examples/
├─ 01_cpp/                 標準C++、CMake、Console Test
├─ 02_2d_game/             2D Loop、入力、描画、衝突、Action
├─ 03_3d_game/             数学、Camera、Model、Animation、Collision
├─ 04_game_ai/             FSM、A*、Behavior Tree、Utility AI
├─ 05_unreal_engine/       `.uproject`とUnreal C++ Module
├─ 06_unity/               Unity Project、C# Script、Scene構築手順
├─ 07_dxlib/               Visual Studio/CMake対応DxLib Sample
├─ 08_directx11/           Win32＋D3D11の独立Sample
├─ 09_directx12/           Win32＋D3D12の独立Sample
├─ 10_engine_architecture/ ECS、Resource、Scene、Event、Job
└─ 11_debug_performance/   Test、Profiler、最適化比較
```

## 各Sampleの必須構成

```text
sample_name/
├─ README.md          目的、前提、Build、実行、期待結果、課題
├─ CMakeLists.txt     CMakeを使うSampleの場合
├─ include/           `.h`／`.hpp`
├─ src/               `.cpp`
├─ assets/            再配布可能な最小Asset
└─ tests/             自動Test可能な処理
```

Engine固有Projectでは、そのEngineが要求する標準構成を優先します。

## 基準環境

- 標準C++：Windows x64、Visual Studio、C++20、CMake
- DirectX：Windows SDK、Win32、DirectX 11／12
- DxLib：導入PathをBuild設定で指定可能にする
- Unreal Engine：実習開始時に導入済みEngine Versionを固定する
- Unity：実習開始時に導入済みEditor/LTS VersionとPackage Versionを固定する

## 一項目の完成条件

- [ ] CleanなBuild DirectoryからBuildできる。
- [ ] READMEのCommandまたはIDE手順だけで起動できる。
- [ ] 正常時の期待結果を確認できる。
- [ ] Sourceの主要処理へ詳細コメントがある。
- [ ] `.h`と`.cpp`の役割を説明している。
- [ ] Debug方法とよくある失敗を記載している。
- [ ] 変更課題を最低一つ用意している。
- [ ] 別Sampleへ暗黙依存していない。

現段階では知識ノートを先に完成させ、その後この順序で実習編を制作します。

## 制作済みSample

- C++実習01：[プログラム構造・ヘッダー・翻訳単位](01_cpp/01_program_structure/README.md)
- C++実習02：[コンパイル・静的ライブラリ・リンク](01_cpp/02_compile_and_link/README.md)
- C++実習03：[型・変数・初期化](01_cpp/03_types_variables_initialization/README.md)
- C++実習04：[コメントの種類と処理意図](01_cpp/04_comments_and_intent/README.md)
- C++実習05：[演算子](01_cpp/05_operators/README.md)
- C++実習06：[条件分岐](01_cpp/06_conditionals/README.md)
- C++実習07：[反復処理](01_cpp/07_loops/README.md)
- C++実習08：[関数](01_cpp/08_functions/README.md)
- C++実習09：[スコープ・記憶域期間・寿命](01_cpp/09_scope_storage_lifetime/README.md)
- C++実習10：[配列](01_cpp/10_arrays/README.md)
- C++実習11：[文字列とエンコーディング](01_cpp/11_strings_encoding/README.md)
