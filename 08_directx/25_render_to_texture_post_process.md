# DirectX 11：Render to Texture・Post Process

この章では、画面へ直接描かず中間TextureへSceneを描くRender to Textureと、その画像へ複数のFull-screen Passを適用するPost Processを学びます。HDR Scene Color、Ping-pong、Blur、Bloom、Exposure、Tone Mapping、Color Grading、Depth再構築、Motion Blur、TAA、Resource Poolまでを扱います。

## 1. Render to Textureとは

Swap Chain Back Buffer以外のTextureをRender Targetとして描画することです。作った画像を後続Shaderの入力として利用できます。

## 2. 基本Pipeline

```text
geometry -> HDR scene color
-> post effects
-> tone mapping
-> UI composite
-> back buffer
-> Present
```

## 3. 中間Textureを使う理由

- HDR値を保持する。
- Bloom、Blur、Color補正を適用する。
- Render ResolutionをWindowから分離する。
- Scene Capture、Mirror、Minimapを作る。
- Pass間依存を明示する。

## 4. Render TextureのView

```text
ID3D11Texture2D
├─ RTV : pass output
└─ SRV : later pass input
```

必要ならUAVも作ります。

## 5. Texture Descriptor

```cpp
D3D11_TEXTURE2D_DESC desc{};
desc.Width = width;
desc.Height = height;
desc.MipLevels = 1;
desc.ArraySize = 1;
desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
desc.SampleDesc = {1, 0};
desc.Usage = D3D11_USAGE_DEFAULT;
desc.BindFlags = D3D11_BIND_RENDER_TARGET |
                 D3D11_BIND_SHADER_RESOURCE;
```

## 6. HDR Format

`R16G16B16A16_FLOAT`は1を超えるLightingや負値を保持できる代表的なScene Color Formatです。Memory/Bandwidthとの釣合いを測ります。

## 7. Format Support

Render Target、Shader Sample、Blend、必要ならTyped UAV等の対応を`CheckFormatSupport`で確認します。

## 8. Size依存Resource

Scene Color、Depth、Velocity、Post Process中間TextureはRender Resolution変更時に再作成します。

## 9. Render Resolution

Back Buffer Sizeと一致させる必要はありません。Dynamic Resolutionでは3D Sceneを縮小し、最終PassでWindow Sizeへ拡大します。

## 10. Viewport

Target Texture寸法に合うViewportを設定します。前PassのViewportを流用しません。

## 11. Scene Pass Binding

```cpp
ID3D11RenderTargetView* rtvs[] = {sceneColorRtv.Get()};
context->OMSetRenderTargets(1, rtvs, sceneDepthDsv.Get());
context->RSSetViewports(1, &sceneViewport);
```

## 12. Clear

Scene ColorとDepthを用途に合う値へClearします。HDR ColorのAlphaに何を保存するかも定義します。

## 13. Fullscreen Triangle

Post Processでは`SV_VertexID`から画面全体を覆う三角形を生成できます。Vertex Bufferを不要にし、Quadの対角線境界も避けます。

## 14. Fullscreen VS

```hlsl
VSOutput VSMain(uint id : SV_VertexID)
{
    float2 uv = float2((id << 1) & 2, id & 2);
    VSOutput o;
    o.uv = uv;
    o.position = float4(uv * float2(2, -2) + float2(-1, 1), 0, 1);
    return o;
}
```

UVが0から2まで広がる大三角形です。

## 15. Fullscreen State

Depth Test/Write無効、Cull不要、目的に合うBlend、Target全域Viewportを明示します。

## 16. Input/Output Hazard

同じSubresourceをSRVとして読みながらRTVとして書けません。入力と出力へ別Textureを使います。

## 17. SRV解除

```cpp
ID3D11ShaderResourceView* nullSrv[] = {nullptr};
context->PSSetShaderResources(0, 1, nullSrv);
```

後で同ResourceをRTVへ戻す前に解除します。

## 18. Ping-pong

二枚のTexture A/Bを交互に入力・出力として使います。

```text
pass 1: A -> B
pass 2: B -> A
pass 3: A -> B
```

## 19. Copyを減らす

各Pass後にBack BufferへCopyせず、View参照を入れ替えて最後だけBack Bufferへ出力します。

## 20. Point/Linear Sampler

Colorの拡縮にはLinear、ID/Mask/一部DepthにはPoint等、Data意味に合わせます。Address ModeはPost ProcessではClampが基本です。

## 21. Texel Size

```cpp
XMFLOAT2 texelSize{1.0f / width, 1.0f / height};
```

隣Pixel Sampling Offsetに使います。

## 22. Blur

周辺Sampleを重み付き平均し、高周波Detailを弱めます。Bloom、Depth of Field、UI Effect等の部品です。

## 23. Gaussian Blur

中心からの距離に応じGaussian Weightを使います。Weight合計を1へ正規化します。

## 24. Separable Blur

2D NxN KernelをHorizontal N Sample＋Vertical N Sampleへ分け、Sample数を大幅に減らします。

## 25. Downsample Blur

小さいTextureへ縮小してBlurするとCostが下がり、広いぼけを得やすくなります。Upsample品質を確認します。

## 26. Bloom

明るい部分を抽出し、縮小・Blurして元のHDR Sceneへ加算します。

## 27. Bright Pass

単純Thresholdだけで急な境界を作らず、Soft Kneeで滑らかに抽出できます。

## 28. Bloom Chain

```text
extract
-> downsample levels
-> blur/filter each level
-> upsample and combine
-> add to HDR scene
```

## 29. BloomはTone Mapping前

HDRの輝度差を利用するため、通常Tone Mapping前に処理します。

## 30. Exposure

Scene輝度を表示可能範囲へScaleします。Manual ExposureとAuto Exposureがあります。

## 31. Luminance

Linear RGBから重み付きで輝度を求めます。Color SpaceのPrimariesに合う係数を使います。

## 32. Auto Exposure

Luminanceを縮小/Histogram化し、目標Exposureへ時間的に適応します。極端なPixelの影響をPercentile等で抑えます。

## 33. 適応速度

明所から暗所、暗所から明所で別速度を使えます。Frame-independentな指数応答にします。

## 34. Tone Mapping

HDR値をDisplay範囲へ非線形変換します。Reinhard、Filmic、ACES近似等があります。

## 35. Tone Map位置

Lighting、Bloom、Exposure後、sRGB Encode/Output変換前に行うのが基本です。

## 36. Clampとの違い

単純ClampはHighlight Detailを失います。Tone Mapping Curveで明部を滑らかに圧縮します。

## 37. Color Grading

Contrast、Saturation、White Balance、Lift/Gamma/Gain、3D LUT等で最終色調を整えます。

## 38. LUT

入力Linear/Tone-mapped Color Space、LUT Domain、Interpolationを一致させます。色変換順序を明記します。

## 39. Vignette

画面端を暗く/着色します。Aspect Ratioに応じて円が楕円にならないよう座標を補正します。

## 40. Chromatic Aberration

RGB Channelを異なるUVでSampleします。強すぎると可読性と酔いへ悪影響があるため演出時に限定します。

## 41. Depth Texture利用

DepthをSRVとして読み、Fog、Depth of Field、Outline、Position再構築に使います。Writable DSVとの同時Bindingを避けます。

## 42. Depthは非線形

Perspective Depth Buffer値をWorld/View距離として直接使いません。Projection ParameterまたはInverse Projectionで復元します。

## 43. World Position再構築

Screen UVとDepthからClip/NDC Positionを作り、Inverse View ProjectionでWorldへ戻します。

## 44. Coordinate Convention

UV Y、Direct3D NDC z=0..1、Jittered Projection、Reversed-Zをすべて復元式へ反映します。

## 45. Fog

View DistanceやWorld HeightからFog Factorを求め、Fog Color/EnvironmentへBlendします。透明物との順序を設計します。

## 46. Depth of Field

Focus距離との差からCircle of Confusionを求め、前景/背景Blurを合成します。単純一枚Blurでは前景漏れが起きます。

## 47. Motion Vector

Current/Previous Clip Position差をVelocity Textureへ保存します。Camera Cut、Teleport、Jitter差を処理します。

## 48. Motion Blur

Velocity方向へ複数Sampleします。Object境界の色漏れ、巨大Velocity、UIへの適用を制限します。

## 49. TAA

現在FrameとReprojectした履歴を合成し、時間方向のSampleを利用してAliasを減らします。

## 50. History Buffer

前Frame Colorを保持するPersistent Resourceです。一時Texture Poolへ無条件に返しません。

## 51. Reprojection

Motion VectorまたはDepth＋前後Matrixから、現在Pixelが前Frameのどこにあったかを求めます。

## 52. History Rejection

Disocclusion、Depth差、Normal差、Camera Cut、Object ID変化で古い履歴を捨てます。

## 53. Neighborhood Clamp

履歴Colorを現在近傍のColor範囲へClampし、Ghostingを減らします。Color Space選択が結果へ影響します。

## 54. Temporal Reset

Resize、Resolution変更、Camera Cut、Scene切替、Projection大変更でHistoryを無効化します。

## 55. UIの順序

3D Scene用TAA/Motion Blur/Tone Mapping後にUIを描くと文字を安定させられます。World-space UIは別Policyを持てます。

## 56. Resolution Scale

3D SceneとPost Effectの一部を縮小解像度で処理し、最終Upscaleします。UIはNative Resolutionへ描くのが一般的です。

## 57. Dynamic Resolution

GPU時間に応じScaleを変えます。Target再作成、Projection/Jitter、History Reset、Mip/LODへ影響します。

## 58. Resource Pool

同時に寿命が重ならない一時TextureをDescriptor Keyで再利用します。毎FrameCreate/Releaseしません。

## 59. Pool Key

```text
width/height
format
mip/array/sample
bind/misc flags
```

互換でないResourceを同じEntryとして扱いません。

## 60. Lifetime

各Passの最後の読取り後に一時Resourceを返却できます。History、External Output、Readbackは別寿命です。

## 61. Pass Descriptor

```text
name
input SRVs
output RTV/UAV
viewport
shader/state
constants
clear/load policy
```

暗黙Global Stateを減らします。

## 62. Debug Capture

各中間TextureをThumbnail/Fullscreen表示し、NaN、範囲、Alpha、Depth、Velocity、Bloom Levelを確認します。

## 63. よくある失敗：同じTextureを入出力

SRV/RTV Hazardになります。Ping-pongまたは別Subresourceを使います。

## 64. よくある失敗：Viewport残留

Half-resolution Blur後、Back Bufferにも小Viewportが残ります。PassごとにTarget SizeをBindingします。

## 65. よくある失敗：毎Pass Texture作成

CPU/Driver CostとMemory断片化が増えます。Resize時作成またはResource Poolを使います。

## 66. よくある失敗：HDR途中でClamp

BloomやTone Mappingに必要なHighlight情報を失います。適切なFloat Formatで保持します。

## 67. よくある失敗：History Reset忘れ

ResizeやCamera Cut後に旧画像がGhostとして残ります。Reset理由を一元管理します。

## 68. Resource Test

- RTV/SRV互換Viewを作る。
- SRV/RTV競合Warningがない。
- TargetごとにViewportが一致する。
- Resize/Scale変更で再作成する。
- Poolが同時使用Resourceを再貸出ししない。

## 69. Effect Test

- Identity Passが入力と一致する。
- Gaussian Weight合計が1である。
- Bloom Threshold/Kneeを可視化する。
- Tone Map前後を比較する。
- Depth/Motionを個別表示する。

## 70. Temporal Test

- Static Cameraで収束する。
- 移動ObjectでGhostingしない。
- Disocclusionで履歴をRejectする。
- Camera Cut/ResizeでResetする。
- 30/60/120 FPSでExposure応答が近い。

## 71. 完成確認表

- [ ] Render TextureへRTV/SRVを作成できる。
- [ ] Fullscreen Triangleを描ける。
- [ ] SRV/RTV Hazardを回避できる。
- [ ] Ping-pongとSeparable Blurを説明できる。
- [ ] Bloom、Exposure、Tone Mappingの順序を説明できる。
- [ ] DepthからView/World Positionを復元できる。
- [ ] Motion VectorとTAA履歴を管理できる。
- [ ] Temporal Reset条件を列挙できる。
- [ ] Dynamic ResolutionとUI解像度を分離できる。
- [ ] 一時TextureをResource Poolで再利用できる。

## 72. この章の要点

- Render to Textureは描画結果を後続Passの入力にします。
- HDR Scene Colorを保持し、Tone Mapping直前まで不要なClampを避けます。
- Post Processは別Textureへ出力し、Ping-pongでHazardを回避します。
- Bloomは明部抽出・縮小・Blur・再合成、Tone MappingはHDR表示変換です。
- Depthは非線形なので、Projection規約に従い距離/位置を復元します。
- Temporal EffectはPrevious Matrix、Velocity、History Rejection、Resetが必要です。
- PassごとにTarget、Viewport、State、入力解除を明示します。
- Resource PoolはDescriptorとLifetimeに基づき一時Textureを再利用します。

## 73. 公式資料

- [Render targets](https://learn.microsoft.com/en-us/windows/win32/direct3d11/overviews-direct3d-11-resources-bind-flags)
- [ID3D11Device::CreateRenderTargetView](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11device-createrendertargetview)
- [ID3D11Device::CreateShaderResourceView](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11device-createshaderresourceview)
- [ID3D11DeviceContext::OMSetRenderTargets](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11devicecontext-omsetrendertargets)
- [ID3D11DeviceContext::PSSetShaderResources](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11devicecontext-pssetshaderresources)
- [HDR and WCG color](https://learn.microsoft.com/en-us/windows/win32/direct3ddxgi/high-dynamic-range)

次章では、同じMeshを多数描くInstancing、Draw CallをまとめるBatch、見えないObjectを除くCullingを扱います。
