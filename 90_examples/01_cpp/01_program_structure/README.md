# C++実習01：プログラム構造・ヘッダー・翻訳単位

## 目的

最小の`main.cpp`だけで終わらず、宣言をHeader、定義をSource、検証をTestへ分け、CompilerとLinkerがProgramを作る単位を実行して確認します。

## 学ぶ内容

- `main`はProgramの入口である。
- `.h`は複数翻訳単位へ公開する宣言を置く。
- `.cpp`は関数定義を持ち、個別にCompileされる。
- `app_core` LibraryをExecutableとTestがLinkして再利用する。
- 終了Code `0`は成功、非0は失敗を表す。
- CMakeはSource、Include Path、Link関係、Testを記述する。

## File構成

```text
01_program_structure/
├─ CMakeLists.txt
├─ build.ps1
├─ include/Application.h
├─ src/Application.cpp
├─ src/main.cpp
└─ tests/ApplicationTests.cpp
```

## 必要環境

- Windows 10/11 x64
- Visual Studio Community 2026または互換Version
- 「C++によるデスクトップ開発」Workload
- C++20対応MSVC
- Visual Studio付属CMake

## Build・Test・実行

PowerShellでこのDirectoryへ移動し、次を実行します。

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\build.ps1 -Configuration Debug -Run
```

`-ExecutionPolicy Bypass`はこの一回のPowerShell Processだけに適用され、Systemの永続設定は変更しません。ScriptはVisual Studioの場所と付属CMakeを自動検出し、`build/`へSolutionを生成してBuild、CTest、Sample実行まで行います。

Release Buildは次です。

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\build.ps1 -Configuration Release -Run
```

## 期待する実行結果

```text
こんにちは、相棒！ C++実習を開始します。
現在レベル: 3
次のレベルに必要な経験値: 900
Application::Run は正常終了しました。
```

Testでは4件が`PASS`になり、失敗数が0になります。

## CompileとLinkで起きること

1. `Application.cpp`がCompileされ、`app_core`静的LibraryのObject Codeになります。
2. `main.cpp`が別の翻訳単位としてCompileされます。
3. Linkerが`main`から参照された`Application::Run`等の定義を`app_core`から結合します。
4. Testも別ExecutableとしてCompile/Linkされ、同じ`app_core`を検証します。

## よくある失敗

- `Application.h`が見つからない：CMakeのInclude DirectoryまたはBuild手順を確認します。
- 未解決の外部Symbol：宣言に対応する定義、SourceのTarget登録、Library Linkを確認します。
- CMakeが見つからない：`build.ps1`を使うかVisual Studio InstallerでCMake Componentを追加します。
- `.ps1`の実行が無効：README記載の`powershell.exe -ExecutionPolicy Bypass -File`形式で起動します。System Policyの恒久変更は不要です。
- 日本語が文字化けする：MSVCの`/utf-8`、Terminal Font/Encodingを確認します。

`/utf-8`はLibraryだけでなく、`main.cpp`とTestを含む全翻訳単位へ必要です。このSampleのCMakeはDirectory内の全Targetへ同じEncoding Optionを設定します。

## Debug方法

`build/cpp_program_structure.sln`をVisual Studioで開き、`program_structure`をStartup Projectに設定します。`main`、`Application::Run`、`BuildGreeting`へBreakpointを置き、Call StackとLocal変数を確認します。

## 変更課題

1. `Application::Run`が受け取った名前を表示できるよう引数を追加する。
2. 経験値式を別Classへ分離しTestを追加する。
3. Levelが負の場合をErrorとして表す方法を考える。
4. Testを意図的に一件失敗させ、CTestと終了Codeの関係を確認してから元へ戻す。
