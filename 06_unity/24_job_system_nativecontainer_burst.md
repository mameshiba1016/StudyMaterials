# Job System・NativeContainer・Burst

> 対象: Unity 6.0、released版Burst 1.8系。Collections/BurstのAPIはPackage Versionで変わるため、Projectで固定したVersionの資料を確認すること。

## 1. 並列化の目的

Job Systemは「何でも別threadへ移す機能」ではありません。大量の独立したdataへ同じ計算を行い、main threadのcritical pathを短くする仕組みです。

```text
Main Thread
  input・Unity Object収集
  ↓ dataをNativeContainerへ準備
Schedule Jobs
  ↓ worker threadsで計算
Complete
  ↓ resultをmain threadでUnity Objectへ反映
```

## 2. Job化に向く処理

- 多数entityの距離・score計算。
- perception候補のfilter。
- projectile軌道のpure simulation。
- animation/steering用数値計算。
- spatial hash構築。
- procedural meshのvertex計算。
- 大量のtransform-independent math。

## 3. Job化に向かない処理

- 少量で一瞬の処理。
- GameObject/Component APIを大量に呼ぶ。
- managed object graphを辿る。
- I/O待ち。
- 頻繁なvirtual call/string処理。
- 強い逐次dependencyを持つ。
- schedule/copy costの方が大きい。

Profilerでmain thread hotspotを見つけてから適用します。

## 4. ThreadとJobの違い

自分で`Thread`を作る代わりに、Unityのworker thread poolへ小さな仕事とdependencyをscheduleします。

- worker数をengineが管理。
- `JobHandle`で依存を表す。
- NativeContainer Safety System。
- Burstとの連携。
- Unity Profiler統合。

Jobはschedule後に任意cancelできる一般的Taskではありません。

## 5. IJobの最小例

```csharp
using Unity.Burst;
using Unity.Collections;
using Unity.Jobs;

[BurstCompile]
public struct AddJob : IJob
{
    public float A;
    public float B;

    // Job struct自体はschedule時にcopyされる。
    // 結果を外へ出すには共有native memoryを指すcontainerを使う。
    public NativeArray<float> Result;

    public void Execute()
    {
        Result[0] = A + B;
    }
}
```

## 6. Schedule・Complete・Dispose

```csharp
NativeArray<float> result = new(
    length: 1,
    allocator: Allocator.TempJob,
    options: NativeArrayOptions.UninitializedMemory);

var job = new AddJob
{
    A = 10f,
    B = 20f,
    Result = result
};

JobHandle handle = job.Schedule();

// ここでmain threadがjobと独立な仕事を進める。

// Resultへ触る直前までCompleteを遅らせる。
handle.Complete();
float answer = result[0];

// Native memoryはGC任せにせず必ずDisposeする。
result.Dispose();
```

`Schedule`直後の`Complete`はworkerと重なる時間を失い、thread化の利点を減らします。

## 7. Completeの意味

`Complete()`は:

- job完了まで必要ならmain threadを待たせる。
- resultを安全に読む境界になる。
- Safety Systemのjob ownership stateをcleanupする。

完了していそうだからhandleを捨てる、ではなくownerが必ずComplete/依存chain管理します。

## 8. IJobParallelFor

```csharp
using Unity.Burst;
using Unity.Collections;
using Unity.Jobs;
using Unity.Mathematics;

[BurstCompile]
public struct DistanceSquaredJob : IJobParallelFor
{
    [ReadOnly] public NativeArray<float3> Positions;
    public float3 Origin;
    [WriteOnly] public NativeArray<float> Results;

    public void Execute(int index)
    {
        float3 offset = Positions[index] - Origin;
        Results[index] = math.lengthsq(offset);
    }
}
```

各indexが独立していれば複数workerで実行できます。同じindex/共有accumulatorへ無秩序にwriteしません。

## 9. Batch size

```csharp
JobHandle handle = job.Schedule(
    arrayLength: positions.Length,
    innerloopBatchCount: 64);
```

- 小batch: load balanceしやすいがschedule/work stealing overhead増。
- 大batch: overhead減、偏りやworker占有が増える。

要素数、1要素cost、worker数、cache localityを対象機で測ります。

## 10. Work stealing

Parallel jobは仕事をbatchへ分け、idle workerが残batchを取ります。各要素costが均一なら大きめbatch、ばらつくなら小さめbatchが有利な場合があります。

設定値を暗記せずProfilerのJob Worker timelineで空き・長いtailを確認します。

## 11. Job dependency

```text
BuildSpatialGrid ─┐
                  ├→ ScoreTargets → SelectTarget
UpdatePositions ──┘
```

後続jobは前jobの`JobHandle`をdependencyとしてScheduleします。これによりmain threadで途中Completeせず、worker上のchainとして繋げられます。

## 12. Dependency code

```csharp
JobHandle buildHandle = buildGridJob.Schedule();
JobHandle updateHandle = updatePositionJob.Schedule();

JobHandle prerequisites = JobHandle.CombineDependencies(
    buildHandle,
    updateHandle);

JobHandle scoreHandle = scoreJob.Schedule(
    targetCount,
    64,
    prerequisites);

// resultを使う最終地点だけで待つ。
scoreHandle.Complete();
```

dependency graphに不要なedgeを足すと並列性を失います。必要なread/write関係だけを表します。

## 13. Race condition

二つのthreadが同じmemoryへ少なくとも一方writeし、順序が未定ならraceです。

```text
Job A: value = value + 1
Job B: value = value + 1
```

read→modify→writeがinterleaveすると結果が1または2になり得ます。dependency、index分離、atomic、reduction等で解決します。

## 14. NativeContainer

managed objectをBurst/Jobから自由に使えないため、unmanaged memoryの安全なwrapperを使います。

代表例:

- `NativeArray<T>`。
- `NativeSlice<T>`。
- Collections packageの`NativeList<T>`。
- `NativeHashMap<TKey,TValue>`。
- `NativeParallelHashMap`等。
- `NativeQueue<T>`。
- `NativeStream`。

実際の型名・並列writer対応はPackage Versionで確認します。

## 15. NativeContainerはvalue type

```csharp
NativeArray<int> a = new(10, Allocator.Persistent);
NativeArray<int> b = a;
```

`b`は全dataをdeep copyせず、同じnative memoryとSafety情報を指す別struct copyです。片方をDispose後にもう片方へ触ると危険です。所有者を一つにします。

## 16. Allocator

### Temp

非常に短い寿命。通常jobへ渡す用途には制約があります。

### TempJob

短期間のjob用。規定frame数以内にDisposeする契約を守ります。

### Persistent

長期保持。allocate/free costが比較的大きく、ownerの明示Disposeが必要。

名称だけでなくCollections Versionのlifetime ruleを確認します。

## 17. NativeArrayOptions

- `ClearMemory`: 0初期化、安全だがcost。
- `UninitializedMemory`: 初期化を省く。全要素をwriteしてからreadすることを証明する。

部分writeのままreadすると未定義/過去dataを利用します。performanceのためのoptionはinvariantとtestを伴います。

## 18. ReadOnly・WriteOnly

```csharp
[ReadOnly] public NativeArray<float3> Inputs;
[WriteOnly] public NativeArray<float> Outputs;
```

access intentをSafety System/Burstへ伝えます。複数jobは同じcontainerを並列readできますが、writeとの競合にはdependencyが必要です。

`[WriteOnly]`はalgorithm上の初期化を自動保証しません。

## 19. ParallelFor restriction

ParallelFor jobでは通常、index `i`のExecuteはwrite対象のindex `i`へ書く制限があります。別indexへのscatter writeはrace防止上制約されます。

restriction disable attributeを安易に付けず、ParallelWriter、sort/group、two-pass algorithmを使います。

## 20. Reduction

全要素の合計を一つへ並列加算すると競合します。

```text
Phase 1: batchごとのpartial sum
Phase 2: partial sumsをreduce
```

thread-local/partition resultを作り、最後に少量を集約します。floating-point加算順が変わり結果bitが変わる点も考慮します。

## 21. NativeContainer Safety System

Safety Systemは:

- job間のread/write conflict。
- job実行中のmain thread access。
- Dispose後access。
- lifetime/leakの一部。

をdevelopmentで検出します。Safety checkを外せば正しくなるのではなく、未検出になるだけです。

## 22. Main thread access

jobが`NativeArray`へwrite中にmain threadから読むことはできません。

```csharp
JobHandle handle = job.Schedule();

// NG: handle.Complete()前にarray[0]を読む。

handle.Complete();
float value = array[0]; // ownershipがmain threadへ戻った後。
```

## 23. Dispose dependency

jobが使い終わった後にcontainerを非同期Disposeできます。

```csharp
JobHandle workHandle = job.Schedule();
JobHandle disposeHandle = array.Dispose(workHandle);

// disposeHandleもdependency chainへ含め、完了前にresource ownerを失わない。
disposeHandle.Complete();
```

利用可能overloadはcontainer/package versionで確認します。

## 24. try/finally

schedule前にexceptionが起きる可能性も含めてcleanupします。

```csharp
NativeArray<float> values = default;

try
{
    values = new NativeArray<float>(1024, Allocator.TempJob);
    // schedule / complete / consume
}
finally
{
    if (values.IsCreated)
    {
        values.Dispose();
    }
}
```

scheduled jobが未完了なら先にComplete/dependency付Disposeが必要です。

## 25. managed型を使えない理由

Job/Burst dataは基本的にblittable/unmanagedである必要があります。

避けるもの:

- `string`。
- class reference。
- `List<T>`。
- managed array。
- delegate/virtual dispatch。
- UnityEngine.Object。

ID、index、fixed-size value、NativeContainerへ変換します。

## 26. UnityEngine.Object境界

Job内で`Transform.position`、`GameObject.SetActive`等を一般的に直接操作しません。

```text
Main Thread: Componentからfloat3/inputをgather
Job: pure dataをsimulate
Main Thread: resultをTransform/Animatorへapply
```

gather/apply costがjob本体より大きくないか測ります。

## 27. TransformAccessArray

TransformをJob Systemと連携する専用APIがありますが、lifetime、main thread synchronization、階層cost、Burst対応を確認します。

大量objectならECSのcomponent dataの方が自然な場合もあります。次章で扱います。

## 28. Structure of Arrays

AoS:

```text
Actor { position, velocity, health, team, animation... }
Actor[]
```

SoA:

```text
positions[]
velocities[]
healths[]
teams[]
```

特定計算で必要fieldだけ連続readでき、cache/SIMDに有利な場合があります。管理complexityと変換costとのtrade-offです。

## 29. Cache locality

CPUは連続memoryをcache line単位で読みます。pointerを辿るmanaged object graphより、連続`NativeArray<float3>`を順に処理する方が有利になりやすいです。

parallel化せずBurst + data layoutだけで速くなることもあります。

## 30. False sharing

別threadが論理的には別変数を書いても、同じcache line上ならcache ownershipが往復し性能低下します。

per-thread counterを隣接配置する時などに注意し、partition、padding、reductionを検討します。推測だけでpaddingせずhardware profilerで測ります。

## 31. Burstとは

BurstはIL/.NET bytecodeからLLVMを使い最適化native codeを生成します。

- SIMD vectorization。
- constant folding。
- inlining。
- alias analysis。
- math最適化。
- platform向けcode生成。

Job SystemなしのBurst-compatible function pointer等もありますが、まずJobで基本を学びます。

## 32. BurstCompile

```csharp
[BurstCompile]
public struct IntegrateJob : IJobParallelFor
{
    public float DeltaTime;
    public NativeArray<float3> Positions;
    [ReadOnly] public NativeArray<float3> Velocities;

    public void Execute(int index)
    {
        Positions[index] += Velocities[index] * DeltaTime;
    }
}
```

Burst menu/settings、AOT/JIT compilation、safety checks、synchronous compile optionをdevelopment/releaseで理解します。

## 33. Unity.Mathematics

`float2/3/4`、`quaternion`、`math`関数はBurst/SIMDを意識した数値型です。

```csharp
float3 direction = math.normalizesafe(target - position);
float distanceSq = math.lengthsq(target - position);
```

`Vector3`との変換を境界でまとめ、hot loop内で往復しません。

## 34. Vectorization

独立iterationと単純な連続dataはSIMD化されやすいです。

妨げる要因:

- data dependency。
- branch過多。
- alias不明。
- managed call。
-複雑なscatter/gather。
- 小さ過ぎるloop。

Burst Inspectorで生成codeとvectorization reportを確認します。

## 35. Burst Inspector

確認できるもの:

- Burst compile対象。
- LLVM IR/assembly。
- optimization diagnostics。
- vectorized loop。
- warning/error。

assemblyを全て読めなくても、scalar/vector、関数call、load/store量を比較できます。

## 36. FloatMode・FloatPrecision

`FloatMode.Fast`等は演算順変更や特殊値handlingに影響し得ます。

- gameplay determinism。
- NaN/Infinity。
- replay/network sync。
- animation/visualのみ。
- platform差。

速い設定を全Jobへ付けず、許容誤差をtestして選びます。

## 37. Determinism

floating-pointはthread実行順、SIMD、FMA、platformでbit一致しない場合があります。

決定性が必要なら:

- fixed-point/quantization。
- reduction順固定。
- authoritative server。
- tolerance-based test。
- nondeterministic presentationとsimulation分離。

Burstを有効にしただけで決定性が得られるわけではありません。

## 38. Static dataの危険

Jobからmutable static dataへ触るとSafety Systemを迂回し、crash/raceを招きます。必要dataはJob fieldとして明示的に渡します。

global singletonを隠れたinputにしないことでdependencyもtestもしやすくなります。

## 39. Scheduleはmain thread

通常Jobの`Schedule`はmain threadから行います。worker jobの中で好きに別jobをscheduleする設計ではありません。

Job graphをmain thread/system側で構築し、dependencyを明示します。

## 40. Runでdebug

`Run()`対応Jobはmain threadで即時実行でき、algorithm correctnessのdebugに便利です。

```text
Run: thread/schedule問題を除いてlogic確認
Schedule: 実際のdependency/parallel性能確認
```

Runが速いからJob化不要、Scheduleが遅いからalgorithmが悪い、とは単純に言えません。

## 41. Schedule overhead

10要素の単純加算をJob化するとschedule/sync costが本体を上回り得ます。

- workをbatch化。
- system単位でまとめる。
- main threadで直接実行。
-一frame遅延を許しoverlap。

thresholdをProfilerで決めます。

## 42. Completeを遅らせる

悪い流れ:

```text
Schedule → Complete → Main work
```

良い候補:

```text
Schedule
 → main threadでUI/input/render prep等の独立work
 → result必要地点でComplete
```

ただしJob workerとrender/other systemsがCPU coreを奪い合うこともあるため全frameを見ます。

## 43. 一framepipeline

一frame遅延を許せる処理:

```text
Frame N: perception input収集・schedule
Frame N+1: result consume・次Job schedule
```

latencyを増やす代わりに長いoverlapを得ます。AI遠距離perception等には使えても、parry直前判定には不向きです。

## 44. Combatへの適用

Job候補:

- 多数敵のdistance/angle/filter。
- threat score。
- hit candidate broad phase。
- crowd steering。
- projectile integration。

Main threadに残す候補:

- GameObject/Animator反映。
- event順序確定。
- damage transaction commit。
- presentation dispatch。

## 45. Target scoring例

```csharp
[BurstCompile]
public struct TargetScoreJob : IJobParallelFor
{
    [ReadOnly] public NativeArray<float3> Positions;
    [ReadOnly] public NativeArray<byte> Alive;
    public float3 PlayerPosition;
    public float DistanceWeight;
    [WriteOnly] public NativeArray<float> Scores;

    public void Execute(int index)
    {
        if (Alive[index] == 0)
        {
            Scores[index] = float.NegativeInfinity;
            return;
        }

        float distanceSq = math.lengthsq(
            Positions[index] - PlayerPosition);

        Scores[index] = -distanceSq * DistanceWeight;
    }
}
```

最終select/tie-breakを順序固定で行えば、presentationのtarget切替が不安定になるのを防げます。

## 46. Gather cost

毎frame10000 GameObjectからTransformを読みNativeArrayへcopyするだけで高costになり得ます。

対策:

- simulation stateを最初からdata-oriented storeへ。
- dirty itemだけ更新。
- TransformAccessArray。
- ECS移行。
- Job対象を本当に大量なsystemへ限定。

Job本体だけのbenchmarkで全体高速化を主張しません。

## 47. Apply cost

結果10000件をmain threadでAnimator/Transformへ反映すれば再びbottleneckになります。

- visible/active subsetだけapply。
- update frequency/LOD。
- GPU animation/instancing。
- ECS transform/rendering。
- resultをevent/decisionへ圧縮。

入力と出力境界のcostを含めます。

## 48. NativeList容量

可変containerはcapacity超過時に再allocationし得ます。

- expected capacityを事前設定。
- parallel writerの契約。
- resize可能なphase。
- overflow policy。
- Clear後capacity保持。

hot job内で予測不能にgrowさせない設計にします。

## 49. ParallelWriter

複数threadからcollectionへ追加するには専用`ParallelWriter`を使える型があります。

注意:

- output順は実行順依存。
- capacity不足。
- key重複。
- deterministic sortingが後で必要。
- writerを作った元containerのlifetime。

parallel-safe APIでもgameplay順序が自動で決定的にはなりません。

## 50. NativeHashMap

hash mapは便利ですが、random access、hash cost、capacity、parallel write競合があります。grid cell→entity listならmulti-hash map系、sort by cellならarray + sort等も比較します。

使用型はCollections packageの現行APIを確認します。

## 51. Entity Command的な出力

Job内でGameObjectをspawnせず、pure commandを出します。

```csharp
public struct SpawnRequest
{
    public int PrefabId;
    public float3 Position;
    public quaternion Rotation;
}
```

main threadが結果をvalidationし、Object Pool/Addressables ownerを通して生成します。

## 52. Exceptionとdebug

Burst Job内で一般managed exception handlingへ依存しません。

- input validationをschedule前。
- development safety checks。
- result error code。
- assertionの対応範囲。
- Burstを一時disableして比較。
- `Run()`。

releaseでcheckが消えても正しく動くinvariantを作ります。

## 53. Leak detection

NativeContainerをDisposeし忘れるとnative memory leakです。

- owner lifecycle。
- `IsCreated`。
- `OnDestroy/Dispose`。
- exception path。
- job handle completion。
- Unity Collections leak detection setting。
- Memory Profiler/long loop。

## 54. MonoBehaviour owner例

```csharp
public sealed class DistanceSystem : MonoBehaviour
{
    private NativeArray<float3> positions;
    private NativeArray<float> distances;
    private JobHandle activeHandle;
    private bool scheduled;

    private void Awake()
    {
        positions = new NativeArray<float3>(
            1024,
            Allocator.Persistent);
        distances = new NativeArray<float>(
            1024,
            Allocator.Persistent);
    }

    private void LateUpdate()
    {
        if (!scheduled)
        {
            return;
        }

        activeHandle.Complete();
        scheduled = false;
        // distancesをmain thread側でconsumeする。
    }

    private void OnDestroy()
    {
        // Scene破棄中でもjob完了を待ってからmemoryを解放する。
        if (scheduled)
        {
            activeHandle.Complete();
            scheduled = false;
        }

        if (positions.IsCreated) positions.Dispose();
        if (distances.IsCreated) distances.Dispose();
    }
}
```

## 55. Application終了

終了順でownerが先に破棄されても、scheduled jobはnative memoryを使い続けます。全handleをcompleteし、containerをdisposeするshutdown phaseを持ちます。

static containerやDomain Reload無効Editor状態もclearします。

## 56. Profiling

見るもの:

- schedule cost。
- Job Worker utilization。
- job duration/batch tail。
- `Complete` wait time。
- main thread gather/apply。
- Burst on/off。
- safety checks on/off。
- native allocation/leak。
- cache miss/vendor counters。

Job内marker対応とProfiler overheadは公式資料を確認します。

## 57. Burst on/off比較

同一data/同一build条件で:

```text
Managed main-thread loop
Job without Burst
Job with Burst
Burst loop via Run
```

を比較すると、parallel化、Burst codegen、schedule overheadの効果を分離できます。

## 58. Small workload test

要素数を10、100、1000、10000と変え、break-even pointを測ります。対象機core数/clockでthresholdが変わります。

runtimeで閾値未満はRun/main thread、以上はScheduleというpolicyも可能ですがcomplexityを測ります。

## 59. Worker saturation

背景Jobを大量に流すと、frame critical jobやUnity内部Jobとcoreを奪い合います。

- priority概念の限界。
- long jobのbatch size。
- background streaming/decompression。
- frame boundary。
- worker count。

全workerを100%埋めることが目的ではありません。

## 60. ECSとの関係

Job System + NativeContainer + BurstはMonoBehaviour projectでも使えます。ECSはさらにdata layout、query、system scheduling、structural changeを統合します。

先にこの章のownership、dependency、unmanaged data、Burst制約を理解すると次章へ繋がります。

## 61. よくある失敗

### Schedule直後にComplete

overlapがなく、schedule overheadだけ増えます。

### Job内でGameObjectを触る

main thread API制約を破ります。dataをgather/applyする。

### Safety attributeで警告を消す

raceを解決せず検出を無効化します。

### NativeArray copyを別memoryと思う

同じnative allocationを指します。二重Dispose/use-after-freeの原因です。

### Burstなら必ず速い

gather/apply、small workload、memory accessが支配する場合があります。

### parallel output順を信じる

実行順は保証されません。必要ならsort/tie-breakする。

## 62. Correctness test

- 0/1/最大要素。
- batch size違い。
- Burst on/off。
- safety on/off。
- Run/Schedule結果一致。
- dependency順。
- same container read/read。
- write/write conflict検出。
- cancel相当のowner破棄。
- Scene unload中job。
- NaN/Infinity。
- deterministic tie-break。
- 1000 loopでnative memory増加なし。

## 63. Performance test

- element count別break-even。
- core数別。
- Mono/IL2CPP。
- Burst on/off。
- development/release。
- gather/job/apply別marker。
- p95/p99 Complete wait。
- worker occupancy。
- thermal long run。

## 64. Review checklist

- ProfilerでJob化対象を特定したか。
- Jobがpure unmanaged data中心か。
- schedule後すぐCompleteしていないか。
- dependency graphが必要最小限か。
- NativeContainer ownerが一つか。
- Allocator lifetimeを守るか。
- 全pathでComplete/Disposeするか。
- read/write attributeが正しいか。
- ParallelForのindex write制約を守るか。
- reduction/scatterを安全に設計したか。
- gather/apply costを含めて測ったか。
- Burst precision/determinismをtestしたか。
- worker saturationを確認したか。

## 65. 学習確認問題

1. Job Systemと自前Threadの主な違いは何か。
2. Schedule直後のCompleteが不利な理由は何か。
3. NativeArrayを変数へ代入するとdataはdeep copyされるか。
4. 複数Jobのread/readは可能でwrite/writeが危険な理由は何か。
5. Parallel reductionを一つの変数へ直接書かない理由は何か。
6. BurstとJob Systemは同じものか。
7. `UninitializedMemory`を使う条件は何か。
8. gather/apply costがJob化を無意味にする例は何か。
9. FloatMode.Fastがgameplayへ影響し得る理由は何か。
10. worker使用率100%が必ず良いとは限らない理由は何か。

## 66. 公式資料

- [Unity Manual: Job system overview](https://docs.unity3d.com/6000.0/Documentation/Manual/job-system.html)
- [Unity Manual: Create and run jobs](https://docs.unity3d.com/6000.0/Documentation/Manual/job-system-creating-jobs.html)
- [Unity Manual: Job dependencies](https://docs.unity3d.com/6000.0/Documentation/Manual/job-system-job-dependencies.html)
- [Unity Manual: Thread-safe types](https://docs.unity3d.com/6000.0/Documentation/Manual/job-system-native-container.html)
- [Unity Manual: Copy NativeContainer structures](https://docs.unity3d.com/6000.0/Documentation/Manual/job-system-copy-nativecontainer.html)
- [Unity Manual: Burst package](https://docs.unity3d.com/6000.0/Documentation/Manual/com.unity.burst.html)
- [Burst 1.8 Manual](https://docs.unity3d.com/Packages/com.unity.burst@1.8/manual/index.html)
- [BurstCompile attribute](https://docs.unity3d.com/Packages/com.unity.burst@1.8/manual/compilation-burstcompile.html)
- [Collections Manual](https://docs.unity3d.com/Packages/com.unity.collections@latest/)

## 67. まとめ

- Job Systemは大量の独立計算をworkerへscheduleし、dependencyで安全な順序を表す。
- NativeContainerはunmanaged memoryのwrapperで、struct copyしても同じmemoryを指す。
- Safety Systemはrace/use-after-freeを検出するが、正しいalgorithmの代わりではない。
- Scheduleを早く、Completeを結果が必要な時まで遅らせてmain/workerを重ねる。
- BurstはLLVMでnative codeを最適化し、連続dataと単純loopでSIMD効果を得やすい。
- Job本体だけでなくgather、apply、schedule、worker競合、native memoryを含めて測る。
- determinism、float precision、lifetime、shutdownまでcorrectness testする。
