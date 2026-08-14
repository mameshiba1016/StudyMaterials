# DirectX 12 第4章：Fence・Event・Frame Resource

この章では、CPUとGPUの非同期実行を安全に管理します。Fence Value、Queue Signal/Wait、Win32 Event、Frames in Flight、Frame Resource Ring、Allocator/Upload/Descriptorの再利用、Deferred Release、待機診断までを扱います。

## 1. なぜ同期が必要か

CPUがCommandを記録・Submitした後もGPUは非同期で処理します。CPUがResourceを早く上書き・解放・ResetするとGPUが壊れたDataを読みます。

## 2. 同期の目的

```text
順序保証 : Producer完了後にConsumerを実行する
再利用   : GPU使用完了後にMemory/Objectを再利用する
解放     : GPU参照終了後にResourceを破棄する
待機     : CPUがGPU結果を必要とする地点だけ待つ
```

## 3. Fenceとは

`ID3D12Fence`は単調に進む64-bit ValueでGPU Timelineの進行を表す同期Objectです。

## 4. FenceはFrame番号ではない

Frame IDとFence Valueを同じ数値にする必要はありません。一つのFrameで複数Signalする場合もあります。

## 5. CreateFence

```cpp
ComPtr<ID3D12Fence> fence;
ThrowIfFailed(
    device->CreateFence(
        0,
        D3D12_FENCE_FLAG_NONE,
        IID_PPV_ARGS(&fence)),
    "ID3D12Device::CreateFence");

fence->SetName(L"Direct Queue Fence");
```

Initial Valueを0として開始する例です。

## 6. Fence Flags

通常は`D3D12_FENCE_FLAG_NONE`です。Shared/Cross-adapter等はProcess/Adapter間共有の要件がある場合だけ使います。

## 7. 次のFence Value

```cpp
UINT64 nextFenceValue = 1;
```

Signalごとに一意で増加する値を払い出します。

## 8. Queue Signal

```cpp
const UINT64 value = nextFenceValue++;
ThrowIfFailed(
    commandQueue->Signal(fence.Get(), value),
    "ID3D12CommandQueue::Signal");
```

QueueでSignalより前のCommandが完了するとFenceがそのValueへ進みます。

## 9. Signal呼出し直後

CPUが`Signal`を呼んだ直後にFenceが完了済みとは限りません。Signal自体がQueue Timelineへ積まれます。

## 10. GetCompletedValue

```cpp
const UINT64 completed = fence->GetCompletedValue();
if (completed >= requiredValue)
{
    // requiredValue以前のQueue Workは完了している。
}
```

等値ではなく`>=`で判定します。

## 11. UINT64_MAXへの注意

Device Removed等の特殊状態では戻り値の意味を公式仕様で確認し、通常完了値として扱わない診断Pathを用意します。

## 12. CPU Polling

毎Loop`GetCompletedValue`を呼ぶBusy WaitはCPU Coreを消費します。短時間Spinの設計をする場合も計測と上限が必要です。

## 13. Win32 Event

Fenceが指定Valueへ到達したときCPU Threadを起こすEventを使えます。

## 14. CreateEvent

```cpp
HANDLE fenceEvent = CreateEventW(
    nullptr,
    FALSE,
    FALSE,
    nullptr);

if (!fenceEvent)
    ThrowLastError("CreateEventW");
```

Auto-reset、初期Non-signaledのEvent例です。

## 15. HANDLEのRAII

```cpp
struct HandleCloser
{
    void operator()(void* handle) const noexcept
    {
        if (handle) CloseHandle(handle);
    }
};
```

実装では`HANDLE`に適したUnique Handle Wrapperを使い、終了時に必ず`CloseHandle`します。

## 16. SetEventOnCompletion

```cpp
ThrowIfFailed(
    fence->SetEventOnCompletion(requiredValue, fenceEvent),
    "ID3D12Fence::SetEventOnCompletion");
```

FenceがValueへ到達したらEventをSignaledにします。

## 17. WaitForSingleObject

```cpp
const DWORD result = WaitForSingleObject(fenceEvent, INFINITE);
if (result != WAIT_OBJECT_0)
    HandleWaitFailure(result);
```

戻り値を確認します。

## 18. 完全なCPU Wait Helper

```cpp
void WaitForFence(
    ID3D12Fence* fence,
    UINT64 value,
    HANDLE eventHandle)
{
    if (fence->GetCompletedValue() >= value)
        return;

    ThrowIfFailed(
        fence->SetEventOnCompletion(value, eventHandle),
        "SetEventOnCompletion");

    const DWORD result = WaitForSingleObject(eventHandle, INFINITE);
    if (result != WAIT_OBJECT_0)
        ThrowWaitError(result);
}
```

## 19. Timeout

開発Buildでは有限Timeoutを使い、Hang時にDRED、Device Removed Reason、最後のPassを保存する設計も有効です。

## 20. INFINITEのTrade-off

正常時は単純ですが、GPU Hangで永久停止する可能性があります。製品ではTimeout後の診断・復旧Policyを決めます。

## 21. Event再利用

一つのQueue同期Helperが直列に待つならEventを再利用できます。複数Threadが同時に同じEventへ待機登録しないよう所有権を決めます。

## 22. 複数同時Wait

Threadごと/RequestごとにEventを持つか、Completion管理Systemを作ります。Shared Auto-reset Eventの取り合いを避けます。

## 23. Queue Wait

```cpp
ThrowIfFailed(
    graphicsQueue->Wait(copyFence.Get(), copyValue),
    "Graphics Queue Wait");
```

GPU Queue同士を同期し、CPU ThreadをBlockしません。

## 24. CPU WaitとQueue Wait

```text
CPU Wait   : CPUがGPU結果/再利用完了を待つ
Queue Wait : GPU Queue BがQueue Aの地点を待つ
```

目的に合わせます。

## 25. CPU Signal

Fence InterfaceにもCPU側`Signal`があります。通常のQueue進行追跡はCommand Queue Signalを使い、用途を混同しません。

## 26. Fence Value規則

- 0を初期値にする。
- Signalごとに増加させる。
- 使い回さない。
- Queue/TimelineごとのOwnerを決める。
- Overflowを理論上考慮する。

## 27. Fence Value Allocator

複数Threadが同じQueueへSubmitするならValue払い出しとSubmit順を一つのSubmission Managerへ集約します。

## 28. Signal順とValue順

Queueへ10、後から9のようなValueをSignalする設計を避けます。単調増加とSubmit順を一致させます。

## 29. QueueごとのFence

Direct、Compute、Copy QueueごとにFenceと次Valueを持つとTimelineの意味が明確になります。

## 30. 一つのFenceを複数Queueで使う場合

可能な操作でもValue所有と順序が複雑になります。初期設計ではQueueごとに分離します。

## 31. Frames in Flight

CPU、GPU、Displayが異なるFrameを同時処理している状態です。

```text
CPU records N+2
GPU executes N+1
Display presents N
```

## 32. Frame Buffering

2～3個等のFrame ResourceをRingで使い、GPU使用中DataとCPU書込みDataを分けます。

## 33. Frame Resourceの内容

```cpp
struct FrameResource
{
    ComPtr<ID3D12CommandAllocator> directAllocator;
    ComPtr<ID3D12Resource> constantUpload;
    std::byte* constantCpuBase = nullptr;
    DescriptorRange transientDescriptors;
    UINT64 fenceValue = 0;
};
```

## 34. Frame Resourceに入れるもの

- Command Allocator
- Dynamic Constant/Vertex/Instance Upload領域
- Frame固有Descriptor範囲
- Query範囲
- 一時CPU Allocator
- Deferred Release Listへの参照

## 35. Frame Resourceに入れないもの

全Frameで不変のTexture、PSO、Root Signature等は永続Registryで共有できます。何でも複製しません。

## 36. Current Frame Index

```cpp
FrameResource& frame = frames[currentFrameIndex];
```

Swap Chain Back Buffer Indexとの関係を意識します。

## 37. Frame開始

```cpp
void BeginFrame(FrameResource& frame)
{
    if (frame.fenceValue != 0)
        WaitForFence(fence.Get(), frame.fenceValue, fenceEvent.Get());

    ThrowIfFailed(frame.directAllocator->Reset(), "Frame Allocator Reset");
    frame.ResetTransientData();
}
```

## 38. Frame終了

```cpp
const UINT64 submittedValue = nextFenceValue++;
ThrowIfFailed(queue->Signal(fence.Get(), submittedValue), "Frame Signal");
frame.fenceValue = submittedValue;
```

そのFrame Resourceを最後に使ったValueを保存します。

## 39. Ring進行

```cpp
currentFrameIndex = (currentFrameIndex + 1) % frameCount;
```

次に使うSlotだけFenceを確認します。

## 40. Back Buffer Indexを使う方式

`swapChain->GetCurrentBackBufferIndex()`で対応Frame Resourceを選ぶ設計があります。Swap ChainとFrame Resource Count/意味を一致させます。

## 41. 独立Frame Index方式

CPU Frame Resource Ringを独立管理することもできます。Back BufferとのLifetime対応表を明示します。

## 42. Frame CountのTrade-off

```text
少ない : Memory/Latencyを抑えるがCPU待機が増え得る
多い   : OverlapしやすいがMemoryと入力Latencyが増え得る
```

## 43. Frame Resourceを待つ瞬間

Ringで再利用しようとするSlotが未完了のときだけ待ちます。前FrameをSubmitするたびに待ちません。

## 44. 毎FrameWait Idleの問題

CPUとGPUが直列化し、D3D12のFrames in Flightが失われます。Debug初期段階以外は避けます。

## 45. Wait Idle Helper

```cpp
void WaitForQueueIdle()
{
    const UINT64 value = nextFenceValue++;
    ThrowIfFailed(queue->Signal(fence.Get(), value), "Idle Signal");
    WaitForFence(fence.Get(), value, fenceEvent.Get());
}
```

## 46. Wait Idleを使う場所

- Shutdown
- 大規模Resource再構築
- 実装初期の単純化
- 一部Resize/Device再生成
- Debug Capture前後の必要地点

通常Frame末尾には置きません。

## 47. Allocator再利用

Frame ResourceのFence完了後にAllocatorをResetします。Allocator単独で別Fence規則を持たせずOwnerを明確にします。

## 48. Upload Memory再利用

GPUがConstant/Vertex Dataを読み終えるまで同じ領域を上書きしません。FrameごとのUpload Sliceを使います。

## 49. Persistent Map

Upload Resourceを一度MapしてCPU Pointerを保持できます。Resource解放までPointerの有効性と書込み範囲を管理します。

## 50. Constant Allocation

```cpp
constexpr UINT Align256(UINT size)
{
    return (size + 255u) & ~255u;
}
```

CBV Alignmentを守り、Frame Upload Cursorを進めます。

## 51. Upload Overflow

Frame内Upload Capacityを超えたらAssert/追加Page/品質低下等のPolicyを定義します。Buffer越境を許しません。

## 52. Descriptor再利用

Shader-visible Descriptor SlotもGPU参照中は上書きできません。Frame ResourceごとのTransient範囲を使います。

## 53. Query再利用

Timestamp/Occlusion Query RangeとReadback BufferもFrame/Fence単位で分離します。

## 54. CPU Scratch再利用

GPUとは無関係なCPU一時Memoryは記録Job完了後に再利用できます。GPU参照Dataと同じLifetimeにしすぎないよう分類します。

## 55. Resource Lifetime表

| Data | 再利用可能地点 |
|---|---|
| CPU-only Render Item | Command記録完了後 |
| Command Allocator | GPU Fence完了後 |
| Upload Data | GPU Fence完了後 |
| Shader-visible Descriptor | GPU Fence完了後 |
| Default Texture | 最終GPU使用Fence完了後 |

## 56. Deferred Release

Resourceを即解放せず、最後に使用したFence Valueと共に保留します。

```cpp
struct DeferredReleaseItem
{
    UINT64 retireFenceValue;
    ComPtr<IUnknown> object;
};
```

## 57. Deferred Release処理

```cpp
void CollectCompleted(UINT64 completedValue)
{
    while (!items.empty() &&
           items.front().retireFenceValue <= completedValue)
    {
        items.pop_front();
    }
}
```

Value順にQueueへ入れる前提です。

## 58. Descriptor Deferred Free

Descriptor AllocationにもRetire Fenceを付け、完了後にFree Listへ戻します。

## 59. PSO/Root Signature解放

Hot Reloadで差し替えても旧Objectを参照するCommandが残る可能性があります。Deferred Release対象です。

## 60. Upload Staging解放

Texture CopyをSubmitした後、Copy完了FenceまでUpload Resourceを保持します。

## 61. Resource Owner破棄

Gameplay/Asset Handleが消えてもGPU使用中ならPhysical ObjectをDeferred Queueへ移します。

## 62. Fence値とObject最終使用

Objectを使用する最後のSubmit Batchに対応するFence Valueを記録します。推測した現在Valueを使いません。

## 63. Multiple Queue Lifetime

Resourceを複数Queueが使う場合、すべての最終使用完了を保証してから解放します。QueueごとのFence Valueを持つ設計があります。

## 64. CopyからGraphicsへの流れ

```text
Copy Queue: Upload Copy -> Signal CopyFence C
Graphics Queue: Wait C -> Draw Resource -> Signal GraphicsFence G
Release: G完了後
```

最後のConsumer Queueを追跡します。

## 65. ComputeからGraphicsへの流れ

Compute UAV書込み後にCompute FenceをSignalし、Graphics QueueがWaitしてSRV等で読みます。必要State TransitionのQueue/Barrier規則も守ります。

## 66. Fence Deadlock

Queue AがBの未来Valueを待ち、Queue BがAの未来Valueを待つ循環を作ると進みません。Submission GraphでCycleを禁止します。

## 67. CPU Deadlock

Fenceを進めるSubmit Thread自身が、そのSubmit前に同Fenceを待つ等の順序誤りを避けます。

## 68. Event Lost Wakeupの考え方

まずCompleted Valueを確認し、未完了ならEvent登録して待ちます。`SetEventOnCompletion`が既に完了したValueを適切に扱う仕様も理解します。

## 69. Fence Completion Thread

多数の非同期Task完了を一つのThread/Systemで監視する設計もあります。単純RendererではFrame開始時収集から始めます。

## 70. Callback Queue

Fence完了後に実行するCPU CallbackをValue順に登録できます。Callback内の重い処理やMain-thread限定処理を分類します。

## 71. Async Upload Ticket

```cpp
struct UploadTicket
{
    QueueType queue;
    UINT64 fenceValue;
};
```

AssetがReadyかをPolling/Dependencyへ使います。

## 72. Ticket IsComplete

対象Queue FenceのCompleted Valueと比較します。CPU WaitせずFallback Resourceを表示する選択もできます。

## 73. Frame Latency

Frames in Flightが多いとCPUが先行できますが、入力反映までのLatencyが増える可能性があります。

## 74. Swap Chain Waitable Object

Frame Latency Waitable Objectを使うSwap Chain設計では、Present QueueingとCPU Frame開始を制御できます。後章で扱います。

## 75. VSyncとの関係

FenceはGPU Command完了、VSyncはDisplay Present Timingに関係します。同じ待機ではありません。

## 76. Frame Capとの関係

Frame LimiterはCPU/GPU/Display Timing制御です。Resource安全性のFenceを代用しません。

## 77. Low Latency設計

Frame Count、Present設定、Input Sample位置、CPU先行量を一緒に測ります。GPUを常にIdleにすることが唯一の低Latency策ではありません。

## 78. Fence Profiler

Wait開始/終了、対象Value、Completed Value、Frame Slot、待機理由を記録します。

## 79. Wait Reason

```cpp
enum class FenceWaitReason
{
    FrameResourceReuse,
    UploadReadiness,
    Resize,
    Shutdown,
    Readback
};
```

理由別の回数と時間を集計します。

## 80. Wait時間

Frame Resource待機が頻発する場合、GPU Bottleneck、Frame Count不足、CPUのBurst、過剰Queue同期を調べます。

## 81. Fence Gap

Submitted ValueとCompleted Valueの差を監視し、GPUの遅れやQueue Depthの目安にします。値差がWork量と比例するとは限りません。

## 82. Frame Resource Memory統計

各FrameのUpload使用量、Descriptor使用数、Query数、Deferred Release数、Peakを表示します。

## 83. Debug Name

Fence、Frame Allocator、Upload Page、Descriptor範囲へFrame/Queue/Indexを含む名前を付けます。

## 84. DREDとの接続

Timeout/Device Removed時に最後のSubmitted Fence、Completed Fence、Frame/Pass MarkerをCrash Logへ保存します。

## 85. Device Removed

Fence待機が永遠に完了しない可能性があります。Timeout後に`GetDeviceRemovedReason`とDREDを確認します。

## 86. Resize

対象Back Buffer/Size-dependent Resourceの最終使用Fenceを待ちます。単純実装ではQueue Idle後にResizeできます。

## 87. Shutdown

新規Submitを止め、WorkerをJoinし、QueueへSignalして完了を待ち、Deferred Releaseを収集後にEvent/Fence/Queueを破棄します。

## 88. Event破棄順

Wait中Threadが残っていないことを保証してからHandleを閉じます。

## 89. Fence Overflow

64-bit値は非常に大きいですが、Long-running SystemではWrapを理論上考慮します。0や予約値との比較規則を文書化します。

## 90. Thread Safety

Next Fence Value、Frame Index、Deferred Queueへ複数Threadが触る場合、Submission Managerへ集約するか同期します。

## 91. SignalとRecordの分離

WorkerはCommandを記録し、Queue Owner Threadが順序決定・Execute・Signalを行うとValue管理が単純になります。

## 92. Frame Coordinator

```cpp
class FrameCoordinator
{
public:
    FrameResource& BeginFrame();
    UINT64 EndFrame(std::span<ID3D12CommandList*> lists);
    void WaitIdle(FenceWaitReason reason);
    void CollectGarbage();
};
```

## 93. BeginFrameの責務

Frame Slot選択、Fence確認/待機、Allocator/Upload/Descriptor/Query Reset、Completed Garbage収集を行います。

## 94. EndFrameの責務

List Execute、Presentとの順序、Fence Signal、Frame Fence保存、統計確定を行います。

## 95. Unit Test

Fence Value払い出し、`>=`比較、Frame Ring、Deferred Queue収集、複数Queue最終使用、Wait Reason統計をGPUなしでTestします。

## 96. Integration Test

GPUへ空/短いCommand ListをSubmitし、Signal/Event Wait、Allocator再利用を繰り返してDebug Layer Message 0件を確認します。

## 97. Stress Test

GPU負荷を増やしCPUを先行させ、Frame Ring Wrap、Upload Overflow、Resize、Shutdown、Timeout診断を試します。

## 98. よくある失敗：Signal直後に完了扱い

Queue Signalは非同期です。Completed ValueかEventで確認します。

## 99. よくある失敗：等値比較

Fenceが要求値を超えて進んでいる正常状態を未完了と判定します。`completed >= required`を使います。

## 100. よくある失敗：毎FrameIdle待機

CPU/GPUを直列化します。再利用するFrame Slotだけ待ちます。

## 101. よくある失敗：一つのEventを同時共有

複数WaiterがAuto-reset Eventを取り合います。Event所有と同時待機Policyを決めます。

## 102. よくある失敗：FenceなしDeferred Free

数Frame後なら安全と推測して解放します。実際の最終使用Fenceへ結び付けます。

## 103. よくある失敗：Queue間CPU順序だけ

Copyを先にSubmitしたからGraphicsが後で読むと仮定します。Queue Signal/Waitを入れます。

## 104. よくある失敗：Frame IDをFence値に固定

UploadやQueue間同期で複数Signalが必要になり破綻します。別Counterとして関連付けます。

## 105. よくある失敗：Wait結果未確認

`WAIT_FAILED`やTimeoutを正常完了として扱います。Win32 ErrorとDevice状態を診断します。

## 106. 実装Checklist

- [ ] Fenceを初期値0で生成できる。
- [ ] Queue Signalごとに単調増加Valueを使う。
- [ ] Completed Valueを`>=`で比較する。
- [ ] Event HandleをRAII管理する。
- [ ] Wait戻り値とTimeoutを処理する。
- [ ] Frame ResourceごとにFence Valueを保存する。
- [ ] 再利用Slotだけ必要時に待つ。
- [ ] Upload/Descriptor/QueryをFrame単位で分離する。
- [ ] Deferred Releaseを最終使用Fenceへ結び付ける。
- [ ] Queue間依存にSignal/Waitを使う。
- [ ] Wait理由と時間をProfilerへ記録する。
- [ ] Shutdown時に全Queue完了後に破棄する。

## 107. 理解確認問題

1. Queue Signal呼出し直後に完了とは限らない理由を説明してください。
2. Completed Valueを`>=`で比較する理由を説明してください。
3. CPU Event WaitとQueue Waitの違いを説明してください。
4. Frame ResourceがAllocator/Upload上書きを防ぐ仕組みを説明してください。
5. 毎Frame Queue Idle待機が遅い理由を説明してください。
6. Deferred Releaseへ最終使用Fenceが必要な理由を説明してください。
7. Copy Queue出力をGraphics Queueで使う同期順を説明してください。
8. Frame CountがLatencyとMemoryへ与える影響を説明してください。

## 108. 章末要点

- FenceはGPU Queue Timelineの完了地点を64-bit Valueで表します。
- Queue Signalは非同期で、Completed ValueまたはEventで完了を確認します。
- CPU WaitとGPU Queue Waitを目的に応じて使い分けます。
- Frame Resource RingでAllocator、Upload、Descriptor、Queryを分離します。
- Ring再利用時にそのSlotのFenceだけを待ち、毎Frame Idleにしません。
- Resource/Descriptorの解放と再利用を最終使用Fenceへ結び付けます。
- 待機理由、時間、Submitted/Completed Valueを計測・診断します。

## 109. 公式資料

- [Fence-based resource management](https://learn.microsoft.com/en-us/windows/win32/direct3d12/fence-based-resource-management)
- [Multi-engine synchronization](https://learn.microsoft.com/en-us/windows/win32/direct3d12/user-mode-heap-synchronization)
- [ID3D12Device::CreateFence](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12device-createfence)
- [ID3D12CommandQueue::Signal](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12commandqueue-signal)
- [ID3D12CommandQueue::Wait](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12commandqueue-wait)
- [ID3D12Fence::GetCompletedValue](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12fence-getcompletedvalue)
- [ID3D12Fence::SetEventOnCompletion](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12fence-seteventoncompletion)
- [CreateEventW](https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-createeventw)
- [WaitForSingleObject](https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-waitforsingleobject)

次章では、DXGI Swap Chain、Back Buffer、RTV Descriptor Heap、Present、Frame Latency、ResizeをD3D12 Frame Loopへ接続します。
