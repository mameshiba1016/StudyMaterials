# DirectX 11：総合3D戦闘描画

この章では、DirectX 11編で学んだ機能を高速3Dアクションの一Frameへ統合します。操作応答、Character、敵群、Lighting、Shadow、Effect、Trail、Hit演出、Post Process、UI、Frame Graph、性能Budgetまでを総合的に扱います。

## 1. 総合目標

```text
正しいGameplay State
    ↓
読みやすく反応の良い戦闘表現
    ↓
大量の敵・Effectでも安定したFrame Time
    ↓
Debug、拡張、復旧が可能なRenderer
```

派手さだけでなく、操作と攻撃の因果が視覚的に理解できることを重視します。

## 2. 描画とGameplayを分離する

Hit判定、Damage、無敵時間、AI、Animation Stateの正解はGameplay側が持ちます。Rendererは確定した状態とEventを可視化します。

## 3. 一Frameの全体像

```text
Input Sample
 -> Fixed Gameplay Update
 -> Animation / Root Motion
 -> Collision / Hit Resolution
 -> Render Snapshot
 -> Culling / Sort / Graph Build
 -> Shadow / Main / Effect / Post / UI
 -> Present
```

順序とDataのOwnerを明確にします。

## 4. Fixed UpdateとRender

Gameplayは固定刻み、描画はDisplay更新に合わせる設計が一般的です。Render Transformは必要に応じて前後Simulation Stateを補間します。

## 5. 入力Latency

Input取得から表示までのQueueを増やしすぎません。Frame Pipelining、VSync、Buffer Count、Simulation/Render分離がLatencyへ与える影響を測ります。

## 6. Render Snapshot

```cpp
struct CombatRenderSnapshot
{
    uint64_t frameId;
    CameraSnapshot camera;
    std::span<const CharacterRenderProxy> characters;
    std::span<const EffectRenderProxy> effects;
    std::span<const LightRenderProxy> lights;
    std::span<const CombatVisualEvent> events;
};
```

Frame中は不変としてWorkerへ共有します。

## 7. Combat Visual Event

```cpp
enum class CombatVisualEventType
{
    AttackStarted,
    HitConfirmed,
    Guarded,
    Dodged,
    EnemyDefeated
};
```

持続状態と瞬間Eventを分けます。

## 8. Event ID

Eventへ一意IDを付け、同じHitを複数Frameで重複再生しません。ReplayやRollback時の扱いも決めます。

## 9. Character Render Proxy

```cpp
struct CharacterRenderProxy
{
    EntityId entity;
    Matrix world;
    Matrix previousWorld;
    MeshHandle mesh;
    SkeletonPoseHandle pose;
    MaterialSetHandle materials;
    Bounds bounds;
    uint32_t visualState;
};
```

Gameplay Object Pointerを直接渡しません。

## 10. Camera Snapshot

View/Projection、前Frame行列、Jitter、Exposure、Focus Target、Camera Cut Flagを含めます。Temporal処理の履歴Reset条件を渡します。

## 11. Cameraの責務

Target追従、障害物回避、Lock-on構図、Shake、FOV演出を段階として合成します。最終Transformだけでなく各寄与をDebug表示します。

## 12. Camera Shake

Gameplay Cameraの基準TransformとVisual Shakeを分離します。Hit Stop中の時間Scale、優先度、減衰、複数Impulse合成を定義します。

## 13. FOV演出

Dashや必殺動作でFOVを変える場合、急激な変化、酔い、Camera Cut、Temporal履歴への影響を確認します。

## 14. Animation Pose

Animation SystemがSkeleton Poseを確定し、RendererはSkinning Paletteを生成します。Animation評価と描画記録のJob依存を明示します。

## 15. CPU SkinningとGPU Skinning

```text
CPU Skinning : 実装・Debugが単純、CPU/Upload Costが増える
GPU Skinning : 多数Characterに有利、Vertex Shader/Buffer設計が必要
```

Character数、頂点数、Pose再利用を測って選びます。

## 16. Bone Matrix Buffer

CharacterごとのPalette Offsetを持ち、一つの大きなBufferへ詰める設計があります。最大Bone数、Alignment、Frame Ringを管理します。

## 17. Previous Bone Pose

Skinned Motion Vectorが必要なら前Frame Bone Matrixも保持します。Teleport、Spawn、LOD切替時は履歴を現在値へResetします。

## 18. Root Motion

Gameplay TransformとAnimation Rootの責務を決め、二重移動を防ぎます。描画は確定済みWorld TransformとPoseを受け取ります。

## 19. Weapon Attachment

Weapon World MatrixはCharacter WorldとSocket/Bone Matrixから計算します。TrailやHit Effectも同じ確定Socket Transformを参照します。

## 20. Model LOD

距離だけでなく画面占有率、Character重要度、Camera Focus、Boss/Player優先度を使えます。切替Hysteresisで振動を防ぎます。

## 21. Animation LOD

遠距離CharacterはPose更新頻度、Bone数、Facial Animation、Secondary Motionを下げられます。Hit対象や画面中央の敵は品質を保ちます。

## 22. Material分類

Opaque、Masked、Hair、Skin、Eye、Transparent、Effect等へ分類し、必要なShader/State/Passを明確にします。

## 23. PBR Material

Base Color、Normal、Metallic、Roughness、AO、Emissive等を一貫したColor Spaceで扱います。Art Parameterの範囲とDefault値を定義します。

## 24. Character Lighting

物理的なLightingだけで顔やSilhouetteが読みにくい場合、Key Light補助、Rim、Shadow調整等をArt Directionとして制御します。

## 25. Environment Lighting

Directional Light、Sky/IBL、Local Light、Emissiveを組み合わせます。Characterと背景の明度差を戦闘可読性の観点で確認します。

## 26. Light分類

重要なKey Light、Gameplay Light、EffectだけのVisual Lightを区別します。全Particleへ実Lightを生成しません。

## 27. Light Budget

画面内Light数、Shadow付きLight数、Pixel当たり評価数へBudgetを設けます。距離、強度、画面寄与、Priorityで選別します。

## 28. Shadow構成

Main Directional LightはCascade Shadow、重要Characterには品質補助、局所Lightは選択的Shadowを使う構成があります。

## 29. Shadow Caster Culling

Main Camera外でも画面内へ影を落とすObjectを含めます。Cascade/ViewごとのCaster Listを作ります。

## 30. Shadow安定性

Texel Snapping、Cascade Split、Bias、Normal Offsetを調整し、Camera移動時のShimmerとPeter Panningを比較します。

## 31. Contact Shadow

足元や接触部の不足を補えますが、Screen-space方式の画面外欠損やNoiseを理解します。Gameplay Collisionの代用にはしません。

## 32. Scene Color Format

LightingとEmissiveを保持できるHDR Formatへ描画し、最後にTone Mapします。BandwidthとPrecisionのTrade-offを測ります。

## 33. Depth Prepass

大量のOpaque、重いPixel Shader、OverdrawがあるSceneでは有効な場合があります。Geometry二重処理Costと比較します。

## 34. ForwardとDeferred

```text
Forward  : 透明・MSAAと相性が良い、Light評価管理が必要
Deferred : 多Lightに強い、G-Buffer/Bandwidth/透明処理が課題
```

Forward+やTiled/Clustered方式も選択肢です。要件から決めます。

## 35. Main Opaque Pass

Depth、Lighting、Material、Shadowを使ってScene Colorへ描画します。Opaque QueueはPipeline/Material/Meshを考慮してSortします。

## 36. Alpha Test

髪、草、布等のCutoutはEarly-Z、MSAA、Temporal安定性、Mip Alpha Coverageへ注意します。

## 37. Transparent Pass

奥から手前のSort、Blend State、Depth Test、Depth Write Policyを決めます。Particle全体の厳密SortはCostが高いため近似も検討します。

## 38. Hair Rendering

Hair CardのAlpha、Normal、Specular、Backface、Shadow、Sortは専用調整が必要です。Silhouetteと顔周辺を優先します。

## 39. Outline

Geometry拡張、Post Process Edge、Stencil Mask等の方式があります。距離で太さが変わらない設計、内側Edge、Transparentとの順序を考えます。

## 40. Effect System

Effect AssetはEmitter、Module、Material、Texture、Lifetime、Spawn RuleをData化します。Gameplay Codeへ個別Particle処理を書き散らしません。

## 41. CPU Particle

少数の重要Effect、Collision、複雑なEvent連携に適します。Object PoolとSoAで更新Costを抑えます。

## 42. GPU Particle

大量の独立Particle更新に向きます。Compute Shader、Append/Counter、Indirect Drawを使い、CPU Readbackを避けます。

## 43. Particle Spawn

Gameplay EventをSpawn Commandへ変換し、Frame内のCapacityを超えた場合のDrop Policyを定義します。

```cpp
struct ParticleSpawnCommand
{
    EffectId effect;
    Matrix transform;
    uint32_t seed;
    float intensity;
};
```

## 44. Deterministic Effect Seed

Event IDからRandom Seedを作るとReplayや比較Captureで同じ見た目を再現しやすくなります。

## 45. Particle Capacity

最大Particle数、Emitter数、Spawn/Frame、Vertex数、OverdrawへBudgetを設定します。溢れたらPriorityの低いEffectから削減します。

## 46. Effect Priority

Player Hit、Enemy Attack Warning、Boss Mechanic等の情報性が高いEffectを優先し、背景Decorative Effectを先に落とします。

## 47. Trail

Weapon/Character Socketの時系列PointからRibbon Geometryを作ります。PositionだけでなくWidth、Color、Age、Orientationを保存します。

## 48. Trail Sampling

Frameごと固定追加では速度やFPSで密度が変わります。距離・角度・時間Thresholdを組み合わせます。

## 49. Trail UV

累積距離をU座標へ使うと伸縮が安定します。寿命によるFadeと先端/末端の形状をShaderで制御します。

## 50. TrailとHit Stop

Hit Stop中にSamplingを止めるか、Visual Timeだけ進めるかを演出仕様として決めます。Game TimeとReal Timeを区別します。

## 51. Decal

Hit跡、地面Effect、範囲表示に使えます。Projection Volume、Depth Reconstruction、Normal Angle、Lifetime、重なり数を管理します。

## 52. Distortion

専用Vector/Mask Targetへ描画し、Scene Colorを後段で歪めます。UIまで歪めないPass順序と画面端Samplingを確認します。

## 53. Dissolve

Noise Threshold、Edge Emissive、Shadow、Depth、Motion Vectorの整合を取ります。Maskだけ消してShadowが残らないようにします。

## 54. Hit Flash

Material Override、Emissive、Color Grade Mask等で実装できます。複数Hit、無敵表現、Element Colorの優先順位を定義します。

## 55. Hit Stop

Simulation停止とRenderer停止を同一にしません。Camera Shake、Particle、UI、Post Effectのどれが進むかTime Domainごとに決めます。

## 56. Time Domain

```text
Game Time   : Combat SimulationとAnimation
Visual Time : 一部Effect、Camera、Post Process
Real Time   : UI、Network表示、Profiler
```

各Systemがどの`deltaTime`を使うか明示します。

## 57. Hit Emphasisの合成

Flash、Shake、Freeze、FOV、Radial Blur、Chromatic Shift、SoundとのTimingを一つのEventから制御します。全部を最大にしません。

## 58. Attack Warning

危険範囲、敵の予備動作、色、Sound、Camera Visibilityを組み合わせます。Decorative Effectより優先して描画します。

## 59. Lock-on表示

World PositionをClip/NDC/Screenへ変換し、画面外・背面・Occlusion時のRuleを決めます。UI ScaleとDPIを考慮します。

## 60. Damage Number

World-space AnchorとScreen-space Layoutを分離し、重なり回避、Lifetime、Critical表示、Object Poolを管理します。

## 61. Post Process順序

```text
HDR Scene
 -> Distortion/Selected Effects
 -> Bloom
 -> Exposure/Tone Map
 -> Color Grade
 -> Anti-alias/Upscale（方式依存）
 -> UI
 -> Present
```

各方式のColor Spaceと履歴要件で順序を調整します。

## 62. Bloom

高輝度情報を抽出し、Downsample/Blur/Upsampleします。常時白く霞ませず、攻撃のEnergy表現と可読性を両立します。

## 63. Exposure

自動露出の順応速度と範囲を制限し、暗所から明るい攻撃で敵が見えなくならないようにします。Camera Cutでは履歴をResetします。

## 64. Tone Mapping

HDR LightingをDisplay範囲へ変換します。Color Grade、UI合成、Screenshot、HDR Outputとの関係を統一します。

## 65. Color Grading

Scene/状態別LUTやParameterをBlendできます。Gameplay情報色を潰さないよう、Damage WarningやElement Colorを確認します。

## 66. Motion Blur

CameraとObject Motion Vectorを使います。高速攻撃の勢いを出せますが、操作中の視認性、UI、Camera Cut、Transparentに注意します。

## 67. Radial Blur

必殺技やImpactへ短時間だけ使い、中心位置をWorld EventからScreenへ投影します。長時間・高強度は可読性を損ないます。

## 68. Depth of Field

Cinematicでは有効ですが、通常戦闘で敵や警告をぼかさないようFocus/強度を制限します。

## 69. Temporal Effect

TAA、Temporal Upscale、Motion Blur等はMotion Vector、Jitter、History、Reactive Mask、Camera Cut処理が必要です。

## 70. History Reset

Resize、Teleport、Camera Cut、FOV急変、Spawn、Animation LOD急変、Device Lost時に対象履歴をResetします。

## 71. UI Pass

通常はTone Map後のDisplay Colorへ合成します。HDR UI、Scene連動UI、World MarkerはColor Spaceと順序を個別に設計します。

## 72. Frame Graph構成

```text
Character Pose Upload
 -> Shadow Cascades
 -> Depth / Motion
 -> Opaque Lighting
 -> Transparent / Hair
 -> Effect / Trail / Decal
 -> Distortion Composite
 -> Bloom / Exposure / ToneMap
 -> UI
 -> BackBuffer
```

Read/Write Resourceから依存を生成します。

## 73. Graph Resource

```text
MainDepth
SceneColorHDR
MotionVectors
ShadowAtlas
DistortionVector
BloomPyramid
LuminanceHistory
PostColor
BackBuffer
```

Format、Size、Lifetime、Clear Policyを記述します。

## 74. Pass Parameter

各Passは必要なHandleと設定だけを受け取り、Gameplay SingletonやGlobal Textureへ暗黙Accessしません。

## 75. Culling Pipeline

Layer → Distance → Frustum → LOD → Occlusionの順に安い判定から適用し、ViewごとのVisible Listを作ります。

## 76. Importance Culling

完全な不可視判定だけでなく、Budget超過時に画面寄与とGameplay重要度からEffect/Light/Decalを減らします。

## 77. Instancing

同一Mesh/Materialの雑魚敵、背景Object、Particle Billboard等をInstance化します。CharacterごとのPose差がある場合はPalette Offset等をInstance Dataへ持たせます。

## 78. Batch Key

Pass、Pipeline、Material、Mesh、Depth/LayerをKey化し、OpaqueのState変更を減らします。Transparent順序を壊しません。

## 79. Multithread Preparation

Animation、Culling、Render Item生成、Sort Key計算をJob化します。Immutable SnapshotとWorker専用Allocatorを使います。

## 80. Deferred Context判断

大量DrawのCommand記録でCPU Bottleneckが確認された場合に検討します。Immediate ContextのSingle-thread Baselineと比較します。

## 81. Frame Resource

Constant、Bone Palette、Instance、Particle Spawn、Debug Primitive等を複数Frame Ringで管理し、GPU使用中領域を上書きしません。

## 82. Upload Budget

FrameごとのConstant/Vertex/Texture Upload ByteとMap回数を測ります。戦闘開始や敵Spawn時のSpikeを確認します。

## 83. Streaming

戦闘前に重要Character、Attack Effect、Shader VariantをPreloadします。Background Assetが操作中にCompile/Createを集中させないようにします。

## 84. Shader Warm-up

代表Material/Effect Variantを事前に生成・実行し、初回使用Stutterを減らします。未使用Variantを無制限にWarm-upしません。

## 85. Memory Budget

Texture、Mesh、Animation、Transient RT、Particle、Upload、Readbackを分類し、Peakを測ります。解像度・品質設定で変わるResourceを把握します。

## 86. 60 FPS Budget例

```text
Frame Target : 16.67 ms
CPU Main     : 例  6.0 ms以内
CPU Render   : 例  5.0 ms以内
GPU Total    : 例 14.0 ms以内
Headroom     : Spikeと機種差のため確保
```

数値はHardwareと要件から決め、固定の正解とは考えません。

## 87. GPU Pass Budget

Shadow、Opaque、Transparent/Effect、Post、UIへBudgetを分けます。合計だけでなくAttack Impact Frameの最大値を見ます。

## 88. Combat Stress Scene

最大敵数、Boss、全Attack Effect、Camera急旋回、破壊Object、UI通知、Streamingを意図的に重ねた再現Sceneを用意します。

## 89. Replay

Input/Event/Seed/Cameraを記録し、同じ戦闘を繰り返せるようにします。画像差分とPerformance比較の基準になります。

## 90. Debug View

- Bounds、Frustum、LOD
- Bone/Sockets/Trail Point
- Light Volume、Shadow Cascade
- Overdraw、Material ID
- Motion Vector、Depth、Normal
- Effect Priority/Capacity
- Frame Graph Resource

## 91. Performance HUD

CPU/GPU/P95/P99、Pass時間、Draw、Triangle、Visible Character、Particle、Light、Upload、Transient Peakを表示します。

## 92. PIX Capture構造

Frame → View → Pass → Material/ChunkのMarker階層を作り、Frame Graph Pass名と一致させます。

## 93. GPU Query

Pass境界へTimestampを置き、Query Ringで数Frame後に取得します。Attack ImpactやCamera CutのSpikeを自動保存します。

## 94. Visual Validation

固定Scene/Seed/TimeでScreenshotを保存し、Shadow、Skin、Hair、Effect、Post、UIの差分を確認します。

## 95. Gameplay Validation

描画を無効にしてもHit/Damage結果が変わらないことを確認します。Visual EffectがGameplay判定のOwnerになっていない証拠です。

## 96. Resize Validation

Window、Render Scale、DPI、Fullscreenを変更し、Camera Aspect、UI、Post Resource、History Reset、Trail/Effect継続を確認します。

## 97. Device Lost Validation

Worker停止、Device/Graph Pool再生成、Asset再Upload、Fallback表示、履歴Reset後に戦闘を継続できるか確認します。

## 98. Capacity Validation

最大値を超えたときCrashやBuffer越境ではなく、Priorityに基づくDrop、LOD、Fallbackへ移ることを確認します。

## 99. 読みやすさの評価

敵の攻撃予兆、Player位置、Target、Hit結果、回避TimingがEffectの多い状況でも認識できるかを動画とPlay Testで確認します。

## 100. よくある失敗：演出が判定を決める

Particleが敵へ触れたからDamage等にするとFrame Rateや描画LODで結果が変わります。Gameplay Collisionが先に確定し、Effectは結果を表現します。

## 101. よくある失敗：全部High Quality

全敵へ最高LOD、全LightへShadow、全Effectへ歪みを使うとBudgetを超えます。重要度と画面寄与で配分します。

## 102. よくある失敗：Hit演出の重ねすぎ

Flash、Shake、Blur、Bloom、Freezeを常に最大化すると情報が読めません。攻撃種別と重要度でPreset化します。

## 103. よくある失敗：時間を一種類にする

Hit StopでUI、Camera、全Particle、Profilerまで停止します。Game/Visual/Real Timeを分けます。

## 104. よくある失敗：Capacity無制限

Particle、Decal、Light、Trail、Damage NumberをEvent数だけ生成すると激戦時に破綻します。PoolとHard Limitを持ちます。

## 105. よくある失敗：平均Frameだけ測る

通常移動が60 FPSでもImpact FrameやSpawn FrameがStutterします。P95/P99/MaximumとScenario別Metricを見ます。

## 106. よくある失敗：Global State依存

PassがGlobal Camera/Texture/Context Stateを暗黙利用するとMulti-view、Frame Graph、Testが壊れます。Parameterへ明示します。

## 107. 実装Phase 1：最小描画

Window、Device、Swap Chain、Triangle、Depth、Camera、Model、Texture、Lightingを作り、Debug Layer 0件を維持します。

## 108. 実装Phase 2：Character

Skeleton Animation、GPU Skinning、Material、Shadow、Socket/Weapon、Camera追従を統合します。

## 109. 実装Phase 3：Combat表現

Hit Event、Particle、Trail、Decal、Flash、Shake、Time Domainを追加し、Gameplayとの分離をTestします。

## 110. 実装Phase 4：画面品質

HDR、Bloom、Tone Map、Color Grade、Motion Vector、必要なTemporal Effect、UI合成を追加します。

## 111. 実装Phase 5：大量表示

Culling、LOD、Batch、Instancing、GPU Particle、Light Budget、Resource Poolを導入します。

## 112. 実装Phase 6：Architecture

Snapshot、Handle、Render Queue、Frame Graph、Feature Module、Resize/Device Lostを統合します。

## 113. 実装Phase 7：計測と調整

PIX、Timestamp、CPU Profiler、Replay、Stress Scene、Visual Regressionを使い、Budget内へ収めます。

## 114. Completion Checklist：基盤

- [ ] Debug Layer Error/Warningを説明できる。
- [ ] Resize、Minimize、Fullscreen、DPIに対応する。
- [ ] Device Lost再生成Pathがある。
- [ ] COM/Resource Lifetimeが明確である。
- [ ] SnapshotとHandleでLayerが分離されている。

## 115. Completion Checklist：戦闘描画

- [ ] Character Animation/Skinning/Shadowが動く。
- [ ] Weapon SocketとTrailが一致する。
- [ ] Hit/Guard/Dodge Eventが一度だけ表示される。
- [ ] EffectにPriority/Pool/Capacityがある。
- [ ] Attack WarningがDecorative Effectより読める。
- [ ] Hit Stop中のTime Domainが仕様どおりである。

## 116. Completion Checklist：性能

- [ ] CPU/GPU Bottleneckを分類できる。
- [ ] Pass別GPU時間とP95/P99を取れる。
- [ ] Draw/Particle/Light/Upload Counterがある。
- [ ] Stress SceneとReplayがある。
- [ ] Capacity超過時に安全に品質を下げる。
- [ ] 複数解像度とHardwareで測定する。

## 117. 理解確認問題

1. Gameplay HitとHit EffectのData Flowを説明してください。
2. Character描画に前Frame Bone Poseが必要になる理由を説明してください。
3. Effect Priorityが戦闘の可読性と性能を守る仕組みを説明してください。
4. Hit Stopで複数Time Domainが必要な理由を説明してください。
5. Shadow、Opaque、Effect、Post、UIのPass順を説明してください。
6. Camera Cut時にResetすべき履歴を挙げてください。
7. Attack Impact Frameを別Scenarioで測る理由を説明してください。
8. Frame Graphが総合Rendererの拡張を助ける理由を説明してください。

## 118. 章末要点

- Gameplayが戦闘結果を確定し、RendererはSnapshotとEventから表現します。
- Camera、Animation、Character、Effect、Post、UIを明示的なPassとTime Domainで統合します。
- Culling、LOD、Priority、Capacityにより激戦時も重要情報を優先します。
- Frame GraphでResource依存、Lifetime、実行順を管理します。
- Resize、Device Lost、History Reset、Frame ResourceをLifecycleへ含めます。
- Replay、Stress Scene、PIX、Timestamp、P95/P99で正しさと性能を検証します。
- 最小描画から段階的に積み上げ、各PhaseでDebug可能な状態を保ちます。

## 119. 関連するDirectX 11章

- 基盤：第1～19章
- 数学・Camera・Lighting：第20～22章
- Model・Animation：第23章
- Shadow・Post Process：第24～25章
- 大量描画・Compute：第26～27章
- Multithread・Lifecycle：第28～29章
- Debug・Architecture：第30～31章

個別技術で迷った場合は対応章へ戻り、この章では技術同士の順序と責務を確認します。

## 120. DirectX 11編の到達点

DirectX 11のDevice/Context/Swap Chainから、Shader、Resource、State、3D数学、Animation、Shadow、Post Process、Compute、Multithread、復旧、Profiler、Frame Graph、総合戦闘描画までの全体像を説明できる状態が到達点です。

次はDirectX 12編で、Command Queue/List、Descriptor Heap、Root Signature、PSO、Resource Barrier、Fence、Frame Resource、GPU Memoryを明示的に管理する方法を学びます。
