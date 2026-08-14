# DirectX 12 第20章：総合3D戦闘描画

この章では、DirectX 12編の全知識を高速3D戦闘Sceneの一Frameへ統合します。起動、Asset、Animation、Skinning、Culling、Shadow、Lighting、透明Effect、Post Process、UI、Frame Graph、Multiple Queue、Memory、診断、品質縮退まで扱います。

## 1. 到達目標

- Rendererの初期化から終了までの所有関係を説明する。
- Gameplay Snapshotを複数描画Passへ変換する。
- Character、敵、Effect、Stage、UIを正しい順序で描画する。
- CPU/GPU/Memory/Latency Budgetを同時に管理する。
- 負荷急増とDevice Removedに安全に対応する。

## 2. 完成像

```text
platform/window
device/queues/frame contexts
asset streaming
gameplay snapshot
animation/render preparation
frame graph compile
parallel command recording
multi-queue submission
present/telemetry/recovery
```

## 3. Layer分割

```text
Platform        : Win32、Window、Input、Display
RHI/D3D12       : Device、Queue、Resource、Descriptor、PSO
Render Core     : Frame Graph、Allocator、Profiler
Render Features : Shadow、Lighting、Effect、Post、UI
Scene Bridge    : GameplayからRender Snapshot生成
Asset           : Model、Texture、Shader、Streaming
```

上位層がD3D12 Objectの寿命を場当たり的に操作しないようにします。

## 4. Ownership

Device Owner、Queue Owner、Resource Registry、Asset Manager、Frame Context、Scene Snapshotの所有者を一意にします。

## 5. 起動順

```text
enable debug/DRED
create factory/adapter/device
create queues/fences
create window swap chain
create descriptor/heap allocators
create frame contexts
create root signatures/PSOs
create persistent resources
start workers/streaming
```

## 6. 終了順

新規Job/Submissionを止め、Workerを終了し、Queue完了またはDevice Lost経路へ進み、Device Childから逆順に解放します。

## 7. Frame Context

各同時進行FrameにAllocator、Command List Slot、Upload Range、Transient Descriptor、Query、Fence値を持たせます。

## 8. Frame開始

再利用ContextのFence完了を確認し、Allocator/Arena/QueryをResetします。全GPUを毎FrameFlushしません。

## 9. Timing

Simulation Fixed Step、Render Delta、Animation Time、Presentation Timeを区別します。Pause/Slow Motion/Frame Spikeを仕様化します。

## 10. Gameplay Snapshot

```cpp
struct RenderSnapshot
{
    CameraData camera;
    std::span<const CharacterProxy> characters;
    std::span<const EffectProxy> effects;
    std::span<const LightProxy> lights;
    UiSnapshot ui;
};
```

描画中にGameplay Objectを直接読むData Raceを避けます。

## 11. Stable ID

Object、Material、Resource、Passに安定IDを付け、Motion History、Selection、Log、PIX、DREDを接続します。

## 12. Camera

View/Projection、Previous Matrix、Jitter、Near/Far、Viewport、ExposureをFrame Dataへ固定します。

## 13. Camera Cut

Teleport/Scene切替ではTAA、Motion Vector、Occlusion History、ExposureをInvalidationします。

## 14. Render Preparation

SnapshotからVisible候補、Draw Packet、Animation Job、Light/Effect Listを作ります。CPU準備をJob化します。

## 15. Asset Ready確認

Mesh/Texture/PSOのUpload Fenceと状態を確認し、未完了ならFallbackを使います。Render Threadを不用意に待たせません。

## 16. Animation評価

State、Clip Sampling、Blend、Layer、Root Motion表示、Morph、Global Pose、Skin MatrixをJobで計算します。

## 17. Gameplay Poseとの整合

Hit判定のSimulation Poseと表示補間Poseの時間差を把握し、Debug表示で両者を比較可能にします。

## 18. Animation LOD

画面Size、距離、重要度、攻撃対象かで更新頻度、Joint、IK、Morphを調整します。

## 19. Joint Upload

Frame Upload ArenaへPaletteを整列配置し、Character InstanceへGPU Address/Indexを記録します。

## 20. Compute Skinning

必要CharacterをBatch Dispatchし、UAV出力をVertex用途へBarrierします。Pass再利用回数からVS Skinningと選択します。

## 21. Previous Pose

Motion Vector用の前Frame Palette/Skinned Positionを保持します。Spawn/Teleport時はCurrentへResetします。

## 22. CPU粗Culling

Scene Partition/距離で明らかに不要なObjectを除外し、GPUへ送る候補量を制限します。

## 23. Depth Pre-pass

Opaque/Masked CharacterとStageのDepthを作り、重いMain Pixel ShaderのOverdrawを減らすか実測します。

## 24. Hi-Z

Depth MipをComputeで生成し、Reversed-Z/通常Zに対応するReductionを使います。

## 25. GPU Culling

Frustum、Hi-Z、LODを判定し、Visible Instance/Indirect Argumentを生成します。Camera急移動時は保守的にします。

## 26. Shadow

Directional Cascade、重要Local Light、Character ShadowをBudget内で選びます。全Light/EffectへShadowを付けません。

## 27. Shadow Caster分類

Character、Stage、Masked、Effectを必要性で分け、Skinning/Alpha Maskだけを最小Bindingで描きます。

## 28. Shadow Cache

Static Geometryや更新不要LightをCacheできます。Moving Character/Light、LOD、Asset ReloadでInvalidationします。

## 29. G-bufferかForwardか

Deferred、Forward+、HybridをLight数、透明、MSAA、Material、Bandwidthから選びます。本章では特定方式を絶対視しません。

## 30. Opaque Pass

PSO/Material/Depth BucketでSortし、Stage、Prop、Characterを描画します。Depth Write、Cull、SRGB/PBR規約を統一します。

## 31. Character Material

Skin、Hair、Eye、Cloth、Outline等をMaterial Variantへ分類し、無制限なPSO/Shader Permutationを避けます。

## 32. Lighting

Directional/Local Light、IBL、Shadow、Material BRDFをLinear HDR空間で計算します。

## 33. Light Culling

Tile/ClusterへLight IndexをComputeで構築し、Capacity Overflowを可視化・縮退します。

## 34. Emissive

攻撃EffectやSkill表現の発光をHDR値で扱い、BloomとExposureを含め白飛びを調整します。

## 35. Decal

Hit Mark等をDepth/Normalへ投影します。適用Layer、Angle、Lifetime、大量発生時の上限を管理します。

## 36. Outline

Stencil/Geometry/Post-process方式を目的別に選び、Character識別、Selection、Hit強調へ使います。

## 37. Transparent Queue

Smoke、Glass、Trail、ParticleをBlend方式とDepth順へ分類します。OpaqueのState Sort規則をそのまま使いません。

## 38. Particle Simulation

GPU BufferでSpawn/Update/Compactし、Alive CountからIndirect Drawを生成できます。Counter Overflowを検出します。

## 39. Particle Rendering

Alpha/Additive、Soft Particle、Distortion、Lit/Unlitを分け、画面占有率とOverdrawをBudget化します。

## 40. Trail

攻撃軌跡の履歴PointからRibbonを生成します。Sample頻度、最大長、Camera Facing、Teleport Resetを管理します。

## 41. Distortion

Scene ColorをOffset Samplingします。適用順、画面外Clamp、低解像度、重なり上限を定めます。

## 42. Hit Stop中のEffect

Gameplay Time停止時も動くEffect/UIがあるため、Time DomainをSimulation/Real/Animationへ分けます。

## 43. Motion Vector

Camera/Object/SkinningのCurrent/Previous Clip Positionから生成し、TAA/Motion Blurへ使います。

## 44. TAA

Jitter、History Reprojection、Rejection、Clamp、Reactive Maskを管理します。高速EffectやCamera CutのGhostを検証します。

## 45. Motion Blur

演出と可読性を両立し、Player、UI、Camera回転、Hit Stopで強度/除外を調整します。

## 46. Bloom

Threshold/Prefilter、Downsample、Blur/Upsample、合成をTransient Textureで構築します。

## 47. Exposure

Histogram/平均輝度等から適応し、急なSkill Flashで視認性が壊れない速度/Rangeを設定します。

## 48. Tone Mapping

HDR Scene ColorをDisplay出力へ変換します。SDR/HDR Output、UI合成位置、Color Spaceを統一します。

## 49. Color Grading

Scene/状態/Skill演出をLUT等で調整します。複数効果のBlend順と範囲を仕様化します。

## 50. UI

HP、Skill、Damage、Lock-on、TutorialをLayer/Clip順で描き、Alpha規約とHDR/SDR合成を合わせます。

## 51. World UI

敵HP/MarkerをWorldからScreenへ投影し、Behind Camera、Occlusion、Edge Clamp、Distance Scaleを処理します。

## 52. Present

Back BufferをPRESENT Stateへ遷移し、Swap Chain設定に従いPresentします。Tearing/VSync/Frame Latencyを設定別にTestします。

## 53. Frame Graph構築

```text
import back buffer/history/assets
declare skinning/depth/Hi-Z/culling
declare shadow/opaque/lighting
declare transparent/post/UI
export present/history
compile and execute
```

## 54. Resource Access宣言

各PassがSRV/UAV/RTV/DSV/Copy/Indirectを正確に宣言し、隠れたGlobal Accessを禁止します。

## 55. Barrier計画

GraphがTransition、UAV、Alias Barrierを生成し、SubresourceとQueueを追跡します。

## 56. Transient Memory

Depth派生、Lighting、Bloom等の非重複LifetimeをAliasし、Peak Memoryを可視化します。

## 57. Queue配置

UploadはCopy、独立Compute候補はCompute、GraphicsはDirectへ置きます。往復WaitとBandwidth競合をPIXで確認します。

## 58. Command Recording

Pass/ChunkをWorkerへ配り、Allocator/List/Descriptor/Upload Rangeを専有させ、決定済み順で提出します。

## 59. Descriptor設計

Persistent Asset ViewとFrame Transient Viewを分離し、Heap Capacity/Generation/Last-use Fenceを追跡します。

## 60. PSO設計

Pass、Material、Depth/Blend/Rasterizer、Format、Sample、ShaderをKey化し、事前生成/Cacheします。

## 61. Root Signature

Frame/View、Material、Object、Texture/Buffer Tableを更新頻度で分けます。Root CostとBinding数を測ります。

## 62. Upload Budget

Constant、Joint、Instance、Texture/Mesh StreamingのByte/Allocation数を別々に監視します。

## 63. GPU Memory Budget

Persistent Asset、History、Shadow、Transient、Upload/Readbackを分類し、Budget超過前にMip/品質を縮退します。

## 64. CPU Budget

Simulation、Animation、Culling/Packet、Graph Compile、Recording、Submission、StreamingをThread Timelineで測ります。

## 65. GPU Budget

Shadow、Skinning、Depth、Lighting、Transparent、Post、UIをTimestamp/PIXで測り、Critical Pathを確認します。

## 66. Latency Budget

Input SampleからSimulation、Render Snapshot、GPU、Presentまでを測ります。平均FPSだけで操作感を判断しません。

## 67. Frame Spike

Percentile/最大値と原因Eventを記録します。Asset到着、PSO作成、Heap拡張、Effect Burst、Fence Waitを分離します。

## 68. Quality Ladder

```text
shadow resolution/count
particle count/resolution
animation/skin update rate
post-process quality
render scale
texture mip residency
```

優先順位とHysteresisを持たせます。

## 69. Gameplay優先度

Player、攻撃対象、Boss Telegraph、危険Effectは遠距離群衆より描画/更新優先度を高くします。

## 70. Overdraw対策

透明Effectの形状、解像度、Spawn数、Lifetime、Cull、Blendを調整し、Overdraw Heatmapで確認します。

## 71. Draw/Dispatch Budget

Pass別Count、Triangle、Instance、Skinned Vertex、Particle、Light Indexを常時計測します。

## 72. Scalability

最低/推奨GPUごとにResolution、Feature、Memory、Frame目標を定義し、自動検出とUser設定を両立します。

## 73. Debug View

Depth、Normal、Roughness、Shadow、Motion、Overdraw、LOD、Culling、Light Tile、Stencil、Object IDを表示します。

## 74. Runtime HUD

CPU/GPU Frame、Queue、Memory Budget、Draw、Particle、Descriptor、Upload、Streaming、Fence待ちを表示します。

## 75. PIX Marker

Frame/Pass/Character Batch/Effect BatchをStable ID付きでMarker化し、Graph可視化、DRED、CPU Logと一致させます。

## 76. Device Removed

最初の失敗でSubmissionを停止し、DRED、Queue Fence、Resource/Descriptor履歴、Memoryを保存します。

## 77. Recovery

全Device Childを破棄/再作成し、CPU Assetから再Uploadします。復旧不能なら診断を残して安全終了します。

## 78. Resize/Display変更

関連Queue完了後、Back Buffer、Depth、History、相対Size Resource、Viewportを再構築しHistoryを無効化します。

## 79. Hot Reload

Shader/PSO/Assetの新VersionがGPU Ready後にFrame境界で切替わり、旧VersionをFence遅延破棄します。

## 80. Deterministic Replay

Input、Seed、Fixed Tick、Asset Hash、Render Toggle、Cameraを保存し、問題FrameのCommand構造を再現します。

## 81. Unit Test

Math、Allocator、Descriptor、State、Graph、Animation、Sort、LOD、Budget PolicyをCPUでTestします。

## 82. Render Test

固定SceneでShadow、Skinning、Material、透明、Post、UIをGolden Imageと許容差比較します。

## 83. Stress Test

大量Character/Effect/Light、Camera移動、Streaming、Resize、Async、Low Budget、長時間を組み合わせます。

## 84. Failure Injection

OOM、Descriptor不足、Upload遅延、Shader失敗、Queue遅延、Device Removed経路を疑似発生させます。

## 85. 一Frameの擬似Code

```cpp
void Renderer::Render(const RenderSnapshot& snapshot)
{
    FrameContext& frame = AcquireCompletedFrame();
    PrepareViewAndHistory(snapshot, frame);
    BuildAnimationAndDrawPackets(snapshot, frame);

    FrameGraph graph;
    ImportPersistentResources(graph, frame);
    AddSkinningAndCullingPasses(graph, snapshot);
    AddShadowAndOpaquePasses(graph, snapshot);
    AddLightingAndEffectPasses(graph, snapshot);
    AddPostProcessAndUiPasses(graph, snapshot);

    CompiledGraph plan = graph.Compile();
    RecordPlanInParallel(plan, frame);
    SubmitQueues(plan, frame);
    PresentAndSignal(frame);
    CollectTelemetry(frame);
}
```

各関数内部のResource Access/Ownershipを明文化します。

## 86. 実装順序

1. Window、Device、Triangle、Fenceを正しくする。
2. Resource/Descriptor/PSO/Upload基盤を作る。
3. Static Model、Depth、Camera、Lightingを作る。
4. Animation/Skinning、Shadow、Materialを加える。
5. Effect/Post/UIを加える。
6. Frame Graph/並列記録/Multiple Queueへ発展させる。
7. Budget、診断、Test、縮退を完成させる。

## 87. 実装Checklist

- [ ] LayerとOwnershipを一意にする。
- [ ] Gameplayから不変Snapshotを渡す。
- [ ] Animation/Asset/HistoryのFrame Lifetimeを守る。
- [ ] Pass AccessをFrame Graphへ全宣言する。
- [ ] Queue、Barrier、Transient Aliasを検証する。
- [ ] Character/Effect/Lightへ明確なBudgetを設定する。
- [ ] CPU/GPU/Memory/Latency/Spikeを同時に測る。
- [ ] Quality LadderとFallbackを実装する。
- [ ] PIX/DRED/Replay/Testで再現可能にする。

## 88. 理解確認問題

1. Renderer LayerとOwnershipを設計してください。
2. Gameplay Snapshotが必要な理由を説明してください。
3. CharacterをAnimationからMain Passまで運ぶ流れを説明してください。
4. GPU CullingとIndirect Drawの依存を説明してください。
5. 透明Effectの品質と性能問題を挙げてください。
6. Frame Graphへ登録する主要Passを順に挙げてください。
7. CPU/GPU/Memory/Latency Budgetを分類してください。
8. 戦闘中の負荷Spikeに対する縮退を提案してください。
9. Device Removed時に保存する情報を挙げてください。
10. 最小機能から統合Rendererへ進む順序を説明してください。

## 89. 要点

- 高速3D戦闘Rendererは一つのShaderでなく、多数Systemの時間・Resource契約です。
- Gameplay Snapshot、Stable ID、Frame ContextでFrame境界を明確にします。
- Animation、Culling、Shadow、Lighting、Effect、Post、UIをGraphのPassへ分解します。
- Barrier、Queue、Descriptor、Transient Memory、Fence Lifetimeを中央管理します。
- Character/Effectの可読性を保ちながらOverdraw、Skinning、Light、MemoryをBudget化します。
- 平均FPSだけでなくCritical Path、Latency、Percentile Spikeを測ります。
- 診断、Replay、Failure Injection、品質縮退まで含めてRendererを完成させます。

## 90. DirectX 12編の総復習

```text
explicit API
 -> device/queue/fence/frame resource
 -> swap chain/descriptor/root signature/PSO
 -> resource/upload/barrier/texture/depth/blend
 -> model/material/animation
 -> compute/UAV/indirect
 -> multithread/multiple queue
 -> memory/transient/residency
 -> DRED/PIX
 -> frame graph
 -> integrated action rendering
```

API名の暗記ではなく、誰がDataを所有し、いつGPUが読み、どの依存とBudgetで一Frameを完成させるか説明できることが重要です。

## 91. 次の段階

DirectX 11/12知識ノートは完了です。全分野の知識ノート完了後は、計画済みの実行可能な実習編で`.h`、`.cpp`、Shader、Project、Build/Run/Testを一項目ずつ作成します。
