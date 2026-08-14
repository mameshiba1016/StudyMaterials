# DirectX 11：Device Lost・Resize・Fullscreen

この章では、Window Size変更、最小化、DPI変更、Fullscreen切替、Display変更、Device Removedから安全に復旧するRenderer Lifecycleを学びます。Resource分類、解放順序、再生成、Thread停止、Error診断、Testまでを扱います。

## 1. Rendererは状態機械である

Rendererは一度初期化して終了まで不変ではありません。Window、Monitor、Device、Swap Chain、Back Bufferは実行中に変化します。

```text
Uninitialized -> Ready -> Resizing -> Ready
                     └-> DeviceLost -> Recreating -> Ready
                     └-> Suspended/Minimized
                     └-> ShuttingDown
```

## 2. 三種類のResource

```text
Device依存      : Shader、Buffer、Texture、State、View
Window Size依存 : Back Buffer RTV、Depth、Scene Color、Post Process Target
Scene依存       : Model、Material Instance、Animation、Effect Instance
```

寿命を分けると再生成範囲を限定できます。

## 3. Device-independent Data

File Path、Decoded Image、Mesh CPU Data、Shader Source/Bytecode等、GPU Deviceがなくても保持できるDataです。Device Lost後の再Uploadに利用できます。

## 4. Device-dependent Resource

`ID3D11Buffer`、`ID3D11Texture2D`、Shader、View、State Object等は特定Deviceに属します。Device再生成時は旧Objectを使えません。

## 5. Size-dependent Resource

WindowのClient SizeやRender Scaleへ依存するResourceです。

- Swap Chain Back Buffer RTV
- Depth Stencil Texture/DSV
- Scene Color HDR Target
- G-Buffer
- Motion Vector
- Bloom Chain
- Screen-space Effect用Texture

## 6. Resizeの入口

Win32では主に`WM_SIZE`で新しいClient Sizeを受け取ります。ただしMessage Handler内で即座に重い再生成を完了させる必要はありません。

## 7. Resize Requestを保存する

```cpp
case WM_SIZE:
{
    const UINT width = LOWORD(lParam);
    const UINT height = HIWORD(lParam);
    renderer.RequestResize(width, height, wParam == SIZE_MINIMIZED);
    return 0;
}
```

Message Threadでは要求値を保存し、安全なFrame境界で適用します。

## 8. ResizeのCoalescing

Window Drag中は多数の`WM_SIZE`が届きます。すべてに対してResourceを再生成せず、最後のSizeだけを適用すると負荷を抑えられます。

## 9. WM_ENTERSIZEMOVEとWM_EXITSIZEMOVE

Interactive Resize開始・終了を検出できます。Resize中の描画頻度を落とし、終了時に最終Sizeを確定するPolicyもあります。

## 10. 0x0 Size

最小化時はClient Sizeが0になることがあります。0幅・0高さのTextureを作らず、描画とResizeを保留します。

```cpp
if (width == 0 || height == 0)
{
    minimized_ = true;
    return;
}
```

## 11. 最小化中のLoop

最小化中も全速力でUpdate/RenderするとCPUとBatteryを消費します。Message待機、低頻度Update、Audio/Network継続等をGame要件で決めます。

## 12. Resize前のThread同期

旧Back Bufferを参照するWorker JobやCommand Listが残っていてはいけません。新規記録を止め、必要Jobを完了させ、古いResultを破棄します。

## 13. ResizeBuffersの大原則

Swap Chain Bufferを直接・間接に参照するObjectをすべて解放してから`ResizeBuffers`を呼びます。

```text
Back Buffer Texture
 └─ RTV
    └─ Context Binding
```

Binding解除とCOM参照解放の両方が必要です。

## 14. ContextからUnbind

```cpp
ID3D11RenderTargetView* nullRTV = nullptr;
immediateContext->OMSetRenderTargets(1, &nullRTV, nullptr);
```

必要に応じてSRV/UAV等も外します。同じResourceのViewが別Stageへ残っていないか確認します。

## 15. ClearStateの選択

```cpp
immediateContext->ClearState();
immediateContext->Flush();
```

`ClearState`は全Bindingを外す強い方法です。Resize Pathでは分かりやすい一方、通常FrameではStateを再設定するCostがあるため乱用しません。

## 16. Flushの意味

`Flush`はCommand送信を促しますが、GPU完了を待つ一般的な同期ではありません。参照を解放する設計と混同しません。

## 17. Size-dependent Viewを解放

```cpp
backBufferRTV_.Reset();
depthDSV_.Reset();
depthTexture_.Reset();
sceneColorSRV_.Reset();
sceneColorRTV_.Reset();
sceneColorTexture_.Reset();
```

派生ViewからResource本体まで参照を手放します。

## 18. ResizeBuffers

```cpp
HRESULT hr = swapChain_->ResizeBuffers(
    0,
    width,
    height,
    DXGI_FORMAT_UNKNOWN,
    swapChainFlags_);
```

Buffer Countに0、Formatに`DXGI_FORMAT_UNKNOWN`を指定すると既存設定を保持できます。FlagはSwap Chain作成時の要件と整合させます。

## 19. ResizeBuffersのError確認

`HRESULT`を必ず確認します。`DXGI_ERROR_INVALID_CALL`ならBack Buffer参照が残っている可能性を調べます。Device Removed系なら復旧Pathへ移ります。

## 20. Back Buffer再取得

```cpp
ComPtr<ID3D11Texture2D> backBuffer;
ThrowIfFailed(swapChain_->GetBuffer(
    0,
    IID_PPV_ARGS(&backBuffer)));

ThrowIfFailed(device_->CreateRenderTargetView(
    backBuffer.Get(), nullptr, &backBufferRTV_));
```

Resize前のBack Buffer Pointerを再利用しません。

## 21. Depth再生成

```cpp
D3D11_TEXTURE2D_DESC depthDesc{};
depthDesc.Width = width;
depthDesc.Height = height;
depthDesc.MipLevels = 1;
depthDesc.ArraySize = 1;
depthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
depthDesc.SampleDesc.Count = 1;
depthDesc.Usage = D3D11_USAGE_DEFAULT;
depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
```

Swap Chainと同じ描画解像度・Sample条件を使います。

## 22. Viewport再設定

```cpp
D3D11_VIEWPORT viewport{};
viewport.Width = static_cast<float>(width);
viewport.Height = static_cast<float>(height);
viewport.MinDepth = 0.0f;
viewport.MaxDepth = 1.0f;
immediateContext->RSSetViewports(1, &viewport);
```

Resize後に旧Viewportを残さないようにします。

## 23. Scissor Rect

Scissorを使う場合は新しいSizeへ更新します。Viewportだけ直してもScissorが旧Sizeなら描画が欠けます。

## 24. Projection Matrix

Aspect Ratioを新しい幅と高さから再計算します。

```cpp
const float aspect = static_cast<float>(width) /
                     static_cast<float>(height);
camera.SetProjection(fovY, aspect, nearZ, farZ);
```

## 25. Screen-space Constant

Resolution、Inverse Resolution、Texel Size、Aspect、Render Scaleを使うShader Constantも更新します。

```hlsl
float2 renderSize;
float2 invRenderSize;
```

## 26. Post Process Resource

Bloom、Blur、AO、Motion Blur等はFull/Half/Quarter Resolutionを使うことがあります。端数の切り上げ規則を統一して再生成します。

## 27. Resizeの完全な順序

1. Resize RequestをFrame境界で取得する。
2. 0x0なら描画を保留する。
3. Render Workerを停止・同期する。
4. Back Buffer関連Bindingを外す。
5. Size-dependent View/Resourceを解放する。
6. `ResizeBuffers`を呼ぶ。
7. Back Buffer RTVを再生成する。
8. Depth/Post Process Resourceを再生成する。
9. Viewport、Scissor、Camera、Constantを更新する。
10. Workerへ新しいRenderer世代を公開する。

## 28. Transactionとして扱う

途中で失敗した場合に半端なReady状態へ戻さず、`Resizing`または`DeviceLost`状態のままErrorを上位へ返します。

## 29. Resize Generation

```cpp
struct RenderSurfaceState
{
    uint64_t generation;
    UINT width;
    UINT height;
};
```

Worker ResultのGenerationが現在と違えば古いCommandを実行しません。

## 30. Logical SizeとPhysical Pixel

Windowの論理Sizeと描画Pixel Sizeを区別します。DPI Scaling環境ではUI座標、Client Pixel、Render Resolutionが一致しない場合があります。

## 31. DPI変更

`WM_DPICHANGED`で新DPIと推奨Window Rectを受け取り、UI ScaleとPixel Size依存Resourceを更新します。

## 32. Per-monitor DPI

Windowを別Monitorへ移すとDPIが変わる可能性があります。起動時だけDPIを読む実装では不十分です。

## 33. Render Scale

Window Sizeと内部3D描画解像度を分離できます。

```text
Swap Chain : 2560x1440
Scene Color: 1920x1080
UI         : 2560x1440
```

Upscale Passを設け、Camera AspectとSampling Sizeを正しく使い分けます。

## 34. Dynamic Resolution

GPU時間に応じてRender Scaleを変える場合、Scene Size-dependent Resourceだけを再生成またはPoolから選択します。Swap Chain Resizeは不要です。

## 35. Fullscreenの種類

```text
Windowed              : 通常Window
Borderless Fullscreen : 枠なしWindowをMonitor全体へ配置
Exclusive Fullscreen  : Swap ChainをFullscreen Stateへ切替
```

UX、切替速度、Display Mode、Compatibilityの違いがあります。

## 36. Borderless Fullscreen

Window StyleからCaption/Frameを外し、対象MonitorのRectへWindowを配置します。Desktop Compositionと共存しやすく、現在のGameで一般的な選択肢です。

## 37. Windowed Rectを保存

Borderlessへ入る前にWindow Style、Extended Style、Position、Sizeを保存し、Windowedへ戻す際に復元します。

## 38. Monitor選択

`MonitorFromWindow`等で対象Monitorを取得し、`GetMonitorInfo`からMonitor Rectを使います。Primary Monitor固定にしないようにします。

## 39. Work AreaとMonitor Area

Windowed配置にはTaskbarを除くWork Area、Borderless Fullscreenには通常Monitor Areaを使います。目的に応じて選択します。

## 40. Exclusive Fullscreen

```cpp
HRESULT hr = swapChain_->SetFullscreenState(TRUE, output.Get());
```

切替失敗、Alt+Tab、Monitor変更等を扱う必要があります。利用する場合はWindowedへ戻す終了処理を必ず用意します。

## 41. ResizeTarget

Exclusive FullscreenでDisplay Modeを変更する場合に関係します。`ResizeBuffers`とは目的が異なり、Window/Target ModeとBack Buffer Sizeを区別します。

## 42. Alt+Enter

DXGIのDefault Alt+Enter動作を使うか、`MakeWindowAssociation`で無効化してApplication側で一貫して管理するかを決めます。

```cpp
factory->MakeWindowAssociation(
    hwnd,
    DXGI_MWA_NO_ALT_ENTER);
```

## 43. Fullscreen切替Request

Input Handlerから直接DXGIやWindow Styleを変更せず、Renderer/Applicationの状態遷移RequestとしてFrame境界へ渡します。

## 44. Display Change

`WM_DISPLAYCHANGE`、Monitor接続解除、解像度変更を想定します。現在のMonitor/Outputが無効なら有効なMonitorへWindowを移します。

## 45. HDRとColor Space

MonitorやFullscreen状態の変化でHDR Support、Color Space、Output Format条件が変わる可能性があります。Swap Chain Color Spaceを再評価します。

## 46. Tearing Flag

Windowed/BorderlessでTearingを使う場合はSystem Support、Swap Chain作成Flag、Present Flagを整合させます。Fullscreen方式による条件差を確認します。

## 47. Present Result

```cpp
HRESULT hr = swapChain_->Present(syncInterval, presentFlags);

if (hr == DXGI_ERROR_DEVICE_REMOVED ||
    hr == DXGI_ERROR_DEVICE_RESET)
{
    HandleDeviceLost(hr);
}
else
{
    ThrowIfFailed(hr);
}
```

PresentはDevice Lostを検出する主要地点です。

## 48. Device Lostとは

GPU Deviceとの接続が失われ、既存D3D Resourceを継続利用できない状態です。Application全体を必ず終了するのではなく、DeviceとResourceを再生成できる設計にします。

## 49. 主なHRESULT

- `DXGI_ERROR_DEVICE_REMOVED`
- `DXGI_ERROR_DEVICE_RESET`
- `DXGI_ERROR_DEVICE_HUNG`
- `DXGI_ERROR_DRIVER_INTERNAL_ERROR`
- `DXGI_ERROR_INVALID_CALL`

意味と対応を一括りにしません。

## 50. GetDeviceRemovedReason

```cpp
const HRESULT reason = device_->GetDeviceRemovedReason();
LogDeviceRemovedReason(triggerHr, reason);
```

最初に検出したHRESULTとRemoved Reasonの両方を記録します。

## 51. DEVICE_REMOVED

Driver Update、GPU切替、物理的なAdapter変化、OSによるDevice再構成等で起こり得ます。再生成Pathへ進みます。

## 52. DEVICE_RESET

不正なCommand等でGPUがResetされた場合があります。再生成を試しつつ、Debug LayerやCaptureで原因を調査します。

## 53. DEVICE_HUNG

無限Loop相当のShader、範囲外Resource Access、不正Command等Application側の問題を疑います。単に再試行し続けず診断情報を保存します。

## 54. INVALID_CALL

Programming Errorを示す場合が多く、Device Lostと同じ扱いで隠蔽しません。Debug Layer Messageと呼出し引数を直します。

## 55. Device Lost検出地点

Presentだけでなく、Resource生成、Map、ResizeBuffers、Query等のHRESULTでも検出できます。共通の分類関数を用意します。

```cpp
bool IsDeviceLostError(HRESULT hr)
{
    return hr == DXGI_ERROR_DEVICE_REMOVED ||
           hr == DXGI_ERROR_DEVICE_RESET;
}
```

## 56. 復旧中の再入を防ぐ

Atomic/Mutexだけに頼らず、Renderer Stateを`DeviceLost`または`Recreating`へ一度だけ遷移させます。複数Error地点から同時に再生成を始めません。

## 57. Device Lost復旧順序

1. 新規描画・Loading Job受付を止める。
2. Workerへ停止を通知してJoinする。
3. Removed Reasonと診断情報を保存する。
4. Context Bindingを外す。
5. Command ListとDevice依存Resourceを解放する。
6. Swap Chain、Context、Device、Adapter参照を解放する。
7. Adapterを再列挙する。
8. DeviceとSwap Chainを再生成する。
9. Size-dependent Resourceを生成する。
10. AssetのGPU Resourceを再Uploadする。
11. Renderer世代を更新してWorkerを再開する。

## 58. Asset再生成情報

Texture File Path、Mesh CPU Copy、Shader Bytecode、Material Descriptor等、GPU Resourceを再構築できる情報をAsset Systemが保持します。

## 59. CPU CopyのTrade-off

全Texture/Meshの展開済みCPU Dataを保持すれば復旧は速い一方、Memoryを多く使います。Sourceから再読込する方式との組合せを設計します。

## 60. Resource Registry

```cpp
class IDeviceResource
{
public:
    virtual ~IDeviceResource() = default;
    virtual void ReleaseDeviceObjects() = 0;
    virtual void CreateDeviceObjects(ID3D11Device* device) = 0;
};
```

登録順・依存順・失敗時Rollbackを考慮し、無秩序なGlobal Callbackにはしません。

## 61. Handleの安定性

GameplayやMaterialが生の`ID3D11ShaderResourceView*`を永続保持すると再生成後に無効になります。安定Handleから現在世代のResourceを解決します。

## 62. Generation Check

```cpp
struct GpuHandle
{
    uint32_t index;
    uint32_t generation;
};
```

旧Device世代のHandle使用をDebug Buildで検出します。

## 63. Default Resource

再読込中はWhite Texture、Default Normal、Fallback Shader等を使い、完全復旧まで段階的に表示できます。

## 64. Async Reload

Device/Swap Chainと最低限のUIを先に復旧し、大きなAssetをBackgroundで再Uploadする設計も可能です。Loading状態とThread同期を明示します。

## 65. Device作成Fallback

元Adapterが消えた場合は再列挙し、適切なHardware Adapterを選び直します。必要ならWARP等のFallback Policyを用意します。

## 66. Debug Device再生成

Debug Buildでは再生成後もDebug Layerを有効にします。Live Object Reportは旧Device解放前の参照漏れ診断に役立ちます。

## 67. DREDとの違い

DREDは主にDirect3D 12のDevice Removed診断機能です。Direct3D 11ではDebug Layer、Info Queue、HRESULT、Capture、Event Log等を組み合わせます。

## 68. Recovery Loopを防ぐ

再生成直後に同じ原因でDevice Lostする場合、無限再試行しません。回数制限、時間間隔、致命Error画面への移行を設計します。

## 69. Save DataとDevice Lost

GPU Device復旧失敗をGame Save破損へ波及させません。Renderer状態とGameplay/Save状態を分離します。

## 70. Audio・Input・Network

Device LostはGraphics Deviceの問題です。他Subsystemまで無条件に破棄せず、Loading/Paused状態で必要な処理を継続します。

## 71. Resize中のUI

UI LayoutはLogical Size/DPIへ、Render TargetはPhysical Pixelへ応答させます。Anchor、Safe Area、Text Rasterization Scaleを再計算します。

## 72. CameraとGameplay

ResizeでCamera Aspectは変えてもGameplay World状態やCamera PositionをResetしません。Graphics設定変更とSimulation状態を分離します。

## 73. ScreenshotとCapture

Resize/Device Lost中に旧ResourceへScreenshot Copyしないよう、Capture RequestにもSurface Generationを持たせます。

## 74. Test：Resize

- 1px単位でWindowを連続Resizeする。
- 横長・縦長・極小Sizeへ変更する。
- 最大化・元に戻すを繰り返す。
- 最小化・復元を繰り返す。
- Resize中にEffectやScene切替を行う。
- 複数Monitor間を移動する。

## 75. Test：DPI

- DPIの異なるMonitor間を移動する。
- OS Scaleを変更する。
- UI Hit Testと表示位置を比較する。
- TextとTextureの鮮明さを確認する。

## 76. Test：Fullscreen

- WindowedとBorderlessを連続切替する。
- Alt+Tabを繰り返す。
- 対象Monitorを切り替える。
- Display接続を変更する。
- VSync/Tearing/HDR設定と組み合わせる。
- 終了時にWindowed Stateへ戻ることを確認する。

## 77. Test：Device Lost

実際のDevice Lostは再現しにくいため、Resource解放・再生成Pathを手動TriggerできるDebug Commandを作ります。

```cpp
if (debugRecreateDeviceRequested)
    renderer.RecreateAllDeviceResourcesForTest();
```

## 78. Recreate Testの限界

手動再生成はDriverが実際にRemovedになった状況を完全再現しません。それでもLifetime、Registry、Handle、Thread停止の欠陥を早期発見できます。

## 79. Logging

次を一つのEventとして記録します。

- TimestampとFrame ID
- Trigger APIとHRESULT
- `GetDeviceRemovedReason`
- Adapter名、Vendor/Device ID、Driver情報
- Window/Render Size、Fullscreen状態
- 最後に実行したPass/Marker
- 復旧段階と所要時間

## 80. よくある失敗：RTVだけReset

Context Bindingや別のSRV、Back Buffer Texture参照が残り`ResizeBuffers`が失敗します。参照Graph全体を確認します。

## 81. よくある失敗：0x0で再生成

最小化Messageを通常Resizeとして処理し、無効SizeのTexture作成に失敗します。復元まで保留します。

## 82. よくある失敗：Viewport更新忘れ

Back Bufferだけ新しくしてViewport/Scissor/Projectionが旧Sizeのまま残り、描画が伸縮・欠落します。

## 83. よくある失敗：生Pointerを永続保持

Device再生成後も旧SRV/Buffer PointerをMaterialやComponentが持ち続けます。安定Handleと世代管理へ変更します。

## 84. よくある失敗：Worker稼働中に破棄

Deferred Context Jobが旧Resourceを記録中にDeviceを解放するとRaceになります。受付停止、完了、Joinの順序を守ります。

## 85. よくある失敗：ErrorをすべてDevice Lost扱い

`E_INVALIDARG`や`DXGI_ERROR_INVALID_CALL`まで再生成で隠すとProgramming Errorが残ります。HRESULTを分類します。

## 86. よくある失敗：無限復旧

原因がShader Hangや不正Commandなら再生成直後に再発します。診断情報を保存し、安全に停止する上限を設けます。

## 87. 実装Checklist

- [ ] Device/Size/Scene依存Resourceを分類している。
- [ ] Resize Requestを安全なFrame境界で処理する。
- [ ] 0x0 Sizeを保留できる。
- [ ] Resize前に全Back Buffer参照を解除する。
- [ ] Depth、Post Process、Viewport、Projectionも更新する。
- [ ] Windowed RectとStyleを復元できる。
- [ ] Monitor/DPI/Display変更へ対応する。
- [ ] Present以外のDevice Lost HRESULTも分類する。
- [ ] Worker停止後にDevice Resourceを破棄する。
- [ ] Assetを再UploadできるCPU側情報がある。
- [ ] HandleにDevice Generation検証がある。
- [ ] 復旧Loopに上限がある。

## 88. 理解確認問題

1. `ResizeBuffers`前に解放すべき参照を説明してください。
2. 最小化時にResizeを保留する理由を説明してください。
3. Window SizeとRender Resolutionを分ける利点を説明してください。
4. BorderlessとExclusive Fullscreenの違いを説明してください。
5. `GetDeviceRemovedReason`から得る情報を説明してください。
6. Device Lost時にCPU Asset Dataが必要な理由を説明してください。
7. GPU HandleへGenerationを持たせる理由を説明してください。
8. Worker ThreadをDevice解放前に止める理由を説明してください。

## 89. 章末要点

- RendererをResize、Minimized、DeviceLost、Recreatingを持つ状態機械として設計します。
- Device依存、Size依存、Scene依存Resourceの寿命を分離します。
- Resize前にBack BufferへのBindingとCOM参照をすべて解除します。
- Resize後はRTVだけでなくDepth、Post Process、Viewport、Projectionも更新します。
- Fullscreen方式、Monitor、DPI、HDR、Tearingの状態を個別に扱います。
- Device Lost時はThreadを止め、旧Resourceを解放し、DeviceからAssetまで順に再生成します。
- Handle世代、Fallback Resource、診断Log、再試行上限で復旧を安全にします。

## 90. 公式資料

- [IDXGISwapChain::ResizeBuffers](https://learn.microsoft.com/en-us/windows/win32/api/dxgi/nf-dxgi-idxgiswapchain-resizebuffers)
- [IDXGISwapChain::SetFullscreenState](https://learn.microsoft.com/en-us/windows/win32/api/dxgi/nf-dxgi-idxgiswapchain-setfullscreenstate)
- [IDXGIFactory::MakeWindowAssociation](https://learn.microsoft.com/en-us/windows/win32/api/dxgi/nf-dxgi-idxgifactory-makewindowassociation)
- [Handling device removed scenarios in Direct3D 11](https://learn.microsoft.com/en-us/windows/uwp/gaming/handling-device-lost-scenarios)
- [ID3D11Device::GetDeviceRemovedReason](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11device-getdeviceremovedreason)
- [DXGI error codes](https://learn.microsoft.com/en-us/windows/win32/direct3ddxgi/dxgi-error)
- [High DPI desktop application development on Windows](https://learn.microsoft.com/en-us/windows/win32/hidpi/high-dpi-desktop-application-development-on-windows)
- [Variable refresh rate displays](https://learn.microsoft.com/en-us/windows/win32/direct3ddxgi/variable-refresh-rate-displays)

次章では、Debug Layer、Info Queue、PIX、RenderDoc、GPU Query、Bottleneck分析を使った描画診断を扱います。
