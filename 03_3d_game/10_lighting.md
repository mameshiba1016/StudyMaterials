# Lighting・BRDF・IBL

Lightingは光源、Surface Material、Geometry、VisibilityからPixelのRadianceを評価します。

## 光源

- Directional：太陽のように方向一定、距離減衰なし。
- Point：位置から全方向。
- Spot：Cone内へ照射。
- Area：面積を持つ光。Real-time近似やBakeを利用。

Point Lightの物理的減衰は概ね距離二乗反比例ですが、0付近の発散と有限範囲を実用式で処理します。

## Surface Vector

```text
N：Surface Normal
L：SurfaceからLight方向
V：SurfaceからCamera方向
H：normalize(L + V)
```

すべて同じ座標空間へ揃え、正規化します。

## Diffuse

Lambertは`max(dot(N,L),0)`を使います。PBRではDiffuse BRDFへ`baseColor / π`等を含め、Energy Conservationを考慮します。

## Specular

Microfacet BRDFの代表構成：

```text
Specular = D * F * G / (4 * NdotL * NdotV)
```

- D：Normal Distribution（GGX等）。
- F：Fresnel（Schlick近似等）。
- G：Geometry/Visibility。

分母0、Roughness 0、NaNを防ぎます。

## Metallic Workflow

- Dielectric：Base ColorはDiffuse、F0は低い無彩色付近。
- Metal：Diffuseはほぼなく、Base ColorがSpecular色。

Metallicを中間値にできても、現実素材では主に0/1で、境界混合や汚れ表現に中間を使います。

## Forward Rendering

Objectごとに影響LightをShaderで評価します。透明、MSAA、Material多様性に強い一方、多数Lightで計算が重複します。Forward+ではScreen Tile/ClusterごとにLight Listを作ります。

## Deferred Rendering

Geometry PassでGBufferへNormal、Material、Depth等を書き、Lighting PassでLightを評価します。多数Lightに強い一方、GBuffer Bandwidth、透明、MSAA、Material表現制限があります。

## Tiled・Clustered

ScreenをTile、さらにDepth Sliceを含むClusterへ分け、影響Light ListをGPUで作ります。Light数上限、List overflow、Depth範囲を管理します。

## IBL

Image-Based LightingはEnvironment Mapから間接Diffuse/Specularを近似します。

- Irradiance Map/SH：Diffuse。
- Prefiltered Environment：Roughness別Specular。
- BRDF LUT：積分の一部を参照。

Reflection Probeを空間へ配置し、複数ProbeをBlendします。

## Global Illumination

直接光以外の反射を扱います。

- Lightmap Bake。
- Light Probe/SH。
- Screen-space GI。
- Voxel/Distance Field。
- Ray Tracing。

Static/Dynamic Object、Build時間、Memory、更新頻度の交換条件があります。

## ExposureとTone Mapping

LightingはHDR値を作り、Exposure後にTone MapperでDisplay範囲へ圧縮します。Material色をTone Mapping後の見た目だけで調整すると、Exposure変更で崩れます。

## Light Units

Candela、Lumen、Lux、Nit等を使うPhysical Lightingでは、Light Typeに合う単位とCamera Exposureを揃えます。任意単位でもProject全体の基準値を決めます。

## Debug View

NdotL、Diffuse、Specular、Roughness、Metallic、Light Count、Probe Weight、Overdrawを個別表示します。最終色だけでは原因を分離できません。
