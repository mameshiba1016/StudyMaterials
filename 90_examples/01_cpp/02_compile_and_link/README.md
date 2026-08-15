# C++実習02：コンパイル・静的ライブラリ・リンク

## 目的

複数の`.cpp`が別々にCompileされ、静的LibraryとExecutableがLinkされる流れを、戦闘Damage計算の小さなProgramで確認します。

## 生成物

```text
CombatMath.cpp -> CombatMath.obj -> combat_math.lib
main.cpp       -> main.obj -------+-> compile_and_link.exe
Tests.cpp      -> Tests.obj ------+-> compile_and_link_tests.exe
```

`.obj`は翻訳単位の機械語、`.lib`はObject Codeをまとめた静的Library、`.exe`はLink済み実行Fileです。

## File構成

```text
02_compile_and_link/
├─ CMakeLists.txt
├─ build.ps1
├─ include/CombatMath.h
├─ src/CombatMath.cpp
├─ src/main.cpp
└─ tests/CombatMathTests.cpp
```

## Visual StudioでBuild・Test・実行

Visual Studio Communityと「C++によるデスクトップ開発」を導入し、PowerShellで次を実行します。

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\build.ps1 -Configuration Debug -Run
```

Release版は次です。

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\build.ps1 -Configuration Release -Run
```

生成された`build/cpp_compile_and_link.sln`をVisual Studioで開いてDebug実行することもできます。

## 期待結果

Debugの場合は次の内容が表示されます。

```text
Build構成: Debug
通常Damage: 85
Critical Bonus: 60
Critical Damage: 145
combat_math.libとのLinkに成功しました。
```

CTestはTest Executableを一件実行し、その内部で7条件を検証します。

## Compile ErrorとLink Errorの違い

- 宣言されていない関数、型違い、構文違反はCompile段階で失敗します。
- Headerに宣言はあるが定義がない、またはLibraryをLinkしていない場合は未解決External SymbolとしてLink段階で失敗します。
- 同じ非`inline`定義を複数翻訳単位へ置くと、多重定義としてLink段階で失敗します。

## 安全な失敗実験

通常版をGitで元に戻せる状態で、`target_link_libraries(compile_and_link PRIVATE combat_math)`を一時的にComment化してBuildすると、`CalculateDamage`等を解決できずLink Errorになります。確認後は必ず元へ戻します。

## Debug方法

Visual Studioで`main`から`CalculateDamage`へStep Inし、別翻訳単位の関数へ移動することを確認します。Build Outputでは`.cpp`ごとのCompileと`.lib`／`.exe`生成順を確認します。

## よくある失敗

- Headerは見えるがLinkできない：SourceがLibrary Targetへ登録されているか、ExecutableがLibraryをLinkしているか確認します。
- Debug/Release Library混在：同じ構成とRuntime Library設定を使います。
- 古い結果が残る：`build/`を作り直してConfigureから確認します。
- 日本語Source Error：全TargetへMSVC `/utf-8`が付いているか確認します。

## 変更課題

1. 属性倍率を引数へ追加し、Testを先に書いてから実装する。
2. Damage上限と整数Overflow対策を加える。
3. `CombatMath`をDynamic Libraryへ変えた場合に必要なExport/Importを調べる。
4. Release Buildを行い`Build構成: Release`になることを確認する。
