# 2Dゲームの最適化

最適化はFPSを闇雲に上げる作業ではなく、Frame Time、Memory、Load、入力遅延、電力の予算を満たす作業です。必ずRelease相当Buildと対象Hardwareで計測します。

## Frame Budget

60 FPSは一フレーム約16.67 ms、120 FPSは約8.33 msです。ただしCPUとGPUは並行するため、単純な足し算だけではありません。Present待ちを処理時間と誤認しないようTimelineを見ます。

## CPU計測

- Update各System。
- Collision Broad/Narrow。
- Animation。
- UI Layout。
- Draw Command生成。
- Asset Streaming。
- Allocation数。

平均だけでなく最大、P95/P99、Stutter発生フレームを保存します。

## GPU計測

- Draw Call。
- Sprite/Vertex数。
- Texture/Render Target切替。
- Overdraw。
- Blend・Shaderコスト。
- Render Target解像度。

2Dは透明Spriteの重なりでFill Rateが支配する場合があります。画面全体の半透明Layerを何枚も重ねないよう確認します。

## Batch

同じMaterial、Blend、Texture AtlasのSpriteをまとめます。ただし透明描画順を壊さず、巨大AtlasによるMemory増大との交換条件を測ります。

## Culling

Camera外SpriteをDrawしません。Particle、Tile、UI Clipも対象です。ただしUpdate停止はゲームルール変更なので別のLOD方針として設計します。

## Collision

- Layer Maskで不要Pairを除外。
- Uniform Grid等のBroad Phase。
- Static Colliderを事前構築。
- Query結果Bufferを再利用。
- Debug時にPair数を表示。

空間分割は要素分布次第で逆効果にもなるため計測します。

## Allocation

毎フレームの一時`vector`、文字列連結、Particleの個別`new`を調べます。

- `reserve`。
- Frame Allocator。
- Object Pool。
- Small Buffer。
- Command Buffer再利用。

Poolは使用中/未使用、世代、二重返却、最大数を管理する複雑さがあります。必要なHot Pathへ限定します。

## Data Layout

大量Entityを連続走査するなら、巨大な多態Object配列より必要Componentの密な配列がCacheに有利な場合があります。Positionだけ更新するのにTexture名やScript状態までCacheへ載せない設計です。

## Asset

- Texture圧縮とサイズ。
- Atlas単位。
- Mipmapの要否。
- Audio Streaming。
- SceneごとのPreload。
- Async I/OとMain Thread反映。

使用していないAssetをPackageへ含めない一方、初回使用時ロードでStutterを起こさないようPrewarmします。

## UI

毎フレーム変わらないTextやLayoutを再構築しません。Dirty Flag、Glyph Cache、Batch、Clipを使います。Debug UI自体の負荷も測ります。

## ログ

Hot Loopで同期File Logすると著しく遅くなります。Level/Category、非同期Buffer、Rate Limitを使います。Releaseで全ログを消すだけでなく、クラッシュ調査に必要な低頻度情報は残します。

## マルチスレッド

まず処理依存を整理し、Animation、Particle、AI Query等をJob化できます。小さすぎるJob、False Sharing、Main Threadへの即時Waitは逆効果です。Renderer・Audio・Engine ObjectのThread制約を守ります。

## Stutter

平均FPSが高くても次で停止します。

- 初回Shader/Pipeline生成。
- 初回Texture/Audio Decode。
- 動的AllocationとGC。
- 同期File I/O。
- OS Security Scan。
- Huge Log flush。

Frame CaptureとMarkerで最初に長い区間を特定します。

## 最適化記録

```text
条件 / Hardware / Build / Scene
変更前Metric
仮説
変更内容
変更後Metric
画質・Memory等の副作用
```

最適化記録では改善率だけでなく測定方法と判断理由を示します。
