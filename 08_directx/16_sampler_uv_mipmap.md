# DirectX 11：Sampler・UV・Mip Map

この章では、Textureのどこを読むかを表すUV、Texel間をどう補間するかを決めるSampler State、距離に応じた縮小画像であるMip Mapを学びます。Filter、Address Mode、Anisotropic Filtering、LOD、Derivative、自動Mip生成、Atlas、Streaming、品質と性能の調整までを扱います。

## 1. 三つの役割を分ける

```text
UV            : Texture上の参照位置
Texture/SRV   : Pixel Dataと見えるSubresource範囲
Sampler State : Filter、Address、LOD選択規則
```

同じTextureを異なるSamplerで読むことができます。

## 2. UV座標

一般的な2D Textureでは、左から右をU、上から下または下から上をVとして扱います。画像Tool、Model Format、RendererでV方向規約が異なる場合があります。

## 3. Normalized座標

通常の`Texture2D.Sample`ではTexture寸法に関係なく0から1付近の正規化座標を使います。

```text
u = 0 -> left edge
u = 1 -> right edge
v = 0/1の上下方向はAsset規約による
```

## 4. UVとTexel座標の違い

UVは正規化された連続値、Texel座標は整数Pixel位置です。`Sample`はUV、`Load`は整数座標を使うのが基本です。

## 5. 頂点UVの補間

Vertex Shaderから出力したUVはRasterizerにより三角形内部へPerspective-correctに補間され、Pixel Shader入力となります。

## 6. UV Transform

```hlsl
float2 transformedUv = input.uv * uvScale + uvOffset;
```

Tiling、Scroll、Atlas領域選択などに使います。

## 7. UV回転

中心を原点へ移動し、2×2回転Matrixを適用して中心を戻します。回転中心とAspect Ratioを意識します。

## 8. Texture Tiling

UVを1より大きくし、Wrap Address Modeで模様を繰り返します。床、壁、地形などに使います。

## 9. Sampler Stateとは

Texture Memoryを持つObjectではありません。Sample位置周辺のTexel選択、Texture範囲外の扱い、Mip Level選択などの規則を持つImmutable State Objectです。

## 10. Sampler Descriptor

```cpp
D3D11_SAMPLER_DESC desc{};
desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
desc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
desc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
desc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
desc.MipLODBias = 0.0f;
desc.MaxAnisotropy = 1;
desc.ComparisonFunc = D3D11_COMPARISON_NEVER;
desc.BorderColor[0] = 0.0f;
desc.BorderColor[1] = 0.0f;
desc.BorderColor[2] = 0.0f;
desc.BorderColor[3] = 0.0f;
desc.MinLOD = 0.0f;
desc.MaxLOD = D3D11_FLOAT32_MAX;
```

## 11. CreateSamplerState

```cpp
Microsoft::WRL::ComPtr<ID3D11SamplerState> sampler;
ThrowIfFailed(device->CreateSamplerState(
    &desc,
    sampler.GetAddressOf()));
```

SamplerはDeviceで一度作り、Draw間で再利用します。

## 12. Filterの三要素

```text
MIN : Textureが画面上で縮小されるとき
MAG : Textureが画面上で拡大されるとき
MIP : 二つのMip Level間をどう選ぶか
```

`MIN_MAG_MIP_LINEAR`は三つすべてLinearです。

## 13. Point Filtering

最も近いTexel一つを選びます。Pixel Art、整数Scale、意図的な粗い見た目に適します。

## 14. Linear Filtering

周辺Texelを補間します。2D Textureの一つのMip Level内では一般にBiliner Filteringとなります。

## 15. Trilinear Filtering

Mip Level内をLinear補間し、さらに隣接する二つのMip Level間もLinear補間します。Mip境界の切替を滑らかにします。

## 16. Filter列挙名を読む

`D3D11_FILTER_MIN_LINEAR_MAG_MIP_POINT`なら、縮小はLinear、拡大とMip選択はPointです。名前を左から分解します。

## 17. Anisotropic Filtering

斜めに見えるSurfaceではFootprintが細長くなります。異方性Filterは複数Sampleを使い、遠方の床や道路のぼやけを抑えます。

## 18. 異方性Sampler

```cpp
desc.Filter = D3D11_FILTER_ANISOTROPIC;
desc.MaxAnisotropy = 8;
```

`MaxAnisotropy`は1から16の範囲で設定します。

## 19. 異方性Level

大きい値ほど常に見た目が大幅改善するとは限らず、Sample Costも増えます。2、4、8、16を実機で比較します。

## 20. Comparison Filter

Shadow MapなどではSample値と比較値を比較するSamplerを使います。通常Color Texture用Samplerと分けます。

## 21. SampleCmp

```hlsl
float visibility = shadowMap.SampleCmp(
    shadowSampler,
    shadowUv,
    receiverDepth);
```

Comparison Samplerと対応するTexture型・Formatが必要です。

## 22. ComparisonFunc

ShadowのDepth規約に合わせて`LESS_EQUAL`などを選びます。通常ZとReversed-Zでは比較方向が変わります。

## 23. Address Mode

UVが0から1の範囲外へ出たとき、どの値を読むかをU/V/W軸ごとに決めます。

## 24. Wrap

```text
... -0.2 -> 0.8
...  1.2 -> 0.2
```

小数部を繰り返すように扱い、Tilingに使います。

## 25. Mirror

Textureを反転しながら繰り返します。繰り返し境界の向きを連続させたい場合があります。

## 26. Clamp

範囲外座標を端へ固定します。UI、Post Process、Render Target Samplingなどで境界外の繰り返しを防ぎます。

## 27. Border

範囲外でDescriptorのBorder Colorを返します。Shadow Mapの外側や特殊Maskで利用します。

## 28. Mirror Once

一度Mirrorした後にClampするAddress Modeです。用途が限定されるため、挙動を可視化して採用します。

## 29. U/V/Wを別設定できる

2D Textureでは主にU/Vを使います。UだけWrap、VはClampなど、Textureの意味に合わせて設定できます。

## 30. Border Color

Border Modeでのみ使われます。Color Texture、Mask、Shadowで期待する外側値は異なります。

## 31. Sampler Binding

```cpp
ID3D11SamplerState* samplers[] = {sampler.Get()};
context->PSSetSamplers(0, 1, samplers);
```

HLSLの`register(s0)`とSlot 0を一致させます。

## 32. StageごとのSampler Slot

PSへBindingしたSamplerはVSから自動で見えません。Vertex ShaderでSampleするなら`VSSetSamplers`へ設定します。

## 33. SRVとSamplerの組合せ

```hlsl
Texture2D baseColorTexture : register(t0);
SamplerState materialSampler : register(s0);

float4 color = baseColorTexture.Sample(
    materialSampler,
    input.uv);
```

`t0`と`s0`は別Namespaceです。

## 34. Sampler Cache

Descriptor全FieldをKeyにして同一Samplerを再利用できます。各Materialが同じStateを重複作成しないようにします。

## 35. Immutable State

作成後にSampler Descriptorは変更できません。設定を変える場合は別Sampler Stateを作ってBindingします。

## 36. Mip Mapとは

元画像を段階的に半分へ縮小した画像列です。Mip 0が最大解像度、Mip 1以降が小さい画像です。

## 37. Mip Chain例

```text
1024 x 512  mip 0
 512 x 256  mip 1
 256 x 128  mip 2
 ...
   2 x   1
   1 x   1  last mip
```

各軸は最低1まで縮小します。

## 38. 完全Mip数

```text
floor(log2(max(width, height))) + 1
```

1024×512なら11 Levelです。

## 39. Mip Mapの目的

- 遠方TextureのちらつきとAliasを減らす。
- Cache localityを改善する。
- 小さな画面領域へ巨大Mipを読む帯域を減らす。
- Streamingの解像度段階に使う。

## 40. Mipは単なるぼかしではない

Pixel Footprintに合う事前Filter済み画像を選ぶ仕組みです。MipなしでMinifyすると一Pixelへ多数Texelが折り重なりAliasが生じます。

## 41. LODとは

Sample時にどのMip Levelを使うかを表す連続値です。0がMip 0、1がMip 1、0.5ならMip Filter次第で二Levelを混ぜます。

## 42. 自動LOD計算

Pixel Shaderの通常`Sample`では、隣接PixelのUV変化量からTexture上のFootprintを推定し、LODを決めます。

## 43. Derivative

`ddx`と`ddy`は隣接実行Lane間の値変化を求めます。LOD推定はUV DerivativeとTexture寸法に基づきます。

## 44. Divergent Branch内のDerivative

隣接Pixelが異なるBranchを通る場所ではDerivativeが不安定または未定義になり得ます。Sample位置と分岐構造を設計します。

## 45. SampleLevel

```hlsl
float4 color = texture.SampleLevel(sampler, uv, 2.0f);
```

LODを明示します。自動Derivativeが使えないStageや特殊Effectで利用します。

## 46. SampleBias

自動計算LODへBiasを加えます。広域の品質設定に乱用せず、Texture StreamingやEffect規約と合わせます。

## 47. SampleGrad

```hlsl
float4 color = texture.SampleGrad(
    sampler, uv, uvDdx, uvDdy);
```

Derivativeを明示します。UVを複雑に変形するShaderで正しいLODを保つ用途があります。

## 48. MipLODBias

Sampler Descriptorで全SampleのLODへBiasを加えます。負値は高精細側、正値は低精細側へ寄せます。

## 49. Negative BiasのRisk

見た目が鋭くなってもAliasとちらつきが増えます。Temporal AAとの関係を含め動画で評価します。

## 50. MinLODとMaxLOD

Samplerから選べるLOD範囲をClampします。SRVが公開するMip範囲とは別の制限です。

## 51. SRVのMip範囲

`MostDetailedMip`と`MipLevels`でShaderから見えるSubresource範囲を指定します。Sampler LODはその見える範囲内で扱われます。

## 52. Texture作成時のMipLevels

Mipを一枚だけ持つなら1です。完全Chainを確保する指定や、全Subresourceを明示初期化する方式があります。

## 53. Offline Mip生成

Asset Build ToolでMip Chainを生成し、DDS等へ保存します。品質、Gamma、Normal Map処理、Alpha Coverageを制御しやすい方式です。

## 54. Runtime自動Mip生成

Direct3D 11の`GenerateMips`を使い、Mip 0から下位MipをGPU生成できます。ResourceとViewに作成条件があります。

## 55. 自動Mip用Descriptor

```cpp
desc.MipLevels = 0;
desc.Usage = D3D11_USAGE_DEFAULT;
desc.BindFlags =
    D3D11_BIND_SHADER_RESOURCE |
    D3D11_BIND_RENDER_TARGET;
desc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;
```

Formatが自動生成に対応することも確認します。

## 56. 自動Mip作成の流れ

```text
create texture with full chain
-> create SRV exposing all mips
-> upload mip 0
-> context.GenerateMips(srv)
```

Immutable Usageではこの更新経路を使いません。

## 57. UpdateSubresourceでMip 0へUpload

```cpp
context->UpdateSubresource(
    texture.Get(),
    0,
    nullptr,
    pixels.data(),
    rowPitch,
    0);
```

Subresource 0はArray Slice 0のMip 0です。

## 58. GenerateMips

```cpp
context->GenerateMips(srv.Get());
```

戻り値がないため、作成条件とDebug Layer Messageを事前・事後に確認します。

## 59. Format Support確認

`CheckFormatSupport`でShader Sample、Render Target、Mip Autogen等、必要なFlagを確認します。

## 60. sRGB Mip生成

Color TextureはLinear化してFilterし、適切にEncodeする必要があります。Offline ToolやsRGB View経路がColor Space規約どおりか確認します。

## 61. Gamma空間縮小の問題

sRGB数値を単純平均すると輝度が正しくありません。Color TextureのMipはLinear空間でFilterするのが基本です。

## 62. Normal MapのMip

Normal Vectorを単純平均すると長さが変わります。専用処理で平均後に正規化し、圧縮とChannel規約も合わせます。

## 63. RoughnessのMip

単純平均がMaterialの見た目を完全には保存しない場合があります。Normal varianceをRoughnessへ反映するなど高度なAsset処理があります。

## 64. Alpha CutoutのMip

縮小でAlpha Coverageが変わると、葉や柵が遠距離で消えます。Coverage preserving Mip生成を検討します。

## 65. UI TextureのMip

常に1:1表示するUIはMip不要の場合があります。ScaleやDPI変更があるならMipが有効なこともあります。用途で判断します。

## 66. Pixel ArtのMip

意図したPixel境界を保つためPoint FilterとMipなし、または専用Mipを使います。Camera Scaleを整数倍へ揃えることも重要です。

## 67. Texture Atlas

複数画像を一枚へ詰め、UV領域で選びます。SRV Binding回数を減らせますが、FilterとMipで隣領域が滲む問題があります。

## 68. Atlas UV変換

```hlsl
float2 atlasUv = localUv * rectScale + rectOffset;
```

`rectScale/Offset`はTexture全体の正規化座標として渡します。

## 69. Atlas Padding

各領域の周囲へEdge Pixelを複製した余白を作ります。Linear Filterと下位Mipでも隣Tileが混ざらない幅を確保します。

## 70. Half Texelの誤解

Direct3D 9時代の一般的なHalf-pixel補正をDirect3D 11へそのまま持ち込みません。ただしAtlas境界のTexel CenterとFilter Footprintは別問題として扱います。

## 71. Texel Center

幅WのTextureでTexel中心は概念上`(x + 0.5) / W`です。Point SamplingやAtlasの正確な位置計算で役立ちます。

## 72. UV Seam

ModelのUV Seamでは同じ位置に異なるUVを持つため頂点を分割します。TangentもSeamやMirrorで分割・符号管理が必要です。

## 73. Mirror UVとTangent

UVをMirrorした領域ではTangent SpaceのHandednessが変わります。Tangentの符号を保存しNormal Mappingで反映します。

## 74. Wrap SeamとMip

Tileable Textureの左右上下端が連続していないと、Wrap時やMip生成でSeamが見えます。Asset自体を周期的に作ります。

## 75. Texture StreamingとのLOD

高解像度Mipが未Residentな間、利用可能な最詳細Mipを制限します。SRV範囲、Sampler MinLOD、Resource方式を一貫させます。

## 76. LOD Clampの用途

Streaming中の未Upload Mip参照防止、DebugでMip固定、低品質Modeなどに使えます。

## 77. Mip可視化

Mipごとに異なる色を付けるDebug Textureや、Shaderで推定LODをColor表示すると、過度な高解像度Sampleやちらつきを発見できます。

## 78. UV可視化

```hlsl
return float4(frac(input.uv), 0.0f, 1.0f);
```

Tiling、Mirror、Seam、座標反転の問題を確認できます。

## 79. Sampler State集合

```text
PointClamp
PointWrap
LinearClamp
LinearWrap
AnisotropicWrap
ShadowComparisonBorder
```

有限個の共通SamplerをRendererで共有する設計が扱いやすくなります。

## 80. Materialとの境界

MaterialがSamplerを自由生成するより、Filtering/Address PolicyをEnumで選び、共通Cacheから取得すると重複を防げます。

## 81. Global品質設定

AnisotropyやLOD Biasを品質Presetで変える場合、Sampler Cache Keyと再Binding/再作成の仕組みが必要です。

## 82. Sampler Feedbackとの区別

Direct3D 11の通常Sampler Stateと、より新しいAPIのSampler Feedback機能は別概念です。名前だけで混同しません。

## 83. Debug Name

```cpp
SetDebugName(*sampler.Get(), "Anisotropic Wrap x8");
```

Filter、Address、Anisotropy、Comparison用途を名前で識別します。

## 84. よくある失敗：Mipなしで遠景を描く

床や細線模様が激しくちらつきます。完全Mip Chainと適切なMinification Filterを用意します。

## 85. よくある失敗：MaxAnisotropyだけ変更

Filterが`ANISOTROPIC`でなければ期待した異方性Filterになりません。Filterと値を一組で設定します。

## 86. よくある失敗：AtlasにWrapを使う

Subrect外へ出たUVがTexture全体をWrapし、別Spriteを読みます。UV Clamp、Padding、Sampler設計を合わせます。

## 87. よくある失敗：Color/Dataで同じMip生成

sRGB Color、Normal、Roughness、Alpha Cutoutを同じ平均処理へ通します。Texture Semantic別に生成規則を持ちます。

## 88. よくある失敗：GenerateMips条件不足

Render Target Bind Flag、Generate Mips Misc Flag、全Mip SRV、Format Supportのどれかが欠けています。Debug Layerを確認します。

## 89. よくある失敗：古いSamplerが残る

SRVだけMaterialごとに交換し、Sampler Slotは前Materialのままです。Pipeline/Material Bindingで両方を明示します。

## 90. Sampler Test

- Point/Linearを拡大画像で比較する。
- Wrap/Mirror/Clamp/Borderを範囲外UVで確認する。
- Anisotropyを斜めの床で比較する。
- Comparison Samplerを通常Samplerと区別する。
- Stage/SlotがHLSLと一致する。

## 91. Mip Test

- 完全Mip数を非正方形Textureでも計算する。
- 各Mip寸法が最低1になる。
- Point MipとLinear Mipの境界を比較する。
- sRGB、Normal、Alpha Cutout専用Mipを検証する。
- Mip固定ColorでLOD遷移を確認する。

## 92. UV Test

- V方向規約をModel Importと画像で確認する。
- UV 0/1境界と範囲外を描く。
- Atlas Paddingで隣領域が滲まない。
- Mirrored UVのTangent Handednessを確認する。
- ResizeやResolution Scale後もLODが安定する。

## 93. 性能Test

- Mipあり/なしのGPU時間とちらつきを比較する。
- Anisotropy 1/2/4/8/16を比較する。
- Texture Cache missを計測する。
- Sampler State切替回数を記録する。
- 遠距離で高Mipを過剰Sampleしていないか確認する。

## 94. 完成確認表

- [ ] UVとTexel座標を区別できる。
- [ ] MIN/MAG/MIP Filterを説明できる。
- [ ] 五つのAddress Modeを使い分けられる。
- [ ] Anisotropic Filterの用途とCostを説明できる。
- [ ] SamplerとSRVを別々にBindingできる。
- [ ] 完全Mip数を計算できる。
- [ ] DerivativeからLODが選ばれる概念を説明できる。
- [ ] Runtime自動Mip生成の全作成条件をそろえられる。
- [ ] Color、Normal、Alpha用Mip処理を分けられる。
- [ ] AtlasのPaddingとFilter境界を設計できる。

## 95. この章の要点

- UVは参照位置、SRVはDataの見え方、SamplerはSampling規則です。
- FilterはMinification、Magnification、Mip選択の三要素を持ちます。
- Address Modeは範囲外UVの扱いを軸ごとに決めます。
- Mip Mapは遠景のAlias、帯域、Cache効率を改善します。
- Pixel Shaderの通常SampleはUV DerivativeからLODを推定します。
- 自動Mip生成にはSRV/RTV Bind、Generate Mips Flag、対応Formatが必要です。
- Color、Normal、Roughness、Alpha CutoutはMip生成規則が異なります。
- Atlas、Streaming、品質設定ではSamplerとMip範囲を一体で管理します。

## 96. 公式資料

- [D3D11_SAMPLER_DESC](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/ns-d3d11-d3d11_sampler_desc)
- [ID3D11Device::CreateSamplerState](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11device-createsamplerstate)
- [D3D11_FILTER](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/ne-d3d11-d3d11_filter)
- [D3D11_TEXTURE_ADDRESS_MODE](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/ne-d3d11-d3d11_texture_address_mode)
- [ID3D11DeviceContext::PSSetSamplers](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11devicecontext-pssetsamplers)
- [ID3D11DeviceContext::GenerateMips](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11devicecontext-generatemips)
- [Texture sampling functions](https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/dx-graphics-hlsl-to-sample)
- [Sample](https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/dx-graphics-hlsl-to-sample)
- [SampleLevel](https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/dx-graphics-hlsl-to-samplelevel)
- [SampleGrad](https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/dx-graphics-hlsl-to-samplegrad)

次章では、三角形を塗りつぶすか線で描くか、表裏をどう判定するか、描画範囲をどう切り取るかを決めるRasterizer State、Cull、Scissorを扱います。
