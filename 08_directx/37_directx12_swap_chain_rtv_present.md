# DirectX 12 第5章：Swap Chain・RTV・Present

この章では、GPUの描画結果をWindowへ表示するまでを実装します。DXGI Swap Chain、Flip Model、Back Buffer、RTV Descriptor Heap、Resource Barrier、Clear、Present、VSync、Tearing、Frame Latency、Resizeを扱います。

## 1. この章の完成状態

```text
Window
 -> Swap Chain生成
 -> Back Buffer取得
 -> RTV生成
 -> PRESENTからRENDER_TARGETへTransition
 -> Clear
 -> PRESENTへ戻す
 -> Execute
 -> Present
 -> Fenceで再利用管理
```

## 2. Swap Chainとは

表示用Bufferを複数持ち、描画済みBufferをDesktop Composition/Displayへ渡すDXGI Objectです。

## 3. Back Buffer

Swap Chainが所有する表示候補Textureです。Applicationは`GetBuffer`でInterfaceを取得しRTVを作ります。

## 4. Front Bufferを直接描かない

描画中の途中画像を表示せず、完成したBack BufferをPresentします。

## 5. D3D12ではQueueを渡す

Swap Chain生成時、D3D11 DeviceではなくD3D12 Direct Command Queueを渡します。

## 6. Swap Chain Config

```cpp
struct SwapChainConfig
{
    UINT width = 1280;
    UINT height = 720;
    UINT bufferCount = 3;
    DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM;
    bool allowTearing = false;
    bool useFrameLatencyWaitableObject = false;
};
```

## 7. Client Size

Window全体ではなくClient AreaのPixel Sizeを使います。DPIと最小化時の0x0を考慮します。

## 8. DXGI_SWAP_CHAIN_DESC1

```cpp
DXGI_SWAP_CHAIN_DESC1 desc{};
desc.Width = config.width;
desc.Height = config.height;
desc.Format = config.format;
desc.Stereo = FALSE;
desc.SampleDesc.Count = 1;
desc.SampleDesc.Quality = 0;
desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
desc.BufferCount = config.bufferCount;
desc.Scaling = DXGI_SCALING_STRETCH;
desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
desc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
desc.Flags = swapChainFlags;
```

## 9. Flip Model

現代のWindowed Swap ChainではFlip Modelを使用します。`FLIP_DISCARD`または要件に応じて`FLIP_SEQUENTIAL`を選びます。

## 10. Flip Discard

Present後のBack Buffer内容を保持する前提にしません。毎Frame必要領域を描画/Clearします。

## 11. Flip Sequential

Buffer内容保持の挙動が必要な場合に検討します。通常のReal-time RenderingではFlip Discardが一般的です。

## 12. Sample Countは1

Flip Model Swap Chain Bufferは通常MSAA Textureにしません。MSAA用別Render Targetへ描画し、Back BufferへResolveします。

## 13. Buffer Count

Double/Triple Buffering等を選びます。Memory、Latency、CPU/GPUのOverlapへ影響します。

## 14. Format

SDRでは`R8G8B8A8_UNORM`等を使えます。HDRではSwap Chain FormatとColor Spaceを別途設計します。

## 15. SRGB View

Swap Chain Resource FormatとRTV View Format、Shader出力のColor Spaceを整理します。UNORM/SRGBを名前だけで混同しません。

## 16. Swap Chain Flags

```cpp
UINT swapChainFlags = 0;

if (config.allowTearing)
    swapChainFlags |= DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;

if (config.useFrameLatencyWaitableObject)
    swapChainFlags |= DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;
```

作成・Resize・PresentのFlagを整合させます。

## 17. Tearing Support確認

```cpp
BOOL allowTearing = FALSE;
ComPtr<IDXGIFactory5> factory5;

if (SUCCEEDED(factory.As(&factory5)))
{
    factory5->CheckFeatureSupport(
        DXGI_FEATURE_PRESENT_ALLOW_TEARING,
        &allowTearing,
        sizeof(allowTearing));
}
```

戻り値も確認してSupportを決めます。

## 18. SupportなしFallback

設定でTearing希望でもSystem非対応ならFlagを外し、VSync/Present Policyを安全側へ変更します。

## 19. CreateSwapChainForHwnd

```cpp
ComPtr<IDXGISwapChain1> swapChain1;
ThrowIfFailed(
    factory->CreateSwapChainForHwnd(
        directQueue,
        hwnd,
        &desc,
        nullptr,
        nullptr,
        &swapChain1),
    "IDXGIFactory2::CreateSwapChainForHwnd");
```

第一引数はDirect Command Queueです。

## 20. Fullscreen Desc

Windowed/Borderlessを基本にするなら`pFullscreenDesc`を`nullptr`にできます。Exclusive Fullscreenは別の状態管理が必要です。

## 21. Restrict To Output

特定OutputへContentを制限する特殊要件がなければ`nullptr`です。通常のMonitor選択とは意味が異なります。

## 22. IDXGISwapChain3

```cpp
ComPtr<IDXGISwapChain3> swapChain;
ThrowIfFailed(swapChain1.As(&swapChain), "Query IDXGISwapChain3");
```

`GetCurrentBackBufferIndex`等を使用できます。

## 23. Alt+Enter管理

```cpp
ThrowIfFailed(
    factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER),
    "IDXGIFactory::MakeWindowAssociation");
```

Fullscreen切替をApplication側で一貫管理します。

## 24. Factory Current確認

Adapter/Display構成変更時にFactoryが古くなっていないか`IsCurrent`を利用できる場合があります。再列挙Policyへ接続します。

## 25. RTV Descriptor Heap

Back Buffer数ぶんRTV Descriptor Slotを用意します。

## 26. RTV Heap Description

```cpp
D3D12_DESCRIPTOR_HEAP_DESC heapDesc{};
heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
heapDesc.NumDescriptors = config.bufferCount;
heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
heapDesc.NodeMask = 0;
```

RTV HeapはShader-visibleにしません。

## 27. CreateDescriptorHeap

```cpp
ComPtr<ID3D12DescriptorHeap> rtvHeap;
ThrowIfFailed(
    device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&rtvHeap)),
    "Create RTV Descriptor Heap");

rtvHeap->SetName(L"Swap Chain RTV Heap");
```

## 28. RTV Increment Size

```cpp
const UINT rtvIncrement =
    device->GetDescriptorHandleIncrementSize(
        D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
```

固定値や`sizeof`を使いません。

## 29. Heap Start Handle

```cpp
D3D12_CPU_DESCRIPTOR_HANDLE handle =
    rtvHeap->GetCPUDescriptorHandleForHeapStart();
```

CPU Descriptor Handleを通常PointerとしてDereferenceしません。

## 30. Back Buffer配列

```cpp
std::vector<ComPtr<ID3D12Resource>> backBuffers(config.bufferCount);
std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> rtvHandles(config.bufferCount);
```

Buffer IndexでResourceとRTVを対応させます。

## 31. GetBuffer

```cpp
for (UINT i = 0; i < config.bufferCount; ++i)
{
    ThrowIfFailed(
        swapChain->GetBuffer(i, IID_PPV_ARGS(&backBuffers[i])),
        "IDXGISwapChain::GetBuffer");

    rtvHandles[i] = handle;
    device->CreateRenderTargetView(backBuffers[i].Get(), nullptr, handle);
    handle.ptr += rtvIncrement;
}
```

## 32. CreateRenderTargetViewの戻り値

`CreateRenderTargetView`は`void`です。Debug Layer、Descriptor/Resource設定、Device Removedを別地点で診断します。

## 33. Default RTV Description

`nullptr`を渡すとResource Formatに基づくDefault Viewを作ります。異なるView Formatが必要なら明示Descを使います。

## 34. Back Buffer Name

```cpp
backBuffers[i]->SetName(
    (L"Swap Chain Back Buffer " + std::to_wstring(i)).c_str());
```

CaptureでIndexを識別できます。

## 35. 初期Resource State

Swap Chain Back Bufferは通常`D3D12_RESOURCE_STATE_PRESENT`として扱います。

## 36. Current Back Buffer Index

```cpp
const UINT index = swapChain->GetCurrentBackBufferIndex();
```

自前で単純加算したIndexと一致するとは仮定しません。

## 37. Current Resource/RTV

```cpp
ID3D12Resource* backBuffer = backBuffers[index].Get();
const D3D12_CPU_DESCRIPTOR_HANDLE rtv = rtvHandles[index];
```

## 38. PRESENTからRTVへTransition

```cpp
auto toRenderTarget = TransitionBarrier(
    backBuffer,
    D3D12_RESOURCE_STATE_PRESENT,
    D3D12_RESOURCE_STATE_RENDER_TARGET);

commandList->ResourceBarrier(1, &toRenderTarget);
```

## 39. RTV Binding

```cpp
commandList->OMSetRenderTargets(
    1,
    &rtv,
    FALSE,
    nullptr);
```

最後の引数はDSV Handle Pointerです。

## 40. Single Handle Range Flag

`RTsSingleHandleToDescriptorRange = FALSE`の場合、Handle配列として扱います。連続Descriptor Rangeとの意味を区別します。

## 41. ClearRenderTargetView

```cpp
const float clearColor[4] = { 0.05f, 0.08f, 0.12f, 1.0f };
commandList->ClearRenderTargetView(rtv, clearColor, 0, nullptr);
```

Color値のColor Spaceを意識します。

## 42. Clear Rect

Rect数0/Pointer`nullptr`なら全体をClearします。部分Clearが必要な場合だけRectを渡します。

## 43. Viewport

```cpp
D3D12_VIEWPORT viewport{};
viewport.TopLeftX = 0.0f;
viewport.TopLeftY = 0.0f;
viewport.Width = static_cast<float>(width);
viewport.Height = static_cast<float>(height);
viewport.MinDepth = 0.0f;
viewport.MaxDepth = 1.0f;
commandList->RSSetViewports(1, &viewport);
```

## 44. Scissor Rect

```cpp
D3D12_RECT scissor{
    0,
    0,
    static_cast<LONG>(width),
    static_cast<LONG>(height)
};
commandList->RSSetScissorRects(1, &scissor);
```

Resize後に更新します。

## 45. RTVからPRESENTへTransition

```cpp
auto toPresent = TransitionBarrier(
    backBuffer,
    D3D12_RESOURCE_STATE_RENDER_TARGET,
    D3D12_RESOURCE_STATE_PRESENT);

commandList->ResourceBarrier(1, &toPresent);
```

Present前に戻します。

## 46. Command List Close/Execute

Barrier、Clear、戻しBarrierを記録後にCloseし、Direct QueueへExecuteします。

## 47. Present

```cpp
const UINT syncInterval = vsyncEnabled ? 1u : 0u;
const UINT presentFlags =
    (!vsyncEnabled && tearingEnabled)
        ? DXGI_PRESENT_ALLOW_TEARING
        : 0u;

HRESULT hr = swapChain->Present(syncInterval, presentFlags);
```

HRESULTを分類します。

## 48. Sync Interval 1

VSync有効の基本設定で、表示Refreshに同期します。Frame TimingはRefresh RateとGPU処理時間に影響されます。

## 49. Sync Interval 0

PresentをRefresh待ちに固定しません。Tearing許可条件やWindowed/Fullscreen Modeを確認します。

## 50. Present Allow Tearing

Swap Chain作成時のFlag、System Support、Sync Interval 0等の条件が揃う場合に指定します。

## 51. Exclusive Fullscreenとの関係

Tearing Flagの利用条件はPresentation Modeで異なります。Borderless/WindowedとExclusiveを同一扱いにしません。

## 52. Present結果

`DXGI_ERROR_DEVICE_REMOVED`、`DXGI_ERROR_DEVICE_RESET`等はDevice復旧Pathへ移します。`DXGI_STATUS_OCCLUDED`等のStatusも目的に応じて扱います。

## 53. PresentはGPU完了待機ではない

Present後もGPU/Displayが処理中です。Back BufferやFrame Resource再利用はFence/Swap Chain Indexで管理します。

## 54. PresentとFence Signal順

```text
ExecuteCommandLists
 -> Present
 -> Queue Signal FrameFence
```

採用する順序をRenderer全体で統一し、Frame ResourceへSignal Valueを保存します。

## 55. Back BufferごとのFence

```cpp
std::vector<UINT64> backBufferFenceValues(bufferCount, 0);
```

各Bufferの最後のGPU使用完了地点を記録できます。

## 56. Buffer再利用時

`GetCurrentBackBufferIndex`で選ばれたBufferのFenceが未完了なら待ちます。全Queue Idleは不要です。

## 57. Frame Resourceとの対応

Back Buffer indexとFrame Resource indexを同じにする単純設計があります。独立管理するなら対応関係を明示します。

## 58. 最小RenderFrame

```cpp
void RenderFrame()
{
    const UINT index = swapChain->GetCurrentBackBufferIndex();
    FrameResource& frame = frames[index];

    WaitForFrame(frame);
    frame.allocator->Reset();
    commandList->Reset(frame.allocator.Get(), nullptr);

    RecordClear(commandList.Get(), index);

    commandList->Close();
    ID3D12CommandList* lists[] = { commandList.Get() };
    queue->ExecuteCommandLists(1, lists);

    ThrowIfFailed(swapChain->Present(syncInterval, presentFlags), "Present");

    frame.fenceValue = SignalFrame();
}
```

各HRESULT確認は実装で省略しません。

## 59. Frame Latency

CPUがPresent Queueへ何Frame先行できるかはLatencyとThroughputへ影響します。

## 60. Maximum Frame Latency

`IDXGISwapChain2::SetMaximumFrameLatency`で最大Latencyを設定できる構成があります。Flag/Swap Chain種別との要件を確認します。

## 61. Frame Latency Waitable Object

対応Flag付きSwap ChainからWaitable Handleを取得し、次Frame生成Timingを制御できます。

## 62. GetFrameLatencyWaitableObject

```cpp
ComPtr<IDXGISwapChain2> swapChain2;
ThrowIfFailed(swapChain.As(&swapChain2), "Query IDXGISwapChain2");

HANDLE latencyHandle = swapChain2->GetFrameLatencyWaitableObject();
```

無効Handleを確認し、RAII管理します。

## 63. Latency Wait位置

Frame開始前等、Input SamplingとCPU Workの設計に合わせて待ちます。Fence Waitの代用ではありません。

## 64. FenceとLatency Handle

```text
Fence          : GPU Resource再利用の安全性
Latency Handle : Present Queue/Frame生成の先行量制御
```

両方が必要な設計があります。

## 65. Low Latency

Buffer Count、Maximum Latency、VSync/Tearing、Frame Cap、Input位置、GPU負荷を合わせて実測します。

## 66. Occlusion

Windowが完全に隠れた/最小化された場合、Present StatusとMessageを使い描画頻度を下げられます。

## 67. DXGI_PRESENT_TEST

実際にPresentせず状態確認に使えるFlagがあります。Occlusion回復確認等で仕様に沿って利用します。

## 68. Minimize

Client Size 0x0では`ResizeBuffers`やSize依存Texture生成を保留し、描画Loopを低頻度化します。

## 69. Resize Request

`WM_SIZE`で最新Client Sizeを保存し、Frame境界でResizeを処理します。Messageごとに重い再生成を繰り返しません。

## 70. Resize前の同期

Back Bufferを参照する全Commandの完了をFenceで保証します。単純実装ではDirect Queue Idleを待ちます。

## 71. Resize前の解放

```cpp
for (auto& buffer : backBuffers)
    buffer.Reset();
```

Back Bufferへの全COM参照を解放します。RTV Descriptor自体は上書き可能ですが参照Resourceは解放します。

## 72. ResizeBuffers

```cpp
ThrowIfFailed(
    swapChain->ResizeBuffers(
        bufferCount,
        newWidth,
        newHeight,
        format,
        swapChainFlags),
    "IDXGISwapChain::ResizeBuffers");
```

作成時Flagと整合させます。

## 73. Resize後

Back Bufferを再取得し、既存RTV Heap SlotへRTVを再作成し、Index/Fence/Viewport/Scissorを更新します。

## 74. Back Buffer Fence Reset

Queue Idle後のResizeなら各BufferのFence Valueを現在完了状態へReset/再設定できます。古いIndex Valueを誤利用しません。

## 75. Projection更新

Aspect Ratio、Screen Size Constant、Post Process Target、Depth Bufferも新しいSizeへ更新します。

## 76. Resize Generation

記録済みCommand ResultへSurface Generationを持たせ、Resize前世代のListを実行しません。

## 77. Color Space

Swap ChainのColor Spaceを`CheckColorSpaceSupport`等で確認し設定できます。SDR/HDR切替時にMonitor/Format/Metadataを再評価します。

## 78. HDR Format

10-bit/16-bit Format、Color Space、Tone Map、OS HDR状態を一体で設計します。Format変更だけでHDRにはなりません。

## 79. Monitor変更

Windowが別Monitorへ移動したらOutput、Refresh、HDR、DPIを再評価します。

## 80. Borderless Fullscreen

Window Styleを枠なしにしてMonitor Rectへ配置します。Swap ChainはWindowed Flip Modelとして扱う設計が一般的です。

## 81. Exclusive Fullscreen

`SetFullscreenState`を使う場合、Display Mode、Alt+Tab、終了時復元、Tearing条件を別途管理します。

## 82. Refresh Rate

Windowed Flip ModelではDesktop Compositionとの関係があります。古い`RefreshRate`固定設計を無条件に持ち込みません。

## 83. Scaling

Window SizeとBack Buffer Sizeが異なる場合のScaling Modeを理解します。通常は一致させ、Dynamic Resolutionは内部Scene Targetで行う設計があります。

## 84. Dynamic Resolution

Swap Chain Sizeを毎回変えず、Scene ColorだけScaleし最終PassでBack BufferへUpscaleできます。

## 85. MSAA Resolve

```text
MSAA Scene Target
 -> ResolveSubresource
 -> Non-MSAA Back Buffer/Intermediate
 -> Present
```

Format/Stateを正しく管理します。

## 86. Screenshot

Back BufferまたはTone Map後TextureをCopy SourceへTransitionしReadbackします。Copy完了Fenceまで待ち、Row Pitchを扱います。

## 87. Back BufferをSRVで読む場合

Swap ChainのUsage/Format/View要件とState Transitionを確認します。通常はIntermediate Scene/Post ColorをSRVとして使う方が設計しやすいです。

## 88. Composition Swap Chain

CoreWindow/Composition等、HWND以外のSwap Chain生成APIがあります。この章はDesktop HWNDを基準にします。

## 89. Multiple Windows

WindowごとにSwap Chain、Back Buffer、RTV、Present、Resize、Surface Generationを管理します。Queue/Device共有時のLifetimeを明示します。

## 90. Present Statistics

対応API/ToolでPresent Timing、Dropped Frame、Queue Depthを調査できます。PIX/PresentMon等の用途を分けます。

## 91. Debug NameとMarker

Back Buffer/RTV Heapへ名前を付け、FrameのPresent前後へMarkerを入れます。

## 92. PIX確認項目

- Current Back Buffer Index
- PRESENT/RENDER_TARGET State
- Clear/Draw Output
- RTV Format
- Queue Submit順
- Present Timing
- BufferごとのFence

## 93. Device Removed

PresentのHRESULTを必ず確認し、`GetDeviceRemovedReason`、DRED、最後のFrame Markerを保存します。

## 94. Test：Clear Color

Frameごとに明確な色を切り替え、Buffer Index、Barrier、Presentが正しいか確認できます。

## 95. Test：VSync/Tearing

VSync on/off、Tearing Support有無、Windowed/Borderless、複数Refresh RateでPresent ErrorとFrame Timeを確認します。

## 96. Test：Resize

連続Resize、最小化、最大化、0x0、極端なAspect、DPI/Monitor移動を繰り返します。

## 97. Test：GPU遅延

GPU Workを意図的に重くし、Back Buffer Fence待機、Frame Resource Ring、Latency Handleが破綻しないか確認します。

## 98. Test：Device復旧

Swap Chain/RTV再生成をDebug Commandから呼び、COM参照、Descriptor再作成、世代管理を検証します。

## 99. よくある失敗：QueueでなくDeviceを渡す

`CreateSwapChainForHwnd`へD3D12 Deviceを渡します。D3D12ではDirect Command Queueを渡します。

## 100. よくある失敗：MSAA Swap Chain

Flip Model BufferへSample Count > 1を設定します。別MSAA TargetからResolveします。

## 101. よくある失敗：PRESENT Barrier忘れ

Render Target StateのままPresentします。描画前後の二Transitionを記録します。

## 102. よくある失敗：Index自前加算

`(index+1)%count`だけでCurrent Bufferを決めます。`GetCurrentBackBufferIndex`を使います。

## 103. よくある失敗：PresentをFence扱い

Present後すぐAllocator/Back Buffer関連Dataを上書きします。Queue Fenceで完了を追跡します。

## 104. よくある失敗：Tearing Flag片側だけ

作成FlagなしでPresent Flagを使う、またはSupport未確認で指定します。三条件を揃えます。

## 105. よくある失敗：Resize参照残り

Back BufferのComPtrや記録中List参照が残り`ResizeBuffers`に失敗します。Fence完了と全参照解放を確認します。

## 106. よくある失敗：Latency HandleをFence代用

Present Queueが進んだからUpload/Allocator再利用も安全と判断します。Resource安全性はFenceで管理します。

## 107. 実装Checklist

- [ ] Flip Discard、Sample Count 1でDescを作る。
- [ ] Tearing SupportをFactoryで照会する。
- [ ] Direct Queueを使いSwap Chainを作る。
- [ ] Alt+Enter Policyを設定する。
- [ ] Buffer CountぶんRTV Heap/Resource/Viewを作る。
- [ ] Increment SizeでHandleを進める。
- [ ] Current Back Buffer Indexを取得する。
- [ ] PRESENT→RTV→PRESENT Barrierを記録する。
- [ ] Present HRESULTを分類する。
- [ ] Buffer/Frame Resource再利用をFenceで管理する。
- [ ] VSync/Tearing/Latency設定を分離する。
- [ ] Resize前にFence完了と参照解放を保証する。
- [ ] Resize後にRTV/Viewport/Size Resourceを再生成する。

## 108. 理解確認問題

1. D3D12 Swap Chain作成へCommand Queueを渡す理由を説明してください。
2. Flip ModelでSample Count 1にする理由を説明してください。
3. RTV Heap、Back Buffer Resource、RTV Descriptorの関係を説明してください。
4. Present前後に必要なResource Stateを説明してください。
5. `GetCurrentBackBufferIndex`が必要な理由を説明してください。
6. PresentとFence完了の違いを説明してください。
7. Tearingを有効化する条件を説明してください。
8. Frame Latency Waitable ObjectとFenceの違いを説明してください。
9. Resize前後のResource処理順を説明してください。

## 109. 章末要点

- Swap ChainはDirect Command QueueとHWNDから作ります。
- Flip Model、Buffer Count、Format、Tearing/Latency Flagを要件から決めます。
- Back Buffer数ぶんRTV Descriptorを作りIndexで対応させます。
- 描画前にPRESENT→RENDER_TARGET、後にPRESENTへ戻します。
- PresentはGPU完了を意味せず、Buffer再利用はFenceで管理します。
- VSync、Tearing、Frame Latency、Frame Resourceは別概念として調整します。
- Resize時はGPU完了、Back Buffer参照解放、ResizeBuffers、RTV再作成の順を守ります。

## 110. 公式資料

- [DXGI flip model](https://learn.microsoft.com/en-us/windows/win32/direct3ddxgi/dxgi-flip-model)
- [For best performance, use DXGI flip model](https://learn.microsoft.com/en-us/windows/win32/direct3ddxgi/for-best-performance--use-dxgi-flip-model)
- [IDXGIFactory2::CreateSwapChainForHwnd](https://learn.microsoft.com/en-us/windows/win32/api/dxgi1_2/nf-dxgi1_2-idxgifactory2-createswapchainforhwnd)
- [IDXGISwapChain3::GetCurrentBackBufferIndex](https://learn.microsoft.com/en-us/windows/win32/api/dxgi1_4/nf-dxgi1_4-idxgiswapchain3-getcurrentbackbufferindex)
- [ID3D12Device::CreateDescriptorHeap](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12device-createdescriptorheap)
- [ID3D12Device::CreateRenderTargetView](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12device-createrendertargetview)
- [IDXGISwapChain::Present](https://learn.microsoft.com/en-us/windows/win32/api/dxgi/nf-dxgi-idxgiswapchain-present)
- [Variable refresh rate displays](https://learn.microsoft.com/en-us/windows/win32/direct3ddxgi/variable-refresh-rate-displays)
- [IDXGISwapChain2::GetFrameLatencyWaitableObject](https://learn.microsoft.com/en-us/windows/win32/api/dxgi1_3/nf-dxgi1_3-idxgiswapchain2-getframelatencywaitableobject)
- [IDXGISwapChain::ResizeBuffers](https://learn.microsoft.com/en-us/windows/win32/api/dxgi/nf-dxgi-idxgiswapchain-resizebuffers)

次章では、RTV以外も含むDescriptor Heap、CPU/GPU Handle、Persistent/Transient Descriptor Allocator、Descriptor Tableの寿命を扱います。
