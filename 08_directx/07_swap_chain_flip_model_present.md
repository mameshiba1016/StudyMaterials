# DirectX 11：Swap Chain・Flip Model・Present

この章では、Direct3D 11で描いた画像をWin32 Windowへ表示するためのDXGI Swap Chainを学びます。作成APIの引数を読むところから、Flip Model、VSync、Tearing、Frame Latency、Occlusion、Resize、Fullscreen、Device Removedまでを一続きの処理として理解します。

## 1. Swap Chainとは

Swap Chainは、画面表示に使う複数のBufferと、その表示順序をDXGIへ管理させる仕組みです。

```text
Game renders -> Back Buffer -> Present -> Desktop Window Manager / Display
```

単なるTexture配列ではなく、Window、表示方式、同期、色Formatなどを含む表示契約です。

## 2. なぜ複数Bufferが必要か

表示中の画像へ直接描くと、描画途中の内容が見えてしまいます。描画用Back Bufferを別に持ち、完成後に表示へ渡すことで破綻を防ぎます。

## 3. Front BufferとBack Buffer

- Front Buffer：現在表示に使われる画像という概念。
- Back Buffer：次のFrameを描く画像。
- Flip Model：Bufferの内容を複製するより、表示対象の役割を切り替える方式。

Desktop Composition下では実装詳細が抽象化されるため、自分でFront Bufferを直接操作する考え方はしません。

## 4. DXGIが担当する範囲

```text
Direct3D 11: Resource作成、Pipeline設定、Draw
DXGI       : Adapter、Output、Swap Chain、Present
Win32      : HWND、Window size、Message
```

三者の責任を分けて読むと初期化Codeを理解しやすくなります。

## 5. 使用するCOM Interface

```cpp
Microsoft::WRL::ComPtr<IDXGIFactory2> factory;
Microsoft::WRL::ComPtr<IDXGISwapChain1> swapChain;
```

`IDXGISwapChain1`は`CreateSwapChainForHwnd`で作る現代的なWindow向けInterfaceです。必要なら`As`で後続Versionを問い合わせます。

## 6. 推奨する作成経路

```text
Adapter -> D3D11 Device
Adapter -> Parent Factory2
Factory2 + Device + HWND + DESC1 -> SwapChain1
```

Deviceを作ったAdapterと同じ系統のFactoryを使うと、選択の一貫性を保てます。

## 7. DeviceからDXGI Deviceを得る

```cpp
Microsoft::WRL::ComPtr<IDXGIDevice> dxgiDevice;
ThrowIfFailed(device.As(&dxgiDevice));
```

`As`はCOMの`QueryInterface`を安全に行うWRLの操作です。Textureへ変換しているのではなく、同一Objectが公開する別Interfaceを得ています。

## 8. AdapterとFactoryをたどる

```cpp
Microsoft::WRL::ComPtr<IDXGIAdapter> adapter;
ThrowIfFailed(dxgiDevice->GetAdapter(&adapter));

Microsoft::WRL::ComPtr<IDXGIFactory2> factory;
ThrowIfFailed(adapter->GetParent(IID_PPV_ARGS(&factory)));
```

`GetParent`で、そのAdapterを列挙したFactoryへ戻ります。

## 9. DXGI_SWAP_CHAIN_DESC1

```cpp
DXGI_SWAP_CHAIN_DESC1 desc{};
desc.Width = 0;
desc.Height = 0;
desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
desc.Stereo = FALSE;
desc.SampleDesc = {1, 0};
desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
desc.BufferCount = 2;
desc.Scaling = DXGI_SCALING_STRETCH;
desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
desc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
desc.Flags = 0;
```

ゼロ初期化して全重要Fieldを明示すると、未初期化値による不具合を防げます。

## 10. WidthとHeight

`0`を指定すると、`CreateSwapChainForHwnd`では対象WindowのClient Area寸法が使われます。明示値を使う設計でも、実際のBack Buffer寸法を作成後に記録します。

## 11. Client AreaとWindow全体

Title BarやBorderを含むWindow Rectangleと、描画領域であるClient Rectangleは異なります。

```cpp
RECT client{};
GetClientRect(hwnd, &client);
const UINT width = static_cast<UINT>(client.right - client.left);
const UINT height = static_cast<UINT>(client.bottom - client.top);
```

## 12. Format

一般的なSDR表示では`DXGI_FORMAT_R8G8B8A8_UNORM`などを使います。色空間、HDR、sRGB解釈はFormat名だけで決めず、Swap ChainのColor SpaceとShader側の変換を含めて設計します。

## 13. SampleDesc

Flip ModelのSwap Chain BufferはMultisampleにしません。

```cpp
desc.SampleDesc.Count = 1;
desc.SampleDesc.Quality = 0;
```

MSAAが必要なら別のMultisample Render Targetへ描き、単一SampleのBack BufferへResolveします。

## 14. BufferUsage

```cpp
desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
```

Back BufferをRender Targetとして使う意思を示します。別用途が必要な場合だけFlagを追加し、最小権限にします。

## 15. BufferCount

Flip Modelでは2以上を指定します。まず2 Bufferから始め、LatencyやFrame pacingを計測して必要なら増やします。

Buffer数を増やせば必ず速くなるわけではなく、入力から表示までの待ちが増える可能性があります。

## 16. Scaling

`DXGI_SCALING_STRETCH`は出力領域へ伸縮します。Resize時に同じ解像度でBack Bufferを再作成するなら、通常は目立つ拡大縮小を長時間残しません。

## 17. AlphaMode

通常の不透明なHWND Windowでは`DXGI_ALPHA_MODE_IGNORE`を使います。透明合成が必要なComposition Swap Chainとは設計が異なります。

## 18. Swap Effectの種類

- `DISCARD`：従来のBlt Model。
- `SEQUENTIAL`：従来型で内容順序を保つ。
- `FLIP_SEQUENTIAL`：Flip Modelで内容を保持する。
- `FLIP_DISCARD`：Flip ModelでPresent後の内容保持を要求しない。

新しいDesktop Applicationでは、原則としてFlip Modelを検討します。

## 19. Flip Discardを基本にする理由

Flip ModelはDesktop Window Managerとの連携、効率、現代的な表示機能との互換性で有利です。`FLIP_DISCARD`はPresent後の古い内容を再利用しない一般的なGame描画に合います。

## 20. Present後の内容は未定義として扱う

Flip Discardでは、Present後もBack Bufferに前Frameが残ると仮定しません。毎Frame必要なPixelを描画またはClearします。

## 21. CreateSwapChainForHwnd

```cpp
ThrowIfFailed(factory->CreateSwapChainForHwnd(
    device.Get(),
    hwnd,
    &desc,
    nullptr,
    nullptr,
    swapChain.GetAddressOf()));
```

第1引数にはDirect3D Deviceを渡します。COM Objectの型と寿命が有効である必要があります。

## 22. Fullscreen Description

第4引数の`DXGI_SWAP_CHAIN_FULLSCREEN_DESC`はWindowed Swap Chainなら`nullptr`にできます。まずWindowed Modeを安定させるのが基本です。

## 23. Restrict To Output

第5引数は表示先Outputを制限する高度な用途です。通常は`nullptr`にしてDXGIへ任せます。

## 24. 作成直後の確認

```cpp
DXGI_SWAP_CHAIN_DESC1 actual{};
ThrowIfFailed(swapChain->GetDesc1(&actual));
```

実際のFormat、Buffer Count、Swap Effect、FlagsをLogへ残すと環境差の診断に役立ちます。

## 25. Alt+Enterの既定動作を止める

```cpp
ThrowIfFailed(factory->MakeWindowAssociation(
    hwnd,
    DXGI_MWA_NO_ALT_ENTER));
```

Fullscreen切替をGame側で管理するなら、DXGIの自動Alt+Enterを無効にします。

## 26. Window Associationの注意

`MakeWindowAssociation`はWindowとFactoryの関連付けです。入力処理を無効化するAPIではありません。Fullscreen Policyの所有者を明確にする目的で使います。

## 27. IDXGISwapChainとIDXGISwapChain1

`IDXGISwapChain1`は基底Interfaceの機能も持ちます。`Present`は基底側、`Present1`は追加情報を渡せる拡張です。

## 28. Presentの基本形

```cpp
HRESULT hr = swapChain->Present(1, 0);
```

第1引数はSync Interval、第2引数はPresent Flagです。戻り値を無視してはいけません。

## 29. Sync Interval 1

```cpp
swapChain->Present(1, 0);
```

一般的なVSync設定です。表示更新境界へ同期するためTearingを避けやすい一方、待機とLatencyが生じます。

## 30. Sync Interval 0

```cpp
swapChain->Present(0, presentFlags);
```

垂直同期を待たずにPresentします。Tearing許可条件を満たす構成では`ALLOW_TEARING`と組み合わせます。

## 31. VSyncはFrame Rate固定機能ではない

VSyncは表示境界との同期方針です。Game LoopのSimulation刻み、CPU/GPU負荷、Monitor周波数まで自動的に一定にするものではありません。

## 32. Tearingとは

一回の画面走査中に表示対象が切り替わり、上下で異なるFrameが見える現象です。低Latencyと引き換えに許容する場合があります。

## 33. Tearing Supportの問い合わせ

```cpp
BOOL allowTearing = FALSE;
HRESULT hr = factory->CheckFeatureSupport(
    DXGI_FEATURE_PRESENT_ALLOW_TEARING,
    &allowTearing,
    sizeof(allowTearing));

const bool tearingSupported = SUCCEEDED(hr) && allowTearing == TRUE;
```

OSや名前から推測せず、Factoryへ問い合わせます。

## 34. Swap Chain作成時のTearing Flag

```cpp
if (tearingSupported)
{
    desc.Flags |= DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
}
```

Present時だけでなく、Swap Chain作成時にも許可Flagが必要です。

## 35. Present時のTearing Flag

```cpp
UINT flags = 0;
if (!vsyncEnabled && tearingSupported && windowed)
{
    flags |= DXGI_PRESENT_ALLOW_TEARING;
}

HRESULT hr = swapChain->Present(vsyncEnabled ? 1u : 0u, flags);
```

`ALLOW_TEARING`はSync Interval 0で使います。

## 36. TearingとFullscreen

Tearing Flagの利用条件は表示Modeによって異なります。Windowed/Borderless Windowを中心に設計し、Exclusive Fullscreenと同じ条件だと決めつけません。

## 37. Borderless Fullscreen

Window Styleと位置をMonitor全体へ合わせる方式です。Swap Chain自体はWindowedのまま扱え、切替が比較的安定します。

## 38. Exclusive Fullscreen

`SetFullscreenState`を使う排他的Modeです。Mode切替、Focus喪失、複数Monitor、復帰処理が複雑になるため、必要性を確認して採用します。

## 39. PresentのHRESULT分類

```cpp
HRESULT hr = swapChain->Present(syncInterval, flags);

if (hr == DXGI_STATUS_OCCLUDED)
{
    // Windowが見えない。描画頻度を落とす。
}
else if (hr == DXGI_ERROR_DEVICE_REMOVED ||
         hr == DXGI_ERROR_DEVICE_RESET)
{
    // Device再作成経路へ移る。
}
else
{
    ThrowIfFailed(hr);
}
```

成功/失敗の二値だけでなく、状態変化として分類します。

## 40. DXGI_STATUS_OCCLUDED

Windowが他Windowに完全に隠れた、最小化されたなど、表示結果が見えない状態を示します。致命的Errorとして終了せず、CPU/GPU使用量を抑えます。

## 41. Occlusion確認Present

```cpp
HRESULT hr = swapChain->Present(0, DXGI_PRESENT_TEST);
```

`PRESENT_TEST`は実表示せず状態確認に使えます。一定間隔で試し、表示可能になったら通常描画へ戻します。

## 42. 最小化中の扱い

`WM_SIZE`の`SIZE_MINIMIZED`を記録し、0×0のBack Bufferを作ろうとしません。描画を休止し、Message処理は続けます。

## 43. Device Removed

```cpp
HRESULT reason = device->GetDeviceRemovedReason();
```

PresentでDevice Removed/Resetを受けたら理由をLogへ残し、Device、Context、Swap Chain、それらに属するResourceを再作成します。

## 44. PresentとCPU/GPU並列性

CPUはCommandを記録し、GPUは以前のFrameを実行し、Displayはさらに別のFrameを表示できます。このQueueがThroughputを上げる一方、入力Latencyも増やします。

## 45. Frame Latency

Frame LatencyはCPUが表示より何Frame先行できるかに関係します。Buffer Countだけで決まらず、DXGI Queue、Driver、同期方針も影響します。

## 46. IDXGISwapChain2

```cpp
Microsoft::WRL::ComPtr<IDXGISwapChain2> swapChain2;
if (SUCCEEDED(swapChain.As(&swapChain2)))
{
    ThrowIfFailed(swapChain2->SetMaximumFrameLatency(1));
}
```

低Latency化に役立つ場合がありますが、値を小さくすれば常に滑らかになるわけではありません。

## 47. Frame Latency Waitable Object

Swap Chain作成時に`DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT`を指定し、`IDXGISwapChain2::GetFrameLatencyWaitableObject`から待機Handleを得る方式があります。

## 48. Waitable Objectの概念

```text
wait until DXGI accepts next frame
-> process input / simulate
-> record and submit draw
-> Present
```

無制限にCPUを先行させず、Frame開始Timingを制御できます。

## 49. Handleの寿命

Waitable HandleはSwap Chainの寿命と結び付きます。自分で無関係なHandleとして閉じたり、Swap Chain破棄後に待機したりしない設計にします。

## 50. Present1

```cpp
DXGI_PRESENT_PARAMETERS parameters{};
HRESULT hr = swapChain->Present1(syncInterval, flags, &parameters);
```

Dirty RectangleやScroll情報を渡せます。毎Frame全面更新するGameでは空のParameterから始められます。

## 51. Dirty Rectangle

変化した矩形だけをCompositionへ通知する最適化です。3D Gameのように画面全体が変わる場合は管理Costに見合わないことがあります。

## 52. Back Buffer Index

DirectX 11の典型的なSwap Chainでは`GetBuffer(0)`から現在描画対象を得ます。DirectX 12のようにCurrent Back Buffer Indexを自前で回す設計と混同しません。

## 53. Present前の基本順序

```text
Clear / Draw
-> optional Resolve / Post Process
-> unbind conflicting resources if needed
-> Present
-> classify HRESULT
```

PresentはDraw Commandそのものではなく、完成した表示画像をDXGIへ渡す境界です。

## 54. PresentはGPU完了待ちではない

Presentが返ったことだけで、そのFrameのGPU処理が完全終了したとは限りません。ResourceをCPUで再利用する同期にはQueryなど別の仕組みが必要です。

## 55. Flushとの違い

`ID3D11DeviceContext::Flush`は保留Commandの送信を促します。Present、VSync、GPU完了Fenceの代用品ではありません。

## 56. Resizeとの関係

Window sizeが変わったら、Back Bufferを参照するRTVなどを解放し、`ResizeBuffers`を呼び、新Back BufferからViewを再作成します。詳細は次章以降で扱います。

## 57. Resize中にPresentし続ける問題

Interactive Resize中は大量の`WM_SIZE`が届きます。毎回即時再作成するか、Resize終了時にまとめるかをPolicy化します。

## 58. DPIとの関係

Windowの論理Sizeと実Pixel SizeはDPI設定で異なる場合があります。Swap Chain寸法は描画対象Pixelとして管理し、UI Scaleとは分離します。

## 59. Refresh Rateを固定値と決めつけない

60 Hzだけを前提にしません。高Refresh Rate、可変Refresh Rate、Monitor移動を考慮し、SimulationをPresent回数へ直結させない設計にします。

## 60. Game Loopとの統合例

```cpp
while (running)
{
    PumpWindowMessages();

    if (minimized || occluded)
    {
        WaitForEventsOrTestVisibility();
        continue;
    }

    UpdateSimulation();
    RenderFrame();
    PresentFrame();
}
```

非表示中にもBusy Loopしないことが重要です。

## 61. Present設定を構造体へ集約

```cpp
struct PresentationSettings
{
    bool vsync = true;
    bool tearingSupported = false;
    bool windowed = true;
};
```

UI設定とHardware能力を分け、最終Flagを一か所で導出します。

## 62. Present関数の例

```cpp
HRESULT PresentFrame(
    IDXGISwapChain1& swapChain,
    const PresentationSettings& settings)
{
    const UINT syncInterval = settings.vsync ? 1u : 0u;

    UINT flags = 0;
    if (!settings.vsync &&
        settings.tearingSupported &&
        settings.windowed)
    {
        flags |= DXGI_PRESENT_ALLOW_TEARING;
    }

    return swapChain.Present(syncInterval, flags);
}
```

この関数の呼出側でHRESULTを分類します。

## 63. 初期化関数の責任

```text
Validate HWND
-> obtain Factory
-> query tearing capability
-> build DESC1
-> create Swap Chain
-> disable automatic Alt+Enter
-> query optional newer interfaces
-> log actual configuration
```

途中失敗時は不完全なObjectを公開しません。

## 64. Swap Chain所有Class

```cpp
class PresentationSurface
{
public:
    void Create(ID3D11Device& device, HWND hwnd);
    HRESULT Present(bool vsync);
    void Resize(UINT width, UINT height);
    void Reset();

private:
    Microsoft::WRL::ComPtr<IDXGISwapChain1> swapChain_;
    bool tearingSupported_ = false;
    bool minimized_ = false;
};
```

Device全体と表示面の責任を分けると、ResizeやDevice再作成を整理できます。

## 65. Thread Ownership

Swap Chain操作とImmediate Context描画をRender Threadへ集約すると順序が明確になります。Window Messageから直接Resizeせず、要求をRender Threadへ渡す設計も有効です。

## 66. Debug Name

Swap Chain自体へD3D11 Resourceと同じDebug Nameを付けられない場合でも、Back BufferやRTVへ名前を付け、Window IDと設定をLogに残します。

## 67. よくある失敗：旧CreateSwapChainを無条件使用

古いSampleをそのまま写し、Blt Modelを選ぶ失敗です。対象OS要件を確認し、`CreateSwapChainForHwnd`とFlip Modelを基本候補にします。

## 68. よくある失敗：MSAA Swap Chain

Flip ModelのBack BufferへMSAA Countを直接設定します。MSAA用TextureとResolve先Back Bufferを分けます。

## 69. よくある失敗：Tearing Flag片側だけ

作成時Flagだけ、またはPresent時Flagだけを設定します。Capability確認、作成Flag、Sync Interval 0、Present Flag、Windowed条件を一組で扱います。

## 70. よくある失敗：Present戻り値を捨てる

Device RemovedやOccludedを見逃し、画面停止後も描画し続けます。戻り値を状態遷移へ変換します。

## 71. よくある失敗：最小化時に0×0 Resize

最小化Messageを通常Resizeとして処理します。Minimized状態を記録し、有効なClient Sizeへ戻ってから再作成します。

## 72. よくある失敗：Buffer数でLatencyを断定

二重Bufferなら必ず低Latency、三重なら必ず滑らかと断定します。CPU/GPU時間、Present Queue、VSync、Waitable Objectを計測します。

## 73. 作成テスト

- Flip Discard、Buffer Count 2、Sample Count 1で作れる。
- 実際のDescriptorを取得して期待値と比較する。
- Tearing非対応環境でも正常作成できる。
- 無効なHWNDで明確に失敗する。
- Optional Interface非対応でもCrashしない。

## 74. Presentテスト

- VSync ONでSync Interval 1になる。
- VSync OFFかつ対応時だけTearing Flagを使う。
- Occluded時に描画頻度を落とす。
- Device Removedを再作成経路へ渡す。
- Present失敗をLogに残す。

## 75. Window状態テスト

- 最小化から復帰できる。
- Resizeを連続しても破綻しない。
- Monitor間移動に耐える。
- DPI変更後も正しいPixel寸法になる。
- Alt+Enter Policyが二重管理されない。

## 76. Frame pacing計測

```text
CPU update time
CPU render submission time
Present call duration
GPU frame time
display interval
input-to-photon latency if measurable
```

平均FPSだけでなくFrame timeの揺れを観察します。

## 77. 完成確認表

- [ ] Swap Chainの役割を説明できる。
- [ ] Flip ModelとBlt Modelを区別できる。
- [ ] DESC1の各Fieldを説明できる。
- [ ] Flip ModelでSample Count 1を使う理由を説明できる。
- [ ] VSyncとFrame Rate制御を区別できる。
- [ ] Tearingの対応確認と二つのFlagを設定できる。
- [ ] PresentのHRESULTを分類できる。
- [ ] Occluded、Minimized、Device Removedを状態として扱える。
- [ ] Frame LatencyをBuffer Countだけで断定しない。
- [ ] Resize前後のBack Buffer参照寿命を理解している。

## 78. この章の要点

- Swap Chainは表示用BufferとPresent Policyを管理します。
- 現代的なDesktop描画ではFlip Modelを基本候補にします。
- Flip ModelのBack Bufferは単一Sampleにし、MSAAは別Textureで処理します。
- VSync、Tearing、Frame Latencyは別々の概念です。
- Tearingは能力確認、作成Flag、Present Flag、Sync Intervalの全条件をそろえます。
- Presentの戻り値はOcclusion、Device喪失、一般Errorへ分類します。
- 最小化中や非表示中は描画を休止し、無駄なBusy Loopを避けます。
- 表示設定は計測可能なPolicyとして一か所に集約します。

## 79. 公式資料

- [For best performance, use DXGI flip model](https://learn.microsoft.com/en-us/windows/win32/direct3ddxgi/for-best-performance--use-dxgi-flip-model)
- [DXGI flip model](https://learn.microsoft.com/en-us/windows/win32/direct3ddxgi/dxgi-flip-model)
- [IDXGIFactory2::CreateSwapChainForHwnd](https://learn.microsoft.com/en-us/windows/win32/api/dxgi1_2/nf-dxgi1_2-idxgifactory2-createswapchainforhwnd)
- [DXGI_SWAP_CHAIN_DESC1](https://learn.microsoft.com/en-us/windows/win32/api/dxgi1_2/ns-dxgi1_2-dxgi_swap_chain_desc1)
- [IDXGISwapChain::Present](https://learn.microsoft.com/en-us/windows/win32/api/dxgi/nf-dxgi-idxgiswapchain-present)
- [IDXGISwapChain1::Present1](https://learn.microsoft.com/en-us/windows/win32/api/dxgi1_2/nf-dxgi1_2-idxgiswapchain1-present1)
- [Variable refresh rate displays](https://learn.microsoft.com/en-us/windows/win32/direct3ddxgi/variable-refresh-rate-displays)
- [IDXGIFactory5::CheckFeatureSupport](https://learn.microsoft.com/en-us/windows/win32/api/dxgi1_5/nf-dxgi1_5-idxgifactory5-checkfeaturesupport)
- [IDXGISwapChain2::SetMaximumFrameLatency](https://learn.microsoft.com/en-us/windows/win32/api/dxgi1_3/nf-dxgi1_3-idxgiswapchain2-setmaximumframelatency)
- [IDXGISwapChain2::GetFrameLatencyWaitableObject](https://learn.microsoft.com/en-us/windows/win32/api/dxgi1_3/nf-dxgi1_3-idxgiswapchain2-getframelatencywaitableobject)

次章では、Swap ChainからBack Buffer Textureを取得し、Render Target Viewを作成してOutput MergerへBindingする手順を扱います。
