# DirectX 11：Vertex Shader・Pixel Shader

この章では、Compile済みBytecodeからVertex ShaderとPixel Shaderを作り、PipelineへBindingして描画するまでを学びます。各Stageの入力・出力、Semantic、補間、座標変換、Material、分岐、破棄、Shader交換、3D Action描画への発展を扱います。

## 1. 描画経路

```text
Vertex Buffer
-> Input Assembler
-> Vertex Shader
-> Rasterizer / interpolation
-> Pixel Shader
-> Output Merger
-> Render Target
```

## 2. Vertex Shaderの責任

Vertexごとに実行され、最低限Clip Space位置を`SV_Position`として出力します。座標変換、Skinning、頂点属性の準備を担当します。

## 3. Pixel Shaderの責任

RasterizeされたFragmentごとに実行され、色などを`SV_Target`へ出力します。Texture、Lighting、Material、Fog、Effectを計算します。

## 4. VertexはPixelではない

Vertex Shaderは三角形の頂点だけを処理します。三角形内部のFragmentはRasterizerが生成し、Vertex Shader出力を補間してPixel Shaderへ渡します。

## 5. 共通HLSL構造

```hlsl
struct VSInput
{
    float3 position : POSITION;
    float4 color    : COLOR0;
};

struct VSOutput
{
    float4 position : SV_Position;
    float4 color    : COLOR0;
};
```

## 6. Vertex Shader本体

```hlsl
VSOutput VSMain(VSInput input)
{
    VSOutput output;
    output.position = float4(input.position, 1.0f);
    output.color = input.color;
    return output;
}
```

入力位置がすでにClip Spaceにある最小例です。

## 7. Pixel Shader本体

```hlsl
float4 PSMain(VSOutput input) : SV_Target0
{
    return input.color;
}
```

Rasterizerが補間したColorをRender Target 0へ出します。

## 8. Stage間契約

Vertex Shader出力とPixel Shader入力はSemantic名とIndexで対応します。C++構造体のField順のような単純な位置対応ではありません。

## 9. SV_Position

Vertex Shader出力ではClip Space位置を表します。Perspective DivideとViewport変換後、Pixel Shader入力ではScreen上の位置として意味が変化します。

## 10. Clip Space

```text
-w <= x <= w
-w <= y <= w
 0 <= z <= w  (Direct3D convention)
```

範囲外PrimitiveはClipping対象になります。

## 11. Perspective Divide

Rasterizerへ進む前に概念上`x/w`、`y/w`、`z/w`が計算されます。Vertex Shaderは通常、自分でDivideせずHomogeneous座標を出力します。

## 12. World・View・Projection

```text
Local -> World -> View -> Clip
```

Object配置、Camera変換、透視投影を順に適用します。

## 13. 変換例

```hlsl
cbuffer PerObject : register(b0)
{
    float4x4 worldViewProjection;
};

output.position = mul(float4(input.position, 1.0f), worldViewProjection);
```

CPU側と行列方向・転置規約を一致させます。

## 14. w成分

位置は通常`w = 1`、方向Vectorは`w = 0`として変換します。平行移動を方向へ適用しないためです。

## 15. 補間

三角形の各頂点から出たUV、Color、Normalなどは、Fragment位置に応じて補間されます。通常はPerspective-correct interpolationです。

## 16. nointerpolation

```hlsl
nointerpolation uint materialId : MATERIAL_ID;
```

整数IDなど、三角形内で補間してはいけない値に使います。

## 17. centroidとsample

MSAA境界で補間位置を制御するModifierです。必要性を測定し、通常描画へ無条件に付けません。

## 18. Semanticの一致例

```hlsl
// VS output
float2 uv : TEXCOORD0;

// PS input
float2 uv : TEXCOORD0;
```

変数名が異なってもSemantic契約が一致すれば接続されます。

## 19. 不要出力

Pixel Shaderが使わないVertex Shader出力は最適化される場合があります。Stage Interfaceは必要最小限にし、帯域を抑えます。

## 20. Vertex Shader Object作成

```cpp
Microsoft::WRL::ComPtr<ID3D11VertexShader> vertexShader;
ThrowIfFailed(device->CreateVertexShader(
    vsBytecode->GetBufferPointer(),
    vsBytecode->GetBufferSize(),
    nullptr,
    vertexShader.GetAddressOf()));
```

## 21. Pixel Shader Object作成

```cpp
Microsoft::WRL::ComPtr<ID3D11PixelShader> pixelShader;
ThrowIfFailed(device->CreatePixelShader(
    psBytecode->GetBufferPointer(),
    psBytecode->GetBufferSize(),
    nullptr,
    pixelShader.GetAddressOf()));
```

## 22. Class Linkage引数

第3引数はDynamic Shader Linkage用です。使わない基本構成では`nullptr`にします。

## 23. Shader作成とCompile

CompileはHLSLからBytecodeを作り、`CreateVertexShader`等はBytecodeからDevice Objectを作ります。失敗地点を別々に診断します。

## 24. VSSetShader

```cpp
context->VSSetShader(vertexShader.Get(), nullptr, 0);
```

第2・第3引数はClass Instance配列と個数です。通常は`nullptr, 0`です。

## 25. PSSetShader

```cpp
context->PSSetShader(pixelShader.Get(), nullptr, 0);
```

この呼び出しだけではTextureやConstant BufferはBindingされません。

## 26. Shaderを解除する

```cpp
context->VSSetShader(nullptr, nullptr, 0);
context->PSSetShader(nullptr, nullptr, 0);
```

Pass境界やDebugで明示解除できますが、毎Drawの不要な解除は避けます。

## 27. BindingはContext State

ShaderをBindingすると以後のDrawがそのStateを使います。Shader Object作成だけでは描画へ影響しません。

## 28. Drawに必要な他State

- Vertex BufferとStride/Offset。
- Input Layout。
- Primitive Topology。
- Viewport。
- Render Target。
- 必要なConstant Buffer、Texture、Sampler。

Shader二つだけでは三角形は出ません。

## 29. 最小Binding順

```cpp
context->IASetInputLayout(inputLayout.Get());
context->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);
context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
context->VSSetShader(vertexShader.Get(), nullptr, 0);
context->PSSetShader(pixelShader.Get(), nullptr, 0);
context->Draw(3, 0);
```

## 30. Draw引数

```cpp
context->Draw(vertexCount, startVertexLocation);
```

Index Bufferを使わない描画です。`DrawIndexed`とは入力の読み方が異なります。

## 31. Vertex IDだけで三角形を作る

```hlsl
VSOutput VSMain(uint vertexId : SV_VertexID)
{
    const float2 positions[3] =
    {
        float2( 0.0f,  0.5f),
        float2( 0.5f, -0.5f),
        float2(-0.5f, -0.5f)
    };

    VSOutput output;
    output.position = float4(positions[vertexId], 0.0f, 1.0f);
    return output;
}
```

Vertex BufferなしのPipeline確認に使えます。

## 32. Fullscreen Triangle

`SV_VertexID`から画面を覆う大きな三角形を作る方式はPost Processで便利です。四角形より頂点数と対角線境界を減らせます。

## 33. Pixel Shaderを省略できる場合

Depth-only PassなどColor出力不要ならPixel Shaderを`nullptr`にできる場合があります。ただしRasterizerやAlpha Test相当の処理要件を確認します。

## 34. discard

```hlsl
if (alpha < alphaCutoff)
{
    discard;
}
```

該当Fragmentの出力を破棄します。草・柵などCutout表現に使います。

## 35. discardのCost

Early Depth最適化やGPU実行効率へ影響する場合があります。透明度の低い領域、Overdraw、Alpha-to-Coverage等と比較します。

## 36. clip

```hlsl
clip(alpha - alphaCutoff);
```

値が負なら破棄する組込み関数です。`discard`と同じ目的で簡潔に書けます。

## 37. derivative

`ddx`、`ddy`、`fwidth`は近傍Pixelとの差を使います。Mip選択、Normal計算、Anti-aliasingに有用ですが、分岐内の不定なDerivativeへ注意します。

## 38. Branch

```hlsl
if (useEffect)
{
    color.rgb *= effectColor;
}
```

GPUはThread群で実行するため、隣接Threadが別Branchへ進むDivergenceで両経路を処理する場合があります。

## 39. Branchを恐れすぎない

一様な条件や重い処理の回避では有効です。手作業でBranchless化して必ず高速になるとは限らず、Compiler結果とGPU計測で判断します。

## 40. Texture Sampling

```hlsl
Texture2D baseColorTexture : register(t0);
SamplerState baseSampler : register(s0);

float4 sampled = baseColorTexture.Sample(baseSampler, input.uv);
```

Texture ResourceとSampler Stateは別Bindingです。

## 41. Linear空間でLighting

Base Color TextureのsRGB Decode、Lighting計算、最終Encodeの責任を統一します。Gamma空間で直接Lightingすると結果が不自然になります。

## 42. Normalの変換

非一様Scaleを含むWorld変換では、位置と同じMatrixでNormalを変換できません。逆転置Matrixまたは適切に構築したNormal Matrixを使います。

## 43. Normalの正規化

補間後のNormalは長さ1とは限りません。Pixel Shaderで`normalize`してからLightingへ使います。

## 44. Material出力

Forward RenderingではLighting後のColorを直接出し、Deferred RenderingではBase Color、Normal、Roughness等を複数Render Targetへ出します。

## 45. Multiple Render Targets

```hlsl
struct GBufferOutput
{
    float4 baseColor : SV_Target0;
    float4 normal    : SV_Target1;
};
```

C++側RTV SlotとFormatを一致させます。

## 46. Depth出力

Pixel Shaderから`SV_Depth`を出力できますが、Early Depth最適化へ影響する場合があります。必要なAlgorithmだけで使用します。

## 47. Skinningへの発展

Vertex ShaderでBone IndexとWeightを読み、複数Bone Matrixによる位置・Normal変換を合成します。Weight正規化、最大Influence数、Palette範囲を検証します。

## 48. Motion Vectorへの発展

現在Frameと前FrameのClip Positionを出力・比較し、Temporal AAやMotion Blur用の速度を生成します。Camera CutとTeleport時は履歴を無効化します。

## 49. Hit Effectへの発展

Damage Flash、Dissolve、Outline用ParameterをMaterial Constantへ渡します。Gameplay状態をShader Branchの乱立にせず、Effect VariantとDataを整理します。

## 50. Shader Variant管理

```text
skinned / static
opaque / cutout
normal-map on / off
shadow receive on / off
```

全組合せを無制限に増やさず、実際に使うVariantだけをBuildします。

## 51. Shader Objectの所有

```cpp
struct GraphicsShaders
{
    Microsoft::WRL::ComPtr<ID3D11VertexShader> vertex;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> pixel;
    Microsoft::WRL::ComPtr<ID3DBlob> vertexBytecode;
};
```

Vertex BytecodeはInput Layout作成まで保持します。

## 52. Hot Reload交換

新BytecodeのCompile、Shader Object作成、Reflection検証がすべて成功した後、Frame境界で組を交換します。VSだけ新しくPSが旧契約の状態を作りません。

## 53. Debug Name

```cpp
SetDebugName(*vertexShader.Get(), "Character VS Skinned");
SetDebugName(*pixelShader.Get(), "Character PS Opaque");
```

Stage、Material Path、Variantを識別できる名前にします。

## 54. よくある失敗：SV_Position未出力

Vertex Shaderが有効なClip Positionを返さず、Primitiveが表示されません。NaN、`w = 0`、Matrix不一致も確認します。

## 55. よくある失敗：Semantic不一致

VS出力が`TEXCOORD0`、PS入力が`TEXCOORD1`になっています。Compile成功しても期待Dataが接続されません。

## 56. よくある失敗：Shader作成だけで満足

ContextへBindingしていないため旧ShaderまたはNullが使われます。Graphics DebuggerでPipeline Stateを確認します。

## 57. よくある失敗：Constant Buffer Slot違い

HLSLは`b0`、C++はSlot 1へBindingします。Reflectionで名前、Slot、Sizeを検証します。

## 58. よくある失敗：TextureだけBinding

SRVを設定してSamplerを忘れます。ResourceとSampling規則は別Objectです。

## 59. よくある失敗：透明をdiscardだけで表現

半透明表現にはBlend、描画順、Depth Write方針が必要です。CutoutとTranslucencyを区別します。

## 60. 最小三角形テスト

- `SV_VertexID`だけで三角形を表示する。
- 固定色Pixel Shaderで出力を確認する。
- 頂点Color補間を確認する。
- ViewportとCull方向を確認する。
- Debug Layer Warningがないことを確認する。

## 61. 座標変換テスト

- Identityで元形状が出る。
- Translation、Rotation、Scaleを個別に試す。
- PerspectiveでNear/Farを確認する。
- Window Resize後にAspectを更新する。
- CPU/HLSL行列規約を自動Testする。

## 62. Materialテスト

- TextureなしFallback色を表示する。
- sRGB Decode規約を確認する。
- Normalを可視化する。
- Alpha Cutoff境界を確認する。
- 未Binding Resourceを検出する。

## 63. 完成確認表

- [ ] VSとPSの実行単位を説明できる。
- [ ] Clip Spaceと`SV_Position`を説明できる。
- [ ] Stage間Semanticを一致させられる。
- [ ] Shader Objectを作成・Bindingできる。
- [ ] `Draw`に必要な他Stateを列挙できる。
- [ ] 補間と`nointerpolation`を使い分けられる。
- [ ] TextureとSamplerを別々にBindingできる。
- [ ] `discard`と半透明を区別できる。
- [ ] Matrix、Normal、Color Space規約を統一できる。
- [ ] Hot ReloadをShader組単位で安全に交換できる。

## 64. この章の要点

- Vertex Shaderは頂点を処理し、Clip Spaceの`SV_Position`を必ず出力します。
- Pixel Shaderは補間された値からRender Target出力を計算します。
- Stage間はSemantic名とIndexで接続されます。
- Compile、Shader Object作成、Context Bindingは別工程です。
- Shader以外のInput Assembly、Viewport、Render TargetもDrawに必要です。
- 分岐やdiscardの性能は推測せずGPUで計測します。
- 3D Action描画ではSkinning、Material Variant、Motion Vectorへ発展します。
- ReflectionとGraphics DebuggerでBinding契約を検証します。

## 65. 公式資料

- [Vertex shader stage](https://learn.microsoft.com/en-us/windows/win32/direct3d11/vertex-shader-stage)
- [Pixel shader stage](https://learn.microsoft.com/en-us/windows/win32/direct3d11/pixel-shader-stage)
- [ID3D11Device::CreateVertexShader](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11device-createvertexshader)
- [ID3D11Device::CreatePixelShader](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11device-createpixelshader)
- [ID3D11DeviceContext::VSSetShader](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11devicecontext-vssetshader)
- [ID3D11DeviceContext::PSSetShader](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11devicecontext-pssetshader)
- [HLSL semantics](https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/dx-graphics-hlsl-semantics)
- [Shader model 5 assembly](https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/shader-model-5-assembly--directx-hlsl-)
- [ID3D11DeviceContext::Draw](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11devicecontext-draw)

次章では、C++の頂点構造体をGPU Memoryへ配置するVertex Bufferと、Byte LayoutをShader Semanticへ接続するInput Layoutを扱います。
