# Texture・Sampling・Mipmap・Compression

Textureは画像だけでなく、Normal、Material値、Depth、Lookup TableなどをGPUで参照する多次元データです。

## TexelとUV

Texture要素をTexelと呼びます。UVは通常0～1の正規化座標ですが、原点とV方向はAsset・APIで異なります。

```text
u = pixelX / textureWidth
v = pixelY / textureHeight
```

Texel中心、境界、Atlas余白を考慮します。

## Filter

- Point/Nearest：最も近いTexel。
- Bilinear：同一Mipmapの周囲4 Texelを補間。
- Trilinear：二つのMipmap間も補間。
- Anisotropic：斜め面で複数Sampleし鮮明さを改善。

Filter品質はSample数とBandwidthを増やします。

## Address Mode

- Clamp：端を延長。
- Wrap：周期的に繰り返す。
- Mirror：反転しながら繰り返す。
- Border：範囲外へ指定色。

Shadow MapやScreen-space Textureでは範囲外Sampleの扱いが重要です。

## Mipmap

元画像を段階的に縮小したLevel群です。遠い面へ高解像度Textureを直接Sampleすると、複数Texelが一Pixelへ潰れAlias・ちらつきになります。GPUはUV微分から適切なLODを選びます。

```text
Level 0: 1024×1024
Level 1:  512×512
...
```

Mipmapは元画像に対し概ね約1/3追加Memoryを使います。UIやPixel Artで不要な場合もあります。

## Gamma-correct Mipmap

Base Colorの縮小はLinear色空間で平均し、Normal MapはVectorとして再正規化します。単純なbyte平均では暗さやNormal長が壊れます。Maskは用途により最大値保持など別Filterが必要です。

## Texture Compression

BC、ASTC、ETC等のBlock CompressionはGPUが圧縮状態でSampleでき、MemoryとBandwidthを削減します。

- 色・Alpha・Normalで適切なFormatが違う。
- 4×4等のBlock単位なので細いMaskにArtifactが出る。
- Platformごとの対応Formatが違う。
- Disk圧縮PNG/JPEGとGPU Texture圧縮は別。

## HDR Format

LightingやBloom用Render Targetは1を超える値を保持するFloat Formatを使います。RGBA16F等は精度とMemoryの交換条件があります。すべてを32-bit Floatにしません。

## Texture Array・Cube Map・3D Texture

- Array：同一Size/FormatのLayer群。Material variation、Shadow等。
- Cube：六面から方向をSample。Sky/Reflection。
- 3D：Volume Fog、Noise、LUT。

Atlasと異なりLayer境界のBleedingを避けられますが、対応とBinding規則を確認します。

## Streaming

Camera距離に応じ必要MipmapだけGPUへ常駐させます。

```text
Requested LOD → Async I/O → Decode/Transcode → GPU Upload → Residency更新
```

急回転時のぼけ、Memory Budget、優先度、キャンセルを管理します。

## SamplerとTextureの分離

同じTextureをWrap/Clamp、Linear/Pointで異なるSamplerから参照できます。APIによってSampler ObjectやDescriptorの扱いが違います。

## Render Texture

Camera出力、Shadow、Post ProcessをTextureへ描きます。Size変更時の再確保、MSAA Resolve、Resource State、Frame間Lifetimeを管理します。Render Graphで一時Textureを再利用できます。

## Import検査

- sRGBかDataか。
- Alpha用途。
- Normal Map Convention。
- Mipmap有無。
- Compression Format。
- 最大解像度。
- Streaming可否。
- Address ModeによるBorder。

誤設定はShaderコードより大きく見た目とMemoryへ影響します。
