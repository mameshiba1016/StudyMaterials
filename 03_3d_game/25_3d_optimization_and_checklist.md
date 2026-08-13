# 3D最適化・初稿完了チェック

3D性能はCPU、GPU、Memory、I/O、Shader Compilation、Animation、Physicsを個別に測ります。

## CPU

- Game/Render Thread時間。
- Draw Command生成。
- Animation評価・IK・Skinning準備。
- Physics Pair/Solver。
- AI/Spatial Query。
- AllocationとLock競合。

Job化前に依存関係と粒度を確認します。

## GPU

- Pass別GPU時間。
- Triangle/Vertex数。
- Draw/Dispatch数。
- Overdraw。
- Texture Bandwidth。
- Render Target解像度。
- Shadow Light/Cascade数。
- Shader occupancy/branch。

Frame Captureで最も長いPassから調べます。

## Memory

- Texture Mipmap/Streaming。
- Mesh/Index。
- Animation Clip/Pose Buffer。
- Render Target。
- Physics Shape。
- Duplicate Asset。

予約量、常駐量、Peak、Fragmentationを分けます。

## Stutter

- Shader/Pipeline初回生成。
- Asset Decode/Upload。
- Sync I/O。
- GPU Fence待ち。
- 大量Entity Spawn。
- Memory確保。

Precompile、Prewarm、Async Streaming、Poolを使い、Markerで証明します。

## Dynamic Resolution/LOD

GPU負荷へ応じ内部解像度を調整します。TAA/Upscaler履歴、UI解像度、Screen Queryを更新します。LODはHysteresisを持ち、Animation/AIにも段階を設けます。

## 3D基礎チェック

- [ ] 座標系、単位、行列Conventionが文書化される。
- [ ] Transform階層とNormal変換が正しい。
- [ ] Camera Projection・Depth精度を理解する。
- [ ] Mesh/Tangent/色空間Importを検証する。
- [ ] Lighting、Shadow、Post Processを個別Debug可能。
- [ ] Skeleton、Skinning、Blend、Root Motionを説明できる。
- [ ] IKとGameplay判定の境界が明確。
- [ ] Collision QueryとPhysics Step順が明確。

## 高速戦闘チェック

- [ ] Character移動がCamera-relativeで壁・Slope・Stepへ対応。
- [ ] Camera CollisionとTarget Lock Scoreを可視化できる。
- [ ] Attack DefinitionとInstanceを分離する。
- [ ] Startup/Active/Recovery、Combo、Cancelを固定tick管理する。
- [ ] 高速WeaponをSwept Queryで検出する。
- [ ] Dodge、Parry、Armor、Reactionの解決順がある。
- [ ] Root Motion、Tracking、Collisionを一つの移動Pipelineへ通す。
- [ ] 交代・支援を安全なState Transitionとして扱う。
- [ ] VFX/Audio/Cameraは意味Eventから駆動する。

## 品質

- [ ] Debug View、Frame Capture、Profiler Markerがある。
- [ ] Release相当Buildで計測する。
- [ ] Camera Cut/Teleport/Scene遷移で履歴をResetする。
- [ ] 長時間実行でMemoryとHandle寿命を検証する。
- [ ] 揺れ、点滅、Blur、振動を軽減設定できる。

このチェック完了後、同じ原理をUnreal EngineとUnityの具体APIへ接続します。
