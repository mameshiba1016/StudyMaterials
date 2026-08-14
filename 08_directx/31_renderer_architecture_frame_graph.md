# DirectX 11：Renderer Architecture・Frame Graph

この章では、個別の描画技術を保守可能なRendererへ統合します。Layer分離、Render Snapshot、Handle、Pass、Resource Lifetime、Frame Graphの構築・Compile・Execute、Transient Resource再利用、Debug/Profiler統合までを扱います。

## 1. Architectureの目的

良いRenderer Architectureは機能を増やすだけでなく、変更範囲、依存関係、Resource寿命、実行順序、性能Costを理解可能にします。

## 2. 巨大Render関数の問題

```cpp
void Game::Render()
{
    // Shadow、Character、Effect、Post Process、UIが全部ここにある。
}
```

短期的には動いても、State依存、順序依存、Resize、Device Lost、Testが絡み合います。

## 3. 責務のLayer

```text
Gameplay / Scene
      ↓ Render Snapshot
Render World / View
      ↓ Visible Render Items
Render Pipeline / Frame Graph
      ↓ Compiled Passes
D3D11 Backend
      ↓ API Commands
GPU
```

上位Layerが低水準COM Objectを直接所有しないようにします。

## 4. FrontendとBackend

```text
Frontend : 何を描くか、View、Material、Pass要求を作る
Backend  : D3D11 Resource、Binding、Draw/Dispatchを実行する
```

API固有処理をBackendへ集約すると設計を検証しやすくなります。

## 5. Renderer Interface

```cpp
class IRenderer
{
public:
    virtual ~IRenderer() = default;
    virtual void Resize(uint32_t width, uint32_t height) = 0;
    virtual void Render(const RenderSnapshot& snapshot) = 0;
};
```

Interfaceを増やしすぎず、Frame単位の入力とLifecycleを明確にします。

## 6. Render Snapshot

Gameplayの可変Objectを直接読む代わりに、そのFrameの描画に必要な不変Dataを渡します。

```cpp
struct RenderSnapshot
{
    uint64_t frameId;
    std::span<const RenderProxy> objects;
    std::span<const LightProxy> lights;
    CameraSnapshot camera;
    EnvironmentSnapshot environment;
};
```

## 7. Render Proxy

```cpp
struct RenderProxy
{
    Matrix world;
    Matrix previousWorld;
    Bounds bounds;
    MeshHandle mesh;
    MaterialHandle material;
    uint32_t layerMask;
    uint32_t flags;
};
```

Rendererが必要とする値だけを保持します。

## 8. Previous Frame Data

Motion Vector、Temporal AA、Interpolationには前Frame Transformが必要です。Gameplay履歴へ後から依存せずSnapshot契約へ含めます。

## 9. Stable Handle

```cpp
template<class Tag>
struct Handle
{
    uint32_t index;
    uint32_t generation;
};
```

生のCOM Pointerを上位へ渡さず、Registryが現在世代のResourceを解決します。

## 10. Resource Registry

Mesh、Texture、Material、ShaderをHandleから検索します。生成中、Ready、Failed、Evicted等の状態を持たせます。

## 11. Fallback Resource

未読込Texture、失敗Shader、無効Meshを安全なDefault Resourceへ置換し、Frame全体を壊さず診断表示します。

## 12. Materialの責務

MaterialはShader Variant、Texture、数値Parameter、Render State分類を表します。Draw順序やView Resourceを勝手に生成しません。

## 13. Material Instance

共有Material定義とObject固有Parameterを分けます。同じPipelineを使うObjectをBatchしやすくなります。

## 14. Render View

Main Camera、Shadow Cascade、Reflection、Minimap等を共通のView表現へします。

```cpp
struct RenderView
{
    Matrix view;
    Matrix projection;
    Frustum frustum;
    Rect viewport;
    uint32_t layerMask;
    ViewType type;
};
```

## 15. ViewごとのVisible List

Main ViewのCulling結果をShadowへ無条件に流用しません。ViewごとにVisibility条件が異なります。

## 16. Render Item

```cpp
struct RenderItem
{
    MeshHandle mesh;
    MaterialHandle material;
    uint32_t submesh;
    uint32_t objectIndex;
    uint64_t sortKey;
    float viewDepth;
};
```

Scene ObjectからGPU Drawへ変換する中間表現です。

## 17. Render Queue

Opaque、Alpha Test、Transparent、Shadow、Effect、UI等のQueueへ分類します。分類RuleはMaterial/Pass側に集約します。

## 18. Sort Key

```text
Opaque      : pass | pipeline | material | mesh | depth bucket
Transparent : pass | depth back-to-front | material
Shadow      : cascade | pipeline | mesh | material
```

Bit Layoutを文書化し、OverflowとStable OrderをTestします。

## 19. Passとは

Passは入力Resourceを読み、出力Resourceへ書き、Draw/Dispatch/Copyを行う処理単位です。

## 20. Pass Contract

```text
Name    : BloomDownsample
Reads   : SceneColor
Writes  : BloomHalf
Execute : Fullscreen/Compute処理
```

暗黙Global Resourceを減らします。

## 21. Frame Graphとは

一Frame内のPassとResource依存をGraphとして宣言し、正しい実行順序、Resource Lifetime、不要Pass除去、Transient再利用を導く仕組みです。

## 22. GraphのNodeとEdge

```text
Node : Render/Compute/Copy Pass
Edge : ResourceのWrite -> Read依存
```

Resource自体をNodeとして表すBipartite Graph設計もあります。

## 23. 最小Graph例

```text
Shadow ───────────────┐
Depth Prepass ────────┤
Opaque ─> SceneColor ─┼─> PostProcess ─> UI ─> BackBuffer
Transparent ──────────┘
```

実際には各PassのRead/Write宣言からEdgeを生成します。

## 24. BuilderとExecutor

```cpp
graph.AddPass<OpaqueData>(
    "Opaque",
    [&](FrameGraphBuilder& builder, OpaqueData& data)
    {
        data.depth = builder.Read(depth);
        data.color = builder.Write(sceneColor);
    },
    [&](const OpaqueData& data, RenderContext& context)
    {
        DrawOpaque(data, context);
    });
```

Setupで依存を宣言し、ExecuteでAPI Commandを発行します。

## 25. Resource Handle

```cpp
struct FGTextureHandle
{
    uint32_t id = invalidId;
};
```

Frame Graph内では実Texture Pointerではなく論理Handleを渡します。

## 26. Resource Descriptor

```cpp
struct FGTextureDesc
{
    uint32_t width;
    uint32_t height;
    DXGI_FORMAT format;
    uint32_t mipLevels;
    uint32_t arraySize;
    uint32_t bindFlags;
    uint32_t sampleCount;
};
```

物理Resource生成に必要な属性を完全に含めます。

## 27. Logical ResourceとPhysical Resource

`SceneColor`という論理Resourceと、実際の`ID3D11Texture2D/RTV/SRV`を分離します。Compile後に物理Resourceを割り当てます。

## 28. Import Resource

Swap Chain Back Buffer、永続Shadow Atlas、History Texture等、Graph外で所有するResourceをImportします。

## 29. Create Transient Resource

Frame内だけ必要な中間TextureはGraphで作成宣言し、最初の使用前に確保し最後の使用後に再利用可能にします。

## 30. Read宣言

PassがSRV等で読むResourceを宣言します。読んでいないResourceへExecute Callbackから勝手にAccessさせません。

## 31. Write宣言

RTV、DSV、UAV、Copy Destination等として書くResourceを宣言します。Writeは新Versionを生成する設計もあります。

## 32. Read-Write

同じ論理Dataを読み書きする場合も、実APIのSRV/UAV同時Binding禁止やPing-Pongを考慮します。単なる`ReadWrite`宣言でHazardが消えるわけではありません。

## 33. Resource Versioning

```text
SceneColor v0 -> Opaque writes -> v1
v1 -> Transparent writes -> v2
v2 -> ToneMap reads
```

どのWrite結果を読むか明確になります。

## 34. Dependency生成

Pass BがAの出力を読むならA→B Edgeを作ります。Write-after-writeやRead-before-writeも検証します。

## 35. Topological Sort

依存Edgeを壊さない順序へPassを並べます。循環がある場合はCompile Errorとして報告します。

## 36. Cycle検出

```text
Pass A reads Y, writes X
Pass B reads X, writes Y
```

同一Frameで循環しているならHistory Resourceとして前Frame入力を分離する等、設計を直します。

## 37. Pass Culling

最終出力へ到達しないPassは実行不要です。Back Bufferや外部副作用Passから逆向きに必要PassをMarkします。

## 38. Side Effect

Readback、Timestamp、外部Output、Debug Capture等、Resource Edgeだけでは必要性を表せないPassには明示的なSide Effect Flagを持たせます。

## 39. Pass Cullingの注意

Global Counter更新や隠れたState変更をExecuteへ埋め込むと誤って除去されます。副作用を宣言するかArchitectureから除きます。

## 40. Lifetime解析

各Transient ResourceのFirst UseとLast UseをCompile時に求めます。

```text
Depth      : pass 0 ───────── pass 5
BloomHalf  :          pass 4 ─ pass 6
TempBlur   :             pass 5 ─ pass 6
```

## 41. Resource Pool

Descriptorが一致するTexture/BufferをPoolから再利用します。毎FrameCreate/Releaseしません。

## 42. Aliasingの考え方

Lifetimeが重ならない論理Resourceへ同じ物理Memoryを割り当てる最適化です。Direct3D 11では明示Heap Aliasingが限定的なため、同一Descriptor ResourceのPool再利用として実装しやすいです。

## 43. Direct3D 12との違い

Direct3D 12ではResource Barrier、Heap、Queue同期をFrame Graph Compilerがより明示的に生成できます。D3D11版は依存、Binding解除、Pool、診断の基礎になります。

## 44. Resource Key

```cpp
struct TexturePoolKey
{
    uint32_t width;
    uint32_t height;
    DXGI_FORMAT format;
    uint32_t bindFlags;
    uint32_t mipLevels;
    uint32_t sampleCount;
};
```

互換でないResourceを同じPool Entryとして使いません。

## 45. Relative Size

`Full`、`Half`、`Quarter`、`RenderScale`等をDescriptorへ表現し、Resize時に実Sizeへ解決します。

## 46. Size Rounding

奇数解像度のHalf/Quarter Sizeに切り上げ・切り捨てのどちらを使うか統一します。Compute Group SizeやMip生成と一致させます。

## 47. Clear Policy

Resource作成時に自動Clearすると正しいが遅い場合があります。Passが全Pixelを書き潰すならDiscard相当、部分書込みならClearが必要です。

## 48. Load/Storeの意図

D3D11 APIにD3D12/Vulkanの明示Load/Store Actionがなくても、Pass Contractとして`Clear/Load/DontCare`を表現すると移植と検証に役立ちます。

## 49. View Cache

Texture本体だけでなくFormat、Mip、Array SliceごとのRTV/SRV/UAV/DSVをCacheします。KeyにSubresource範囲を含めます。

## 50. Binding Tracker

D3D11 Backendで各StageのSRV/UAV/RTV Bindingを追跡し、同一SubresourceのRead/Write Hazard前に必要SlotをUnbindします。

## 51. State Cache

現在Bind中のShader、Buffer、View、Sampler、Stateを追跡し、同じAPI設定を省略できます。ただしContext Stateを外部から変更させないことが前提です。

## 52. Cache無効化

Command List実行、`ClearState`、外部Library描画等で実Stateが変わるならCacheをInvalidateします。Cacheと実Contextの不一致は危険です。

## 53. Render Context

```cpp
class RenderContext
{
public:
    ID3D11DeviceContext* NativeContext();
    TextureView Resolve(FGTextureHandle handle);
    void BindPipeline(PipelineHandle pipeline);
    void Draw(const DrawPacket& packet);
};
```

PassへDevice全体やRegistryの無制限Accessを渡しません。

## 54. Pipeline State Object風の表現

D3D11ではShaderと各Stateを個別Bindしますが、Engine側では一つのPipeline Descriptor/Handleへまとめられます。

## 55. Draw Packet

Mesh、Submesh、Pipeline、Material Parameter、Object Constantを実行可能なPacketへまとめます。生Scene Pointerを含めません。

## 56. Descriptor Binding Table

D3D11にDescriptor Heapはなくても、Engine側でSlot LayoutをReflectionから定義し、Material Bindingの一貫性を検証できます。

## 57. Shader Reflection

Constant Buffer、SRV、Sampler、UAVの名前・Slot・Sizeを抽出し、Material Layoutと照合します。Releaseでは生成済みMetadataを使えます。

## 58. Pass Parameter構造体

各Passの論理Resource Handleと設定を専用構造体へ持たせ、文字列検索やGlobal変数を避けます。

## 59. Blackboard

複数Pass間でよく使うHandleを型付きBlackboardへ登録する設計があります。

```cpp
struct MainViewResources
{
    FGTextureHandle depth;
    FGTextureHandle sceneColor;
    FGTextureHandle motionVectors;
};
```

無秩序なString Mapにしません。

## 60. Graph構築の例

```text
Import BackBuffer
Create MainDepth, SceneColor, MotionVectors
Add Shadow Pass
Add Depth Pass
Add Opaque Pass
Add Transparent Pass
Add Bloom Passes
Add ToneMap Pass
Add UI Pass
Present BackBuffer
```

Feature設定に応じてPassを追加・省略します。

## 61. Feature Module

Shadow、Bloom、SSR、Effect等が自身のPassとResourceをGraphへ登録します。中央Rendererが全Feature詳細を知る必要を減らします。

## 62. Feature依存

Module同士を直接呼び合わず、共有Resource Contractを介します。循環依存をCompile時に検出します。

## 63. Optional Feature

Bloom無効時はPassを追加せず、ToneMapはScene Colorを直接読みます。Dummy Passを大量に残しません。

## 64. Quality設定

Shadow Cascade数、AO解像度、Bloom Level等をGraph構築Parameterへし、Resource DescriptorとPass数へ反映します。

## 65. Graph Cache

同じResolution/Feature構成ならCompiled Graphを再利用できます。CameraやObject数等のFrame DataとGraph構造を分けます。

## 66. Recompile条件

Resize、Render Scale、MSAA、HDR Format、Feature Toggle、Pass構成変更時にRecompileします。毎Frame不要なら避けます。

## 67. Dynamic Pass数

Shadow Light数等でPass数が変わる場合、構築CostとCache Strategyを測ります。最大固定Graphと動的GraphのTrade-offがあります。

## 68. History Resource

TAA、Exposure、Occlusion履歴はFrameを跨ぐためTransientではありません。

```text
History Prev -> Current Pass -> History Next
Frame EndでPrev/Next交換
```

## 69. History無効化

Resize、Camera Cut、Teleport、FOV急変、Device Lost時は履歴をClear/Resetします。古いDataをBlendしません。

## 70. Multi-view

Main、Split Screen、Reflection、MinimapへView IDを持たせます。View固有Resourceと共有Shadow等を区別します。

## 71. Shadow Graph

Cascade/LightごとのShadow PassがAtlas領域へ書き、Main LightingがAtlasを読みます。Atlas Slice/ViewportもResource Access情報へ含めます。

## 72. Compute Pass

Compute ShaderもGraph Nodeです。SRV Read、UAV Write、Dispatch寸法を宣言し、後続Drawとの依存を生成します。

## 73. Copy/Readback Pass

Screenshot、Picking、Profiler ReadbackをCopy Passとして明示します。CPU取得は数Frame遅延し、Frame Graph Resource寿命を保証します。

## 74. Async Computeについて

D3D11 Deferred Contextは独立GPU Compute Queueではありません。Graph上でCompute Passを分けても自動的にGPU並列実行されない点を理解します。

## 75. Multithread Recording

依存上同じLevelにあるPass/ChunkはDeferred Contextへ並列記録できます。ただし最終Execute順とResource LifetimeはCompiled Graphに従います。

## 76. Frame Allocator

Graph Node、Edge、Pass Data、Render Item等の一Frame一時DataをLinear Allocatorへ置き、Frame終了時にまとめてResetします。

## 77. Lambda CaptureのLifetime

Execute LambdaがStack参照や一時ContainerをCaptureし、実行時にDanglingしないようにします。Pass DataはGraph所有MemoryへCopyします。

## 78. Compile Error

- 未初期化ResourceのRead
- 同一Versionへの複数Write
- 循環依存
- 無効Handle
- View Format不一致
- Bind Flag不足
- Size 0
- 未使用Output

人が直せるPass/Resource名付きMessageを出します。

## 79. Graph Visualization

Node、Edge、Resource Lifetime、Physical AllocationをDOT/JSON等へ出力し、Capture Frameと対応させます。

## 80. Debug UI

Pass有効/無効、Resource Preview、Format/Size、First/Last Use、GPU時間、Physical Pool IDを一覧表示します。

## 81. Pass Marker自動化

Frame Graph ExecutorがPass名で`BeginEvent/EndEvent`を自動発行します。各FeatureがMarkerを書き忘れません。

## 82. GPU Timestamp自動化

Pass境界へQueryを自動挿入し、Debug UIへGPU時間を表示します。Query数上限と数Frame遅延を管理します。

## 83. Statistics

PassごとのDraw、Dispatch、Triangle、Instance、Resource Read/Write、Transient Byteを集計します。

## 84. Unit Test：Graph

GPUを使わず、Pass追加から期待するEdge、Topological Order、Culling、Cycle Error、LifetimeをTestできます。

## 85. Unit Test：Resource Pool

Descriptor一致・不一致、Lifetime重複、Resize、Peak数、世代更新をTestします。

## 86. Integration Test

固定Sceneを描き、Screenshot差分、Debug Layer Message 0件、Pass順、GPU Marker、Resource Sizeを確認します。

## 87. Device Lost

Frame Graphの論理定義とAsset Handleを残し、Physical Resource Pool、View Cache、Backend Objectを再生成します。

## 88. Resize

Size依存Compiled Graph/Physical Resourceを無効化し、新しいSizeで再Compileします。永続Asset Resourceまで作り直しません。

## 89. Shutdown

Worker停止、Command Result破棄、Graph/Pool解放、Asset GPU Resource解放、Context/Swap Chain/Device解放の順を明示します。

## 90. よくある失敗：Graph内Global Access

Passが宣言せずGlobal Textureを読むとDependencyとLifetime解析が壊れます。すべてのGraph Resource AccessをContractへ出します。

## 91. よくある失敗：万能Pass

一つのPassがShadow、Opaque、Effect、Post Processを処理するとEdgeも計測も粗くなります。意味ある出力境界で分けます。

## 92. よくある失敗：細かすぎるPass

一DrawごとのPass化はGraph構築、Marker、Query、Binding Costを増やします。独立Resource依存や計測単位で分けます。

## 93. よくある失敗：毎FrameResource生成

論理Resourceを毎Frame宣言しても、物理D3D ResourceはPoolで再利用します。宣言とAllocationを混同しません。

## 94. よくある失敗：GraphがGameplayを知る

Frame GraphはHP、Combo、AI Stateを直接読みません。確定したRender Snapshot/Parameterを入力にします。

## 95. よくある失敗：D3D11 Hazardを無視

GraphのRead/Write順が正しくても同じResource ViewがContext Slotへ残る場合があります。Backend Binding TrackerでUnbindします。

## 96. 導入手順

1. Render SnapshotとRender Proxyを作る。
2. Handle/Registryで生Pointer依存を減らす。
3. Render QueueとSort Keyを作る。
4. 既存描画をPass単位へ分ける。
5. PassのRead/Writeを手動宣言する。
6. Topological SortとValidationを作る。
7. Transient Resource Poolを接続する。
8. Marker、Timestamp、Visualizationを自動化する。
9. Feature ModuleとHistory Resourceを統合する。
10. Resize/Device Lost/Testを通す。

## 97. Architecture Checklist

- [ ] GameplayとD3D11 Objectが分離されている。
- [ ] Render SnapshotがFrame中不変である。
- [ ] Resourceは世代付きHandleで参照される。
- [ ] ViewごとにVisible Listがある。
- [ ] PassのRead/Write/Side Effectが宣言される。
- [ ] GraphがCycleと未初期化Readを検出する。
- [ ] Transient ResourceのLifetimeを解析する。
- [ ] Physical ResourceをPoolで再利用する。
- [ ] D3D11 Binding HazardをBackendが解除する。
- [ ] Resize/Device Lostで再構築できる。
- [ ] Pass MarkerとGPU時間が自動で付く。
- [ ] Graph構造を可視化・Testできる。

## 98. 理解確認問題

1. Render SnapshotがGameplayとRendererを分離する仕組みを説明してください。
2. Logical ResourceとPhysical Resourceの違いを説明してください。
3. PassのRead/Write宣言から依存Edgeを作る方法を説明してください。
4. Topological SortとCycle検出が必要な理由を説明してください。
5. Pass CullingでSide Effect宣言が必要な理由を説明してください。
6. First/Last UseからResourceを再利用する方法を説明してください。
7. D3D11版Frame GraphでもBinding Trackerが必要な理由を説明してください。
8. History ResourceをTransientにできない理由を説明してください。

## 99. 章末要点

- DataをGameplayからSnapshot、Queue、Pass、Backendへ一方向に流します。
- Frame GraphはPassとResourceの依存を宣言し、実行順とLifetimeを導きます。
- 論理Handleと物理D3D Resourceを分離し、Transient ResourceをPoolで再利用します。
- Read/Write、Version、Side Effectを明示し、Cycleや未初期化Readを検出します。
- D3D11 BackendはBinding Hazard、State Cache、View Cacheを正しく管理します。
- Marker、Timestamp、Graph Visualization、Unit TestをArchitectureへ組み込みます。
- Resize、Device Lost、Multi-view、Historyを最初からLifecycleへ含めます。

## 100. 参考資料

- [FrameGraph: Extensible Rendering Architecture in Frostbite](https://www.gdcvault.com/play/1024612/FrameGraph-Extensible-Rendering-Architecture-in)
- [Introduction to a render pass framework](https://gpuopen.com/learn/render_graphs/)
- [Introduction to a resource in Direct3D 11](https://learn.microsoft.com/en-us/windows/win32/direct3d11/overviews-direct3d-11-resources-intro)
- [Introduction to buffers in Direct3D 11](https://learn.microsoft.com/en-us/windows/win32/direct3d11/overviews-direct3d-11-resources-buffers-intro)
- [Introduction to textures in Direct3D 11](https://learn.microsoft.com/en-us/windows/win32/direct3d11/overviews-direct3d-11-resources-textures-intro)
- [ID3D11DeviceContext::OMSetRenderTargets](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11devicecontext-omsetrendertargets)
- [Direct3D 11 resource limits](https://learn.microsoft.com/en-us/windows/win32/direct3d11/overviews-direct3d-11-resources-limits)

次章では、DirectX 11編の知識を高速3D戦闘Sceneの一Frameへ統合する総合描画設計を扱います。
