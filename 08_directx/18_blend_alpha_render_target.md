# DirectX 11：Blend・Alpha・Render Target

この章では、Pixel Shaderが出力したSource値とRender Targetにすでに入っているDestination値を合成するBlend Stageを学びます。Straight Alpha、Premultiplied Alpha、Additive、Blend Factor、Sample Mask、Alpha-to-Coverage、Color Write Mask、MRT、透明物の順序までを扱います。

## 1. Blendの位置

```text
Pixel Shader output
-> depth/stencil tests
-> blending with render-target destination
-> color write mask
-> render target storage
```

BlendはOutput Merger Stageの機能です。

## 2. SourceとDestination

- Source：現在のPixel Shader出力。
- Destination：Render Targetにすでに保存されている値。
- Result：Blend式で合成され、書き戻される値。

## 3. Color Blend式

```text
result.rgb =
    source.rgb * sourceBlendFactor
    blendOperation
    destination.rgb * destinationBlendFactor
```

FactorとOperationを別々に読みます。

## 4. Alpha Blend式

```text
result.a =
    source.a * sourceAlphaBlendFactor
    alphaBlendOperation
    destination.a * destinationAlphaBlendFactor
```

RGBとAlphaは別設定を持ちます。

## 5. Blend無効

Blend無効では、Write Maskで許可されたSource出力がDestinationを置換します。Opaque Materialの基本です。

## 6. Blend State Descriptor

```cpp
D3D11_BLEND_DESC desc{};
desc.AlphaToCoverageEnable = FALSE;
desc.IndependentBlendEnable = FALSE;

auto& target = desc.RenderTarget[0];
target.BlendEnable = FALSE;
target.SrcBlend = D3D11_BLEND_ONE;
target.DestBlend = D3D11_BLEND_ZERO;
target.BlendOp = D3D11_BLEND_OP_ADD;
target.SrcBlendAlpha = D3D11_BLEND_ONE;
target.DestBlendAlpha = D3D11_BLEND_ZERO;
target.BlendOpAlpha = D3D11_BLEND_OP_ADD;
target.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
```

## 7. CreateBlendState

```cpp
Microsoft::WRL::ComPtr<ID3D11BlendState> blendState;
ThrowIfFailed(device->CreateBlendState(
    &desc,
    blendState.GetAddressOf()));
```

作成後にDescriptorは変更できません。

## 8. OMSetBlendState

```cpp
const float blendFactor[4] = {1, 1, 1, 1};
context->OMSetBlendState(
    blendState.Get(),
    blendFactor,
    0xFFFFFFFFu);
```

State、動的Blend Factor、Sample Maskを設定します。

## 9. Blend Factor引数

Descriptorで`BLEND_FACTOR`または`INV_BLEND_FACTOR`を使うと、`OMSetBlendState`の4成分値が式へ入ります。

## 10. Sample Mask引数

32-bit MaskでMultisample Sampleへの書込みを制限します。通常全Sample有効は`0xFFFFFFFF`です。

## 11. nullptr Blend State

`OMSetBlendState(nullptr, nullptr, 0xFFFFFFFF)`は既定Stateへ戻します。Engineでは用途別Stateを明示すると残留Bugを減らせます。

## 12. D3D11_BLEND_ZERO

Factor 0です。項を完全に消します。

## 13. D3D11_BLEND_ONE

Factor 1です。値をそのまま使用します。

## 14. SRC_COLOR

Source RGBをFactorとして使います。Alpha Channel側で使用可能なFactorには制約があるためAPI定義を確認します。

## 15. INV_SRC_COLOR

`1 - source color`です。特殊なColor合成に使われます。

## 16. SRC_ALPHA

Source Alphaを全Color成分へFactorとして使います。Straight Alpha合成の中心です。

## 17. INV_SRC_ALPHA

`1 - source alpha`です。Sourceが不透明に近いほどDestinationを弱めます。

## 18. DEST_ALPHAとDEST_COLOR

Destination側の保存値をFactorに使います。Render Target FormatにAlphaがあるか、そこへ何を保存してきたかが重要です。

## 19. SRC_ALPHA_SAT

Source AlphaとDestination Alphaから飽和Factorを作ります。用途が限定されるため式を確認して使います。

## 20. Blend Operation

- `ADD`：二項を加算。
- `SUBTRACT`：Source項からDestination項を引く。
- `REV_SUBTRACT`：Destination項からSource項を引く。
- `MIN`：小さい値を選ぶ。
- `MAX`：大きい値を選ぶ。

## 21. MIN/MAXの注意

MIN/MAX OperationではBlend Factorの扱いがADD系と異なります。FactorでScaleできると決めつけず、API定義どおりに使います。

## 22. Opaque State

```text
BlendEnable = false
WriteMask = RGBA
```

不透明物を先に描き、Depth Bufferを確立します。

## 23. Straight Alphaとは

Texture/ShaderのRGBがAlphaで事前乗算されていない表現です。

```text
stored = (original rgb, alpha)
```

## 24. Straight Alpha Color式

```text
result.rgb = source.rgb * source.a
           + destination.rgb * (1 - source.a)
```

## 25. Straight Alpha Descriptor

```cpp
target.BlendEnable = TRUE;
target.SrcBlend = D3D11_BLEND_SRC_ALPHA;
target.DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
target.BlendOp = D3D11_BLEND_OP_ADD;
target.SrcBlendAlpha = D3D11_BLEND_ONE;
target.DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
target.BlendOpAlpha = D3D11_BLEND_OP_ADD;
```

Alpha Channelへ何を蓄積したいかでAlpha式は変わります。

## 26. Premultiplied Alphaとは

RGBがすでにAlphaで乗算された表現です。

```text
stored.rgb = original.rgb * alpha
stored.a   = alpha
```

## 27. Premultiplied Color式

```text
result.rgb = source.rgb
           + destination.rgb * (1 - source.a)
```

## 28. Premultiplied Descriptor

```cpp
target.SrcBlend = D3D11_BLEND_ONE;
target.DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
target.BlendOp = D3D11_BLEND_OP_ADD;
```

Source RGBが事前乗算済みであることが前提です。

## 29. Straight/Premultiplied不一致

Straight画像をPremultiplied Stateで描くと明るい縁、Premultiplied画像をStraight Stateで描くと暗い縁などが発生します。

## 30. Premultiplyする空間

Color Space規約を守り、意図したLinear/Encoded値に対して処理します。sRGB値へ無計画に乗算しません。

## 31. Additive Blend

```text
result.rgb = source.rgb + destination.rgb
```

光、火花、発光Particleなどに使います。

## 32. Additive Descriptor

```cpp
target.SrcBlend = D3D11_BLEND_ONE;
target.DestBlend = D3D11_BLEND_ONE;
target.BlendOp = D3D11_BLEND_OP_ADD;
```

HDR Render Targetでは1を超える光量を保持できます。

## 33. Alpha-weighted Additive

Source Factorを`SRC_ALPHA`、Destinationを`ONE`にし、Alphaで発光強度を制御する方式もあります。

## 34. Multiply Blend

Destination Colorを暗くする合成などに使います。Factorの組合せで式を導出し、Color SpaceとAlpha意味を確認します。

## 35. Subtractive表現

SUBTRACT/REV_SUBTRACTは特殊Effectで使えますが、UNORM Render Targetでは負値がClampされます。HDR Float Targetとの結果差を確認します。

## 36. ColorとAlphaを別設計する

画面表示だけならRGBが主目的でも、後続Post ProcessやCompositionがAlphaを読む場合があります。Alpha ChannelにCoverage、Opacity、Maskなど何を保存するか決めます。

## 37. Alphaは透明度と自動的に決まらない

Shaderが何を出し、Blend Stateが何を蓄積するかで意味が決まります。Render Target Alphaを用途なしに放置しません。

## 38. Clear Alpha

Render TargetをClearするとき、最終Alphaの意味に合う値を使います。不透明Scene Colorなら1、透明Layerなら0など設計で変わります。

## 39. RenderTargetWriteMask

```cpp
target.RenderTargetWriteMask =
    D3D11_COLOR_WRITE_ENABLE_RED |
    D3D11_COLOR_WRITE_ENABLE_GREEN |
    D3D11_COLOR_WRITE_ENABLE_BLUE;
```

Channelごとの書込みを許可・禁止します。

## 40. Alphaだけ書かない

RGBだけ更新し既存Alphaを保持できます。後続PassがAlphaを使う場合に役立ちます。

## 41. Color Write全無効

Write Mask 0ならColorを書きません。Depth-only的なPass、Stencil Mask、Occlusion用描画等で使えます。

## 42. Write MaskとPixel Shader Cost

Color Writeを無効にしてもPixel Shader実行が必ず消えるわけではありません。Depth-onlyならPixel Shader省略などPipeline全体を設計します。

## 43. Multiple Render Targets

Output Mergerへ複数RTVをBindingし、Pixel Shaderの`SV_Target0`以降へ出力できます。

## 44. RenderTarget配列

`D3D11_BLEND_DESC::RenderTarget[0..7]`に最大8 Slot分のBlend設定があります。

## 45. IndependentBlendEnable FALSE

FALSEではRenderTarget[0]のBlend設定が全Render Targetへ適用される基本動作です。

## 46. IndependentBlendEnable TRUE

TRUEでは各MRT Slotへ異なるBlend/Write Mask設定を使えます。

## 47. G-bufferの例

Deferred G-bufferは通常Blend無効で、Base Color、Normal、Material値等を各Targetへ書きます。Targetごとに不要ChannelのWrite Maskを制御できます。

## 48. Light Accumulation

Lighting結果をAdditiveでHDR Targetへ加算するPassがあります。Depth/Stencilで影響領域を限定します。

## 49. FormatのBlend Support

すべてのDXGI FormatがBlend可能とは限りません。`CheckFormatSupport`でRender TargetおよびBlend用途の対応を確認します。

## 50. Integer Render Target

整数Formatなどでは通常のBlendが使えない場合があります。Object ID Target等はBlend無効で書きます。

## 51. sRGB Render TargetとBlend

sRGB Viewを使う場合、Linear LightingとStorage Encodeの規約を保ちます。Feature LevelやFormatによる挙動も確認し、Gamma空間で合成しない設計にします。

## 52. HDR Render Target

Float FormatへLinear HDR値を蓄積し、Tone Mapping後に表示用Targetへ変換します。Additive Effectが白く飽和する時点を制御できます。

## 53. BlendはOrder-dependent

一般的なAlpha Blendは描画順で結果が変わります。

```text
A over B over C != B over A over C
```

## 54. 透明物の基本順序

不透明物を先に描いてDepthを作り、透明物を概ね奥から手前へSortして描きます。

## 55. 透明物のDepth Test

通常はDepth Testを有効にし、不透明物の後ろにある透明Fragmentを捨てます。

## 56. 透明物のDepth Write

一般的な半透明ではDepth Writeを無効にします。手前の透明面が後続透明面を完全に隠してしまうのを避けます。

## 57. Sort Key

Cameraからの距離、View Space Depth、Bounding Center/Far Pointなどを使います。Object単位Sortでは交差Geometryを完全には解決できません。

## 58. Particle Sorting

大量Particleを個別SortするCostと見た目を比較します。Additive Particleは順序依存が弱く、Sort省略できる場合があります。

## 59. 交差する透明面

Object単位の奥行きSortではTriangleが交差する場合に正しい順序を作れません。Mesh分割、特殊Shader、Order-independent Transparency等を検討します。

## 60. Weighted Blended OIT

複数Targetへ色とRevealage等を蓄積し、近似的に順序依存を減らす方式です。通常Alpha Blendと品質・Costを比較します。

## 61. Dual-source Blending

Pixel Shaderから二つのSourceを出し、`SRC1_COLOR`等をFactorに使う高度な方式です。Feature Support、出力Semantic、MRT制約を確認します。

## 62. Alpha Testとの違い

Alpha Test相当の`clip/discard`はFragmentを完全に残すか捨てる処理、Alpha BlendはSourceとDestinationを混ぜる処理です。

## 63. Cutout Material

葉、柵、髪Card等はAlpha CutoffでOpaque/Cutoutとして描くと、Depth WriteとSortingを利用できます。境界AliasingにはMSAA/Alpha-to-Coverage等を検討します。

## 64. AlphaToCoverageEnable

Source AlphaからMSAA Sample Coverage Maskを生成します。Blendによる半透明とは異なり、Sample単位でCoverageを近似します。

## 65. Alpha-to-Coverageの条件

MSAA Render Targetが必要です。非MSAA TargetでFlagだけ有効にしても期待するEdge表現にはなりません。

## 66. Alpha-to-Coverageの用途

葉、草、細い柵などCutout境界を滑らかにする用途があります。透明Layerを正確に合成する一般解ではありません。

## 67. Alpha-to-CoverageとBlend

Coverage生成とColor Blendは別概念です。同時利用時の結果をTarget Sample CountとStateで検証します。

## 68. Sample Mask

`OMSetBlendState`のSample MaskとAlpha-to-Coverage結果はSample Coverageに関係します。Debug時に全Bit有効を基準にします。

## 69. Blend Factor Color

```cpp
const float factor[4] = {tintR, tintG, tintB, tintA};
context->OMSetBlendState(state.Get(), factor, 0xFFFFFFFFu);
```

Descriptorが`BLEND_FACTOR`を使う場合だけ式へ影響します。

## 70. State Cache

Blend Descriptor全FieldをKeyにして同じStateを再利用します。Padding ByteをHashしないよう、ゼロ初期化とField比較を徹底します。

## 71. 代表State集合

```text
Opaque
StraightAlpha
PremultipliedAlpha
Additive
AlphaWeightedAdditive
ColorWriteDisabled
AlphaToCoverage
```

Materialは名前付きModeを選ぶと管理しやすくなります。

## 72. PassごとのState明示

Opaque、Transparent、Particle、UI、Post Process、G-bufferの開始時にBlend State、Factor、Sample Maskを明示します。

## 73. Blend State残留

Additive Particle後にStateを戻さず、UIや次FrameのOpaqueが加算されるBugがあります。Pass境界で論理PipelineをBindingします。

## 74. Draw SortingとState変更

透明物はDepth順が優先されるため、Blend State/Materialだけで自由にSortできません。描画正しさとState変更Costを分けて評価します。

## 75. UI Blend

UI AssetがStraightかPremultipliedか、Font Rasterizer出力が何を意味するかを統一します。Panel、Text、Iconで異なる規約を混ぜません。

## 76. Font Rendering

Glyph TextureのChannelをCoverageとして使い、Text Colorと組み合わせます。Source RGB/Alphaの作り方とBlend Stateを一組で設計します。

## 77. Particle Blend

煙はAlpha、火花はAdditiveなどEffectの物理・芸術的意図でModeを分けます。一つのParticle System内でBatch Keyに含めます。

## 78. Hit Effect

斬撃、火花、残像、Damage FlashではAdditive、Premultiplied、Screen的近似などを使い分けます。HDR Scene Colorで強度を保持しBloomへ渡します。

## 79. Render Target Alphaの利用

Scene Capture、UI Layer、Video/Composition、Post Process MaskではAlphaを後から使う場合があります。Back Bufferで見えないから不要と決めつけません。

## 80. Debug Name

```cpp
SetDebugName(*blendState.Get(), "Blend Premultiplied RGBA");
```

Alpha規約、Operation、Write Maskを識別できる名前にします。

## 81. Graphics Debuggerで確認する

- Source Pixel Shader出力。
- Blend Descriptor。
- Blend FactorとSample Mask。
- Blend前Destination。
- Blend後Render Target値。
- Color Write Mask。

## 82. よくある失敗：Alphaだけ出せば透明になる

Blend Stateが無効ならSourceがそのまま書かれます。Shader出力とOutput Merger Stateの両方が必要です。

## 83. よくある失敗：Straight/Premultiplied混在

Assetごとに規約が違い、縁の色や明るさが崩れます。Import MetadataとMaterial Blend Modeを一致させます。

## 84. よくある失敗：透明物を手前から描く

後から描いた遠方色が前面へ混ざり、不正な結果になります。基本は奥から手前です。

## 85. よくある失敗：透明物がDepthを書き込む

先に描いた透明面が後続面を遮断します。Depth TestとDepth Writeを別々に設定します。

## 86. よくある失敗：Alpha Channel式を放置

RGBは正しく見えてもRender Target Alphaが壊れ、後続Compositionで問題になります。Alphaの意味と式を定義します。

## 87. よくある失敗：Write Mask残留

Depth-only PassのWrite Mask 0が後のColor Passへ残ります。Blend StateをPass開始時に明示します。

## 88. 数式Test

- Source/Destinationへ既知RGBA値を入れる。
- CPUでBlend期待値を計算する。
- GPU Capture結果と比較する。
- Alpha 0、0.5、1を試す。
- UNORM ClampとFloat Targetの差を確認する。

## 89. Alpha規約Test

- Straight Asset＋Straight Stateを確認する。
- Premultiplied Asset＋Premultiplied Stateを確認する。
- 不一致時のArtifactを記録する。
- Transparent Pixelの縁を拡大確認する。
- sRGB/Linear Pipelineを確認する。

## 90. 透明描画Test

- 不透明物の前後へ透明Quadを置く。
- 二枚の透明Quadの順序を反転する。
- Depth Test ON/Write OFFを確認する。
- 交差Meshの限界を確認する。
- Particle数とSort Costを計測する。

## 91. MRT/MSAA Test

- Independent Blend OFF/ONを比較する。
- SlotごとのWrite Maskを確認する。
- Blend非対応Formatを事前検出する。
- Alpha-to-CoverageをSample Count別に確認する。
- Sample MaskのBitを変えてCoverageを見る。

## 92. 完成確認表

- [ ] Source、Destination、Resultを説明できる。
- [ ] Color/Alpha Blend式を別々に読める。
- [ ] StraightとPremultiplied Alphaを区別できる。
- [ ] Opaque、Alpha、Additive Stateを作成できる。
- [ ] Blend FactorとSample Maskの役割を説明できる。
- [ ] Color Write MaskをChannelごとに設定できる。
- [ ] Independent BlendでMRTを制御できる。
- [ ] 透明物のDepth Test/WriteとSortを設計できる。
- [ ] Alpha-to-CoverageとAlpha Blendを区別できる。
- [ ] Render Target Alphaの意味をPass全体で定義できる。

## 93. この章の要点

- BlendはPixel Shader SourceとRender Target DestinationをOutput Mergerで合成します。
- RGBとAlphaは別Factor・別Operationを持ちます。
- Straight AlphaはSource RGBへAlphaを掛け、Premultipliedはすでに掛かったRGBを使います。
- Additive Blendは光や発光EffectをHDR Targetへ蓄積するのに適します。
- Write MaskはChannel書込みを制御し、Independent BlendはMRTごとにStateを変えます。
- 通常Alpha Blendは順序依存で、透明物は基本的に奥から手前へ描きます。
- 透明物はDepth Testを使いながらDepth Writeを切る構成が基本です。
- Alpha-to-CoverageはMSAA Sample CoverageでCutout境界を近似する機能です。

## 94. 公式資料

- [Blending overview](https://learn.microsoft.com/en-us/windows/win32/direct3d11/d3d10-graphics-programming-guide-blend-state)
- [D3D11_BLEND_DESC](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/ns-d3d11-d3d11_blend_desc)
- [D3D11_RENDER_TARGET_BLEND_DESC](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/ns-d3d11-d3d11_render_target_blend_desc)
- [D3D11_BLEND](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/ne-d3d11-d3d11_blend)
- [D3D11_BLEND_OP](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/ne-d3d11-d3d11_blend_op)
- [ID3D11Device::CreateBlendState](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11device-createblendstate)
- [ID3D11DeviceContext::OMSetBlendState](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11devicecontext-omsetblendstate)
- [Configuring blending functionality](https://learn.microsoft.com/en-us/windows/win32/direct3d11/d3d10-graphics-programming-guide-blend-state)

次章では、Depth Test、Depth Write、Stencil Mask、Front/Back Face別Stencil Operationを定義するDepth Stencil Stateを扱います。
