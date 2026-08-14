# DirectX 11：Visual Studio・Windows SDK・Project設定

この章では、DirectX 11のCodeを書く前に、Compiler、Linker、Windows SDK、Build構成を整えます。設定画面の場所だけでなく、Sourceが実行Fileになるまでに何が起こり、各設定がどの段階へ影響するかを理解します。

## 1. 必要な開発要素

- Visual StudioまたはMSVC Build Tools。
- Desktop development with C++ Workload。
- MSVC C++ CompilerとStandard Library。
- Windows SDK。
- x64向けLibraryとDebugger。
- GitなどのVersion管理。

Visual Studio Installerでは「C++によるデスクトップ開発」が基本です。Game Development with C++もDirectX関連Componentを含められますが、必要な個別Componentを確認します。

## 2. Visual Studioの役割

Visual StudioはEditorだけではありません。

```text
Editor
Project system
MSVC compiler driver
Linker
Debugger
Profiler
Static analysis
Graphics diagnostics
```

IDEとCompilerを同一視しません。

## 3. Windows SDKの役割

Windows SDKはWindows APIとDirectX APIを使うためのHeader、Import Library、Tool、Metadataを提供します。

```text
Include/um      Win32、D3D11、DXGIなど
Include/shared  共通型、HRESULTなど
Lib/um/x64      x64用Import Library
bin             SDK Tool
```

実際のInstall PathをSource Codeへ直書きしません。Visual StudioのMacroが解決します。

## 4. HeaderとLibraryとDLL

```text
Header (.h)          Compile時に宣言を提供する
Import Library (.lib) Link時にDLLのSymbol情報を提供する
Runtime DLL          実行時に本体処理を提供する
```

`.lib`へDirect3D Runtime全体が静的に埋め込まれるとは限りません。

## 5. DirectX 11で使う代表Header

```cpp
#include <Windows.h>       // Win32 WindowとMessage。
#include <d3d11.h>         // Direct3D 11 Interfaceと構造体。
#include <dxgi1_6.h>       // DXGI Interface。使う最小Versionを選ぶ。
#include <DirectXMath.h>   // VectorとMatrix演算。
#include <wrl/client.h>    // Microsoft::WRL::ComPtr。
```

必要なHeaderだけを各境界でIncludeします。

## 6. 代表的なImport Library

```text
d3d11.lib       D3D11CreateDeviceなど
dxgi.lib        CreateDXGIFactoryなど
dxguid.lib      一部のGUID定義
dxcompiler.lib  DXCを使う場合
d3dcompiler.lib 旧来のD3DCompileを使う場合
user32.lib      Window、Message、Input関連
gdi32.lib       一部のWindow描画やMonitor関連
```

実際に利用するFunctionへ対応するLibraryだけをLinkします。

## 7. pragma commentによるLink

```cpp
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
```

小規模な学習Projectでは便利ですが、Build依存がSourceへ埋まります。大規模ではProject、Property Sheet、CMakeなどへ集約する方法もあります。

## 8. Project Template

学習では「空のProject」または最小のWindows Desktop Applicationを使えます。Templateが生成したCodeを残す場合、役割を理解せず不要なFrameworkを抱えないよう確認します。

## 9. SolutionとProject

- Solution：複数ProjectをまとめるContainer。
- Project：Compile単位、Output、設定、Source一覧を持つ。

将来はEngine Static Library、Game Executable、Test Executableへ分けられます。

## 10. 推奨構成

```text
DirectXStudy.sln
  DirectXStudyApp       Windows executable
  DirectXStudyCore      optional static library
  DirectXStudyTests     optional console test executable
```

最初はApp一つで始め、境界が見えてから分割します。

## 11. Platformはx64

通常のPC向け学習ではx64を選びます。Win32とx64のObject FileやLibraryを混ぜるとLinkできません。

```text
Debug | x64
Release | x64
```

Visual Studioで表示される`Win32` Platformは32bit x86 Buildを意味し、Win32 API全体の意味とは異なります。

## 12. Configuration

```text
Debug       診断優先、最適化を抑え、Debug情報を持つ
Release     性能優先、最適化し、不要診断を減らす
Development 任意追加。最適化とDebug機能を両立する構成
```

すべてのConfigurationへ同じ設定を入れたつもりか確認します。

## 13. Property Page上部

Visual StudioのProject Propertyには「Configuration」と「Platform」の選択があります。Debug x64だけ変更し、Release x64へ反映されていない事故が多いため注意します。

## 14. Windows SDK Version

ProjectのWindows SDK VersionはInstall済みSDKから選びます。必要理由がない限り、特定Machineだけに存在するPathをAdditional Include Directoriesへ直書きしません。

## 15. Platform Toolset

Platform Toolsetは使用するMSVC CompilerとBuild Toolの組を指定します。共同作業では利用VersionをREADMEやBuild設定に記録します。

## 16. C++ Language Standard

```text
Project Properties
 -> C/C++
 -> Language
 -> C++ Language Standard
```

教材ではC++20などProjectで決めたStandardを全Targetで統一します。Compiler Defaultへ暗黙依存しません。

## 17. Conformance Mode

`/permissive-`はStandard準拠を強めます。移植性と誤り検出のため有効化を検討します。

## 18. Warning Level

```text
/W4       高いWarning Level
/WX       WarningをErrorとして扱う
```

外部HeaderのWarningまで無理に直さず、自分のCodeと第三者Codeの境界を分けます。

## 19. Debug Information

CompilerがDebug情報を生成し、LinkerがPDBを作ります。PDBにはSourceと機械語を対応付ける情報が入り、BreakpointやCall Stack解析に使われます。

## 20. Runtime Library

```text
/MDd  Debug用Dynamic C Runtime
/MD   Release用Dynamic C Runtime
/MTd  Debug用Static C Runtime
/MT   Release用Static C Runtime
```

異なるRuntime LibraryでBuildしたObjectやLibraryを混ぜると、Heap所有権やABIの問題が起こり得ます。

## 21. DebugとReleaseを混ぜない

Debug LibraryをReleaseへLinkしたり、逆にRelease ObjectをDebug前提で扱ったりしません。Output DirectoryもConfigurationごとに分けます。

## 22. Preprocessor Macro

```text
_DEBUG       Debug Runtimeで一般的に利用
NDEBUG       assertを無効化するRelease側で一般的
UNICODE
_UNICODE     Wide Character版Windows APIを選ぶ
WIN32_LEAN_AND_MEAN
NOMINMAX
```

Macroの意味を理解してから全体定義します。

## 23. NOMINMAX

`Windows.h`の`min`と`max` Macroが`std::min`、`std::max`へ干渉することを防ぎます。

```cpp
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>
```

Build Option `/DNOMINMAX`へ置く方法もあります。

## 24. WIN32_LEAN_AND_MEAN

`Windows.h`がIncludeする一部の不要なAPI Headerを減らします。Compile時間と名前衝突を抑える助けになります。

## 25. UNICODE

`UNICODE`と`_UNICODE`を定義すると、`CreateWindow`などのMacroがWide Character版へ解決されます。

```cpp
int WINAPI wWinMain(HINSTANCE instance,
                    HINSTANCE,
                    PWSTR,
                    int showCommand)
{
    // UTF-16のWin32入口。
}
```

内部文字列形式とFile Encodingは別問題です。

## 26. Source Encoding

Source FileはUTF-8へ統一し、Compiler Option `/utf-8`を設定できます。日本語CommentやLiteralがMachineごとに異なる解釈を受けることを防ぎます。

## 27. Subsystem

Linkerの`/SUBSYSTEM`はExecutableが利用するWindows実行環境を指定します。

```text
/SUBSYSTEM:CONSOLE  Consoleを持ち、main / wmainが入口
/SUBSYSTEM:WINDOWS  GUI ApplicationでWinMain / wWinMainが入口
```

Subsystemは「DirectXが使えるか」ではなく、OSが選ぶ実行入口やConsole提供に関係します。

## 28. 学習中のConsole

初期段階ではConsole Subsystemと`main`を使い、その中でWin32 Windowを作る方法もDebug Logに便利です。最終的にWindows Subsystemへ変える場合、入口とLog出力先を整理します。

## 29. Entry Pointを手動指定しない

通常は`main`、`wmain`、`WinMain`、`wWinMain`とSubsystemからLinkerに選ばせます。`/ENTRY`を理由なく手動設定するとC Runtime初期化を飛ばす危険があります。

## 30. CompilerからExecutableまで

```text
.cpp
 -> Preprocessor
 -> Compiler
 -> .obj
 -> Linker + .lib
 -> .exe + .pdb
 -> Loader resolves runtime DLL imports
```

Errorがどの段階で起きたかを見分けます。

## 31. Compile Error

Syntax Error、型不一致、宣言不足、Header未発見など、`.cpp`から`.obj`を作る前に起こります。

```text
fatal error C1083: Cannot open include file
```

Include Path、File名、Install Componentを確認します。

## 32. Link Error

宣言は見えたが実装Symbolを見つけられない場合に起こります。

```text
LNK2019 unresolved external symbol D3D11CreateDevice
```

`d3d11.h`をIncludeしただけではLinkされません。`d3d11.lib`が必要です。

## 33. Runtime Error

Build成功後、DLL不足、Device作成失敗、Null Pointer、不正API使用などで起こります。HRESULT、Debug Layer、Debuggerを使います。

## 34. HeaderとLibraryのVersion一致

新しいSDK Headerで宣言されたInterfaceを使っても、対象OS RuntimeがそのInterfaceを提供するとは限りません。`QueryInterface`と機能検査で対応を確認します。

## 35. Additional Include Directories

Windows SDK標準Headerは通常、Visual Studioが自動設定します。自分のProject Headerや第三者Libraryへだけ追加Pathを使い、絶対Pathを避けます。

```text
$(ProjectDir)src
$(SolutionDir)third_party\include
```

## 36. Additional Library Directories

Windows SDK標準Libraryも通常は自動設定されます。第三者Libraryの場合はPlatformとConfigurationを分けます。

```text
$(SolutionDir)third_party\lib\$(Platform)\$(Configuration)
```

## 37. Additional Dependencies

```text
Linker
 -> Input
 -> Additional Dependencies
```

`d3d11.lib;dxgi.lib;...`のように指定できます。Property SheetまたはBuild Scriptへ集約する方法もあります。

## 38. Property Macro

```text
$(SolutionDir)
$(ProjectDir)
$(Configuration)
$(Platform)
$(OutDir)
$(IntDir)
```

Macro展開後の値をProperty画面で確認し、末尾区切り文字を意識します。

## 39. Output Directory

```text
bin\$(Platform)\$(Configuration)\
```

Executable、Runtime DLL、Assetの配置先を整理します。

## 40. Intermediate Directory

```text
obj\$(Platform)\$(Configuration)\
```

`.obj`や中間FileをSource Directoryへ混ぜません。

## 41. Working Directory

Visual Studioから実行したときのCurrent Working DirectoryはAssetの相対Pathに影響します。

```text
Debugging -> Working Directory -> $(ProjectDir)
```

ただし本番設計ではExecutable PathまたはAsset Rootを明示的に解決します。

## 42. Asset Pathの注意

`../../texture.png`のようなCurrent Directory依存を各所へ散らしません。Asset Rootと論理Asset IDをResource Managerへ集めます。

## 43. Precompiled Header

PCHは変更頻度の低い巨大Headerを事前Compileし、Build時間を減らします。最初は無効でも構いません。Projectが大きくなってから計測して導入します。

## 44. Unity Buildとの混同

Unity Buildは複数`.cpp`を一つのTranslation UnitへまとめるBuild最適化で、Unity Engineとは別物です。隠れたInclude依存を生み得るため注意します。

## 45. Incremental Build

変更したTranslation Unitだけ再Compileし、Linkし直します。Headerへ不要なIncludeを増やすと、多数の`.cpp`が再Compileされます。

## 46. Static Library

Engine Coreを`.lib`へまとめても、それはImport Libraryとは役割が異なります。

- Static Library：Object Codeをまとめる。
- Import Library：DLL Export Symbolへの接続情報を持つ。

拡張子が同じでも意味が違います。

## 47. DLLの検索

実行時DLLはWindows Loaderの検索規則で見つけられます。無関係なSystem Directoryへ手動Copyせず、Applicationと同じDirectoryや正規Installerで配置します。

## 48. DirectX Runtimeの注意

Direct3D 11とDXGIの基本RuntimeはWindows Componentです。一方、使用するShader Compilerや補助Libraryによっては別の配布要件があります。採用APIごとに公式Deployment資料を確認します。

## 49. D3DXを前提にしない

古いTutorialにあるD3DX11は現在のWindows SDK標準設計ではありません。DirectXMath、WIC、DirectX Tool Kitなど役割別の現行手段を確認します。

## 50. Debug Layer Component

`D3D11_CREATE_DEVICE_DEBUG`でDevice作成が`DXGI_ERROR_SDK_COMPONENT_MISSING`になる場合、Graphics Toolsや対応Debug Layer ComponentのInstall状況を確認します。Releaseで無条件にDebug Flagを使いません。

## 51. 最小Link確認Code

```cpp
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <d3d11.h>
#include <wrl/client.h>

#pragma comment(lib, "d3d11.lib")

int main()
{
    Microsoft::WRL::ComPtr<ID3D11Device> device;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
    D3D_FEATURE_LEVEL obtainedLevel{};

    const HRESULT hr = D3D11CreateDevice(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        0,
        nullptr,
        0,
        D3D11_SDK_VERSION,
        device.GetAddressOf(),
        &obtainedLevel,
        context.GetAddressOf());

    return FAILED(hr) ? 1 : 0;
}
```

Windowも描画もしませんが、Header、Library、Runtime、Device作成までを切り分けられます。

## 52. Codeの内部処理

1. PreprocessorがHeaderとMacroを展開する。
2. Compilerが`D3D11CreateDevice`呼び出しを含むObject Fileを作る。
3. Linkerが`d3d11.lib`からImport情報を解決する。
4. Windows Loaderが実行時DLLを解決する。
5. RuntimeがDriverとAdapterを選びDeviceを作る。
6. ComPtrが終了時にCOM参照をReleaseする。

## 53. Debug Flag付きFallback

Debug Layer不足を「GPU非対応」と誤判定しないよう、Debug Buildでは失敗HRESULTを確認し、開発環境の不足として報告します。黙ってDebugなしへ落とすかどうかはTeam方針です。

## 54. Build Log

Visual StudioのOutput Windowで実際のCompilerとLinker Command Lineを確認できます。Property画面の結果がどうOptionへ変換されたかを見ます。

## 55. `/showIncludes`

IncludeされたHeader一覧を表示し、重い依存や意図しないHeaderを調査できます。常時有効にするとLogが多いため診断時に使います。

## 56. RebuildとClean

- Build：変更分だけBuild。
- Rebuild：対象をCleanして全Build。
- Clean：中間生成物を削除。

SourceやAssetを消す操作ではありませんが、未保存の生成物だけに依存しないようにします。

## 57. Project FileをVersion管理する

`.sln`、`.vcxproj`、`.filters`、Property Sheet、Build Scriptは再現に必要です。`.vs`、中間Object、個人設定、巨大Build Outputは通常除外します。

## 58. `.gitignore`例

```gitignore
.vs/
bin/
obj/
*.user
*.VC.db
*.pdb
*.ilk
```

配布に必要なPDBやAssetを別工程で扱う場合は規則を調整します。

## 59. Machine固有設定

User Macroや`.user`へ個人Pathを置き、共有Projectへ絶対PathをCommitしません。依存VersionはDocumentationとLock可能な仕組みへ記録します。

## 60. Property Sheet

複数Projectへ共通のWarning、Language Standard、Output Pathを適用する`.props`を使えます。変更の影響範囲を理解し、Configuration条件を確認します。

## 61. CMakeという選択肢

CMakeはProject Fileを生成できます。最初はVisual Studio Propertyを理解し、その後Build設定をCodeとして管理する選択肢を学ぶと、LinkerとCompilerの関係を見失いません。

## 62. よくある失敗：All Configurationsを見ない

Debug x64だけ動き、Release x64でLibrary不足になります。設定変更時に対象ConfigurationとPlatformを毎回確認します。

## 63. よくある失敗：x86とx64を混ぜる

Machine Type不一致のLink Errorになります。Library Folder、Project Platform、Dependency ProjectのPlatformを揃えます。

## 64. よくある失敗：Headerだけ追加する

Compileは通るがLinkで未解決Symbolになります。Header、Import Library、Runtime Componentの三段階を確認します。

## 65. よくある失敗：絶対Path

自分のPCでだけBuildできます。`$(SolutionDir)`などのMacro、Package Manager、Repository内Dependencyを使います。

## 66. よくある失敗：ReleaseだけCrash

未初期化値、寿命違反、Data Race、未定義動作が最適化で表面化します。Debugで動くことを正しさの証明にしません。

## 67. 環境確認表

- [ ] Desktop development with C++が入っている。
- [ ] MSVC x64 Toolsetが入っている。
- [ ] Windows SDKが選択できる。
- [ ] DebugとReleaseのx64構成がある。
- [ ] C++ Standardと`/utf-8`が統一されている。
- [ ] `/W4`とConformance Modeを確認した。
- [ ] OutputとIntermediate Directoryが分離されている。
- [ ] `d3d11.lib`と必要LibraryをLinkできる。

## 68. 最小Project確認表

- [ ] `Windows.h`と`d3d11.h`をIncludeできる。
- [ ] ComPtrをCompileできる。
- [ ] `D3D11CreateDevice`のLinkが通る。
- [ ] Hardware Deviceを作れる。
- [ ] 失敗HRESULTをLogできる。
- [ ] Debug Layer有効時のMessageを確認できる。
- [ ] 終了時にCOM Resourceを解放できる。

## 69. この章の要点

- Visual Studio、MSVC、Linker、Windows SDKは別の役割を持ちます。
- Header、Import Library、Runtime DLLの三段階を区別します。
- x64、Configuration、Toolset、SDK Versionを明示します。
- Subsystemは実行環境と既定Entry Pointを決めます。
- Source Encoding、Warning、C++ StandardをProject全体で統一します。
- 絶対Pathを避け、Property Macroと共有設定を使います。
- Compile、Link、Runtime Errorを発生段階で切り分けます。
- 最小Device作成Codeで環境だけを先に検証します。

## 70. 公式資料

- [Install C and C++ support in Visual Studio](https://learn.microsoft.com/en-us/cpp/build/vscpp-step-0-installation)
- [Windows C++ desktop application types](https://learn.microsoft.com/en-us/cpp/windows/overview-of-windows-programming-in-cpp)
- [Visual C++ project property pages](https://learn.microsoft.com/en-us/cpp/build/reference/property-pages-visual-cpp)
- [MSVC linker reference](https://learn.microsoft.com/en-us/cpp/build/reference/linking)
- [SUBSYSTEM linker option](https://learn.microsoft.com/en-us/cpp/build/reference/subsystem)
- [d3d11.h header reference](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/)

次章では、`wWinMain`、Window Class、`HWND`、Window Procedure、Message Queueを使い、Direct3Dが描画するためのWin32 Windowを作ります。
