# DirectX 11：全体像・Win32・COM・描画Pipeline

この章では、DirectX、Direct3D 11、DXGI、Win32、COM、GPUがそれぞれ何を担当するかを整理します。最初から三角形を表示するCodeだけを暗記せず、一つのFrameがどのObjectを通って画面へ届くかを理解します。

## 1. DirectXとは

DirectX（ダイレクトエックス）は、Windows上でGraphics、Audio、Inputなどを扱うAPI群の総称です。3D描画を担当するAPIがDirect3Dです。

「DirectX 11で描画する」という表現は、多くの場合Direct3D 11とDXGIを使ってGPU描画を行うことを意味します。

## 2. Direct3D 11とは

Direct3D 11は、Buffer、Texture、Shader、Pipeline Stateを作り、GPUへ描画命令を送るGraphics APIです。

```text
C++ Application
 -> Direct3D 11 Runtime
 -> Graphics Driver
 -> GPU
```

ApplicationはGPUの機械語を直接作らず、Direct3D APIを通してDriverへ意図を伝えます。

## 3. DXGIとは

DXGIはDirectX Graphics Infrastructureの略です。

- Graphics Adapterの列挙。
- Monitorに対応するOutputの列挙。
- Swap Chainの作成と管理。
- Back Bufferを画面へPresentする処理。
- Window SizeやFull Screenとの連携。

Direct3Dが「何を描くか」を担当し、DXGIが「どのGPUと画面へどう表示するか」を担当すると考えると整理しやすくなります。

## 4. Win32との関係

Desktop Applicationでは、Direct3Dより先にWin32 Windowを作ります。

```text
WinMain
 -> RegisterClassEx
 -> CreateWindowEx
 -> ShowWindow
 -> Message Loop
 -> Direct3D initialization
 -> Update / Render / Present
```

Swap Chainは描画結果を表示するWindow Handle、つまり`HWND`と結び付きます。

## 5. 一枚のFrameの旅

```text
CPU updates game state
 -> CPU records D3D11 commands through Device Context
 -> Driver translates and schedules commands
 -> GPU executes shaders and rasterization
 -> pixels are written to Back Buffer
 -> Swap Chain presents Back Buffer
 -> Desktop compositor / display shows the image
```

`Draw`を呼んだ瞬間にMonitorへPixelが出るとは限りません。CPU、Driver、GPUは非同期に進みます。

## 6. CPUとGPU

CPUはGame Logic、AI、Command発行を担当し、GPUは大量のVertexやPixelを並列処理します。GPUの完了をCPUが毎Frame待つ設計は性能を失います。

## 7. RuntimeとDriver

Direct3D Runtimeは引数やStateを処理し、Graphics DriverはHardware向けCommandへ変換します。同じApplicationでもGPU VendorやDriverで内部実装は異なります。

## 8. DirectX 11の主要Object

```text
ID3D11Device              ResourceとState Objectを作る
ID3D11DeviceContext       PipelineへStateを設定しCommandを発行する
IDXGISwapChain            Back BufferとPresentを管理する
ID3D11Resource            BufferやTextureの基底概念
ID3D11View                Resourceを特定用途として見る
ID3D11*Shader             Compile済みShaderを表す
ID3D11*State              Blend、Rasterizer、Depthなどを表す
```

## 9. Deviceの意味

Device（デバイス）はGPU能力へ接続されたResource Factoryです。

```cpp
Microsoft::WRL::ComPtr<ID3D11Device> device;
```

Buffer、Texture、Shader、View、State Objectを作成するときに使います。

## 10. Device Contextの意味

Device Context（デバイス・コンテキスト）は、Pipeline Stateを設定し、Draw、Copy、MapなどのCommandを発行します。

```cpp
Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
```

Deviceが「物を作る」、Contextが「物を使って命令する」という分担です。

## 11. Immediate Context

通常最初に使うのはImmediate Contextです。設定したCommandはDriverへ送られる順序へ入ります。

Microsoft公式資料では、DeviceごとにImmediate Contextは一つであり、Contextの多くのMethodを複数Threadから同時に呼ぶ前提ではありません。

## 12. Deferred Context

Deferred ContextはCommand Listを記録し、後からImmediate Contextで実行する仕組みです。最初の三角形に不要なので、基本Pipelineを理解してから扱います。

## 13. Swap Chain

Swap Chain（スワップ・チェーン）は表示用Buffer列を管理します。

```text
Application renders to Back Buffer
 -> Present
 -> displayed buffer changes
 -> next Back Buffer becomes available
```

現代のWindow表示ではFlip Modelを後の章で扱います。

## 14. Back Buffer

Back Bufferは次に表示する画像を書き込むTextureです。Swap Chainから取得し、Render Target Viewを作ってOutput Mergerへ設定します。

## 15. Front Bufferという説明の注意

初心者向けにはFrontとBackの交換と説明されますが、Desktop Window ManagerやFlip Modelを含む実際の表示は単純なMemory Copyだけではありません。概念と内部実装を分けて理解します。

## 16. COMとは

DirectX Interfaceの多くはCOM Objectです。COMはComponent Object Modelの略です。

- Interface Pointerを通してMethodを呼ぶ。
- `QueryInterface`で対応Interfaceを問い合わせる。
- `AddRef`と`Release`で参照数を管理する。
- `HRESULT`で成功・失敗を返す。

## 17. Interfaceと実体

`ID3D11Device*`はC++ Class実体そのものではなく、COM InterfaceへのPointerです。実装はRuntimeやDriver側に隠されています。

## 18. IUnknown

COM Interfaceは基本となる`IUnknown`の契約を持ちます。

```cpp
struct IUnknown
{
    virtual HRESULT QueryInterface(...)=0;
    virtual ULONG AddRef()=0;
    virtual ULONG Release()=0;
};
```

これは概念説明用の省略形です。実際の宣言を自作しません。

## 19. 参照Count

COM Objectは参照を持つ側が`AddRef`し、不要時に`Release`します。最後の参照が解放されるとObjectが破棄されます。

手作業でReleaseを書くと早期Returnや例外で漏れやすいため、Smart Pointerを使います。

## 20. ComPtr

```cpp
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

ComPtr<ID3D11Device> device;
ComPtr<ID3D11DeviceContext> context;
ComPtr<IDXGISwapChain> swapChain;
```

`ComPtr`はScope終了時にReleaseを呼ぶRAII Wrapperです。

## 21. GetとGetAddressOf

```cpp
ID3D11Device* rawDevice = device.Get();

HRESULT result = SomeCreateFunction(device.GetAddressOf());
```

- `Get()`：保持中の生Pointerを借りる。
- `GetAddressOf()`：出力引数としてPointerのAddressを渡す。

既存Objectを保持したまま上書きするAPIでは、必要に応じて`ReleaseAndGetAddressOf()`を検討します。

## 22. HRESULT

多くのWindows APIとDirectX APIは`HRESULT`を返します。

```cpp
HRESULT result = CreateSomething();

if (FAILED(result))
{
    // resultの値と呼び出した処理をLogへ残します。
    return false;
}
```

`result == S_OK`だけで判定せず、`SUCCEEDED`と`FAILED` Macroを使います。

## 23. HRESULTを捨てない

初期化失敗後もNull Pointerを使うと、本当の原因から遠い場所でCrashします。失敗したAPI名、HRESULT、設定値を即座に記録します。

## 24. Feature Level

Feature Levelは利用可能なGPU機能の段階です。Direct3D 11 APIを使っていても、選ばれたFeature Levelによって使える機能が異なります。

```cpp
D3D_FEATURE_LEVEL featureLevel{};
```

API VersionとHardware Capabilityを同一視しません。

## 25. Driver Type

```text
D3D_DRIVER_TYPE_HARDWARE  実GPUを使用する通常経路
D3D_DRIVER_TYPE_WARP      CPU上の高速Software Rasterizer
D3D_DRIVER_TYPE_REFERENCE 正確さ検証向けの低速実装
```

通常はHardwareを使い、Hardwareが使えないTest環境などでWARPを検討します。

## 26. Debug Layer

Debug Buildでは`D3D11_CREATE_DEVICE_DEBUG`を指定すると、不正なAPI使用やStateの問題をDebug Outputへ報告できます。

```cpp
UINT flags = 0;
#if defined(_DEBUG)
flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
```

必要なDebug Componentが入っていない環境ではDebug Device作成が失敗する場合があります。

## 27. Device作成の概形

```cpp
D3D_FEATURE_LEVEL requestedLevels[] =
{
    D3D_FEATURE_LEVEL_11_1,
    D3D_FEATURE_LEVEL_11_0,
    D3D_FEATURE_LEVEL_10_1,
    D3D_FEATURE_LEVEL_10_0
};

HRESULT hr = D3D11CreateDevice(
    nullptr,
    D3D_DRIVER_TYPE_HARDWARE,
    nullptr,
    flags,
    requestedLevels,
    static_cast<UINT>(std::size(requestedLevels)),
    D3D11_SDK_VERSION,
    device.GetAddressOf(),
    &featureLevel,
    context.GetAddressOf());
```

これは役割確認用です。11.1を明示した配列は対応Runtimeがない環境で`E_INVALIDARG`になるため、Fallback戦略を後章で実装します。

## 28. D3D11CreateDeviceAndSwapChain

DeviceとSwap Chainを同時に作る関数もあります。学習では一度理解できますが、DXGI Factoryと新しいSwap Chain Interfaceを明示的に扱う設計も後章で学びます。

## 29. Adapter

Adapter（アダプター）はGraphics HardwareまたはSoftware能力を表すDXGI Objectです。複数GPU環境では統合GPUと独立GPUなど複数候補があります。

## 30. Output

Output（アウトプット）はMonitorなどの表示出力を表します。一つのAdapterに複数Outputが接続される場合があります。

## 31. Factory

DXGI FactoryはAdapter列挙やSwap Chain作成の入口です。

```cpp
ComPtr<IDXGIFactory> factory;
```

後の章ではより新しいFactory Interfaceへ`QueryInterface`して機能を使います。

## 32. QueryInterface

COM Objectが新しいInterfaceを実装しているか問い合わせます。

```cpp
ComPtr<IDXGIDevice> dxgiDevice;
HRESULT hr = device.As(&dxgiDevice);
```

`ComPtr::As`は`QueryInterface`を扱いやすくしたものです。Castで無理に変換しません。

## 33. Resource

ResourceはGPUが利用するMemory領域を表す概念です。

- Buffer：Vertex、Index、Constantなどの配列Data。
- Texture：1D、2D、3Dの画像Data。

Resource作成時に用途、Bind Flag、CPU Access、Formatを定義します。

## 34. View

同じResourceをPipelineでどう使うかを表すObjectがViewです。

```text
RTV  Render Target View
DSV  Depth Stencil View
SRV  Shader Resource View
UAV  Unordered Access View
```

Resourceそのものと用途を分離します。

## 35. Viewが必要な理由

一つのTexture ResourceをRender Targetとして書き込み、後でShader Resourceとして読み取る場合があります。それぞれ別のViewを作り、PipelineへBindingします。

同時に競合する読み書きを設定してはいけません。

## 36. Graphics Pipeline

```text
Input Assembler
 -> Vertex Shader
 -> Hull Shader (optional)
 -> Tessellator (optional)
 -> Domain Shader (optional)
 -> Geometry Shader (optional)
 -> Rasterizer
 -> Pixel Shader
 -> Output Merger
```

最初はInput Assembler、Vertex Shader、Rasterizer、Pixel Shader、Output Mergerを使います。

## 37. Input Assembler

Vertex Buffer、Index Buffer、Input Layout、Primitive Topologyを受け取り、Vertex Shaderへ入力を組み立てます。

## 38. Vertex Shader

Vertex Shaderは各Vertexを処理し、最終的にClip Space位置を出力します。World、View、Projection行列による座標変換が代表例です。

## 39. Rasterizer

RasterizerはTriangleを画面上のFragment候補へ変換し、Cull、Fill Mode、Viewport、Scissorを適用します。

## 40. Pixel Shader

Pixel Shaderは補間された値、Texture、LightingなどからPixelの出力色を計算します。最終表示Pixelと必ず一対一ではなく、DepthやBlendで破棄・合成されます。

## 41. Output Merger

Output MergerはPixel Shader出力をRender Targetへ書き込みます。Depth Stencil TestとBlendもこの段階に関係します。

## 42. Pipeline State

Direct3D 11ではContextへShader、Buffer、Texture、Sampler、State Objectを個別にBindingします。

```cpp
context->IASetVertexBuffers(...);
context->IASetInputLayout(...);
context->VSSetShader(...);
context->PSSetShader(...);
context->OMSetRenderTargets(...);
context->Draw(...);
```

`Draw`はその時点でContextへ設定されているStateを使用します。

## 43. State Machineとして読む

Direct3D 11 Contextは巨大なState Machineと考えられます。前の描画で設定したStateが残るため、RendererはPassごとに必要Stateを明示します。

## 44. Draw Call

Draw CallはGPUへ「現在のPipeline Stateで指定数のVertexまたはIndexを処理せよ」と命令します。C++のLoop一回とGPU処理完了一回が同期するわけではありません。

## 45. Commandの非同期性

CPUが`Draw`を呼び終えても、GPUは以前のFrameを処理している場合があります。GPU ResourceをCPUから即座に書き換える処理には同期とUsage設計が必要です。

## 46. Present

```cpp
HRESULT hr = swapChain->Present(1, 0);
```

第1引数は同期Intervalの例です。表示Mode、VSync、Tearing、Flip Modelの詳細はSwap Chain章で扱います。

## 47. Clear

Frame開始時にはRender TargetとDepth BufferをClearします。

```cpp
const float clearColor[4] = {0.05f, 0.08f, 0.12f, 1.0f};
context->ClearRenderTargetView(renderTargetView.Get(), clearColor);
context->ClearDepthStencilView(depthStencilView.Get(),
    D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
```

DepthのClear値は採用するDepth規約と一致させます。

## 48. 最小Render Loop

```cpp
void Renderer::Render()
{
    BeginFrame();        // Clearと共通Target設定。
    DrawScene();         // Pipeline Stateを設定してDraw。
    DrawUserInterface(); // UI用Stateへ切り替える。
    EndFrame();          // PresentとError確認。
}
```

責務を段階へ分け、`WinMain`へ全APIを書きません。

## 49. Window Resize

Window Size変更時はSwap Chain Bufferに依存するRTV、DSV、Depth Textureなどを解放し、BufferをResizeして作り直します。幅または高さ0の最小化状態を処理します。

## 50. Device Removed

GPU Driver更新、Reset、Hardware問題などでDeviceが失われる可能性があります。`Present`などのHRESULTを確認し、Device Removed Reasonを記録します。

## 51. Debug Object Name

Debug BuildではResourceへ名前を付けるとGraphics Debuggerで追いやすくなります。

```cpp
const char* name = "MainBackBufferRTV";
resource->SetPrivateData(WKPDID_D3DDebugObjectName,
                         static_cast<UINT>(std::strlen(name)), name);
```

Helper関数にまとめ、NullとRelease Buildを安全に扱います。

## 52. Live Object Report

終了時にDebug Interfaceから残存D3D Objectを報告すると、COM参照漏れを発見できます。報告前にContext StateとApplication側ComPtrを適切に解放します。

## 53. DXライブラリとの対応

```text
DxLib_Init                 -> Win32 / Device / Swap Chain初期化の集合
LoadGraph                  -> Texture作成・Upload・SRV作成の集合
DrawGraph                  -> Pipeline設定・Vertex送信・Drawの集合
SetDrawScreen              -> Render Target Binding
ScreenFlip                 -> Swap Chain Present
DeleteGraph / DxLib_End    -> COM Resource解放と終了処理
```

DXライブラリで一つだった関数の内部が、DirectXでは複数責務へ分かれます。

## 54. DirectX 11と12の違いの入口

DirectX 11 RuntimeとDriverは多くのResource状態、Command管理、同期を隠します。DirectX 12ではCommand List、Descriptor、Resource Barrier、FenceなどをApplicationがより明示的に管理します。

まず11で「描画に何が必要か」を理解してから12で「誰が管理するか」を学びます。

## 55. 初期化の所有順

```text
Window
 -> DXGI / D3D Device and Context
 -> Swap Chain
 -> Back Buffer RTV
 -> Depth Texture and DSV
 -> Viewport
 -> Shader / Buffer / Texture resources
```

終了は参照する側から逆順に解放します。

## 56. Rendererが保持する最小Object

```cpp
class D3D11Renderer final
{
private:
    ComPtr<ID3D11Device> device_{};
    ComPtr<ID3D11DeviceContext> context_{};
    ComPtr<IDXGISwapChain> swapChain_{};
    ComPtr<ID3D11RenderTargetView> renderTargetView_{};
    ComPtr<ID3D11Texture2D> depthTexture_{};
    ComPtr<ID3D11DepthStencilView> depthStencilView_{};
    D3D11_VIEWPORT viewport_{};
    D3D_FEATURE_LEVEL featureLevel_{};
};
```

後章でFactory、Adapter、Debug Interface、新しいDXGI Interfaceを追加します。

## 57. 初期化結果

```cpp
enum class RendererInitError
{
    WindowHandleInvalid,
    DeviceCreationFailed,
    SwapChainCreationFailed,
    BackBufferCreationFailed,
    DepthBufferCreationFailed
};

struct RendererInitResult final
{
    std::optional<RendererInitError> error{};
    HRESULT hresult{S_OK};
    explicit operator bool() const { return !error; }
};
```

単なるfalseでは原因を説明できません。

## 58. Threadingの基本

DeviceによるResource作成とDevice ContextによるCommand発行はThread安全性の性質が異なります。最初はImmediate ContextをRender Thread一つから使い、必要性を計測してから並列化します。

## 59. よくある失敗：生PointerのRelease漏れ

早期Returnや再初期化でCOM参照が残ります。`ComPtr`を標準にし、所有しない借用Pointerだけを短時間使います。

## 60. よくある失敗：HRESULT無視

Device作成失敗後にSwap Chainを使い、Null参照でCrashします。各境界で失敗を返し、初期化を中止します。

## 61. よくある失敗：DeviceとContextを混同

Buffer作成はDevice、Pipeline BindingとDrawはContextです。名前も`device`と`context`で区別します。

## 62. よくある失敗：Stateが自動Resetされると思う

前DrawのShader、Texture、Blend Stateが残ります。Passが必要なStateを明示的に設定します。

## 63. よくある失敗：CPUとGPUが同期していると思う

Draw直後にGPU完了を前提としたResource変更を行います。Dynamic Resource、Map方式、複数Buffer、Query、Fence相当の考え方を後章で学びます。

## 64. Debug確認表

- [ ] Debug BuildでDebug Layerを有効化する。
- [ ] すべてのHRESULTを確認する。
- [ ] ObjectへDebug Nameを付ける。
- [ ] Frame CaptureでPipeline Stateを見る。
- [ ] 終了時にLive Objectを確認する。
- [ ] PresentとDevice Removedを記録する。

## 65. 理解確認

- DirectXとDirect3Dはどう違うか。
- DXGIは何を担当するか。
- DeviceとDevice Contextはどう違うか。
- ResourceとViewはなぜ分かれているか。
- Draw時に使われるStateはいつ設定されたものか。
- Presentは何をするか。
- COM ObjectをComPtrで持つ理由は何か。
- Feature LevelとAPI Versionはどう違うか。

## 66. この章の要点

- Win32 Windowが表示先、DXGIがAdapterとSwap Chain、D3D11がGPU描画を担当します。
- DeviceはResourceを作り、Device ContextはPipeline StateとCommandを扱います。
- COM Interfaceは参照Countを持つため、ComPtrでRAII管理します。
- ResourceとViewを分け、同じTextureを用途別にPipelineへBindingします。
- Drawは現在のPipeline Stateを使うCommandであり、GPU完了を意味しません。
- Debug Layer、HRESULT、Debug Name、Live Object Reportを最初から利用します。
- DirectX 11で隠されている管理を理解した後、DirectX 12で明示化します。

## 67. 公式資料

- [Introduction to a Device in Direct3D 11](https://learn.microsoft.com/en-us/windows/win32/direct3d11/overviews-direct3d-11-devices-intro)
- [D3D11CreateDeviceAndSwapChain](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-d3d11createdeviceandswapchain)
- [Using the debug layer to debug apps](https://learn.microsoft.com/en-us/windows/win32/direct3d11/using-the-debug-layer-to-test-apps)
- [DXGI overview](https://learn.microsoft.com/en-us/windows/win32/direct3ddxgi/d3d10-graphics-programming-guide-dxgi)
- [How to create a Swap Chain](https://learn.microsoft.com/en-us/windows/win32/direct3d11/overviews-direct3d-11-devices-create-swap-chain)

次章では、Visual Studio、Windows SDK、Header、Library、Build構成を整え、最小ProjectがなぜLinkできるかまで確認します。
