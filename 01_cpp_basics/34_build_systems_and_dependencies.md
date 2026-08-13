# ビルドシステム・コンパイラ設定・依存ライブラリ

ソースコードを実行ファイルへするには、対象ファイル、includeパス、ライブラリ、コンパイルオプション、構成、プラットフォームを再現可能に定義する必要があります。

## コンパイラ・ビルドシステム・IDE

- コンパイラ：C++をオブジェクトコードへ変換する。MSVC、Clang、GCCなど。
- リンカー：オブジェクトとライブラリを結合する。
- ビルドシステム：どのコマンドをどの順で実行するか管理する。Ninja、MSBuild、Makeなど。
- メタビルド：ビルド定義から各ビルドシステム用ファイルを生成する。CMakeなど。
- IDE：編集、デバッグ、プロジェクト操作を統合する。Visual Studio等。

Visual StudioとMSVC、CMakeとコンパイラを同じものとして扱わないことが重要です。

## 最小CMake例

```cmake
cmake_minimum_required(VERSION 3.25)
project(StudyExample LANGUAGES CXX)

add_executable(StudyExample
    src/main.cpp
    src/Player.cpp
)

target_compile_features(StudyExample PRIVATE cxx_std_20)
target_include_directories(StudyExample PRIVATE include)
```

グローバルなフラグやinclude設定より、`target_*`で各ターゲットの使用要件を表します。依存ライブラリは`target_link_libraries`で`PRIVATE`、`PUBLIC`、`INTERFACE`の伝播範囲を指定します。

## DebugとRelease

- Debug：デバッグ情報、低い最適化、追加検査など。実行速度と挙動がReleaseと異なる。
- Release：最適化、有効なインライン展開、assert無効化など。

Debugだけで正しくても完成ではありません。未初期化メモリ、未定義動作、タイミング依存はReleaseでだけ現れる場合があります。RelWithDebInfoのように最適化とデバッグ情報を両立した構成も使います。

## 警告

高い警告レベルを有効にし、自作コードでは警告を原則修正します。外部ライブラリの警告はシステムヘッダー扱いなどで分離します。警告を一括無効化すると、符号変換、シャドーイング、未使用値、危険な寿命などの手掛かりを失います。

警告はC++規格違反とは限らず、コンパイラごとに異なります。複数コンパイラでCIビルドすると移植性の問題を早期発見できます。

## Sanitizer

- AddressSanitizer：境界外、use-after-freeなど。
- UndefinedBehaviorSanitizer：一部の未定義動作。
- ThreadSanitizer：データ競合（環境対応を確認）。
- MemorySanitizer：未初期化読み取り（主に対応Clang環境）。

すべての問題を検出する保証はありませんが、通常テストと組み合わせると強力です。

## 静的・動的ライブラリ

- 静的ライブラリ：リンク時に必要コードを取り込む。
- 動的ライブラリ：実行時にDLL等をロードする。
- ヘッダーオンリー：利用側でテンプレート等をコンパイルする。

配布サイズ、更新、ABI、ライセンス、デバッグシンボル、ロード失敗を考慮します。

## 依存関係管理

vcpkg、Conan、Git submodule、FetchContent、手動vendorなどがあります。重要なのは次です。

- バージョンまたはコミットを固定し再現可能にする。
- ライセンスとNOTICE義務を確認する。
- 推移的依存と脆弱性を把握する。
- ハッシュ・署名・公式配布元を確認する。
- 更新手順と互換性テストを用意する。
- ビルド済みバイナリのコンパイラ、ランタイム、構成を合わせる。

## インクリメンタルビルドとPCH

ヘッダー変更は、それをincludeする多数の翻訳単位を再コンパイルさせます。前方宣言、PImpl、安定したインターフェース、Precompiled Header、Unity Build、Modulesなどがビルド時間へ影響します。Unity BuildはODR違反や名前衝突を露呈・隠蔽することもあり、通常ビルドもCIで維持します。

## CI

クリーン環境でconfigure、build、test、静的解析、成果物生成を自動化します。「自分のPCに偶然あるSDK・環境変数・絶対パス」への依存を検出できます。学習用プロジェクトでも、README記載の一手順でビルドできる状態は大きな品質証拠になります。

## Unreal Build Tool

Unreal EngineプロジェクトはUBT、ModuleRules、TargetRules、UHT、Build.csなど独自のビルドパイプラインを使います。一般CMakeプロジェクトの設定をそのまま適用せず、モジュール依存、Public/Private include、Editor/Runtimeターゲットを専用章で扱います。
