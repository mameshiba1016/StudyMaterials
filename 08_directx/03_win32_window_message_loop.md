# DirectX 11：Win32 Window・Message Loop

この章では、Direct3D 11の描画先となるWin32 Windowを作ります。Windowは単なる枠ではなく、OSからResize、入力、終了、DPI変更などのMessageを受け取る窓口です。

## 1. Direct3Dより先にWindowを作る理由

Desktop用Swap Chainは通常、表示対象となる`HWND`を必要とします。

```text
Register Window Class
 -> Create Window and receive HWND
 -> Create D3D Device / Swap Chain for HWND
 -> Run Message and Game Loop
```

## 2. Win32 API

Win32 APIはWindows Desktop ApplicationからOS機能を利用するC形式のAPIです。Window、Message、File、Threadなどを扱います。

```cpp
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
```

## 3. Handle

HandleはOSが管理するObjectを識別する値です。

```text
HWND       Window Handle
HINSTANCE  Module Instance Handle
HCURSOR    Cursor Handle
HICON      Icon Handle
HMENU      Menu Handle
```

Handleの内部値やPointer性を仮定しません。

## 4. wWinMain

Windows SubsystemのUnicode入口として`wWinMain`を使えます。

```cpp
int WINAPI wWinMain(HINSTANCE instance,
                    HINSTANCE previousInstance,
                    PWSTR commandLine,
                    int showCommand)
{
    // instanceは実行Moduleを識別します。
    // previousInstanceはWin32では通常使用しません。
    // commandLineは引数文字列です。
    // showCommandは初期表示方法です。
    return 0;
}
```

## 5. WINAPI

`WINAPI`は関数のCalling Conventionを指定するMacroです。OSが期待する呼び出し規則と一致させます。

## 6. Window ClassはC++ Classではない

Win32 Window Classは、Window Procedure、Cursor、Icon、背景などをまとめたOS側Templateです。C++の`class`構文とは別概念です。

## 7. WNDCLASSEXW

```cpp
constexpr wchar_t kWindowClassName[] = L"DirectXStudyWindowClass";

WNDCLASSEXW windowClass{};
windowClass.cbSize = sizeof(windowClass);
windowClass.style = CS_HREDRAW | CS_VREDRAW;
windowClass.lpfnWndProc = WindowProcedure;
windowClass.hInstance = instance;
windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
windowClass.lpszClassName = kWindowClassName;
```

構造体を`{}`でゼロ初期化してから必要Memberを設定します。

## 8. cbSize

`cbSize`へ構造体Sizeを渡すことで、OS側は受け取った構造体Versionと利用可能範囲を判断できます。

## 9. Class Style

`CS_HREDRAW`と`CS_VREDRAW`は幅・高さ変更時の再描画動作に関係します。Direct3Dで常時描画するApplicationでは必要性を検討します。

## 10. Window Procedureの宣言

```cpp
LRESULT CALLBACK WindowProcedure(HWND window,
                                 UINT message,
                                 WPARAM wParam,
                                 LPARAM lParam);
```

- `HWND`：Message対象Window。
- `UINT`：Message ID。
- `WPARAM`、`LPARAM`：Message固有の追加Data。
- `LRESULT`：Message処理結果。

## 11. RegisterClassExW

```cpp
const ATOM classAtom = RegisterClassExW(&windowClass);
if (classAtom == 0)
{
    const DWORD error = GetLastError();
    // errorをLogし、Window作成へ進みません。
    return false;
}
```

登録失敗を無視しません。

## 12. A版とW版

Win32 APIにはANSI版`A`とUnicode版`W`があります。

```text
CreateWindowExA  narrow string
CreateWindowExW  wide string / UTF-16
```

教材では`W`版を明示し、文字型の混在を避けます。

## 13. Client AreaとWindow Rectangle

- Client Area：Applicationが描画する内側。
- Window Rectangle：Title BarとBorderを含む外側。

1280×720のBack Bufferが必要なら、Client Areaを1280×720にします。

## 14. AdjustWindowRectExForDpi

```cpp
RECT rectangle{0, 0, 1280, 720};
const DWORD style = WS_OVERLAPPEDWINDOW;
const DWORD extendedStyle = 0;
const UINT dpi = 96; // 実際はDPI Awareness設定に応じて取得します。

if (!AdjustWindowRectExForDpi(
        &rectangle, style, FALSE, extendedStyle, dpi))
{
    return false;
}

const int windowWidth = rectangle.right - rectangle.left;
const int windowHeight = rectangle.bottom - rectangle.top;
```

## 15. CreateWindowExW

```cpp
HWND window = CreateWindowExW(
    extendedStyle,
    kWindowClassName,
    L"DirectX 11 Study",
    style,
    CW_USEDEFAULT,
    CW_USEDEFAULT,
    windowWidth,
    windowHeight,
    nullptr,
    nullptr,
    instance,
    applicationPointer);

if (window == nullptr)
{
    const DWORD error = GetLastError();
    return false;
}
```

## 16. CreateWindowExの引数

- Extended Style。
- 登録済みClass名。
- Title文字列。
- Window Style。
- 初期位置と外側Size。
- ParentまたはOwner。
- Menu。
- Module Instance。
- Window Procedureへ渡す任意Data。

## 17. WS_OVERLAPPEDWINDOW

一般的なTop-level Window用Styleの組み合わせです。Title Bar、System Menu、Size変更枠、最小化・最大化Buttonなどを含みます。

## 18. Window StyleはBit Flag

```cpp
DWORD style = WS_OVERLAPPEDWINDOW;
style &= ~WS_THICKFRAME; // 例：Size変更枠を除く。
style &= ~WS_MAXIMIZEBOX;
```

採用StyleとClient Size計算を一致させます。

## 19. ShowWindow

```cpp
ShowWindow(window, showCommand);
UpdateWindow(window);
```

`CreateWindowExW`成功だけでは必ずしも表示済みではありません。

## 20. Window作成中にもMessageが来る

`CreateWindowExW`は戻る前に`WM_NCCREATE`や`WM_CREATE`などをWindow Procedureへ送ります。したがってApplication Pointerの接続は作成Message中に行います。

## 21. CREATESTRUCT

`CreateWindowExW`の最後の引数は`CREATESTRUCT::lpCreateParams`として`WM_NCCREATE`へ渡されます。

```cpp
case WM_NCCREATE:
{
    auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
    auto* app = static_cast<Application*>(create->lpCreateParams);
    SetWindowLongPtrW(window, GWLP_USERDATA,
                      reinterpret_cast<LONG_PTR>(app));
    return TRUE;
}
```

## 22. GWLP_USERDATA

WindowごとにApplication Pointerなどを保存できる領域です。x64対応の`SetWindowLongPtrW`と`GetWindowLongPtrW`を使います。

## 23. Pointerの取り出し

```cpp
Application* GetApplication(HWND window)
{
    return reinterpret_cast<Application*>(
        GetWindowLongPtrW(window, GWLP_USERDATA));
}
```

`WM_NCCREATE`以前はNullになり得ます。

## 24. Static ProcedureからMemberへ転送

```cpp
LRESULT CALLBACK WindowProcedure(HWND window, UINT message,
                                 WPARAM wParam, LPARAM lParam)
{
    if (message == WM_NCCREATE)
    {
        auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
        auto* app = static_cast<Application*>(create->lpCreateParams);
        SetWindowLongPtrW(window, GWLP_USERDATA,
                          reinterpret_cast<LONG_PTR>(app));
    }

    if (auto* app = GetApplication(window))
        return app->HandleWindowMessage(window, message, wParam, lParam);

    return DefWindowProcW(window, message, wParam, lParam);
}
```

## 25. Lifetime条件

`GWLP_USERDATA`へ保存するApplicationはWindowより長く生存する必要があります。Stack上Applicationを作る場合も、Window破棄完了までScopeを維持します。

## 26. Messageとは

MessageはOSやApplicationがWindowへ通知する数値Codeと追加Dataです。

```text
WM_CLOSE       閉じる要求
WM_DESTROY     Windowが破棄された
WM_SIZE        Client Sizeが変わった
WM_PAINT       再描画要求
WM_ACTIVATEAPP ApplicationのActive状態変更
WM_DPICHANGED  DPI変更
```

## 27. Message Queue

Windowを作ったThreadにはMessage Queueがあります。Keyboard、Mouse、Window操作などのQueued MessageをLoopで取り出します。

## 28. GetMessage

GUI ToolのようにMessageが来るまで待つApplicationでは`GetMessage`を使えます。Queueが空ならBlockします。

```cpp
MSG message{};
while (GetMessageW(&message, nullptr, 0, 0) > 0)
{
    TranslateMessage(&message);
    DispatchMessageW(&message);
}
```

## 29. GameでのPeekMessage

GameはMessageがなくてもUpdateとRenderを続けるため、非Blockの`PeekMessage`を使います。

```cpp
bool running = true;
while (running)
{
    MSG message{};
    while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE))
    {
        if (message.message == WM_QUIT)
        {
            running = false;
            break;
        }

        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    if (!running)
        break;

    game.Update();
    renderer.Render();
}
```

## 30. PM_REMOVE

`PM_REMOVE`は取得したMessageをQueueから削除します。削除しなければ同じMessageを繰り返し取得します。

## 31. TranslateMessage

KeyboardのVirtual Key Messageから文字Messageを生成する処理に関係します。すべての入力がここだけで得られるわけではありません。

## 32. DispatchMessage

`DispatchMessageW`はMessage対象のWindow Procedureを呼びます。自分でWindow Procedureを直接呼ぶ通常設計ではありません。

## 33. WM_QUITはWindow Messageではない

`WM_QUIT`はThread Message Queueへ入り、`DispatchMessage`されません。Message Loop側で検出します。

## 34. WM_CLOSE

Windowを閉じてよいか問い合わせる機会です。

```cpp
case WM_CLOSE:
    DestroyWindow(window);
    return 0;
```

未保存Dataの確認が必要なら、確認後に`DestroyWindow`します。

## 35. WM_DESTROY

```cpp
case WM_DESTROY:
    PostQuitMessage(0);
    return 0;
```

Main Window破棄後にMessage Loopを終了させます。複数Windowでは、どのWindow破棄で終了するかを決めます。

## 36. WM_NCDESTROY

Windowの最後期Messageです。保存した`GWLP_USERDATA`をNullへ戻し、以後古いPointerを使わないようにできます。

```cpp
case WM_NCDESTROY:
    SetWindowLongPtrW(window, GWLP_USERDATA, 0);
    return DefWindowProcW(window, message, wParam, lParam);
```

## 37. DefWindowProc

処理しないMessageは`DefWindowProcW`へ渡します。Title Bar、移動、Size変更など標準挙動が実装されています。

## 38. Messageの戻り値

Messageごとに期待される戻り値が違います。何でも0を返さず、公式Referenceで確認します。

## 39. WM_SIZE

```cpp
case WM_SIZE:
{
    const UINT width = LOWORD(lParam);
    const UINT height = HIWORD(lParam);
    const bool minimized = wParam == SIZE_MINIMIZED;
    app->QueueResize(width, height, minimized);
    return 0;
}
```

Resize処理をWindow Procedure内で即座に行わず、Rendererの安全な更新境界へQueueできます。

## 40. LOWORDとHIWORD

`WM_SIZE`では幅と高さが`lParam`へ格納されます。Messageごとに`WPARAM`と`LPARAM`の意味が違うため、正しいMacroを使います。

## 41. 最小化

最小化時にはClient幅・高さが0になることがあります。0×0のSwap ChainやDepth Textureを作らず、描画とResizeを保留します。

## 42. Resizeの遅延

Window BorderをDrag中は多数の`WM_SIZE`が届きます。毎Messageで重いGPU Resource再作成をする代わりに、最新Sizeだけ保存してFrame境界で一度処理します。

## 43. WM_ENTERSIZEMOVEとWM_EXITSIZEMOVE

対話的なWindow移動・Resizeの開始と終了を検出できます。Resize中の描画頻度を下げ、終了時に確定Resizeする設計があります。

## 44. Client Size取得

```cpp
RECT client{};
if (!GetClientRect(window, &client))
    return false;

const UINT width = static_cast<UINT>(client.right - client.left);
const UINT height = static_cast<UINT>(client.bottom - client.top);
```

Swap Chain用SizeはClient Areaから取得します。

## 45. WM_PAINT

Direct3D Gameは通常Loopで描画しますが、`WM_PAINT`を無視して無効領域を残し続けないよう、最低限`BeginPaint`と`EndPaint`で検証します。

```cpp
case WM_PAINT:
{
    PAINTSTRUCT paint{};
    BeginPaint(window, &paint);
    EndPaint(window, &paint);
    return 0;
}
```

## 46. Background Brush

Window Classの背景BrushをNullにし、Direct3Dで全画面描画すると、OSによる背景消去のFlickerを減らせます。ただし描画開始前やDevice Lost中の見え方を考慮します。

## 47. WM_ERASEBKGND

Direct3DがClient全体を描画する場合、背景消去を処理済みとして返す設計があります。描画しない領域がある場合は残像を起こさないよう注意します。

## 48. Active状態

```cpp
case WM_ACTIVATEAPP:
    app->SetActive(wParam != FALSE);
    return 0;
```

非Active時に入力をClearし、Simulation停止またはfps制限を行えます。

## 49. Focus

`WM_SETFOCUS`と`WM_KILLFOCUS`でKeyboard Focusを追跡できます。Focus喪失時に押下状態を残すと、復帰後に移動し続けます。

## 50. PowerとSuspend

LaptopやSession状態を考慮するApplicationではPower BroadcastやSession Messageを扱います。最初の描画学習では範囲を限定し、後でApplication Lifecycleへ追加します。

## 51. DPI Awareness

DPI Awarenessを宣言しないApplicationはOSによるBitmap Scalingを受け、Client Sizeと見た目のPixelがずれる場合があります。ManifestでPer-Monitor DPI Awarenessを設定する方法を後の実行例で扱います。

## 52. DPIと論理Size

96 DPIを100%の基準とし、高DPI MonitorではUIやWindow非Client領域のScaleが変わります。Render Resolution、Window Size、UI論理Sizeを同一視しません。

## 53. WM_DPICHANGED

Monitor間移動などでDPIが変わると通知されます。`lParam`の推奨Window Rectangleを使って位置とSizeを更新できます。

```cpp
case WM_DPICHANGED:
{
    const auto* suggested = reinterpret_cast<const RECT*>(lParam);
    SetWindowPos(window, nullptr,
        suggested->left, suggested->top,
        suggested->right - suggested->left,
        suggested->bottom - suggested->top,
        SWP_NOZORDER | SWP_NOACTIVATE);
    return 0;
}
```

## 54. GetDpiForWindow

Windowの現在DPIを取得します。DPI Awareness Modeにより結果の意味が変わるため、Process Manifestと合わせます。

## 55. Cursor

Window ClassへArrow Cursorを設定し、Game中の表示・非表示、Client内制限、Relative Mouse入力はInput章で分離して扱います。

## 56. Alt+Enter

DXGIの自動Alt+Enter Full Screen切替を無効化し、自前のWindow Mode管理を行う設計があります。Swap Chain作成後の章で扱います。

## 57. Alt+F4

通常は`WM_CLOSE`へつながります。Game LoopでKeyを直接監視して終了処理を重複させません。

## 58. Error取得

Win32 APIが失敗した直後に`GetLastError`を呼びます。別APIを挟むとError値が変わる可能性があります。

```cpp
const DWORD error = GetLastError();
```

すべてのAPIが`GetLastError`を意味ある形で設定するとは限らないため、各Referenceを確認します。

## 59. Error文字列

`FormatMessageW`でSystem Error Codeを文字列化できます。Code自体も必ずLogへ残します。

## 60. Window Wrapper

```cpp
class Win32Window final
{
public:
    bool Create(HINSTANCE instance, void* owner,
                UINT clientWidth, UINT clientHeight);
    void Destroy();

    HWND Handle() const { return window_; }
    UINT ClientWidth() const { return clientWidth_; }
    UINT ClientHeight() const { return clientHeight_; }
    bool Minimized() const { return minimized_; }

private:
    HWND window_{nullptr};
    HINSTANCE instance_{nullptr};
    UINT clientWidth_{};
    UINT clientHeight_{};
    bool minimized_{};
};
```

Copyを禁止し、HWNDの所有者を一つにします。

## 61. Destroyの冪等性

```cpp
void Win32Window::Destroy()
{
    if (window_ != nullptr)
    {
        DestroyWindow(window_);
        window_ = nullptr;
    }
}
```

すでに`WM_CLOSE`で破棄済みの場合を考慮します。実際には`WM_NCDESTROY`との同期も行います。

## 62. Class登録解除

必要なら最後のWindow破棄後に`UnregisterClassW`します。Windowが残っている状態では解除できません。

## 63. Window Procedureを軽く保つ

Window Procedureは再入される可能性があります。重いResource LoadやGPU待機を直接行わず、状態更新やCommand Queueへ渡します。

## 64. Message PumpとGame Loopの分離

```cpp
bool PumpMessages()
{
    MSG message{};
    while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE))
    {
        if (message.message == WM_QUIT)
            return false;
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    return true;
}
```

Application Loopから読みやすく呼び出します。

## 65. CPU使用率

VSyncなしで空のGame Loopを回すとCPUを使い切ります。Present、Frame Limiter、Waitable Object、非Active時Sleepなどを後章で設計します。

## 66. よくある失敗：GetMessageをGame Loopに使う

MessageがないとBlockし、連続UpdateとRenderが止まります。Gameは通常`PeekMessage`を使います。

## 67. よくある失敗：WM_QUITをDispatchする

`WM_QUIT`はWindow Procedure向けではありません。Loop側で終了します。

## 68. よくある失敗：Window ProcedureでRendererを即Resize

GPU Resource操作の再入、0 Size、多数の再作成が起こります。最新SizeをQueueし、安全なFrame境界で処理します。

## 69. よくある失敗：DefWindowProcへ渡さない

未処理Messageの標準挙動が失われ、Window移動、Size変更、System Menuが壊れます。

## 70. よくある失敗：Client Sizeと外側Sizeを混同

Back Bufferが意図より小さくなります。希望Client RectangleをWindow StyleとDPIで外側Rectangleへ変換します。

## 71. よくある失敗：Pointer寿命

`GWLP_USERDATA`が破棄済みApplicationを指します。Windowより長い寿命を保証し、`WM_NCDESTROY`で解除します。

## 72. Message Loopテスト

- Window作成と表示に成功する。
- Close Buttonで`WM_QUIT`へ到達する。
- Resizeで最新Client Sizeが記録される。
- 最小化で0 Size Resourceを作らない。
- Focus喪失で入力状態をClearする。
- DPI変更で推奨Rectangleを適用する。

## 73. Window Lifetimeテスト

- Create失敗時に部分状態を戻す。
- Destroyを二度呼んでもCrashしない。
- Window破棄後にApplication Pointerを使わない。
- Class登録と解除の順序が正しい。
- SceneやRendererより先にWindowを破棄しない。

## 74. Debug確認表

- [ ] Win32 API失敗時にGetLastErrorを記録する。
- [ ] 受信Messageを必要に応じてTraceできる。
- [ ] Client Size、DPI、Active、Minimizedを表示する。
- [ ] Resize Queueの要求Sizeと適用Sizeを比較できる。
- [ ] WM_CLOSE、WM_DESTROY、WM_QUITの順を確認できる。

## 75. 実装順序

1. `wWinMain`を作る。
2. `WNDCLASSEXW`とProcedureを登録する。
3. Client SizeからWindow Sizeを計算する。
4. `CreateWindowExW`と`ShowWindow`を呼ぶ。
5. `PeekMessageW` Loopを作る。
6. CloseとDestroyを処理する。
7. `GWLP_USERDATA`でApplicationへ転送する。
8. Resize、Minimize、FocusをQueueする。
9. DPI Awarenessと`WM_DPICHANGED`を追加する。

## 76. 完成確認表

- [ ] Unicode版Win32 APIを統一して使う。
- [ ] Window ClassとC++ Classを区別できる。
- [ ] 希望Client Sizeを正しく作れる。
- [ ] Window作成中のMessageを安全に処理する。
- [ ] `PeekMessage`でGame Loopを止めない。
- [ ] `WM_QUIT`をLoop側で処理する。
- [ ] 未処理Messageを`DefWindowProcW`へ渡す。
- [ ] Resizeを安全なRenderer境界へQueueする。
- [ ] 最小化と0 Sizeを処理する。
- [ ] WindowとApplication Pointerの寿命が一致する。

## 77. この章の要点

- Window Classを登録し、`CreateWindowExW`から`HWND`を得ます。
- Window ProcedureはOSからMessageを受け取ります。
- Gameでは非Blockの`PeekMessage`でMessageを処理します。
- `WM_CLOSE`、`WM_DESTROY`、`WM_QUIT`は異なる段階です。
- Client SizeとWindow RectangleをDPI込みで区別します。
- Resizeや重い処理はWindow Procedureから安全な更新境界へ渡します。
- `GWLP_USERDATA`でC++ Instanceへ転送し、寿命を保証します。

## 78. 公式資料

- [Create a window](https://learn.microsoft.com/en-us/windows/win32/learnwin32/creating-a-window)
- [Your first Windows program](https://learn.microsoft.com/en-us/windows/win32/learnwin32/your-first-windows-program)
- [Window messages](https://learn.microsoft.com/en-us/windows/win32/learnwin32/window-messages)
- [Writing the window procedure](https://learn.microsoft.com/en-us/windows/win32/learnwin32/writing-the-window-procedure)
- [About Windows](https://learn.microsoft.com/en-us/windows/win32/winmsg/about-windows)
- [WM_CLOSE](https://learn.microsoft.com/en-us/windows/win32/winmsg/wm-close)
- [AdjustWindowRectExForDpi](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-adjustwindowrectexfordpi)

次章では、DirectX Objectの基礎となるHRESULT、COM、ComPtr、QueryInterface、Debug Layer、Live Object Reportをさらに詳しく扱います。
