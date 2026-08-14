# DirectX 11：Lighting・Normal・Material

この章では、Surfaceの向きであるNormal、光源Data、Material Parameterを組み合わせてPixel Colorを計算するLightingを学びます。Lambert、Blinn-Phong、Directional/Point/Spot Light、Tangent Space Normal Mapping、Metallic-Roughness PBR、GGX、Fresnel、IBL、Material管理までを扱います。

## 1. Lightingの入力

```text
surface position
surface normal
view direction
light direction / position / color / intensity
material parameters
shadow / ambient / environment
```

すべて同じ座標空間とColor Spaceで計算します。

## 2. Linear Color Space

Lightingは通常Linear空間で行います。Base Color TextureをsRGB SRVでDecodeし、最終表示時に適切にEncodeします。

## 3. Gamma空間Lightingの問題

sRGB値を直接加算・乗算すると光量の関係が不正確になり、暗部や中間色が不自然になります。

## 4. Normalとは

Surfaceに垂直な方向Vectorです。位置ではないため平行移動の影響を受けません。

## 5. NormalをUnit化する

```hlsl
float3 N = normalize(input.worldNormal);
```

頂点補間後は長さ1でなくなるためPixel Shaderで再正規化します。

## 6. Normal Matrix

非一様Scaleを含むWorld Matrixでは、線形部分の逆転置でNormalを変換します。位置と同じ行列をそのまま使いません。

## 7. Uniform Scaleの場合

回転＋一様Scaleだけなら簡略化できる場合がありますが、Engine規約で非一様Scaleを許すなら一般解を用意します。

## 8. World Space Lighting

World Position、World Normal、World Light、Camera World Positionを使います。理解しやすくDebug可視化もしやすい方式です。

## 9. View Space Lighting

すべてView Spaceへ変換して計算する方式です。Camera位置が原点になる利点があります。World/Viewを途中で混ぜません。

## 10. Lightへ向かうVector

本章ではSurfaceからLightへ向かうUnit Vectorを`L`とします。

```hlsl
float3 L = normalize(lightPosition - worldPosition);
```

規約を逆にするとDot符号が反転します。

## 11. View Vector

SurfaceからCameraへ向かうUnit Vectorを`V`とします。

```hlsl
float3 V = normalize(cameraPosition - worldPosition);
```

## 12. N dot L

```hlsl
float NdotL = saturate(dot(N, L));
```

SurfaceがLightへ向くほど1、横向きで0、裏向きはClampされ0です。

## 13. Lambert Diffuse

理想拡散Surfaceの基本式です。

```hlsl
float3 diffuse = baseColor * (1.0f / PI) * NdotL;
```

`1/PI`はEnergy normalizationです。

## 14. 学習用簡略Lambert

最初は`baseColor * NdotL`で向きの効果を確認できます。PBRへ進む際は正規化とSpecularのEnergyを含めます。

## 15. Ambient項

一定Colorを足す単純Ambientは理解用には便利ですが、方向性、遮蔽、環境反射を表せません。IBLやProbeへ発展させます。

## 16. Directional Light

太陽のように全SurfaceでLight方向が同じとみなす光です。位置と距離減衰を持ちません。

## 17. Directional Light Data

```cpp
struct DirectionalLight
{
    XMFLOAT3 direction;
    float intensity;
    XMFLOAT3 color;
    float padding;
};
```

Directionが「光が進む向き」か「SurfaceからLightへ向く向き」かを明記します。

## 18. Direction符号

Asset/Editorが光線の進行方向を保存するなら、Shaderの`L`には負号を付けます。名前だけで推測しません。

## 19. Point Light

位置から全方向へ光る光源です。Surfaceごとに方向と距離を計算します。

## 20. Point Light距離

```hlsl
float3 toLight = lightPosition - worldPosition;
float distanceSq = dot(toLight, toLight);
float distance = sqrt(distanceSq);
float3 L = toLight / max(distance, epsilon);
```

距離0を安全に処理します。

## 21. Inverse-square Attenuation

物理的な点光源では概念上`1 / distance^2`で弱くなります。0付近の発散と有限RadiusをEngine式で扱います。

## 22. Range Attenuation

Light Range外を滑らかに0へするWindow関数を掛けます。境界で急に消えるHard Cutを避けます。

## 23. 単位とIntensity

物理単位に近づけるか、Artist向け無次元値にするかを決めます。Light種別間で同じ数値の意味を揃えます。

## 24. Spot Light

位置、方向、内Cone、外Coneを持つ円錐状の光です。距離減衰と角度減衰を掛けます。

## 25. Spot Cone

Light軸とLight-to-surface方向のDotを、内外角のCosineと比較します。毎Pixel`acos`せずCosine空間で計算します。

## 26. Smooth Spot Falloff

```hlsl
float cone = smoothstep(
    outerCos,
    innerCos,
    dot(lightForward, directionToSurface));
```

innerCosは通常outerCosより大きくなります。

## 27. Area Light

面積を持つ光はSoft Highlight/Shadowを作ります。正確な積分は高価なためLTC、Sample、Bake等の近似を使います。

## 28. DiffuseとSpecular

- Diffuse：内部散乱して広く返る成分。
- Specular：Surface境界で鏡面的に返る成分。

Materialに応じてEnergyを分配します。

## 29. Reflection Vector

```hlsl
float3 R = reflect(-L, N);
```

符号規約を確認します。

## 30. Phong Specular

Reflection VectorとView VectorのDotをPowerへ上げます。Exponentが大きいほどHighlightが狭くなります。

## 31. Half Vector

```hlsl
float3 H = normalize(L + V);
```

LightとViewの中間方向です。

## 32. Blinn-Phong

```hlsl
float specularTerm = pow(
    saturate(dot(N, H)),
    shininess);
```

学習しやすい経験的Specular Modelです。

## 33. Shininess

値を大きくするとHighlightが小さく鋭くなります。PBRのRoughnessとは同じParameterではありません。

## 34. PBRとは

物理法則を近似し、Material Parameterと環境が変わっても一貫した見た目を得るRendering方針です。

## 35. Metallic-Roughness Workflow

代表的ParameterはBase Color、Metallic、Roughness、Normal、Occlusion、Emissiveです。

## 36. Base Color

DielectricではDiffuse色、MetalではSpecular反射色に大きく影響します。sRGB Textureとして読みLinearへDecodeします。

## 37. Metallic

0がDielectric、1がMetalを表す基本規約です。中間値は混合・境界・汚れ等に使いますが、均質Materialでは端値が中心です。

## 38. Roughness

Microfacet方向のばらつきを表します。0で滑らか、1で粗いSurfaceです。完全0は数式特異性を避けるため最小値へClampします。

## 39. F0

正面入射付近の基礎Specular反射率です。一般的Dielectricでは小さな値、MetalではBase Color由来の色付きF0を使います。

## 40. F0の混合

```hlsl
float3 dielectricF0 = 0.04f;
float3 F0 = lerp(dielectricF0, baseColor, metallic);
```

0.04は一般的出発点で、IORから求める方が正確です。

## 41. Fresnel

視線がSurfaceへ平行に近づくほど反射が強くなる現象です。

## 42. Schlick Fresnel

```hlsl
float3 F = F0 + (1.0f - F0) *
    pow(1.0f - saturate(dot(H, V)), 5.0f);
```

効率的な近似です。

## 43. Microfacet BRDF

Specularを概念上次の積で表します。

```text
specular = D * F * G / (4 * NdotL * NdotV)
```

Dは分布、FはFresnel、GはMasking/Shadowingです。

## 44. GGX NDF

Microfacet Normalの分布を表すD項です。長いHighlight Tailを持ち、Real-time PBRで広く使われます。

## 45. Geometry Term

Microfacet同士がLight/Viewを遮る効果です。Smith法とSchlick-GGX近似などを使います。

## 46. Denominatorの安全性

NdotL/NdotVが0付近でNaN/Infinityを作らないよう、分母へ小さな下限を設けます。

## 47. Energy Conservation

反射したEnergy以上を出さないようDiffuseとSpecularを配分します。

```hlsl
float3 kS = F;
float3 kD = (1.0f - kS) * (1.0f - metallic);
```

## 48. Direct Lighting式

```text
(kD * baseColor / PI + specularBRDF)
* radiance
* NdotL
```

Lightごとに加算します。

## 49. Radiance

Light ColorとIntensity、距離/角度減衰を組み合わせたSurfaceへ届く光量です。

## 50. HDR計算

Lighting結果は1を超え得ます。Float Scene Colorへ保存し、Exposure/Tone Mappingで表示範囲へ変換します。

## 51. Tone Mappingとの境界

Material Lighting中に各LightをClampせず、HDRで合成してからFrame後半でTone Mappingします。

## 52. Emissive

自身が発光して見えるColorです。通常Direct Lightと無関係に加算しますが、他Objectを照らすにはGI等が必要です。

## 53. EmissiveとBloom

1を超えるEmissive強度をHDRで保持するとBloom抽出へつなげられます。表示Colorと発光強度を別Parameterにできます。

## 54. Ambient Occlusion

狭い隙間で環境光が届きにくい近似値です。Direct Light全体を単純に暗くするより、主にIndirect成分へ適用します。

## 55. IBL

Image-Based LightingはEnvironment MapからDiffuse/Specular間接光を得ます。

## 56. Diffuse IBL

Irradiance MapまたはSpherical Harmonics等で、Normal方向周辺の広い環境光を積分した値を使います。

## 57. Specular IBL

Roughness別にPrefilterしたEnvironment MapとBRDF LUTを使うSplit-sum近似が一般的です。

## 58. Environment Reflection Vector

View VectorをNormalで反射した方向からCube MapをSampleします。座標系とCube Face方向を確認します。

## 59. Normal Map

Textureに細かなSurface方向を保存し、Geometryを増やさずLighting detailを追加します。

## 60. Tangent Space Normal

一般的Normal MapはTangent Spaceで保存されます。Sample値0から1を-1から1へ変換します。

```hlsl
float3 tangentNormal = normalSample.xyz * 2.0f - 1.0f;
```

## 61. Normal MapはLinear Data

sRGB Decodeを適用しません。Color TextureではなくVector Dataです。

## 62. TBN Matrix

Tangent、Bitangent、NormalでTangent SpaceからWorld/View Spaceへ変換します。

## 63. Tangent Handedness

頂点Tangentのwへ符号を保存し、Mirror UVでBitangent方向を再構築します。

```hlsl
float3 B = tangentSign * cross(N, T);
```

Cross順はProject規約に合わせます。

## 64. TBNの再直交化

補間後のTangentをNormalへ直交化し、両方NormalizeしてからBasisを作ります。

## 65. Normal Channel規約

DirectX/OpenGL系ToolでY Channel符号が異なる場合があります。Asset Import時に統一し、Shaderごとの手動反転を避けます。

## 66. Vertex Normal生成

共有頂点に隣接するFace Normalを角度/面積で加重平均します。Hard Edgeでは頂点を分割します。

## 67. Smoothing Group

どのFace間でNormalを共有するかを決めます。UV Seam、Material境界、Hard Edgeと頂点分割に影響します。

## 68. Material Data

```cpp
struct alignas(16) MaterialConstants
{
    XMFLOAT4 baseColorFactor{1, 1, 1, 1};
    XMFLOAT3 emissiveFactor{0, 0, 0};
    float metallicFactor = 0;
    float roughnessFactor = 1;
    float normalScale = 1;
    float occlusionStrength = 1;
    float alphaCutoff = 0.5f;
};
```

実際は16 Byte Packingを再確認します。

## 69. Texture×Factor

```text
final base color = texture sample * material factor * optional instance tint
```

Textureがない場合はFallback Sample 1を使えます。

## 70. Packed Texture

Occlusion/Roughness/Metallicを別Channelへ詰める方式があります。Channel規約をMetadataとShaderで一致させます。

## 71. Material Blend Mode

Opaque、Mask/Cutout、TransparentをMaterial属性として持ち、Blend/Depth/Rasterizer StateとShader Variantを選びます。

## 72. Double-sided Material

Cull Noneだけでなく、裏面Normal、Tangent、Shadow、Lightmap等の規約を設定します。

## 73. Forward Rendering

Object描画時にLightを評価して最終Colorを出します。透明物、MSAA、単純Sceneに向きますが、多数Lightの管理が課題です。

## 74. Deferred Rendering

Geometry情報をG-bufferへ保存し、後でLightを画面空間評価します。多数Lightに強い一方、透明物、帯域、MSAAが複雑です。

## 75. Forward+

Screen Tile/Clusterごとに影響Light Listを作り、Forward Shaderが該当Lightだけ評価します。

## 76. Light Culling

Point/Spot LightのBoundsをFrustum、Tile、Clusterで絞ります。全Pixelが全LightをLoopしない設計にします。

## 77. Light Data Upload

少数固定LightならConstant Buffer、多数可変LightならStructured Buffer等を検討します。Feature LevelとShaderアクセス方式を合わせます。

## 78. Material Cache

Shader Variant、Texture SRV、Sampler、Constant、Blend/Depth/Rasterizer StateをMaterial Assetとしてまとめ、同一組をBatchします。

## 79. Hit Flash

MaterialのBase Colorを直接永久変更せず、一時Effect ParameterとしてEmission/Tintを加えます。元MaterialとGameplay状態を分離します。

## 80. Rim Lighting

`1 - saturate(dot(N,V))`を基に輪郭を強調します。視認性、Hit、Ultimate演出に有効ですが、PBR Energyとは別のStylized項として管理します。

## 81. Dissolve

NoiseとThresholdで`clip`し、境界へEmissionを加えます。Shadow PassとDepth/Velocity Passでも同じMask規則を使います。

## 82. Toon Lighting

NdotLを段階化し、Ramp Texture等で色を選びます。PBRとは異なるStylized Modelとして、ShadowやSpecular規約を明示します。

## 83. Debug View

Base Color、World Normal、Roughness、Metallic、AO、Emissive、NdotL、Light Countを個別表示します。

## 84. よくある失敗：NormalをNormalizeしない

補間で長さが変わり、Light強度が不正になります。変換・補間後にNormalizeします。

## 85. よくある失敗：座標空間混在

World NormalとView-space LightをDotします。Variable名へSpaceを含め、Shader Interfaceを統一します。

## 86. よくある失敗：Normal MapをsRGBで読む

Vector成分へ非線形変換が入り方向が壊れます。Linear SRVを使います。

## 87. よくある失敗：Roughness 0でNaN

GGX式の分母が不安定になります。Perceptual Roughnessへ実用的な最小値を設けます。

## 88. よくある失敗：各Light結果をClamp

HDR Energyを失い、Tone Mapping/Bloomが機能しません。Float Targetで加算後に表示変換します。

## 89. Normal Test

- World NormalをRGB表示する。
- 非一様ScaleでNormal Matrixを確認する。
- Normal Mapなし/ありを比較する。
- Mirror UVのTangent符号を確認する。
- Front/Back Faceを確認する。

## 90. Light Test

- Directional/Point/Spotを単独表示する。
- 距離と角度減衰をGraph/画面で確認する。
- NdotL 0/0.5/1の既知面を使う。
- Light 0個でAmbient/IBLだけを見る。
- 多数LightのGPU時間を測る。

## 91. Material Test

- Metallic 0/1、Roughness 0.1/0.5/1をGrid表示する。
- Base ColorをsRGB/Linearで比較する。
- EmissiveをHDR値でBloomへ渡す。
- Packed Channelを個別可視化する。
- Fallback Textureで有効値になる。

## 92. BRDF Test

- NdotL/NdotVを0付近で有限に保つ。
- FresnelがGrazing Angleで強くなる。
- RoughnessでHighlight幅が変わる。
- MetalでDiffuseが減る。
- Reference Image/Shaderと比較する。

## 93. 完成確認表

- [ ] Normalを正しいMatrixで変換・Normalizeできる。
- [ ] L、V、N、Hの方向規約を説明できる。
- [ ] Directional/Point/Spot Lightを実装できる。
- [ ] LambertとBlinn-Phongを説明できる。
- [ ] Base Color/Metallic/Roughness/F0を説明できる。
- [ ] GGXのD/F/G項を説明できる。
- [ ] Energy ConservationをDiffuse/Specularへ適用できる。
- [ ] TBNとNormal Map Channel規約を管理できる。
- [ ] HDR Lighting、IBL、Emissiveを分離できる。
- [ ] Forward/Deferred/Forward+を比較できる。

## 94. この章の要点

- Lighting入力は同じ座標空間・Linear Color Spaceへそろえます。
- Normalは逆転置Matrixで変換し、補間後にNormalizeします。
- Directional、Point、Spot Lightは方向・距離・Cone計算が異なります。
- LambertはDiffuse、Blinn-Phongは学習用Specularの基本です。
- PBRはBase Color、Metallic、RoughnessとMicrofacet BRDFで一貫した反射を作ります。
- GGX SpecularはD/F/Gと分母、DiffuseはEnergy Conservationを考慮します。
- Normal MapはLinear Dataで、TBNとMirror UV Handednessが必要です。
- Material、Light Culling、Debug ViewをRenderer Architectureとして管理します。

## 95. 公式資料

- [HLSL intrinsic functions](https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/dx-graphics-hlsl-intrinsic-functions)
- [dot](https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/dx-graphics-hlsl-dot)
- [cross](https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/dx-graphics-hlsl-cross)
- [normalize](https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/dx-graphics-hlsl-normalize)
- [reflect](https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/dx-graphics-hlsl-reflect)
- [Lighting in Direct3D](https://learn.microsoft.com/en-us/windows/win32/direct3d9/lighting)
- [DirectXMath vector functions](https://learn.microsoft.com/en-us/windows/win32/dxmath/ovw-xnamath-reference-functions-vector3)

次章では、Model FileからMesh、Submesh、Material、Skeleton、Animationを読み、GPU ResourceとAnimation Poseへ変換する設計を扱います。
