# DXライブラリ：Profiler・Memory・最適化

この章では、ゲームが遅い理由を推測ではなく計測で特定し、正しさを維持したまま改善する方法を学びます。最適化は「短いコードへ書き換えること」ではありません。目標Hardwareと最悪のGameplay状況でFrame Budgetを守り、Memory、Loading、Battery、発熱も含めた体験を安定させる作業です。

> 計測機能やDriver挙動は環境で変わります。DXライブラリ公式仕様、利用中のHeader、Visual Studio Profiler、PIX等の最新資料も確認してください。

## 1. 最適化の順序

```text
目標を定義する
 -> 再現可能な負荷Sceneを作る
 -> 計測する
 -> Bottleneckを特定する
 -> 一つの仮説を立てる
 -> 最小変更で試す
 -> 同条件で再計測する
 -> 正しさをRegression Testする
```

Profilerを見ずに書き換えると、速くない場所を複雑にするだけになりがちです。

## 2. FPSよりFrame Time

```text
60 FPS  = 16.666... ms/frame
120 FPS =  8.333... ms/frame
144 FPS =  6.944... ms/frame
30 FPS  = 33.333... ms/frame
```

FPSは逆数なので加算比較しにくい値です。120から100 FPSへの低下は約1.67ms、60から40 FPSへの低下は約8.33msです。処理CostはFrame Timeで考えます。

## 3. Frame Budget

60 FPSの16.67msを例に、Budgetを仮置きします。

```text
Input/Game Logic   2.0 ms
Animation          2.0 ms
Physics/Collision  2.0 ms
AI                 1.5 ms
Render Submit      2.5 ms
GPU Rendering     12.0 ms
Margin             2.0 ms
```

CPUとGPUは並行動作するためCPU合計とGPU合計を単純加算するとは限りません。ただし同期点で待ちが発生します。

## 4. 平均だけでは足りない

平均16msでも、定期的に80ms Frameがあれば操作感は悪化します。

- Median：典型的なFrame。
- P95：遅い上位5%の境界。
- P99：Stutterを捉える。
- Maximum：極端な停止。ただしDebugger等の外乱も確認する。
- 1% Low / 0.1% Low：低Frame Rate側の安定性。

Frame Time Histogramと時間軸Graphを残します。

## 5. 高精度Timer

DXライブラリの `GetNowHiPerformanceCount` はマイクロ秒単位です。

```cpp
class ScopedCpuTimer final
{
public:
    ScopedCpuTimer(std::string_view name, Profiler& profiler)
        : name_(name), profiler_(profiler), start_(GetNowHiPerformanceCount()) {}

    ~ScopedCpuTimer()
    {
        const LONGLONG end = GetNowHiPerformanceCount();
        profiler_.RecordMicroseconds(name_, end - start_);
    }

    ScopedCpuTimer(const ScopedCpuTimer&) = delete;
    ScopedCpuTimer& operator=(const ScopedCpuTimer&) = delete;

private:
    std::string_view name_;
    Profiler& profiler_;
    LONGLONG start_{};
};

void Game::Update(float deltaSeconds)
{
    ScopedCpuTimer timer{"Game.Update", profiler_};
    UpdateCharacters(deltaSeconds);
}
```

計測自体にもCostがあります。細かすぎるMarkerはSamplingを行うかRelease計測版で選択的に有効化します。

## 6. `GetNowCount`との違い

`GetNowCount` はミリ秒単位、`GetNowHiPerformanceCount` はマイクロ秒単位です。16.67ms付近の細かな内訳には高精度版を使います。

Timer差分は絶対時刻よりOverflowへ強くなりますが、型と単位を混同しないよう名前へ含めます。

```cpp
using Microseconds = std::int64_t;

Microseconds MeasureWork()
{
    const LONGLONG beginMicroseconds = GetNowHiPerformanceCount();
    DoWork();
    return GetNowHiPerformanceCount() - beginMicroseconds;
}
```

## 7. `GetFPS`の意味

DXライブラリの `GetFPS` は、直近2回の `ScreenFlip` 間に経過した時間から1秒を割った値です。実際の1秒間のFlip回数を数えた平均ではありません。

瞬間値は揺れるため、Frame Time Ring Bufferから移動平均、P95、P99も計算します。

## 8. `GetDrawCallCount`の意味

`GetDrawCallCount` は自作の `DrawGraph` 等を呼んだ回数ではなく、WindowsではDirect3Dの描画APIが呼ばれた回数です。DXライブラリ内部Batchにより複数描画が一Callへまとまる場合があります。

値が少ないほど常に速いとは限りません。巨大な一Draw Call、重いPixel Shader、過剰OverdrawはCall数だけでは見えません。

## 9. Profiler Sample

```cpp
struct ProfileSample final
{
    std::uint32_t nameId{};
    std::uint32_t threadId{};
    std::uint32_t depth{};
    std::int64_t beginMicroseconds{};
    std::int64_t endMicroseconds{};
};
```

文字列を毎回Copyせず、起動時にNameをID化します。Threadごとの固定容量Bufferへ記録し、Frame終了時に集約します。

## 10. Nested Scope

```text
Frame 16.4 ms
├─ Update 5.1 ms
│  ├─ AI 1.2 ms
│  ├─ Animation 2.3 ms
│  └─ Collision 1.0 ms
└─ Render CPU 6.8 ms
   ├─ Visibility 1.1 ms
   ├─ Sort 0.7 ms
   └─ Submit 4.4 ms
```

子Sample合計と親時間の差は、未計測処理やProfiler overheadを示します。

## 11. Exclusive TimeとInclusive Time

- Inclusive Time：子処理を含むScope全体。
- Exclusive Time：子処理を除いた自身だけの時間。

親Functionが遅く見えても、実際は一つの子Functionが支配している場合があります。両方を表示します。

## 12. CPU Sampling Profiler

Sampling Profilerは一定間隔でCall Stackを観測します。

- Instrumentationより導入が容易。
- 全体のHot Function探索に向く。
- 短い処理や待機原因を見落とす場合がある。
- Sampling intervalにより精度が変わる。

最初にSamplingでHotspotを探し、必要箇所へMarkerを追加する流れが効率的です。

## 13. CPU BoundとGPU Bound

```text
解像度を大きく下げてFPSが大幅改善 -> GPU Boundの可能性
解像度を下げても変化が小さい       -> CPU/同期/Frame capの可能性
```

これは目安であり確定ではありません。GPU Profiler、CPU Timeline、Present待ちを合わせて確認します。

## 14. CPU-GPU Parallelism

CPUはDraw Commandを準備し、GPUは以前のFrameを実行できます。CPU Timerで `ScreenFlip` やResource更新が長く見える場合、関数内部処理ではなくGPU完了待ちの可能性があります。

```text
CPU: Frame N+1 submit ---- wait
GPU:       Frame N render --------
```

同期点の手前に積み上がったGPU負荷が、待った関数のCostとして見えることに注意します。

## 15. GPU計測

CPU時計でDraw Call前後を囲んでもGPU実行時間は測れません。PIX等のGPU Capture/Timingを使い、Pass、Shader、Render Target、Bandwidth、Occupancyを調べます。

DXライブラリの高水準APIだけで見えない場合、DirectX編でTimestamp QueryやPipeline Statisticsを学びます。

## 16. VSyncとFrame Cap

VSync有効では `ScreenFlip` がDisplay更新を待ち、16.67ms付近に固定されることがあります。性能測定では次を記録します。

- VSyncの有無。
- Frame Limitの有無。
- Window/Full Screen。
- Display Refresh Rate。
- GPU Driver設定。

`SetWaitVSyncFlag` は公式仕様上 `DxLib_Init` 前に設定する必要があります。

## 17. Debug Buildで判断しない

Debug BuildはIterator検査、最適化無効、Debug Runtime等で遅くなります。

```text
Debug          : 正しさ、Assert、開発
Development    : Compiler最適化 + Profiler/Log
Release        : 配布条件に近い最終計測
```

Development Buildを用意し、Symbolを保持したまま最適化されたCodeをProfileします。

## 18. 再現可能なBenchmark

- Camera Pathを固定する。
- Random Seedを固定する。
- Enemy数と行動Scriptを固定する。
- Replay入力を使う。
- Warm-up時間を設ける。
- 計測時間を固定する。
- Background Applicationを記録する。
- Build、Commit、Driver、Hardwareを記録する。

変更前後を同条件で比較しなければ改善量を判断できません。

## 19. ColdとWarm

- Cold Run：初回File I/O、Shader生成、Page Fault、Cache未命中を含む。
- Warm Run：一度使用済みでCacheが温まった状態。

両方に意味があります。初回体験にはCold、通常戦闘にはWarmを測り、混ぜて平均しません。

## 20. Stutterの主因

- Gameplay中の同期File I/O。
- Texture・Model・Shaderの遅延生成。
- 大量Allocation/Free。
- Container再確保。
- 初回だけのStatic初期化。
- OS Page Fault。
- Thread Lock競合。
- GPU Resource Uploadと同期。
- Shader/Driver Compilation。
- SaveやLogのFlush。

平均負荷が低くても一回の同期処理でStutterします。

## 21. Memoryの分類

```text
Code/Static
Heap
Stack
Mapped File
Graphics Resource / VRAM
Driver Resource
OS Cache
```

Task Managerの一つの値だけでLeak判定しません。Private Bytes、Working Set、Commit、VRAMを区別します。

## 22. OwnershipがMemory管理の基礎

- 値として所有できるなら値を使う。
- 単一所有は `std::unique_ptr`。
- 共有寿命が本当に必要な場合だけ `std::shared_ptr`。
- 非所有参照はPointer、Reference、Handle、`std::span`。
- DX Handleは型付きRAII Wrapperで所有する。

所有者が不明なMemoryはLeak・二重解放・Use-after-freeの原因です。

## 23. Allocationを計測する

```cpp
struct AllocationStats final
{
    std::uint64_t allocationCount{};
    std::uint64_t freeCount{};
    std::uint64_t allocatedBytes{};
    std::uint64_t peakLiveBytes{};
};
```

FrameごとのAllocation数、Size、Call Site、Lifetimeを記録します。Global `new` HookはLibrary内部や初期化順への影響があるため、専用AllocatorやProfiler機能を優先します。

## 24. LeakとHigh-water Mark

Memoryが増えたままでも、必要なCacheが最大量まで育っただけならLeakとは限りません。

```text
同じSceneを Load -> Play -> Unload する
 -> 毎Cycle基準値が増える: Leak疑い
 -> 最初だけ増え以後安定: Cache/Poolの可能性
```

Resource種類・Owner・作成Call Stackを追跡します。

## 25. Fragmentation

空きMemory合計が十分でも、連続BlockがなくAllocationできない状態です。

- SizeとLifetimeが似たObjectを同じAllocatorへまとめる。
- Pool/Slabを使う。
- 大きな一時Bufferを再利用する。
- Long-livedとShort-livedを分離する。
- 頻繁な可変Size Allocationを減らす。

Poolは上限Memoryを保持するため、用途と解放Policyを決めます。

## 26. Stack

Stack Allocationは高速ですが容量に限りがあります。巨大配列、深い再帰、大きな値ObjectのCopyでStack Overflowが起きます。

```cpp
void BadExample()
{
    // 数MBの局所配列はThread Stackを圧迫する。
    std::array<std::byte, 4 * 1024 * 1024> temporary{};
}
```

大容量はHeap、Frame Arena、再利用Bufferへ置きます。

## 27. `std::vector`の再確保

```cpp
std::vector<RenderItem> items;
items.reserve(expectedVisibleCount); // 予測できる最大付近を確保。

for (const Entity& entity : entities)
{
    if (IsVisible(entity))
        items.push_back(BuildRenderItem(entity));
}
```

`reserve` は要素数を増やさずCapacityだけ確保します。過剰ReserveはMemoryを浪費するため実測Peakから決めます。

## 28. Object Pool

Projectile、Particle、Damage Numberのように生成破棄が多いObjectを再利用します。

```cpp
struct PoolSlot final
{
    Projectile object{};
    std::uint32_t generation{};
    bool active{};
};

struct ProjectileHandle final
{
    std::uint32_t index{};
    std::uint32_t generation{};
};
```

Generationを比較し、再利用後に古いHandleが別Objectを指す問題を防ぎます。

## 29. Frame Arena

Frame終了までだけ必要なRender List、Sort Key、一時CommandはLinear Arenaへまとめられます。

```text
Frame begin: offset = 0
alloc A: offset += aligned size
alloc B: offset += aligned size
Frame end: 全Destructor不要なDataならoffset = 0
```

Arenaを越えてPointerを保持してはいけません。非Trivial Destructorを持つObjectの扱いも設計します。

## 30. Data Locality

CPU Cacheは近接Memoryをまとめて読みます。Pointerを辿るLinked構造より連続配列が有利な場合が多いです。

```cpp
// AoS: Entity単位で全Fieldが近い。
struct Particle { VECTOR position; VECTOR velocity; float life; int texture; };

// SoA: 同じ処理で使うFieldが連続する。
struct Particles
{
    std::vector<VECTOR> positions;
    std::vector<VECTOR> velocities;
    std::vector<float> lifetimes;
};
```

更新がPositionとVelocityだけならSoAが有利な可能性があります。必ずBenchmarkします。

## 31. Cache LineとFalse Sharing

複数Threadが別変数を書いていても、同じCache Line上なら互いに無効化し合います。

```cpp
struct alignas(64) ThreadCounter final
{
    std::atomic<std::uint64_t> value{};
};
```

64byteは典型値であり対象Hardwareを確認します。Alignmentを増やすとMemory使用量も増えます。

## 32. BranchとVirtual Call

Branch Prediction失敗や間接CallはCostになり得ますが、可読性を壊してまで除く前にProfileします。

- 同じ状態のEntityをまとめて処理する。
- Hot Loop内のType分岐を外側へ移す。
- Function Pointer/Virtual CallよりData Localityが支配的な場合も多い。
- Branchless Codeは常に速いわけではない。

## 33. Algorithm Complexity

Micro OptimizationよりAlgorithm変更が大きく効きます。

```text
全Entity対全Entity collision: O(N²)
Spatial Gridで近傍だけ:       期待O(N + candidate)
```

Sort、Search、Pathfinding、Visibility、String処理の要素数と増加率を記録します。

## 34. Early Out

```cpp
void Enemy::Update(float deltaSeconds)
{
    if (!active_) return;
    if (distanceToPlayer_ > simulationRange_) return;
    if (IsCulledAndSleeping()) return;

    UpdateDecision(deltaSeconds);
    UpdateMovement(deltaSeconds);
}
```

安い判定を先に置き、高価な処理へ入る数を減らします。ただし遠距離でも必要なTimerやNetwork同期を止めないよう、更新Levelを分けます。

## 35. Tick Frequency

すべてを毎Frame更新する必要はありません。

- 近距離戦闘AI：毎Frame。
- 遠距離知覚：数Frameごと。
- UIの静的Layout：Dirty時のみ。
- 遠景Animation：低頻度。
- Save Metadata更新：Event時のみ。

分散更新すると一Frameへの集中を避けられます。低頻度化で反応が遅れない要件を定義します。

## 36. Collision最適化

```text
Broad Phase: AABB、Grid、BVHで候補を減らす
Narrow Phase: Sphere/Capsule/Triangle等で正確判定
```

- Collision Layer/Maskで不要Pairを除く。
- Static Geometryを空間構造へ入れる。
- Bounding VolumeをCacheする。
- 同じQueryを複数Systemで重複しない。
- Debug時にCandidate数を可視化する。

## 37. AI最適化

- Perceptionを距離・視野でBroad Phaseする。
- Pathfinding RequestをQueue化し分散する。
- 同じGoalのPath/Flow Fieldを共有する。
- 遠距離AIを簡略Stateへ落とす。
- Behavior Treeの評価Node数を計測する。
- Line of Sight Ray数へBudgetを設ける。

AIの品質を保つ重要Enemyへ優先度を割り当てます。

## 38. Animation最適化

- 画面外ModelのPose更新を低頻度化する。
- 骨数をLODで減らす。
- 同一Clip Sample結果を共有できる条件を探す。
- Root MotionとGameplay Boneは必要頻度を守る。
- Shadow PassとMain Passで同じBone Paletteを使う。
- Animation Eventを低頻度化で飛ばさない。

主役の入力応答へ関係するAnimationは安易に遅らせません。

## 39. RenderingのCPU Cost

- Visibility Test。
- Render Item生成。
- Sort。
- State設定。
- Draw Call発行。
- Constant更新。

不透明物をShader・Material・Texture順へまとめ、状態変更を減らします。透明物は正しいDepth順が優先です。

## 40. BatchとAtlas

DXライブラリ内部でBatchされる場合も、Texture、Blend、描画先、Shader変更でBatchが分断されます。

- SpriteをTexture Atlasへまとめる。
- 同じStateのUIを連続して描く。
- 個別Primitiveの大量描画をMesh化する。
- Static描画をRender TargetへCacheする。

AtlasはPadding不足によるMip Bleeding、巨大化、更新単位の問題も持ちます。

## 41. Draw Callだけを追わない

100 Callでも重いShaderと巨大三角形なら遅く、1000 Callでも小さく軽い場合があります。

合わせて測る値：

- Triangle/Vertex数。
- Pixel数、Overdraw。
- Shader Instruction/Texture Sample。
- Render Target解像度とFormat。
- State変更回数。
- Constant Upload量。
- GPU Pass時間。

## 42. Overdraw

同じPixelを何度も塗るほどPixel ShaderとBlendのCostが増えます。

- 不透明物はFront-to-backで早期Depth rejectを狙う。
- 大きな透明Particleを減らす。
- 画面外・極小ParticleをCullする。
- Alpha 0でもShader実行される場合を理解する。
- UIの全画面半透明Layerを重ねすぎない。

Overdraw可視化で画面上の集中箇所を見ます。

## 43. Resolution Scaling

Pixel Costが支配する場合、3D Render Target解像度を下げ、UIはNative解像度で描く方法があります。

```text
GPU frame > upper threshold: scaleを少し下げる
GPU frame < lower threshold: scaleを少し上げる
```

Hysteresisと変更間隔を設け、解像度が毎Frame揺れないようにします。最低Scaleと文字/UI品質を守ります。

## 44. LOD

- Mesh LOD：遠距離の頂点を減らす。
- Material LOD：Shader・Texture Sampleを減らす。
- Animation LOD：骨・更新頻度を減らす。
- Shadow LOD：Casterや解像度を減らす。
- Simulation LOD：AI・Physicsを簡略化する。

距離だけでなくScreen占有率を使い、切替にHysteresisを設けます。

## 45. Texture Memory

概算は次のとおりです。

```text
uncompressed bytes ≒ width × height × bytesPerPixel × mip factor
full mip chain factor ≒ 4/3
```

2048×2048 RGBA8はMipなし約16MiB、完全Mip込み約21.3MiBです。圧縮Format、Array、Cube、Render Target、Staging Copyも含めて集計します。

## 46. Mipmap

MipはMemoryを増やしますが、遠距離SamplingのCache効率とちらつきを改善します。UIやPixel Artでは不要な場合がある一方、3D Textureで省くと逆に遅くなることがあります。

用途別Import Policyを作ります。

## 47. Model Memory

- Vertex数 × Vertex stride。
- Index数 × 2または4byte。
- Bone Weight、Tangent、複数UV。
- Animation Curve、Key数。
- Collision Mesh。
- CPU CopyとGPU Copy。

見た目に使わないAttributeをExportしないようAsset検査を自動化します。

## 48. Resource Cache

同じPathを複数回LoadするとMemoryとLoading Costを重複させます。

```cpp
struct ResourceRecord final
{
    int handle{-1};
    std::size_t estimatedBytes{};
    std::uint32_t strongReferenceCount{};
    std::uint64_t lastUsedFrame{};
};
```

正規化PathをKeyにし、Owner数、最終使用Frame、Size、Stateを追跡します。

## 49. Eviction Policy

- LRU：最終使用が古いものから解放。
- Size-aware：大きいResourceを優先候補にする。
- Priority：主役・UI・共通Resourceを保護。
- Scene scope：Scene終了でまとめて解放。
- Memory pressure：Budget超過時だけ段階的に解放。

GPUが使用中のResourceを即削除しないよう、DXライブラリ/Driverの寿命契約を守ります。

## 50. LoadingとStreaming

```text
I/O -> Decompress -> Parse -> CPU Resource -> GPU Upload -> Ready
```

各段階を計測します。Background Threadで可能な処理と、DXライブラリAPIを呼べるThreadを分けます。GPU Uploadを一Frameへ集中させずBudget化します。

## 51. Asyncは無料ではない

- Job生成・Queue・Synchronization Cost。
- Data Copy。
- Cache競合。
- Thread oversubscription。
- Debugの複雑化。
- 終了・Cancel処理。

小さなJobを細分化しすぎず、処理量Thresholdを設けます。

## 52. Lock Contention

Mutex待ち時間と保持時間を計測します。

- Critical Sectionを短くする。
- I/Oや重い計算をLock中に行わない。
- Read-only Snapshotを渡す。
- Thread-local Bufferを後でMergeする。
- Lock順序を固定してDeadlockを防ぐ。

Lock-freeは難しく、常に速いわけではありません。

## 53. Logging Cost

毎Frameの同期Log、String Format、File FlushはStutter原因になります。

- Log Levelで無効化する。
- Format前にLevelを判定する。
- 固定容量Queueへ送り別Threadで書く。
- SpamをRate Limitする。
- Crash時に必要な直近LogはRing Bufferへ保持する。

Logを消すだけで原因追跡不能にならないよう、開発版と配布版のPolicyを分けます。

## 54. String Cost

- 毎Frameの文字列連結。
- `std::to_string` とFormat。
- Map Keyとしての長いString。
- UTF変換。
- UI Layout再計測。

安定IDへInternし、表示が変わった時だけ文字列を更新します。読みにくい手動Buffer最適化はProfile結果がある箇所だけに限定します。

## 55. Floating-pointとSIMD

Compilerの自動Vector化を妨げない連続Data、Aliasの少ないLoop、単純な分岐が重要です。手動SIMDは対象Instruction Set、Alignment、端数処理、精度差を増やすため、AlgorithmとLayout改善後に検討します。

Fast MathはNaN、Infinity、演算順、再現性を変える可能性があります。

## 56. Determinismと最適化

Parallel化、Container順変更、浮動小数の演算順変更でReplay結果が変わる場合があります。

- Gameplay決定順を固定する。
- Stable IDでSortする。
- Random Streamを用途別に分ける。
- Reduction順を固定する。
- Replay HashでRegressionを検出する。

速さのために戦闘結果の再現性を失う変更は明示判断が必要です。

## 57. Compiler Optimization

- Release最適化を有効にする。
- Link Time Optimizationを測る。
- Debug Symbolを別途保持する。
- Warningを高く保つ。
- Undefined Behavior Sanitizer等を検証Buildで使う。
- Profile Guided Optimizationは代表Workloadで行う。

Compiler Flag変更後は数値精度、Replay、Crash Dump、Build時間も確認します。

## 58. Inlining

小さなHot FunctionのInliningはCall Costを減らせますが、Code Size増加でInstruction Cacheを悪化させる場合があります。Headerへ全実装を移すことと性能は同義ではありません。Compiler判断とProfileを尊重します。

## 59. Memory Budget

```text
System memory budget
├─ Engine/Core
├─ Current Scene
├─ Character
├─ Animation
├─ Audio
├─ UI
├─ Streaming cache
└─ Safety margin

VRAM budget
├─ Textures
├─ Models/Buffers
├─ Render Targets
├─ Shadow Maps
└─ Driver margin
```

Budget超過後にCrashするのではなく、Loading時の見積りとRuntime監視で早期警告します。

## 60. Quality Tier

```cpp
struct QualitySettings final
{
    float renderScale{1.0f};
    int shadowResolution{2048};
    int shadowCascadeCount{3};
    int maximumParticles{5000};
    float effectDistance{60.0f};
    int animationLodBias{};
};
```

Low/Medium/Highを個別値の集まりとしてData化します。一項目が複数Systemへどう影響するかを明記します。

## 61. Dynamic Budget

戦闘の重要度に応じてBudgetを配分します。

- 主役と近距離Enemyを優先。
- 画面外Effectを削減。
- 遠距離AIの更新頻度を下げる。
- Particle Spawnを上限で抑える。
- Shadow更新を分散する。

突然消すより、生成数・距離・更新頻度を滑らかに変化させます。

## 62. Optimization Regression

性能改善で起きやすい不具合：

- Pool再利用時のState初期化漏れ。
- 低頻度更新でEventを飛ばす。
- Cullingで必要なShadow/Audioまで消す。
- Sort変更で透明描画が壊れる。
- Cacheの古いDataを使う。
- Parallel化によるData Race。
- Fast Mathによる判定差。

性能Testと機能Testを同時に通します。

## 63. Performance Testの自動化

```cpp
struct BenchmarkResult final
{
    double medianMilliseconds{};
    double p95Milliseconds{};
    double p99Milliseconds{};
    double maximumMilliseconds{};
    std::uint64_t peakMemoryBytes{};
    int maximumDrawCalls{};
};
```

CI Hardwareの揺れを考慮し、厳密な1%差ではなく大きなRegressionを検出します。結果ArtifactにCommit、Build、Hardware、設定を含めます。

## 64. 戦闘Stress Scene

- 最大想定Enemy数。
- 同時攻撃とProjectile。
- 最大Particle・Trail・Afterimage。
- 複数CharacterのAnimation Blend。
- Camera近距離での高いScreen占有率。
- Damage NumberとHUD更新。
- Shadow付きLight。
- Audio Voice上限。

通常Sceneだけでなく、設計上許される最悪条件を固定Replayにします。

## 65. Profiler Overlay

```text
FPS / Frame ms / P95 / P99
Update / Render CPU / Present wait
Draw calls / Visible objects / Triangles
Heap live / Peak / Allocations this frame
Texture / Model / Sound handles
Particle / Projectile / Enemy counts
```

Overlay自体の描画・文字Format Costも計測し、必要なら更新頻度を下げます。

## 66. Capture Trigger

Frame Timeが閾値を超えた前後の履歴を自動保存します。

```cpp
if (frameMilliseconds > 33.0)
{
    profiler.RequestCapture(
        /*historyFrames=*/120,
        /*futureFrames=*/60);
}
```

Stutter発生前のResource LoadやJob Queue膨張を追跡できます。連続保存をCooldownで制限します。

## 67. Before/After記録

```text
Hypothesis: UI文字列再構築がStutter原因
Change: 値変化時のみFormatしLayout Cache
Before: P99 24.2ms, UI update 3.8ms, 620 alloc/frame
After : P99 17.6ms, UI update 0.7ms, 42 alloc/frame
Risk  : Language/UI scale変更時のCache invalidation
```

改善量とTrade-offを残すと、後から複雑な最適化を維持する理由が分かります。

## 68. よくある誤り

- FPSだけを見て結論を出す。
- 一回の実行結果だけで比較する。
- Debugger接続中の外れ値を混ぜる。
- CPU TimerでGPU時間を測ったと思う。
- Draw Call数だけを減らす。
- `reserve` を全Containerへ巨大指定する。
- Poolを作ったが上限とResetを設計しない。
- Multithread化で同期Costを増やす。
- Releaseでしか起きない問題をDebugだけで検査する。
- 平均を改善してP99を悪化させる。

## 69. 調査Decision Tree

```text
Frameが遅い
├─ CPU frameが長い
│  ├─ Running: Hot function/algorithm/allocationを調査
│  └─ Waiting: lock/I-O/GPU sync/frame capを調査
├─ GPU frameが長い
│  ├─ resolution依存: pixel/overdraw/shader/RTを調査
│  └─ resolution非依存: vertex/draw/state/syncを調査
└─ 時々だけ長い
   ├─ loading/allocation/page fault
   ├─ shader/resource first use
   ├─ save/log flush
   └─ OS/background/thermal
```

## 70. 実装チェックリスト

- [ ] 目標Hardware、FPS、Frame Budgetを定義した。
- [ ] 固定Replayの通常・Stress Benchmarkがある。
- [ ] Frame TimeのMedian、P95、P99、Maximumを記録する。
- [ ] `GetFPS` と実測平均の意味を区別した。
- [ ] `GetDrawCallCount` を描画関数回数と誤解していない。
- [ ] CPU ScopeのInclusive/Exclusive Timeを見られる。
- [ ] CPU待機と実行を区別できる。
- [ ] GPU CaptureでPass時間を確認できる。
- [ ] VSync、解像度、Build、Hardwareを記録した。
- [ ] Frame Allocation数とPeak Memoryを追跡した。
- [ ] DX HandleとResource Ownerを追跡した。
- [ ] Texture、Model、Render Target、ShadowのMemoryを見積もった。
- [ ] I/O、Decompress、Uploadを別々に計測した。
- [ ] LOD、Culling、更新頻度にHysteresisがある。
- [ ] 最適化前後を同条件で複数回比較した。
- [ ] 正しさとReplayのRegression Testを行った。

## 71. 練習課題

1. RAII型CPU Scope Timerを実装する。
2. 300 FrameのRing BufferからMedian/P95/P99を計算する。
3. Update、Animation、Collision、RenderへMarkerを入れる。
4. `GetFPS` と1秒間の実Flip回数を並べて表示する。
5. `GetDrawCallCount` と自作Draw命令数を比較する。
6. Object Pool導入前後のAllocation数を測る。
7. `vector::reserve` 前後の再確保数と時間を測る。
8. O(N²)衝突候補をSpatial Gridへ変え、要素数別に比較する。
9. UI Atlas化前後のDraw CallとTexture切替を測る。
10. 解像度50%・75%・100%でGPU時間を比較する。
11. Texture/Model/Shadow MapのMemory表を作る。
12. 固定戦闘ReplayのP99 Regression Testを作る。
13. 33ms超過時の自動Capture Triggerを作る。
14. Before/After Templateで一件の改善を記録する。

## 72. 理解確認

1. FPSよりFrame Timeが比較に向く理由は何ですか。
2. Averageが良くてもP99が悪いと何が起きますか。
3. CPU TimerでGPU実行時間を直接測れない理由は何ですか。
4. `GetDrawCallCount` と描画関数呼出回数の違いは何ですか。
5. Cold RunとWarm Runを分ける理由は何ですか。
6. Memory増加が必ずLeakではない理由は何ですか。
7. Pool HandleにGenerationが必要な理由は何ですか。
8. Data LocalityがCPU性能へ影響する理由は何ですか。
9. Draw Call削減だけではGPU性能を判断できない理由は何ですか。
10. 最適化後にGameplay Replay Testが必要な理由は何ですか。

## 73. この章の到達点

- FPS、Frame Time、Percentile、Frame Budgetを正しく使える。
- CPU/GPU Bound、実行/待機、平均/Stutterを切り分けられる。
- DXライブラリのTimer・FPS・Draw Call統計を正しく解釈できる。
- Ownership、Pool、Arena、Data LayoutでMemoryを設計できる。
- AI、Animation、Collision、Rendering、UIの負荷を段階的に減らせる。
- Texture、Model、Render Target、CacheのMemory Budgetを作れる。
- 再現可能なBenchmarkと自動Regression Testを構築できる。
- 計測結果・仮説・改善量・Riskを記録して継続的に改善できる。

## 74. 公式・関連資料

- [DXライブラリ：時間関係関数](https://dxlib.xsrv.jp/function/dxfunc_other.html)
- [DXライブラリ：関数一覧](https://dxlib.xsrv.jp/dxfunc.html)
- [Microsoft Learn：Visual Studio performance profiling](https://learn.microsoft.com/en-us/visualstudio/profiling/)
- [Microsoft Learn：PIX on Windows](https://devblogs.microsoft.com/pix/)

Timer単位、VSync設定時期、FPSとDraw Call統計の定義は、利用中のDXライブラリ公式資料で再確認してください。
