# DirectX 12 第3章：Command Queue・Allocator・Command List

この章では、GPU Commandを記録してSubmitする中核Objectを学びます。Queue Type、Priority、AllocatorのBacking Memory、Command ListのReset/Close/Execute、所有権、複数Thread、Pool、失敗診断までを扱います。

## 1. 三Objectの役割

```text
Command Queue     : GPUへCommand ListをSubmitする
Command Allocator : 記録CommandのBacking Memoryを持つ
Command List      : CPUがGPU Commandを順番に記録するInterface
```

名前が似ていますが、責務と再利用条件は異なります。

## 2. 一回の基本Flow

```text
Allocator Reset
 -> Command List Reset
 -> Command記録
 -> Command List Close
 -> Queue ExecuteCommandLists
 -> Fence Signal
 -> GPU完了後にAllocator再利用
```

## 3. Command Queueは実行入口

QueueへSubmitした順序がGPU Timelineの基本順序になります。Queue Methodを呼んだ時点で処理完了したわけではありません。

## 4. Queue Description

```cpp
D3D12_COMMAND_QUEUE_DESC desc{};
desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
desc.NodeMask = 0;
```

Zero Initializeして必要Fieldを明示します。

## 5. CreateCommandQueue

```cpp
ComPtr<ID3D12CommandQueue> directQueue;
ThrowIfFailed(
    device->CreateCommandQueue(&desc, IID_PPV_ARGS(&directQueue)),
    "ID3D12Device::CreateCommandQueue");

directQueue->SetName(L"Direct Queue");
```

## 6. DIRECT Queue

Graphics、Compute、Copy Commandを扱えます。最初のRendererはDirect Queue一つで完成させます。

## 7. COMPUTE Queue

Compute/Copyを扱い、Graphics Drawは記録できません。Async ComputeはQueue間依存とHardware特性を測って導入します。

## 8. COPY Queue

Copy専用Workload向けです。Resource UploadやStreamingをGraphicsから分離できますが、使用前にQueue間同期が必要です。

## 9. BUNDLE Type

Bundleは別のDirect Command Listから実行される補助Command Listです。Queueへ直接Submitする通常Listとは異なります。

## 10. Typeの互換性

Allocator、Command List、QueueのType制約を守ります。Direct AllocatorでCopy ListをResetする等の混在を避けます。

## 11. Queue Priority

Normal/High等があります。High PriorityはOS/権限/用途の制約があり、無条件に指定して他Processを妨げません。

## 12. Queue Flags

通常は`D3D12_COMMAND_QUEUE_FLAG_NONE`です。Disable GPU Timeout等の特殊Flagは用途と権限を理解して使います。

## 13. Node Mask

Multi-adapter Nodeを指定するFieldです。Single GPUの基本実装では0を使い、Multi-nodeは別設計として扱います。

## 14. Timestamp Frequency

```cpp
UINT64 frequency = 0;
ThrowIfFailed(
    directQueue->GetTimestampFrequency(&frequency),
    "ID3D12CommandQueue::GetTimestampFrequency");
```

GPU Timestampを時間へ変換する際に使います。

## 15. Clock Calibration

QueueのGPU TimestampとCPU QPCを対応付けられます。CPU/GPU Timeline分析に使います。

## 16. Command Allocatorとは

記録Command用Memoryを管理します。AllocatorをResetすると、そのMemoryを新しい記録へ再利用可能にします。

## 17. CreateCommandAllocator

```cpp
ComPtr<ID3D12CommandAllocator> allocator;
ThrowIfFailed(
    device->CreateCommandAllocator(
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        IID_PPV_ARGS(&allocator)),
    "ID3D12Device::CreateCommandAllocator");

allocator->SetName(L"Frame 0 Direct Allocator");
```

## 18. AllocatorはThread-safe共有物ではない

同じAllocatorへ複数Threadから同時記録しません。Frame/Worker/Queue Typeごとに所有者を決めます。

## 19. Resetの意味

```cpp
ThrowIfFailed(allocator->Reset(), "ID3D12CommandAllocator::Reset");
```

以前のCommand記録Memoryを再利用可能にします。

## 20. Resetの前提

そのAllocatorを参照する全Command ListのGPU実行が完了済みでなければなりません。CPUで`Close`済みというだけでは不足です。

## 21. Fenceとの関係

Allocatorを最後にSubmitした後のFence Valueを保存し、`GetCompletedValue() >= value`を確認してからResetします。

## 22. Allocatorを毎Frame一つにしない理由

GPUがFrame Nを処理中にCPUがFrame N+1を記録するには、別Allocator Memoryが必要です。

## 23. Frame Resource内Allocator

```cpp
struct FrameResource
{
    ComPtr<ID3D12CommandAllocator> directAllocator;
    UINT64 fenceValue = 0;
};
```

Frame Resource再利用時に対応Fenceだけを待ちます。

## 24. Command Listとは

Barrier、Clear、Draw、Dispatch、Copy等を呼出し順に記録します。呼出し時にGPUが即実行するわけではありません。

## 25. CreateCommandList

```cpp
ComPtr<ID3D12GraphicsCommandList> commandList;
ThrowIfFailed(
    device->CreateCommandList(
        0,
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        allocator.Get(),
        nullptr,
        IID_PPV_ARGS(&commandList)),
    "ID3D12Device::CreateCommandList");
```

作成直後は記録状態です。

## 26. Initial PSO

作成/Reset時に初期PSOを渡せます。Clear/Copyだけなら`nullptr`でも構いません。Draw前には適切なPSOが必要です。

## 27. 作成直後にCloseするPattern

```cpp
ThrowIfFailed(commandList->Close(), "Initial CommandList Close");
```

初期化で作成後に一度Closeし、Frame開始時にResetして記録する設計が一般的です。

## 28. Reset順序

```cpp
WaitForFrameResource(frame);
ThrowIfFailed(frame.directAllocator->Reset(), "Allocator Reset");
ThrowIfFailed(
    commandList->Reset(frame.directAllocator.Get(), initialPso),
    "CommandList Reset");
```

Allocatorを先にResetします。

## 29. Command List Reset

Listを記録状態へ戻し、使用Allocatorと初期PSOを指定します。List Objectと記録Memoryを分離して理解します。

## 30. List ResetとGPU完了

Command List Objectは、前回SubmitがGPU実行中でも別の安全なAllocatorへResetできる設計が可能です。ただし同じListを複数Threadが同時記録してはいけません。

## 31. Recording State

Reset後からCloseまでCommandを記録できます。Close後にDraw/Barrierを追加しません。

## 32. Resource Barrier記録

```cpp
commandList->ResourceBarrier(1, &barrier);
```

Command Stream上の順序として記録されます。CPU Resource State Trackerも同じ順序で更新します。

## 33. Clear記録

```cpp
commandList->ClearRenderTargetView(
    rtvHandle,
    clearColor,
    0,
    nullptr);
```

対応ResourceがRender Target Stateである必要があります。

## 34. Draw記録

Draw前にRoot Signature、PSO、Descriptor Heap、View、Viewport、Scissor、Vertex/Index Buffer等を設定します。

## 35. Copy記録

Copy元/先Resource State、Footprint、Offset、Lifetimeを正しく管理します。Upload BufferをSubmit直後に破棄しません。

## 36. Close

```cpp
ThrowIfFailed(commandList->Close(), "ID3D12GraphicsCommandList::Close");
```

Validation ErrorがClose時に返る場合があるためHRESULTを確認します。

## 37. Close失敗

不正Command Sequence、未対応操作等をDebug Layer Messageと共に確認します。失敗ListをExecuteしません。

## 38. ExecuteCommandLists

```cpp
ID3D12CommandList* lists[] = { commandList.Get() };
directQueue->ExecuteCommandLists(
    static_cast<UINT>(std::size(lists)),
    lists);
```

Return Valueは`void`です。実行時ErrorはDebug LayerやDevice Removedで検出します。

## 39. SubmitとComplete

ExecuteはQueueへ仕事を渡すだけです。GPU完了確認には後続Fence Signalが必要です。

## 40. Fence Signal位置

```text
Execute Command Lists
 -> Present等
 -> Queue Signal(fence, frameValue)
```

Signalより前にQueueへ積まれた仕事の完了地点を表します。

## 41. 複数Listの一括Execute

```cpp
ID3D12CommandList* lists[] =
{
    shadowList.Get(),
    opaqueList.Get(),
    effectList.Get()
};

directQueue->ExecuteCommandLists(3, lists);
```

Array順に依存を満たすよう並べます。

## 42. 一括Submitの利点

Queue呼出し回数を減らせます。ただしList間Barrier/依存、Marker、粒度を正しく設計します。

## 43. Submit順はJob完了順ではない

Workerが早く記録し終えた順にSubmitするとShadow→Main等の依存が壊れます。Frame GraphのCompiled Orderを使います。

## 44. Queue内部の順序

同じQueueへSubmitされたCommandはQueue順序に従います。Resource BarrierとCommand順を明示して正しい依存を作ります。

## 45. Queue間は自動同期されない

Copy Queue出力をDirect Queueで使う場合、Fence Signal/Waitを明示します。CPU Submit順だけではGPU Queue間依存を保証しません。

## 46. Queue Signal

```cpp
ThrowIfFailed(
    directQueue->Signal(fence.Get(), value),
    "ID3D12CommandQueue::Signal");
```

CPUからFence値を直接完了扱いにする操作ではありません。

## 47. Queue Wait

```cpp
ThrowIfFailed(
    directQueue->Wait(copyFence.Get(), uploadFinishedValue),
    "ID3D12CommandQueue::Wait");
```

Queue Timelineが対象Fenceまで待ち、CPU Threadは通常Blockingしません。

## 48. CPU Waitとの違い

CPU Event WaitはCPU Threadを止めます。Queue WaitはGPU Queueの後続Workを止め、CPUは別仕事を継続できます。

## 49. Command ListにFenceは記録しない

Fence Signal/WaitはQueue Operationです。Graphics Command List内のDraw/Barrierと役割を分けます。

## 50. Execute後のCommand List Lifetime

Command List Object自体とAllocator/Resource/Descriptorの寿命を混同しません。GPUが実際に読むBacking Dataと参照先をFenceまで保持します。

## 51. Command Allocator Pool

```cpp
struct AllocatorEntry
{
    ComPtr<ID3D12CommandAllocator> allocator;
    UINT64 availableAfterFence = 0;
};
```

完了済みEntryを再利用し、不足時だけ追加します。

## 52. Command List Pool

Type別にList Objectを再利用します。記録中、Closed、Submitted、Available等の状態をDebug Buildで追跡します。

## 53. Pool Acquire

要求Type、Worker、Completed Fenceを入力に、安全なAllocator/List Pairを返します。

## 54. Pool Release

Submit後のFence ValueをEntryへ保存します。CPU記録終了時点でAvailableへ戻しません。

## 55. Pool成長

GPU遅延やJob増加でPoolが成長し続ける場合、警告とPeak統計を出します。無制限のMemory増加を隠しません。

## 56. 一Thread一List固定の限界

Worker数よりPass/Chunkが多い場合、一Frameで複数Listを順に記録できます。Allocator再利用条件とList粒度を設計します。

## 57. Multithread記録

```text
Worker 0: Allocator A0 + List L0 -> Shadow
Worker 1: Allocator A1 + List L1 -> Opaque Chunk 0
Worker 2: Allocator A2 + List L2 -> Opaque Chunk 1
```

Objectを共有せず独立記録します。

## 58. Deviceは共有可能

PSO/Resource生成等でDeviceを複数Threadから使えますが、Engine側CacheとAllocatorには同期が必要です。

## 59. Command Listは同時共有しない

一つのListへ複数ThreadからCommandを追加しません。所有Workerを一つにします。

## 60. Allocatorは同時Resetしない

別Threadが同Allocatorで記録中にResetすると破壊します。Ownerと状態遷移をAssertします。

## 61. Thread-local Scratch

Render Item、Descriptor Copy List、Barrier List、Marker文字列等の一時DataをWorker専用Allocatorへ置きます。

## 62. Pass Recording Context

```cpp
struct CommandRecordingContext
{
    ID3D12GraphicsCommandList* list;
    D3D12_COMMAND_LIST_TYPE type;
    uint32_t workerIndex;
    uint64_t frameId;
};
```

Raw Listだけでなく診断情報を一緒に渡します。

## 63. Command Context Wrapper

Barrier、Descriptor、Draw Marker、State TrackerをWrapperへ統合できます。Native Listの無制限Accessを減らします。

## 64. Context Begin

安全なAllocatorをAcquireし、Reset、初期PSO、Frame/Pass Marker開始までを一つの操作にします。

## 65. Context Finish

Marker終了、Close、Result生成までを行い、Submit順Keyと使用Resource情報を返します。

## 66. Recording Result

```cpp
struct RecordedCommandList
{
    uint64_t frameId;
    uint32_t passOrder;
    ID3D12CommandList* list;
    AllocatorEntry* allocatorEntry;
};
```

Object Lifetimeを別Ownerが保証します。

## 67. Submission Manager

ResultをPass Orderで並べ、Queue TypeごとにBatchし、Execute、Signal、Pool Release Fence更新を行います。

## 68. Frame Graphとの統合

Graph Compile結果からPass Level、Queue Type、Barrier、Submission Orderを作り、独立Passを並列記録します。

## 69. Queue Type選択

PassがGraphicsを使うならDirect、CopyのみならCopy、ComputeのみならCompute候補です。Resource依存を見て最終決定します。

## 70. Barrier記録Owner

Pass前後BarrierをGraph ExecutorまたはCommand Contextが記録します。Feature Codeが勝手にState Trackerを書き換えないようにします。

## 71. Split Recording

大量OpaqueをChunkごとにListへ分割できます。TransparentのGlobal OrderやPass間Resource依存を壊しません。

## 72. Command Signatureではない

`ID3D12CommandSignature`はExecuteIndirect用Objectです。Command List/Queueの通常記録と名前を混同しません。

## 73. Bundleの作成

Bundle用AllocatorとBundle Type Command Listを使います。Direct Listから`ExecuteBundle`します。

## 74. Bundleの制約

使用できないCommandやState継承規則があります。静的Geometryの繰返し等で効果をProfileして採用します。

## 75. BundleとDirect List Pool

Allocator Typeと寿命を別管理します。Bundleを呼ぶDirect Listとの参照Resource寿命も保証します。

## 76. Command List Version

`ID3D12GraphicsCommandList1`以降に追加Commandがあります。必要Featureに応じて`As`し、存在を仮定しません。

## 77. Enhanced Barriersとの関係

対応Interface/FeatureではEnhanced Barrier Modelを使えます。Legacy `ResourceBarrier`との混在Policyを決め、後章で扱います。

## 78. Predication

GPU条件によりCommand実行を制御できます。Buffer StateとCommand順を正しく管理します。

## 79. ExecuteIndirect

GPU生成ArgumentでDraw/Dispatchできます。Command Signature、Argument Buffer、Counter、Stateが必要で、Compute/Indirect章で詳しく扱います。

## 80. Marker

```cpp
PIXBeginEvent(commandList.Get(), PIX_COLOR_DEFAULT, "Opaque Pass");
// Draw commands
PIXEndEvent(commandList.Get());
```

使用するPIX Runtime/APIに応じたMacroを使い、Pass階層をCaptureへ出します。

## 81. SetName

Queue、Allocator、Command ListへFrame/Worker/Typeを含む名前を付けます。Pool Entry番号も診断に役立ちます。

## 82. Debug Layerで検出できる例

- Type不一致
- Recording/Closed Stateの誤用
- GPU使用中AllocatorのReset
- 不正Command Sequence
- Resource State不一致
- 同時記録の一部問題

すべてのRaceを自動検出できるとは限りません。

## 83. Device Removed

`ExecuteCommandLists`は`void`のため、後続API、Fence、Present、`GetDeviceRemovedReason`、DREDで失敗を診断します。

## 84. DRED Breadcrumb

GPUが最後に進んだCommand List/Eventを特定しやすくするため、意味あるList/Pass名とMarkerを残します。

## 85. Logging

Frame ID、Queue Type、List ID、Allocator ID、Pass、Draw/Dispatch数、Submit順、Fence ValueをDebug Log/Profilerへ記録します。

## 86. Statistics

- 作成済みQueue/List/Allocator数
- Frame内List数
- Execute呼出し数
- Pool Peak
- Allocator待機回数
- Queue別Submit数
- Worker別記録時間

## 87. CPU Profile Scope

Acquire、Allocator Reset、List Reset、Record、Close、Collect、Execute、Signalを分けて測ります。

## 88. GPU時間との違い

Command List記録時間はCPU Costです。GPU Pass時間はTimestamp Query/PIXで測ります。

## 89. 粒度Test

一つの巨大List、Pass単位、Chunk単位を比較します。List数を増やせば常に速いとは限りません。

## 90. Queue Idle待機

初期化/Shutdown/Resize等で全完了が必要な場合はFenceをSignalして待ちます。通常Frame末尾には行いません。

## 91. Flush Helperの正体

D3D12にD3D11 Contextの`Flush`と同じ万能Methodはありません。EngineのFlushはQueue Signal + CPU Fence Waitとして実装されることが多いです。

## 92. Shutdown順序

1. 新規Recording Jobを停止する。
2. WorkerをJoinする。
3. Queueへ残るWorkのFence完了を待つ。
4. List/Allocator Poolを解放する。
5. Queue、Fence、Deviceを解放する。

## 93. Resize時

Back Bufferを参照するListの完了をFenceで保証します。記録途中/未Submit Resultも無効化します。

## 94. Device Lost時

Workerへ旧Device/Listを触らせず停止します。Pool、Queue、Fenceを含むDevice-dependent Objectを再生成します。

## 95. 最小Class設計

```cpp
class DirectCommandSystem
{
public:
    void Initialize(ID3D12Device* device);
    RecordingHandle Begin(uint32_t frameIndex);
    UINT64 Submit(RecordingHandle&& recording);
    void WaitIdle();

private:
    ComPtr<ID3D12CommandQueue> queue_;
    CommandContextPool pool_;
};
```

Fence詳細は次章で統合します。

## 96. Unit Test可能な部分

Type互換表、Pool State遷移、Fence Value比較、Submission Sort、Frame ID検証はGPUなしでもTestできます。

## 97. Integration Test

ClearだけのListを記録・Executeし、Fence完了とDebug Layer Message 0件を確認します。描画はSwap Chain章で接続します。

## 98. Stress Test

多数の小List、複数Worker、Pool不足、GPU遅延、Resize Request、Shutdownを組み合わせ、誤ResetとLeakがないか確認します。

## 99. よくある失敗：Close忘れ

Recording中ListをExecuteします。Submit APIでClosed StateをAssertし、`Close` HRESULTを確認します。

## 100. よくある失敗：二重Close

State管理なしに同じListを重複Finishします。Recording HandleをMove-onlyにし、一度だけFinish可能にします。

## 101. よくある失敗：Allocator早期Reset

CPU記録が終わった時点でPoolへ戻し、GPU使用中にResetします。Submit Fence ValueまでUnavailableにします。

## 102. よくある失敗：Listを同時共有

複数Workerが一つのListへDrawを書きます。WorkerごとのContextをAcquireします。

## 103. よくある失敗：Type不一致

Copy QueueへDirect Listを渡す等の問題が起きます。TypeをRuntime FieldとTemplate/Pool分類で検証します。

## 104. よくある失敗：毎List Fence Wait

SubmitごとにCPUが完了待機し、並列性を失います。Frame Resource/Pool再利用時だけ必要なValueを待ちます。

## 105. よくある失敗：完了順Submit

Worker Resultが届いた順に実行し、Resource依存が壊れます。Compiled Pass OrderへSortします。

## 106. よくある失敗：Pool無制限成長

Fence完了Entryを再利用できていない、GPUが詰まっている、粒度が細かすぎる等を統計から調べます。

## 107. 実装Checklist

- [ ] Queue/Allocator/Listの役割を区別できる。
- [ ] Type互換性を検証する。
- [ ] 作成直後ListのStateを理解している。
- [ ] Allocator Reset後にList Resetする。
- [ ] Close後のListだけSubmitする。
- [ ] ExecuteとGPU完了を区別する。
- [ ] Allocatorへ最後のFence Valueを保存する。
- [ ] List/Allocatorを複数Threadで共有しない。
- [ ] Submission Orderを依存から決定する。
- [ ] PoolのPeakと待機を計測する。
- [ ] Resize/Shutdown/Device Lostで安全に停止できる。

## 108. 理解確認問題

1. Queue、Allocator、Command Listの役割を説明してください。
2. AllocatorをClose直後にResetできない理由を説明してください。
3. Command ListのResetへAllocatorを渡す理由を説明してください。
4. `ExecuteCommandLists`がGPU完了を意味しない理由を説明してください。
5. Queue SignalとQueue Waitの役割を説明してください。
6. Worker完了順でSubmitしてはいけない理由を説明してください。
7. Allocator Pool EntryへFence Valueが必要な理由を説明してください。
8. CPU記録時間とGPU実行時間の違いを説明してください。

## 109. 章末要点

- QueueはSubmit、Allocatorは記録Memory、ListはCommand記録Interfaceです。
- `Allocator Reset -> List Reset -> Record -> Close -> Execute`の順を守ります。
- Executeは非同期であり、完了はFenceで追跡します。
- GPU完了前のAllocator Resetと参照Data再利用を禁止します。
- Queue/List/Allocator TypeとThread所有権を明示します。
- 複数Threadの記録結果は依存順へ並べてSubmitします。
- PoolはFence ValueでRecycleし、成長と待機を計測します。

## 110. 公式資料

- [Command queues and command lists](https://learn.microsoft.com/en-us/windows/win32/direct3d12/command-queues-and-command-lists)
- [Recording command lists and bundles](https://learn.microsoft.com/en-us/windows/win32/direct3d12/recording-command-lists-and-bundles)
- [ID3D12Device::CreateCommandQueue](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12device-createcommandqueue)
- [ID3D12Device::CreateCommandAllocator](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12device-createcommandallocator)
- [ID3D12Device::CreateCommandList](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12device-createcommandlist)
- [ID3D12CommandAllocator::Reset](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12commandallocator-reset)
- [ID3D12GraphicsCommandList::Reset](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12graphicscommandlist-reset)
- [ID3D12GraphicsCommandList::Close](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12graphicscommandlist-close)
- [ID3D12CommandQueue::ExecuteCommandLists](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12commandqueue-executecommandlists)
- [ID3D12CommandQueue::Wait](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12commandqueue-wait)

次章では、Fence、Win32 Event、Frames in Flight、Frame Resource Ring、CPU/GPU待機を安全に実装します。
