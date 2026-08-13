# Shader・Material・PBR基礎

ShaderはGPU Stageで実行されるProgram、MaterialはShaderとParameter・Textureの組合せです。Artistが調整するデータとRendererの実装を分けます。

## Material Parameter

```cpp
struct MaterialParameters
{
    Color baseColor{Color::White};
    float metallic{};
    float roughness{0.5F};
    Vector3 emissive{};
    TextureHandle baseColorTexture{};
    TextureHandle normalTexture{};
};
```

値範囲をImport/Editorで検証します。Roughness 0でも現実の完全鏡面やSampling問題があるため最小値を設ける場合があります。

## Shader Input/Output

Vertex ShaderがClip PositionとWorld Position、Normal、UV等を出し、Pixel Shaderが補間値から色を計算します。使わないVaryingを増やすとBandwidthを消費します。

## Lambert拡散

```text
NdotL = max(dot(N, L), 0)
diffuse = baseColor * lightColor * NdotL
```

`N`と`L`を正規化し、LをSurface→LightかLight→Surfaceのどちらとするか統一します。

## PBR Metallic-Roughness

一般的なPBRでは次を組み合わせます。

- Base Color。
- Metallic。
- Roughness。
- Normal。
- Ambient Occlusion。
- Emissive。

非金属のSpecular反射と拡散、金属の色付きSpecularをエネルギー保存に配慮して扱います。BRDFにはCook-Torrance、GGX等が使われます。

## Normal Map

Textureの値を`[0,1]`から`[-1,1]`へDecodeし、Tangent SpaceからWorld/ViewへTBNで変換します。

```text
nTS = normalize(textureNormal * 2 - 1)
nWS = normalize(T * nTS.x + B * nTS.y + N * nTS.z)
```

DirectX/OpenGL系でGreen Channel向きが異なるAssetがあります。Import設定とTangent Basisを合わせます。

## 色空間

Base Colorは通常sRGBとしてSample後にLinearへDecodeします。Normal、Roughness、Metallic、AOはDataなのでsRGB変換しません。LightingはLinearで行い、最終出力でTone Mappingと表示色空間変換を行います。

## Texture Packing

Roughness、Metallic、AOをRGB ChannelへまとめBandwidthとBindingを減らせます。Channel Conventionを固定し、Compressionによる相互影響を確認します。

## Alpha Mode

- Opaque：Blendなし。
- Masked/Cutout：閾値で破棄。Depth Write可能。
- Transparent：Blendし通常は後方から描画。

Transparentは順序、Depth Write、Shadow、Refractionが難しく、可能ならMaskedで表現します。

## Material Instance

親MaterialのShader構造を共有し、Parameterだけ上書きします。Objectごとに完全なMaterialを複製せず、共有Immutable DataとInstance Dataを分けます。

## Uniform分岐とVariant

FeatureをRuntime BranchにするかCompile-time Variantにするかを選びます。

- Variant：不要コードを除去できるが組合せ爆発。
- Branch：Variant削減、ただしGPU分岐・Resourceコスト。

Material Feature集合を制限し、実際に使うVariantだけBuildします。

## Shaderの数値問題

- 0長Normal。
- 0除算。
- `pow`への負入力。
- NaNがBloom等へ伝播。
- Half精度Range不足。

Debug ViewでNormal、Roughness、Metallic、UV、NaN/Infを可視化します。

## Toon表現

NdotLをRamp Textureや閾値で段階化し、Rim Light、Outline、Face専用Normal等を組み合わせます。単純な`step`だけではAliasが出るため、`smoothstep`、Derivative、MSAA/TAAを考慮します。Stylizedでも色空間、Shadow、Material管理は必要です。

## Material Systemの責任

- Parameter Layoutと型。
- Default/不正Texture代替。
- Shader Variant選択。
- Resource Binding。
- SerializationとVersion。
- Hot Reload。
- Editor UI。
- GPU Memory/Descriptor寿命。

文字列Parameterを毎Draw検索せず、Build時Reflectionと安定Handleを使います。
