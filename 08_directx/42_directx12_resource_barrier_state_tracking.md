# DirectX 12 第10章：Resource Barrier・State Tracking

この章では、GPU ResourceのAccess順序と用途を明示管理します。Transition/UAV/Aliasing Barrier、Read/Write State、Subresource、Promotion/Decay、Split Barrier、Command List/Queue間State統合、Enhanced Barriers、Frame Graph連携を扱います。

## 1. Resource Stateとは

Resource/Subresourceが現在どの種類のGPU Accessに適した状態かを表します。

## 2. Barrierの三目的

```text
Execution ordering : 前Accessが必要地点まで完了する
Memory visibility  : 書込み結果を後続Accessから見えるようにする
Layout/usage       : 次のPipeline用途へResourceを準備する
```

## 3. D3D11との違い

D3D11 Runtimeが暗黙に追跡したHazardを、D3D12ではApplicationが明示します。

## 4. Legacy Barrier三種類

```text
TRANSITION : State変更
UAV        : UAV Access間の順序
ALIASING   : 同じHeap Memoryを別Resourceとして使う境界
```

## 5. Transition Barrier

ResourceをState BeforeからState Afterへ遷移させます。

## 6. Barrier構造体

```cpp
D3D12_RESOURCE_BARRIER barrier{};
barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
barrier.Transition.pResource = resource;
barrier.Transition.StateBefore = before;
barrier.Transition.StateAfter = after;
barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
```

## 7. ResourceBarrier呼出し

```cpp
commandList->ResourceBarrier(1, &barrier);
```

Command Streamのその位置へ同期命令を記録します。

## 8. Helper

```cpp
D3D12_RESOURCE_BARRIER TransitionBarrier(
    ID3D12Resource* resource,
    D3D12_RESOURCE_STATES before,
    D3D12_RESOURCE_STATES after,
    UINT subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES)
{
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition = { resource, subresource, before, after };
    return barrier;
}
```

## 9. State Beforeの正確性

実際のStateと宣言したBeforeが違えば未定義結果やDebug Errorになります。Code近辺から推測せずTrackerを使います。

## 10. State After

Transition後に行う最初のAccessと互換なStateを指定します。

## 11. COMMON

特定用途に固定されていない共通Stateです。Queue間移行やImplicit Promotion/Decayに関係します。

## 12. PRESENT

Swap Chain Back BufferをPresent可能にするStateです。値はCOMMONと同じ扱いを持つ部分がありますが、意味を区別して記録します。

## 13. RENDER_TARGET

RTVとして書き込むStateです。通常PS Shader Resource読取りと同時には使いません。

## 14. DEPTH_WRITE

Depth/Stencilへ書込むStateです。

## 15. DEPTH_READ

DepthをRead-onlyで使用するStateです。Shader Resource読取りStateとの組合せが必要な場合があります。

## 16. PIXEL_SHADER_RESOURCE

Pixel ShaderがSRVとして読むStateです。

## 17. NON_PIXEL_SHADER_RESOURCE

Vertex/Compute等、Pixel以外のShader StageがSRVとして読むStateです。

## 18. GENERIC_READ

複数の読取りStateを組み合わせたConvenience Stateです。Upload Heap Bufferで一般的です。

## 19. UNORDERED_ACCESS

UAVとして読み書きするStateです。連続UAV書込みではStateが同じでもUAV Barrierが必要になることがあります。

## 20. COPY_SOURCE

Copy元として読むStateです。

## 21. COPY_DEST

Copy先として書くStateです。

## 22. RESOLVE_SOURCE/DEST

MSAA Resolveの元/先に使います。

## 23. VERTEX_AND_CONSTANT_BUFFER

Vertex BufferまたはConstant BufferとしてGPUが読むStateです。

## 24. INDEX_BUFFER

Index Bufferとして読むStateです。

## 25. INDIRECT_ARGUMENT

ExecuteIndirectのArgument/Count Bufferとして読むStateです。

## 26. PREDICATION

Predication Bufferとして読む用途です。一部State値のAliasと意味を区別します。

## 27. RAYTRACING_ACCELERATION_STRUCTURE

Acceleration Structure用途の特殊Stateです。DXR対応章で詳しく扱います。

## 28. SHADING_RATE_SOURCE

Variable Rate Shading Image用途のStateです。Feature Supportを確認します。

## 29. Read Stateの組合せ

複数のRead用途が同時に必要なら互換なRead State BitをORできます。

```cpp
const auto shaderRead =
    D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE |
    D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
```

## 30. Write Stateは排他的

Render Target、Depth Write、Copy Dest、UAV等の書込み用途を無秩序に組み合わせません。仕様の互換性を守ります。

## 31. ReadからRead

現在Stateが必要Read Bitを既に含むならTransition不要な場合があります。

## 32. ReadからWrite

通常明示Transitionが必要です。前Read完了と次Write用途を保証します。

## 33. WriteからRead

書込み結果を後続Shader/Copyが読むためTransitionを記録します。

## 34. WriteからWrite

用途が変わるならTransition、同じUAV用途内の依存ならUAV Barrier等を判断します。

## 35. 同じBefore/After

不要Transitionを記録しません。Debug Assertで呼出し元のState理解不足を検出する方針もあります。

## 36. Barrier Batch

```cpp
std::array<D3D12_RESOURCE_BARRIER, 3> barriers = { a, b, c };
commandList->ResourceBarrier(
    static_cast<UINT>(barriers.size()),
    barriers.data());
```

Pass境界のBarrierをまとめます。

## 37. Barrier Cost

BarrierはCache Flush、Decompress、Pipeline Stall、Layout変換等を伴い得ます。正しさを保った上で回数/範囲を最適化します。

## 38. Over-barrier

すべてをCOMMONへ戻す、毎Draw全ResourceをTransitionする等は不要な同期を増やします。

## 39. Under-barrier

必要Barrierを省略するとRace、古いData、画面破損、Device Removedにつながります。

## 40. Subresource

TextureのMip、Array Slice、Plane等の個別部分です。

## 41. Subresource Index

Mip、Array、Planeから線形Indexを計算します。`D3D12CalcSubresource` Helperを使えます。

## 42. ALL_SUBRESOURCES

Resource全体が同じBefore Stateである場合に全Subresourceを一括Transitionします。

## 43. 全体Stateが不均一な場合

MipごとにStateが違うのにALLで単一Beforeを宣言しません。個別Barrierを生成します。

## 44. Mip生成例

Mip 0をSRVで読みMip 1をUAVへ書く等、Mipごとに異なるStateを持ちます。

## 45. Array Slice例

Shadow Cascade/Texture Array SliceごとにRender Target/Shader Read状態を切り替えられます。

## 46. Plane例

Depth/StencilやMulti-plane FormatではPlane Subresourceを考慮します。

## 47. State Storage最適化

全Subresource同一なら一つのState、分岐したときだけ配列へ展開する設計があります。

## 48. UAV Barrier

```cpp
D3D12_RESOURCE_BARRIER barrier{};
barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
barrier.UAV.pResource = resource;

commandList->ResourceBarrier(1, &barrier);
```

## 49. UAV Stateのままでも必要

Dispatch Aが書いたUAV DataをDispatch Bが依存して読む/書く場合、State Transitionなしでも順序保証が必要です。

## 50. UAV Barrier Resource nullptr

全UAV Accessへ作用するGlobal UAV Barrierとして使える形があります。必要範囲が一Resourceなら具体Pointerを使います。

## 51. UAV Barrier不要な場合

前後Accessが同じMemoryへ依存しない、別Resource、明示Transitionが必要同期を含む等、仕様に基づいて判断します。

## 52. Atomicだけでは十分でない

Shader Atomicは対象Operationの原子性を提供しますが、Dispatch/Pass間の全Memory可視性と実行順を自動で保証するものではありません。

## 53. Aliasing Barrier

同一Heap Memoryの使用をBefore ResourceからAfter Resourceへ切り替えることを示します。

```cpp
D3D12_RESOURCE_BARRIER barrier{};
barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_ALIASING;
barrier.Aliasing.pResourceBefore = beforeResource;
barrier.Aliasing.pResourceAfter = afterResource;
```

## 54. nullptr Aliasing Resource

Before/Afterへ`nullptr`を使えるケースがありますが、最適化範囲が広くなり得ます。具体Resourceを追跡できるなら指定します。

## 55. Aliasing Lifetime

Before Resourceの最終GPU使用完了と、After Resource使用開始順序を保証します。CPU Object寿命だけでは不足です。

## 56. Initial StateとAliasing

Placed Resourceを同Heap Rangeへ作る際、Stateと初回使用Barrierの規則を確認します。

## 57. Split Barrier

TransitionをBEGIN_ONLYとEND_ONLYへ分け、間に独立Workを置く最適化です。

## 58. BEGIN_ONLY

```cpp
barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_BEGIN_ONLY;
```

Transition開始を示します。

## 59. END_ONLY

対応する同Resource/Subresource/Before/After TransitionをEND_ONLYで完了します。

## 60. Split中のAccess

BEGINとENDの間で対象Subresourceへ通常Accessしません。対応PairとCommand順を厳密に管理します。

## 61. Split Barrierの目的

高価なTransition処理と独立GPU WorkをOverlapできる可能性があります。正しい通常Barrierの後でProfileします。

## 62. Split Pair管理

開始したままFrame/Command Listを跨いで忘れないようTrackerがPending Transitionを持ちます。

## 63. Implicit Promotion

COMMONから一定の条件を満たす最初のAccess Stateへ、明示BarrierなしでPromotionできる規則があります。

## 64. Promotion対象

Buffer、Simultaneous-access Texture、特定Read/Copy用途等で規則が異なります。Resource Type/Queue/Stateの公式表を確認します。

## 65. Read-only Promotion

COMMONから互換なRead StateへImplicit Promotionし、追加Read Bitを組み合わせられる場合があります。

## 66. Write Promotion

許可される単一Write Stateへの最初のAccess等、条件付きです。すべてのTexture/Writeへ適用されると仮定しません。

## 67. Promotionに依存する設計

Barrier削減になりますが、State TrackerがImplicit Stateを理解しないと後続Beforeを誤ります。

## 68. Decay

特定のImplicitly Promoted Resource/Stateは`ExecuteCommandLists`完了境界でCOMMONへDecayする規則があります。

## 69. Decay条件

Buffer、Simultaneous-access Texture、Copy Queue、Read-only Promotion等で条件があります。無条件に全ResourceがCOMMONへ戻るわけではありません。

## 70. Command List Closeではない

Decayの境界をCommand Listの`Close`と誤解しません。Queue Execute単位の規則を確認します。

## 71. Promotion/Decayを使わない初期実装

最初は明示Transitionで正しさを確立し、Profiler/Trackerが整ってからImplicit規則を利用できます。

## 72. Copy Queue State制約

Copy Queueで使用可能なState/Commandに制約があります。Direct Queueと同じState集合を使えると仮定しません。

## 73. Queue間引渡し

Producer QueueがFence Signal、Consumer QueueがWaitし、Resource State/COMMON規則を満たして使用します。

## 74. Queue Ownershipの考え方

D3D12に他APIと同一の明示Queue Family Ownershipはありませんが、Access順とState、FenceをApplicationが管理します。

## 75. Direct→Compute

Graphics出力をComputeが読むなら、必要Transitionを適切なQueue/Listへ記録し、Fence Waitで順序を保証します。

## 76. Copy→Direct

Upload Copy完了Signal後、Direct QueueがWaitし、Copy DestからShader/Vertex等へTransitionします。

## 77. State Trackerの目的

Resource/Subresourceの既知Stateを保存し、必要Barrierだけ生成し、Before不一致を防ぎます。

## 78. Resource State Record

```cpp
struct ResourceState
{
    D3D12_RESOURCE_STATES allState = D3D12_RESOURCE_STATE_COMMON;
    std::vector<D3D12_RESOURCE_STATES> subresourceStates;
};
```

## 79. Global State

QueueへSubmit済みCommandの順序を反映したResourceの確定Stateです。

## 80. Local Command List State

一つのCommand List内で記録したTransition後のStateを追跡します。並列記録中はGlobalへ即書込みません。

## 81. Pending Barrier

Command List開始時のGlobal Stateが記録時点で未確定なら、First Use TransitionをPendingとしてSubmit時に解決します。

## 82. Parallel Recording問題

List A/Bが同じGlobal Stateを読み、それぞれ別Transitionを記録するとSubmit順でBeforeが変わります。

## 83. Submit時Resolve

Compiled Orderで各ListのPending First Transitionを直前Global Stateから生成し、補助Barrier Listへ記録する方式があります。

## 84. Final State

各Command Listが終了時にResource/Subresourceが何StateかをResult Metadataとして返します。

## 85. Global State更新

Submit Orderに従いFinal StateをGlobal Mapへ反映します。Worker完了順ではありません。

## 86. Tracker Lock

Global MapはSubmission Threadへ集約するとLockを減らせます。Local TrackerはCommand Context専有です。

## 87. Resource Generation

Pointer再利用で別ResourceのStateを誤参照しないよう、世代付きResource IDをKeyにします。

## 88. Destroy時

Resource RegistryからState Recordを削除し、Deferred Release完了までID再利用を防ぎます。

## 89. Import State

Swap Chain Back BufferはPRESENT、UploadはGENERIC_READ、ReadbackはCOPY_DEST等、外部/作成時StateをTrackerへ登録します。

## 90. State Validation

Passが宣言したRead/Write用途から必要Stateを求め、実際のBinding/Copy Commandと照合します。

## 91. Frame Graph連携

```text
Pass A writes SceneColor as RTV
Pass B reads SceneColor as SRV
```

Graph EdgeからRTV→SRV TransitionをPass境界へ生成します。

## 92. Access Declaration

Pass SetupでTexture/Buffer、Subresource Range、Read/Write、Stage、用途を宣言します。

## 93. Barrier Compiler

Logical Resource Version、Pass Order、Queue、Accessを解析しBarrier BatchとQueue Fenceを生成します。

## 94. Pass Merging

連続Passが同じ互換Stateを使うならTransitionを省略できます。副作用/Timing Markerを保ちます。

## 95. Barrier Hoisting

Transitionをより早い安全地点へ移しOverlapを狙えます。Lifetime/依存を壊さずProfileします。

## 96. Barrier Visualization

Graph UIへBefore/After、Resource、Subresource、Pass、Queue、Reasonを表示します。

## 97. Barrier Statistics

- Type別数
- Pass別数
- ALL/個別Subresource数
- Split数
- Queue間同期数
- Redundant候補
- Transition Cost/Timing

## 98. State Debug Log

```text
Frame 120 Pass Bloom
SceneColor mip0: RENDER_TARGET -> PIXEL_SHADER_RESOURCE
Reason: Bloom input read
```

## 99. Debug Layer

State Before不一致、CommandとState不整合等を検出します。GPU-based Validationも利用します。

## 100. PIX

Resource History、Barrier Event、State、Pass間Accessを確認し、過剰/不足Barrierを分析します。

## 101. Enhanced Barriers

新しいBarrier ModelではSynchronization、Access、Texture Layoutを分離してより細かく指定します。

## 102. Feature Support

`D3D12_FEATURE_D3D12_OPTIONS12`等からEnhanced Barriers Supportを確認し、非対応ならLegacy Pathを使います。

## 103. Command List Interface

対応`ID3D12GraphicsCommandList7`等の`Barrier` APIを使用します。Interface取得失敗を考慮します。

## 104. Barrier Group

Global、Buffer、Texture BarrierをTypeごとのGroupとしてCommandへ渡します。

## 105. Global Barrier

Resourceを特定せず、Sync/Access Scope間のMemory依存を表します。必要範囲を過剰に広げません。

## 106. Buffer Barrier

Buffer Resource、Offset/Size、Sync Before/After、Access Before/Afterを指定します。

## 107. Texture Barrier

Texture、Subresource Range、Sync、Access、Layout Before/After、Flagsを指定します。

## 108. Sync Scope

Draw、Compute、Copy、Render Target、Depth、All等、実行Stage Scopeを表します。

## 109. Access Scope

SRV、UAV、RTV、Depth、Copy Source/Dest等のMemory Access種類を表します。

## 110. Texture Layout

Render Target、Shader Resource、Copy、Present等のTexture Layoutを明示します。Legacy Stateとの対応表を設計します。

## 111. Subresource Range

Mip、Array、Planeの範囲を構造的に指定できます。個別Barrier大量生成を減らせる場合があります。

## 112. Enhanced Split

Texture Barrier FlagでBegin/Endを表す仕組みがあります。PairとAccess禁止期間を管理します。

## 113. Legacyとの混在

同じCommand List/Resourceでの混在規則を公式仕様で確認し、Renderer Policyとして一方式へ統一します。

## 114. Translation Layer

Engine Access EnumからLegacy StateまたはEnhanced Sync/Access/Layoutへ変換するBackendを作れます。

## 115. Enhancedが必ず高速ではない

より正確なScopeを表せますが、Hardware/Driver/Workloadで効果が変わります。Captureで測ります。

## 116. Unit Test

State組合せ、Subresource展開/圧縮、Pending Resolve、Submit順、Promotion/Decay判定、Enhanced変換をTestします。

## 117. Integration Test

RTV→SRV、Copy→Vertex、Compute UAV→Graphics SRV、Mip別UAV、Aliasingを実行しDebug Layer Message 0件を確認します。

## 118. Stress Test

並列Command List、複数Queue、Pass Reorder、Resource再生成、Frame Graph Culling、Resizeを組み合わせます。

## 119. よくある失敗：現在Stateを一つだけ保持

Mipごとに違うStateを全体一Stateとして上書きします。Uniform/Per-subresource表現を切り替えます。

## 120. よくある失敗：BindingがStateを変えると思う

SRV Descriptor Tableを設定すればTextureが読取りStateになると誤解します。Barrierは別Commandです。

## 121. よくある失敗：UAV State同じだからBarrierなし

前Dispatchの書込みへ後Dispatchが依存します。UAV Barrier/適切な同期を入れます。

## 122. よくある失敗：全てCOMMON経由

用途変更ごとにCOMMONへ戻し不要Barrierを増やします。直接Before→AfterへTransitionします。

## 123. よくある失敗：Promotion/Decayを無条件適用

対象Resource/Queue/State条件を確認せずBarrierを省きます。条件をTrackerで明示判定します。

## 124. よくある失敗：並列ListがGlobal State更新

記録完了順でGlobal Mapを書換えSubmit順と矛盾します。Local Final StateをSubmit Managerが反映します。

## 125. よくある失敗：Aliasing Barrierなし

同Heap Rangeを別Placed Resourceとして使い、前ResourceのCache/Accessと競合します。

## 126. 実装Checklist

- [ ] Transition/UAV/Aliasingの目的を区別する。
- [ ] Read/Write Stateの互換性を理解する。
- [ ] State BeforeをTrackerから取得する。
- [ ] SubresourceごとのStateを表現できる。
- [ ] UAV依存にBarrierを挿入する。
- [ ] Placed AliasingにBarrierを挿入する。
- [ ] Split PairとPending期間を追跡する。
- [ ] Promotion/Decay条件を明示判定する。
- [ ] Command List Local/Global Stateを分離する。
- [ ] Submit OrderでPending/Final StateをResolveする。
- [ ] Frame Graph AccessからBarrierを生成する。
- [ ] Enhanced SupportとLegacy Fallbackを持つ。

## 127. 理解確認問題

1. BarrierのExecution/Memory/Layout三目的を説明してください。
2. Read StateをORできる理由とWrite Stateの制約を説明してください。
3. Stateが同じUAV間でもBarrierが必要な例を説明してください。
4. ALL_SUBRESOURCESを使えない状況を説明してください。
5. Promotion/Decayを無条件利用できない理由を説明してください。
6. 並列Command ListのGlobal State問題を説明してください。
7. Aliasing BarrierとFence Lifetimeの両方が必要な理由を説明してください。
8. Enhanced BarrierのSync/Access/Layout分離を説明してください。

## 128. 章末要点

- Barrierは実行順、Memory可視性、Resource用途/LayoutをGPUへ伝えます。
- Transition、UAV、Aliasingを依存の種類に応じて使い分けます。
- StateをResource/Subresource単位で正確に追跡します。
- Promotion/Decay/Splitは条件を理解してから最適化として使います。
- 並列記録ではLocal Stateを持ち、Submit順にGlobal StateをResolveします。
- Frame GraphのRead/Write/Queue宣言からBarrierとFenceを生成できます。
- Enhanced BarriersはSync/Access/Layoutを分離し、Legacy Fallbackと計測を用意します。

## 129. 公式資料

- [Using resource barriers to synchronize resource states](https://learn.microsoft.com/en-us/windows/win32/direct3d12/using-resource-barriers-to-synchronize-resource-states-in-direct3d-12)
- [Resource barriers](https://learn.microsoft.com/en-us/windows/win32/direct3d12/resource-barriers)
- [Common state promotion and decay](https://learn.microsoft.com/en-us/windows/win32/direct3d12/using-resource-barriers-to-synchronize-resource-states-in-direct3d-12#common-state-promotion)
- [D3D12_RESOURCE_STATES](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/ne-d3d12-d3d12_resource_states)
- [ID3D12GraphicsCommandList::ResourceBarrier](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12graphicscommandlist-resourcebarrier)
- [D3D12_RESOURCE_TRANSITION_BARRIER](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/ns-d3d12-d3d12_resource_transition_barrier)
- [D3D12_RESOURCE_UAV_BARRIER](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/ns-d3d12-d3d12_resource_uav_barrier)
- [D3D12_RESOURCE_ALIASING_BARRIER](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/ns-d3d12-d3d12_resource_aliasing_barrier)
- [Enhanced barriers](https://learn.microsoft.com/en-us/windows/win32/direct3d12/enhanced-barriers)
- [ID3D12GraphicsCommandList7::Barrier](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12graphicscommandlist7-barrier)

次章では、Texture Subresource、Row Pitch、GetCopyableFootprints、CopyTextureRegion、Mip、SRV、WIC/DDS Uploadを扱います。
