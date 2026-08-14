# DirectX 12 第14章：Compute・UAV・Indirect

この章では、Compute Shaderで汎用GPU処理を実装し、その結果を描画へ接続します。Thread/Group、SRV/UAV、同期、Atomic、Scan/Compaction、GPU Culling、Command Signature、ExecuteIndirect、Particle、Skinning、性能計測まで扱います。

## 1. 到達目標

- Compute Pipelineを作りDispatchできる。
- Thread IDとGroup Shared Memoryを正しく使える。
- SRV/UAVとResource State/Barrierを管理できる。
- GPU上で可視ObjectとIndirect Commandを生成できる。
- Race、Out-of-bounds、同期不足を診断できる。

## 2. Compute Shaderとは

TriangleをRasterizeせず、指定したThread GridでDataを読み書きするShader Stageです。画像処理、Particle、Skinning、Culling、Lighting等へ利用できます。

## 3. Graphicsとの違い

Input AssemblerやRender Targetを必要としません。処理対象、Thread配置、出力先、同期をApplicationとShaderが明示します。

## 4. 処理全体

```text
create root signature
create compute PSO
prepare SRV/UAV/CBV
transition resources
SetPipelineState / SetComputeRootSignature
bind descriptors
Dispatch
barrier / transition
consume result
```

## 5. 最小Compute Shader

```hlsl
RWStructuredBuffer<float> Output : register(u0);

[numthreads(64, 1, 1)]
void CSMain(uint3 dispatchId : SV_DispatchThreadID)
{
    Output[dispatchId.x] = dispatchId.x * 2.0f;
}
```

実際には要素数を渡し、範囲外Threadを除外します。

## 6. numthreads

`[numthreads(x,y,z)]`は一Thread Group内のThread数です。Shader ModelとHardwareの上限を守ります。

## 7. Dispatch

```cpp
const UINT groupCount = (elementCount + 63u) / 64u;
commandList->Dispatch(groupCount, 1, 1);
```

Dispatch引数はThread数ではなくGroup数です。

## 8. 端数処理

```hlsl
if (dispatchId.x >= ElementCount)
    return;
```

切上げDispatchでは必須です。GPU Buffer外Accessを絶対に許しません。

## 9. SV_DispatchThreadID

Dispatch全体で一意の3次元Thread座標です。1D配列、2D画像、3D VolumeのIndexへ変換します。

## 10. SV_GroupID

Dispatch内のThread Group座標です。Tile単位の処理やGroup出力位置に使います。

## 11. SV_GroupThreadID

Group内のLocal座標です。Group Shared ArrayのIndexや協調Loadに使います。

## 12. SV_GroupIndex

Group内3D座標を一次元化したIndexです。ReductionやScanのLocal Indexに便利です。

## 13. Thread Group Size選択

32/64/128/256等を候補にし、Register、Shared Memory、Wave Size、分岐、対象Sizeを含めProfilerで比較します。

## 14. Occupancy

Groupが大き過ぎる、RegisterやShared Memoryを使い過ぎると同時常駐Group数が減ります。Thread数だけで性能を判断しません。

## 15. Wave

GPUがLockstepに近い形で実行するThread集合です。Wave Sizeを固定値と決めつけず、必要ならWave Intrinsicの要件を確認します。

## 16. Divergence

同じWave内でBranchが分かれると両経路を実行する場合があります。Data分類やBranchless化は実測して適用します。

## 17. Coalesced Access

隣接Threadが隣接AddressへAccessするLayoutはMemory Transactionを効率化しやすくなります。AoS/SoAを処理単位で選びます。

## 18. Structured Buffer

要素Strideを持つ構造体配列です。SRVで読取り、RWStructuredBuffer/UAVで書込みできます。

## 19. Raw Buffer

ByteAddressBuffer/RWByteAddressBufferとしてByte Offsetで扱います。柔軟ですがAlignment、型変換、範囲を自分で管理します。

## 20. Typed Buffer

Format付きViewで要素を解釈します。Resource/View Formatの互換性とHardware Supportを確認します。

## 21. RWTexture

`RWTexture2D`等でTextureへRandom Writeできます。UAV対応FormatとResource Flagが必要です。

## 22. UAV対応Resource

Resource作成時に`D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS`を指定します。Swap Chain Buffer等、用途に制約があるResourceもあります。

## 23. UAV Descriptor

```cpp
D3D12_UNORDERED_ACCESS_VIEW_DESC desc{};
desc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
desc.Format = DXGI_FORMAT_UNKNOWN;
desc.Buffer.NumElements = count;
desc.Buffer.StructureByteStride = sizeof(Element);
device->CreateUnorderedAccessView(resource, nullptr, &desc, handle);
```

Structured BufferではFormat UNKNOWNと正しいStrideを使います。

## 24. SRV Descriptor

同じBufferを別Passで読む場合、用途に対応するSRVも作ります。View範囲とStrideをUAV側と一致させます。

## 25. Counter付きUAV

Append/Consume Structured Buffer等はCounter Resource/Offsetを使います。Counterの初期化、State、Overflowを管理します。

## 26. AppendStructuredBuffer

条件を満たす要素を可変長出力へ追加できます。順序は通常保証されないため、安定順序が必要なら別Algorithmを使います。

## 27. Counter Reset

Frame/Pass開始前にCounterを0へ戻します。Clear、Copy等の方法と必要State/Barrierを設計します。

## 28. ClearUnorderedAccessView

GPU/CPU Descriptor Handleの両方、Shader-visible Heap、Resource Stateが必要です。Typed Clear値とFormatを一致させます。

## 29. Root Signature

Compute用CBV/SRV/UAV Table、Root Constant等を定義します。Graphicsと共有するか専用にするかはBinding頻度と互換性で決めます。

## 30. Compute PSO

```cpp
D3D12_COMPUTE_PIPELINE_STATE_DESC desc{};
desc.pRootSignature = rootSignature.Get();
desc.CS = { shader->GetBufferPointer(), shader->GetBufferSize() };
device->CreateComputePipelineState(&desc, IID_PPV_ARGS(&pso));
```

Graphics PSOと異なりRasterizer/Blend/RTV等はありません。

## 31. Command List設定

```cpp
commandList->SetPipelineState(pso.Get());
commandList->SetComputeRootSignature(rootSignature.Get());
commandList->SetDescriptorHeaps(1, heaps);
commandList->SetComputeRootDescriptorTable(0, tableGpuHandle);
commandList->Dispatch(groupX, groupY, groupZ);
```

Graphics用Root APIとCompute用Root APIを混同しません。

## 32. Compute Queue

Compute CommandはDirect Queueでも実行できます。専用Compute QueueでOverlapさせる設計は第16章で扱います。

## 33. Resource State

UAV Accessでは`D3D12_RESOURCE_STATE_UNORDERED_ACCESS`、後でShader Resourceとして読むなら対応するRead StateへTransitionします。

## 34. UAV Barrier

同じUAV Resourceへの書込みと後続Accessの順序/可視性が必要な場合に使います。Stateが変わらなくても必要になり得ます。

## 35. Transition Barrierとの違い

Transitionは用途Stateを変えます。UAV BarrierはUAV Access間の順序を保証します。目的を区別します。

## 36. Barrier省略判断

別Resourceで依存がない場合等、省略できる条件があります。推測で消さず、依存Graphと仕様、Profilerを根拠にします。

## 37. Group Shared Memory

```hlsl
groupshared float Shared[256];
```

同じGroup内Threadが共有する高速なOn-chip Memoryです。Group間では共有できません。

## 38. GroupMemoryBarrierWithGroupSync

Group内全Threadの共有Memory Accessを同期します。全Threadが到達できない分岐内に置くとDeadlock/未定義動作の原因になります。

## 39. DeviceMemoryBarrier

Memory Scopeと同期対象を理解して使います。Group同期だけで別Groupの実行順を保証できません。

## 40. Group間同期

一つのDispatch内でGroupの実行順は保証されません。全Group結果が必要ならDispatchを分け、UAV Barrier等でPass境界を作ります。

## 41. Race Condition

複数Threadが同じAddressへ非Atomic Read/Modify/Writeすると結果が不定になります。Ownership分割、Atomic、別Pass化で解決します。

## 42. Interlocked Operation

Add、Min、Max、CompareExchange等でAtomic更新できます。正しさは得られても競合が多いと性能が落ちます。

## 43. Atomic Contention

全Threadが一Counterへ集中する代わりにGroup内集約後に一度だけAtomicする等、階層化を検討します。

## 44. Reduction

多数の値からSum/Min/Max等を作ります。Group Sharedで段階的に要素数を半減し、複数Passで全体結果へまとめます。

## 45. Prefix Sum

各要素より前の合計を求めるScanです。Compaction、Particle生成、Indirect Offset計算の基礎になります。

## 46. ExclusiveとInclusive Scan

Exclusiveは自分を含まず、Inclusiveは自分を含む累積値です。出力Index用途ではExclusiveがよく使われます。

## 47. Stream Compaction

```text
predicate -> scan -> scatter
```

条件を満たす要素だけを詰めた配列へ出力します。安定順序の有無も仕様化します。

## 48. Radix Sort

KeyのBit範囲ごとに分類/Scan/Scatterを繰り返すGPU Sortです。透明SortやSpatial Keyで使えますがCostと一時Memoryが必要です。

## 49. DispatchIndirect

GPU Buffer内のGroup Countを使ってCompute Dispatchできます。前Pass結果に応じた可変Work量へ利用します。

## 50. Indirect Argument State

Indirect Argument Bufferは実行時に`D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT`が必要です。生成時UAVからTransitionします。

## 51. ExecuteIndirect

GPU Bufferに記録されたCommand ArgumentをCommand Signatureに従って実行します。GPU Driven Renderingの中心APIです。

## 52. Command Signature

```cpp
D3D12_INDIRECT_ARGUMENT_DESC argument{};
argument.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED;

D3D12_COMMAND_SIGNATURE_DESC signature{};
signature.ByteStride = sizeof(D3D12_DRAW_INDEXED_ARGUMENTS);
signature.NumArgumentDescs = 1;
signature.pArgumentDescs = &argument;
device->CreateCommandSignature(&signature, nullptr,
                               IID_PPV_ARGS(&commandSignature));
```

Argument LayoutとByteStrideをGPU生成構造体へ厳密に一致させます。

## 53. Indirect Argument種類

Draw、DrawIndexed、DispatchのほかRoot Constant、CBV/SRV/UAV、Vertex/Index Buffer View等をSignatureへ含められます。

## 54. Root Signatureとの関係

SignatureがRoot Parameterを変更する場合、対応Root Signatureが必要です。Parameter Index/型を一致させます。

## 55. Argument Buffer

UAVとしてComputeから生成し、UAV Barrier/Transition後にIndirect Argumentとして読みます。AlignmentとStrideを検証します。

## 56. Count Buffer

ExecuteIndirectの最大Command数とは別に、GPU生成Count Bufferで実行数を制限できます。Counter Offsetと範囲を守ります。

## 57. 最大Command数

Count値が壊れても最大数を超えないようAPI引数とBuffer Capacityを設定します。生成側でもOverflowを記録します。

## 58. GPU Culling

Object BoundsをFrustum/Hi-Z等で判定し、Visible ObjectだけをOutput ListやIndirect Commandへ書きます。

## 59. Frustum Culling

Sphere/AABBと6 Planeを判定します。World Bounds、非一様Scale、Camera ConventionをCPU版Testと照合します。

## 60. Hi-Z

Depth BufferからMip階層を作り、大きい領域のOcclusionを少数Sampleで保守的に判定します。

## 61. Hi-Z生成

通常は前MipのDepthを2x2等でReductionします。通常Z/Reversed-ZによりMin/Max演算の意味が変わります。

## 62. Occlusionの保守性

誤ってVisible Objectを消さないことを優先します。Bounds拡張、前Frame Depth、Camera急移動時の無効化等を使います。

## 63. Culling出力

Visible Instance List、Draw Count、Material/LOD別Bucket、Indirect Arguments等を生成します。後続Passが必要な順序を定義します。

## 64. LOD選択

Screen Sizeや距離からGPUでLODを選べます。Hysteresisや前回LODが必要ならPersistent Stateを管理します。

## 65. Draw統合

同一Mesh/Material/PSOのInstanceをまとめます。GPUがCommandを生成してもPSO切替自体の構造は別途設計します。

## 66. Indirectの利点

Visible ResultをCPUへReadbackせずDrawへ接続でき、Object数が多いSceneのCPU Command生成を減らせます。

## 67. IndirectのCost

追加Compute、Buffer、Barrier、Sort、Debug複雑性が増えます。Object数が少ない場合はCPU方式が速く単純なこともあります。

## 68. GPU Particle

Particle StateをStructured Bufferへ持ち、Computeで更新、Compact/Spawnし、Indirect Draw/Dispatchへ接続できます。

## 69. Ping-pong Buffer

入力Stateと出力Stateを別Bufferにし、Frameごとに交換するとRead/Write競合を単純化できます。Memoryは増えます。

## 70. Particle Spawn

Free List/Dead List、Append Counter、固定Slot等の方式があります。最大数とOverflow Policyを必ず決めます。

## 71. Particle Simulation

位置、速度、Lifetime、Collision、Noise等を更新します。可変Deltaによる不安定性と決定性の要件を検討します。

## 72. Particle描画

Alive CountからIndirect Draw Argumentを生成できます。透明Sort、Blend、OverdrawがCompute以外の主要Costになります。

## 73. Compute Skinning

前章のJoint PaletteとVertexを読み、Skinned Vertex Bufferへ書きます。Shadow/Main/Motion Vector等で再利用可能です。

## 74. Skinning Barrier

出力をUAVからVertex Buffer用途へTransitionします。Async ComputeならQueue Fenceも必要です。

## 75. Image Processing

Bloom、Blur、Tone Mapping補助、Mip生成等に使えます。Tile/Shared Memoryで重複Texture Loadを減らせます。

## 76. Tile処理

Groupが画像Tileと周辺HaloをShared MemoryへLoadし、同期後Filterします。境界ClampとShared配列Sizeを検証します。

## 77. Light Culling

Screen Tile/Clusterごとに影響Light ListをComputeで作成し、Pixel Shaderの照明Loopを減らせます。

## 78. Clustered Lighting

画面XYに加えDepth方向も分割します。Light Index List Capacity、Overflow、Depth Slice配置を管理します。

## 79. Buffer Allocation

一時UAV、Counter、Argument、Readbackを用途/Lifetime別Allocatorから確保します。Frame間上書きをFenceで防ぎます。

## 80. Transient Resource

Pass間だけ必要なBufferはLifetimeが重ならなければMemoryをAliasできます。Aliasing BarrierとDebug名で追跡します。

## 81. Readback

Debug Countや結果検証ではReadback BufferへCopyしFence完了後CPUで読みます。毎Frame同期ReadbackはPipelineを停止させます。

## 82. 非同期Readback

数Frame遅延を許し、Ring状Readback SlotとFenceを使います。Gameplayの即時判断へ安易に使いません。

## 83. Error Flag Buffer

Overflow、NaN、範囲外相当の条件をAtomic Flag/Counterへ記録し、開発Buildで遅延ReadbackするとGPU処理を診断しやすくなります。

## 84. Shader Debug

PIX Capture、UAV可視化、Debug Output Buffer、固定小Dataを使います。GPU Shader内でCPU Debuggerと同じ感覚を期待しません。

## 85. PIX Timing

DispatchごとのDuration、Occupancy関連指標、Cache/Memory、Barrier、Queue Idleを確認します。単一GPUの結果だけで断定しません。

## 86. Timestamp Query

Pass前後へTimestampを書き、FrequencyからGPU時間へ変換します。Query HeapとResolve BufferのLifetimeを管理します。

## 87. 性能分類

ALU-bound、Bandwidth-bound、Latency-bound、Atomic-bound、Occupancy不足等をCounterとExperimentで切り分けます。

## 88. 最適化の順序

正しい結果を小Dataで確認し、実Sceneを計測し、最大Costを一つずつ改善します。推測だけでThread数を変更しません。

## 89. CPU参照実装

Scan/Culling/Particle等の単純CPU版を用意し、同じ入力でGPU結果と比較するとAlgorithm/同期Bugを見つけやすくなります。

## 90. 決定性

Atomic追加順や浮動小数Reduction順は結果が変わり得ます。Network/Game Simulationに必要な決定性をGPUへ無条件に期待しません。

## 91. Floating-point誤差

並列ReductionはCPU逐次加算と丸め順が異なります。絶対/相対誤差とNaN PolicyをTestに定義します。

## 92. Bounds検査

要素数、Stride、Offset、Dispatch端数、Counter CapacityをCPU側でも検証します。Debug LayerだけではData論理Bugを全て検出できません。

## 93. Resource初期化

Counter/Argument/Outputを必要な初期値へClearします。未初期化GPU Memoryを有効Dataと解釈しません。

## 94. Descriptor Lifetime

GPUが参照中のSRV/UAV Descriptorを上書きしません。Frame Descriptor領域とPersistent領域を分けます。

## 95. Hot Reload

ShaderのRoot Signature、Thread Group Size、Data Layoutが変わる場合、PSOだけ差替えればよいとは限りません。Compatibilityを検証します。

## 96. よくある失敗：結果が全部0

Descriptor Table、UAV State、Root Parameter、Dispatch Count、Bounds条件、UAV書込み先を確認します。

## 97. よくある失敗：結果が時々壊れる

UAV Barrier不足、Counter Reset漏れ、Race、Frame Resource上書き、Queue同期不足を確認します。

## 98. よくある失敗：一部だけ欠ける

Group Countの切捨て、末尾Bounds、Capacity不足、Count Buffer Offset、Frustum規約を確認します。

## 99. よくある失敗：ExecuteIndirectが描かない

Command Signature/Stride、Argument State、Count、Root Signature、VB/IB/PSO、生成DataをPIXで確認します。

## 100. よくある失敗：Compute化で遅くなる

小さ過ぎるWork、追加Barrier、Readback、Atomic競合、低Occupancy、余分なMemory往復を測定します。

## 101. 高速戦闘Sceneへの適用

多数Effect/敵のCulling、Particle、Skinning、Light Listを候補にします。ただしLatency、透明Overdraw、Animation更新Costを含むFrame全体で評価します。

## 102. Frame内の例

```text
animation upload
 -> compute skinning
 -> UAV/vertex transition
 -> depth pre-pass
 -> Hi-Z build
 -> GPU culling
 -> indirect argument transition
 -> ExecuteIndirect shadow/main
 -> particle simulation/draw
```

Resource依存からBarrierとQueue同期を導きます。

## 103. 実装Checklist

- [ ] Thread/Group/Dispatch数と末尾処理を確認する。
- [ ] Buffer Format/Stride/Capacityを一致させる。
- [ ] UAV Flag、Descriptor、Stateを正しく設定する。
- [ ] Group内/Dispatch間/Queue間同期を区別する。
- [ ] Counterを初期化しOverflowを検出する。
- [ ] Indirect SignatureとArgument Layoutを一致させる。
- [ ] GPU結果をCPU参照実装と比較する。
- [ ] Descriptor/BufferをFence完了前に再利用しない。
- [ ] PIX/Timestampで実測する。

## 104. 理解確認問題

1. Dispatch引数とnumthreadsの関係を説明してください。
2. Group Shared Memoryが共有される範囲を説明してください。
3. Transition BarrierとUAV Barrierの違いを説明してください。
4. Atomic Contentionを減らす方法を提案してください。
5. ScanがCompactionに必要な理由を説明してください。
6. ExecuteIndirectのSignature、Argument、Countを説明してください。
7. GPU CullingからDrawまでのState遷移を説明してください。
8. Compute Skinningの利点とCostを説明してください。
9. 非同期Readbackが必要な理由を説明してください。
10. Compute化が遅くなる条件を挙げてください。

## 105. 要点

- ComputeはThread Gridへ処理を割り当てる汎用GPU Pipelineです。
- UAVはRandom Writeを可能にしますがRace、State、Barrier管理が必要です。
- Group内同期はGroup間同期の代わりになりません。
- Scan/Compaction/SortはGPU Driven Data生成の基礎です。
- ExecuteIndirectはGPU結果をCPU ReadbackなしでDraw/Dispatchへ接続します。
- Particle、Skinning、Cullingは容量、同期、Memory Costを含めて評価します。
- 正しさをCPU参照実装で検証し、PIXとTimestampで性能を測ります。

## 106. 公式資料

- [Compute Shader Overview](https://learn.microsoft.com/en-us/windows/win32/direct3d11/direct3d-11-advanced-stages-compute-shader)
- [ID3D12GraphicsCommandList::Dispatch](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12graphicscommandlist-dispatch)
- [ID3D12Device::CreateUnorderedAccessView](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12device-createunorderedaccessview)
- [Executing and Synchronizing Command Lists](https://learn.microsoft.com/en-us/windows/win32/direct3d12/executing-and-synchronizing-command-lists)
- [Indirect Drawing](https://learn.microsoft.com/en-us/windows/win32/direct3d12/indirect-drawing)
- [ID3D12GraphicsCommandList::ExecuteIndirect](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12graphicscommandlist-executeindirect)

## 107. 次章への接続

次章ではMultithread Command Recordingを扱います。Compute/Graphics PassをJobへ分解し、複数ThreadでCommand Listを安全に記録してFrame Submissionへ統合します。
