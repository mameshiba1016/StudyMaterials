# DirectX 11：Depth Stencil・Viewport・Resize

この章では、3D Objectの前後関係を判定するDepth Buffer、用途を限定するStencil Buffer、描画領域を決めるViewport、Window Size変更に追従するResize処理を学びます。四つは別機能ですが、すべてBack Buffer寸法と強く結び付くため、一つの再構築単位として扱います。

## 1. この章の全体像

```text
valid client size
-> resize Swap Chain buffers
-> recreate Back Buffer RTV
-> create Depth Stencil Texture
-> create DSV
-> build Viewport
-> bind RTV + DSV
-> clear color + depth + stencil
-> draw
```

## 2. Depth Bufferとは

各Pixelについて、Cameraから見た奥行き値を保存するBufferです。新しいFragmentのDepthと保存値を比較し、手前にあるFragmentだけを通すことで隠面を処理します。

## 3. Painter's Algorithmとの違い

CPUで遠いObjectから順に並べるだけでは、Mesh同士が交差する場合や一つの三角形内部の奥行きを正しく処理できません。Depth TestはRasterizeされたFragment単位で比較します。

## 4. Depth TestとDepth Write

- Depth Test：新しいDepth値を既存値と比較する。
- Depth Write：Testを通過した値をDepth Bufferへ書く。

透明ObjectではTestを有効、Writeを無効にするなど、二つを別に制御します。

## 5. Stencil Bufferとは

Pixelごとの小さな整数値を保存し、条件に応じて描画を許可・拒否・更新するBufferです。Mask、Outline、Mirror、Portal、Light Volumeなどに使います。

## 6. DepthとStencilを一つのResourceに持つ

`DXGI_FORMAT_D24_UNORM_S8_UINT`は、24 bit Depthと8 bit Stencilを組み合わせた代表的なFormatです。

## 7. Depth用の主要Format

- `DXGI_FORMAT_D32_FLOAT`：32 bit浮動小数Depth。
- `DXGI_FORMAT_D24_UNORM_S8_UINT`：24 bit正規化Depth＋8 bit Stencil。
- `DXGI_FORMAT_D16_UNORM`：16 bit Depth。
- `DXGI_FORMAT_D32_FLOAT_S8X24_UINT`：Depth 32 bit＋Stencil 8 bitを含む高精度構成。

必要な精度、Stencil有無、Memory、対応状況で選びます。

## 8. 最初の推奨Format

Stencilを学ぶなら`D24_UNORM_S8_UINT`、Depthだけなら`D32_FLOAT`が分かりやすい出発点です。Format SupportはDeviceへ問い合わせます。

## 9. Format Support確認

```cpp
UINT support = 0;
ThrowIfFailed(device->CheckFormatSupport(
    DXGI_FORMAT_D24_UNORM_S8_UINT,
    &support));

const bool canUseDepthStencil =
    (support & D3D11_FORMAT_SUPPORT_DEPTH_STENCIL) != 0;
```

Format名が存在することと、対象Deviceで目的用途に使えることは別です。

## 10. Depth Textureの所有物

```cpp
Microsoft::WRL::ComPtr<ID3D11Texture2D> depthTexture;
Microsoft::WRL::ComPtr<ID3D11DepthStencilView> depthDsv;
```

DSVがResource参照を保持しますが、Depth Textureを後でSRV化する設計では本体も明示所有すると分かりやすくなります。

## 11. D3D11_TEXTURE2D_DESC

```cpp
D3D11_TEXTURE2D_DESC desc{};
desc.Width = width;
desc.Height = height;
desc.MipLevels = 1;
desc.ArraySize = 1;
desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
desc.SampleDesc.Count = 1;
desc.SampleDesc.Quality = 0;
desc.Usage = D3D11_USAGE_DEFAULT;
desc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
desc.CPUAccessFlags = 0;
desc.MiscFlags = 0;
```

## 12. WidthとHeight

Back Bufferと同じPixel寸法を使います。異なる寸法のRTVとDSVを同時Bindingしないよう、同じSize Sourceから作ります。

## 13. MipLevels

通常のDepth BufferはMip 1枚です。自動Mip生成を行う表示Textureとは用途が異なります。

## 14. ArraySize

通常Camera一つなら1です。Cube Shadow MapやLayered RenderingではArray Textureを使う場合があります。

## 15. SampleDesc

Back BufferやColor TargetとSample Count、Qualityを一致させます。非MSAAなら`{1, 0}`です。

## 16. Usage

GPUが描画中に読み書きするため`D3D11_USAGE_DEFAULT`を使います。CPUから毎Frame MapするResourceではありません。

## 17. BindFlags

```cpp
desc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
```

Depth Stencil Outputとして使う意思を作成時に指定します。

## 18. CreateTexture2D

```cpp
ThrowIfFailed(device->CreateTexture2D(
    &desc,
    nullptr,
    depthTexture.GetAddressOf()));
```

初期Dataは`nullptr`です。描画開始時にClearし、GPUが値を書き込みます。

## 19. Texture作成とView作成を分ける理由

ResourceはMemory、Viewは用途と解釈です。一つのResourceへ互換な複数Viewを作れるため、APIが分離されています。

## 20. CreateDepthStencilView

```cpp
ThrowIfFailed(device->CreateDepthStencilView(
    depthTexture.Get(),
    nullptr,
    depthDsv.GetAddressOf()));
```

通常の単一Textureなら既定View Descriptorを使えます。

## 21. 明示DSV Descriptor

```cpp
D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
dsvDesc.Flags = 0;
dsvDesc.Texture2D.MipSlice = 0;
```

ResourceのFormat、Sample設定、Dimensionと一致させます。

## 22. MSAA用ViewDimension

MSAA Textureでは`D3D11_DSV_DIMENSION_TEXTURE2DMS`を使います。MSAA TextureにMip Sliceはありません。

## 23. Read-only DSV

対応するInterfaceとFeature Levelでは、DepthまたはStencilをRead-onlyにするDSV Flagがあります。DepthをSRVで読むPassなどで役立ちます。

## 24. DepthをShaderから読む設計

Depth ResourceをTypeless Formatで作り、DSVとSRVへ互換なTyped Formatを割り当てます。単純なDepth専用ResourceよりDescriptor設計が複雑です。

## 25. Typeless Depthの例

```text
Resource: R24G8_TYPELESS
DSV     : D24_UNORM_S8_UINT
SRV     : R24_UNORM_X8_TYPELESS
```

同じMemoryをDepth出力とShader入力で解釈します。同時に読み書きはできません。

## 26. DSVとSRVの競合

書込み可能DSVとしてBinding中の同一SubresourceをSRVとして読むことはできません。Pass境界で解除するか、Read-only DSVを正しく使います。

## 27. RTVとDSVを同時Binding

```cpp
ID3D11RenderTargetView* rtvs[] = {backBufferRtv.Get()};
context->OMSetRenderTargets(1, rtvs, depthDsv.Get());
```

Color出力とDepth/Stencil出力をOutput Mergerへ設定します。

## 28. ClearDepthStencilView

```cpp
context->ClearDepthStencilView(
    depthDsv.Get(),
    D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL,
    1.0f,
    0);
```

Flag、Depth Clear値、Stencil Clear値の順です。

## 29. Depth Clear値1.0

通常のDepth比較`LESS`または`LESS_EQUAL`では、1.0を最も遠い値としてClearします。近いFragmentほど小さい値になります。

## 30. Reversed-Z

Projectionと比較関数を反転し、Depthを0.0でClearして`GREATER`系で比較する方式です。遠距離精度を改善できますが、Renderer全体で規約を統一する必要があります。

## 31. Stencil Clear値0

Stencil Maskを何も印されていない状態へ戻す一般的な値です。Algorithmが別の初期値を要求するなら明示します。

## 32. 必要な面だけClearする

Stencilを使わないなら`D3D11_CLEAR_DEPTH`だけにできます。Depthを保持してStencilだけ消すなど、Pass設計に合わせます。

## 33. ClearはState設定ではない

Clearは指定Viewへ値を書き込むCommandです。Depth Testの有効/無効や比較関数はDepth Stencil Stateで別途設定します。

## 34. Depth Stencil Stateとの違い

- DSV：値を保存するResource View。
- Depth Stencil State：比較、書込みMask、Stencil演算の規則。

本格的なState作成は後の章で扱います。

## 35. Viewportとは

Clip SpaceからRender Target上のPixel領域へ変換するRasterizer設定です。TextureやViewではありません。

## 36. D3D11_VIEWPORT

```cpp
D3D11_VIEWPORT viewport{};
viewport.TopLeftX = 0.0f;
viewport.TopLeftY = 0.0f;
viewport.Width = static_cast<float>(width);
viewport.Height = static_cast<float>(height);
viewport.MinDepth = 0.0f;
viewport.MaxDepth = 1.0f;
```

## 37. TopLeftXとTopLeftY

Render Target内でViewportを開始するPixel座標です。全画面描画なら0、分割画面やEditor ViewではOffsetを使います。

## 38. WidthとHeight

Floatで指定します。通常はBack Bufferの整数Pixel寸法を変換します。0以下の領域を設定しません。

## 39. MinDepthとMaxDepth

通常は0.0から1.0です。Viewport変換後のDepth範囲を指定します。ProjectionのNear/Far距離そのものではありません。

## 40. RSSetViewports

```cpp
context->RSSetViewports(1, &viewport);
```

Rasterizer StageへViewport配列を設定します。

## 41. ViewportはContext State

ResizeしてTextureを作り直してもViewportは自動更新されません。新しいSizeで`RSSetViewports`を再度呼びます。

## 42. 複数Viewport

APIは配列を受け取れます。Split Screen、Shadow Atlas、Editor Paneなどで複数領域を使います。Feature Levelによる制限を確認します。

## 43. ViewportとScissor Rectangle

Viewportは座標変換領域、ScissorはRasterize結果を切り取る矩形です。二つは別Stateで、Scissor有効化にはRasterizer Stateも関係します。

## 44. Aspect Ratio

```cpp
const float aspect =
    static_cast<float>(width) / static_cast<float>(height);
```

Resize後はProjection MatrixのAspect Ratioも更新します。Height 0の除算を避けます。

## 45. ViewportとProjectionを混同しない

ViewportはPixelへの写像、Projection MatrixはCamera空間をClip Spaceへ変換します。Window縦横比変更では両方の更新が必要です。

## 46. Resizeが必要になる契機

- `WM_SIZE`によるClient Area変更。
- DPI変更。
- Borderless Fullscreen切替。
- Render Resolution変更。
- Swap Chain再作成。

## 47. WM_SIZEから得る値

```cpp
const UINT width = LOWORD(lParam);
const UINT height = HIWORD(lParam);
```

これはMessageに含まれる新Client Sizeです。最終的な正確さを重視する場合は`GetClientRect`で再確認できます。

## 48. SIZE_MINIMIZED

```cpp
if (wParam == SIZE_MINIMIZED)
{
    minimized = true;
    return 0;
}
```

0×0 Resourceを作らず、描画とResizeを保留します。

## 49. 復帰時

`SIZE_RESTORED`や`SIZE_MAXIMIZED`で有効Sizeを記録し、Render ThreadへResize要求を渡します。

## 50. Window Procedureで直接GPU操作しない設計

Window ThreadとRender Threadが別なら、`WM_SIZE`では値だけをAtomic/Queueへ保存します。GPU Objectの破棄・再作成はRender Threadへ集約します。

## 51. Resize要求の統合

Interactive Resize中はMessageが連続します。最新Sizeだけを保持し、Frame境界で一度処理すると無駄な再作成を減らせます。

## 52. WM_ENTERSIZEMOVEとWM_EXITSIZEMOVE

Drag Resize開始・終了を記録し、操作中は描画を軽くする、終了時に一度だけ本ResizeするPolicyを作れます。

## 53. Resizeの事前条件

```text
width > 0
height > 0
device and context valid
swap chain valid
not already resizing
```

同じSizeなら処理を省略できます。

## 54. GPU Commandとの関係

Immediate Context上のCommand順序は保たれますが、Back Buffer参照の解除とResource寿命を正しく管理します。Resizeを複数Threadから同時実行しません。

## 55. Resize前のBinding解除

```cpp
context->OMSetRenderTargets(0, nullptr, nullptr);
```

Back Buffer RTVとDepth DSVをOutput Mergerから外します。

## 56. ViewをResetする

```cpp
backBufferRtv.Reset();
depthDsv.Reset();
depthTexture.Reset();
```

Back Buffer由来のSRV/UAVや一時Texture参照があれば、それらも解放します。

## 57. ResizeBuffers

```cpp
ThrowIfFailed(swapChain->ResizeBuffers(
    0,
    width,
    height,
    swapChainFormat,
    swapChainFlags));
```

Buffer Count 0は既存数を維持します。Flip ModelではFormatへ`DXGI_FORMAT_UNKNOWN`を使えないため、作成時Formatを保存して明示します。Flagsも作成時要件と整合させます。

## 58. ResizeBuffersの参照条件

Back Bufferへの直接・間接参照をすべて解放する必要があります。RTVをResetしても、Context SlotやCommand Listが参照していないか確認します。

## 59. Resize後のBack Buffer RTV

```cpp
Microsoft::WRL::ComPtr<ID3D11Texture2D> backBuffer;
ThrowIfFailed(swapChain->GetBuffer(
    0,
    IID_PPV_ARGS(backBuffer.GetAddressOf())));

ThrowIfFailed(device->CreateRenderTargetView(
    backBuffer.Get(),
    nullptr,
    backBufferRtv.GetAddressOf()));
```

古いRTVを再利用せず、新しいBufferから作り直します。

## 60. Depth Resource再作成

新しいWidth/Heightを使い、Depth TextureとDSVを作り直します。ColorだけResizeしてDepthが旧Sizeのままにならないようにします。

## 61. Viewport再設定

新しい寸法からViewportを組み立て、`RSSetViewports`を呼びます。保存した古いViewportを使い続けません。

## 62. Projection再計算

新Aspect RatioからProjection Matrixを更新し、Camera Constant Bufferへ反映します。これを忘れると映像が横伸び・縦伸びします。

## 63. Resize処理のTransaction

```text
release old size-dependent resources
-> ResizeBuffers
-> create all new resources locally
-> if all succeed, publish new surface state
```

途中失敗時に新旧Objectが混ざらない設計にします。

## 64. 再構築関数の例

```cpp
void ResizeSurface(UINT width, UINT height)
{
    if (width == 0 || height == 0)
    {
        return;
    }

    context_->OMSetRenderTargets(0, nullptr, nullptr);
    backBufferRtv_.Reset();
    depthDsv_.Reset();
    depthTexture_.Reset();

    ThrowIfFailed(swapChain_->ResizeBuffers(
        0, width, height, swapChainFormat_, swapChainFlags_));

    CreateBackBufferRtv(width, height);
    CreateDepthStencil(width, height);
    SetViewport(width, height);
    UpdateProjection(width, height);
}
```

実装では各作成結果とDevice Removedを分類します。

## 65. ClearStateを使う場合

複雑なRendererでどこに参照が残ったか不明なら`ClearState`で全Stageを解除できます。ただしStateを再設定する責任が増えるため、Resize境界など限定的に使います。

## 66. Deferred ContextとCommand List

未実行Command Listが古いBack Buffer Viewを参照している場合があります。Resize前にWorker記録を停止し、古いCommand Listを破棄または実行完了させます。

## 67. Size依存Resourceを分類する

- Back Buffer RTV。
- Depth Texture / DSV / Depth SRV。
- Scene Color。
- G-buffer。
- Post Process中間Texture。
- UI Render Target。
- Viewport / Scissor。
- ProjectionのAspect依存値。

## 68. 固定解像度Rendering

Window Sizeと3D Render Resolutionを分ける方式です。Scene ColorとDepthは固定またはScale寸法、Back BufferだけWindow寸法にして最終合成します。

## 69. Dynamic Resolution

GPU負荷に応じてScene Render Sizeを変えます。Back Buffer Resizeとは別経路にし、DepthやPost Process Textureだけを再作成できる設計が有効です。

## 70. LetterboxとPillarbox

Aspect Ratioを固定する場合、Viewportを中央の一部へ設定し、余白をClearします。Window全体へ無理に引き伸ばす必要はありません。

## 71. Split Screen

同じRTV/DSVへ複数Viewportを順に設定して描画できます。各CameraのViewport、Scissor、Projection Aspectを個別に管理します。

## 72. Depth Precision

Near Planeを極端にCameraへ近づけ、Far Planeを非常に遠くすると精度分布が厳しくなります。Format bit数だけでなくProjection範囲を適切にします。

## 73. Z-fighting

ほぼ同じDepthのSurfaceが交互に見える現象です。Near/Far範囲、Geometry重複、Depth Bias、Format精度、Reversed-Zを検討します。

## 74. Depthを透明描画で扱う

透明Objectは通常、不透明物のDepthをTestしつつDepth Writeを無効にし、奥から手前へ並べます。DSVを外すのではなくStateを変更します。

## 75. UI描画とDepth

2D UIではDepth Testを無効にするか、専用Depth規約を使います。3D PassのDepth Stateを引き継いでUIが消えないよう、PassごとにStateを明示します。

## 76. Shadow Mapとの共通点

Shadow MapもLight視点のDepthをTextureへ描きます。Window Back Bufferとは異なる寸法・Format・Viewportを使うため、PassごとのTarget Setを管理します。

## 77. Debug表示

Depth SRVを作り、値を可視化するPixel Shaderで画面へ表示すると、Clear忘れ、Projection誤り、精度問題を発見しやすくなります。

## 78. Debug Name

```cpp
SetDebugName(*depthTexture.Get(), "Main Depth Texture");
SetDebugName(*depthDsv.Get(), "Main Depth DSV");
```

SizeやFormatを名前またはLogへ含めると再作成履歴を追いやすくなります。

## 79. Resize Log

```text
reason: WM_SIZE / DPI / fullscreen / dynamic-resolution
old size
new size
buffer count and flags
depth format
result HRESULT
```

連続Resizeや0 Sizeの原因を診断できます。

## 80. よくある失敗：DepthをClearしない

前FrameのDepth値が残り、新しいObjectが不規則に欠けます。通常はFrame開始時にDepthをClearします。

## 81. よくある失敗：Depth Clear値と比較関数が逆

通常Zで0 Clear＋LESS、またはReversed-Zで1 Clear＋GREATERなど、規約が不一致になります。Projection、Clear、Compareを一組で定義します。

## 82. よくある失敗：Viewport更新忘れ

RTVだけResizeし、描画が旧Sizeの一部にしか出ません。ViewportとProjectionを同じResize Transactionで更新します。

## 83. よくある失敗：Height 0でAspect計算

最小化時に0除算し、ProjectionへNaNが入ります。有効Size判定を最初に行います。

## 84. よくある失敗：ResizeBuffers前の参照残り

OM Binding、RTV、Back Buffer Texture、SRV、Command Listのどれかが残ります。Debug LayerとLive Object情報を使って参照元を探します。

## 85. よくある失敗：作成時Flagsを失う

TearingやFrame Latency Waitable ObjectなどSwap Chain FlagをResize時に不整合な値へ変えます。作成設定を保存し、API要件どおり渡します。

## 86. Depth作成テスト

- Width/HeightがBack Bufferと一致する。
- FormatがDepth Stencil用途をSupportする。
- Sample Count/QualityがColor Targetと一致する。
- TextureとDSV作成が成功する。
- Clear後のDepth値が期待どおりである。

## 87. Viewportテスト

- 全画面Viewportが新Sizeを覆う。
- MinDepth/MaxDepthが規約どおりである。
- Resize後にAspectが更新される。
- Split Screenの各領域が重複しない。
- Letterbox余白が意図した色になる。

## 88. Resizeテスト

- 最大化、復元、最小化から復帰する。
- Window端を連続DragしてもCrashしない。
- 同じSize要求を安全に省略できる。
- 0×0要求を保留できる。
- 古いViewの参照を残さない。
- Device Removedを通常Resizeと区別する。

## 89. 完成確認表

- [ ] Depth TestとDepth Writeを区別できる。
- [ ] DepthとStencilの用途を説明できる。
- [ ] Depth Texture Descriptorを組み立てられる。
- [ ] DSVを作成してRTVと同時Bindingできる。
- [ ] Clear Flag、Depth値、Stencil値を説明できる。
- [ ] ViewportとProjection Matrixを区別できる。
- [ ] ResizeでSize依存Resourceを列挙できる。
- [ ] `ResizeBuffers`前のすべての参照を解除できる。
- [ ] 0×0 Sizeと最小化を安全に扱える。
- [ ] Resize後にRTV、DSV、Viewport、Projectionを再構築できる。

## 90. この章の要点

- Depth BufferはFragmentごとの奥行きを保存し、隠面を判定します。
- DSVはDepth Stencil ResourceをOutput Mergerで使うViewです。
- Depth ResourceとColor Targetは寸法とSample設定をそろえます。
- ViewportはTextureではなくRasterizerの座標変換Stateです。
- ResizeではBack Bufferへのすべての参照を外してから`ResizeBuffers`を呼びます。
- RTV、Depth、Viewport、Projectionを同じSize変更処理で再構築します。
- 最小化中の0×0 SizeはResource作成せず保留します。
- Render ResolutionとWindow Resolutionを分離すると高度な描画へ拡張しやすくなります。

## 91. 公式資料

- [ID3D11Device::CreateTexture2D](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11device-createtexture2d)
- [D3D11_TEXTURE2D_DESC](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/ns-d3d11-d3d11_texture2d_desc)
- [ID3D11Device::CreateDepthStencilView](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11device-createdepthstencilview)
- [D3D11_DEPTH_STENCIL_VIEW_DESC](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/ns-d3d11-d3d11_depth_stencil_view_desc)
- [ID3D11DeviceContext::ClearDepthStencilView](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11devicecontext-cleardepthstencilview)
- [ID3D11DeviceContext::RSSetViewports](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11devicecontext-rssetviewports)
- [D3D11_VIEWPORT](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/ns-d3d11-d3d11_viewport)
- [Configuring depth-stencil functionality](https://learn.microsoft.com/en-us/windows/win32/direct3d11/d3d10-graphics-programming-guide-depth-stencil)
- [IDXGISwapChain::ResizeBuffers](https://learn.microsoft.com/en-us/windows/win32/api/dxgi/nf-dxgi-idxgiswapchain-resizebuffers)
- [WM_SIZE message](https://learn.microsoft.com/en-us/windows/win32/winmsg/wm-size)

次章では、HLSL SourceをShader ModelへCompileし、Compiler Message、Blob、Reflection、Input情報、Resource Binding情報を取得する方法を扱います。
