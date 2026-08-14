# Mobile・Console・PC最適化

> 対象: Unity 6.0。最適化とは画質を闇雲に下げることではなく、対象device上のframe time、memory、I/O、電力予算を守りながら意図した体験を維持する作業です。

## 1. 最適化の正しいloop

```text
目標とbudgetを決める
→ target実機で計測
→ bottleneckを一つ特定
→ 仮説を立てる
→ 小さく変更
→ 同条件で再計測
→ 品質・正しさを確認
```

Profilerを見ずに「Updateを減らす」「全部poolする」と決めるのは最適化ではありません。

## 2. Frame budget

```text
30 fps ≒ 33.33 ms/frame
60 fps ≒ 16.67 ms/frame
90 fps ≒ 11.11 ms/frame
120 fps ≒  8.33 ms/frame
```

CPUとGPUが並行していても、遅い側がframe rateを制限します。60fps目標でCPU 8ms、GPU 22msならGPU boundです。

## 3. 平均だけを見ない

平均16msでも、数秒ごとに80msなら操作感は悪化します。

- median。
- 95th/99th percentile。
- worst frame。
- hitch頻度。
- 連続してbudget超過した時間。

を見ます。gameplayの同じ区間を繰り返し計測します。

## 4. Target deviceで測る

EditorはInspector、Scene View、asset管理などの影響を受けます。Unity公式も最終target上のDevelopment BuildへProfilerを接続する方法を案内しています。

```text
実機Development Buildで問題を発見
→ Editorで素早く仮説検証
→ 実機buildで再確認
→ Release相当buildで最終確認
```

Deep Profilingは便利ですがoverheadが大きく、通常性能そのものと誤認しません。

## 5. Device matrix

「Android」「PC」を一機種として扱いません。

| Tier | CPU | GPU | Memory | Storage | 目的 |
|---|---|---|---|---|---|
| Minimum | 下限 | 下限 | 下限 | 遅い | 動作保証 |
| Typical | 多数派 | 多数派 | 標準 | 標準 | 主調整 |
| High | 高性能 | 高性能 | 多い | 高速 | 高品質 |

OS version、driver、thermal状態、解像度、refresh rateも記録します。

## 6. Performance budget表

```text
Target: 60fps / 1080p internal
Main Thread:        8.0ms
Render Thread:      5.0ms
GPU:               14.0ms
Managed Heap:     300MB
Total Resident:  2500MB
Streaming hitch:   <4ms
Gameplay GC Alloc:  0B/frame goal
```

値はproject/deviceごとに決めます。CPUとGPUへ16.67msを丸ごと別々に割り当てないようpipeline overlapを理解します。

## 7. CPU boundの兆候

- Main ThreadのScript/Physics/Animationが長い。
- Render Threadがdraw submissionで長い。
- GPUが待っている。
- 解像度を下げてもfpsが変わらない。

Timelineでthread間の依存と待ちを見ます。Hierarchyの合計だけでは並行関係が分かりません。

## 8. GPU boundの兆候

- GPU frame timeがbudget超過。
- 解像度低下で明確に改善。
- fill rate、shadow、post effect、透明物が重い。
- CPU側にGfx.WaitForPresent等の待ちが見える。

CPUの待ちsample自体をCPU処理の原因と誤認せず、GPU profiler/frame debugger/vendor toolへ進みます。

## 9. Memory boundとI/O bound

Memory不足はGCだけではありません。

- texture/mesh/audio native memory。
- managed heap。
- graphics driver/resource。
- AssetBundle/Addressables cache。
- temporary render targets。
- OSによるkill。

I/Oではasset load、shader cache、save、decompressionがhitch原因になります。

## 10. Profiler tools

- Unity Profiler: CPU、GPU、Rendering、Memory、Audio、Physics等。
- Profile Analyzer: 複数frame/capture比較。
- Memory Profiler: snapshotと参照関係。
- Frame Debugger: draw順とpass。
- RenderDoc/Xcode GPU tools/vendor profiler: GPU詳細。
- Build Report: build内容とasset size。

一つのtoolだけで全原因は分かりません。

## 11. ProfilerMarker

```csharp
using Unity.Profiling;

public sealed class CombatSimulation
{
    private static readonly ProfilerMarker TickMarker =
        new("Game.CombatSimulation.Tick");

    public void Tick(float deltaTime)
    {
        using (TickMarker.Auto())
        {
            // 自作systemを意味単位で囲む。objectごとに細か過ぎるmarkerはoverheadにもなる。
            UpdateActions(deltaTime);
            ResolveHits();
        }
    }
}
```

Deep Profileなしでも重要処理を追跡できます。

## 12. GC Allocation

短命managed allocationが蓄積するとGC pauseを起こします。

よくある発生源:

- 毎frameの`new List/Dictionary/array`。
- string連結/format。
- LINQ iterator。
- closureを作るlambda。
- boxing。
- APIが返す新配列。
- Coroutine/yield objectの使い方。

ProfilerのGC Alloc columnでcall siteを確認します。

## 13. Zero allocationが目的ではない

loading画面や低頻度editor toolまで複雑化して0Bにする必要はありません。優先は:

- 毎framehot path。
- 戦闘peak。
- mobile memory pressure。
- latency-sensitive処理。

可読性とbug riskを含めて判断します。

## 14. Collection再利用

```csharp
private readonly List<Enemy> nearbyEnemies = new(64);

private void GatherEnemies()
{
    nearbyEnemies.Clear(); // capacityは保持され、通常は再allocationしない。
    spatialIndex.Query(transform.position, 10f, nearbyEnemies);
}
```

上限不明のListを永久再利用するとpeak capacityを保持します。Scene切替やpool縮小時にmemory方針を決めます。

## 15. Physics最適化

- Layer Collision Matrixで不要collisionを切る。
- query LayerMaskを狭くする。
- NonAlloc queryと適切なbuffer。
- fixed timestepを必要以上に細かくしない。
- moving static Colliderを避ける。
- MeshCollider形状とconvexを見直す。
- solver iterationを要件に合わせる。
- 大量rayはJob/batchを検討。

判定を削ってgameplayを変えないようtestします。

## 16. Fixed timestepのspiral

一frameが遅れると追いつくため複数FixedUpdateが走り、さらに遅くなる場合があります。

```text
long frame
→ physics stepが複数必要
→ CPU負荷増
→ さらにlong frame
```

Maximum Allowed Timestep、simulation設計、負荷peakを確認します。fixedDeltaTimeを大きくすると軽くなる代わりにsimulation精度が下がります。

## 17. Animation CPU

- Animator数。
- bone数。
- layer/IK/constraint。
- skinning方式。
- off-screen更新。
- animation compression。
- StateMachineBehaviour/callback。

遠距離AIはAnimator culling/LODを使いますが、Root Motionやeventを止めてgameplayが変わらないか確認します。

## 18. AI CPU

第30章のperception、decision、path queryを:

- 距離別頻度。
- frame分散。
- spatial partition。
- AI LOD。
- active combat優先。

で制御します。100体を同じframeに思考させません。

## 19. Rendering pipeline選択

URP/HDRP/Built-inの選択はplatformと品質目標に影響します。pipeline変更はshader、lighting、toolingを巻き込むため後半の小最適化として行いません。

URPでもrendering path、HDR、depth/opaque texture、shadow、MSAA等の設定がCPU/GPU/memoryを変えます。使用機能だけ有効にします。

## 20. Draw callとBatching

draw call costはCPU側submissionとGPU state changeに影響します。

- SRP Batcher互換shader。
- GPU Instancing。
- static batching。
- dynamic batchingの制約。
- material variation削減。
- mesh結合のtrade-off。

meshを巨大結合するとculling粒度、memory、lightmap、streamingが悪化する場合があります。

## 21. SetPass・Material

同じmesh数よりmaterial/shader pass切替が問題になる場合があります。

```csharp
// renderer.materialはinstance materialを生成し得る。
// 共有変更ならsharedMaterial、object別parameterならMaterialPropertyBlockを検討する。
private readonly MaterialPropertyBlock block = new();

private void SetHitFlash(Renderer renderer, float amount)
{
    renderer.GetPropertyBlock(block);
    block.SetFloat(HitFlashId, amount);
    renderer.SetPropertyBlock(block);
}
```

SRP Batcher/instancingとの相互作用を実測します。

## 22. Overdraw

mobile GPUでは透明particle、UI、草、full-screen effectによるoverdrawが重くなりやすいです。

- 画面を覆う半透明layer数を減らす。
- particle quadの空白面積を減らす。
- 不透明にできる物はopaque/alpha clipを比較。
- UI panel重なりを確認。
- effectを低解像度bufferへ。

alpha clipもGPU/architecture次第でcostがあるため計測します。

## 23. Fill rateと解像度

pixel shader costは概ね描画pixel数に比例します。高DPI mobileでnative resolutionを常に描く必要があるとは限りません。

- render scale。
- Dynamic Resolution。
- upscaling。
- effect buffer half/quarter resolution。
- UIはnative、3Dだけ低解像度。

文字・細線・motion時の品質を実機で確認します。

## 24. Dynamic Resolution controller

```csharp
public sealed class ResolutionBudgetController
{
    private float scale = 1f;

    public float Update(float gpuMilliseconds, float targetMilliseconds)
    {
        // hysteresisを持たせ、毎frame上下して画質がちらつくのを防ぐ。
        if (gpuMilliseconds > targetMilliseconds * 1.08f)
            scale -= 0.05f;
        else if (gpuMilliseconds < targetMilliseconds * 0.85f)
            scale += 0.02f;

        return scale = Mathf.Clamp(scale, 0.6f, 1f);
    }
}
```

実際はGPU timingの遅延、移動平均、platform対応、UI分離を扱います。

## 25. Shadow

影は:

- shadow map resolution/memory。
- cascade数。
- shadow distance。
- caster数。
- additional light shadow。
- soft shadow sampling。

へ影響します。camera付近と戦闘targetを優先し、遠景はbaked/probe/blob shadow等を検討します。

## 26. Lighting

- baked lightmap。
- Light Probe。
- Reflection Probe。
- mixed/realtime light。
- additional lights per object/pixel。
- HDR。

動的action characterと静的environmentを同じ方法で照らす必要はありません。build size、load memory、runtime costのtrade-offを測ります。

## 27. Shader Variant

keyword組合せでvariant数、build時間、memory、起動時compile hitchが増えます。

- 未使用feature/keywordをstrip。
- global/local keywordを整理。
- runtimeで必要variantを収集/warmup。
- platform別shaderを分け過ぎない。
- variant log/build reportを監視。

stripし過ぎるとdevice上でpink/missingになります。代表buildで検証します。

## 28. Texture memory

概算は圧縮形式、mipmap、platformで変わります。source PNGのfile sizeはruntime GPU memoryではありません。

- platform適切なcompression。
- Max Size。
- mipmap。
- streaming mipmap。
- normal map format。
- alpha不要textureからalpha削除。
- Read/Write Enabledを不要ならoff。
- atlasの余白と巨大化。

Memory Profilerとplatform GPU toolで実値を確認します。

## 29. Mipmap

mipmapはmemoryを増やす一方、遠距離sampling品質とcache効率を改善します。3D textureでは通常有効、常に一定pixel表示のUIでは不要な場合があります。

streamingではcamera/priorityに応じ必要mipだけresidentにしますが、急camera移動のblur、I/O、budget設定をtestします。

## 30. Mesh memory

- vertex数だけでなくattribute数。
- index format 16/32bit。
- bone weight。
- blend shape。
- Read/Write Enabled。
- mesh compression。
- LOD mesh。

unused tangent/color/UV channelも帯域とmemoryを使います。asset import pipelineで検証します。

## 31. LOD Group

LODはtriangleを減らすだけでなくshader、material、bone、shadow casterも簡略化できます。

- 切替時のpopping。
- cross-fade cost。
- silhouette保持。
- gameplay hurtboxとの独立。
- camera FOV/解像度差。

を確認します。Bossの重要telegraphまで消しません。

## 32. Occlusion Culling

遮蔽物の多い室内では有効ですが、開けたarenaではbake data/CPU overheadに見合わない場合があります。

dynamic object、cell size、smallest occluder、camera高速移動、additive Sceneとの相性を測ります。frustum cullingは別途常に重要です。

## 33. Particle・VFX

- 最大particle数。
- emission peak。
- transparent overdraw。
- collision。
- light付与。
- trail geometry。
- VFX Graph buffer。
- off-screen simulation。

戦闘VFXは画面が最も忙しい時にpeakが重なります。単体previewでなく最悪の同時発生を測ります。

## 34. Audio

- decompress on load / compressed in memory / streaming。
- sample rate/channel。
- simultaneous voice数。
- spatialization/effect。
- AudioMixer DSP。

短いSEをstreamingするとI/O overhead、長いBGMを全展開するとmemoryを消費します。clip用途別presetを作ります。

## 35. Asset Loading

同期loadを戦闘中に行うとhitchします。

```text
予測して非同期load
→ dependency完了待ち
→ 必要なら一frameのinstantiate量を分散
→ 使用開始
→ reference解放
→ 安全な地点でunused cleanup
```

Addressables handleを早くreleaseし過ぎない・解放し忘れないownershipを設計します。

## 36. Instantiateのhitch

asset load済みでもInstantiateはGameObject/component生成、Awake/OnEnable、Animator/Physics登録で重くなります。

- pool。
- preload。
- 一frame生成数budget。
- prefab component削減。
- activationを段階化。

大量poolは常駐memoryを増やすためpeak同時数からcapacityを決めます。

## 37. Scene Streaming

大Scene一括loadを避け、additive SceneやAddressablesでregionを分割できます。

- player移動予測。
- preload距離。
- dependency共有。
- lightmap/probe/navmesh。
- unload安全性。
- teleport時fallback。

Scene境界を跨ぐ参照を直接持ち過ぎるとunloadできません。

## 38. SaveとStorage

main threadで大きなserialize/writeを行わず、snapshotを作ってbackground I/Oへ渡します。ただしUnityEngine.Objectをworker threadで触りません。

mobileではflash寿命・書込頻度、consoleではplatform save API・quota・user profile、PCでは権限/cloud conflictを扱います。

## 39. Threading・Job・Burst

並列化向き:

- 大量独立計算。
- animation/AI補助計算。
- raycast batch。
- data変換。

不向き:

- 少量処理。
- GameObject API中心。
- dependency/synchronizationが多い処理。

Jobをscheduleして即`Complete`すると並列効果が薄い場合があります。Main Thread待ちをTimelineで確認します。

## 40. Mobile固有: Thermal

mobileは開始直後60fpsでも数分後にthermal throttlingします。

- 15～30分以上の連続test。
- battery/充電状態。
- ambient temperature。
- device case。
- CPU/GPU level変化。

を記録します。短いbenchmarkだけでは持続性能が分かりません。

## 41. Adaptive Performance

device thermal/power情報に応じて段階的に:

- render scale。
- shadow distance。
- effect密度。
- target frame rate。
- AI/animation LOD。

を調整できます。突然全品質を落とさずhysteresisと回復条件を持たせます。対応provider/deviceを確認します。

## 42. Battery

常に最大fps、最大GPU clock、GPS/sensor/network pollingを行うとbatteryを消費します。

- menu/background時fps低下。
- device refreshに合うtarget。
- 不要sensor無効。
- network batch。
- OLED等を含む画面輝度はappだけで完全制御しない。

発熱とbatteryはperformance requirementの一部です。

## 43. Mobile input/UI

- safe area/notch。
- aspect ratio。
- touch target size。
- multi-touch。
- gesture conflict。
- virtual stickの指追従。
- controller接続切替。

UI Canvas rebuildやraycast target過多もCPU costになります。画面解像度別に実機確認します。

## 44. Mobile lifecycle

background、focus loss、OS kill、audio interruption、permission変更を扱います。

```csharp
private void OnApplicationPause(bool paused)
{
    if (paused)
    {
        // 小さいcritical stateを安全に保存要求する。
        // 長時間処理が完了できるとは仮定しない。
        saveCoordinator.RequestUrgentSave();
    }
}
```

callback順と保証範囲はplatformで確認します。

## 45. Console固有

console SDK、certification、TRC/TCR/XR等の詳細は各platform holderの機密資料・契約に従います。一般原則:

- suspend/resume。
- controller disconnect/reassignment。
- user sign-in/out。
- storage/full/error。
- network service loss。
- safe area。
- required system UI。
- fixed hardwareでの安定frame pacing。

公開資料で推測せず、正式developer documentationを使います。

## 46. Console memory

fixed memory budgetへ:

- engine/runtime。
- executable/code。
- graphics resource。
- audio。
- animation。
- streaming cache。
- transient peak。

を割り当てます。平均residentだけでなくScene遷移中の旧新asset同時保持peakを測ります。

## 47. PC固有: Hardware幅

PCはCPU core数、GPU vendor、VRAM、RAM、storage、driver、resolutionが広いです。

- minimum/recommended specを実機検証。
- GPU vendor別。
- integrated GPU。
- HDD/SATA SSD/NVMe。
- 16:9/ultrawide/multi-monitor。
- 60～高refresh。
- laptop power mode。

開発機だけを推奨環境としません。

## 48. PC Graphics Settings

個別に調整可能にします。

- resolution/window/fullscreen。
- VSync/frame cap。
- render scale/upscaler。
- texture quality。
- shadow。
- effects/post processing。
- anti-aliasing。
- foliage/crowd/view distance。

Low/Medium/High presetは個別値の集合であり、変更後のcustom状態を保持します。

## 49. Frame pacing

平均60fpsでもframeが10ms、23msと交互なら滑らかに見えません。

- VSync。
- targetFrameRate。
- display refresh rate。
- present mode。
- CPU/GPU queue。
- background process。

を確認します。fps counterだけでなくframe time graphを見ます。

## 50. Platform dependent compilation

```csharp
#if UNITY_ANDROID
    // Android buildだけにcompileされる実装。
    ConfigureAndroidBackend();
#elif UNITY_IOS
    ConfigureAppleMobileBackend();
#elif UNITY_STANDALONE_WIN
    ConfigureWindowsBackend();
#endif
```

runtimeで変える必要がなければcompile defineにより不要code/dependencyを除けます。共通domainを`#if`だらけにせずplatform adapterへ閉じ込めます。

## 51. RuntimePlatformとの違い

`Application.platform`はruntime分岐、platform defineはcompile時分岐です。Editor上でdevice挙動を模倣する場合や同一build内の環境差はruntime設定、native plugin/API差はcompile境界が適します。

## 52. Quality Tier設計

```csharp
public readonly record struct RuntimeQuality(
    float RenderScale,
    float ShadowDistance,
    int ParticleBudget,
    int MaxActiveEnemies,
    int TargetFrameRate);
```

Project Quality Settingsだけでなくgame固有budgetも一つのprofileへまとめます。gameplay difficultyをperformance tierで変えないよう、敵数削減時の設計に注意します。

## 53. Automatic quality

初回benchmarkでpresetを提案しても、userが変更できるようにします。device model名のhardcodeだけで判定せず:

- GPU/CPU capability。
- memory。
- resolution。
- 短いcalibration。
- 持続thermal情報。

を組み合わせます。誤判定に備え安全なdefaultを持ちます。

## 54. Build Profile

Unity 6のBuild Profilesでplatform/configuration別設定を管理します。

```text
Android Low Test
Android Release
iOS Release
Windows Development
Windows Release
Dedicated Server
```

define、Scene、Player Settings override、Development/Profiler設定の混線を防ぎます。設定はversion control対象にします。

## 55. IL2CPPとManaged Stripping

IL2CPP/AOTではreflection、generic、dynamic codeがEditor/Monoと異なる問題を起こし得ます。Managed Strippingを上げるとsizeを削減できますが、reflection経由型が除去される場合があります。

- target buildでtest。
- `link.xml`/preserveを最小限。
- Addressables type。
- serializer。
- generic instantiation。
- native plugin architecture。

を確認します。

## 56. Startup time

起動を区間分けします。

```text
process start
→ engine初期化
→ first Scene
→ service/auth
→ shader/resource warmup
→ title操作可能
→ gameplay可能
```

全部終わるまでblack screenにせず、依存を分けて必要なものを先にloadします。ただし利用直前hitchへ移しただけにならないよう計測します。

## 57. Loading画面

progress値は実際の複数taskをweight付きで集約します。

```text
Scene load 50%
Addressables 25%
Network ready 15%
Shader/prewarm 10%
```

`AsyncOperation.progress`一個だけで全準備完了とは限りません。activationの大きなhitchも別計測します。

## 58. Optimization regression

継続的に代表sceneを自動実行し:

- frame time percentile。
- memory peak。
- load time。
- build size。
- GC allocation。

を保存します。hardware/OS/driverが違う結果を無条件比較せず、同一lab deviceでtrendを取ります。

## 59. 性能変更の品質確認

- shadow削減で攻撃予兆が読めるか。
- particle削減で命中方向が分かるか。
- AI LODで遠距離敵が不正停止しないか。
- animation cullingでeventが失われないか。
- resolution低下でUI文字が読めるか。
- audio voice制限でparry音が消えないか。

性能合格でもgameplay/accessibilityを壊したら完成ではありません。

## 60. よくある失敗

### Editorだけで測る

target deviceのCPU/GPU/memory/driver差が見えない。実buildで測る。

### 平均fpsだけを見る

hitchを隠す。percentileとframe time graphを見る。

### GCだけをmemory問題と呼ぶ

native/GPU/asset memoryを見落とす。Memory Profiler等で分類する。

### Draw callだけ減らす

結合でculling/streamingが悪化する。CPU/GPU双方を再計測する。

### Mobileを短時間test

thermal throttlingが出ない。長時間loopする。

### 全platform同じ品質

hardware/電力/入力/画面差へ対応できない。profileとtierを作る。

### 最適化で処理順を変更

戦闘判定・network determinismを壊す。testとreplayで確認する。

## 61. 実装・改善順

1. platform別target fpsとmemory budget。
2. minimum/typical/high実機を確保。
3. worst-case gameplay capture。
4. CPU/GPU/Memory/I/O bound分類。
5. 最大bottleneckを修正。
6. rendering/asset import preset。
7. streaming/pool/LOD。
8. mobile thermal・console lifecycle・PC settings。
9. automated regression capture。
10. release相当buildで長時間test。

## 62. 完成確認表

- [ ] target deviceごとのfps/memory budgetがある。
- [ ] Editorでなく実機Development Buildを計測した。
- [ ] CPU/GPU boundを区別した。
- [ ] averageと95/99 percentileを記録した。
- [ ] gameplay hot pathのGC Allocを確認した。
- [ ] texture/mesh/audioのplatform import presetがある。
- [ ] render scale/shadow/particle budgetをtier化した。
- [ ] worst-case戦闘VFXと敵数で測った。
- [ ] Scene遷移時memory peakを測った。
- [ ] mobileをthermal throttlingまで連続testした。
- [ ] suspend/resume/controller/storage errorをtestした。
- [ ] PC minimum specと複数resolutionをtestした。
- [ ] IL2CPP/strippingのrelease相当buildをtestした。
- [ ] 最適化後にgameplayと視認性を再確認した。

## 63. 確認問題

1. 60fpsのframe budgetは何msか。
2. 解像度を下げてもfpsが変わらない場合、何boundの可能性が高いか。
3. Editor計測だけでは不十分な理由は何か。
4. `Gfx.WaitForPresent`をCPU処理原因と即断できない理由は何か。
5. Draw call削減のための巨大mesh結合には何の欠点があるか。
6. source PNGのfile sizeがruntime texture memoryでない理由は何か。
7. Dynamic Resolutionにhysteresisが必要な理由は何か。
8. mobile性能を長時間測る理由は何か。
9. `RuntimePlatform`とplatform defineの違いは何か。
10. 最適化後に視認性・gameplay testが必要な理由は何か。

## 64. 公式資料

- [Unity 6 Target Device Profiling](https://docs.unity3d.com/6000.0/Documentation/Manual/profiling-target-device.html)
- [Unity 6 CPU Profiler](https://docs.unity3d.com/6000.0/Documentation/Manual/ProfilerCPU.html)
- [Unity 6 URP Performance](https://docs.unity3d.com/6000.0/Documentation/Manual/urp/configure-for-better-performance.html)
- [Unity 6 Build Profiles](https://docs.unity3d.com/6000.0/Documentation/Manual/BuildSettings.html)
- [Unity 6 RuntimePlatform](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/RuntimePlatform.html)
- [Unity Memory Profiler package](https://docs.unity3d.com/6000.0/Documentation/Manual/com.unity.memoryprofiler.html)

Console固有要件は各platform holderの正式developer portalと最新SDK資料を参照してください。
