# DirectX 12 第16章：Multiple Queue・Async Compute

この章では、Direct、Compute、Copy Queueを組み合わせ、GPU内の依存をFenceで構築する方法を学びます。Queue能力、Signal/Wait、Resource State、Upload、Async Compute、Frame Timeline、Deadlock、性能計測、高速戦闘Sceneへの適用まで扱います。

## 1. 到達目標

- Queue Typeごとの役割と制約を説明する。
- GPU側Signal/WaitでQueue間依存を作る。
- Resource StateとLifetimeを複数Queueで管理する。
- Async Computeが有効/逆効果な条件を判断する。
- Deadlock、過剰同期、Frame遅延を診断する。

## 2. 三種類のQueue

```text
DIRECT  : graphics、compute、copy
COMPUTE : compute、copy
COPY    : copy中心
```

対応しないCommandを記録/実行しません。

## 3. Queueを増やす目的

CPU整理のためではなく、GPU Engineの利用重複、Upload分離、依存の明示化が目的です。Hardwareが実際に同時実行できるとは限りません。

## 4. Queue作成

```cpp
D3D12_COMMAND_QUEUE_DESC desc{};
desc.Type = D3D12_COMMAND_LIST_TYPE_COMPUTE;
desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
device->CreateCommandQueue(&desc, IID_PPV_ARGS(&computeQueue));
```

Typeに対応するAllocator/Listを用意します。

## 5. Queue Priority

Normal/High等がありますが、権限やPlatform制約を確認します。Priorityは依存設計やDeadline保証の代わりではありません。

## 6. Command Queueの順序

同一Queueへ提出したCommandはQueue順序を形成します。異なるQueue間にはFenceを使わない限り自動の実行順保証がありません。

## 7. Fenceの役割

単調増加値で特定地点までのGPU進行を表します。CPU待機、Frame Resource再利用、Queue間依存、Deferred Destructionに使います。

## 8. Queue Signal

```cpp
const UINT64 value = ++nextFenceValue;
queue->Signal(fence.Get(), value);
```

SignalはQueue内で先行Commandの後へ並びます。

## 9. CPU Wait

Fence完了値を確認し、未完了ならEventを設定して待ちます。毎Pass CPU Waitすると並列性とPipeliningを失います。

## 10. GPU Queue Wait

```cpp
consumerQueue->Wait(producerFence.Get(), producerValue);
```

CPUを停止せず、Consumer Queueの後続処理だけをProducer完了まで待たせます。

## 11. Producer/Consumer

ProducerがResourceへ書き、FenceをSignalし、Consumerがその値をWaitしてから読む、という方向を明示します。

## 12. CopyからGraphicsへ

```text
copy queue: upload resource copy -> signal copyFence N
direct queue: wait copyFence N -> use resource
```

Resource StateとUpload Resource Lifetimeも同時に管理します。

## 13. GraphicsからComputeへ

Depth/Normal等をGraphicsで生成後Computeが読むなら、Direct Queue SignalとCompute Queue Waitを置きます。

## 14. ComputeからGraphicsへ

Compute Skinning/Culling結果をDrawが読むなら、Compute SignalとDirect Waitを置きます。

## 15. 往復依存

一Frame内でGraphics→Compute→Graphicsと往復すると同期Pointが増えます。Pass配置を変えて依存を一方向へ近づけます。

## 16. Deadlock

Queue AがBの未来値を待ち、BがAの未来値を待つCycleを作ると進行不能になります。

## 17. Fence Dependency Graph

Queue Waitを有向EdgeとしてGraph化し、Cycleを拒否します。Frameを跨ぐEdgeも含めます。

## 18. Fence値管理

Queueごとに単調増加Counterを持たせ、値の所有者を明確にします。同じFence値を曖昧な複数地点で再利用しません。

## 19. Fence Overflow

64-bit値は現実的に十分大きいですが、0や初期値、再生成、Device Lost時のReset規約を定めます。

## 20. Queue Timeline

```text
Direct : Depth ----- Main -------- Post ---- Present
Compute:       Skin/Cull -- Light --------
Copy   : Upload A ----- Upload B ----------
```

Wait位置とIdle区間をTimelineで確認します。

## 21. Async Computeとは

Graphics処理と独立なCompute Workを別Queueへ置き、GPU上で時間的にOverlapさせる設計です。

## 22. Concurrent実行の条件

Hardware Engine、Driver Scheduling、Resource依存、Bandwidth、Occupancyに余裕が必要です。別Queueに置くだけでは速くなりません。

## 23. Overlap候補

独立Particle Simulation、Light Culling、Animation/Skinning、Post-process準備等が候補ですが、入力が準備済みか確認します。

## 24. Overlapできない処理

直前Graphics出力を読み、直後Graphicsが結果を必要とする短いComputeはWaitで挟まれ、Overlap範囲がほぼありません。

## 25. Critical Path

Frame終了を決める最長依存列を短くできるかが重要です。非CriticalなWorkが重なってもFrame時間が変わらない場合があります。

## 26. Resource競合

別Queueが同時に同じResource/Subresourceへ競合Accessしないよう依存を設定します。Read/ReadとWriteを区別します。

## 27. Read/Read

互換Read Stateで双方が読むだけなら同時利用できる可能性があります。State Bitの組合せと用途を検証します。

## 28. Write依存

少なくとも一方が書くなら実行順とMemory可視性が必要です。Fence Waitと適切なBarrierを組み合わせます。

## 29. BarrierとFenceの違い

BarrierはResource Access/Layout/VisibilityをCommand Stream内で定義し、FenceはQueue間/CPU間の実行依存を定義します。片方だけで全てを代替しません。

## 30. Queue間State Tracking

Resourceの最終Stateだけでなく、どのQueueがいつAccessし、どのFence値で完了するか記録します。

## 31. COMMON State

Copy Queueとの移行やImplicit Promotion/Decayには規則があります。便利さだけで曖昧にせず、仕様とDebug Layerで検証します。

## 32. Explicit Transition

複雑なMultiple Queue設計では開始/終了Stateを明示し、Pass Contractとして記録すると追跡しやすくなります。

## 33. Split Barrier

BEGIN_ONLY/END_ONLYで遷移を分割できる場合がありますがQueue/仕様制約を確認します。まず通常BarrierとFenceで正しくします。

## 34. Enhanced Barriers

Sync、Access、Layoutをより明示的に表します。Queue間設計でもPassのAccess宣言から適切なBarrierへ変換します。

## 35. Copy Queue Upload

Texture/Buffer UploadをCopy Queueへ移すとDirect QueueのRecording/実行から分離できます。Upload量とLatencyをBudget化します。

## 36. Upload Batch

小さいCopyごとにSignal/WaitせずBatch化します。AssetごとのReady Fence値をBatch Fenceへ関連付けます。

## 37. Upload Ring

Copy完了FenceまでUpload Memoryを再利用しません。Queue別FenceとAllocation Rangeを記録します。

## 38. Texture Footprint

`GetCopyableFootprints`のRow/Placement Alignmentを守ります。Copy Queue化しても規則は変わりません。

## 39. Asset Ready State

```text
Unloaded -> CPU Ready -> Uploading -> GPU Ready -> Retiring
```

GPU Readyは対応Fence完了後にのみ公開します。

## 40. First-use Wait

Resourceを最初に使うQueueだけがUpload FenceをWaitする方式で不要なGlobal CPU Waitを避けられます。

## 41. Placeholder

未Upload AssetはFallbackを描画し、未完了Fenceを毎Drawで待たない設計も有効です。

## 42. Streaming Priority

Camera近傍、現在Character、次Action等へUpload優先度を付けます。Queue PriorityだけでAsset優先順は決まりません。

## 43. Copy Bandwidth

大量StreamingがGraphicsのMemory Bandwidthと競合する場合があります。FrameごとのByte BudgetとThrottleを設けます。

## 44. Compute Skinning例

Animation Upload完了後ComputeがSkinningし、Direct QueueがそのFenceをWaitしてVertex Bufferを使います。

## 45. Skinningの前倒し

前Frame末尾や次Frame序盤に必要Dataを準備できればOverlap幅が増えますが、入力LatencyとPose時刻を確認します。

## 46. GPU Culling例

Depth/Hi-Z依存がある場合、Graphics Signal→Compute Wait→Culling→Compute Signal→Graphics Waitという往復になります。

## 47. 前FrameDepth利用

前Frame Hi-Zを使えば早期Cullingを開始できますが、Camera/Object移動による誤Occlusionを保守的に処理します。

## 48. Particle例

前Frame状態とGameplay Spawn Bufferが準備済みなら、Shadow/Depth処理とParticle Simulationを重ねられる可能性があります。

## 49. Post Process例

各Effectは前段Color/Depthへの依存が強く、別Queue移動でGraphics/Compute往復が増える場合があります。Pass群全体を配置します。

## 50. Queue Submission Plan

Frame GraphがPass Topology、Queue割当、Barrier、Signal/Waitをまとめて生成する設計が有効です。

## 51. Queue Assignment

PassのCommand能力、Resource依存、推定Cost、Overlap候補からQueueを選びます。名前がComputeでもDirect Queueが最適な場合があります。

## 52. Automatic Schedulingの注意

単純なEarliest StartだけではBandwidth競合やGPU特性を表せません。安全な固定Scheduleから計測して改善します。

## 53. Semaphore的乱用

Passごとに相互WaitするとQueueが細切れになり、Driver/Hardware Scheduling Costが増えます。依存をBatch化します。

## 54. Queue Flush

Resize、Shutdown、Device Resource再構築等では必要QueueをSignal/Waitして安全地点を作ります。通常Frameで毎回Flushしません。

## 55. Present

Present対象Back Bufferへの全描画がDirect Queue上で完了する順序を保証します。Computeが間接的に必要ならDirect側Waitを先に置きます。

## 56. Frame Latency

Queue間OverlapでThroughputが上がっても入力から表示までのLatencyが増える場合があります。同時進行Frame数を監視します。

## 57. CPU Ahead

CPUが複数Frameを提出するとGPU利用率は上がり得ますがLatencyとResource Memoryが増えます。Frame Latency制御と合わせます。

## 58. Triple Bufferingとの関係

Back Buffer数、Frame Context数、各Queueの進行は同じではありません。Resource再利用は関連Fenceで個別に判定します。

## 59. Per-resource Last-use Fence

Resourceが複数Queueで使われる場合、Queue別の最終Fence値または統合Retirement条件を保持します。

## 60. Deferred Destruction

参照した全QueueのFence完了後にResource/Descriptorを解放します。Direct Fenceだけ待ってCopy/Compute使用を見落としません。

## 61. Descriptor Lifetime

Queue WaitがあってもShader-visible Descriptor自体を早期上書きできるわけではありません。最後のConsumer完了まで保持します。

## 62. Command Allocator Lifetime

AllocatorはそのCommand Listを実行したQueueのFence完了後にResetします。別Queue Fenceを誤参照しません。

## 63. Device Removed

Fenceが永遠に進まない可能性を考え、Wait LoopにDevice Removed検査と診断経路を持たせます。

## 64. Event Handle

Fence待機Eventの作成/破棄、複数待機、Shutdown Raceを管理します。無限Waitの前に失敗状態を確認します。

## 65. ThreadとQueue

一Queueへ複数CPU Threadが提出する場合、Application側でSubmission順とFence値発行を直列化します。

## 66. Submission Ownership

Queueごとに専用Submission Context/Threadを持たせると、Signal値と順序を一元管理できます。

## 67. Packet化

Command List列、Wait依存、Signal値、Debug LabelをSubmission PacketとしてQueue Ownerへ渡します。

## 68. Timestamp

QueueごとにTimestamp Queryを記録し、対応Frequencyで時間へ変換します。異なるQueueのTimestamp基準比較にはCalibrationを考慮します。

## 69. Clock Calibration

`GetClockCalibration`等でGPU/CPU Clockの対応を取り、Timeline Toolへ統合できます。取得時点とFrequencyを記録します。

## 70. PIX Queue Timeline

Direct/Compute/Copy LaneのOverlap、Wait、Idle、Barrier、Pass Durationを確認します。色が重なっただけで高速化と判断しません。

## 71. Baseline

全ComputeをDirect Queue、Uploadも単純同期した正しい基準版を維持し、Multiple Queue版と同じSceneで比較します。

## 72. 測定項目

GPU Frame Time、Critical Path、Queue Idle、Overlap時間、Bandwidth、Occupancy、CPU Submission、Latency、Memoryを記録します。

## 73. Async Computeが逆効果な例

GraphicsとComputeが同じALU/Cache/Bandwidthを奪い合い、両方遅くなる場合があります。単独時間と重複時時間を比較します。

## 74. 小さいDispatch

Queue間Signal/Wait CostがWork本体を上回り得ます。小PassをまとめるかDirect Queueへ残します。

## 75. GPU別差

Engine構成、Scheduling、Bandwidthが異なります。Vendor一社/一機種だけでQueue配置を固定判断しません。

## 76. Dynamic Policy

品質設定やGPU ProfileによりAsync Computeを切替える場合、両経路の結果一致とCacheをTestします。

## 77. 高速戦闘Sceneの優先事項

入力LatencyとFrame Spikeを優先し、平均FPSだけを見ません。大量Effect時にUpload/Particle/LightingがCritical Pathを塞がないか測ります。

## 78. Frame Budget例

Skinning、Culling、Particle、Shadow、Main、Post、UploadのGPU時間と依存を記録し、Overlap可能区間を可視化します。

## 79. Graceful Degradation

負荷上昇時はParticle数、遠距離Skinning頻度、Streaming Byte、Async Pass品質を下げ、Fence待ちSpikeを抑えます。

## 80. Deterministic Submission

同じFrame入力なら同じQueue Packet/Wait/Signal順を生成します。Worker完成順でFence Graphを変えません。

## 81. Logging

Frame、Queue、List、Signal値、Wait対象、Resource、Passを循環Bufferへ記録し、Hang直前の依存を復元します。

## 82. Graph Validation

未Signal値Wait、Cycle、同時Write、未初期化Resource、Queue能力違反を提出前に開発Buildで検査します。

## 83. Unit Test

Fence値発行、Dependency Graph Cycle、Retirement条件、Queue割当、Batch化、State ContractをTestします。

## 84. Integration Test

Copy→Direct、Direct→Compute→Direct、複数Frame、Resize、Streaming、Async ON/OFFを同じ画像結果で比較します。

## 85. Stress Test

大量Upload、Queue遅延、Fence値Gap、Device Lost、Shutdown中Job、Resource Hot Reloadを組み合わせます。

## 86. よくある失敗：時々古いData

Consumer Wait漏れ、誤Fence値、Signal前提出順、Descriptor早期再利用、State/Visibility不足を確認します。

## 87. よくある失敗：GPU Hang

Queue間待機Cycle、未Signal値、Device Removed、Allocator/Resource破棄をFence LogとDREDで確認します。

## 88. よくある失敗：Asyncで遅い

短いWork、往復Wait、Bandwidth競合、Critical Path外、Queue Packet過多をPIX Timelineで確認します。

## 89. よくある失敗：Shutdown Crash

全Submission停止、Worker終了、各Queue Fence完了、Deferred Destruction、Queue/Device破棄の順を確認します。

## 90. 実装Checklist

- [ ] Queue能力に合うCommandだけを記録する。
- [ ] QueueごとにFence値を単調発行する。
- [ ] Producer Signal→Consumer Waitを明示する。
- [ ] Wait GraphのCycleと未Signal値を検査する。
- [ ] BarrierとFenceの役割を分ける。
- [ ] Upload/Descriptor/Resourceを全Consumer完了まで保持する。
- [ ] Signal/WaitをPassごとに乱発せずBatch化する。
- [ ] BaselineとPIX TimelineでAsync効果を実測する。
- [ ] ThroughputだけでなくLatency/Spikeを測る。

## 91. 理解確認問題

1. Direct/Compute/Copy Queueの能力を説明してください。
2. Queue SignalとQueue Waitの順序を説明してください。
3. FenceとResource Barrierの違いを説明してください。
4. Queue間Deadlockが起こるGraphを説明してください。
5. Copy Upload Resourceを使えるようになるまでを説明してください。
6. Async ComputeがOverlapできる条件を挙げてください。
7. Graphics→Compute→Graphics往復の問題を説明してください。
8. Resource破棄に複数Queue Fenceが必要な理由を説明してください。
9. Async Computeが逆効果になる原因を挙げてください。
10. 高速戦闘Sceneで測るべき指標を挙げてください。

## 92. 要点

- 異なるQueue間にはFence Signal/Waitで明示的な順序を作ります。
- BarrierはResource Access、FenceはQueue/CPU実行依存を扱います。
- Async Computeは独立WorkとHardware余力がある場合だけ効果が期待できます。
- Copy UploadはBatch化し、Ready FenceとResource Lifetimeを追跡します。
- 往復依存、過剰Wait、Bandwidth競合は並列性を失わせます。
- Resource/Descriptorは最後に使う全Queueの完了まで生存させます。
- PIX TimelineとBaselineでCritical Path、Latency、Spikeを実測します。

## 93. 公式資料

- [Command Queues and Command Lists](https://learn.microsoft.com/en-us/windows/win32/direct3d12/user-mode-heap-synchronization)
- [ID3D12CommandQueue::Signal](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12commandqueue-signal)
- [ID3D12CommandQueue::Wait](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12commandqueue-wait)
- [ID3D12Fence](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nn-d3d12-id3d12fence)
- [Multi-engine synchronization](https://learn.microsoft.com/en-us/windows/win32/direct3d12/multi-engine)

## 94. 次章への接続

次章ではGPU Memory・Transient Resourceを扱います。Multiple Queueで使うResourceのHeap配置、Budget、Residency、Alias、Transient Lifetimeを体系化します。
