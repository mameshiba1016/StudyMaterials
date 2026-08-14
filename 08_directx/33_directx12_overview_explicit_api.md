# DirectX 12 第1章：DirectX 11との違い・明示的API・全体構造

この章では、DirectX 12が何を明示的に管理するAPIなのかを学びます。DirectX 11との対応、Device、Command Queue、Command Allocator、Command List、Fence、Descriptor、PSO、Resource State、Frame Resourceの全体像を扱います。

## 1. DirectX 12を学ぶ目的

DirectX 12は、CPU Overhead、Multithread Command生成、GPU同期、Memory管理をApplication側で細かく制御するLow-level Graphics APIです。

## 2. 低水準APIとは

Hardwareを直接自由に操作するという意味ではありません。Runtime/Driverが暗黙に行っていたState追跡、同期、Descriptor管理等の責任がApplicationへ移ります。

## 3. D3D11からD3D12への中心変化

```text
D3D11 : Contextへ命令、Runtime/Driverが多くを追跡
D3D12 : Commandを記録し、QueueへSubmitし、同期と寿命を自分で管理
```

## 4. 明示的管理の代価

Performanceを制御しやすくなる代わりに、Resource State、Fence、Descriptor Lifetime、Allocator再利用を間違えるとGPU Crashや画面破損につながります。

## 5. D3D12が自動で高速なわけではない

D3D11と同じ設計を複雑に移植しただけでは速くならない場合があります。Command生成、Batch、Memory、同期をAPI特性に合わせて設計します。

## 6. 全体Object関係

```text
IDXGIFactory
 └─ Adapter
     └─ ID3D12Device
         ├─ Command Queue
         ├─ Command Allocator
         │   └─ Graphics Command List
         ├─ Descriptor Heap
         ├─ Resource / Heap
         ├─ Root Signature
         ├─ Pipeline State Object
         └─ Fence

Swap Chain -> Back Buffers -> QueueへPresent対象を接続
```

## 7. FactoryとAdapter

DXGI FactoryでAdapterを列挙し、Software Adapterを除外しながら必要FeatureをSupportするGPUを選びます。

## 8. ID3D12Device

Resource、Descriptor Heap、Root Signature、PSO、Command Object、Fence等を生成する中心Objectです。Draw Commandを直接発行するContextではありません。

## 9. Feature Level

Device作成時に必要Feature Levelを指定します。Feature Levelだけで全機能を判断せず、個別Featureを`CheckFeatureSupport`で調べます。

## 10. Device Interface Version

`ID3D12Device1`以降の追加Interfaceがあります。OS/SDK/機能要件に合わせてQueryInterfaceし、存在を仮定しません。

## 11. Command Queue

GPUへCommand ListをSubmitするQueueです。D3D11 Immediate ContextのSubmit面が明示Objectになったと考えられます。

## 12. Queue Type

```text
DIRECT  : Graphics、Compute、Copy
COMPUTE : Compute、Copy
COPY    : Copy中心
```

Command List/AllocatorのTypeと整合させます。

## 13. Direct Queue

最初はDirect Queue一つで描画・Compute・Copyを行うと同期を理解しやすくなります。複数Queue最適化は正しい単一Queue実装の後です。

## 14. Command Allocator

Command Listが記録するCommand DataのBacking Memoryを管理します。Allocator自体へDrawを書き込むわけではありません。

## 15. Command List

GPU CommandをCPU上で記録するObjectです。Resetして記録し、CloseしてQueueへExecuteします。

```text
Allocator Reset
 -> Command List Reset
 -> Barrier/Clear/Drawを記録
 -> Command List Close
 -> Queue ExecuteCommandLists
```

## 16. Allocator再利用の絶対条件

そのAllocatorを使って記録したCommandをGPUが実行し終えるまで`Reset`してはいけません。完了確認にはFenceを使います。

## 17. Command List再利用

Command List ObjectはGPU完了前でも別Allocatorを使ってReset可能な場合がありますが、Allocatorと参照Resourceの寿命条件を正しく管理します。

## 18. Close

記録を終えたCommand Listを`Close`してからQueueへ渡します。HRESULTを確認し、Debug Layer Messageを調べます。

## 19. ExecuteCommandLists

```cpp
ID3D12CommandList* lists[] = { commandList.Get() };
commandQueue->ExecuteCommandLists(1, lists);
```

この呼出しはGPU完了を待ちません。CPUは通常すぐ次の処理へ進みます。

## 20. CPUとGPUの非同期性

```text
CPU : Frame 2を記録中
GPU : Frame 0を実行中
Display : Frame -1を表示中
```

複数Frameが同時進行するため、Dataを早く上書きしてはいけません。

## 21. Fence

Queue上の進行地点へ単調増加ValueをSignalし、GPUがどこまで完了したかCPUまたは別Queueが確認する同期Objectです。

## 22. Fence Signal

```cpp
const UINT64 value = nextFenceValue++;
ThrowIfFailed(commandQueue->Signal(fence.Get(), value));
```

このSignalもQueueへ順番に積まれます。

## 23. Completed Value

```cpp
if (fence->GetCompletedValue() >= value)
{
    // valueまでのGPU処理が完了済み。
}
```

Fence Valueの大小で完了を判断します。

## 24. CPU Wait

未完了なら`SetEventOnCompletion`とEventで待てます。毎Frame無条件に待つとCPU/GPU並列性を失います。

## 25. GPU Queue Wait

Queue同士の依存にはQueue側の`Wait`を使えます。CPUを起こさずGPU Timeline上で同期できます。

## 26. Frame Resource

FrameごとにGPU使用中Dataを分離する構造です。

```cpp
struct FrameResource
{
    ComPtr<ID3D12CommandAllocator> allocator;
    ComPtr<ID3D12Resource> constantUpload;
    UINT64 fenceValue = 0;
};
```

## 27. Frame Count

Swap Chain Buffer CountとFrame Resource Countを関連付ける設計が一般的ですが、必ず同一概念とは限りません。Latency、Memory、Queue設計で決めます。

## 28. Frame開始時の待機

次に使うFrame ResourceのFence Valueが未完了なら、そのFrame Resourceだけ待ちます。GPU全体を毎回Idleにしません。

## 29. Resourceとは

BufferやTextureを表す`ID3D12Resource`です。用途はDescription、Heap、Resource State、View Descriptorで決まります。

## 30. ResourceとView

Resource本体とCBV/SRV/UAV/RTV/DSVは別です。ViewはDescriptorとしてHeapへ格納されます。

## 31. Descriptor

GPU Resourceを特定用途で参照するための小さな記述Dataです。D3D11のView Objectに相当する情報がDescriptor Heap内のSlotになります。

## 32. Descriptor Heap

連続したDescriptor Slotの配列です。TypeとShader Visibilityに応じたHeapを作ります。

## 33. Heap Type

```text
CBV_SRV_UAV : Constant/Shader Resource/UAV
SAMPLER     : Sampler
RTV         : Render Target View
DSV         : Depth Stencil View
```

異なるTypeを同じHeapへ混在させません。

## 34. Shader-visible Heap

CBV/SRV/UAVとSampler HeapはShaderから参照可能にできます。RTV/DSV HeapはShader-visibleではありません。

## 35. CPU Descriptor Handle

CPUがDescriptorを書込み、RTV/DSVをBindingする際等に使うHandleです。通常のCPU PointerとしてDereferenceしません。

## 36. GPU Descriptor Handle

Shader-visible Heap内のDescriptor TableをGPUへ示すHandleです。CPU Handleとの変換を勝手に仮定しません。

## 37. Descriptor Increment Size

Heap Typeごとに`GetDescriptorHandleIncrementSize`で取得します。`sizeof`や固定値でSlot Offsetを計算しません。

## 38. Descriptor Lifetime

GPUが参照中のDescriptor Slotを別Resource用に上書きしてはいけません。Frame Ring、Persistent領域、Free List等で寿命を管理します。

## 39. Root Signature

Shaderが使うResource Binding Interfaceを定義します。Root Parameter、Descriptor Table、Root Constant、Static Sampler等を宣言します。

## 40. Root Parameter

- Descriptor Table
- Root Descriptor
- Root Constants

更新頻度、Data量、Hardware Costを考慮して使い分けます。

## 41. Descriptor Table

Shader-visible Heap内の連続RangeをRoot Signatureから参照します。Texture群やCBV/SRV/UAV集合に使います。

## 42. Root Descriptor

BufferのGPU Virtual Addressを直接Rootへ設定します。Texture SRV等には使えない等の制約を確認します。

## 43. Root Constants

少量の32-bit値をCommandへ直接格納します。大きな定数配列を置かずRoot Signature Size Budgetを守ります。

## 44. Root Signatureの互換性

PSOとShaderが期待するRoot Signature Layoutを一致させます。Binding規約をRenderer全体で統一します。

## 45. Pipeline State Object

PSOはShader、Blend、Rasterizer、Depth/Stencil、Input Layout、Primitive Type、Render Target Format等をまとめた不変Objectです。

## 46. D3D11 Stateとの比較

```text
D3D11 : Shader/Blend/Rasterizer/Depth Stateを個別Bind
D3D12 : 主要Pipeline StateをPSOとして事前生成しBind
```

## 47. PSOが不変な理由

DriverがPipeline設定を事前検証・Compileしやすくなります。実行中の細かなState変更を減らす代わりにVariant管理が必要です。

## 48. PSO Variant

Material、Pass、Vertex Layout、RT Format、Depth Format、MSAA等の組合せからKeyを作りCacheします。

## 49. PSO Explosion

すべてのFlag組合せを生成すると数が爆発します。実際に必要な組合せ、Shader Feature設計、Lazy/Create Pipeline Library等を検討します。

## 50. Resource State

Resourceが現在どの用途でAccess可能かを表します。

```text
PRESENT
RENDER_TARGET
DEPTH_WRITE
PIXEL_SHADER_RESOURCE
UNORDERED_ACCESS
COPY_SOURCE / COPY_DEST
```

## 51. Resource Barrier

用途変更やMemory順序をGPUへ明示します。D3D11 Runtimeの暗黙Hazard管理がD3D12ではApplication責任になります。

## 52. Transition Barrier

```cpp
D3D12_RESOURCE_BARRIER barrier{};
barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
barrier.Transition.pResource = backBuffer;
barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

commandList->ResourceBarrier(1, &barrier);
```

## 53. StateBeforeを正しく追跡する

実際のStateと`StateBefore`が違えば未定義結果やDebug Errorになります。Resource/ SubresourceごとのState Trackerを設計します。

## 54. Subresource State

MipやArray Sliceごとに別Stateを持てます。常に全Subresource Transitionすると不要な同期が増える場合があります。

## 55. UAV Barrier

同じUAV Resourceへの前後Access順序を保証するために使います。StateがUAVのままでも必要になる場合があります。

## 56. Aliasing Barrier

同じHeap Memoryを異なるPlaced Resourceで再利用する際の境界を示します。高度なMemory Aliasingで使います。

## 57. Split Barrier

TransitionをBegin/Endへ分割できる仕組みがあります。正しい通常Barrierの後で、Overlap最適化として検討します。

## 58. HeapとMemory

D3D12ではResourceのMemory配置も明示的です。Committed、Placed、Reserved Resourceがあります。

## 59. Committed Resource

Resourceと専用Heap Allocationを一度に作る扱いやすい方式です。学習初期と大きな永続Resourceに適します。

## 60. Placed Resource

Applicationが作ったHeapのOffsetへResourceを配置します。Memory Pool、Aliasing、Fragmentation管理が必要です。

## 61. Reserved Resource

Virtual Address範囲とPhysical Tile Mappingを分けるTiled Resource用途です。Streaming等の発展機能です。

## 62. Heap Type

```text
DEFAULT  : GPU Local中心、CPUから直接書けない
UPLOAD   : CPU書込み、GPU読取り
READBACK : GPU書込み結果をCPUで読む
```

## 63. Default Heap

Texture、Vertex/Index Buffer等の本番Resourceを置きます。初期Dataは通常Upload HeapからCopyします。

## 64. Upload Heap

CPUからMapして書けます。一般に`GENERIC_READ` Stateで使用し、GPU Read向けです。大容量Textureを永続的にUpload Heapへ置きません。

## 65. Readback Heap

Copy DestinationとしてGPU結果を受け、Fence完了後にCPUがMapします。即時待機を避けます。

## 66. Uploadの流れ

```text
CPU Data
 -> Upload Resourceへ書込み
 -> Copy Command記録
 -> Default ResourceへCopy
 -> 必要StateへTransition
 -> Fence完了までUpload Dataを保持
```

## 67. GPU Virtual Address

Buffer ResourceはGPU Virtual Addressを取得できます。CBV、Vertex/Index Buffer View、Root Descriptor等で使用します。

## 68. Constant Buffer Alignment

CBVのSize/Addressは256-byte Alignment要件を守ります。構造体の実SizeとAllocation Pitchを区別します。

## 69. Swap Chain

DXGI Swap Chain自体はD3D11と共通概念ですが、D3D12ではCommand Queueを使って作成し、各Back Buffer Stateを明示管理します。

## 70. Back Buffer State

Frame開始時に`PRESENT -> RENDER_TARGET`、描画後に`RENDER_TARGET -> PRESENT`へTransitionするのが基本です。

## 71. RTV Descriptor

Swap Chainの各Back BufferへRTV Descriptorを作成します。Back Buffer Count分のResourceとDescriptorを保持します。

## 72. PresentとFence

Present後にQueueへFence Signalし、そのFrame Resource/Back Bufferに対応するValueを保存します。

## 73. 最小Frame Loop

```text
1. Current Back Buffer/Frame Resourceを選ぶ
2. 必要ならそのFenceを待つ
3. AllocatorをReset
4. Command ListをReset
5. PRESENT -> RENDER_TARGET
6. RTVをClearしてDraw
7. RENDER_TARGET -> PRESENT
8. Command ListをClose
9. QueueへExecute
10. Present
11. FenceをSignalしてValue保存
```

## 74. D3D11との対応表

| DirectX 11 | DirectX 12 |
|---|---|
| Immediate Context | Command Queue + Command List |
| Deferred Context | 複数Command List記録 |
| View Object | Descriptor Heap内Descriptor |
| 個別Pipeline State | PSO + Root Signature |
| 暗黙Resource State | 明示Resource Barrier |
| Driver内部同期 | Fence/Queue Wait |
| Driver Memory管理 | Heap/Allocationをより明示管理 |

## 75. Threading Model

複数ThreadがそれぞれAllocatorとCommand Listを持ち、並列記録できます。一つのAllocator/Listを同時共有しません。

## 76. Command List Pool

Worker/Frame/Queue TypeごとにAllocatorを管理し、Fence完了後にRecycleします。無制限に生成しません。

## 77. Submission Order

並列記録の完了順ではなく、Frame Graph依存に基づいてCommand ListをQueueへ並べます。

## 78. Bundle

繰り返す一部CommandをBundleへ記録できますが、制約と効果を測定します。最初から必須ではありません。

## 79. Copy Queue

Asset UploadをGraphics Queueから分離できます。Copy完了をGraphics QueueがFence WaitしてからResourceを使います。

## 80. Compute Queue

Async Compute候補を実行できますが、Resource依存、Queue間Fence、Hardwareの同時実行特性を理解する必要があります。

## 81. Queueを増やすCost

Queue間同期、Resource所有、Debug、Memory Peakが複雑になります。PassをCompute Queueへ移せば必ず高速になるわけではありません。

## 82. Debug Layer

D3D12 Debug LayerをDevice作成前に有効化し、Resource State、Command List、Descriptor、Lifetime Errorを検出します。

## 83. GPU-based Validation

ShaderからのDescriptor Access等、CPU Debug Layerだけでは見つけにくい問題を検出できます。Overheadが大きいためDebug用途で使用します。

## 84. DRED

Device Removed Extended DataはGPU Fault時のBreadcrumbやPage Fault情報を診断する機能です。Device作成前の設定とCrash Log保存を設計します。

## 85. PIX

Command Queue、Command List、Barrier、Descriptor、Resource Lifetime、GPU TimingをCaptureで確認します。Object NameとEvent Markerを付けます。

## 86. HRESULTだけでは足りない

多くのGPU ErrorはCommand記録時ではなく後の実行で表面化します。Debug Layer、DRED、PIX、Fence地点、Pass Markerを組み合わせます。

## 87. Device Removed

既存Resource、Heap、Descriptor、PSO等を使えません。D3D11と同様にCPU Asset Dataと再生成可能なRegistryが必要です。

## 88. Resize

対象Back BufferをGPUが使い終えたことをFenceで保証し、参照を解放して`ResizeBuffers`後にResource/RTVを再取得します。

## 89. Lifetimeの三段階

```text
CPU Object寿命
Command Listが参照する寿命
GPU実行が完了するまでの寿命
```

Scopeを抜けたから解放可能とは限りません。

## 90. Deferred Release Queue

Resourceを即解放せず、最後に使ったFence Valueと共にQueueへ入れ、完了後に解放します。

```cpp
struct DeferredRelease
{
    UINT64 fenceValue;
    ComPtr<IUnknown> object;
};
```

## 91. DescriptorのDeferred Free

Descriptor SlotもGPU参照中は再利用できません。ObjectだけでなくSlotのFreeもFence完了後に行います。

## 92. Frame Graphとの接続

PassのRead/Write宣言からResource State、Barrier、Queue依存、Lifetime、Transient Allocationを生成できます。

## 93. State Tracker

Graph CompilerまたはBackendがResource/Subresourceの現在Stateを追跡し、必要なTransitionだけを挿入します。

## 94. Barrier Batch

複数Barrierをまとめて`ResourceBarrier`へ渡せます。Pass境界で必要Barrierを集約します。

## 95. Transient Heap

Lifetimeが重ならないResourceを同じHeap Memoryへ配置できます。Alignment、Aliasing Barrier、Peak解析が必要です。

## 96. Fast Action Rendererへの価値

大量Character、Particle、Shadow、Post ProcessのCommandを並列生成し、Frame ResourceとDescriptorを明示管理してCPU OverheadとStutterを制御できます。

## 97. Gameplayとの境界

D3D12へ移行してもHit、AI、Animation StateのOwnerは変わりません。Renderer BackendのAPI変更をGameplayへ漏らしません。

## 98. 最初に作る範囲

1. Debug LayerとAdapter列挙。
2. Device、Direct Queue、Fence。
3. Swap ChainとRTV Heap。
4. Allocator、Command List。
5. Back Buffer Barrier、Clear、Present。
6. Frame Resource Ring。
7. Root Signature、PSO、Triangle。
8. Upload BufferとTexture。

## 99. 最初は作らない高度機能

複数Queue、Placed Resource Allocator、Bindless、Mesh Shader、Raytracing、Async Computeは、単一Queueの正しいFrame Loopと同期を理解した後に進みます。

## 100. よくある失敗：Allocatorを早くReset

GPUがまだCommandを読んでいるAllocatorをResetすると未定義動作になります。Frame ResourceのFenceを待ちます。

## 101. よくある失敗：Barrier不足

Present状態のBack BufferへRTV書込み、Copy DestinationのままShader読取り等を行います。Passごとの必要Stateを宣言します。

## 102. よくある失敗：Descriptor上書き

GPU参照中のShader-visible Slotを次Frame用に書き換えます。Frame/Fenceに基づくDescriptor Allocationを使います。

## 103. よくある失敗：毎FrameGPU待機

Frame末尾でGPU Idleを待つとFrames in Flightが消えます。再利用対象のFrame Resourceだけ必要時に待ちます。

## 104. よくある失敗：StateBeforeを推測

直前のCodeだけ見てStateを決めると別PassやSubresourceで破綻します。中央State TrackerまたはGraphで追跡します。

## 105. よくある失敗：ObjectとGPU寿命を同一視

C++ Ownerが破棄されてもGPUがCommand内で参照している場合があります。Deferred Releaseを使います。

## 106. よくある失敗：D3D11のGlobal Context設計を移植

どこからでもCommand ListやHeapを触ると同期とState追跡が崩れます。Pass、Frame、Queue、Threadごとの所有権を決めます。

## 107. 学習Checklist

- [ ] DeviceとCommand Queueの役割を区別できる。
- [ ] AllocatorとCommand Listの関係を説明できる。
- [ ] Fence ValueでGPU完了を判定できる。
- [ ] Frame Resourceが必要な理由を説明できる。
- [ ] Descriptor Heap Typeを区別できる。
- [ ] Root SignatureとPSOの役割を説明できる。
- [ ] Resource StateとBarrierを説明できる。
- [ ] Default/Upload/Readback Heapを使い分けられる。
- [ ] Descriptor/ResourceをFence完了まで保持できる。
- [ ] D3D11との責任範囲の違いを説明できる。

## 108. 理解確認問題

1. D3D12が明示的APIと呼ばれる理由を説明してください。
2. Command AllocatorをGPU完了前にResetできない理由を説明してください。
3. Command List実行とGPU完了が別である理由を説明してください。
4. Descriptor HeapとResource本体の違いを説明してください。
5. Root SignatureとPSOの違いを説明してください。
6. Back Bufferに必要な二つのTransitionを説明してください。
7. Upload HeapからDefault HeapへDataを渡す流れを説明してください。
8. Deferred Release Queueが必要な理由を説明してください。

## 109. 章末要点

- D3D12ではCommand、同期、Descriptor、Resource State、Memoryの責任がApplicationへ移ります。
- Command Listを記録してQueueへSubmitし、FenceでGPU完了を追跡します。
- Frame ResourceにAllocatorと一時Dataを分け、GPU使用中の上書きを防ぎます。
- Root SignatureがBinding Interface、PSOがPipeline設定を表します。
- Resource Barrierで用途変更とAccess順序を明示します。
- ResourceとDescriptorの寿命をFence Valueへ結び付けます。
- 最初はDirect Queue一つとCommitted Resourceで正しいFrame Loopを完成させます。

## 110. 公式資料

- [Direct3D 12 programming guide](https://learn.microsoft.com/en-us/windows/win32/direct3d12/directx-12-programming-guide)
- [Important changes from Direct3D 11 to Direct3D 12](https://learn.microsoft.com/en-us/windows/win32/direct3d12/important-changes-from-directx-11-to-directx-12)
- [Command queues and command lists](https://learn.microsoft.com/en-us/windows/win32/direct3d12/command-queues-and-command-lists)
- [Creating command queues and command lists](https://learn.microsoft.com/en-us/windows/win32/direct3d12/recording-command-lists-and-bundles)
- [Synchronization and multi-engine](https://learn.microsoft.com/en-us/windows/win32/direct3d12/user-mode-heap-synchronization)
- [Descriptor heaps overview](https://learn.microsoft.com/en-us/windows/win32/direct3d12/descriptor-heaps-overview)
- [Root signatures overview](https://learn.microsoft.com/en-us/windows/win32/direct3d12/root-signatures-overview)
- [Managing graphics pipeline state](https://learn.microsoft.com/en-us/windows/win32/direct3d12/managing-graphics-pipeline-state-in-direct3d-12)
- [Using resource barriers](https://learn.microsoft.com/en-us/windows/win32/direct3d12/using-resource-barriers-to-synchronize-resource-states-in-direct3d-12)
- [Fence-based resource management](https://learn.microsoft.com/en-us/windows/win32/direct3d12/fence-based-resource-management)

次章では、Windows SDK設定、Debug Layer、DXGI Factory、Adapter列挙、Feature Support、D3D12 Device生成を実際の初期化順で扱います。
