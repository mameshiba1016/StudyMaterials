# DirectX 11：Back Buffer・Render Target View

この章では、Swap Chainが所有するBack Bufferを取得し、Render Target Viewを作成して、Output Merger Stageへ設定します。ResourceとViewの違い、Format解釈、Clear、複数Render Target、Binding競合、参照寿命、Resize前の解放までを詳しく扱います。

## 1. 最初に覚える処理の流れ

```text
Swap Chain
-> GetBufferでBack Buffer Textureを取得
-> DeviceでRender Target Viewを作成
-> ContextでOutput MergerへBinding
-> Clear / Draw
-> Present
```

作成はDevice、使用はContext、表示はSwap Chainという責任分担です。

## 2. Back Bufferとは

Back Bufferは、次に画面へ表示する色を描く2次元Textureです。Swap Chainが本体を所有し、ApplicationはCOM Interfaceを通して参照します。

## 3. TextureとViewは別物

```text
ID3D11Texture2D       = Memory、寸法、Format、Mip、Sample情報を持つResource
ID3D11RenderTargetView = そのResourceを描画出力として解釈するView
```

ViewはPixel Dataの複製ではありません。同じResourceを見る用途別の窓口です。

## 4. なぜViewが必要か

GPU Pipelineは同じResourceをRender Target、Shader Resource、Depth Stencilなど異なる役割で扱います。Viewにより、どのSubresourceをどのFormatと用途で使うかを指定できます。

## 5. 基本の所有変数

```cpp
Microsoft::WRL::ComPtr<ID3D11Texture2D> backBuffer;
Microsoft::WRL::ComPtr<ID3D11RenderTargetView> backBufferRtv;
```

長期的にTexture本体を保持する必要がなければ、RTV作成後に一時的な`backBuffer`を解放できます。RTV自身がResource参照を保持します。

## 6. IDXGISwapChain::GetBuffer

```cpp
ThrowIfFailed(swapChain->GetBuffer(
    0,
    IID_PPV_ARGS(backBuffer.GetAddressOf())));
```

Buffer番号、要求Interface ID、出力Pointerを渡します。

## 7. Buffer番号0

Direct3D 11でSwap Chainの描画対象を得る一般的な呼び出しは`GetBuffer(0, ...)`です。Buffer Countが2だから毎Frame 0と1を交互に指定する、という意味ではありません。

## 8. IID_PPV_ARGS

```cpp
IID_PPV_ARGS(backBuffer.GetAddressOf())
```

要求InterfaceのIIDと`void**`相当の出力を、型情報から組み立てるMacroです。Interfaceと出力型の取り違えを減らします。

## 9. GetAddressOfとReleaseAndGetAddressOf

- `GetAddressOf()`：現在のPointerを解放せず、格納先Addressを得ます。
- `ReleaseAndGetAddressOf()`：現在の参照を解放してから格納先Addressを得ます。

再作成時に既存値が残る可能性があるなら、先に`Reset()`するか`ReleaseAndGetAddressOf()`を使います。

## 10. GetBufferの参照寿命

成功するとCOM参照Countが増えます。`ComPtr`がScopeを抜けるか`Reset()`されるまでBack Bufferへの外部参照が残ります。

## 11. Texture Descriptorを調べる

```cpp
D3D11_TEXTURE2D_DESC textureDesc{};
backBuffer->GetDesc(&textureDesc);
```

戻り値のない取得Methodです。Width、Height、Format、Sample Count、Bind Flagsなどを診断できます。

## 12. 記録すべきDescriptor情報

```cpp
Log(textureDesc.Width);
Log(textureDesc.Height);
Log(textureDesc.Format);
Log(textureDesc.SampleDesc.Count);
Log(textureDesc.BindFlags);
```

Swap Chainへ要求した値と実際のResourceが一致するか確認します。

## 13. Render Target Viewとは

RTVはOutput Merger Stageが色出力を書き込むためのViewです。Pixel Shaderの出力が、Blend処理などを経てRTVの対応Pixelへ格納されます。

## 14. CreateRenderTargetView

```cpp
ThrowIfFailed(device->CreateRenderTargetView(
    backBuffer.Get(),
    nullptr,
    backBufferRtv.GetAddressOf()));
```

第1引数はResource、第2引数はView Descriptor、第3引数は生成されたViewの出力です。

## 15. View Descriptorをnullptrにする意味

`nullptr`ならResource作成情報から既定Viewを作ります。単一Mip、単一Array Sliceの通常Back Bufferでは最も簡潔です。

## 16. 明示的なRTV Descriptor

```cpp
D3D11_RENDER_TARGET_VIEW_DESC rtvDesc{};
rtvDesc.Format = textureDesc.Format;
rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
rtvDesc.Texture2D.MipSlice = 0;

ThrowIfFailed(device->CreateRenderTargetView(
    backBuffer.Get(),
    &rtvDesc,
    backBufferRtv.GetAddressOf()));
```

学習や特殊Format解釈では明示Descriptorが役立ちます。

## 17. RTV Descriptorの構成

```text
Format
ViewDimension
DimensionごとのUnion member
```

`ViewDimension`と異なるUnion Memberへ値を書かないようにします。

## 18. ViewDimension

通常の非MSAA Back Bufferは`D3D11_RTV_DIMENSION_TEXTURE2D`です。MSAA Textureなら`TEXTURE2DMS`を使い、Mip Slice指定はありません。

## 19. MipSlice

Render Targetとして使うMip Levelを指定します。Swap Chain Back Bufferは通常Mip 0を使います。

## 20. Array Slice

Texture Arrayでは、最初のSliceと個数をViewで選べます。通常Back Bufferは単一TextureなのでArray用Dimensionは使いません。

## 21. Resource FormatとView Format

View FormatはResource Formatと互換でなければなりません。任意のFormatへ変換できるわけではありません。

## 22. Typed Format

`DXGI_FORMAT_R8G8B8A8_UNORM`のようにComponentと数値解釈が定まったFormatです。既定ViewはResourceのTyped Formatを引き継ぎます。

## 23. Typeless Format

Typeless Resourceは、互換なTyped Viewによって用途ごとの解釈を与えます。ただしSwap Chain Formatには制約があり、通常のBack Bufferで安易にTypelessを選びません。

## 24. UNORMの意味

Unsigned Normalized Integerです。8 bit値をShader上の0.0から1.0へ対応させて扱います。整数値をそのまま0から255として扱う`UINT`とは異なります。

## 25. sRGB View

sRGB RTVではShader出力を保存するときにLinearからsRGBへの変換が関係します。Texture Format、View Format、Shader計算、Swap Chain Color Spaceを一体で設計します。

## 26. Gammaを二重適用しない

Shaderで手動Gamma補正し、さらにsRGB RTV変換を使うと二重変換になります。Renderer内のColor Space規約を一つに決めます。

## 27. HDRとの境界

HDRはRTV Formatを変えるだけでは完成しません。Swap Chain Format、Color Space、Display能力、Tone Mapping、Metadataなどが関係します。本章ではSDR Back Bufferを基準にします。

## 28. CreateRenderTargetViewの検証専用呼出し

出力Pointerを`nullptr`にしてDescriptorの妥当性確認だけを行えるAPI規約があります。ただし通常実装では実際のViewを受け取り、Debug LayerのMessageも確認します。

## 29. RTVがResource寿命を保持する

RTVは元Resourceへの参照を持ちます。そのため一時`ComPtr<ID3D11Texture2D>`を解放してもRTVは有効ですが、Resize時にはRTVを解放しない限りBack Buffer参照が残ります。

## 30. 一時Back Buffer方式

```cpp
Microsoft::WRL::ComPtr<ID3D11Texture2D> backBuffer;
ThrowIfFailed(swapChain->GetBuffer(
    0,
    IID_PPV_ARGS(backBuffer.GetAddressOf())));
ThrowIfFailed(device->CreateRenderTargetView(
    backBuffer.Get(), nullptr, backBufferRtv.GetAddressOf()));
// Scope終了時にTextureの直接参照だけ解放される。
```

RTVが必要な参照を保持します。

## 31. Output Merger Stage

Output MergerはPixel Shader出力、Render Target、Depth Stencil、Blend State、Depth Stencil Stateなどを組み合わせるPipeline終端です。

## 32. OMSetRenderTargets

```cpp
ID3D11RenderTargetView* rtvs[] = {backBufferRtv.Get()};
context->OMSetRenderTargets(1, rtvs, nullptr);
```

第1引数はRTV数、第2引数は生Pointer配列、第3引数はDepth Stencil Viewです。

## 33. 単一RTVの短い書き方

```cpp
ID3D11RenderTargetView* rtv = backBufferRtv.Get();
context->OMSetRenderTargets(1, &rtv, nullptr);
```

`ComPtr`のAddressを渡すのではなく、`Get()`で得た生Pointer変数のAddressを渡します。

## 34. ComPtrのAddressを誤用しない

```cpp
// 意図が異なるため避ける。
context->OMSetRenderTargets(1, backBufferRtv.GetAddressOf(), nullptr);
```

動作しそうに見えても、API入力配列とCOM出力先を区別します。入力には明示的な生Pointer配列を作ると読みやすくなります。

## 35. Bindingは参照を保持する

ContextへRTVをBindingすると、Context StateがそのObjectを参照します。Application側`ComPtr`をResetしただけでは、Pipelineから参照が外れない場合があります。

## 36. Context Stateという考え方

`OMSetRenderTargets`は即座にPixelを書き込む命令ではなく、以降のDrawが使うOutput Stateを設定します。Draw時点の全Pipeline StateがCommandになります。

## 37. ClearRenderTargetView

```cpp
const float clearColor[4] = {0.05f, 0.08f, 0.12f, 1.0f};
context->ClearRenderTargetView(backBufferRtv.Get(), clearColor);
```

RGBA順の4要素Floatを渡します。

## 38. Clear Colorの値域

一般的なUNORM Render Targetでは0.0から1.0を基準にします。HDRやFloat Targetでは1.0を超える値を使う場合もあります。

## 39. Clearは描画命令

ClearもGPU Commandとして記録・実行されます。CPUがTexture全PixelをLoopしているわけではありません。

## 40. 毎Frame Clearする理由

Flip DiscardではPresent後の内容を保持すると仮定できません。画面全域を確実に上書きしないFrameではClearが未定義領域を防ぎます。

## 41. Clear省略が有効な場合

Fullscreen Passなどで全Pixelを必ず上書きするならClearを省ける場合があります。ただしScissor、Discard、描画漏れを含め、実測と正しさを確認します。

## 42. ClearとBindingの順序

`ClearRenderTargetView`はViewを引数で直接指定するため、Clear自体の前にOMへBindingする必要はありません。ただしDraw前にはBindingが必要です。

## 43. 最小描画Frame

```cpp
void RenderFrame(
    ID3D11DeviceContext& context,
    ID3D11RenderTargetView& rtv)
{
    const float color[4] = {0.02f, 0.03f, 0.05f, 1.0f};
    context.ClearRenderTargetView(&rtv, color);

    ID3D11RenderTargetView* outputs[] = {&rtv};
    context.OMSetRenderTargets(1, outputs, nullptr);

    // Viewport、Shader、Bufferなどを設定してDrawする。
}
```

Presentはこの関数の後で行います。

## 44. Viewportも必要

RTVをBindingしただけではRasterizerの出力範囲は決まりません。`RSSetViewports`でBack Buffer寸法に合うViewportを設定します。詳細は次章で扱います。

## 45. Depth Stencilも必要になる

3D描画では奥行きを正しく処理するDepth Stencil ViewをRTVと同時にBindingします。

```cpp
context->OMSetRenderTargets(1, rtvs, depthStencilView.Get());
```

## 46. Multiple Render Targets

```cpp
ID3D11RenderTargetView* rtvs[] =
{
    colorRtv.Get(),
    normalRtv.Get(),
    materialRtv.Get()
};
context->OMSetRenderTargets(3, rtvs, depthDsv.Get());
```

Deferred RenderingなどでPixel Shaderから複数Targetへ出力できます。

## 47. MRTの同時条件

同時BindingするRender Targetは寸法やSample設定などの互換条件を満たす必要があります。Debug LayerのWarningを確認します。

## 48. Pixel Shader出力との対応

```hlsl
struct PixelOutput
{
    float4 color  : SV_Target0;
    float4 normal : SV_Target1;
};
```

`SV_Target0`がRTV Slot 0、`SV_Target1`がSlot 1へ対応します。

## 49. 最大RTV数

Direct3D 11では複数のRender Target Slotがあります。必要数だけBindingし、Hardware Feature Levelの保証範囲を意識します。

## 50. Slotの穴

配列要素へ`nullptr`を入れて未使用Slotにできます。ただしShader出力とSlot設計を一致させ、意図しない穴を作りません。

## 51. RTVを解除する

```cpp
context->OMSetRenderTargets(0, nullptr, nullptr);
```

Output MergerからすべてのRender TargetとDepth Stencilを外します。

## 52. 明示解除が必要な場面

- Resize前にBack Buffer参照を外す。
- 同Resourceを別用途でBindingする前。
- Scene/Rendererを破棄する前。
- Debug時にStateの境界を明確化したい場合。

## 53. Read/Write Binding競合

同じSubresourceをRTVとして書き込みながら、Shader Resource Viewとして読み取ることはできません。InputとOutputが同じMemoryを同時使用する競合です。

## 54. Debug Layerによる自動解除

競合するViewをBindingすると、Runtimeが一方を`nullptr`へ変更しWarningを出す場合があります。自動的に直ったように見えても、RendererのBinding設計にBugがあります。

## 55. SRVを明示解除する例

```cpp
ID3D11ShaderResourceView* nullSrvs[] = {nullptr};
context->PSSetShaderResources(0, 1, nullSrvs);
```

同ResourceをRTVへ戻す前に、以前のShader Input Slotから外します。

## 56. Hazard Tracking

Direct3D 11 RuntimeはResource BindingのHazardを追跡します。これは便利ですが、無駄な競合Bindingを繰り返してよい理由にはなりません。

## 57. ResourceとSubresource

Texture全体がResource、特定Mip LevelやArray SliceがSubresourceです。Viewは対象範囲を限定できるため、競合判定では重なるSubresource範囲が重要です。

## 58. Back BufferをSRVにしたい場合

Swap Chainの`BufferUsage`とFormatが用途を許す必要があります。ただしPost Processでは、別のScene Color Textureへ描いてからBack Bufferへ合成する設計が一般的です。

## 59. Scene Colorを分ける利点

- HDR形式で中間描画できる。
- Post Processを連鎖できる。
- Resolution Scaleを独立させられる。
- Back Bufferへの依存を最後のPassへ限定できる。

## 60. Back Bufferへの最終Pass

```text
3D Scene -> HDR Scene Color
Post Effects -> temporary textures
Tone Mapping / UI Composite -> Back Buffer RTV
Present
```

高速3D Actionでもよく使う基本構造です。

## 61. ResizeBuffersが失敗する代表原因

Back Bufferを参照するRTV、SRV、一時Texture Interface、Context Bindingが残っているとResizeできません。

## 62. Resize前の解放順序

```text
Stop issuing draws
-> OMからRTV/DSVを解除
-> 必要ならSRV/UAVも解除
-> Back Buffer由来のViewをReset
-> Back Buffer Textureの直接参照をReset
-> ResizeBuffers
```

## 63. ClearStateを使う判断

```cpp
context->ClearState();
```

全Pipeline Bindingを外せます。Resizeや全面再初期化では確実ですが、通常Frame中に乱用するとStateをすべて再設定するCostと複雑性が増えます。

## 64. Flushは参照解除ではない

`Flush()`を呼んでもApplicationやContextが持つCOM参照は自動で消えません。Binding解除と`ComPtr::Reset()`を明示します。

## 65. Resize後の再作成

```text
ResizeBuffers success
-> GetBuffer(0)
-> CreateRenderTargetView
-> recreate Depth Texture / DSV
-> update Viewport
-> resume rendering
```

Window size依存Resourceを一まとまりにします。

## 66. 0×0 Sizeを拒否する

最小化中はClient Width/Heightが0になる場合があります。Resize処理を保留し、有効なSizeへ戻ってからBack Buffer関連Resourceを再作成します。

## 67. Debug Nameを付ける

```cpp
constexpr char name[] = "Main Back Buffer RTV";
backBufferRtv->SetPrivateData(
    WKPDID_D3DDebugObjectName,
    static_cast<UINT>(sizeof(name) - 1),
    name);
```

Graphics DebuggerやLive Object Reportで識別しやすくなります。

## 68. Helper関数にする

```cpp
void SetDebugName(ID3D11DeviceChild& object, std::string_view name)
{
    object.SetPrivateData(
        WKPDID_D3DDebugObjectName,
        static_cast<UINT>(name.size()),
        name.data());
}
```

空文字、長すぎる名前、Release Buildでの有無をProject方針として決めます。

## 69. Back Buffer一式の構造体

```cpp
struct BackBufferSurface
{
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> rtv;
    DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
    UINT width = 0;
    UINT height = 0;
};
```

Texture本体を保持しない場合でも、描画に必要なMetadataは保存します。

## 70. 作成関数の例

```cpp
BackBufferSurface CreateBackBufferSurface(
    ID3D11Device& device,
    IDXGISwapChain1& swapChain)
{
    Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
    ThrowIfFailed(swapChain.GetBuffer(
        0,
        IID_PPV_ARGS(texture.GetAddressOf())));

    D3D11_TEXTURE2D_DESC desc{};
    texture->GetDesc(&desc);

    BackBufferSurface surface{};
    ThrowIfFailed(device.CreateRenderTargetView(
        texture.Get(), nullptr, surface.rtv.GetAddressOf()));

    surface.format = desc.Format;
    surface.width = desc.Width;
    surface.height = desc.Height;
    return surface;
}
```

失敗時には不完全なSurfaceを公開しない例です。

## 71. Reset関数

```cpp
void ResetBackBuffer(
    ID3D11DeviceContext& context,
    BackBufferSurface& surface)
{
    context.OMSetRenderTargets(0, nullptr, nullptr);
    surface.rtv.Reset();
    surface.width = 0;
    surface.height = 0;
    surface.format = DXGI_FORMAT_UNKNOWN;
}
```

ほかのStageにも同ResourceのViewがあれば、それらも解除します。

## 72. Render Target State Cache

同じRTVを何度もBindingしない最適化は可能ですが、外部CodeがContext Stateを変更するとCacheと実状態がずれます。Context操作をRendererへ集約してから導入します。

## 73. Deferred Contextとの関係

Deferred ContextでRTV BindingとDrawを記録できます。Command List実行時の順序、Immediate Context State復元方針、Resource寿命を明確にします。

## 74. UAVとの同時Binding

Output MergerにはRTVとUAVを扱う拡張Methodがあります。同一Subresourceの競合、Slot範囲、Feature対応を確認し、単純なBack Buffer描画から段階的に進めます。

## 75. Clear Colorの設計利用

Debug中はPassごとに特徴的なClear Colorを使うと、未描画領域やPass未実行を目視できます。最終版では意図した背景色へ戻します。

## 76. Alpha Channel

不透明WindowでAlphaが表示合成に使われなくても、Back BufferのAlpha値はPost ProcessやCaptureで意味を持つ場合があります。通常はClear Alphaを1.0にします。

## 77. CaptureとScreenshot

Back BufferはCPUから直接読めないことが一般的です。Staging TextureへCopyし、GPU完了後にMapして読む処理が必要です。表示用RTVの責任とは分離します。

## 78. よくある失敗：Textureへ直接Drawできると思う

Texture Interfaceを得ただけでOutputになると誤解します。対応するRTVを作り、OM StageへBindingします。

## 79. よくある失敗：GetBufferのIndexをFrameごとに増やす

Swap Chain Buffer Countを配列Indexとして手動管理します。Direct3D 11の通常経路では`GetBuffer(0)`から描画対象Viewを作ります。

## 80. よくある失敗：RTVを作るたびにTextureが複製されると思う

ViewはResourceへの解釈です。Memory量や寿命を考えるときはResourceとViewを分けます。

## 81. よくある失敗：Clearだけで表示されると思う

ClearはBack BufferへCommandを出しますが、Windowへ表示するにはPresentが必要です。

## 82. よくある失敗：RTVだけで三角形が出ると思う

Viewport、Shader、Input Assembly、Rasterizerなど他Pipeline Stateも必要です。RTVは出力先だけを決めます。

## 83. よくある失敗：Resize前にComPtrだけReset

ContextのOM SlotにBindingが残っています。Pipelineから外し、すべてのBack Buffer派生Viewと直接参照を解放します。

## 84. よくある失敗：同じTextureを読み書きする

Post Process入力SRVを解除せず同TextureのRTVへ戻します。Pass境界でInput/Output Bindingを明示します。

## 85. 作成テスト

- `GetBuffer(0)`がTexture2Dを返す。
- Descriptor寸法がClient Pixel Sizeと一致する。
- FormatとSample Countが期待値に一致する。
- RTV作成がDebug Layer Warningなしで成功する。
- 一時Texture参照解放後もRTVを使用できる。

## 86. 描画テスト

- Clear Colorが画面全体へ表示される。
- 毎Frame異なるClear Colorを表示できる。
- OM Slot 0へ期待RTVがBindingされる。
- Present前後でErrorがない。
- Resize後も新しい全領域をClearできる。

## 87. 競合テスト

- SRVとして使用後に明示解除してRTVへ戻せる。
- Debug LayerにInput/Output競合Warningがない。
- 複数PassでViewのBinding順が再現可能である。
- Context State Cacheと実Bindingが一致する。

## 88. Resizeテスト

- OM Bindingを解除する。
- Back Buffer由来ViewをすべてResetする。
- 一時Texture参照を残さない。
- 0×0 SizeではResizeを保留する。
- 新SizeでRTVとMetadataを再構築する。

## 89. 完成確認表

- [ ] ResourceとViewの違いを説明できる。
- [ ] `GetBuffer(0)`の引数を説明できる。
- [ ] `IID_PPV_ARGS`の目的を説明できる。
- [ ] 既定RTVと明示Descriptorを使い分けられる。
- [ ] Texture FormatとView Formatの互換性を意識できる。
- [ ] RTVをOM Stageへ正しくBindingできる。
- [ ] Clear ColorのRGBAと値域を説明できる。
- [ ] RTV/SRV同時Binding競合を回避できる。
- [ ] RTVがResource寿命を保持することを理解している。
- [ ] Resize前にContext参照とCOM参照を解放できる。

## 90. この章の要点

- Back BufferはSwap Chain所有のTexture2Dです。
- RTVはTextureを描画出力として解釈するViewで、Pixel Dataの複製ではありません。
- DeviceでViewを作り、ContextでOutput MergerへBindingします。
- ClearはGPU Commandであり、表示にはPresentが別途必要です。
- 同一SubresourceをRTVとして書きながらSRVとして読むことはできません。
- Context BindingもCOM参照を保持するため、Resize前に明示解除します。
- Post Processを行うRendererではScene ColorとBack Bufferを分離すると拡張しやすくなります。
- DescriptorとDebug Nameを記録するとGraphics Debuggingが容易になります。

## 91. 公式資料

- [IDXGISwapChain::GetBuffer](https://learn.microsoft.com/en-us/windows/win32/api/dxgi/nf-dxgi-idxgiswapchain-getbuffer)
- [ID3D11Device::CreateRenderTargetView](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11device-createrendertargetview)
- [D3D11_RENDER_TARGET_VIEW_DESC](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/ns-d3d11-d3d11_render_target_view_desc)
- [ID3D11DeviceContext::OMSetRenderTargets](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11devicecontext-omsetrendertargets)
- [ID3D11DeviceContext::ClearRenderTargetView](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11devicecontext-clearrendertargetview)
- [Render targets](https://learn.microsoft.com/en-us/windows/win32/direct3d11/overviews-direct3d-11-resources-bind-flags)
- [Introduction to a resource in Direct3D 11](https://learn.microsoft.com/en-us/windows/win32/direct3d11/overviews-direct3d-11-resources-intro)
- [Introduction to a view in Direct3D 11](https://learn.microsoft.com/en-us/windows/win32/direct3d11/overviews-direct3d-11-resources-intro)
- [DXGI formats](https://learn.microsoft.com/en-us/windows/win32/direct3ddxgi/dxgi-format)
- [IDXGISwapChain::ResizeBuffers](https://learn.microsoft.com/en-us/windows/win32/api/dxgi/nf-dxgi-idxgiswapchain-resizebuffers)

次章では、Depth Stencil TextureとView、Viewport、Window Resize時のResource再構築をまとめて扱います。
