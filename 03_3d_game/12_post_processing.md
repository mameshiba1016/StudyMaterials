# Post Processing・HDR・Temporal処理

Post ProcessingはSceneをRender Targetへ描いた後、画面全体または履歴Bufferを使って画像を加工します。

## 典型的な順序

```text
HDR Scene Color
→ Exposure
→ Bloom等HDR Effect
→ Tone Mapping
→ Color Grading
→ Anti-Aliasing/Upscale（方式による）
→ UI（HDR UIの方式による）
→ Display Output
```

順序を変えると結果が変わります。

## Auto Exposure

Luminance Histogram/平均から目標Exposureを求め、時間でAdaptします。明るいEffect一個で画面全体が暗くならないようPercentileやMetering Maskを使います。

Gameplay中の急変を避け、Cut時はResetします。

## Tone Mapping

HDR RadianceをDisplay Rangeへ圧縮します。ACES近似等を使います。白飛び、彩度変化、Negative値を処理し、UI色の適用位置を決めます。

## Bloom

明るい領域を抽出し、Downsample/Blur/Upsampleして加算します。単純な白画像の重ね合わせではなくHDR値を基準にします。低解像度Pyramidを使って効率化します。

## Color Grading

Exposure、Contrast、White Balance、Saturation、LUTを使います。Linear/Log/Displayのどの空間で処理するかを固定します。Sceneや状態間のVolume Blendも行います。

## TAA

Projection Jitterを変えながら過去FrameをMotion Vectorで現在へReprojectし、Aliasを減らします。

必要データ：

- History Color。
- Current/Previous ViewProjection。
- Object Motion Vector。
- Depth。
- Reactive/Disocclusion情報。

Camera Cut、Teleport、解像度変更でHistoryをResetします。誤ReprojectionはGhostingを起こします。

## Motion Blur

Motion VectorとShutter設定からPixelを軌跡方向へSampleします。Camera/Object両方のMotionが必要です。競技性や酔いへの配慮で無効化・強度設定を用意します。

## Depth of Field

DepthからCoC（Circle of Confusion）を求め、前景・背景をBlurします。透明、UI、Hair、Particleとの順序が難しく、Gameplay可読性を優先します。

## SSAO/SSR

- SSAO：Depth/Normal周辺から近接遮蔽を近似。
- SSR：Screen内のDepthをRay Marchして反射。

画面外・遮蔽物裏の情報がなく、Noise、Halo、欠落があります。Temporal FilterとProbe/Fallbackを組み合わせます。

## Upscaling

低い内部解像度から高解像度出力を再構築します。SpatialとTemporal方式があり、Jitter、Motion Vector、Exposure、Reactive Maskが必要な方式があります。Dynamic Resolutionと連携します。

## Post Process Volume

空間VolumeごとにParameterを持ち、Camera位置とPriority/WeightでBlendします。洞窟、Underwater、Damage演出を表せます。無制限なVolume評価を空間分割します。

## Ping-Pong Resource

Effect Aの出力をBが読む際、同じTextureを同時Read/Writeできないため二つのRender Targetを交互に使うことがあります。Render GraphがLifetimeとAliasingを管理できます。

## Debug・性能

Effectを一つずつ無効化しGPU時間と画質差を測ります。Full-screen Passは解像度に比例し、4Kで負荷が大きくなります。Half/Quarter Resolution、Compute、Async Computeを検討します。

点滅、Blur、色収差、Vignetteは設定で軽減可能にします。
