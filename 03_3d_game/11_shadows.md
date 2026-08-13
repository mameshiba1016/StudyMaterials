# Shadow Mapping・精度・Artifact

Shadowは光からSurfaceが見えるかを近似します。一般的なShadow Mapは、Light視点のDepthをTextureへ描き、Main Passで比較します。

## 基本処理

```text
1. Light ViewProjectionでCasterをDepth Mapへ描画
2. World PositionをLight Clip Spaceへ変換
3. Shadow UVと比較Depthを求める
4. 保存Depthより奥なら遮蔽
```

## Depth Bias

同じSurfaceの数値誤差により自分自身をShadowと判定するShadow Acneを避けます。

- Constant Bias。
- Slope-scaled Bias。
- Normal Offset。

大きすぎるとShadowが物体から離れるPeter Panningが出ます。World Unit、Map解像度、Light角度へ合わせます。

## PCF

周辺複数Depthを比較し平均してEdgeを柔らかくします。Kernelが大きいほどSample Costが増えます。Hardware comparison samplerを利用できます。

## Cascaded Shadow Maps

Directional LightのCamera Frustumを距離帯へ分け、近距離へ高解像度を割当てます。

```text
Cascade 0：近距離、高密度
Cascade 1：中距離
Cascade 2：遠距離
```

Split、Cascade間Blend、Texel Snappingによる安定化を管理します。Camera移動でShadowが泳ぐShimmeringを抑えます。

## Point Light Shadow

全方向をCube Map六面へ描くため高コストです。Dual Paraboloid等の代替もあります。Shadowを落とすPoint Light数と更新頻度へ上限を設けます。

## Spot Light Shadow

一つのPerspective Shadow Mapで表せます。Cone角、Near/Far、Atlas配置を管理します。

## Shadow Atlas

複数Light/Cascadeを一枚のTextureへ配置します。Tile境界のFilter Bleedingを防ぐPaddingが必要です。Allocation、Eviction、更新優先度を持ちます。

## Static Shadow

動かないCasterをCache/Bakeし、Dynamic Objectだけ毎Frame更新できます。Light・Objectが変化した時にCacheをInvalidationします。

## Contact Shadow

Shadow Map解像度で消える接地点をScreen-space Ray March等で補います。画面外情報がなく、Thickness/Noise Artifactがあります。

## VSM/EVSM

Depth MomentをFilter可能なTextureへ保存しSoft Shadowを作りますが、Light Bleedingや数値Rangeを処理します。

## Ray-traced Shadow

GeometryへRayを飛ばし正確なVisibilityとArea LightのPenumbraを得られます。Acceleration Structure、Denoise、Sample数、Hardware対応が必要です。

## Gameplayとの分離

見た目のShadowと、Stealth Gameplayの「暗い場所」判定をPixel結果から直接取らない方が安定します。Gameplay用Light/Visibility Modelを別に定義します。

## Debug

Shadow Map、Cascade色分け、Atlas、Bias、Caster数、更新理由、GPU時間を表示します。Artifact名と原因を対応付けて調整します。
