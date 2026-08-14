# DirectX 11：Shadow Mapping

この章では、Light視点でSceneのDepthを記録し、Camera描画時に現在SurfaceのLight Depthと比較するShadow Mappingを学びます。Typeless Resource、DSV/SRV、Bias、PCF、Cascade、Spot/Point Shadow、更新Budgetまでを扱います。

## 1. 二つのPass

```text
shadow pass : light viewからdepth mapを作る
camera pass : depth mapとreceiver depthを比較する
```

## 2. Shadow CasterとReceiver

CasterはShadow MapへDepthを書くObject、ReceiverはCamera PassでShadow判定されるSurfaceです。同じObjectが両方になる場合があります。

## 3. Shadow Mapの中身

Lightから各Texel方向へ見た最も近いDepthです。Color画像ではありません。

## 4. ResourceとView

```text
typeless Texture2D
├─ DSV : shadow passでdepth書込み
└─ SRV : camera passでshader読取り
```

同時に書込みと読取りはできません。

## 5. Texture Descriptor

```cpp
D3D11_TEXTURE2D_DESC desc{};
desc.Width = shadowWidth;
desc.Height = shadowHeight;
desc.MipLevels = 1;
desc.ArraySize = 1;
desc.Format = DXGI_FORMAT_R32_TYPELESS;
desc.SampleDesc = {1, 0};
desc.Usage = D3D11_USAGE_DEFAULT;
desc.BindFlags = D3D11_BIND_DEPTH_STENCIL |
                 D3D11_BIND_SHADER_RESOURCE;
```

## 6. Typeless Format

Resource Memoryの解釈をView側へ任せます。任意Formatへ変換できるわけではなく、互換Group内のViewを作ります。

## 7. DSV Descriptor

```cpp
D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
dsvDesc.Texture2D.MipSlice = 0;
```

## 8. SRV Descriptor

```cpp
D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
srvDesc.Texture2D.MostDetailedMip = 0;
srvDesc.Texture2D.MipLevels = 1;
```

## 9. D24S8系の対応

R24G8 Typeless ResourceへD24_UNORM_S8 DSVとR24_UNORM_X8 SRVを作る構成もあります。Stencilが不要ならD32系が分かりやすい出発点です。

## 10. Format Support

`CheckFormatSupport`でTexture、Depth Stencil、Shader Sample、Comparison Sample等、必要用途を確認します。

## 11. Shadow Viewport

```cpp
D3D11_VIEWPORT viewport{};
viewport.Width = static_cast<float>(shadowWidth);
viewport.Height = static_cast<float>(shadowHeight);
viewport.MinDepth = 0;
viewport.MaxDepth = 1;
```

Main Back Buffer寸法を流用しません。

## 12. Shadow Pass Binding

```cpp
context->PSSetShaderResources(shadowSlot, 1, nullSrv);
context->OMSetRenderTargets(0, nullptr, shadowDsv.Get());
context->RSSetViewports(1, &viewport);
context->ClearDepthStencilView(
    shadowDsv.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);
```

通常Zの例です。

## 13. Color Targetなし

Shadow MapへColorは不要なのでRTV数0でDSVだけBindingできます。

## 14. Shadow Shader

Vertex ShaderはPositionをLight Clip Spaceへ変換します。Opaque Depth-onlyならPixel Shaderを省略できる場合があります。

## 15. Alpha Cutout Caster

葉、髪、柵はPixel ShaderでAlpha TextureをSampleし`clip`して、透明部分がShadowを落とさないようにします。

## 16. Skinning Caster

Animated CharacterはShadow Passでも同じBone PoseでSkinningします。Static Position Shaderを使うとShadowだけBind Poseになります。

## 17. Light View Matrix

Light位置/方向からViewを作ります。Directional Lightは実質位置を持たないため、対象領域を覆う仮想位置とOrthographic Projectionを作ります。

## 18. Directional Projection

Directional Light ShadowはOrthographic Projectionが基本です。Camera周辺の必要領域をLight Space BoxへFitします。

## 19. Spot Projection

Spot LightはPerspective Projectionを使い、FOVを外Cone角、Near/FarをLight Rangeへ合わせます。

## 20. Point Projection

Point Lightは全方向を覆うため、Cube Mapの6 Faceへ90度Perspectiveで描く方式が基本です。

## 21. Light View Projection

```cpp
XMMATRIX lightViewProjection = lightView * lightProjection;
```

Row Vector規約の例です。

## 22. WorldからShadow UV

```text
world -> light clip -> divide by w -> light NDC
-> map x from [-1,1] to [0,1]
-> map y with texture-coordinate convention
```

Depth zはDirect3D NDCの0から1です。

## 23. Bias Matrix

NDCからTexture UVへ変換するMatrixをLight View Projectionへ合成できます。CPU/HLSLの乗算・転置規約へ合わせます。

## 24. Receiver Depth

Camera PassのWorld PositionをLight Clipへ変換し、wで割ったzをShadow Map保存Depthと比較します。

## 25. 範囲外UV

Light Frustum外はShadowなし等のPolicyにします。Border Comparison Samplerを使う場合はBorder値と比較関数を合わせます。

## 26. Comparison Sampler

```cpp
D3D11_SAMPLER_DESC desc{};
desc.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
desc.AddressU = D3D11_TEXTURE_ADDRESS_BORDER;
desc.AddressV = D3D11_TEXTURE_ADDRESS_BORDER;
desc.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
desc.ComparisonFunc = D3D11_COMPARISON_LESS_EQUAL;
desc.BorderColor[0] = 1;
desc.BorderColor[1] = 1;
desc.BorderColor[2] = 1;
desc.BorderColor[3] = 1;
desc.MinLOD = 0;
desc.MaxLOD = 0;
```

## 27. SampleCmp

```hlsl
float visibility = shadowMap.SampleCmpLevelZero(
    shadowSampler,
    shadowUv,
    receiverDepth);
```

通常1がLightに見える、0が遮られる結果です。

## 28. Shadow適用

Shadow Visibilityは主に該当LightのDirect Diffuse/Specularへ掛けます。Emissiveや全Ambientを一律0にしません。

## 29. Hard Shadow

一Texel比較だけでは境界が階段状になります。解像度を上げるだけでなくFilterを使います。

## 30. PCF

Percentage Closer Filteringは周辺のDepth比較結果を平均します。Depth値そのものを平均してから比較する方式とは異なります。

## 31. 手動PCF

```hlsl
float sum = 0;
for (int y = -1; y <= 1; ++y)
for (int x = -1; x <= 1; ++x)
{
    float2 offset = float2(x, y) * shadowTexelSize;
    sum += shadowMap.SampleCmpLevelZero(
        shadowSampler, uv + offset, receiverDepth);
}
return sum / 9.0f;
```

## 32. Texel Size

```text
shadowTexelSize = 1 / float2(width, height)
```

UV OffsetとPixel Offsetを混同しません。

## 33. Kernel Size

大きいPCFはSoftに見えますがSample CostとLight漏れが増えます。距離/Quality別に調整します。

## 34. Poisson/Rotated Kernel

規則的Grid Artifactを減らすSample配置です。NoiseとTemporal安定性を考慮します。

## 35. Shadow Acne

CasterとReceiverのDepth量子化差でSurfaceが自分を遮る縞です。

## 36. Rasterizer Depth Bias

Shadow Passで`DepthBias`、`SlopeScaledDepthBias`、`DepthBiasClamp`を設定します。

## 37. Normal Bias

Receiver/Caster PositionをNormal方向へOffsetする近似です。斜面で有効ですが、接触Shadowが離れる可能性があります。

## 38. Peter Panning

Biasが大きすぎてShadowがObjectから浮くArtifactです。Biasを増やすだけでAcneを解決しません。

## 39. Biasの単位

Constant Depth BiasはWorld距離ではありません。Depth Format、Light Projection、Resolution、Cascadeで最適値が変わります。

## 40. Front-face Cull

Shadow PassでFront FaceをCullし裏面Depthを使う手法があります。薄いGeometry、開いたMesh、Peter Panningへの副作用を確認します。

## 41. Shadow Resolution

大きいほど細部が出ますがMemory、描画、Bandwidthが増えます。Lightが覆うWorld範囲とのTexel密度が重要です。

## 42. Texel密度

同じ2048²でも10mを覆うMapと1kmを覆うMapでは品質が違います。必要範囲へProjectionをFitします。

## 43. Shimmering

Camera移動でLight Projectionが少しずつ動き、Shadow Texel境界が揺れる現象です。

## 44. Stable Projection

Light-space中心をShadow Texel単位へSnapし、微小Camera移動によるProjection移動を抑えます。

## 45. Cascaded Shadow Maps

Directional LightのCamera Frustumを距離で複数分割し、近距離へ高いTexel密度を割り当てます。

## 46. Cascade Split

Linear SplitとLogarithmic Splitを混ぜる実用方式があります。近距離品質と遠距離範囲を調整します。

## 47. Cascade選択

Camera View Depthから該当Cascadeを選びます。Depth Bufferが非線形なら、正しくView Depthへ復元します。

## 48. Cascade Blend

境界周辺で二CascadeをBlendし切替線を抑えます。二重Sample Costがあります。

## 49. Cascade Atlas/Array

複数MapをTexture ArrayまたはAtlasへ保存します。Array Slice View、Atlas Viewport/Scissor、Paddingを設計します。

## 50. Caster Culling

Light Frustum/Cascadeへ影響しないObjectをShadow Passから除外します。Camera外でもShadowをCamera内へ落とすCasterは必要です。

## 51. Receiver BoundsからCaster範囲

Camera可視領域だけでなくLight方向上流のCasterを含めます。単純Camera Frustum CullをShadow Passへ流用しません。

## 52. Static Shadow Cache

動かないLight/GeometryのShadowを再利用し、Dynamic Casterだけ更新・合成する設計があります。

## 53. Update Frequency

全Lightを毎Frame更新せず、重要度、距離、変化、画面影響、Budgetで更新します。

## 54. Point Light Cost

Cube 6 Face描画が必要で高価です。Shadowを落とすPoint Light数、解像度、更新頻度を制限します。

## 55. Shadow Atlas Budget

LightごとにTileを割り当て、PriorityとLifetimeで管理します。移動Lightの再配置によるちらつきを防ぎます。

## 56. Binding Hazard

Shadow Pass前にSRV Slotから外し、Camera Pass前にDSVから外します。Runtime自動Null化Warningへ依存しません。

## 57. Pass復元

Shadow Viewport、Rasterizer Bias State、Depth State、ShaderをMain Pass用へ戻します。論理Passごとに全必要StateをBindingします。

## 58. Debug表示

Shadow Map Depth、Cascade色、Light Frustum、Caster Bounds、Bias、PCF Kernelを可視化します。

## 59. よくある失敗：Camera Depthを比較

Receiver Depthと保存Depthが別空間です。両方を同じLight Clip/NDC規約へそろえます。

## 60. よくある失敗：UVのY反転

NDC-to-texture変換規約が違いShadowが上下反転します。既知QuadとLight Frustum角で検証します。

## 61. よくある失敗：SRV/DSV同時Binding

同じResourceを読み書きしDebug Warningが出ます。Pass境界で明示解除します。

## 62. よくある失敗：Skinned Caster更新忘れ

Character本体は動くのにShadowがBind Pose/前Frameです。Camera Passと同じAnimation Poseを使います。

## 63. よくある失敗：全Sceneを一枚で覆う

Texel密度不足で近距離Shadowが粗くなります。Fit、Cascade、Distance制限を使います。

## 64. Resource Test

- Typeless Resourceへ互換DSV/SRVを作る。
- Format Supportを確認する。
- DSV ClearとSRV読取りを別Passで行う。
- Resize/Device Lost後に再作成する。
- Debug Layer Warningがない。

## 65. Projection Test

- Lightから見たDepthをGray表示する。
- World→Shadow UVの既知点を確認する。
- Directional/Spot/Pointを個別検証する。
- Frustum外Policyを確認する。
- Animated Casterを確認する。

## 66. Quality Test

- Bias 0からAcne/Peter Panningを比較する。
- Hard/PCFを比較する。
- ResolutionとWorld範囲を変える。
- Cascade境界とStabilizationを確認する。
- 30/60/120 FPSでShimmerを見る。

## 67. 完成確認表

- [ ] Shadow Mappingの二Passを説明できる。
- [ ] Typeless Resource、DSV、SRVを作成できる。
- [ ] World PositionをShadow UV/Depthへ変換できる。
- [ ] Comparison SamplerとSampleCmpを使える。
- [ ] PCFと通常Linear Filterを区別できる。
- [ ] Acne、Bias、Peter Panningを調整できる。
- [ ] Directional/Spot/Point Shadowを区別できる。
- [ ] Cascade Split、選択、Blend、Stabilizationを説明できる。
- [ ] Caster CullingとCamera Cullingを区別できる。
- [ ] Shadow更新をBudget化できる。

## 68. この章の要点

- Shadow MapはLight視点で最も近いDepthを保存します。
- Typeless TextureへDepth書込みDSVとShader読取りSRVを作ります。
- Receiverと保存Depthを同じLight空間で比較します。
- Comparison Sampler/PCFで比較結果をFilterします。
- BiasはAcneを減らしますが大きすぎるとShadowが浮きます。
- Directional LightはCascadeとTexel Snapで近距離品質・安定性を改善します。
- Point Shadowは6 Faceで高価なため更新Budgetが重要です。
- SRV/DSV Hazard、State復元、Animated CasterをPass Lifecycleで管理します。

## 69. 公式資料

- [Shadow depth maps](https://learn.microsoft.com/en-us/windows/win32/dxtecharts/common-techniques-to-improve-shadow-depth-maps)
- [D3D11 texture resources](https://learn.microsoft.com/en-us/windows/win32/direct3d11/overviews-direct3d-11-resources-textures)
- [ID3D11Device::CreateDepthStencilView](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11device-createdepthstencilview)
- [ID3D11Device::CreateShaderResourceView](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11device-createshaderresourceview)
- [SampleCmpLevelZero](https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/dx-graphics-hlsl-to-samplecmplevelzero)
- [Depth bias](https://learn.microsoft.com/en-us/windows/win32/direct3d11/d3d10-graphics-programming-guide-output-merger-stage-depth-bias)

次章では、SceneをTextureへ描き、複数PassでBlur、Bloom、Tone Mapping等を行うRender to TextureとPost Processを扱います。
