# DXライブラリの全体像・初期化・Game Loop

> 対象: Windows版DXライブラリとVisual StudioのC++ project。関数仕様は利用versionの公式referenceを確認してください。

## 1. DXライブラリとは

DXライブラリは、WindowsやDirectXなどの低level APIを、ゲーム制作で使いやすいglobal関数群として包んだlibraryです。

```text
自分のGame Code
  ↓
DXライブラリ API
  ↓
Windows API / DirectX / Audio / Input / File I/O
  ↓
OS / Driver / CPU / GPU / Device
```

`DrawGraph`一回の裏でも、texture resource、command発行、GPU処理、presentなどが関係します。簡単なAPIを使いながら、後のDirectX編につながる内部構造を学びます。

## 2. DXライブラリが隠してくれるもの

- Win32 window class登録とwindow生成。
- Windows message処理の多く。
- DirectX device/swap chain/render target等の初期化。
- image/audio/model fileの読み込み。
- keyboard/mouse/gamepad取得。
- 2D/3D描画state設定。
- resource handle管理。
- platformごとの差の一部。

隠されていることと、存在しないことは違います。画面flip、message pump、resource解放、frame timingは自分の設計にも残ります。

## 3. DXライブラリとDirectXの違い

DirectXはMicrosoftのgraphics/audio/input等のAPI群です。DXライブラリはそれらを利用しやすくした別libraryです。

```text
DXライブラリを使う
≠ DirectX APIを直接呼んでいる
≠ GPU内部を理解しなくてよい
```

DirectX直接実装ではdevice、resource view、pipeline state、command等を細かく管理します。DXライブラリではhandleと関数呼び出しへ簡略化されています。

## 4. 学習時の役割

DXライブラリはC++基礎を実ゲームへ統合する中間地点です。

- C++ class/RAIIをgame objectへ使う。
- game loopと時間を自分で設計する。
- Scene/Input/Resource/Combatを分離する。
- global API依存をadapterへ閉じ込める。
- rendering resourceの寿命を学ぶ。
- 後でDirectX APIへ置換できる境界を作る。

## 5. 最小program

```cpp
#include "DxLib.h"

int WINAPI WinMain(
    HINSTANCE instance,
    HINSTANCE previousInstance,
    LPSTR commandLine,
    int showCommand)
{
    // DXライブラリ内部のwindow・graphics・audio・input等を初期化する。
    // 戻り値0は成功、-1は失敗。失敗後にDX関数を使ってはいけない。
    if (DxLib_Init() == -1)
    {
        return -1;
    }

    // 初期化済みresourceと内部systemを終了する。
    // 呼び出し後はDXライブラリ関数を使わず、速やかにprogramを終える。
    DxLib_End();

    return 0;
}
```

これは何も描画しませんが、library lifetimeの最小形です。

## 6. `#include "DxLib.h"`

headerは関数宣言、型、定数などをcompilerへ知らせます。headerをincludeしただけではimplementation codeはprogramに入りません。

```text
Compile: DxLib_Initという関数が存在する契約を知る
Link:    実際の関数implementationをlibraryから結び付ける
Run:     DLL/driver/resourceを利用して処理する
```

compile error、link error、runtime errorを区別します。

## 7. include directory

compilerが`DxLib.h`を探すdirectoryをprojectへ設定します。

```text
Project Properties
→ C/C++
→ General
→ Additional Include Directories
```

設定名やUIはVisual Studio versionで変わる場合があります。Debug/Release、x64等のConfiguration/Platformごとに設定対象を確認します。

## 8. library directoryとlink

linkerが`.lib`を探すdirectoryと、必要libraryを設定します。DXライブラリ配布形態やproject設定手順に従います。

典型的な失敗:

- headerは見つかるがunresolved external symbol。
- x86用libraryとx64 buildの不一致。
- Debug/Releaseの設定片方だけ。
- 相対pathの基準が異なる。
- library versionとheader versionが不一致。

## 9. ConfigurationとPlatform

Visual Studioの`Debug/Release`と`x86/x64`は別軸です。

| 軸 | 例 | 意味 |
|---|---|---|
| Configuration | Debug/Release | 最適化・debug情報等 |
| Platform | Win32/x64 | target CPU architecture |

現在選択中だけ設定して「別buildで突然壊れる」ことを防ぎます。Property Sheetやmacro pathで共通化できます。

## 10. entry point

Windows GUI applicationでは`WinMain`/`wWinMain`がentry pointになる構成があります。DXライブラリのproject設定によっては通常の`main`を使える仕組みもあります。

```cpp
int WINAPI WinMain(
    HINSTANCE instance,         // 現在process/moduleのinstance handle。
    HINSTANCE previousInstance, // 現代Win32では通常未使用の互換引数。
    LPSTR commandLine,          // command line文字列。
    int showCommand)            // windowの初期表示方法を示す値。
```

DXライブラリのversion/設定とsubsystemに合うentry pointを選びます。

## 11. `WINAPI`

`WINAPI`はWindows APIのcalling conventionを表すmacroです。calling conventionは引数の渡し方、stack cleanup、symbol表現等のABIに関係します。

通常はtemplateを正しく使い、理由なく削除・変更しません。x64では差が目立ちにくくてもsource contractとして理解します。

## 12. 未使用引数

学習中でもwarningを無視せず、意図的な未使用を示せます。

```cpp
int WINAPI WinMain(
    HINSTANCE instance,
    HINSTANCE previousInstance,
    LPSTR commandLine,
    int showCommand)
{
    (void)instance;
    (void)previousInstance;
    (void)commandLine;
    (void)showCommand;

    // ...
}
```

C++17以降ならparameter名を省略する、`[[maybe_unused]]`を使う選択肢もあります。

## 13. 初期化前設定

画面mode、window mode、文字codeなど一部の設定関数は`DxLib_Init`より前に呼ぶ必要があります。

```cpp
// window modeで起動する設定。初期化前に行う。
ChangeWindowMode(TRUE);

// 論理画面の幅、高さ、color bit depthを要求する。
SetGraphMode(1280, 720, 32);

if (DxLib_Init() == -1)
{
    return -1;
}
```

全関数が初期化前に使えるわけではありません。referenceの契約を確認します。

## 14. 設定順序はstate machine

```text
Uninitialized
  ├─ pre-init configuration可
  ↓ DxLib_Init成功
Initialized
  ├─ resource load/draw/input可
  ↓ DxLib_End
Ended
  └─ DX関数を使わずprogram終了
```

APIの有効範囲をstateとして考えると、順序bugを防げます。

## 15. `DxLib_Init`

公式referenceでは成功0、error -1です。

```cpp
const int initResult = DxLib_Init();

if (initResult < 0)
{
    // 初期化に失敗した状態でload/drawしない。
    return -1;
}
```

`== -1`と`< 0`のどちらを使うかはAPI contractに合わせます。DXライブラリ関数は多くが-1を失敗として返しますが、各関数を確認します。

## 16. 初期化失敗の原因

- 必要file/libraryの不足。
- 対応しないgraphics environment。
- architecture/configuration不一致。
- 不正な初期化前設定。
- OS/driver/device問題。
- 作業directoryや権限。

「-1だった」だけで終わらずlog file、Debugger、環境差を確認します。

## 17. Log file

DXライブラリは通常logを出力でき、初期化・resource load等の原因調査に役立ちます。配布時にlog出力をどう扱うか、個人情報やpathが含まれないかも確認します。

自作logには:

```text
時刻 / severity / subsystem / operation / path or ID / result
```

を含め、同じ問題を再構成できるようにします。

## 18. `DxLib_End`

公式referenceではDXライブラリ使用終了前に呼ぶ必要があり、呼び出し後は速やかにprogramを終了します。

終了処理では内部resource、window/system状態等が片付けられます。しかし、自作C++ objectのdestructor順やexternal resourceまで自動で正しくなるとは限りません。

## 19. 正常終了と異常終了

```cpp
if (DxLib_Init() == -1)
{
    // 初期化が成功していないので、通常のDxLib_End対象状態ではない。
    return -1;
}

// 初期化成功後の処理。

DxLib_End();
return 0;
```

初期化成功flagを持たず無条件cleanupするより、lifetimeを型へ包む方法を後で学びます。

## 20. RAII wrapper

```cpp
class DxRuntime final
{
public:
    DxRuntime() = default;

    // copyすると「終了責任が二つ」になるため禁止する。
    DxRuntime(const DxRuntime&) = delete;
    DxRuntime& operator=(const DxRuntime&) = delete;

    bool initialize()
    {
        if (initialized_)
        {
            return true; // 二重初期化を防ぐ。
        }

        initialized_ = (DxLib_Init() == 0);
        return initialized_;
    }

    ~DxRuntime()
    {
        if (initialized_)
        {
            // scopeを抜ける全経路で一度だけ終了する。
            DxLib_End();
        }
    }

private:
    bool initialized_ = false;
};
```

ただしDX resourceを所有するobjectは`DxRuntime`より先に破棄される必要があります。宣言順とscopeを設計します。

## 21. destruction order

local objectは構築の逆順で破棄されます。

```cpp
DxRuntime runtime;       // 先に構築。
runtime.initialize();

TextureCache textures;   // 後に構築。

// scope終了:
// 1. textures destructorでDeleteGraph等
// 2. runtime destructorでDxLib_End
```

DXライブラリ終了後に`DeleteGraph`を呼ぶdestructor順は不正です。

## 22. Windows message pump

window applicationにはOSからmessageが届きます。

- close要求。
- keyboard/mouse。
- window移動・resize。
- focus。
- paint/system event。

これらを処理しないとwindowが応答不能になります。DXライブラリでは`ProcessMessage`がこの役割を包みます。

## 23. `ProcessMessage`

公式referenceの必須関数です。基本的にmain loop内で毎回呼びます。

```cpp
while (ProcessMessage() == 0)
{
    // update / draw
}
```

0なら継続、-1なら終了要求/errorとしてloopを抜ける形が基本です。

## 24. なぜ毎frame必要か

game loopがCPU処理だけを続けると、OS message queueを処理できません。

```text
OSがClose messageをqueue
→ GameがProcessMessageを呼ぶ
→ DXライブラリ/Win32がdispatch
→ 終了状態を返す
→ loop終了
```

重いloadや無限loopでProcessMessageへ戻らない場合も「応答なし」になります。

## 25. 最小game loop

```cpp
#include "DxLib.h"

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
    // window mode・1280x720を初期化前に設定する。
    ChangeWindowMode(TRUE);
    SetGraphMode(1280, 720, 32);

    // library初期化に失敗したら、以降のDX関数を一切呼ばず終了する。
    if (DxLib_Init() == -1)
    {
        return -1;
    }

    // 描画先を現在表示中のfront bufferではなくback bufferへ変更する。
    SetDrawScreen(DX_SCREEN_BACK);

    // ProcessMessageが0の間だけgameを継続する。
    while (ProcessMessage() == 0)
    {
        // 前frameのback buffer内容は保証されないため、毎frame明示的に消去する。
        ClearDrawScreen();

        // Update: 入力、simulation、collisionなどをここで更新する。

        // Render: 今frameのworld stateから描画commandを発行する。
        DrawString(32, 32, "Hello DX Library", GetColor(255, 255, 255));

        // 完成したback bufferを表示側へ反映する。
        if (ScreenFlip() == -1)
        {
            break;
        }
    }

    DxLib_End();
    return 0;
}
```

## 26. Game loopの順序

```text
Process OS Messages
→ Read Input
→ Calculate Delta Time
→ Update Game State
→ Resolve Collision/Events
→ Clear Back Buffer
→ Draw Current State
→ Present/ScreenFlip
```

描画してから状態更新すると一frame遅れに見える場合があります。systemごとの順序を明示します。

## 27. front bufferとback buffer

front bufferは現在表示されている画面、back bufferは次frameを作る描画先です。

```text
Display: Front Buffer [完成frame N]
CPU/GPU: Back Buffer  [frame N+1を構築]
                        ↓
                    ScreenFlip
```

描画途中を見せず、ちらつきを防ぎます。実際のswap chain bufferingは環境・設定によりさらに複雑です。

## 28. `SetDrawScreen`

```cpp
const int result = SetDrawScreen(DX_SCREEN_BACK);
if (result == -1)
{
    // 描画先設定失敗として初期化処理を中断する。
}
```

描画先にはfront/backだけでなく`MakeScreen`で作成した描画可能handleを指定できる場合があります。render target切替はcamera/draw area等のstateへ影響するためreferenceを確認します。

## 29. `ClearDrawScreen`

back bufferの前内容を消去します。

```cpp
SetBackgroundColor(16, 18, 24);

while (ProcessMessage() == 0)
{
    ClearDrawScreen();
    // draw...
    ScreenFlip();
}
```

画面をclearしないことで残像を作るのはbuffer内容保証や複数bufferの関係から安定しません。残像effectは明示render targetとblendで作ります。

## 30. `ScreenFlip`

back bufferの完成内容をfrontへ反映します。公式referenceはflip後のback buffer内容が環境依存なので、次frameに`ClearDrawScreen`等で初期化するよう説明しています。

```cpp
if (ScreenFlip() < 0)
{
    // device/window終了等を想定しmain loopを抜ける。
    break;
}
```

## 31. VSync

`ScreenFlip`時にvertical synchronizationを待つ設定では、display refreshに合わせて待機する場合があります。

利点:

- tearingを抑える。
- 無制限loopによるGPU/CPU使用を抑える場合がある。

欠点:

- input latency。
- refreshと処理時間次第のframe pacing。
- Profilerで待ち時間が処理costに見える。

設定とtarget fpsを実測します。

## 32. Escapeで終了

```cpp
while (ProcessMessage() == 0)
{
    if (CheckHitKey(KEY_INPUT_ESCAPE) != 0)
    {
        break;
    }

    ClearDrawScreen();
    ScreenFlip();
}
```

`CheckHitKey`は現在押されている状態です。一回だけの押下eventは前frameとの差から作ります。入力章で詳しく扱います。

## 33. return code

一般に0は正常終了、非0はerrorを表します。

```cpp
enum class ExitCode : int
{
    Success = 0,
    DxInitializationFailed = -1,
    RuntimeFailure = -2
};
```

OS/process監視から原因を区別しやすくなります。ただしplatform/toolが期待するcode範囲に合わせます。

## 34. Global関数の問題

どこからでも`DrawGraph`、`LoadGraph`、`CheckHitKey`を呼べるため、小規模では便利です。しかし大規模化すると:

- test時に置換しにくい。
- load/delete所有者が不明。
- SceneとUIが互いを直接操作。
- API call順が散らばる。
- DirectXへ移行しにくい。

## 35. Adapter interface

```cpp
class IRenderer2D
{
public:
    virtual ~IRenderer2D() = default;

    virtual void beginFrame() = 0;
    virtual void drawText(int x, int y, const char* text, unsigned int color) = 0;
    virtual bool endFrame() = 0;
};
```

gameplay codeは`DrawString`を知らず、Renderer implementationだけがDXライブラリを呼びます。ただし最初から全関数を抽象化せず、変更理由がある境界から分けます。

## 36. Application class

```cpp
class Application final
{
public:
    int run();

private:
    bool configureBeforeInitialization();
    bool initialize();
    bool processFrame();
    void update(double deltaSeconds);
    void render();
    void shutdown();
};
```

entry pointは`Application::run`を呼ぶだけにし、main loopの所有者を一つにします。

## 37. `Application::run`例

```cpp
int Application::run()
{
    if (!configureBeforeInitialization())
    {
        return -1;
    }

    if (!initialize())
    {
        return -2;
    }

    // processFrameがfalseを返したら、window終了またはruntime errorとして停止する。
    while (processFrame())
    {
        // 一frameの全処理はprocessFrameへ集約する。
    }

    shutdown();
    return 0;
}
```

exceptionを使う場合も、DX runtime/resourceのcleanup順を保証します。

## 38. Frame関数

```cpp
bool Application::processFrame()
{
    if (ProcessMessage() != 0)
    {
        return false;
    }

    const double deltaSeconds = clock_.tick();

    input_.update();
    sceneManager_.update(deltaSeconds);

    ClearDrawScreen();
    sceneManager_.render(renderer_);

    return ScreenFlip() == 0;
}
```

`clock_`等は後章で実装します。この段階ではdata flowと順序を読みます。

## 39. UpdateとRenderの分離

```cpp
void Player::update(double deltaSeconds)
{
    // game stateだけを更新。描画APIを呼ばない。
    position_.x += velocity_.x * deltaSeconds;
}

void Player::render(IRenderer2D& renderer) const
{
    // stateを変更せず、現在stateの表示要求だけを行う。
    renderer.drawSprite(texture_, position_);
}
```

これによりheadless test、複数camera、interpolation等へ拡張しやすくなります。

## 40. Error propagation

DX関数の失敗をその場で無視せず、呼び出し元へ伝えます。

```cpp
[[nodiscard]] bool Renderer2D::present() noexcept
{
    return ScreenFlip() == 0;
}
```

`[[nodiscard]]`で戻り値無視をwarningにできます。error message、operation、resource IDをlogします。

## 41. Handle

DXライブラリはimage、sound、model等を整数handleで返すAPIが多いです。

```cpp
const int graphHandle = LoadGraph("Data/Player.png");
if (graphHandle == -1)
{
    // -1を有効handleとして保存しない。
}
```

整数でも単なる数値ではなく、DXライブラリ内部resourceを指すopaque IDです。加算・比較順・別resource種別への流用をしません。

## 42. Resource lifetime予告

```text
LoadGraph成功
→ handleを所有
→ DrawGraph等で借用
→ 最終使用後DeleteGraph
→ handleをinvalidへ戻す
```

同一handleの二重delete、delete後use、Scene切替漏れをRAII/resource cacheで防ぎます。

## 43. Working directory

`LoadGraph("Data/A.png")`のrelative pathは、source file位置でなくprocessのcurrent working directoryを基準に解決されます。

Visual Studioから起動、Explorerから起動、配布folderから起動で違う場合があります。実行file基準のasset rootを設計し、起動方法ごとにtestします。

## 44. 文字code

DXライブラリの文字列API、project character set、source encoding、file path encodingの組合せを一致させます。

```text
source file UTF-8
→ C++ literal encoding
→ DX library string API mode
→ OS/file system
```

文字化けは「日本語だから」ではなく、byte列をどのencodingとして解釈したかの不一致です。次章で詳しく扱います。

## 45. Debug/Release差

Debugはassert、symbol、低最適化で診断しやすく、Releaseは最適化やruntime差があります。

- 未初期化変数。
- iterator/reference invalidation。
- timing依存。
- assertの副作用。
- working directory/config path。
- library設定。

がReleaseだけで表面化することがあります。両方を定期buildします。

## 46. Warning policy

warningをerrorに近い品質signalとして扱います。

- narrowing conversion。
- signed/unsigned比較。
- 戻り値無視。
- 未使用変数。
- shadowing。
- lifetime/reference。

DXライブラリheader由来warningと自作codeを区別しつつ、自作warning 0を目標にします。

## 47. よくある失敗

### `DxLib_Init`結果を無視

初期化失敗後のAPI callで原因が離れる。直ちに終了する。

### `ProcessMessage`を呼ばない

windowが応答不能になる。毎loop処理する。

### front bufferへ直接描画

描画途中が見え、ちらつく。back bufferへ描画してflipする。

### `ScreenFlip`後にclearしない

back buffer内容を環境依存で利用する。次frame開始時にclearする。

### `DxLib_End`後にresource destructor

library終了後にDelete系APIが走る。所有objectを先に破棄する。

### global関数を全classから呼ぶ

依存と所有が追跡不能になる。Application/adapterへ集約する。

## 48. 最小Debug checklist

- [ ] active Configuration/Platformは正しいか。
- [ ] `DxLib.h`とlibrary versionは一致するか。
- [ ] include/library directoryはその構成へ設定済みか。
- [ ] entry pointとsubsystemは合うか。
- [ ] 初期化前関数を`DxLib_Init`より前に呼んだか。
- [ ] `DxLib_Init`の戻り値を確認したか。
- [ ] `ProcessMessage`へ毎frame戻るか。
- [ ] 描画先はback bufferか。
- [ ] clear→draw→flipの順か。
- [ ] DX resourceは`DxLib_End`より前に破棄されるか。
- [ ] logとworking directoryを確認したか。

## 49. 確認問題

1. DXライブラリとDirectXは何が違うか。
2. header include、library link、runtime実行の違いは何か。
3. `DxLib_Init`失敗後に描画を続けてはいけない理由は何か。
4. 初期化前に呼ぶ必要がある設定関数があるのはなぜか。
5. `ProcessMessage`がないとwindowが応答不能になる理由は何か。
6. front/back bufferの役割を説明してください。
7. `ScreenFlip`後に毎frameclearすべき理由は何か。
8. RAII wrapperのdestructor順が重要な理由は何か。
9. DXライブラリの整数handleを普通のint値と同様に扱えない理由は何か。
10. UpdateとRenderを分ける利点は何か。

## 50. 次章への接続

次章では、project設定と文字codeをさらに詳しく扱います。

```text
Source encoding
→ Compiler character set
→ narrow/wide string
→ DXライブラリの文字列mode
→ file path / draw text
```

日本語path・文字描画・logを偶然動かすのではなく、byteとcode pointの変換経路から理解します。

## 51. 公式資料

- [DXライブラリ公式・関数リファレンス](https://dxlib.xsrv.jp/dxfunc.html)
- [DXライブラリ公式・画面操作系リファレンス](https://dxlib.xsrv.jp/function/dxfunc_graph3.html)
- [DXライブラリ公式・ゲームプログラムの基礎知識](https://dxlib.xsrv.jp/lecture/lecture1.html)
- [DXライブラリ公式・Visual Studio Community 2026での使用手順](https://dxlib.xsrv.jp/use/dxuse_vscom2026.html)

配布version、Visual Studio version、target architectureに対応する公式手順を確認してください。
