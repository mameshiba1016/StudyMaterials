# DirectX 12 第6章：Descriptor Heap・Handle・Allocator

この章では、ResourceをShaderやPipelineへ結び付けるDescriptor管理を学びます。Heap Type、CPU/GPU Handle、View生成、Descriptor Copy、Persistent/Transient Allocator、Fence付き再利用、Heap Binding、Debug/Testまでを扱います。

## 1. Descriptorとは

ResourceをCBV、SRV、UAV、RTV、DSV等の用途で参照するための小さな記述Dataです。

## 2. Resourceとの違い

```text
ID3D12Resource : Buffer/Texture本体とMemory
Descriptor     : そのResourceをどう見るかという記述
```

Descriptorを解放してもResource本体が自動解放されるわけではありません。

## 3. 一Resourceに複数Descriptor

同じTextureへMip別SRV、Array Slice別RTV、UAV等の複数Viewを作れます。

## 4. Descriptor Heap

同じTypeのDescriptor Slotを連続配置するObjectです。作成後にSlotへDescriptorを書き込みます。

## 5. 四つのHeap Type

```text
CBV_SRV_UAV : Constant/Shader Resource/Unordered Access View
SAMPLER     : Sampler
RTV         : Render Target View
DSV         : Depth Stencil View
```

## 6. Typeを混在できない

RTV HeapへSRVを置く等はできません。AllocatorもHeap Typeごとに分けます。

## 7. Shader-visibleにできるType

`CBV_SRV_UAV`と`SAMPLER` HeapだけがShader-visibleになれます。RTV/DSVはCPU HandleでCommandへ渡します。

## 8. CPU-only Heap

`D3D12_DESCRIPTOR_HEAP_FLAG_NONE`で作り、CPUがDescriptorを保管・Copy・RTV/DSV Bindingする用途です。

## 9. Shader-visible Heap

`D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE`で作り、Descriptor Tableを通してGPU Shaderが参照します。

## 10. Heap Description

```cpp
D3D12_DESCRIPTOR_HEAP_DESC desc{};
desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
desc.NumDescriptors = 4096;
desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
desc.NodeMask = 0;
```

## 11. CreateDescriptorHeap

```cpp
ComPtr<ID3D12DescriptorHeap> heap;
ThrowIfFailed(
    device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&heap)),
    "ID3D12Device::CreateDescriptorHeap");

heap->SetName(L"Main Shader Visible Resource Heap");
```

## 12. Heap Capacity

作成後にSlot数を拡張できません。不足時は別Heapを作るか、大きなHeapへ移行する設計が必要です。

## 13. Descriptor Increment Size

```cpp
const UINT increment = device->GetDescriptorHandleIncrementSize(
    D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
```

Device/Heap Typeごとに取得し、固定値を仮定しません。

## 14. CPU Descriptor Handle

```cpp
D3D12_CPU_DESCRIPTOR_HANDLE cpuStart =
    heap->GetCPUDescriptorHandleForHeapStart();
```

`ptr`はOpaqueなHandle値で、通常Memory PointerとしてDereferenceしません。

## 15. GPU Descriptor Handle

```cpp
D3D12_GPU_DESCRIPTOR_HANDLE gpuStart =
    heap->GetGPUDescriptorHandleForHeapStart();
```

Shader-visible Heapで使用します。

## 16. Slot Offset

```cpp
D3D12_CPU_DESCRIPTOR_HANDLE CpuHandleAt(
    D3D12_CPU_DESCRIPTOR_HANDLE start,
    UINT index,
    UINT increment)
{
    start.ptr += static_cast<SIZE_T>(index) * increment;
    return start;
}
```

## 17. GPU Slot Offset

```cpp
D3D12_GPU_DESCRIPTOR_HANDLE GpuHandleAt(
    D3D12_GPU_DESCRIPTOR_HANDLE start,
    UINT index,
    UINT increment)
{
    start.ptr += static_cast<UINT64>(index) * increment;
    return start;
}
```

## 18. Handle Pair

```cpp
struct DescriptorHandle
{
    D3D12_CPU_DESCRIPTOR_HANDLE cpu{};
    D3D12_GPU_DESCRIPTOR_HANDLE gpu{};
    uint32_t index = UINT32_MAX;
};
```

CPU-only HeapではGPU Handleを無効として扱います。

## 19. Handleは所有権ではない

HandleをCopyしてもDescriptor SlotやResource Lifetimeは延びません。Allocator/RegistryがOwnerです。

## 20. CBV

Constant BufferのGPU Virtual AddressとSizeを記述します。

```cpp
D3D12_CONSTANT_BUFFER_VIEW_DESC cbv{};
cbv.BufferLocation = constantResource->GetGPUVirtualAddress() + offset;
cbv.SizeInBytes = Align256(sizeof(CameraConstants));

device->CreateConstantBufferView(&cbv, destinationCpuHandle);
```

## 21. CBV Alignment

Address/Sizeの256-byte Alignment要件を守ります。構造体Sizeと割当Pitchを区別します。

## 22. SRV

ShaderからResourceを読み取るViewです。Texture Dimension、Format、Mip、Array範囲、Component Mapping等を指定します。

## 23. Texture2D SRV

```cpp
D3D12_SHADER_RESOURCE_VIEW_DESC srv{};
srv.Format = textureFormat;
srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
srv.Texture2D.MostDetailedMip = 0;
srv.Texture2D.MipLevels = mipLevels;
srv.Texture2D.ResourceMinLODClamp = 0.0f;

device->CreateShaderResourceView(texture, &srv, destinationCpuHandle);
```

## 24. Component Mapping

`D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING`を設定し忘れないようにします。Swizzleが必要なら明示します。

## 25. Buffer SRV

First Element、Num Elements、Structure Byte Stride、Format、Raw Flagを用途に合わせます。

## 26. Structured Buffer SRV

Formatは通常`DXGI_FORMAT_UNKNOWN`で、`StructureByteStride`へ一要素Sizeを指定します。

## 27. Raw Buffer SRV

対応Typeless FormatとRaw Flagを使用し、Shader側ByteAddressBufferと一致させます。

## 28. UAV

Shaderから読み書きするViewです。Resource State、UAV Barrier、Counter Resourceも別途管理します。

## 29. CreateUnorderedAccessView

```cpp
device->CreateUnorderedAccessView(
    resource,
    counterResource,
    &uavDesc,
    destinationCpuHandle);
```

Counter不要なら`nullptr`です。

## 30. RTV

Render Target OutputへBindするViewです。RTV HeapのCPU Handleを`OMSetRenderTargets`等へ渡します。

## 31. DSV

Depth/Stencil Buffer用Viewです。Read-only Depth/Stencil Flagを使うViewも作れます。

## 32. Sampler Descriptor

Filter、Address Mode、LOD、Anisotropy、Comparison等を記述しSampler Heapへ置きます。

## 33. Static Sampler

Root Signatureへ不変Samplerを埋め込む方式です。Sampler Heap Slotを消費しませんが動的変更できません。

## 34. Null Descriptor

ResourceがないSlotへ型に合うNull Descriptorを置き、Shaderの任意Bindingを安全に表現できます。

## 35. Null SRV/UAV

Resource Pointerを`nullptr`にし、有効なView Descを渡してNull Descriptorを作る方法があります。Dimension/FormatをShader期待と合わせます。

## 36. Uninitialized Slotを使わない

Heapを作っただけでは全Slotが有効Descriptorになるとは限りません。Nullまたは実Descriptorで初期化します。

## 37. Descriptor作成APIはvoid

CBV/SRV/UAV/RTV/DSV作成Methodの多くは`void`です。Debug Layerと入力Validationを使います。

## 38. CPU Master Heap

永続AssetのDescriptorをCPU-only Heapへ保持し、必要なものをShader-visible範囲へCopyする設計です。

## 39. なぜMaster Heapを使うのか

GPU参照中Slotを直接書き換えず、Resource固有Descriptorを安定管理し、FrameごとのTableを構築しやすくなります。

## 40. CopyDescriptorsSimple

```cpp
device->CopyDescriptorsSimple(
    count,
    destinationCpuHandle,
    sourceCpuHandle,
    D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
```

同一Heap Type間で連続RangeをCopyします。

## 41. CopyDescriptors

複数Source/Destination RangeとSize配列を指定できます。Material Tableを複数箇所から組み立てる場合に使えます。

## 42. Copy先のLifetime

GPUがCopy先Slotを参照中なら上書きできません。Frame Fence完了まで保持します。

## 43. Copy元のLifetime

Copy完了後、Descriptor内容はDestination側に複製されます。ただしDescriptorが参照するResource本体のLifetimeはGPU完了まで必要です。

## 44. DescriptorとResource参照Count

DescriptorはResourceへCOM参照Countを持つOwnerとは考えません。Resource Registryが明示的に保持します。

## 45. SetDescriptorHeaps

```cpp
ID3D12DescriptorHeap* heaps[] =
{
    resourceHeap.Get(),
    samplerHeap.Get()
};

commandList->SetDescriptorHeaps(
    static_cast<UINT>(std::size(heaps)),
    heaps);
```

Shader-visible Heapだけを設定します。

## 46. 同時Binding数

CBV/SRV/UAV Heap一つとSampler Heap一つを同時に設定します。同TypeのShader-visible Heapを複数同時Bindしません。

## 47. Heap切替

Shader-visible Heapの切替はGPU/Driver CostやRoot Table再設定を伴い得ます。一Frameで大きなHeapを継続利用する設計が一般的です。

## 48. Root Descriptor Table

```cpp
commandList->SetGraphicsRootDescriptorTable(
    rootParameterIndex,
    tableGpuHandle);
```

Root SignatureのDescriptor Range LayoutとHeap内配置を一致させます。

## 49. Compute Table

Compute Pipelineでは`SetComputeRootDescriptorTable`を使います。Graphics/Compute Root Binding Stateを区別します。

## 50. Heap変更後の再Bind

`SetDescriptorHeaps`でHeapを変えた後は、そのHeapを参照するRoot Descriptor Tableを再設定します。

## 51. Root Tableの範囲

GPU HandleはTable先頭を指し、Root Signatureで定義したRange/Offsetに従って各Descriptorが解釈されます。

## 52. Table Layout

```text
slot +0 : CBV Frame
slot +1 : CBV Object
slot +2 : SRV Albedo
slot +3 : SRV Normal
slot +4 : SRV Material
```

Shader Registerと対応を文書化します。

## 53. Register Space

`register(t0, space1)`等のSpaceをRoot Signature Rangeと一致させます。Feature/Material領域の分離に使えます。

## 54. Append Offset

Descriptor Range Offsetへ`D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND`を使う場合、直前Rangeからの連続配置になります。

## 55. Volatility Flags

Root Signature 1.1ではDescriptor/Dataの静的・可変性Hintを指定できます。実際の更新頻度と矛盾させません。

## 56. Descriptor Allocatorの目的

Heap Slotの確保、解放、Handle計算、世代検証、Fence付きRecycle、統計を一つにまとめます。

## 57. Allocation Handle

```cpp
struct DescriptorAllocation
{
    uint32_t start = UINT32_MAX;
    uint32_t count = 0;
    uint32_t generation = 0;
    D3D12_CPU_DESCRIPTOR_HANDLE cpu{};
    D3D12_GPU_DESCRIPTOR_HANDLE gpu{};
};
```

## 58. Move-only Allocation

同じRangeを二重Freeしないよう、Allocation ObjectをMove-onlyにする設計があります。

## 59. Linear Allocator

Frame内Transient DescriptorをCursor順に割り当て、Frame Fence完了後にまとめてResetします。

```cpp
uint32_t start = cursor;
cursor += count;
```

## 60. Linear Allocatorの利点

高速、Fragmentationなし、Lock分離しやすい一方、個別Freeはできません。

## 61. FrameごとのTransient範囲

```text
Heap [ Persistent | Frame0 | Frame1 | Frame2 ]
```

GPU使用中Frame範囲を別Frameが上書きしません。

## 62. Fixed Partition

Frameごとに同じSizeを予約する方式です。単純ですが一部Frameの余りを他Frameが使えません。

## 63. Ring Allocator

Fence Value付きRingで可変Size Rangeを割り当てられます。Wrap、Head/Tail、連続Range、Alignmentを正しく管理します。

## 64. Free List Allocator

Persistent Descriptorの個別確保/解放に向きます。Fragmentation、Range結合、世代管理が必要です。

## 65. Bitmap Allocator

一Slot単位の確保ならBitmapで空きを探せます。連続Table確保にはRun探索が必要です。

## 66. Buddy Allocator

Power-of-two Range管理に使えますが、Descriptor Tableの任意Sizeでは内部Fragmentationを評価します。

## 67. Persistent Descriptor

Texture/Buffer Asset等、長期間安定Indexを持たせるDescriptorです。Free後の再利用には世代を使います。

## 68. Transient Descriptor

FrameごとのMaterial TableやPass Table等、短期間だけ必要なDescriptorです。Frame Linear Allocationが適します。

## 69. Bindless Index

Shader-visible大規模HeapのPersistent IndexをMaterial/Instance Dataへ格納する設計です。Binding Tier、Shader Model、Root Signature Flagを確認します。

## 70. Stable Index

BindlessでIndexを長期保持する場合、Heap再構築・Compaction・Device Lost時のMappingを管理します。

## 71. Descriptor Heap成長

Shader-visible Heapを単純にResizeできません。新Heapを作りDescriptorをCopyし、GPU参照切替と旧HeapのDeferred Releaseを行います。

## 72. Heap Migration

```text
1. 大きな新Heap生成
2. 使用DescriptorをCopy
3. Index/Mappingを維持または更新
4. 新規Commandから新HeapをBind
5. 旧Heap最終使用Fence後に解放
```

## 73. Index変更の危険

GPU BufferやMaterialが旧Indexを保持している場合、Compactionで全参照更新が必要です。安定Index方式を検討します。

## 74. Descriptor Heap Tier

Hardware TierによりHeap/Binding制約があります。大量Descriptor設計前にCapabilitiesを確認します。

## 75. Sampler Heap上限

Shader-visible Sampler Heapには実用上重要なSize制約があります。Samplerを共有・正規化し、無制限にMaterialごと生成しません。

## 76. Sampler Cache

Filter/Address/Comparison等をKey化し、同じSampler Descriptorを再利用します。

## 77. RTV Allocator

Swap Chain、Scene Color、Shadow、G-Buffer等へCPU-only RTV Slotを割り当てます。Shader Fenceとは別にResource Lifetimeと対応させます。

## 78. DSV Allocator

Depth Textureごとに通常DSV、Read-only DSV等の複数Viewを持つ場合があります。

## 79. CPU Master Descriptor Page

CPU-only HeapをPage単位で追加し、各PageのFree Listを管理できます。CPU HandleはPageを跨いで連続とは限りません。

## 80. Shader-visible連続性

一つのDescriptor TableはHeap内で連続したRangeが必要です。複数CPU Pageから一つのGPU RangeへCopyします。

## 81. Descriptor Table Cache

MaterialのMaster Descriptor ID列とVersionをKeyに、Frame Table CopyをCacheする設計があります。GPU使用中Rangeを上書きしません。

## 82. Dirty Tracking

Texture/CBV変更時だけTableを再構築できます。ただしFrame-local GPU Heap RangeのLifetimeと整合させます。

## 83. Default Descriptor Set

White、Black、Flat Normal、Default Material、Null UAV等を起動時に作り、欠損Assetを安全に置換します。

## 84. Descriptor Registry

```cpp
struct DescriptorRecord
{
    DescriptorAllocation master;
    ResourceHandle resource;
    ViewDescription view;
    uint32_t version;
};
```

Device Lost時にViewを再生成できる情報を保持します。

## 85. Device Lost

全Descriptor Heap/Handleは旧Device依存です。論理Descriptor記録からHeapとViewを再生成します。

## 86. Resize

Back Buffer/Size依存ResourceのRTV/SRV/UAVを既存または新Slotへ再作成します。GPU使用完了をFenceで保証します。

## 87. Hot Reload

Texture/Buffer差替え時、GPU使用中Descriptorを直接上書きせず、新Descriptorへ切替後に旧SlotをRetireします。

## 88. Deferred Free

```cpp
struct RetiredDescriptorRange
{
    uint64_t fenceValue;
    uint32_t start;
    uint32_t count;
};
```

Fence完了後にAllocatorへ戻します。

## 89. Multiple Queue

DescriptorをDirect/Compute Queueが参照する場合、すべての最終使用完了後に再利用します。

## 90. Thread Safety

Persistent Free ListはLock等で保護し、Frame Transient領域はWorkerごとにChunkを配ってLock競合を減らせます。

## 91. Worker Chunk

Frame開始時に各WorkerへDescriptor範囲を割当て、局所Cursorで確保します。不足時のFallbackを決めます。

## 92. Capacity Planning

Frame/Pass/Material/DrawごとのDescriptor使用数を測り、Peakに余裕を加えてCapacityを決めます。

## 93. Overflow Policy

- Debug Assertと詳細Log
- 追加Heap/Batchへ分割
- 次Pageから確保
- 低Priority描画を省略

Buffer越境は許しません。

## 94. Statistics

- Type別Capacity/使用数/Peak
- Persistent/Transient使用数
- Copy Descriptor数
- Heap切替回数
- Allocation失敗数
- Deferred Free数
- Fragmentation

## 95. Debug Name

HeapへType、Visibility、Page、Capacityを含む名前を付けます。Allocationには論理LabelをCPU Debug Tableへ保存します。

## 96. Debug Layer

不正Heap Type、GPU Handle、Root Table範囲、Heap未Binding等を検出できます。GPU-based Validationも併用します。

## 97. PIX

Draw時のBound Heap、Root Table、Descriptor内容、参照Resource、Format/Mipを確認します。

## 98. Unit Test

Handle Offset、Range Allocation/Free、世代、不正二重Free、Ring Wrap、Fence収集、Fragment結合、OverflowをTestします。

## 99. Integration Test

複数Texture/SamplerをDescriptor TableへCopyし、ShaderでIndex通りの色が出るか確認します。

## 100. Stress Test

Capacity直前/一致/超過、大量Material、複数Worker、Frame Ring Wrap、Hot Reload、Resizeを組み合わせます。

## 101. よくある失敗：Increment固定

Descriptor Sizeを固定値で計算します。DeviceからHeap Type別Incrementを取得します。

## 102. よくある失敗：CPU HandleをGPUへ渡す

CPU/GPU Handleを同じ数値と思い込みます。Shader-visible HeapのGPU HandleをRoot Tableへ渡します。

## 103. よくある失敗：Descriptorだけ保持

Resource本体を解放してDescriptorがDanglingになります。RegistryがResource Lifetimeを保持します。

## 104. よくある失敗：GPU使用中上書き

次FrameのMaterial Tableを同じSlotへCopyします。Frame範囲とFenceを使います。

## 105. よくある失敗：Heap切替後Table未設定

Root Tableが旧HeapのHandleを指します。Heap変更後にDescriptor Tableを再Bindします。

## 106. よくある失敗：非連続Table

一つのTableのDescriptorを別Heap/Pageへ散らします。Shader-visible Heapへ連続Copyします。

## 107. よくある失敗：Null未初期化

任意TextureなしのSlotを未初期化のままShaderが読みます。型に合うNull/Default Descriptorを置きます。

## 108. よくある失敗：無制限Sampler

Materialごとに重複Samplerを生成しHeapを使い切ります。Sampler Cache/Static Samplerを使います。

## 109. 実装Checklist

- [ ] 四Heap TypeとShader Visibilityを説明できる。
- [ ] Increment SizeをTypeごとに取得する。
- [ ] CPU/GPU Handleを区別する。
- [ ] CBVの256-byte Alignmentを守る。
- [ ] SRV/UAV Dimension/Format/範囲を正しく指定する。
- [ ] Null/Default Descriptorを用意する。
- [ ] Master DescriptorとGPU Tableを分ける。
- [ ] Tableを連続RangeへCopyする。
- [ ] Frame Transient SlotをFenceまで保持する。
- [ ] Persistent Slotへ世代とDeferred Freeを使う。
- [ ] Heap切替後にRoot Tableを再設定する。
- [ ] Capacity/Peak/Copy/切替回数を計測する。

## 110. 理解確認問題

1. ResourceとDescriptorの違いを説明してください。
2. Shader-visibleにできるHeap Typeを答えてください。
3. CPU HandleとGPU Handleの用途を説明してください。
4. CPU Master HeapからGPU HeapへCopyする利点を説明してください。
5. Descriptor SlotをFence完了前に再利用できない理由を説明してください。
6. PersistentとTransient Allocatorの違いを説明してください。
7. Heap切替後にRoot Table再設定が必要な理由を説明してください。
8. Descriptor Heap成長で旧Heapを即解放できない理由を説明してください。

## 111. 章末要点

- DescriptorはResourceの見方であり、Resource本体のOwnerではありません。
- Heap Type、Shader Visibility、CPU/GPU Handleを厳密に区別します。
- Increment SizeをDeviceから取得してSlot Handleを計算します。
- CPU Master DescriptorからShader-visible連続RangeへCopyできます。
- PersistentはFree List/世代、TransientはFrame Linear/Ringが適します。
- GPU参照中のDescriptorを上書きせず、Fence後に再利用します。
- Heap切替を減らし、切替後はRoot Descriptor Tableを再設定します。
- Capacity、Peak、Copy、Fragmentationを計測してAllocatorを検証します。

## 112. 公式資料

- [Descriptor heaps overview](https://learn.microsoft.com/en-us/windows/win32/direct3d12/descriptor-heaps-overview)
- [Shader-visible descriptor heaps](https://learn.microsoft.com/en-us/windows/win32/direct3d12/shader-visible-descriptor-heaps)
- [Creating descriptor heaps](https://learn.microsoft.com/en-us/windows/win32/direct3d12/creating-descriptor-heaps)
- [ID3D12Device::GetDescriptorHandleIncrementSize](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12device-getdescriptorhandleincrementsize)
- [ID3D12GraphicsCommandList::SetDescriptorHeaps](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12graphicscommandlist-setdescriptorheaps)
- [ID3D12Device::CopyDescriptors](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12device-copydescriptors)
- [ID3D12Device::CopyDescriptorsSimple](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12device-copydescriptorssimple)
- [ID3D12Device::CreateConstantBufferView](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12device-createconstantbufferview)
- [ID3D12Device::CreateShaderResourceView](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12device-createshaderresourceview)
- [ID3D12Device::CreateUnorderedAccessView](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12device-createunorderedaccessview)

次章では、Root Signature、Descriptor Range/Table、Root CBV/SRV/UAV、Root Constants、Static SamplerとShader RegisterのBinding契約を扱います。
