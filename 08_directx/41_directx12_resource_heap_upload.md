# DirectX 12 第9章：Resource・Heap・Upload

この章では、GPU Dataを置くResourceとMemoryを管理するHeapを学びます。Buffer/Texture Description、DEFAULT/UPLOAD/READBACK、Committed/Placed/Reserved Resource、Allocation/Alignment、Map、Suballocation、Vertex/Index/Constant Upload、Fence Lifetimeを扱います。

## 1. 三つの概念

```text
Resource : Buffer/Textureの形と用途
Heap     : GPU Memory Allocation
Upload   : CPU DataをGPU向けResourceへ運ぶ経路
```

## 2. ID3D12Resource

BufferやTextureを表し、Description、GPU Virtual Address、Map等を提供します。

## 3. Resource Description

```cpp
D3D12_RESOURCE_DESC desc{};
desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
desc.Alignment = 0;
desc.Width = byteSize;
desc.Height = 1;
desc.DepthOrArraySize = 1;
desc.MipLevels = 1;
desc.Format = DXGI_FORMAT_UNKNOWN;
desc.SampleDesc = { 1, 0 };
desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
desc.Flags = D3D12_RESOURCE_FLAG_NONE;
```

## 4. Resource Dimension

```text
BUFFER
TEXTURE1D
TEXTURE2D
TEXTURE3D
```

UNKNOWNは完成Resource作成には使いません。

## 5. Buffer Description規則

BufferはHeight/DepthOrArray/Mipが1、Format UNKNOWN、Sample Count 1、Layout ROW_MAJOR等の規則があります。

## 6. Buffer Width

Byte単位のBuffer Sizeです。0、Overflow、Alignment切上げを検証します。

## 7. Texture Description

Width/Height、Array/Depth、Mip、Format、Sample、Layout、Flagsを用途に合わせます。

## 8. DepthOrArraySize

1D/2D TextureではArray Size、3D TextureではDepthを表します。意味をDimensionで区別します。

## 9. Mip Levels

0指定で完全Mip Chainを要求できる場合がありますが、作成/Upload/利用方針を明示します。

## 10. Format

Resource FormatとView Formatを分けて考えます。Typeless ResourceからSRV/RTV/DSV Viewを作る構成があります。

## 11. Sample Description

MSAA ResourceはSample Count/QualityをFeature Queryで検証します。Buffer/通常TextureはCount 1です。

## 12. Texture Layout

通常Textureは`D3D12_TEXTURE_LAYOUT_UNKNOWN`でHardware最適Layoutを使います。Upload/ReadbackのFootprintは別途計算します。

## 13. Row Major Texture

用途・制約のあるLayoutです。通常のGPU TextureをCPU ImageのRow配列と同一と仮定しません。

## 14. Resource Flags

- ALLOW_RENDER_TARGET
- ALLOW_DEPTH_STENCIL
- ALLOW_UNORDERED_ACCESS
- DENY_SHADER_RESOURCE

必要用途だけ指定します。

## 15. FlagはStateではない

Flagは作成用途、Stateは現在Access用途です。UAV Flagを付けても自動でUAV Stateにはなりません。

## 16. Clear Value

Render Target/Depth Resource作成時のOptimized Clear Valueを指定できます。

## 17. Optimized Clear Value

頻繁に使うClear Color/Depthと一致させるとHardware最適化に役立つ場合があります。異なる値でも正しさは維持される仕様を確認します。

## 18. Heap Properties

```cpp
D3D12_HEAP_PROPERTIES properties{};
properties.Type = D3D12_HEAP_TYPE_DEFAULT;
properties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
properties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
properties.CreationNodeMask = 1;
properties.VisibleNodeMask = 1;
```

Single-node例です。

## 19. DEFAULT Heap

GPUアクセスに適したMemoryです。通常CPUからMapして初期Dataを書かず、Upload ResourceからCopyします。

## 20. UPLOAD Heap

CPUが書込み、GPUが読み取るMemoryです。一般に初期Stateは`GENERIC_READ`です。

## 21. READBACK Heap

GPUからCopyしたDataをCPUが読むMemoryです。一般に`COPY_DEST`として使います。

## 22. CUSTOM Heap

CPU Page PropertyとMemory Poolを明示する高度な用途です。Architecture QueryとHardware要件を理解して使います。

## 23. Heap Type比較

| Type | CPU | GPU | 主用途 |
|---|---|---|---|
| DEFAULT | 通常直接Accessしない | 高速Access | Texture、Mesh、Render Target |
| UPLOAD | Write | Read | 初期Data、Dynamic Constant |
| READBACK | Read | Copy Write | Screenshot、Query結果 |

## 24. Resource作成方式

```text
Committed : 専用HeapとResourceを一括生成
Placed    : 自作HeapのOffsetへResourceを配置
Reserved  : Virtual Resourceを作りTile Mappingを別管理
```

## 25. Committed Resource

最も単純で安全です。初期学習、永続大Resource、特殊Heapに向きます。

## 26. CreateCommittedResource

```cpp
ComPtr<ID3D12Resource> resource;
ThrowIfFailed(
    device->CreateCommittedResource(
        &properties,
        D3D12_HEAP_FLAG_NONE,
        &desc,
        initialState,
        optimizedClearValue,
        IID_PPV_ARGS(&resource)),
    "ID3D12Device::CreateCommittedResource");
```

## 27. Initial State

Resource作成直後のStateです。最初のCommand用途とUpload Flowから決めます。

## 28. DEFAULT Buffer初期State

初期Data Copyをするなら`COPY_DEST`で作り、Copy後にVertex/Index/Shader Resource等へTransitionします。

## 29. UPLOAD State

Upload Heap Resourceは通常`GENERIC_READ`で作成し、そのState前提を維持します。

## 30. READBACK State

Readback Heap Resourceは`COPY_DEST`で作り、GPU Copy後Fenceを待ってCPU Mapします。

## 31. Placed Resource

Applicationが作成したHeapの指定OffsetへResourceを配置します。

## 32. CreateHeap

```cpp
D3D12_HEAP_DESC heapDesc{};
heapDesc.SizeInBytes = heapSize;
heapDesc.Alignment = 0;
heapDesc.Properties.Type = D3D12_HEAP_TYPE_DEFAULT;
heapDesc.Flags = D3D12_HEAP_FLAG_ALLOW_ONLY_BUFFERS;

ComPtr<ID3D12Heap> heap;
ThrowIfFailed(
    device->CreateHeap(&heapDesc, IID_PPV_ARGS(&heap)),
    "ID3D12Device::CreateHeap");
```

## 33. Heap Flags

Bufferのみ、非RT/DS Textureのみ、RT/DS Textureのみ等に制限するFlagがあります。Resource Heap TierとAllocator方針に合わせます。

## 34. CreatePlacedResource

```cpp
ComPtr<ID3D12Resource> placed;
ThrowIfFailed(
    device->CreatePlacedResource(
        heap.Get(),
        heapOffset,
        &resourceDesc,
        initialState,
        clearValue,
        IID_PPV_ARGS(&placed)),
    "ID3D12Device::CreatePlacedResource");
```

## 35. Heap Offset Alignment

Resource Allocation Infoが要求するAlignmentへOffsetを揃えます。固定64 KiBだけを無条件に仮定しません。

## 36. GetResourceAllocationInfo

```cpp
D3D12_RESOURCE_ALLOCATION_INFO info =
    device->GetResourceAllocationInfo(
        0,
        1,
        &resourceDesc);
```

SizeInBytesとAlignmentを取得します。

## 37. Allocation Failure検出

無効Desc等で異常値が返る場合を確認し、Overflowを検証します。

## 38. Small Resource Alignment

条件を満たす小Texture等で小さいAlignmentを利用できる場合があります。Eligibilityを公式仕様どおり判定します。

## 39. MSAA Alignment

MSAA Resourceは大きなAlignment要件を持つ場合があります。通常Textureと同じ扱いにしません。

## 40. Heap Suballocation

大きなHeapを作り、複数ResourceへOffset Rangeを割り当てます。Create/Memory Overheadを減らせます。

## 41. Free List

空きBlockをSize/Offsetで管理し、Alignment込みでResourceを配置します。解放時に隣接Blockを結合します。

## 42. Buddy Allocator

Power-of-two Blockへ分割し高速Allocation/結合を行えます。内部Fragmentationを測ります。

## 43. Linear Allocator

Frame内Transient Resourceを順に配置し、Fence後にHeap/Page全体をResetできます。

## 44. Pool分類

Buffer、通常Texture、RT/DS Texture、Upload、Readback等を別Poolへ分けるとHeap Flag/Tier管理が単純になります。

## 45. External Fragmentation

合計空きSizeは十分でも連続Block不足でAllocationできない状態です。

## 46. Internal Fragmentation

Alignment/Block丸めによりAllocation内部で使われないMemoryです。

## 47. Fragmentation統計

Total、Used、Free、Largest Free Block、Allocation Count、Alignment Waste、Peakを記録します。

## 48. Defragmentation

Resourceを新AllocationへCopyし、Descriptor/Handleを切替え、旧ResourceをFence後に解放します。GPU Virtual Address/Index参照更新が必要です。

## 49. Stable Handle

上位は生`ID3D12Resource*`でなくHandleを持ち、Physical Resource移動時にRegistry Mappingを更新します。

## 50. Aliasing

Lifetimeが重ならないPlaced Resourceを同じHeap Rangeへ配置できます。切替時にAliasing Barrierが必要です。

## 51. Transient Resource

Frame GraphのFirst/Last Useから同じMemoryを別論理Resourceへ再利用できます。

## 52. Reserved Resource

Virtual Address Spaceを予約し、Physical Tile Mappingを別管理します。Tiled Resource/Streamingの発展機能です。

## 53. Tile Mapping

Queue APIでTileをHeapへMap/Unmapします。Texture全体を常駐させないStreaming等に利用します。

## 54. Residency

GPU Memory Budgetを超えるResourceのResident/Evictを管理する発展領域です。OS Memory Managerと連携します。

## 55. Memory Budget

DXGI AdapterのVideo Memory InfoからBudget/Usageを取得できます。Dedicated VRAM値だけで判断しません。

## 56. Budget変化

他ProcessやOSによりBudgetは変わります。定期監視し、Streaming/Qualityを調整します。

## 57. UMA

Integrated GPU等のUnified Memory ArchitectureではCPU/GPU Memory特性がDiscrete GPUと異なります。Architecture Queryを使います。

## 58. Cache Coherent UMA

対応Flagを性能設計へ利用できますが、正しさの同期規則を省略しません。

## 59. Mapとは

CPU Virtual Addressを取得しResource MemoryへAccessするAPIです。Heap Type/Resource用途により許可と効率が異なります。

## 60. Upload Map

```cpp
std::byte* mapped = nullptr;
D3D12_RANGE readRange{ 0, 0 };

ThrowIfFailed(
    uploadResource->Map(
        0,
        &readRange,
        reinterpret_cast<void**>(&mapped)),
    "ID3D12Resource::Map Upload");
```

CPUが読まないためRead Rangeを空にします。

## 61. Persistent Map

Upload Resourceを一度Mapし、Resource Lifetime中Pointerを保持できます。CPU書込み同期と範囲管理はApplication責任です。

## 62. Unmap

Persistent Mapでは最後までUnmapしない設計があります。Unmapする場合、CPUが書いたRangeを指定できます。

## 63. Written Range

`D3D12_RANGE`はCache Tool/RuntimeへのHintです。GPU同期を自動で行うBarrierではありません。

## 64. Readback Map

GPU Copy完了Fenceを待ってからMapし、CPUが読むRangeを指定します。

## 65. DEFAULT Map

通常Default Heap Resourceを直接Mapしません。Upload/Readback経路を使います。

## 66. CPU Memory Copy

```cpp
std::memcpy(
    mapped + destinationOffset,
    sourceData,
    sourceByteSize);
```

Offset/Capacity/Alignment/Source Lifetimeを検証します。

## 67. Upload Buffer Page

```cpp
struct UploadPage
{
    ComPtr<ID3D12Resource> resource;
    std::byte* cpuBase = nullptr;
    D3D12_GPU_VIRTUAL_ADDRESS gpuBase = 0;
    UINT64 capacity = 0;
    UINT64 cursor = 0;
};
```

## 68. Upload Allocation

```cpp
struct UploadAllocation
{
    std::byte* cpu = nullptr;
    D3D12_GPU_VIRTUAL_ADDRESS gpu = 0;
    UINT64 offset = 0;
    UINT64 size = 0;
};
```

## 69. Align Up

```cpp
constexpr UINT64 AlignUp(UINT64 value, UINT64 alignment)
{
    return (value + alignment - 1) & ~(alignment - 1);
}
```

Power-of-two前提をAssertします。

## 70. Upload Linear Allocation

CursorをAlignmentへ切り上げ、Capacity内ならRangeを返します。Frame Fence完了後にCursorを0へ戻します。

## 71. Constant Buffer Allocation

256-byte Alignmentで確保し、GPU AddressをRoot CBVまたはCBV Descriptorへ渡します。

## 72. Vertex Upload Allocation

必要Alignmentで確保し`D3D12_VERTEX_BUFFER_VIEW`のBufferLocation/Size/Strideを設定できます。

## 73. Index Upload Allocation

Index Formatに応じたAlignment/Sizeで確保し`D3D12_INDEX_BUFFER_VIEW`を作ります。

## 74. Dynamic Geometry

毎Frame変わるDebug Line、UI Vertex、Trail等はFrame Upload Bufferから直接GPUが読む方式を使えます。

## 75. Static Geometry

変化しないMeshはUploadからDefault BufferへCopyし、GPU Local Resourceを描画に使います。

## 76. Buffer Copy

```cpp
commandList->CopyBufferRegion(
    defaultBuffer,
    destinationOffset,
    uploadBuffer,
    sourceOffset,
    byteSize);
```

Copy Source/Destination Stateを正しくします。

## 77. Copy後Transition

Default Bufferを`COPY_DEST`から`VERTEX_AND_CONSTANT_BUFFER`、`INDEX_BUFFER`、`GENERIC_READ`等の必要Stateへ遷移します。

## 78. Upload Lifetime

Copy Commandを記録・SubmitしただけではUpload Resourceを破棄できません。Copy完了Fenceまで保持します。

## 79. Upload Ticket

```cpp
struct UploadTicket
{
    UINT64 copyFenceValue;
    ResourceHandle destination;
};
```

Asset Ready判定に使います。

## 80. Copy Queue Upload

Copy QueueでUploadしFenceをSignal、Direct QueueがWaitしてからResourceを使用します。

## 81. Upload Batch

複数Resource Copyを一つのCommand List/Submitへまとめ、Queue/Fence Overheadを減らします。

## 82. Immediate Upload Helperの危険

ResourceごとにSubmitしてCPU Wait IdleするとLoadingが直列化します。Batch/Async Ticketへ発展させます。

## 83. Vertex Buffer作成Flow

1. Default BufferをCOPY_DESTで作る。
2. Upload Rangeを確保しVertex DataをCopyする。
3. `CopyBufferRegion`を記録する。
4. Vertex Buffer StateへTransitionする。
5. SubmitしFence Valueを保存する。
6. Fence後にUpload RangeをRecycleする。

## 84. Vertex Buffer View

```cpp
D3D12_VERTEX_BUFFER_VIEW view{};
view.BufferLocation = vertexBuffer->GetGPUVirtualAddress();
view.SizeInBytes = vertexBytes;
view.StrideInBytes = sizeof(Vertex);
```

## 85. Index Buffer View

```cpp
D3D12_INDEX_BUFFER_VIEW view{};
view.BufferLocation = indexBuffer->GetGPUVirtualAddress();
view.SizeInBytes = indexBytes;
view.Format = DXGI_FORMAT_R32_UINT;
```

16-bitならR16_UINTです。

## 86. GPU Virtual Address

Buffer Resourceから取得します。Resourceを移動/再生成するとAddressが変わるため生Addressの長期保存に注意します。

## 87. GPU Address + Offset

SuballocationしたBuffer RangeはBase Address + OffsetでView/Root Descriptorを作ります。

## 88. Address Alignment

CBV、Acceleration Structure、Copy等、用途ごとのAlignment要件を守ります。

## 89. Constant Buffer Ring

Frame ResourceごとにUpload Pageを持ち、Frame内でCamera/Object/Material Constantを順に確保します。

## 90. Constant Data Layout

C++/HLSL Field順、16-byte Packing、Matrix、bool、Arrayを一致させます。CBV Allocation 256 byteとHLSL Packing 16 byteは別概念です。

## 91. Structured Buffer

Element StrideとCountを管理し、Default BufferへUploadしてSRV/UAVを作ります。

## 92. Raw Buffer

Byte Address Access用Flag/View Formatを設定します。4-byte単位Accessと範囲を守ります。

## 93. Counter Buffer

UAV Counter Resource/Offset/State/LifetimeをData Bufferと共に管理します。

## 94. Texture Uploadとの違い

TextureはRow Pitch、Slice Pitch、Subresource Footprintが必要です。次のTexture章で詳しく扱います。

## 95. GetCopyableFootprints

Texture SubresourceをUpload Bufferへ置くLayout、Row Count、Row Size、Total Sizeを取得します。

## 96. Resource Size Overflow

Count×Stride、Mip/Array Size、Align Upで64-bit Overflowを検証します。`UINT`へ早く縮小しません。

## 97. Allocation Registry

Resource HandleからResource、Allocation Page/Offset/Size、State、Name、Last Fenceを追跡します。

## 98. Resource Lifetime

COM参照が0でもGPU参照中なら危険です。最後に使用した全Queue Fence完了後に解放します。

## 99. Deferred Release

ResourceとPlaced Allocation RangeをFence付きQueueへ入れます。Resource Object解放前後のHeap Range再利用順を守ります。

## 100. Placed Range再利用

旧ResourceのGPU使用完了を待ち、必要Aliasing Barrierを次Resource使用前に記録します。

## 101. Resource Naming

用途、Asset、Size、Heap Page、OffsetをDebug Name/CPU Registryへ記録します。

## 102. Memory Leak診断

Live Object ReportだけでなくAllocatorのActive Allocation、Owner Handle、作成Callsite、Sizeを表示します。

## 103. PIX Memory確認

Resource Description、Heap Type、Size、State、View、Copy Event、Lifetimeを確認します。

## 104. Residency/Memory Tool

PIX等でBudget/Usage/Allocationを確認し、Peak SceneとStreaming Eventを分析します。

## 105. Device Lost

CPU Source/Asset Metadata、Resource Desc、Heap PolicyからGPU Resourceを再作成・再Uploadします。

## 106. Resize

Size依存Render Target/Depth/Post TextureのAllocationを再生成し、旧ResourceをFence後に解放します。

## 107. Hot Reload

新ResourceをUpload完了後にHandle MappingへPublishし、旧Resourceを最終使用Fence後に解放します。

## 108. Multithread Allocation

Upload PageをWorkerごとに配る、Thread-safe Page Poolを使う等でGlobal Lock競合を減らします。

## 109. Allocation Failure Policy

- 追加Pageを作る
- Streaming/Evictionを促す
- Quality/LODを下げる
- 明確なOut-of-memory Errorで停止する

Null Pointerを返して後段でCrashさせません。

## 110. Memory Budget Policy

Soft LimitでStreaming/品質調整、Hard Limit前にAllocationを拒否する等の段階を定めます。

## 111. Unit Test

Align Up、Overflow、Free List結合、Buddy分割、Ring Wrap、Fence Recycle、Size/Offset/GenerationをTestします。

## 112. Integration Test

Vertex/Index/Constant BufferをUploadしTriangleを描画、Fence後にUpload Rangeを再利用して画像を比較します。

## 113. Stress Test

多数の小Buffer、大Texture、Fragmentation、Budget低下、Async Upload、Resize、Device Lost再生成を試します。

## 114. よくある失敗：Default Heapへmemcpy

GPU Local ResourceへCPU Pointerがあると仮定します。Upload HeapからCopyします。

## 115. よくある失敗：Upload即解放

Command記録/Submit直後にUpload Bufferを解放します。Copy Fence完了まで保持します。

## 116. よくある失敗：SizeとAlignment混同

必要Sizeだけ進め次Resource Offsetが未Alignmentになります。Allocation Infoを使います。

## 117. よくある失敗：Uploadへ静的Mesh永続配置

動作してもGPU Access効率が悪い場合があります。Default BufferへCopyします。

## 118. よくある失敗：Map Range誤用

Read/Write RangeをGPU同期APIと誤解します。CPU Cache HintでありFence/Barrierは別です。

## 119. よくある失敗：Heap Range早期再利用

Placed Resource ObjectをResetした直後に同Rangeを再利用します。GPU FenceとAliasing規則を守ります。

## 120. よくある失敗：Resourceだけ世代管理

Descriptor、GPU Address、Material Indexが旧Resourceを指し続けます。Handle Mappingを一括更新します。

## 121. 実装Checklist

- [ ] Buffer/Texture DescのFieldを説明できる。
- [ ] FlagとResource Stateを区別する。
- [ ] DEFAULT/UPLOAD/READBACKを使い分ける。
- [ ] Committed/Placed/Reservedの違いを説明できる。
- [ ] Allocation InfoのSize/Alignmentを使う。
- [ ] Upload ResourceをPersistent Mapできる。
- [ ] Constant Allocationを256-byte Alignmentする。
- [ ] Static BufferをUploadからDefaultへCopyする。
- [ ] Copy後に必要StateへTransitionする。
- [ ] Upload RangeをFenceまで保持する。
- [ ] Placed AllocationにFragmentation/世代/Fenceを持たせる。
- [ ] Memory Budget/Peak/Leakを計測する。

## 122. 理解確認問題

1. ResourceとHeapの違いを説明してください。
2. DEFAULT/UPLOAD/READBACK HeapのCPU/GPU Accessを説明してください。
3. CommittedとPlaced ResourceのTrade-offを説明してください。
4. Allocation Infoが必要な理由を説明してください。
5. Upload BufferをSubmit直後に解放できない理由を説明してください。
6. Constant Bufferで二種類のAlignmentを区別してください。
7. Placed Resource AliasingにBarrierが必要な理由を説明してください。
8. GPU Memory Budget低下時のPolicyを説明してください。

## 123. 章末要点

- Resourceは形/用途、HeapはMemory、UploadはCPU Data転送経路です。
- DEFAULTはGPU、UPLOADはCPU書込み、READBACKはCPU読取りへ使います。
- 最初はCommitted Resourceで正しく実装し、必要時にPlaced/Suballocatorへ進みます。
- Allocation InfoのSize/Alignmentを使い、FragmentationとBudgetを計測します。
- Static DataはUploadからDefaultへCopyし、必要StateへTransitionします。
- Upload/Resource/Heap Rangeを最終使用Fenceまで保持します。
- Stable HandleでDescriptor、GPU Address、Physical Allocationの変更を吸収します。

## 124. 公式資料

- [Resources](https://learn.microsoft.com/en-us/windows/win32/direct3d12/resources)
- [Resource creation](https://learn.microsoft.com/en-us/windows/win32/direct3d12/resource-creation)
- [Memory management](https://learn.microsoft.com/en-us/windows/win32/direct3d12/memory-management)
- [ID3D12Device::CreateCommittedResource](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12device-createcommittedresource)
- [ID3D12Device::CreateHeap](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12device-createheap)
- [ID3D12Device::CreatePlacedResource](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12device-createplacedresource)
- [ID3D12Device::GetResourceAllocationInfo](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12device-getresourceallocationinfo)
- [ID3D12Resource::Map](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12resource-map)
- [Uploading resources](https://learn.microsoft.com/en-us/windows/win32/direct3d12/uploading-resources)
- [Fence-based resource management](https://learn.microsoft.com/en-us/windows/win32/direct3d12/fence-based-resource-management)

次章では、Resource State、Transition/UAV/Aliasing Barrier、Subresource Tracking、Implicit Promotion/Decay、Enhanced Barriersを扱います。
