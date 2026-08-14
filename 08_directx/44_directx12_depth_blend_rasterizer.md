# DirectX 12 第12章：Depth・Blend・Rasterizer

この章では、Triangleが画面へ残るまでの固定機能Stageを学びます。Depth/Stencil Test、Blend、Rasterizer、Cull、Scissor、MSAA、透明描画、PSO管理、描画順、性能計測を一つの描画設計へ統合します。

## 1. この章の到達目標

- 手前のObjectだけを正しく表示する。
- 不透明、半透明、加算、UIを目的別に合成する。
- Cull、Fill、Depth Bias、Scissorを説明して設定する。
- Depth/Blend/Rasterizerを含むPSOを安全に管理する。
- 描画結果の破綻とGPU負荷を切り分ける。

## 2. Fragmentが残るまで

```text
Vertex Shader
 -> Primitive Assembly
 -> Rasterizer/Culling/Clipping
 -> Pixel Shader
 -> Depth/Stencil Test
 -> Blend
 -> Render Target
```

Early-Z等で実際の順序は最適化されます。概念順とHardware実行順を完全に同一だと思わないことが重要です。

## 3. PSOに含まれる固定機能State

```text
D3D12_GRAPHICS_PIPELINE_STATE_DESC
  RasterizerState
  BlendState
  DepthStencilState
  SampleMask
  PrimitiveTopologyType
  NumRenderTargets / RTVFormats
  DSVFormat
  SampleDesc
```

D3D12ではこれらを描画直前に個別変更せず、Pipeline State Objectとして事前に組み合わせます。

## 4. PSOと描画契約

Shaderだけ同じでもDepth WriteやBlendが違えば別PSOです。Render Target Format、Depth Format、Sample CountもPSOと実際のAttachmentで一致させます。

## 5. Depth Bufferの役割

各PixelでCameraから見た奥行きを保持し、新しいFragmentを残すか捨てるか判定します。Color Bufferとは別Resourceです。

## 6. Depth Resource作成例

```cpp
D3D12_RESOURCE_DESC desc{};
desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
desc.Width = width;
desc.Height = height;
desc.DepthOrArraySize = 1;
desc.MipLevels = 1;
desc.Format = DXGI_FORMAT_D32_FLOAT;
desc.SampleDesc = {1, 0};
desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

D3D12_CLEAR_VALUE clearValue{};
clearValue.Format = DXGI_FORMAT_D32_FLOAT;
clearValue.DepthStencil.Depth = 1.0f;
clearValue.DepthStencil.Stencil = 0;
```

Optimized Clear Valueは頻繁に使うClear値と合わせます。

## 7. DSV Descriptor Heap

```cpp
D3D12_DESCRIPTOR_HEAP_DESC heapDesc{};
heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
heapDesc.NumDescriptors = 1;
heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&dsvHeap));
```

DSV HeapはShader Visibleにしません。

## 8. DSV作成

```cpp
D3D12_DEPTH_STENCIL_VIEW_DESC dsv{};
dsv.Format = DXGI_FORMAT_D32_FLOAT;
dsv.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
dsv.Flags = D3D12_DSV_FLAG_NONE;
device->CreateDepthStencilView(depth.Get(), &dsv, dsvHandle);
```

Resource FormatとView Formatの互換性を守ります。

## 9. Depth Formatの選択

```text
D32_FLOAT             : 32-bit float depth
D24_UNORM_S8_UINT     : 24-bit depth + 8-bit stencil
D32_FLOAT_S8X24_UINT  : 高精度depth + stencil
D16_UNORM             : 容量重視
```

Stencilが不要ならD32_FLOATが単純です。必要機能、精度、Memory、Hardware Supportから選びます。

## 10. Typeless Resourceと複数View

Depthを後でShaderから読む場合、ResourceをTypeless Formatで作り、DSVはDepth Format、SRVは対応する読取りFormatとして作る方法があります。

## 11. Depth State遷移

```text
Clear/Write : D3D12_RESOURCE_STATE_DEPTH_WRITE
Read only   : D3D12_RESOURCE_STATE_DEPTH_READ
Shader read : PIXEL_SHADER_RESOURCE等との互換なRead State
```

同時用途の可否とSubresource範囲を追跡します。

## 12. ClearDepthStencilView

```cpp
commandList->ClearDepthStencilView(
    dsvHandle,
    D3D12_CLEAR_FLAG_DEPTH,
    1.0f,
    0,
    0,
    nullptr);
```

通常DepthはFrame開始時にClearします。Stencil使用時はFlagを追加します。

## 13. OMSetRenderTargets

```cpp
commandList->OMSetRenderTargets(
    1,
    &rtvHandle,
    FALSE,
    &dsvHandle);
```

PSOのRTV/DSV Formatと実際にBindするViewを一致させます。

## 14. Depth Enable・Write・Func

```cpp
D3D12_DEPTH_STENCIL_DESC state{};
state.DepthEnable = TRUE;
state.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
state.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
state.StencilEnable = FALSE;
```

Testを行うか、合格時に書くか、どの比較を合格とするかは別設定です。

## 15. Comparison Function

```text
LESS / LESS_EQUAL       : 通常Z
GREATER / GREATER_EQUAL : Reversed-Z
EQUAL                   : Depth Pre-passとの一致等
ALWAYS                  : 常に通す
NEVER                   : 常に落とす
```

ProjectionとClear値を含むDepth規約全体で統一します。

## 16. Depth TestとDepth Writeの違い

透明Objectは既存Depthに対してTestしつつ、後続の透明Objectを不当に隠さないようWriteを無効にすることが一般的です。

## 17. 通常Z

Near側が小さくFar側が大きい設計ではClearを1、比較をLESS/LESS_EQUALにします。

## 18. Reversed-Z

Near側を大きくFar側を小さくし、Clearを0、比較をGREATER/GREATER_EQUALにします。Float Depthの精度分布を活用しやすくなります。

## 19. Near Planeの重要性

Nearを極端に小さくするとDepth精度が悪化します。Cameraが必要とする範囲に合わせ、意味なく0へ近づけません。

## 20. Z-Fighting

ほぼ同じ奥行きのSurfaceが量子化精度内で競合し、ちらつく現象です。Geometryを離す、Near/Farを見直す、Depth Biasを用途限定で使います。

## 21. Early-Z

Pixel Shader前にDepth Testで隠れたFragmentを捨てられれば、重いShader実行を削減できます。

## 22. Early-Zを妨げる要因

Pixel Shaderの`discard`、Depth出力、UAV Side Effect、複雑な依存はEarly処理を制限し得ます。実際の挙動はGPUとShaderによるためProfilerで測ります。

## 23. Depth Pre-pass

先に簡素なShaderでDepthだけ描き、後の重いPassで隠れたPixelを早期除外します。Geometryを二度処理するCostとの交換です。

## 24. Pre-passが有効な場面

Pixel Shaderが重くOverdrawが多いSceneでは有効になり得ます。軽いSceneやVertex負荷が高い場合は逆効果もあります。

## 25. Depth Read-only DSV

Depthを変更せずTestするPassではRead-only DSVを用意できます。同Resourceを対応するShader Read用途で扱う設計にも関係します。

## 26. Stencil Buffer

Pixelごとの小さな整数Maskです。輪郭、Portal、Mirror、Selection、Lighting Volume、領域制限等に利用できます。

## 27. Stencil Testの構成

```text
StencilReadMask
StencilWriteMask
FrontFace / BackFace
  StencilFunc
  StencilFailOp
  StencilDepthFailOp
  StencilPassOp
```

表裏で異なる処理を設定できます。

## 28. Stencil Reference

```cpp
commandList->OMSetStencilRef(1);
```

Reference値はDynamic Stateです。PSOのComparison/Operationと組み合わせて使います。

## 29. 輪郭表示の例

1回目にCharacterを通常描画しStencilへ値を書き、2回目に拡大した裏面等をStencil不一致部分だけ描く方式があります。形状やCamera条件によるArtifactを検証します。

## 30. Stencil Mask設計

用途ごとにBitを割り当てる場合、書込みMask、Clear時期、Pass間Ownershipを文書化します。無計画な共有は衝突を生みます。

## 31. Blendの役割

Pixel Shader出力SourceとRender Target既存値Destinationを演算し、最終Colorを作ります。

## 32. Blend式

```text
Result = Source * SourceFactor
       OP Destination * DestinationFactor
```

ColorとAlpha Channelには別のFactor/Operationを設定できます。

## 33. Blend無効

```cpp
target.BlendEnable = FALSE;
target.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
```

不透明物は通常Blendを無効にしてDepth Writeを有効にします。

## 34. Straight Alpha Blend

```text
SrcColor * SrcAlpha + DstColor * (1 - SrcAlpha)
```

```cpp
target.BlendEnable = TRUE;
target.SrcBlend = D3D12_BLEND_SRC_ALPHA;
target.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
target.BlendOp = D3D12_BLEND_OP_ADD;
```

## 35. Alpha Channel側の式

Color式をそのままAlphaへ複製するとは限りません。後続Compositionが期待するAlphaの意味を先に決めます。

## 36. Premultiplied Alpha

Texture Colorへ事前にAlphaを掛けた表現です。

```text
SrcColor + DstColor * (1 - SrcAlpha)
```

Asset生成、Sampling、Shader出力、Blend Stateを同じ規約へ揃えます。

## 37. StraightとPremultipliedの混同

混同すると暗い縁、明るい縁、色漏れが発生します。Texture RGBが既にAlpha乗算済みか確認します。

## 38. Additive Blend

```text
SrcColor * factor + DstColor
```

発光、火花、Energy Effect等に向きます。加算し過ぎによる白飛びとHDR Rangeを管理します。

## 39. Multiplicative Blend

Destinationを暗くする影や特殊Effect等で使えますが、Lighting Pipelineとの整合を確認します。

## 40. Blend Operation

ADD、SUBTRACT、REV_SUBTRACT、MIN、MAXがあります。Factorと組み合わせた意味を式で確認します。

## 41. Independent Blend

MRTごとに異なるBlendを設定する場合、`IndependentBlendEnable`を有効にし各RenderTarget要素を定義します。

## 42. Render Target Write Mask

RGBAのどのChannelへ書くか制御します。Depth-only PassではColor Target自体をBindしない設計も検討します。

## 43. Sample Mask

PSOの`SampleMask`はSample単位の書込みを制限します。通常は`UINT_MAX`です。

## 44. Alpha-to-Coverage

MSAA Sample CoverageへAlphaを反映し、草や葉等のCutout Edgeを滑らかにできます。透明Blendそのものの完全な代替ではありません。

## 45. Logic Operation

Blendとは別のBitwise Logic Opがあります。Formatや機能制約を確認し、一般的な透明合成と混同しません。

## 46. 透明描画の基本

```text
Depth Test  : ON
Depth Write : OFF
Blend       : ON
Order       : back-to-front
```

相互交差する透明面は単純なObject Sortだけでは完全に解決できません。

## 47. 不透明描画順

Front-to-backはEarly-Z効率を改善し得ます。一方、PSO/Material切替を減らすSortも重要なため、Depth BucketとState Keyを組み合わせます。

## 48. 透明描画順

Cameraから遠い順を基本にします。Object中心だけのSortでは大きいMeshやParticleで誤りが出るため、表現別に妥協点を選びます。

## 49. Order-Independent Transparency

Weighted Blended、Per-pixel Linked List等の手法があります。品質、Memory、Atomic、Bandwidth、MSAAとのCostを比較します。

## 50. CutoutとTransparent

Cutoutは閾値でPixelを残す/捨てるため、Depth Write可能です。Softな透明とは描画順とAA戦略が異なります。

## 51. ParticleのBlend

SmokeはAlpha系、SparkはAdditive系等、Effectの物理的・演出的目的でPSOを分けます。全Particleを同一式にしません。

## 52. UIのBlend

UI AssetのAlpha規約、Linear/SRGB、Render Target Format、HDR Tone Mapping前後のどこへ合成するかを統一します。

## 53. Rasterizerの役割

Clip Space PrimitiveをScreen上のFragment/Sampleへ変換し、Cull、Fill、Depth Bias、Conservative Rasterization等を制御します。

## 54. Rasterizer State例

```cpp
D3D12_RASTERIZER_DESC raster{};
raster.FillMode = D3D12_FILL_MODE_SOLID;
raster.CullMode = D3D12_CULL_MODE_BACK;
raster.FrontCounterClockwise = FALSE;
raster.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
raster.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
raster.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
raster.DepthClipEnable = TRUE;
raster.MultisampleEnable = FALSE;
raster.AntialiasedLineEnable = FALSE;
raster.ForcedSampleCount = 0;
raster.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
```

未初期化Fieldを残さず全項目を明示します。

## 55. Fill Mode

SOLIDは面を塗り、WIREFRAMEは辺を表示します。WireframeはDebugに便利ですが、実際のTopologyやOverdrawを完全には表しません。

## 56. Face Culling

BACK、FRONT、NONEから選びます。閉じた不透明MeshではBack-face Cullが不要なFragmentを減らします。

## 57. Front Face規約

`FrontCounterClockwise`はRender Target上でどちらの頂点順をFrontと扱うか決めます。Model変換の負Scaleでも向きが反転し得ます。

## 58. 座標系とWinding

左手/右手座標系だけで自動的に決まると思わず、Asset Import、Index順、Projection、Viewport変換を含めて表示結果を検証します。

## 59. Cull Noneの用途

葉、布、Effect Card等の両面表示に使えますが、Pixel Costが増えNormal/Lightingの裏面処理も必要です。

## 60. Depth Clip

`DepthClipEnable`はNear/Far範囲外のDepth処理に関係します。通常は有効にし、特殊なShadow Volume等のみ要件を精査します。

## 61. Viewport

```cpp
D3D12_VIEWPORT viewport{
    0.0f, 0.0f,
    static_cast<float>(width),
    static_cast<float>(height),
    0.0f, 1.0f
};
commandList->RSSetViewports(1, &viewport);
```

ViewportはDynamic StateでPSO外です。

## 62. Scissor Rectangle

```cpp
D3D12_RECT rect{0, 0,
    static_cast<LONG>(width),
    static_cast<LONG>(height)};
commandList->RSSetScissorRects(1, &rect);
```

Scissor外のPixelを除外します。UI Clipや分割画面にも使います。

## 63. Scissorの典型的失敗

未設定、Size 0、Resize後の更新漏れで何も描かれないことがあります。ViewportだけでなくScissorも確認します。

## 64. Depth Bias

Rasterized DepthへOffsetを加え、Shadow MapのSelf-shadowing等を軽減します。Constant、Slope-scaled、Clampを組み合わせます。

## 65. Shadow AcneとPeter Panning

Bias不足は縞状Artifact、過剰Biasは影が物体から浮く現象を起こします。Scene ScaleとShadow Resolutionに応じて調整します。

## 66. Conservative Rasterization

Triangleが少しでも覆うPixelを保守的に生成する機能です。VoxelizationやVisibility処理等で利用し、Feature Supportを確認します。

## 67. MSAAの概念

Pixel内に複数Sampleを持ち、Geometry EdgeのCoverageを改善します。Texture内部のShader Aliasすべてを解決するものではありません。

## 68. Sample CountとQuality

Resource、PSO、RTV/DSVのSample Count/Qualityを一致させます。Hardware Supportは`CheckFeatureSupport`等で確認します。

## 69. MSAA Render Target

Swap Chain Back Bufferへ直接MSAA描画するのでなく、Multisampled Targetへ描き、通常Sample TargetへResolveしてPresentする構成が一般的です。

## 70. ResolveSubresource

Resolve元/先を適切なStateへ遷移させ、Format互換性を守ります。Depth ResolveはColorと同じ単純な扱いとは限りません。

## 71. MSAAとDeferred Rendering

G-buffer数に応じMemory/Bandwidthが増えます。TAA等のPost-process AAとの品質・Cost比較が必要です。

## 72. PSO Variant

```text
Opaque
OpaqueTwoSided
Masked
Transparent
Additive
DepthOnly
ShadowCaster
Outline
UI
```

必要な組合せだけ生成し、無制限なPermutationを避けます。

## 73. PSO Key

Shader ID、Root Signature、Vertex Layout、Rasterizer、Blend、Depth、RTV/DSV Format、Sample Count、Topology等をKeyへ含めます。

## 74. Hashと等価比較

Paddingを含む生の構造体Byte列だけへ安易に依存せず、意味のあるFieldを正規化してHash/比較します。

## 75. PSO Cache

同じKeyのPSOを再利用し、作成CostとMemoryを管理します。Pipeline Libraryや永続CacheはDriver/Adapter/Version互換性を検証します。

## 76. PSO作成失敗の診断

HRESULT、Debug Layer Message、Shader Signature、Root Signature、RTV/DSV Format、Sample Count、Input Layoutを記録します。

## 77. Default State Helper

公式Helper Header等のDefault値を使う場合も、最終値の意味を理解します。自前Factory関数なら全Fieldを確実に初期化します。

## 78. Opaque PSO例

```text
Cull Back
Depth Test LESS
Depth Write ON
Blend OFF
Color Write RGBA
```

## 79. Transparent PSO例

```text
Cull: asset次第
Depth Test ON
Depth Write OFF
Blend ON
Sort back-to-front
```

## 80. Depth-only PSO例

Pixel Shaderを省略可能な構成、Render Target 0個、Depth Write有効を検討します。Alpha CutoutはTexture Sampling/Pixel判定が必要です。

## 81. Outline PSO例

Front Cull、拡大Vertex、Stencil Test、専用Color等を組み合わせられます。Animation済み頂点と法線方向の整合を保ちます。

## 82. 描画Queue分類

```text
Depth Pre-pass
Opaque
Masked
Decal/特殊Surface
Transparent
Effect
Post Process
UI
```

分類と順序をRendererの明示的な規則にします。

## 83. 高速戦闘Sceneでの設計

多数のCharacter、Effect、破片、UIが重なるため、OpaqueのFront-to-back、ParticleのBudget、透明解像度、Effect別Blendを管理します。

## 84. Hit Effect

加算Effectを無制限に重ねるとGPU負荷と露出が急増します。発生数、画面占有率、Lifetime、LODをBudget化します。

## 85. Character表示

Skin、Hair、Cloth、Eye、Outlineで異なるCull/Blend/Depth要件を持ちます。Material分類をPSO Variantへ明示的にMappingします。

## 86. Camera密着時

Near Clip、Character Fade、Camera Collision、透明化のDepth/Sortが競合します。Camera SystemとRenderer双方の規約を設計します。

## 87. Overdraw

同じPixelへ何度もShader/Blendする状態です。大Particle、全画面透明、CullなしMeshで増加しやすくなります。

## 88. Overdraw対策

- Effect Meshを見た目に近い形へする。
- 不要な透明領域をTexture/Geometryから減らす。
- 小さなEffectをLOD/Cullする。
- 低解像度透明Passを検討する。
- PIX等で実測する。

## 89. State Sortと見た目の正しさ

State切替削減のために透明順序を壊してはいけません。OpaqueとTransparentでSort Keyの優先順位を変えます。

## 90. Sort Key例

```text
Opaque      : pass -> PSO -> material -> depth bucket
Transparent : pass -> depth descending -> PSO/material
```

完全な一意解ではなく、Scene特性に合わせて計測します。

## 91. Dynamic State一覧

Viewport、Scissor、Stencil Reference、Blend Factor、Primitive Topology等、Command Listへ設定する値とPSO内Stateを区別します。

## 92. OMSetBlendFactor

Blend Factor定数を使うBlend設定の場合に値を渡します。通常のSRC_ALPHA式では不要なこともあります。

## 93. Primitive Topologyとの整合

PSOのTopology Typeと`IASetPrimitiveTopology`の具体Topologyを互換にします。Triangle/Line/Point/Patchの不一致を避けます。

## 94. Resize時の再作成

Window Size依存のDepth Resource、DSV、Viewport、Scissor、MSAA TargetをGPU使用完了後に再作成します。

## 95. Resource Lifetime

置換前Depth/MSAA Resourceを参照するCommandがFence完了するまで破棄しません。Descriptor上書きも同様です。

## 96. Debug Layerで見る項目

PSOとAttachment Format不一致、Resource State不正、無効Descriptor、Sample Count不一致、同時Read/Write等を確認します。

## 97. PIXでの確認

Draw Eventを選び、Rasterizer/Depth/Stencil/Blend State、Render Target、Depth値、Pixel History、Overdrawを調べます。

## 98. Pixel History

特定PixelへどのDrawが到達し、Cull/Depth/Stencil/Shader/Blendのどこで失敗したかを追うのに役立ちます。

## 99. 可視化Debug View

Depth、Stencil、Alpha、Overdraw、Object ID、Material分類を画面表示できると原因究明が速くなります。

## 100. Depth可視化

非線形Depth値をそのままGray表示すると読みにくいため、Projection規約に従いView Space Depthへ復元して表示します。

## 101. Blend検証用背景

黒、白、Checker、HDR Bright Backgroundで透明Assetを確認し、暗い縁やPremultiply不一致を見つけます。

## 102. Golden Image Test

固定Camera/固定入力で画像を保存し、許容誤差付き比較を行います。GPU/Driver差による微小差を考慮します。

## 103. Unit Test対象

PSO Key、State Factory、Sort Key、Depth規約、Format組合せ、Blend式、Resize Size計算をCPU Testします。

## 104. Integration Test対象

交差する不透明Mesh、透明板、Cutout、両面Mesh、Stencil輪郭、MSAA Edge、Resizeを描き期待結果と比較します。

## 105. Stress Test対象

大量Particle、重なるCharacter、PSO Variant切替、頻繁なResize、Fullscreen、HDR、Device Lostを組み合わせます。

## 106. よくある失敗：何も映らない

Scissorが0、Cull向き逆、Depth比較逆、Viewport不正、PSO Format不一致、DSV未Bindを順に確認します。

## 107. よくある失敗：常に手前へ出る

Depth無効、Depth Write無効、Depth Clear忘れ、Projection/比較規約不一致を確認します。

## 108. よくある失敗：透明順が変

Depth Writeが有効、Front-to-back Sort、Object中心Sortの限界、Premultiply規約不一致を確認します。

## 109. よくある失敗：輪郭が欠ける

Stencil Clear/Mask、Cull、Depth Func、負Scale、拡大量、Animation Normalを確認します。

## 110. よくある失敗：Resize後だけ破綻

Depth Size、Viewport、Scissor、Fence待機、古いDescriptor、PSO Sample/Formatを確認します。

## 111. よくある失敗：GPUが急に重い

Cull None、透明Overdraw、Depth Pre-passの二重Cost、Early-Z阻害、大面積Particle、MSAA Sample数を計測します。

## 112. 実装Checklist

- [ ] Depth規約、Clear値、Comparisonを統一する。
- [ ] Depth Resource/DSV/Stateを正しく作る。
- [ ] Opaque/Masked/Transparent等のPSOを分離する。
- [ ] Straight/Premultiplied Alpha規約を固定する。
- [ ] ViewportとScissorを毎Frame正しく設定する。
- [ ] Winding/CullをAsset Importまで統一する。
- [ ] PSO Keyへ全互換条件を含める。
- [ ] ResizeとLifetimeをFenceで守る。
- [ ] PIXでDepth/Blend/Overdrawを確認する。

## 113. 理解確認問題

1. Depth TestとDepth Writeの違いを説明してください。
2. Reversed-ZのClear値と比較関数を説明してください。
3. Straight AlphaとPremultiplied Alphaの式を説明してください。
4. 透明物を通常遠い順に描く理由を説明してください。
5. Front Face、Cull、負Scaleの関係を説明してください。
6. Depth Biasの過不足で起きる現象を説明してください。
7. Early-ZとDepth Pre-passの利点・Costを説明してください。
8. PSO Keyに含めるStateを挙げてください。
9. MSAA TargetをPresentするまでの流れを説明してください。
10. 高速戦闘SceneのOverdraw対策を提案してください。

## 114. 要点

- Depthは可視性を決め、TestとWriteは別々に制御します。
- BlendはSource/Destinationの式でありAlpha資産規約まで含めて設計します。
- RasterizerはCull、Fill、Bias、Clip、Coverageを制御します。
- D3D12では固定機能StateをPSO Variantとして事前構築します。
- 正しい描画順とState SortはPass種別ごとに優先順位が異なります。
- 透明Overdrawと重いPixel Shaderは高速戦闘Sceneの重要な性能課題です。
- Debug Layer、PIX、可視化、画像Testで見た目と性能を検証します。

## 115. 公式資料

- [D3D12_DEPTH_STENCIL_DESC](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/ns-d3d12-d3d12_depth_stencil_desc)
- [D3D12_BLEND_DESC](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/ns-d3d12-d3d12_blend_desc)
- [D3D12_RASTERIZER_DESC](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/ns-d3d12-d3d12_rasterizer_desc)
- [Depth testing](https://learn.microsoft.com/en-us/windows/win32/direct3d12/depth-testing)
- [Blending](https://learn.microsoft.com/en-us/windows/win32/direct3d12/blending)
- [Rasterizer stage](https://learn.microsoft.com/en-us/windows/win32/direct3d12/rasterizer-stage)
- [Multisampling](https://learn.microsoft.com/en-us/windows/win32/direct3d12/multisampling)
- [ID3D12GraphicsCommandList::ClearDepthStencilView](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12graphicscommandlist-cleardepthstencilview)

## 116. 次章への接続

次章ではModel・Material・Animationを扱います。本章のPSO分類へMesh、Material Parameter、Skinning結果を接続し、Characterを構成する複数Surfaceを正しいStateと順序で描画します。
