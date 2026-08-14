# DirectX 12 第15章：Multithread Command Recording

この章では、複数CPU ThreadでDirectX 12 Command Listを安全に記録し、正しい順序でGPUへ提出する設計を学びます。Job System、Allocator/List Pool、Frame Context、Pass/Draw分割、Barrier、Descriptor、Upload、負荷分散、再現性、性能計測まで扱います。

## 1. 到達目標

- GPU実行とCPU Command Recordingを区別する。
- ThreadごとのCommand Allocator/Listを安全に管理する。
- Pass依存と提出順を保ちながら並列記録する。
- Descriptor/Upload/State TrackerのData Raceを防ぐ。
- 並列化の効果をCPU/GPU双方から計測する。

## 2. なぜ並列記録するのか

Draw数が多いFrameでは、State設定、Descriptor割当、可視Object処理、Command EncodingがMain Threadの制限になります。独立部分を複数Coreへ分散します。

## 3. 並列記録がGPUを並列化するとは限らない

複数Threadが作ったDirect Command Listも、同じQueueへ提出すればQueue順に実行されます。CPU Recording並列性とGPU Queue並列性は別概念です。

## 4. 基本構成

```text
main thread: build frame/pass plan
    -> dispatch recording jobs
worker 0..N: reset allocator/list, record, close
    -> collect completed lists
main/submission thread: order lists, execute, signal fence
```

## 5. Thread Safetyの原則

同じCommand ListやCommand Allocatorを複数Threadから同時操作しません。Deviceの作成APIがThread-safeでも、個々のObjectの利用規則を確認します。

## 6. Command Allocator Ownership

一つのAllocatorは同時に一つのRecording作業へ所有させます。GPUがそのAllocator由来Commandを実行中ならResetできません。

## 7. Command List Ownership

Recording中のListは単一Threadが所有します。Close後は提出側へOwnershipを移し、ResetされるまでWorkerは触りません。

## 8. Frame Context

```cpp
struct FrameContext
{
    std::vector<Microsoft::WRL::ComPtr<ID3D12CommandAllocator>> allocators;
    UploadArena upload;
    DescriptorArena descriptors;
    UINT64 fenceValue = 0;
};
```

Back Buffer数ではなく同時進行Frame数と対応させます。

## 9. Frame開始

Frame ContextのFence完了を確認してからAllocator、Upload Arena、Transient Descriptor ArenaをResetします。

## 10. Fence待機を減らす

毎Frame全GPU待機せず、再利用対象Frame Contextだけを待ちます。CPUがGPUより進み過ぎない数のContextを用意します。

## 11. Allocator Pool

Frame×Worker×Queue Typeまたは必要数のAllocatorを持ちます。使用中、記録中、再利用可能を明示します。

## 12. Command List Pool

Command Listは作成Costを避けて再利用します。Allocatorを指定してResetし、PSO初期値も必要なら渡します。

## 13. Reset順序

AllocatorをResetし、そのAllocatorでCommand ListをResetします。GPU完了前のAllocator Resetは重大な誤りです。

## 14. Close

記録完了したListをCloseし、HRESULTを検査します。Close失敗を無視して提出しません。

## 15. Queue Typeを一致させる

Direct/Compute/Copy Command Listと対応Allocator/Queueを一致させます。実行可能なCommand種類も異なります。

## 16. Job System

小さな作業をQueueへ投入しWorkerが取得します。Renderer専用Threadを固定する方式と汎用Job Systemを共有する方式があります。

## 17. Job Data

Jobへ必要な読み取り専用Snapshotと専用出力領域を渡します。Gameplay Objectへの可変Pointerを直接渡しません。

## 18. Recording Job例

```cpp
struct RecordJob
{
    PassId pass;
    std::span<const DrawPacket> packets;
    CommandListSlot slot;
    FrameIndex frame;
    SortRange range;
};
```

Jobが書ける領域と完成通知を明確にします。

## 19. Draw Packet

PSO、Root Data、VB/IB、Draw引数、Material/Instance参照等をRender前段で不変Dataへ変換したものです。

## 20. Snapshotの利点

Recording中にGameplay ThreadがObjectを削除/変更しても影響されません。Frame境界で一貫した描画状態を得られます。

## 21. 分割単位

Pass単位、Material Bucket単位、Draw Range単位、Shadow Cascade単位等があります。依存と負荷に合わせます。

## 22. Pass単位分割

Shadow、Depth、Opaque等を別Jobにできます。ただし前Pass結果やBarrierの依存を保ちます。

## 23. Draw Range分割

一つの重いPassを複数Rangeへ分けます。各Listの固定Setup Commandが重複するCostがあります。

## 24. 粒度が細か過ぎる場合

Job Queue、Allocator/List、Close/Execute、State再設定のOverheadがDraw処理より大きくなります。

## 25. 粒度が粗過ぎる場合

一つのJobだけ長く残り他CoreがIdleになるTail Problemが発生します。

## 26. Chunk Size

固定Draw数、推定Cost、過去Frame時間等でChunk化します。Characterと単純Propを同一Draw数Costと見なしません。

## 27. Work Stealing

Workerが自分のQueueを終えたら他QueueからJobを取得し、負荷偏りを減らします。順序依存Jobには適用条件があります。

## 28. Job Dependency

Pass BがPass AのCPU生成結果を必要とするならDependency Counter/Event等で開始条件を表します。

## 29. GPU依存との違い

CPU Job完成はGPU処理完成ではありません。Command記録依存、Queue提出依存、GPU Fence依存を区別します。

## 30. Command List提出順

`ExecuteCommandLists`へ渡した配列順とQueueへの呼出順がGPU実行順を形成します。Worker完成順のまま無秩序に提出しません。

## 31. Stable Pass Order

Pass Graphから決定した順序へListを配置します。Job完成時刻は描画順ではありません。

## 32. List内Stateは独立

Command List開始時に必要なPSO、Root Signature、Heap、Viewport、Scissor等を明示します。他List末尾Stateへ暗黙依存しません。

## 33. Descriptor Heap Bind

Shader-visible Heapは各Command Listで設定します。別Listで設定済みだから省略できると仮定しません。

## 34. Root Parameter

Root Signature設定後、必要なRoot ParameterをList内で設定します。PSO/Root Signature切替による無効化を意識します。

## 35. Resource State Trackerの難しさ

複数Listが同じResourceの開始/終了Stateを独立推測すると矛盾します。Global State確定とLocal Trackingを分離します。

## 36. Local State Tracker

各Recording JobはList内の遷移を追跡し、開始時に要求するStateと終了Stateを記録します。

## 37. Global State統合

提出順が確定した段階で各Listの開始要求と前List終了Stateを照合し、必要なBarrierを解決します。

## 38. Prologue Command List

並列記録Listの前に必要なPending Transitionを小さなPrologue Listへ記録する方式があります。

## 39. Pass境界Barrier

Pass Graph構築時にResource Accessを宣言し、中央SchedulerがBarrierを生成すればWorker間のState共有を減らせます。

## 40. UAV Barrier

同Stateでも書込み順序が必要な場合があります。PassのRead/Write宣言からUAV依存を導きます。

## 41. Aliasing Barrier

Transient ResourceのMemory再利用境界は中央Allocator/Graphが管理します。各Draw Jobが勝手に判断しません。

## 42. Split Barrier

BEGIN/ENDを別Listへ配置する場合は提出順とQueue制約を厳密に管理します。まず通常Barrierで正しく作ります。

## 43. Descriptor Allocation

複数Threadが一つのOffsetをAtomic加算する方式、WorkerごとのRangeを事前配布する方式等があります。

## 44. Worker別Descriptor Range

競合を減らせますが余りが発生します。Frame開始時に予算配布し、不足時のFallbackを定義します。

## 45. Descriptor Overflow

上限を超えて隣Frame領域へ書かず、明示的に失敗、追加Page、低優先Draw省略等のPolicyを使います。

## 46. PersistentとTransient

Texture等の長寿命DescriptorとFrameごとのTableを別Allocatorへ分けます。永続IndexをFrame Resetで失効させません。

## 47. Descriptor Copy

CPU HeapからShader-visible Heapへ並列Copyする場合、Destination Rangeが重複しないことを保証します。

## 48. Upload Allocation

Constant/Instance Data用Upload RingもWorkerごとのSub-rangeまたはAtomic Allocationで割り当てます。

## 49. Alignment

CBV 256 byte、Copy Footprint等のAlignmentをAllocator内部で処理します。Atomic加算前後のPaddingも容量計算へ含めます。

## 50. Upload Data Race

同じAddressへ複数Jobが書かず、GPU使用中Frame領域へCPUが上書きしないようFenceとRange Ownershipを守ります。

## 51. Dynamic Constant

Draw Packet準備段階で書くかRecording Job内で書くかを統一します。二重AllocationとCache Missを測ります。

## 52. Read-only Cache

Mesh/Material/PSO RegistryはRecording中に不変にします。Hot ReloadはFrame境界でVersionを切替えます。

## 53. PSO Cache Thread Safety

Recording中にPSOが欠けて作成するとLock/Driver Costが発生します。可能なら事前生成し、Fallback PSOを用意します。

## 54. Lazy PSO作成

必要ならConcurrent Mapと一度だけ作る仕組みを使います。同一Keyを複数Threadが重複作成しないようにします。

## 55. Asset Lifetime

Draw Packetが参照するMesh/Texture/Descriptorは提出FrameのFence完了まで生存させます。CPU参照CountだけではGPU寿命を表しません。

## 56. Deferred Destruction

破棄要求ResourceをFence値付きQueueへ入れ、完了後に解放します。Hot Reload/Scene遷移も同じ規則へ統一します。

## 57. Thread-local Scratch

一時Vector、Sort Buffer、Linear AllocatorをWorkerごとに持つとHeap Lockを減らせます。Frame終了でまとめてResetします。

## 58. False Sharing

別Threadが同じCache Line上のCounterへ頻繁に書くと競合します。Worker StateをCache Line分離し、集計頻度を下げます。

## 59. Atomicの使い過ぎ

正しくても全DrawごとのGlobal AtomicはScalabilityを落とします。Local集計後に一度だけMergeします。

## 60. Mutexの範囲

Driver/API呼出し全体を一つのGlobal Mutexで囲むと並列化が消えます。共有Metadataの最小範囲だけ保護します。

## 61. Lock-freeの注意

Lock-freeは自動的に高速/安全ではありません。ABA、Memory Order、Lifetimeが複雑になるため計測とTestが必要です。

## 62. C++ Memory Model

Job公開前の書込みがWorkerから見えるHappens-beforeをQueue/Mutex/Atomicで保証します。`volatile`はThread同期ではありません。

## 63. Completion Counter

Job完了をAtomic Counter/Latch等で集計します。Main ThreadがBusy Waitし続けない設計にします。

## 64. Exception Policy

Worker内例外を消失させず、Error Channelへ記録しFrameを安全に中止/縮退します。通常Runtimeでは例外を跨ぐ設計を慎重にします。

## 65. HRESULT

Workerで発生したAPI失敗をJob ID、Pass、Thread、Frameと共に保存し、Main側へ伝播します。

## 66. Debug Name

Command List/AllocatorへFrame、Pass、Workerを含む名前を付け、PIX/DREDで追跡しやすくします。

## 67. PIX Event

各ListへPass/ChunkのEvent Markerを入れます。CPU Job名とGPU Event名を対応させます。

## 68. CPU Profiler

Job待ち、Recording、Sort、Descriptor、Upload、Close、Submission時間をThread Timelineで確認します。

## 69. GPU Profiler

CPU Recordingが短くなってもGPU Commandが悪化していないか確認します。細かいListによるState重複も測ります。

## 70. Parallel Efficiency

```text
speedup = single-thread time / parallel time
efficiency = speedup / worker count
```

完全な線形Speedupは通常得られません。Serial部分とOverheadを測ります。

## 71. Critical Path

全Job時間合計でなく、Frame完成を遅らせる最長依存列を短くします。最後に残る重いJobを分割します。

## 72. Submission Thread

提出をMain Threadで行うか専用Threadにするか決めます。Present、Fence、Frame Graph、Device ErrorのOwnershipを一元化します。

## 73. Batch Execute

完成Listを一回の`ExecuteCommandLists`へまとめられます。依存/順序を保ちつつAPI呼出し数を減らします。

## 74. List数のCost

Listが多過ぎるとClose/Execute、State再設定、Driver/GPU Scheduling Costが増えます。適正数を機種別に測ります。

## 75. Bundle

繰返しCommandをBundleに記録できますが、State制約と柔軟性を理解します。現代の設計で常に有利とは限りません。

## 76. Bundleと動的Data

Root ParameterやDescriptorの更新要件によって再利用性が下がります。Static Geometry等、適合する範囲を限定します。

## 77. Shadow Cascade例

CascadeごとにVisible ListとRecording Jobを作れます。共有Shadow Atlas領域、Viewport、Barrier、提出順を事前割当します。

## 78. Opaque Pass例

Sort済みDraw PacketをCost推定でChunkへ分け、各ListがPass共通Stateを設定してRangeを描画します。

## 79. Transparent Pass

Back-to-front順を壊さないRange分割が必要です。List提出順までSort順と一致させます。

## 80. UI Pass

Clip/Layer順に依存しやすいため無理に並列化せず、準備だけ並列、記録は順次等も検討します。

## 81. Compute Pass

独立Dispatchは並列記録可能ですが、UAV依存とDispatch間Barrierを中央Graphから生成します。

## 82. 高速戦闘Scene

Shadow、Character、Environment、EffectのRecordingを分けます。透明Effect順と大量Skinned Drawの偏りを重点的に測ります。

## 83. Frame変動

敵/Effect数が急増すると固定Chunkが偏ります。前Frame統計や動的分割でSpikeを抑えます。

## 84. Deterministic Order

同じ入力なら同じSort/Submission順になるよう、同値KeyへStable IDを含めます。Worker完了順を結果へ反映しません。

## 85. Reproducibility

Frame番号、Job Seed、Packet数、Pass順、Thread数をLogし、並列Bugを再現できるようにします。

## 86. Single-thread Mode

同じJob/Recording経路をWorker 1で実行できるModeを用意し、並列性によるBugか切り分けます。

## 87. Randomized Scheduler Test

開発TestでJob順やYield位置を変え、偶然のTiming依存を露出させます。出力順はStableに統合します。

## 88. Thread Sanitizer等

利用可能なCPU Race検出ToolをJob/Allocator Codeへ適用します。GPU Resource RaceはDebug Layer/PIXと別に調べます。

## 89. Unit Test

Chunk分割、Sort、Dependency、Allocator Range、Alignment、Overflow、Fence再利用判定をTestします。

## 90. Integration Test

Worker数1/2/N、空Pass、Draw 1件、大量Draw、Resize、Hot Reloadを同じ画像結果と比較します。

## 91. Stress Test

Job大量投入、意図的遅延、Allocator不足、Descriptor不足、Frame Spike、Device Lostを組み合わせます。

## 92. よくある失敗：ResetでDevice Removed

GPU完了前Allocator Reset、同じAllocatorの同時利用、Fence値管理ミスを確認します。

## 93. よくある失敗：時々Textureが違う

Descriptor Range重複、Frame Heap早期Reset、Material Hot Reload、Root Table設定漏れを確認します。

## 94. よくある失敗：Barrier警告

Local/Global State不一致、提出順違い、Subresource粒度、UAV依存漏れを確認します。

## 95. よくある失敗：透明順が壊れる

Worker完成順で提出していないか、Chunk境界とStable Sort Keyを確認します。

## 96. よくある失敗：Coreを増やすと遅い

Job粒度、Global Lock、Atomic競合、False Sharing、List数、Memory Allocation、Critical Pathを測ります。

## 97. よくある失敗：終了時Crash

Worker停止順、Job内Resource参照、Device破棄、Fence待機、Deferred Destructionを確認します。

## 98. 実装Checklist

- [ ] Allocator/Listを単一Jobへ所有させる。
- [ ] Fence完了後だけFrame ResourceをResetする。
- [ ] 不変Draw Packet/SnapshotをWorkerへ渡す。
- [ ] Pass順とList提出順を中央で決定する。
- [ ] Local/Global Resource Stateを統合する。
- [ ] Descriptor/Upload Rangeを重複させない。
- [ ] Asset/PSOをGPU完了まで生存させる。
- [ ] Job粒度とCritical Pathを計測する。
- [ ] Worker数が変わっても結果を再現可能にする。

## 99. 理解確認問題

1. CPU Recording並列化とGPU並列実行の違いを説明してください。
2. Command AllocatorをResetできる条件を説明してください。
3. Worker完成順でListを提出してはいけない理由を説明してください。
4. Local/Global State Trackerを分ける理由を説明してください。
5. Descriptor/Uploadを安全に並列割当する方法を挙げてください。
6. Job粒度の大小による問題を説明してください。
7. Transparent Passを並列記録するときの制約を説明してください。
8. GPU LifetimeとCPU Pointer Lifetimeの違いを説明してください。
9. Parallel EfficiencyとCritical Pathを説明してください。
10. Timing依存Bugを再現する仕組みを提案してください。

## 100. 要点

- Command List RecordingはCPU並列化でありGPU Queue並列化とは別です。
- Allocator/List/Descriptor/Upload領域のOwnershipを明示します。
- Workerには不変Snapshotと専用出力範囲を渡します。
- Pass Graphが提出順とResource Barrierを中央管理します。
- Job完成順ではなく決定済みPass/Sort順でListを提出します。
- FenceはAllocator ResetだけでなくResource/Descriptor寿命も守ります。
- Core数ではなくCritical Path、Lock、粒度、List Costを計測します。

## 101. 公式資料

- [Recording Command Lists and Bundles](https://learn.microsoft.com/en-us/windows/win32/direct3d12/recording-command-lists-and-bundles)
- [ID3D12CommandAllocator::Reset](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12commandallocator-reset)
- [ID3D12GraphicsCommandList::Reset](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12graphicscommandlist-reset)
- [ID3D12CommandQueue::ExecuteCommandLists](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12commandqueue-executecommandlists)
- [Multithreading and synchronization](https://learn.microsoft.com/en-us/windows/win32/direct3d12/multithreading)

## 102. 次章への接続

次章ではMultiple Queue・Async Computeを扱います。本章で並列記録したDirect/Compute/Copy Listを複数Queueへ配置し、FenceでQueue間依存を構築します。
