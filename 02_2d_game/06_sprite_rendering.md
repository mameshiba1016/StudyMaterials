# スプライト描画

スプライトはテクスチャの一部または全部を、画面上の四角形へ描く基本表現です。内部では頂点、UV、インデックス、シェーダー、ブレンド、サンプラー状態を使ってGPUへ描画されます。

## 描画データ

```cpp
struct SpriteDrawCommand
{
    TextureHandle texture{};
    Rect sourcePixels{};      // テクスチャのどの領域を使うか。
    Vector2 worldPosition{};  // 世界座標。
    Vector2 originPixels{};   // 回転・拡縮の基準点。
    Vector2 scale{1.0F, 1.0F};
    float rotationRadians{};
    Color tint{Color::White};
    int layer{};
};
```

ゲームオブジェクトが直接GPU APIを細かく呼ぶより、描画コマンドを集めてRendererが順序・バッチを管理する設計があります。

## テクスチャ座標（UV）

テクスチャ内の位置を0～1へ正規化したUVで指定するAPIが一般的です。

```cpp
float u0{sourceX / textureWidth};
float v0{sourceY / textureHeight};
float u1{(sourceX + sourceWidth) / textureWidth};
float v1{(sourceY + sourceHeight) / textureHeight};
```

整数除算を避け、floatへ変換します。APIごとのV方向、texel中心、半ピクセル規則を確認します。

## Origin・Pivot

画像左上をpositionへ置くのか、中央を置くのかを統一します。

```cpp
Vector2 origin{sourceWidth * 0.5F, sourceHeight * 0.5F};
```

キャラクターは足元中央をPivotにすると地面配置と反転が扱いやすい場合があります。アニメーションフレームごとに画像サイズが違うとPivotが揺れるため、共通キャンバスまたはメタデータを使います。

## 変換順序

一般的には、ローカル頂点からOriginを引き、Scale、Rotate、Translateします。

```text
local -= origin
local *= scale
local = rotate(local)
world = local + position
```

順序を変えると結果が変わります。負のXスケールで左右反転する場合、Originとカリング設定を確認します。

## アルファブレンディング

典型的なstraight alphaは概念的に次です。

```text
output.rgb = source.rgb * source.a + destination.rgb * (1 - source.a)
```

Premultiplied alphaではRGBが事前にalpha倍され、ブレンド式が異なります。画像データ、シェーダー、ブレンド設定を一致させないと黒・白い縁が出ます。

半透明物体は通常、奥から手前へ描く必要があります。完全不透明スプライトは状態変更削減のため別に並べられることがあります。

## 描画順

方法例：

- 明示layer：背景、地形、キャラクター、エフェクト、UI。
- Y座標：見下ろしゲームで下にいる物を手前へ。
- sort key：layer、material、texture、depthをビットへまとめる。

見た目の正しさとバッチ効率が衝突します。透明順序を壊してまでテクスチャ順へ並べません。

## Sprite Batch

一枚ごとにDraw Callを発行するとCPUオーバーヘッドが増えます。同じパイプライン・テクスチャ等をまとめ、頂点を動的バッファへ詰めて一度に描きます。Texture AtlasやTexture Arrayにより切替を減らせます。

ただし、バッチのために巨大アトラスを作ると、ロード単位、メモリ、圧縮、最大サイズ、更新頻度が悪化する場合があります。

## サンプラー

- Nearest：最近傍。ピクセルアートの輪郭を保つ。
- Linear：周囲を補間。拡縮が滑らか。
- Address mode：Clamp、Wrap等。

アトラスでlinear samplingすると隣フレーム色が混ざるtexture bleedingが起きます。余白、edge extrusion、UV調整、mipmap生成を適切に行います。

## 色空間

色テクスチャはsRGBから線形空間へ変換してライティング・ブレンドし、出力時に表示色空間へ戻すのが基本です。法線、マスク、データテクスチャをsRGB扱いすると値が変化します。2Dでも正しい色合成には重要です。

## 解像度対応

内部解像度へ描いてウィンドウへ拡大する方法があります。

- Stretch：画面全体へ引き伸ばし、縦横比が変形。
- Fit/Letterbox：縦横比を保ち余白。
- Fill/Crop：画面を埋め一部を切る。
- Integer scale：整数倍でピクセルを保つ。

マウス画面座標も同じViewport変換を逆に通して内部座標へ戻します。

## テクスチャ寿命

Draw Commandが参照するTextureはGPUがコマンドを実行し終えるまで有効である必要があります。CPU側で削除した直後にGPUがまだ使用中という問題があり、Rendererはフレーム遅延解放やFenceを利用します。高水準ライブラリが隠していても原理を理解します。
