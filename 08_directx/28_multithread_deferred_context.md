# DirectX 11：Multithread・Deferred Context

この章では、CPUの複数Threadで描画準備とDirect3D 11 Command記録を進める方法を学びます。Device、Immediate Context、Deferred ContextのThread規則、Command List、Resource更新、Job System、同期、性能測定までを扱います。

## 1. なぜ描画をMultithread化するのか

Sceneが大きくなると、Culling、Animation、Sorting、Constant生成、Draw Command発行等のCPU処理が一つのThreadへ集中します。GPUが待っているならCPU側の並列化が候補になります。

## 2. CPUとGPUを区別する

```text
CPU Thread並列化 : 描画Data準備やCommand記録を複数Coreで進める
GPU並列実行     : GPUがShaderやDrawを処理する
```

Deferred ContextはCPU側のCommand記録機構であり、GPU上でDrawを自動的に同時実行する機能ではありません。

## 3. Direct3D 11の三要素

```text
ID3D11Device
 ├─ Resource/View/Shader/State Objectを作る
 ├─ Immediate Contextを一つ持つ
 └─ Deferred Contextを複数作れる
```

役割とThread規則を個別に覚えます。

## 4. DeviceのThread規則

`ID3D11Device`のResource生成Methodは一般に複数Threadから呼べるよう設計されています。ただしApplication側のCache、Allocator、Containerは別途同期が必要です。

## 5. Immediate Contextの役割

Immediate ContextはCommandを実行Streamへ送る中心Contextです。Present、Command List実行、最終的なFrame順序の確定を通常Render Threadが担当します。

## 6. Immediate ContextのThread規則

Immediate Contextは同時に複数Threadから操作しません。一つの所有Threadへ限定するのが基本設計です。

```text
Render ThreadだけがImmediate Contextを触る
Worker ThreadはCPU Dataまたは専用Deferred Contextを触る
```

## 7. Deferred Contextの役割

Deferred ContextはDrawやState設定等をCommand Listへ記録します。記録したCommandは後でImmediate Contextから実行します。

## 8. Deferredの意味

Commandの実行を後回しにするという意味です。GPUへ直接Submitする独立Queueではありません。

## 9. Deferred Contextも同時共有しない

一つのDeferred Contextを複数Threadから同時操作しません。Workerごと、または同時実行Jobごとに専用Contextを割り当てます。

## 10. 所有権表

| Object | 推奨所有者 | 同時操作 |
|---|---|---|
| Device | Engine全体 | Resource生成は可能 |
| Immediate Context | Render Thread | しない |
| Deferred Context 0 | Worker 0 | しない |
| Deferred Context 1 | Worker 1 | しない |
| Scene writable state | Simulation側 | Phaseで分離 |
| Immutable Render Snapshot | Render Job群 | 読み取り共有 |

## 11. Deferred Context生成

```cpp
Microsoft::WRL::ComPtr<ID3D11DeviceContext> deferredContext;

ThrowIfFailed(device->CreateDeferredContext(
    0,
    &deferredContext));
```

Reserved引数は0を指定します。

## 12. Context Pool

```cpp
struct DeferredWorker
{
    ComPtr<ID3D11DeviceContext> context;
    ComPtr<ID3D11CommandList> commandList;
    LinearFrameAllocator frameAllocator;
};
```

毎Frame Contextを生成せず、Worker数に応じて再利用します。

## 13. Command記録

```cpp
void RecordOpaquePass(
    ID3D11DeviceContext* context,
    std::span<const RenderItem> items)
{
    context->VSSetShader(vertexShader.Get(), nullptr, 0);
    context->PSSetShader(pixelShader.Get(), nullptr, 0);

    for (const RenderItem& item : items)
    {
        BindItem(context, item);
        context->DrawIndexed(item.indexCount, item.startIndex, item.baseVertex);
    }
}
```

同じ関数へImmediate/Deferred Contextのどちらも渡せる形にするとSingle-thread Pathと共有できます。

## 14. FinishCommandList

```cpp
ComPtr<ID3D11CommandList> commandList;
ThrowIfFailed(deferredContext->FinishCommandList(
    FALSE,
    &commandList));
```

記録を終了し、実行可能なCommand Listを作ります。

## 15. RestoreDeferredContextState

`FinishCommandList`の第一引数は、完了後にDeferred ContextのStateを記録開始前へ復元するかを指定します。`FALSE`ならDefault Stateへ戻るため、次の記録で必要Stateを再設定します。

## 16. ExecuteCommandList

```cpp
immediateContext->ExecuteCommandList(
    commandList.Get(),
    FALSE);
```

Command ListはImmediate Contextへ渡した順序でGPU Command Streamへ組み込まれます。

## 17. RestoreContextState

`ExecuteCommandList`の第二引数は、実行後にImmediate ContextのStateを実行前へ復元するかを指定します。

```text
TRUE  : 実行前Stateを保存して復元するCostがある
FALSE : Command List実行後のStateを前提にせず再Bindする
```

Performanceと正しさを測定し、Renderer全体でPolicyを統一します。

## 18. Stateを暗黙に頼らない

Command Listが前のPassのStateを継承すると仮定せず、Passが必要とするShader、Input Layout、Buffer、State、Viewを明示します。

## 19. Command Listの実行順序

Workerの完了順ではなくRendering要件で実行順を決めます。

```text
Shadow -> Depth Prepass -> Opaque -> Transparent -> Post Process -> UI
```

## 20. Frame Timeline

```text
Main/Render : snapshot作成 ───────── collect ─ execute lists ─ present
Worker 0    :               shadow record ─┘
Worker 1    :               opaque A record ─┘
Worker 2    :               opaque B record ─┘
Worker 3    :               effect record ───┘
```

並列記録後にFrame順序へMergeします。

## 21. 何をJobへ分割するか

- ViewごとのCulling
- Render Item生成
- Animation Pose評価
- Skinning Matrix生成
- Sort Key計算
- Shadow Cascadeごとの記録
- Opaque Item範囲ごとの記録
- Particle Data準備

## 22. Pass単位分割

Shadow、Opaque、Effect等のPass単位は依存関係が分かりやすい一方、Item数が偏ると一部Workerだけが長く働きます。

## 23. Chunk単位分割

大量Opaque Itemを数百個ずつChunkに分けるとLoad Balanceしやすくなります。ただしCommand List数とExecute Costが増えます。

## 24. 粒度のTrade-off

```text
小さすぎるJob : Queue、同期、Command ListのOverheadが増える
大きすぎるJob : Coreごとの仕事量が偏る
```

Frame CaptureとCPU Profilerで実測して決めます。

## 25. Render Snapshot

Simulation中のObjectをWorkerが直接読むと、同時更新によるData Raceが起きます。Frame開始時に描画専用の不変Snapshotを作ります。

```cpp
struct RenderObjectSnapshot
{
    Matrix world;
    Bounds bounds;
    MeshHandle mesh;
    MaterialHandle material;
    uint32_t layer;
};
```

## 26. Mutable Gameplay Stateを渡さない

CharacterやEnemyの可変Object PointerをRender Jobへ直接渡さず、必要な値をRender Proxy/SnapshotへCopyします。

## 27. Double Buffer Snapshot

```text
Simulation writes Snapshot B
Rendering reads Snapshot A
Frame boundaryでA/B交換
```

更新と描画の所有権を明確にできますが、入力遅延とMemory Costを理解して採用します。

## 28. Jobの入力と出力

```cpp
struct RecordJob
{
    const RenderSnapshot* snapshot;
    std::span<const uint32_t> visibleIndices;
    PassType pass;
    uint32_t workerIndex;
};

struct RecordResult
{
    uint32_t submissionOrder;
    ComPtr<ID3D11CommandList> commandList;
};
```

共有書込みを減らし、結果をJobごとに返します。

## 29. Worker Queue

Thread-safe QueueへJobを積み、Workerが取得します。停止時にはCancellationとJoin順序を設計し、Device破棄後にContextを触らせません。

## 30. Barrier

Command Listを実行する前に必要Jobがすべて完了している必要があります。`std::latch`、Task Group、Fence相当のCPU同期を使えます。

```cpp
std::latch recordingDone(jobCount);
```

## 31. Busy Waitを避ける

Main ThreadがAtomic Flagを回し続けるより、Job SystemのWait機構やCondition Variableを使います。ただし短時間待機ではEngine固有のScheduler設計も計測対象です。

## 32. False Sharing

別Threadが同じCache Line上のCounterやResultを書き換えると、論理的に別DataでもCache Lineが往復します。Worker統計やAllocator Headを分離します。

```cpp
struct alignas(64) WorkerStats
{
    uint64_t drawCalls = 0;
    uint64_t triangles = 0;
};
```

## 33. Per-thread Allocator

Render Item、Constant Data、一時配列を共有Heapから頻繁に確保するとLock競合します。FrameごとのLinear AllocatorをWorker単位で持たせます。

## 34. Resource生成

DeviceによるResource/View/State生成はWorkerから可能でも、同じAssetを重複生成しないCache同期が必要です。Loading PipelineとRenderingを分けます。

## 35. Immutable Resource

MeshやTexture等のImmutable Resourceは生成完了後に読み取り共有しやすいDataです。公開前に完全初期化し、HandleのLifetimeを保証します。

## 36. Resource Lifetime

Command Listが参照するResourceは、List実行とGPU使用が終わるまで生存させます。Job終了時にCPU側Handleを破棄できるとは限りません。

## 37. COM Pointer

`ComPtr`のCopyは参照Countを操作します。大量ItemごとにCopyせず、Frame中のLifetimeを別Ownerが保証したHandleやPointerを使う設計も検討します。

## 38. Constant Buffer更新

Deferred ContextでのDynamic Resource更新には制約とDriver差が関係します。単純に全Workerが同じDynamic Constant BufferへMapしてはいけません。

## 39. Deferred ContextのMap

Deferred Contextで`Map`できる用途は限定されます。Dynamic Resourceと`D3D11_MAP_WRITE_DISCARD`を中心に設計し、使用するRuntime/API Versionの仕様を確認します。

## 40. 最初のMapとDISCARD

Command List内でDynamic Resourceを更新する場合、最初のMapは`WRITE_DISCARD`を使用する規則を守ります。`WRITE_NO_OVERWRITE`の利用可否はResource種類とDirect3D 11.1機能Supportを確認します。

## 41. Worker専用Dynamic Buffer

```text
Worker 0 -> Dynamic CB pool 0
Worker 1 -> Dynamic CB pool 1
Worker 2 -> Dynamic CB pool 2
```

同じBufferへの同時Mapを避けられますが、Memory量と管理Object数が増えます。

## 42. Prebuilt Constant Data

WorkerはCPU MemoryへConstantを構築し、Render ThreadがまとめてUploadする方式もあります。Deferred Context記録とUpload順序の依存を単純化できます。

## 43. Constant Buffer Ring

大きなDynamic Bufferの領域をFrame/Workerごとに割り当てる設計です。Direct3D 11.1のConstant Buffer範囲Bindingを使える環境では小分けObjectを減らせます。

## 44. UpdateSubresource

`UpdateSubresource`は便利ですが、Resource競合時に内部Copyが増える可能性があります。頻度、Size、Context、Resource Usageに合う更新方法を測定します。

## 45. Staging Resource

StagingへのReadbackやCPU AccessはCommand記録のHot Pathと分離します。即時MapでGPU完了を待つ設計は並列記録の利益を失います。

## 46. QueryとGetData

GPU Query結果を取得する`GetData`はImmediate Context側の処理として扱います。Deferred Contextは結果を同期取得する場所ではありません。

## 47. Predicationと特殊Command

すべてのContext MethodをDeferred Contextで同じ感覚で使えるとは限りません。API仕様、Feature Level、Driver Supportを確認し、Debug Layerで検証します。

## 48. ClearState

`ClearState`はContextのBindingをDefaultへ戻します。毎Draw呼ぶものではなく、記録境界のPolicyに沿って使用します。

## 49. Hazard Tracking

Command List AがResourceへ書き、Bが読む場合、Immediate ContextでA→Bの順に実行します。Direct3D 11 RuntimeがBinding Hazardを管理しても、Application側は論理依存を正しく並べます。

## 50. Command List間依存

```text
Shadow Depth書込み -> Main PassでShadow SRV読取り
Compute UAV書込み -> DrawでVertex/SRV読取り
Scene Color書込み -> Post ProcessでSRV読取り
```

ProducerをConsumerより前にExecuteします。

## 51. Resource Conflictを避ける

Worker Jobが同じTexture領域へ無秩序に書き込む設計を避けます。PassやSubresource、Viewport、Buffer領域を明示的に分割します。

## 52. Transparent描画

Transparent Objectは通常Depth順が必要です。WorkerごとのListを単純連結すると順序が壊れるため、Global Sort後にChunk化するかMerge Sortします。

## 53. Opaque描画

OpaqueはPipeline/Material/Mesh/Depth等のSort Keyを作り、安定したOrderでChunkへ分割しやすい対象です。

## 54. Shadow Cascade

Cascadeごとに異なるViewとVisible Listを持つため、独立Jobへしやすい処理です。ただし各Cascadeの負荷差を測定します。

## 55. Animationとの依存

Pose評価が終わる前にSkinning Matrixを読むRender Jobを開始してはいけません。

```text
Animation Evaluate -> Palette Build -> Render Record
```

Job Graphへ依存Edgeを表します。

## 56. Effectとの依存

GameplayがSpawn Eventを確定し、Effect Simulationが更新し、そのSnapshotから描画Commandを生成します。可変Listへの同時追加はThread-local ListからMergeします。

## 57. Fast Action Sceneでの分割例

```text
Phase A : Character/Enemy Animation並列評価
Phase B : World/Shadow/Effect Culling
Phase C : Sort + Render Item確定
Phase D : Pass/Chunk別Command記録
Phase E : Render Threadが順序どおりExecute + Present
```

## 58. Frame Latency

SimulationとRenderingを別FrameとしてPipeline化するとCore利用率を上げられますが、入力から表示までのLatencyが増える可能性があります。操作応答を重視するGameでは慎重に測ります。

## 59. Overlapの考え方

GPUがFrame Nを処理中にCPUがFrame N+1を準備するのは通常のPipelineです。さらにCPU Worker間の並列性を加えても、同期点が早すぎるとOverlapできません。

## 60. Amdahlの法則

直列部分が残るほどCore数を増やしても高速化の上限があります。Present、Execute、Global Sort、Main-thread-only処理をProfileで特定します。

## 61. Deferred Contextが必ず速いわけではない

Driver、Draw数、State変更、CPU Core数、Command List粒度により、記録と実行のOverheadが上回る場合があります。Single-thread Immediate Pathと必ず比較します。

## 62. Driver Command List Support

`D3D11_FEATURE_THREADING`を`CheckFeatureSupport`で調べ、DriverがCommand ListやConcurrent Resource Creationをどの程度Supportするか確認できます。

```cpp
D3D11_FEATURE_DATA_THREADING threading{};
ThrowIfFailed(device->CheckFeatureSupport(
    D3D11_FEATURE_THREADING,
    &threading,
    sizeof(threading)));
```

## 63. DriverCommandLists

`DriverCommandLists`がFALSEでもRuntimeが機能をEmulateする場合がありますが、性能特性は変わります。機能可否と高速化可否を混同しません。

## 64. DriverConcurrentCreates

Concurrent Resource CreationのDriver Supportを示します。FALSEならRuntimeの同期やSerializationが入り得るため、Loading性能を測ります。

## 65. ID3D11Multithread

Immediate Contextから`ID3D11Multithread`を取得しMultithread Protectionを有効にできますが、Context呼出しをLockで直列化する安全策です。

```cpp
ComPtr<ID3D11Multithread> multithread;
ThrowIfFailed(immediateContext.As(&multithread));
multithread->SetMultithreadProtected(TRUE);
```

## 66. Multithread ProtectionのCost

複数ThreadからImmediate Contextを自由に叩く設計を推奨する機能ではありません。内部Lock Costが増えるため、単一Render Thread所有を基本にします。

## 67. DXGIとPresent

Swap Chain操作やPresentもRendererの所有Threadへ集約すると、Window/Resize/Frame順序を整理しやすくなります。DXGIとD3D ContextのThread規則を別々に確認します。

## 68. Resizeとの同期

ResizeBuffers前にWorkerの記録を止め、Back Buffer参照Command Listを破棄または完了させ、View参照を解放します。古いSizeのResourceをJobが参照しない世代管理が必要です。

## 69. Shutdown順序

1. 新規Render Jobの受付を止める。
2. Workerへ停止を通知する。
3. 実行中Jobを完了またはCancelする。
4. Worker ThreadをJoinする。
5. Command ListとDeferred Contextを解放する。
6. Immediate Context、Swap Chain、Deviceを解放する。

## 70. Device Removed時

Device再生成中にWorkerが旧Device/Contextを触らないよう、Renderer世代を停止してJoinします。復旧設計は次章で詳しく扱います。

## 71. Exceptionと失敗伝播

Worker内のHRESULT失敗を握り潰さず、Job ResultへErrorを保存してRender Threadへ返します。途中失敗したCommand Listを実行しません。

## 72. Deterministic Submission

Job完了順が毎回変わっても、`submissionOrder`でSortしてCommand List実行順を固定します。再現性とCapture比較に役立ちます。

## 73. Frame ID

JobとResultへFrame ID、View ID、Pass ID、Chunk IDを付けます。遅れて到着した古いFrameのResultを誤実行しないよう検証します。

```cpp
struct CommandListKey
{
    uint64_t frameId;
    uint16_t viewId;
    uint16_t passId;
    uint32_t chunkId;
};
```

## 74. Debug Name

Deferred Context、Command List相当のPass Data、ResourceへDebug Nameを付け、Capture上でWorker/Pass/Chunkを識別できるようにします。

## 75. CPU Profiler Marker

Job Queue待機、Culling、Sort、Record、FinishCommandList、Collect、Executeを別Markerにします。Thread Timelineで空白時間と長いJobを探します。

## 76. GPU Marker

Command List内部にもPass/Chunk Markerを記録し、CPU記録時間とGPU実行時間を対応付けます。CPU高速化がGPU Bottleneckを変えたか確認します。

## 77. 測定値

- Main/Render Thread時間
- 各WorkerのBusy/Idle時間
- Job数と平均/最大時間
- Command List数
- RecordとFinishの時間
- ExecuteCommandList時間
- Draw Call数とState変更数
- Frame全体CPU/GPU時間

## 78. Baseline

まずImmediate Contextだけの正しい実装をBaselineとして残します。同じScene、Camera、Frame条件でDeferred版と比較します。

## 79. Scaling Test

Worker数を1、2、4、物理Core数等へ変え、Frame時間がどう変化するか測ります。Logical Coreを増やせば必ず比例するとは限りません。

## 80. Stress Test

Object数、Shadow Caster数、Animation数、Effect数を段階的に増やします。通常SceneだけでなくCPU Bottleneckが見える条件を作ります。

## 81. よくある失敗：Immediate Contextを共有

複数WorkerがImmediate Contextを同時に操作するとRaceや内部Lock競合の原因になります。Render Threadへ所有を集約します。

## 82. よくある失敗：一つのDeferred Contextを共有

Deferredという名前でもThread-safe共有Objectではありません。一つの記録期間につき所有Workerを一つにします。

## 83. よくある失敗：State継承を仮定

Command Listが外部Stateを当然引き継ぐと考えると描画が壊れます。Pass Contractとして必要StateをすべてBindします。

## 84. よくある失敗：細かすぎるList

数DrawごとにCommand Listを作るとFinish、Queue、ExecuteのCostが支配します。Profile結果に基づきChunkを大きくします。

## 85. よくある失敗：可変Sceneを直接読む

SimulationとRecordが同じTransformやVectorを同時に読み書きするとC++ Data Raceです。Immutable Snapshotを渡します。

## 86. よくある失敗：Lifetime不足

Command List記録後にResourceやConstant領域を再利用し、実行時に無効Dataを参照します。Frame Resourceの使用完了まで保持します。

## 87. よくある失敗：完了順でExecute

Workerが早く終わった順に実行するとPass依存とTransparent順序が壊れます。明示的なSubmission Keyで整列します。

## 88. よくある失敗：測定なし導入

Parallel化はCode、Memory、同期、Debugの複雑さを増やします。対象CPU Bottleneckと改善幅を測定して導入します。

## 89. 最小導入手順

1. Immediate ContextをRender Thread専有にする。
2. Render Snapshotを作る。
3. Culling/SortだけCPU Job化する。
4. 一つのDeferred Contextで一Passを記録する。
5. Command Listを固定順で実行する。
6. Single-thread結果と画像比較する。
7. Worker/Contextを増やす。
8. Dynamic Resource更新をWorker単位へ分離する。
9. CPU/GPU Profilerで利益を確認する。

## 90. Architecture Checklist

- [ ] Immediate Contextの所有Threadが一つである。
- [ ] Deferred Contextを同時共有していない。
- [ ] WorkerはImmutable Render Snapshotを読む。
- [ ] Job依存がGraphまたはPhaseで明示されている。
- [ ] Command Listの実行順が決定的である。
- [ ] Dynamic Resourceの更新領域が競合しない。
- [ ] Resource LifetimeがGPU使用完了まで保証される。
- [ ] Resize/Device Lost/Shutdown時にWorkerを停止できる。
- [ ] ErrorがWorkerからRender Threadへ伝播する。
- [ ] Single-thread Baselineと比較している。

## 91. Debug Checklist

- [ ] Debug LayerにContext/Resource Warningがない。
- [ ] Frame IDとPass IDをLogへ出せる。
- [ ] CPU Thread TimelineをCaptureできる。
- [ ] Command List数とDraw数を表示できる。
- [ ] WorkerごとのBusy/Idle率が見える。
- [ ] Single-thread Modeへ切り替えて比較できる。
- [ ] Scene更新を止めた状態で画像が一致する。

## 92. 理解確認問題

1. Device、Immediate Context、Deferred ContextのThread規則を説明してください。
2. Deferred ContextがGPUの独立Queueではない理由を説明してください。
3. `FinishCommandList`と`ExecuteCommandList`の役割を説明してください。
4. Command Listの完了順と実行順を分ける理由を説明してください。
5. Render SnapshotがC++ Data Raceを防ぐ仕組みを説明してください。
6. Job粒度が小さすぎる場合のCostを挙げてください。
7. Multithread Protectionと単一所有設計の違いを説明してください。
8. Deferred Context導入が遅くなる条件を説明してください。

## 93. 章末要点

- Deferred ContextはCPU上でCommand Listを並列記録する仕組みです。
- Immediate ContextはRender Thread一つへ所有させます。
- 一つのDeferred Contextも一つのWorkerだけが操作します。
- WorkerへImmutable Render Snapshotを渡し、可変Gameplay Stateと分離します。
- Command Listは描画依存に従う固定順でImmediate Contextから実行します。
- Resource更新、Lifetime、Resize、ShutdownをThread設計に含めます。
- Multithread化はBaselineとProfilerで実際の利益を確認します。

## 94. 公式資料

- [Introduction to Multithreading in Direct3D 11](https://learn.microsoft.com/en-us/windows/win32/direct3d11/overviews-direct3d-11-render-multi-thread-intro)
- [How To: Use Command Lists](https://learn.microsoft.com/en-us/windows/win32/direct3d11/overviews-direct3d-11-render-multi-thread-command-list)
- [ID3D11Device::CreateDeferredContext](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11device-createdeferredcontext)
- [ID3D11DeviceContext::FinishCommandList](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11devicecontext-finishcommandlist)
- [ID3D11DeviceContext::ExecuteCommandList](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11devicecontext-executecommandlist)
- [ID3D11Multithread](https://learn.microsoft.com/en-us/windows/win32/api/d3d11_4/nn-d3d11_4-id3d11multithread)
- [D3D11_FEATURE_DATA_THREADING](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/ns-d3d11-d3d11_feature_data_threading)
- [ID3D11DeviceContext::Map](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11devicecontext-map)

次章では、Window Resize、Fullscreen、Device Removed、Resource再生成、復旧可能なRenderer Lifecycleを扱います。
