# Profiler・Frame Debugger・Memory Profiler

> 対象: Unity 6.0、Memory Profiler 1.1系。Profilerの対応counter、GPU timing、接続方法はplatform・Graphics API・build設定で異なる。

## 1. 最適化は調査である

```text
症状を定義
 → 再現条件を固定
 → 計測
 → CPU/GPU/Memory/I/Oを分類
 → hotspotを絞る
 → 一変更
 → 同条件で再計測
 → correctnessを確認
```

「重そうなcode」を眺めて書き換えるのではなく、frame captureと数値から原因を特定します。

## 2. FPSよりframe time

```text
30 FPS  = 33.33 ms/frame
60 FPS  = 16.67 ms/frame
90 FPS  = 11.11 ms/frame
120 FPS =  8.33 ms/frame
```

FPSは逆数なので平均しづらく、spikeを隠します。CPU main/render、GPU、p95/p99、worst frameをmsで見ます。

## 3. Budgetを先に決める

60 FPSの16.67 msを全てScriptへ使えるわけではありません。

```text
Main thread gameplay/animation/physics  7 ms
Render submission                     4 ms
余裕/OS/variation                     5 ms

GPU shadow + opaque + transparent + post < 16.67 ms
```

CPUとGPUはpipelineで並行するため、単純に両者を足しません。target deviceごとにbudgetを作ります。

## 4. 平均だけでは足りない

例:

```text
999 frame: 8 ms
1 frame: 120 ms
average: 約8.1 ms
```

平均は良くても一瞬止まります。percentile、最大、spike頻度、連続over-budget frameを記録します。

## 5. 再現条件

- build commit/content version。
- Unity/Package Version。
- device/OS/driver。
- resolution/render scale。
- quality設定。
- targetFrameRate/VSync。
- thermal状態/battery mode。
- stage、camera、enemy数。
- fresh cache/warm cache。
- recording開始までのwarm-up。

条件が違うcaptureを比較して結論を出しません。

## 6. EditorとPlayer

EditorにはInspector、Scene View、EditorLoop、domain関連のoverheadがあります。

```text
Editor Play Mode → 素早い仮説検証
Development Player → 詳細な対象機capture
Release相当 Player → 最終的な実性能
```

Development Build/Profiler接続自体もoverheadを持つため、最終数値はrelease条件でも測ります。

## 7. Unity Profilerの主なModule

- Highlights。
- CPU Usage。
- GPU Usage。
- Rendering。
- Memory。
- Audio。
- Physics。
- UI/UI Details。
- Video。
- File Access。
- Virtual Texturing等。

利用可能moduleはVersion/Package/Platformで変わります。最初から全moduleをrecordするとoverhead/data量が増えるため必要なものへ絞ります。

## 8. Profiler接続

対象機buildでは通常:

1. Development Build。
2. Autoconnect Profilerまたは接続先指定。
3. Profiler Windowのtarget selector。
4. 同一network/USB等のplatform要件。
5. Firewall、IP、port、device permission確認。

buildを公開環境でprofiling可能なままにしないよう、development/release設定を分離します。

## 9. Record範囲

長時間無差別recordでは問題frameを探しにくくなります。

```text
5秒warm-up
 → Record開始
 → 3秒通常戦闘
 → 必殺技 + enemy wave
 → 2秒
 → Record停止
```

再現操作にmarker/telemetry eventを置くとcapture内で見つけやすくなります。

## 10. Highlights Module

Unity 6のHighlightsはtarget frame timeに対してCPU/GPUがoverしたframeを示し、CPU boundかGPU boundかの調査入口になります。

これは最終原因を自動診断するものではありません。CPU側ならTimeline、GPU側ならGPU Profiler/Frame Debugger/vendor toolへ進みます。

## 11. CPU boundとGPU bound

### CPU bound

CPU active timeがbudget超過し、GPUが待つ/余裕がある状態。

### GPU bound

GPU timeがbudget超過し、CPUがpresent/fence等で待つ状態。

### Present limited

VSync/target frame rate待ちで、処理が遅いとは限りません。

### Balanced

CPU/GPUが近い。小変更でbottleneck側が入れ替わります。

## 12. Wait markerを仕事と誤解しない

代表例:

- `WaitForTargetFPS`: target FPS待ち。
- `Gfx.WaitForPresentOnGfxThread`: GPU/present/render thread側待ちを示す場合。
- `Gfx.WaitForGfxCommandsFromMainThread`: render threadがmain threadからのcommand待ち。
- Job workerのSemaphore wait。

長いsample名だけでそのthreadが原因とは判断せず、「誰を待っているか」を他threadと合わせて読みます。

## 13. CPU Usage Timeline

Timelineはthreadごとのsampleを時間軸で表示します。

見る順:

1. frame全体幅。
2. Main Threadの長いblock。
3. Render Thread。
4. Job Workerの並列度と待ち。
5. GC/Physics/Animation/Script markers。
6. 子sampleへ掘る。

色だけで判断せずmarker名、duration、call countを確認します。

## 14. Hierarchy View

Hierarchyはsampleをcall hierarchyとして集計します。

- Total: 子を含む時間。
- Self: 子を除く自身の時間。
- Calls: 呼出回数。
- GC Alloc: managed allocation。
- Mean/peak等。

Totalが大きい親を直すのではなく、Selfの大きいleaf、過剰calls、allocation sourceを探します。

## 15. Raw Hierarchy

Hierarchyのgroupingで隠れる詳細をraw viewで確認できます。同名markerが別callsite/threadにある場合や、PlayerLoop配下を正確に追う時に使います。

表示modeの意味はUnity Versionのmanualを確認します。

## 16. CallsとSelf Time

```text
Method A: 0.02 ms × 10,000 calls = 200 ms
Method B: 2 ms × 1 call          =   2 ms
```

一回が小さくてもcall数で支配します。Updateを持つComponent数、GetComponent/query回数、event fan-out等を見ます。

## 17. PlayerLoopを読む

概念例:

```text
Initialization
EarlyUpdate
FixedUpdate
PreUpdate
Update
PreLateUpdate
PostLateUpdate
Rendering/Present
```

Scriptの`Update`だけでなく、Physics simulation、Animator update、Canvas rebuildがどのphaseに出るか理解すると原因をsystemへ対応できます。

## 18. FixedUpdate burst

frameが遅れると、simulation timeへ追いつくため一frame内で複数FixedUpdate/Physics stepが走り、さらにframeが遅くなる場合があります。

Timelineで同frame内のstep数を数え、fixedDeltaTime、maximumDeltaTime、physics cost、spiral対策を確認します。

## 19. Script costの入口

- `Update.ScriptRunBehaviourUpdate`等の配下。
- custom ProfilerMarker。
- allocation call stack。
- object/component数。
- method call count。

`Update`全体が大きいだけではどのsystemか分からないため、domain単位markerを追加します。

## 20. ProfilerMarker

```csharp
using Unity.Profiling;

public sealed class CombatSimulation
{
    // Markerはstatic readonlyで一度作り、毎frame名前stringを生成しない。
    private static readonly ProfilerMarker SimulateMarker =
        new(ProfilerCategory.Scripts, "Game.Combat.Simulate");

    public void Simulate(float deltaTime)
    {
        // using scopeを抜ける時にEndされるため、exception/early returnでも対称。
        using (SimulateMarker.Auto())
        {
            ResolveCommands();
            UpdateActors(deltaTime);
            ResolveHits();
        }
    }

    private void ResolveCommands() { }
    private void UpdateActors(float deltaTime) { }
    private void ResolveHits() { }
}
```

`ProfilerMarker`はDevelopmentでarbitrary code blockを可視化し、non-Developmentではconditional compilationにより除かれる契約があります。

## 21. Marker命名

```text
Game.Combat.Simulate
Game.Combat.ResolveHits
Game.AI.Perception
Game.AI.Decision
Game.Presentation.SpawnVfx
Game.Streaming.ActivateStage
```

class名だけよりdomain.phaseを含めます。dynamic IDをmarker名へ連結するとmarker種類/data量が爆発するため、entity別情報は別counter/logへ。

## 22. Marker粒度

粗過ぎる:

```text
Game.Update 12 ms
```

細か過ぎる:

```text
一回数nanosecond相当のmethodごとに数万marker
```

最初はsystem/phase単位、hotspotが見えたら一時的に細分化します。instrumentation overheadも計測します。

## 23. Begin/Endの危険

```csharp
Marker.Begin();
DoWork();
Marker.End();
```

exceptionやearly returnで`End`を通らないとsampleが壊れます。通常は`using (Marker.Auto())`を使い、Job/Burst等で適切なAPIを確認します。

## 24. ProfilerCounter

時間以外のdomain値も重要です。

```text
Active Enemies
Hit Tests / Frame
Visible VFX
Pool Misses
Streaming Bytes
AI Decisions / Frame
```

countとtimeを並べると、「AIが遅い」のか「AI数が多い」のかを分離できます。利用APIはUnity VersionのProfiler Counter資料に従います。

## 25. ProfilerRecorder

`ProfilerRecorder`はmarker/counter値をruntime codeやtestから取得できます。

```csharp
using Unity.Profiling;
using UnityEngine;

public sealed class RuntimePerformanceHud : MonoBehaviour
{
    private ProfilerRecorder gcAllocatedRecorder;
    private ProfilerRecorder mainThreadRecorder;

    private void OnEnable()
    {
        // counter名/categoryは対象UnityでGetAvailable等から確認する。
        gcAllocatedRecorder = ProfilerRecorder.StartNew(
            ProfilerCategory.Memory,
            "GC Allocated In Frame",
            capacity: 15);

        mainThreadRecorder = ProfilerRecorder.StartNew(
            ProfilerCategory.Internal,
            "Main Thread",
            capacity: 15);
    }

    private void OnDisable()
    {
        // Native側resourceを持つstructなので必ずDisposeする。
        gcAllocatedRecorder.Dispose();
        mainThreadRecorder.Dispose();
    }

    public long LastGcBytes => gcAllocatedRecorder.Valid
        ? gcAllocatedRecorder.LastValue
        : 0L;

    public double LastMainThreadMilliseconds => mainThreadRecorder.Valid
        ? mainThreadRecorder.LastValue * 1e-6 // nanosecond counterならmsへ変換。
        : 0.0;
}
```

counterのunitをmetadataで確認し、名前の存在をplatformごとに検証します。

## 26. Recorderの統計

ring buffer sampleから:

- min/max。
- mean。
- percentile近似。
- over-budget count。
- consecutive spikes。

を算出できます。毎framesampleを`ToArray`/sortして新規allocationしないよう、低頻度集計や再利用bufferを使います。

## 27. FrameTimingManager

CPU total/main/render thread、GPU frame time等の高水準timingをruntime overlayやadaptive qualityへ使えます。

counter availability/latency/platform差があり、GPU timingが0/unsupportedの場合を扱います。adaptive qualityが頻繁に上下しないようhysteresisを入れます。

## 28. CPU hotspotの分類

- Gameplay script。
- Physics。
- Animation。
- Rendering submission/culling。
- UI rebuild。
- GC。
- Asset load/deserialize。
- Job scheduling/completion。
- synchronization wait。
- logging/profiler overhead。

分類後に担当toolへ移ります。

## 29. Main Thread bound

Main Threadのactive workが長い場合:

- algorithm/call count削減。
- update frequencyを落とす。
- event駆動。
- spatial partition。
- batching。
- Job/Burstへ適切なdata parallel処理を移す。
- render submission削減。

thread化する前にdependencyとmain-thread Unity API境界を明確にします。

## 30. Render Thread bound

Render Threadが長くMain Thread/GPUが待つ場合:

- draw/dispatch数。
- state change。
- material/pass数。
- shadow caster。
- command generation。
- Graphics Jobs対応。
- pipeline feature。

Frame Debuggerで何を提出しているか確認します。

## 31. Job Worker

- workerが均等にbusyか。
-一つのlong jobがcritical pathか。
- small job過多でschedule overheadか。
- `Complete`でMain Threadが待つか。
- dependency chainが直列化していないか。
- safety check/Burst有無。

worker使用率が高いこと自体は悪ではなく、frame critical pathを超える仕事が問題です。

## 32. GC.Alloc調査

1. Allocation Call Stacksを有効化。
2. spike/steady state frameをcapture。
3. `GC.Alloc` sampleを選択。
4. callsiteへ移動。
5. call count × bytesを確認。
6. Playerで再確認。

managed heap growthと`GC Allocated In Frame`は同じ値ではありません。再利用可能heap領域もあります。

## 33. GPU Usage Profiler

GPU時間のpass別内訳を調べます。

- shadows/depth。
- opaque。
- transparent。
- lighting。
- post process。
- compute。
- UI。
- other/SRP pass。

Graphics APIやplatformでUnity GPU Profilerが使えない/制限される場合、RenderDoc、PIX、Xcode、vendor profiler等を使います。

## 34. GPU bound検証

resolutionを大きく下げてframe timeが明確に改善するなら、pixel/fill/bandwidth側の疑いが強まります。改善しなければgeometry/CPU/固定cost等を調べます。

これは診断実験であり、即「解像度を下げて解決」とはしません。

## 35. GPU bottleneck分類

- vertex/geometry bound。
- pixel/fill-rate bound。
- texture bandwidth/cache。
- shadow rendering。
- overdraw/transparency。
- compute shader。
- memory capacity。
- synchronization/readback。
- shader compilation/PSO warm-up。

vendor counterとshader analysisで確認します。

## 36. Rendering Module

代表値:

- Batches/draw calls。
- SetPass calls。
- Triangles/vertices。
- shadow casters。
- visible/animated skin mesh。
- render texture changes。
- dynamic/static batchingやinstancing情報。

数字の多寡だけで善悪を決めず、target GPU/CPU、pass数、shader costと合わせます。

## 37. Frame Debugger

Frame Debuggerは一frameのrendering eventを順に止めて確認するtoolです。

```text
Clear
 → Shadow pass
 → Depth prepass
 → Opaque draws
 → Lighting
 → Transparent/VFX
 → Post processing
 → UI
 → Final blit/present
```

GPU時間を直接正確に測るtoolではなく、「何が、どの順で、どのstateで描かれたか」を調べます。

## 38. Frame Debuggerの使い方

1. `Window > Analysis > Frame Debugger`。
2. target processを選択。
3. Enableで一frame capture。
4. event hierarchyを移動。
5. Game View/RenderTextureの積み上がりを見る。
6. event detailsを見る。

Player接続にはDevelopment Build等の条件があります。capture中はapplicationがpauseされます。

## 39. Draw eventで見る情報

- GameObject/renderer。
- mesh/vertex/index。
- material/shader/pass。
- shader keyword。
- render target/depth target。
- blend/depth/stencil/cull state。
- batching reason。
- instance count。
- render queue。

Version/pipelineにより表示項目が異なります。

## 40. なぜbatchされないか

- material instanceが違う。
- shader/pass/keyword違い。
- render state違い。
- lightmap/probe等の違い。
- mesh/renderer条件。
- transparency sorting。
- SRP Batcher非互換property layout。
- MaterialPropertyBlock/instancing条件。

Frame Debuggerのbatch reasonを見てから修正します。

## 41. renderer.material事故

`renderer.material`へアクセスするとmaterial instanceが作られ、共有/batching/memoryへ影響し得ます。

Frame Debuggerで同じ見た目のobjectが別material drawになっていないか、Memory ProfilerでMaterial instance数が増えていないか確認します。

## 42. Overdraw

透明VFX/UIは前後関係にかかわらずpixel shader/blendingを重ねやすいです。

```text
large smoke quad
 + additive slash
 + particles
 + full-screen post
 + UI overlay
```

Frame Debugger/Rendering Debugger/vendor overdraw viewで、画面を覆う透明passと描画順を確認します。

## 43. Shadow cost

- caster数。
- cascade数。
- resolution。
- additional light shadow。
- alpha clipped vegetation。
- skinned mesh。
- update頻度。

Frame Debuggerでshadow passのdraw数、GPU Profilerでtime、visual QAで品質を比較します。

## 44. RenderTexture lifetime

Frame Debuggerでoffscreen targetがいつclear/write/blitされるか追います。

-不要なfull resolution target。
- MSAA/depth format。
- 同時live temporary target。
- camera stacking。
- post process ping-pong。
- Scene View/Editor固有target。

Memory Profiler/Render Graph viewer等と合わせてpeakを見る。

## 45. Frame Debuggerの限界

- captureでtemporal effectが変わる。
- pauseによりstreaming/time-dependent挙動が変化。
- GPU costを直接示さない。
- hardware counterを持たない。
- Editor captureはPlayerと違う。

TAA jitter等は`FrameDebugger.enabled`を見てdebug時だけ安定化させる場合があります。

## 46. URP/HDRP Rendering Debugger

pipeline固有debug viewを使います。

- material validation。
- lighting/shadow。
- overdraw。
- mipmap/texture streaming。
- rendering layers。
- Render Graph/resource。
- bottleneck stats。

debug shader variant stripping設定とDevelopment Build要件を確認し、releaseへ不要variantを残さないようにします。

## 47. Memory Profiler ModuleとPackage

### Built-in Memory Module

frameごとの高水準trend/counterを見る。

### Memory Profiler Package

snapshotを取得し、Managed/Native object、allocation、reference chain、snapshot比較を詳しく調査する。

Unity 6.0ではMemory Profiler 1.1系がreleased packageです。

## 48. Snapshotとは

ある時点のmemory状態を保存します。

- Unity Objects。
- managed heap/object。
- native allocation/object。
- graphics memoryの推定/情報。
- types/categories。
- references/paths。

snapshot取得自体は重く、applicationを止め、追加memory/diskを使います。戦闘frame time測定と同時目的ではありません。

## 49. Snapshot比較手順

leak調査例:

```text
A: Boot直後
 → Stage load
 → Battle
 → Stage unload
 → cleanup/wait
B: Boot相当へ戻った後

Compare B - A
```

一回だけでcache/pool初期化をleakと誤認しないため、同じloopを複数回行い増加が継続するか見ます。

## 50. Baselineを揃える

比較snapshotは同じlogical stateで取ります。

- same Scene/content set。
-同じcamera/UI。
- async load完了後。
- GC/cleanup timingを揃える。
- pool policyを記録。
- profiler connection条件を揃える。

Stage中とTitle中のmemory差は当然で、leakの証明になりません。

## 51. Memory categories

- Texture。
- Mesh。
- Material/Shader。
- AnimationClip/Animator。
- AudioClip。
- GameObject/Component。
- Managed object/array/string。
- NativeArray/graphics buffer。
- AssetBundle/Addressables。
- Profiler overhead。

大きいcategoryからobject list、size、count、referenceへ掘ります。

## 52. Resident・Reserved・Allocated

用語はtool/platformで定義を確認します。

- Allocated/Used: allocatorが利用中として扱う量。
- Reserved: allocatorがOS等から確保したarena。
- Resident: physical memoryにresidentなpage。
- Committed: virtual addressへbackingがある等。

objectを解放してもreserved/process memoryが即減らない場合があります。

## 53. Managed ShellとNative Object

UnityEngine.Objectはmanaged wrapperとnative objectを持ちます。

```text
Managed C# wrapper
  ↕ instance ID/native pointer
Native Unity Object
  → Graphics/Audio resource
```

managed reference chainとnative referenceを両方見ます。C# wrapperが小さくてもTexture native/GPU memoryは巨大です。

## 54. Reference chain

不要objectが残る時、「誰が参照しているか」を追います。

典型root:

- static field/singleton。
- event/delegate。
- DontDestroyOnLoad object。
- pool。
- cache/dictionary。
- coroutine/Task closure。
- ScriptableObject asset。
- Addressables handle。
- native engine manager。

大きいobject自体でなくroot ownerのlifecycleを直します。

## 55. Static event leak

```csharp
private void OnEnable()
{
    GlobalEvents.StageChanged += OnStageChanged;
}

private void OnDisable()
{
    GlobalEvents.StageChanged -= OnStageChanged;
}
```

subscriberがScene unload後もstatic publisherから参照されると回収されません。OnDisableが必ず対称になるか、exception/Domain Reloadもtestします。

## 56. Poolをleakと誤認しない

poolがinactive objectを意図的に保持するためsnapshot差分で増えます。

確認:

- retained minimum/max。
- high-water mark。
- clear条件。
- content lease。
- pool telemetry。

意図的residentでもbudget超過ならpolicyを変える必要があります。

## 57. Cache warm-up

初回だけ増え、その後plateauになるもの:

- shader/PSO cache。
- font atlas。
- localization table。
- Addressables catalog/bundle cache metadata。
- object pool。
- managed reflection/serialization cache。

loopごとに同量増え続けるかを見ます。

## 58. Texture memory

見る項目:

- dimensions/mip count。
- format/compression。
- read/write enabledによるCPU copy。
- streaming mip residency。
- duplicate texture。
- RenderTexture。
- platform override。
- Sprite atlas。

file sizeとruntime memoryは異なります。圧縮fileがGPU上でどのformat/sizeになるか確認します。

## 59. Mesh memory

- vertex/index count。
- vertex attributes。
- 16/32-bit index。
- read/write enabled。
- blend shape。
- skinning data。
- duplicate imported mesh。
- runtime generated meshをDestroyしたか。

LODはrender costだけでなくresident mesh memoryも増やし得ます。

## 60. Audio memory

- Load Type。
- compressed/decompressed size。
- preload data。
- streaming buffer。
- voice language全localeを同時residentにしていないか。
- duplicated AudioClip。

Audio ProfilerとMemory Snapshotを合わせます。

## 61. Addressables調査

- handle release漏れ。
- poolがinstanceを保持。
- shared dependencyが他Assetから参照。
- catalog/cacheとmemoryの混同。
- Bundle churn。
-旧content catalogのBundle。

Addressables Event Viewer/diagnostic data、Memory Profiler、custom lease telemetryを対応させます。

## 62. NativeArray leak

`NativeArray`等はGCが通常のmanaged objectのように自動回収しません。

- `Dispose`。
- Job dependency後のDispose。
- exception/cancel path。
- Scene unload。
- persistent allocator owner。
- safety warning。

GC allocation削減でnative leakを作らないでください。

## 63. Memory peak

snapshotは点なので遷移中peakを逃す場合があります。

```text
Old stage resident
 + New stage loading
 + decompression/temp buffers
 + upload staging
 = transition peak
```

Memory Module/counterのtime seriesと、peak付近snapshotを組み合わせます。

## 64. Asset unload後も減らない時

順に確認:

1. active instanceが残る。
2. pool/cache/static/event。
3. Addressables handle。
4. shared dependency。
5. async request未完了。
6. GPU command未完了/deferred destruction。
7. allocator reserved memory。
8. Editor/Profiler overhead。

すぐ`UnloadUnusedAssets`を追加して隠しません。

## 65. File Access Profiler

stutterがI/O由来の場合:

-同期file read。
- AssetBundle load。
- shader cache。
- save/autosave。
- log write。
- streaming audio/texture。

File Access moduleやplatform I/O toolでpath、bytes、duration、threadを確認します。file名にはprivacy情報が含まれ得るため共有時に注意します。

## 66. Physics Profiler

- simulation time。
- active body。
- contact/collider数。
- broadphase。
- query count。
- CCD。
- layer collision matrix。
- multiple fixed steps。

Physics Debug windowでshapeを確認し、GC/nonalloc queryはCPU call stackでも見ます。

## 67. Animation Profiler

- Animator数。
- culling mode。
- layer/state complexity。
- IK。
- skinning。
- rig constraints。
- update offscreen。
- controller override。

一体のcostとactive character数を分け、NPC LOD/update frequencyを設計します。

## 68. UI Profiler

- Canvas rebuild。
- layout rebuild。
- graphic rebuild。
- batch split。
- text generation。
- event processing。
- list virtualization。

HUD全体を一つの頻繁にdirtyなCanvasへ置かず、更新頻度/重なりで分ける判断をProfiler/Frame Debuggerで行います。

## 69. Audio Profiler

- voice count。
- virtual voice。
- DSP CPU。
- streaming CPU/buffer。
- clip memory。
- mixer group/effect。
- decode spike。

音が鳴っていないように見えてもloop/inaudible sourceがactiveな場合があります。

## 70. Profiler overhead

- Deep Profile。
- Call Stacks。
-大量custom marker。
- Autoconnect/streaming data。
- screenshot/capture。
- development safety checks。
- Editor inspectors。

診断featureを一つずつ変え、overhead込みの数値をrelease targetと同一視しません。

## 71. Captureを保存する

Profiler data、Memory Snapshot、Frame Debugger情報をissueへ紐付けます。

metadata:

```text
commit/build ID
device/OS/GPU/driver
quality/resolution
reproduction steps
expected budget
capture timestamp
tool settings
```

巨大captureや機密path/dataの共有policyも決めます。

## 72. Before/After report

| Metric | Before | After | Budget |
|---|---:|---:|---:|
| CPU Main p99 | 14.2 ms | 8.1 ms | 9 ms |
| GPU p99 | 12.0 ms | 10.8 ms | 14 ms |
| GC Alloc/frame | 18 KB | 0.3 KB | 1 KB |
| Stage resident | 2.3 GB | 2.0 GB | 2.1 GB |
| Transition peak | 4.6 GB | 3.4 GB | 3.6 GB |

visual品質、correctness、battery/thermalへの副作用も併記します。

## 73. A/B実験

runtime toggleでfeatureを切り、同じcamera pathで比較します。

- shadow off/on。
- VFX tier。
- enemy AI update frequency。
- post process。
- resolution scale。
- UI panel。

複数設定を一度に変えず、因果を絞ります。

## 74. Camera replay

手動操作差を減らすため、deterministic input replay、camera rail、fixed encounter seed等を用意します。

完全deterministicでなくても、enemy数、camera angle、durationを揃えるだけで比較精度が上がります。

## 75. Thermal profiling

mobile/handheldでは起動直後60 FPSでも10分後にthermal throttlingします。

- cold run/warm run。
- 15〜30分soak。
- device temperature/clock。
- battery/charging。
- sustained performance mode。

短いProfiler captureだけで長時間性能を保証しません。

## 76. Memory soak test

```text
Title → Lobby → Battle → Result → Lobby
× 50〜100 loop
```

loopごとに:

- managed heap。
- native used/reserved。
- graphics memory。
- object count。
- Addressables leases。
- pool count。
- handle count。

を記録し、plateauか単調増加かを見ます。

## 77. Performance regression gate

CI/automated device testで:

- median/p95/p99 frame time。
- GC Alloc。
- memory peak。
- load time。
- draw count。

をbaselineと比較します。noiseを考慮したthreshold、同一device pool、warm-up、複数runが必要です。

## 78. 誤った結論

### FPSが60なので余裕

VSyncで60に張り付いているだけかもしれません。active timeを見る。

### Main Threadのwaitが長いのでMain Threadが遅い

GPU/render thread待ちかもしれません。全thread/GPUを見る。

### Snapshot Bが大きいのでleak

異なるScene/cache/pool状態かもしれません。同logical stateでloop比較する。

### Draw callが多いからGPU bound

draw submissionはCPU側、shader/fillはGPU側です。timingで分類する。

### GC Alloc 0なのでmemory問題なし

native/GPU/pooled resident memoryが増えている可能性があります。

## 79. CPU調査checklist

- target Playerでcaptureしたか。
- target frame budgetを設定したか。
- waiting timeとactive workを分けたか。
- TimelineでMain/Render/Workersを見たか。
- HierarchyのSelf/Calls/GC Allocを見たか。
- custom markerを適切な粒度で入れたか。
- spikeとsteady stateを別々に見たか。
- p95/p99/worstを記録したか。

## 80. GPU/Rendering調査checklist

- GPU timingがsupported/validか。
- resolution diagnosticを試したか。
- Frame Debuggerでpass/draw順を見たか。
- batching reasonを確認したか。
- shadow/transparent/postのcostを分けたか。
- RenderTexture size/lifetimeを見たか。
- platform vendor toolでhardware counterを見たか。
- visual regressionを確認したか。

## 81. Memory調査checklist

- built-in trendとpackage snapshotを使い分けたか。
-同じlogical stateで比較したか。
- 複数loopで増加が継続するか。
- managed/native/graphicsを区別したか。
- reference chain/root ownerを追ったか。
- pool/cacheを意図的residentとして分類したか。
- transition peakを測ったか。
- Addressables/NativeArrayの明示解放を確認したか。

## 82. 学習確認問題

1. FPSよりframe timeを見る理由は何か。
2. `WaitForTargetFPS`が長い時、最適化が必要とは限らない理由は何か。
3. HierarchyのTotalとSelfは何が違うか。
4. ProfilerMarkerをstatic readonlyにする理由は何か。
5. Frame DebuggerとGPU Profilerの役割はどう違うか。
6. Snapshot BがAより大きいだけでleakと断定できない理由は何か。
7. managed wrapperとnative Unity Objectの関係は何か。
8. pool/cacheのmemory増加をどう評価するか。
9. Development Buildの数値をrelease性能と同一視できない理由は何か。
10. performance fix後にcorrectness/visual testが必要な理由は何か。

## 83. 公式資料

- [Unity Manual: Profiler window](https://docs.unity3d.com/6000.0/Documentation/Manual/ProfilerWindow.html)
- [Unity Manual: CPU Usage Profiler module](https://docs.unity3d.com/6000.0/Documentation/Manual/ProfilerCPU.html)
- [Unity Manual: Highlights Profiler module](https://docs.unity3d.com/6000.0/Documentation/Manual/ProfilerHighlights.html)
- [Unity Manual: Profile on target devices](https://docs.unity3d.com/6000.0/Documentation/Manual/profiling-target-device.html)
- [Unity Scripting API: ProfilerMarker](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/Unity.Profiling.ProfilerMarker.html)
- [Unity Scripting API: ProfilerRecorder](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/Unity.Profiling.ProfilerRecorder.html)
- [Unity Manual: Frame Debugger](https://docs.unity3d.com/6000.0/Documentation/Manual/FrameDebugger-landing.html)
- [Unity Manual: Debug a frame](https://docs.unity3d.com/6000.0/Documentation/Manual/FrameDebugger-debug.html)
- [Unity Manual: Memory Profiler package](https://docs.unity3d.com/6000.0/Documentation/Manual/com.unity.memoryprofiler.html)
- [Memory Profiler 1.1 Manual](https://docs.unity3d.com/Packages/com.unity.memoryprofiler@1.1/manual/index.html)
- [Unity Manual: Memory performance data](https://docs.unity3d.com/6000.0/Documentation/Manual/profiler-memory.html)
- [Unity Manual: Graphics performance and profiling](https://docs.unity3d.com/6000.0/Documentation/Manual/profiling-landing.html)

## 84. まとめ

- performanceはFPSの印象ではなくframe time、percentile、budgetで扱う。
- CPU/GPU/Present limitedを分類してから担当toolへ進む。
- CPU Timelineはthreadと待機関係、HierarchyはTotal/Self/Calls/GC Allocを見る。
- custom `ProfilerMarker`とcounterでgame domainのcostを可視化する。
- Frame Debuggerは一frameのrender event/state、GPU ProfilerはGPU時間を調べる。
- Memory Profilerは同じlogical stateのsnapshotを複数loopで比較し、reference rootを追う。
- Editor/Development/Release、cold/warm、短時間/soakを分けて計測する。
- 一変更ごとに再計測し、速度だけでなく正しさ、画質、memory peakを確認する。
