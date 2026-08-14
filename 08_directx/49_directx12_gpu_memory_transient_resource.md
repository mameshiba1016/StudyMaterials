# DirectX 12 第17章：GPU Memory・Transient Resource

この章では、DirectX 12のGPU Memoryを明示的に管理します。Heap Type、Committed/Placed/Reserved Resource、Allocation、Alignment、Fragmentation、Residency、Budget、Transient Resource、Aliasing、Upload/Readback、OOM対策、計測まで扱います。

## 1. 到達目標

- ResourceとHeap Memoryの関係を説明する。
- 用途別Allocatorを設計する。
- Transient ResourceのLifetimeからMemoryを再利用する。
- Residency/Budget超過を検出し品質を縮退する。
- Fenceを使って安全に解放・再利用する。

## 2. ResourceとMemory

ResourceはTexture/Bufferの形、Format、用途を表し、HeapはBacking Memoryを表します。Committed Resourceでは両者が一体化して見えます。

## 3. Heap Type

```text
DEFAULT  : GPU利用向け
UPLOAD   : CPU write、GPU read向け
READBACK : GPU write結果をCPU read
CUSTOM   : Memory Pool/CPU Page Propertyを明示
```

## 4. DEFAULT Heap

Texture、Vertex/Index、Render Target等の主なGPU Resourceを置きます。通常CPUから直接Mapして書きません。

## 5. UPLOAD Heap

CPUがMapしてDataを書き、GPUがCopy/Constant等で読みます。Resource StateはGENERIC_READとして扱う規則があります。

## 6. READBACK Heap

GPU結果をCopyし、Fence完了後CPUがMapして読みます。直接Rendering先にはしません。

## 7. Committed Resource

```cpp
device->CreateCommittedResource(
    &heapProperties,
    D3D12_HEAP_FLAG_NONE,
    &resourceDesc,
    initialState,
    clearValue,
    IID_PPV_ARGS(&resource));
```

実装が単純で独立Allocationになります。

## 8. Committedの用途

初期実装、大きい専用Resource、特殊Flag、寿命の長いResourceに向きます。大量小ResourceではAllocation Cost/Fragmentationを検討します。

## 9. Placed Resource

Applicationが作ったHeapの指定OffsetへResourceを配置します。複数Resourceを一HeapへSuballocateできます。

## 10. CreateHeap

Heap Size、Alignment、Properties、Flagsを指定します。用途互換性を守り、Debug名へPool/Page情報を含めます。

## 11. CreatePlacedResource

Heap、Offset、Resource Desc、Initial State、Clear Valueを渡します。OffsetとHeapがResourceのAllocation要件を満たす必要があります。

## 12. Reserved Resource

Virtual Address範囲を持ち、Tile単位でPhysical Memory Mappingを変更できます。Tiled Resource/巨大Texture Streaming等の高度な用途です。

## 13. Allocation Info

```cpp
auto info = device->GetResourceAllocationInfo(
    0, 1, &resourceDesc);
```

`SizeInBytes`と`Alignment`をAllocatorの入力にします。

## 14. Alignment

Resource種別、MSAA等で要件が異なります。固定64KBと決めつけずAllocation Infoを使用します。

## 15. Small Resource Alignment

条件を満たす小Textureは小さいAlignmentを利用できる場合があります。要求して結果を確認し、非対応時へFallbackします。

## 16. Heap Flag

Bufferのみ、RT/DS Textureのみ、非RT/DS Textureのみ等へ制限するとTier/Platform上の互換性を管理しやすくなります。

## 17. Resource Heap Tier

Hardware Tierにより同じHeapへ混在できるResource Categoryが異なります。`CheckFeatureSupport`で確認します。

## 18. Allocatorの責務

Size/Alignmentに合う範囲の確保、解放、Fence遅延、統計、Debug情報、OOM Policyを担当します。

## 19. Linear Allocator

Offsetを一方向に進め、全体をまとめてResetします。Frame Uploadや同一LifetimeのTransient Resourceに適します。

## 20. Ring Allocator

循環Buffer上で範囲を確保し、Fence完了した古い範囲から再利用します。WrapとHead/Tail衝突を検査します。

## 21. Free-list Allocator

任意Size範囲を確保/解放し、隣接Free Blockを結合します。長寿命Resourceの混在に使えます。

## 22. Buddy Allocator

2の冪Blockへ分割/結合します。高速で結合しやすい一方、内部Fragmentationが発生します。

## 23. Slab/Pool

同じSize/Classの小AllocationをPageから配ります。Descriptor Metadataや一定Size Buffer等に向きます。

## 24. Dedicated Allocation

非常に大きいResourceは専用Heapへ分離し、小Resource Poolを圧迫しないようにします。

## 25. Internal Fragmentation

確保Blockが要求Sizeより大きく、Block内部に無駄が生じる状態です。Alignment/Paddingも含めて記録します。

## 26. External Fragmentation

Free容量合計は十分でも連続領域がなく大Allocationできない状態です。最大Free Blockも監視します。

## 27. Fragmentation対策

Lifetime/Size/用途別Pool、Buddy、Page追加、長寿命と短寿命の分離、Build時Packing等を使います。

## 28. Defragmentation

Resourceを新AllocationへCopyして参照を切替える方法があります。GPU Address、Descriptor、Fence、State、External参照を更新します。

## 29. 移動できない参照

GPU Virtual Addressを永続保存するCommand/Dataがあると移動が難しくなります。Handle/Descriptor Index等の間接参照を検討します。

## 30. Page設計

Heap Page Sizeを大きくするとAllocation回数は減りますが未使用Memoryが増えます。Resource統計からSize Classを決めます。

## 31. Memory Class

```text
static buffer
static texture
render target/depth
transient texture/buffer
upload
readback
reserved/tile
```

用途とLifetimeでAllocatorを分離します。

## 32. Clear Value

RT/DS ResourceのOptimized Clear Valueは作成時に決めます。AliasするResource間でClear規約も管理します。

## 33. Resource Flag

ALLOW_RENDER_TARGET、ALLOW_DEPTH_STENCIL、ALLOW_UNORDERED_ACCESS、DENY_SHADER_RESOURCE等はHeap/使用方法と整合させます。

## 34. Texture Size

単純なwidth×height×byteだけではMip、Array、Plane、Compression、Alignmentを表せません。Allocation InfoとFootprintを目的別に使います。

## 35. Buffer Size

要素数×StrideのOverflowを検査し、CBV、Structured、Raw、Indirect等のAlignment/Flagを満たします。

## 36. Upload Arena

大きいPersistent Map BufferをFrame/RingでSuballocateし、小さいCommitted Uploadを大量作成しません。

## 37. Persistent Map

UPLOAD Resourceを一度MapしてPointerを保持できます。GPU使用中範囲を上書きしないことが重要です。

## 38. Write-combined Memory

Upload MemoryはCPU readに不向きです。書いた値をCPUで読み戻す実装やread-modify-writeを避けます。

## 39. CBV Allocation

GPU AddressとSizeを256-byte境界へ揃えます。実Data SizeとAllocation Sizeを区別します。

## 40. Texture Upload

`GetCopyableFootprints`で必要Bytes/Row Pitchを得てUpload範囲を確保し、Fence完了まで保持します。

## 41. Readback Pool

Screenshot、Query、Debug Counter等をSize/Lifetime別にPoolし、非同期Fenceで回収します。

## 42. Readback Row Pitch

Texture ReadbackはPaddingを含みます。CPU画像へ行ごとに有効ByteだけをCopyします。

## 43. GPU Virtual Address

Buffer Resourceから得られます。Resourceが解放/移動された後のAddressを使用しません。

## 44. DescriptorとMemory

DescriptorはResource ViewでありBacking Memoryそのものではありません。Resourceを移動/再作成したらView更新と寿命管理が必要です。

## 45. Transient Resource

一Frame内の限られたPass間だけ存在するRender Target/UAV/Bufferです。Lifetimeが重ならないResourceでMemoryを共有できます。

## 46. Lifetime Interval

```text
first use pass -> last use pass
```

Pass GraphのTopology確定後に求めます。Queue非同期実行では単純なPass番号だけで重なりを判断できません。

## 47. Interval Coloring

Lifetimeが重ならないResourceを同じMemory範囲へ割り当てる問題として扱えます。Size、Alignment、Heap Classも制約です。

## 48. Aliasing

同じHeap範囲に異なるPlaced Resourceを時間分割配置します。同時に有効利用しません。

## 49. Aliasing Barrier

前Resource利用後、新Resource利用前に`D3D12_RESOURCE_BARRIER_TYPE_ALIASING`を置きます。

## 50. Barrier対象

Before/After Resourceを指定し、必要に応じnullptrを使う規則があります。可能なら正確なResourceを追跡します。

## 51. Alias後の内容

新Resourceの以前の内容を有効Dataとみなしません。Clear/完全書込みしてから読みます。

## 52. ClearとDiscard

最初の利用が全領域Clear/Writeなら旧内容不要です。RenderPass Begin Access等のDiscard/Clear意味も活用します。

## 53. Multiple Queue Lifetime

別Queue上のPassがOverlapし得る場合、CPU上の記録順ではなくSignal/Waitを含む実行可能期間でAlias可否を判断します。

## 54. Conservative Scheduling

初期版ではQueueを跨ぐTransientをAliasしない等、安全側へ制限し、Timeline検証後に最適化します。

## 55. Transient Heap

RT/DS、Texture、Buffer等のClassごとに大きいHeapを作り、Frame Graph Compile時にOffsetを割り当てます。

## 56. Heap Size不足

追加Page、品質縮退、非Alias Dedicated Allocation、Frame失敗等のPolicyを定めます。範囲外配置はしません。

## 57. Peak Memory

全Resource Size合計ではなく、Timeline上で同時生存するAllocationのPeakを測ります。

## 58. Lifetime短縮

Pass順を変更し、不要になったResourceのLast Useを早めるとPeakを減らせます。ただしQueue Parallelismとの交換です。

## 59. Format Reuse

同じMemoryを異なるResource DescでAliasできますが、Heap Flag、Alignment、Clear、Hardware制約を満たします。

## 60. History Resource

TAA History等のFrameを跨ぐDataはTransientではありません。Persistent ResourceとしてResize/Invalidationを管理します。

## 61. Imported Resource

Swap Chain、External Texture等はFrame GraphへImportし、所有権/初期最終Stateを宣言します。Transient Allocatorは解放しません。

## 62. Memory Budget

DXGI AdapterからLocal/Non-local SegmentのBudget、CurrentUsage等を取得し、物理VRAM容量だけに依存しません。

## 63. QueryVideoMemoryInfo

```cpp
DXGI_QUERY_VIDEO_MEMORY_INFO info{};
adapter3->QueryVideoMemoryInfo(
    0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &info);
```

HRESULTとAdapter Nodeを確認します。

## 64. Budgetは変動する

他Application、OS、表示構成等でBudgetが変わります。起動時一回だけでなく定期的に監視します。

## 65. Reservation

Video Memory Reservation APIはBudget管理のSignalであり、無制限な専有保証ではありません。Platform方針に従います。

## 66. Residency

Resource/HeapのBacking MemoryがGPUからResidentである状態を管理します。Budget超過によるPagingは大きなStutterを起こし得ます。

## 67. MakeResident/Evict

明示Residency管理は高度で、FenceやQueueと組み合わせます。必要性を計測し、Library利用も検討します。

## 68. Residency Set

Frame/Passが必要なHeap集合を把握し、実行前にResidentを保証します。Resource単位でなくHeap単位の影響を理解します。

## 69. OOM

Allocation失敗をCrash直結にせず、Log、Cache解放、品質低下、Retry、User通知等の段階的Policyを用意します。

## 70. OOM時に記録する情報

Budget、Usage、要求Size/Alignment、Heap Class、最大Free Block、Resource Desc、Frame/Scene、最近のAllocationを保存します。

## 71. Eviction Priority

未使用Cache、遠距離Mip、将来用Asset等から削減します。現在Frame必須Resourceを無計画にEvictしません。

## 72. Texture Streaming

Mip Residency、Reserved Resource/Tile等でMemory量を調整します。Descriptor/Viewが参照できるMip範囲も管理します。

## 73. Mesh Streaming

LOD/Chunk単位でUpload/Unloadし、描画中ResourceはFence完了まで保持します。CPU Asset状態とGPU Residencyを分けます。

## 74. Cache Policy

LRUだけでなく再利用予測、Load Cost、重要度、Sizeを考慮します。大量小Assetと巨大Textureを同じ評価にしません。

## 75. Frame Spike

戦闘開始時のEffect/Character/Texture一括生成を避け、Prewarm、Pool、Budgeted Streamingを使います。

## 76. Particle Buffer

最大数をBudget化し、固定Pool/Transient Bufferを使います。Overflow時は低優先Particleを省略します。

## 77. Character Memory

Mesh、Texture、Joint Palette、Skinned Vertex、Motion Vector用前Frame Dataを合算します。表示人数×Pass数で増えるDataを確認します。

## 78. Shadow Memory

Cascade/LightごとのAtlas、Cube Array、Temporary Blurを設計し、品質段階でResolution/枚数を変更できるようにします。

## 79. Post-process Memory

Full/Half/Quarter ResolutionとFormatを選び、Lifetime AliasでPeakを削減します。不要なRGBA32Fを避けます。

## 80. HDR/MSAA

高精度FormatやSample数はRT/Depth Memoryを大きく増やします。Resolve Resourceも含めてBudget計算します。

## 81. Debug Name

Heap、Resourceへ用途、Size、Frame、Allocator Page、Offset、Alias Groupを含む名前を付けます。

## 82. Allocation Log

Allocate/Free/Retire、Size、Alignment、Heap、Offset、Fence、Call Siteを循環Logへ記録します。

## 83. Visualization

Heap Block Map、Lifetime Chart、Peak Timeline、Category別使用量、Budget比率を表示するとFragmentation/Peak原因が分かります。

## 84. PIX

Resource一覧、Heap、Size、Lifetime、Render Target、Capture時使用量を確認します。Application統計と照合します。

## 85. Unit Test

Alignment、Split/Merge、Ring Wrap、Fence回収、Overflow、Interval Alias、Budget PolicyをTestします。

## 86. Randomized Allocator Test

固定SeedでAllocate/Freeを大量生成し、範囲重複、Lost Block、Free合計、Alignment Invariantを検査します。

## 87. Integration Test

Resize、Resolution Scale、MSAA切替、Streaming、Hot Reload、複数Queue、Scene遷移でLeak/Peakを確認します。

## 88. Stress Test

小Budgetを疑似設定し、OOM、Fallback、Cache Eviction、Transient不足、Device Lostを再現します。

## 89. よくある失敗：Placed作成失敗

Offset Alignment、Heap Size、Heap Flag/Tier、Clear Value、Resource Descを確認します。

## 90. よくある失敗：時々画像が壊れる

Alias Lifetime重複、Aliasing Barrier漏れ、初回Clear漏れ、Queue同期、早期再利用を確認します。

## 91. よくある失敗：VRAMは空いているのに失敗

Budget、Fragmentation、Heap Flag、最大連続Block、Process/OS利用、Allocation制約を確認します。

## 92. よくある失敗：戦闘開始時Stutter

一括Upload、PSO作成、Residency Paging、Transient Heap拡張、Texture DecodeをTimelineで分離します。

## 93. よくある失敗：解放後Crash

全QueueのLast-use Fence、Descriptor、GPU Address、Indirect Command、Upload/Readback参照を確認します。

## 94. 実装Checklist

- [ ] Allocation InfoのSize/Alignmentを使う。
- [ ] Heap Type/Flag/TierをResource用途と一致させる。
- [ ] Lifetime別AllocatorとDedicated Allocationを分ける。
- [ ] Fence完了後だけRange/Resourceを再利用する。
- [ ] Transient LifetimeとAliasing BarrierをGraphから生成する。
- [ ] Multiple QueueのOverlapをAlias判定へ含める。
- [ ] Budget/Usage/Fragmentation/Peakを常時記録する。
- [ ] OOM時の段階的縮退を実装する。
- [ ] 小Budget/Randomized Testを行う。

## 95. 理解確認問題

1. Committed/Placed/Reserved Resourceを比較してください。
2. Internal/External Fragmentationを説明してください。
3. Linear/Ring/Buddy Allocatorの用途を説明してください。
4. Transient LifetimeからAlias可否を判断してください。
5. Aliasing Barrierが必要な理由を説明してください。
6. Multiple QueueがLifetime判定を難しくする理由を説明してください。
7. Budgetと物理VRAM容量の違いを説明してください。
8. Resource移動時に更新すべき参照を挙げてください。
9. 戦闘開始時Memory Spikeの対策を提案してください。
10. OOM診断で記録すべき情報を挙げてください。

## 96. 要点

- Resource DescとBacking Heap Memoryを分けて理解します。
- Placed Resourceと用途別Allocatorで大量Allocationを管理します。
- Alignment、Fragmentation、Fence Lifetimeを常に追跡します。
- Transient Resourceは非重複LifetimeでMemoryをAliasできます。
- Alias境界にはBarrierと新内容の初期化が必要です。
- Budget/Residency超過はStutterやAllocation失敗を起こします。
- Peak Timelineと縮退Policyを高速戦闘SceneのMemory設計へ含めます。

## 97. 公式資料

- [Memory Management Strategies](https://learn.microsoft.com/en-us/windows/win32/direct3d12/memory-management-strategies)
- [ID3D12Device::GetResourceAllocationInfo](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12device-getresourceallocationinfo)
- [ID3D12Device::CreatePlacedResource](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12device-createplacedresource)
- [ID3D12Device::CreateReservedResource](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12device-createreservedresource)
- [DXGI_QUERY_VIDEO_MEMORY_INFO](https://learn.microsoft.com/en-us/windows/win32/api/dxgi1_4/ns-dxgi1_4-dxgi_query_video_memory_info)
- [Residency](https://learn.microsoft.com/en-us/windows/win32/direct3d12/residency)

## 98. 次章への接続

次章ではDevice Removed・DRED・PIXを扱います。本章のAllocation、Residency、Fence、Lifetime異常をDevice Lost時の診断Dataへ結び付けます。
