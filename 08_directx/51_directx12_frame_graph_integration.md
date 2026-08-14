# DirectX 12 第19章：Frame Graph統合

この章では、描画PassとResource Accessを宣言し、依存順、Barrier、Transient Memory、Queue同期、並列Command Recordingを自動構築するFrame Graphを学びます。設計、Compile、Execute、検証、可視化、性能改善まで扱います。

## 1. 到達目標

- Pass/Resource/Accessから依存Graphを構築する。
- 不要Passを除去し正しい実行順を得る。
- Resource State、Queue Fence、Aliasを自動生成する。
- Setup時宣言とExecute時処理を分離する。
- Graphを可視化し誤依存と性能問題を診断する。

## 2. Frame Graphとは

一Frame内のPass、Resource、Read/Write関係をGraphとして表し、実行計画へCompileする仕組みです。

## 3. 解決する問題

手書きのBarrier、Pass順、Temporary RT、Queue Waitが増えると変更時に矛盾しやすくなります。Graphが共通情報から一貫して導きます。

## 4. 解決しない問題

Shader数式、Material仕様、Draw内容、Gameplay Dataの正しさは自動では決まりません。Pass実装の責務は残ります。

## 5. 基本要素

```text
Pass node
Resource node/version
Read/Write edge
Imported/Transient resource
Execution callback
```

## 6. 二段階API

```text
Setup/Build : resourceとaccessを宣言
Execute     : commandを記録
```

Execute中に未宣言Resourceへ触れない規約を作ります。

## 7. Pass例

```cpp
graph.AddPass<DepthPassData>(
    "DepthPrepass",
    [&](Builder& b, DepthPassData& data)
    {
        data.depth = b.WriteDepth(depthDesc);
        b.Read(vertexBuffer, Access::VertexBuffer);
    },
    [&](const DepthPassData& data, Context& ctx)
    {
        ctx.DrawDepth(data.depth);
    });
```

型安全Handleと明示Accessを目指します。

## 8. Resource Handle

内部IndexとGenerationを持ち、古いFrame/VersionのHandle使用を検出します。生の`ID3D12Resource*`をGraph外へ漏らし過ぎません。

## 9. Resource Version

Writeごとに新Versionを作るSSA風設計にすると、どのWrite結果をReadするか明確になります。

## 10. Imported Resource

Swap Chain、History、外部Shadow等をGraphへImportし、初期State、最終State、所有者を指定します。

## 11. Transient Resource

Graphが作成/破棄とMemory配置を所有する一時Texture/Bufferです。Frame外へHandleを保存しません。

## 12. Persistent Resource

HistoryやAssetは外部Registryが所有し、GraphはFrame中のAccessだけ管理します。

## 13. Texture Desc

Size、Format、Mip、Array、Sample、Flags、Clear Valueを宣言します。Viewport相対Sizeも解決可能にします。

## 14. Buffer Desc

Byte Size、Stride、Flags、用途を宣言します。Size計算Overflowと0 SizeをCompile時に拒否します。

## 15. Access Type

```text
SRV pixel/non-pixel
UAV read/write
RTV
DSV read/write
copy source/destination
vertex/index/constant
indirect argument
present
```

## 16. ReadとWrite

Read/Readは依存を追加しない場合があります。Write→Read、Read→Write、Write→Writeには順序が必要です。

## 17. Subresource

Mip、Array Slice、Plane単位のAccessを表せれば不要な依存/Barrierを減らせます。初期版は全Resource粒度でも構いません。

## 18. Dependency Edge

ResourceのProducerとConsumerからPass間Edgeを作ります。名前や登録順だけで依存を推測しません。

## 19. RAW

Read After WriteはConsumerがProducer結果を必要とする基本依存です。

## 20. WAR

Write After Readは先のReadが終わるまで内容を上書きできません。Alias/Queue配置にも影響します。

## 21. WAW

Write After Writeは最終結果と順序を明確にします。不要な前WriteならPass Cull候補になります。

## 22. Topological Sort

依存を守る線形実行順を得ます。同時に入次数が0のPassが複数なら並列候補です。

## 23. Cycle検出

SortできなければCycleがあります。関係Pass/Resource/Versionを具体的にError表示します。

## 24. Cycleの典型原因

同Frame Feedback、誤Version参照、暗黙順序、Read/Write宣言間違いです。Historyは前FrameResourceへ分離します。

## 25. Output Root

Present、外部Export、Readback、Side Effect Passを最終Rootとして、そこへ寄与するPassだけを残します。

## 26. Pass Culling

最終Outputへ到達しないPassを除去します。Debug/Query/External Side EffectはCull不可Flagを持たせます。

## 27. Resource Culling

CullされたPassだけが使うTransient Resourceも作成しません。

## 28. Side Effect

外部File/Query/Counter/Queue Signal等、Resource Edgeだけで表せない効果を明示します。乱用するとCull最適化が消えます。

## 29. Compile Phase

```text
validate declarations
build edges
cull
topological sort
derive lifetime
allocate transient memory
plan barriers
assign queues/fences
build recording jobs
```

## 30. Execute Phase

Compile済みPlanを順に/並列に記録し、Barrier、Pass Callback、Signal/WaitをCommand Listへ出力します。

## 31. State Mapping

宣言AccessをLegacy Resource StateまたはEnhanced BarrierのSync/Access/Layoutへ変換します。

## 32. Initial State

Imported Resourceは呼出側が保証する開始State、Transient Resourceは作成時Stateを持ちます。

## 33. Final State

Present、外部利用、次Frame規約に必要な最終Stateへ遷移させます。

## 34. Barrier Coalescing

同じ境界のBarrierをBatch化し、不要な同State遷移を除外します。正しさ確認後に最適化します。

## 35. UAV Barrier

UAV Write後の依存をAccess履歴から生成します。Stateが同じでも順序が必要です。

## 36. Aliasing Barrier

Transient Allocatorが同じHeap範囲を別Resourceへ再利用する境界へ自動挿入します。

## 37. Lifetime

各ResourceのFirst/Last AccessをSort済みPlanから得ます。Queue Overlapを考慮したLifetimeへ拡張します。

## 38. Transient Allocation

Size/Alignment/Heap Classと非重複LifetimeからPlaced Resource Offsetを割り当てます。

## 39. Peak削減

Pass順の自由度内でLifetime重複を減らせます。ただしGPU Parallelism/Cache/Barrierとの交換です。

## 40. Resource Materialization

Compile後、CullされなかったTransient ResourceだけをHeap Offset上へ作成します。

## 41. View生成

RTV/DSV/SRV/UAV Descriptorも宣言AccessとSubresourceから生成/Cacheできます。

## 42. Descriptor Lifetime

Transient ViewはFrame Descriptor Arenaへ置き、関連Frame Fence完了後だけ再利用します。

## 43. Queue Assignment

Graphics必須PassはDirect、CopyはCopy候補、ComputeはDirect/Compute候補です。依存とCostから決めます。

## 44. Queue間Edge

Producer/Consumerが異なるQueueならSignal/Waitを生成します。同一QueueならQueue順で足ります。

## 45. Wait Batch

同じQueue間の細かいEdgeを必要地点の最大Fenceへまとめ、Signal/Wait乱発を避けます。

## 46. Deadlock検査

Queue/Fence GraphのCycleと未Signal値をCompile時に拒否します。

## 47. Async Compute Heuristic

Computeと名付けられただけで移動せず、依存Window、予測Cost、Bandwidth競合、GPU Profileを使います。

## 48. 並列Recording

依存上同時準備可能なPassや一Pass内ChunkをJob化し、前章のAllocator/List Poolへ割り当てます。

## 49. Pass Context

Callbackへ宣言済みResource/View、Command List、Frame Dataだけを渡します。Global Renderer状態への依存を減らします。

## 50. Compile-time型安全性

Pass Data構造体へHandleを保持し、BuilderでWriteしたOutputをExecuteで使います。String検索をHot Pathに残しません。

## 51. Runtime Validation

Execute中に要求したViewがSetup宣言と一致するか開発Buildで検査します。

## 52. Blackboard

Scene Color/Depth等の共有Handle Registryは便利ですが、隠れた依存を増やさないよう型/Ownershipを限定します。

## 53. Pass Template

Shadow、Blur、Mip等の共通Passを関数/Template化します。Global Singletonに結び付けません。

## 54. Conditional Pass

Feature Toggleに応じBuild段階で追加/省略します。Execute中だけ何もしないPassを大量に残しません。

## 55. Dynamic Resolution

相対Size DescをFrame開始時Resolutionへ解決し、Size変更時にHistoryをInvalidationします。

## 56. History Ping-pong

前FrameReadと今FrameWriteを別Persistent Resourceへ割り当て、Frame境界で交換します。

## 57. Multi-view

Split Screen、Shadow Cascade、Stereo等はArray/LoopでPassを生成し、View IDをDebug名へ含めます。

## 58. External Callback

UI/Middlewareを統合する場合もRead/Write/Final StateをGraphへ宣言し、内部Barrierと競合させません。

## 59. Capture Mode

PIX Capture時はAlias無効、Resource保持、追加Readback等の診断Optionを用意できます。通常性能との差を記録します。

## 60. Graph Visualization

Pass Node、Resource Edge、Queue色、Cull、Lifetime、Barrier、Memory OffsetをDOT/JSON/UIへ出力します。

## 61. Resource Timeline

各Resourceの作成、First/Last Use、State、Alias相手を横軸Passで表示します。

## 62. Queue Timeline

Direct/Compute/Copy Lane、Signal/Wait、推定/実測時間を表示しCritical Pathを調べます。

## 63. Memory Timeline

Transient Heap OffsetとLifetime矩形を表示し、Peak、Hole、Alias効率を確認します。

## 64. Barrier Report

PassごとのTransition/UAV/Alias数、全Resource遷移履歴、不要候補を出力します。

## 65. Pass Statistics

CPU Build/Record、GPU時間、Draw/Dispatch、Descriptor、Upload、Read/Write Bytesを収集します。

## 66. Stable ID

Pass/ResourceへFrame内Stable IDを与え、PIX Marker、DRED、Log、Visualizationを対応させます。

## 67. Cache

Graph構造が同じならCompile Planを再利用できます。Resolution、Feature、Format、Queue Policy等をCache Keyへ含めます。

## 68. Dynamic Dataと構造

Object数等のData変化とPass/Resource構造変化を区別し、不必要なGraph Recompileを避けます。

## 69. Error Message

「失敗」だけでなくPass名、Resource名、Version、Access、前Writer、Queue、期待Stateを表示します。

## 70. Unit Test

Edge生成、Cycle、Cull、Sort、Version、State、Lifetime、Alias、Queue Wait、Cache Keyを小GraphでTestします。

## 71. Golden Plan Test

固定GraphのPass順、Barrier、Allocation、Signal/WaitをTextへ出力し、意図した変更かReviewします。

## 72. Random Graph Test

固定Seedで合法/不正Graphを生成し、Cycle検出、範囲、Alias非重複、Determinismを検査します。

## 73. Integration Test

Resize、Feature Toggle、Async ON/OFF、MSAA、History、Imported Resourceを画像結果とDebug Layerで検証します。

## 74. Failure Injection

Transient OOM、Descriptor不足、Pass Callback失敗、Device Removed時にCleanup/Reportが安全かTestします。

## 75. よくある失敗：隠れたAccess

Passが未宣言Global Resourceを触るとBarrier/Lifetimeが壊れます。Context経由AccessとValidationで防ぎます。

## 76. よくある失敗：Passが消える

最終Outputへ繋がらずSide Effect宣言もない可能性をGraph可視化で確認します。

## 77. よくある失敗：Memoryが破損

Lifetime計算、Queue Overlap、Alias Barrier、初回Clear、Frame Fenceを確認します。

## 78. よくある失敗：Barrier過多

Access宣言が過剰、Subresource粒度不足、Pass往復、State Mapping不適切をReportで確認します。

## 79. よくある失敗：Asyncで遅い

Queue往復、短いPass、Bandwidth競合、Critical Path外、Wait Batch不足をTimelineで確認します。

## 80. 高速戦闘Frame例

```text
animation upload
compute skinning
shadow/depth
Hi-Z + culling
opaque character/environment
lighting
transparent effects
post process
UI
present
```

Graphは依存を表し、品質/負荷に応じPassを追加/除去します。

## 81. 実装Checklist

- [ ] SetupとExecuteを分離する。
- [ ] 全Resource Accessを宣言する。
- [ ] Version付きHandleでWriter/Readerを明確にする。
- [ ] Cycle/Cull/Sortを検証する。
- [ ] State/UAV/Alias BarrierをAccessから生成する。
- [ ] Queue EdgeからSignal/Waitを生成する。
- [ ] LifetimeからTransient Allocationを構築する。
- [ ] Pass/Resource/Queue/Memoryを可視化する。
- [ ] 小Graph Unit TestとGolden Planを用意する。

## 82. 理解確認問題

1. Frame Graphが解決する問題を説明してください。
2. Resource Versionが必要な理由を説明してください。
3. RAW/WAR/WAW依存を説明してください。
4. Pass CullingのRootを挙げてください。
5. Compile Phaseの順序を説明してください。
6. Access宣言からBarrierを作る方法を説明してください。
7. Queue間Edgeを実行へ変換してください。
8. Transient LifetimeとAliasを説明してください。
9. Hidden Resource Accessが危険な理由を説明してください。
10. Graphの性能を可視化する方法を提案してください。

## 83. 要点

- Passは使用ResourceとAccessをSetupで宣言します。
- Graphは依存、Cull、Sort、Barrier、Queue Waitを一つの情報源から導きます。
- Version付きResourceで各Write結果を区別します。
- Transient LifetimeからPlaced Resource MemoryをAliasします。
- Execute Callbackは宣言済みContextだけを使います。
- VisualizationとStable IDをPIX/DRED/Logへ接続します。
- Frame Graph自体もUnit/Golden/Random Test対象です。

## 84. 参考資料

- [Direct3D 12 Resource Barriers](https://learn.microsoft.com/en-us/windows/win32/direct3d12/resource-barriers)
- [Multi-engine synchronization](https://learn.microsoft.com/en-us/windows/win32/direct3d12/multi-engine)
- [Memory Management Strategies](https://learn.microsoft.com/en-us/windows/win32/direct3d12/memory-management-strategies)
- [D3D12 Render Passes](https://learn.microsoft.com/en-us/windows/win32/direct3d12/direct3d-12-render-passes)

## 85. 次章への接続

次章はDirectX 12総合3D戦闘描画です。これまでのWindow、Queue、Resource、Descriptor、PSO、Model、Animation、Compute、Memory、Frame Graph、診断を一Frameへ統合します。
