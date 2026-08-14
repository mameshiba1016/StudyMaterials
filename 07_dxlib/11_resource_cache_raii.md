# 第11章 Resource Cache・RAII

Resource管理の目的は「LoadしたものをMapへ入れる」だけではありません。生成に失敗し得る外部資源を、正しい型・所有権・寿命・Threadで扱い、二重解放、Leak、使用中解放、同一Assetの重複Loadを防ぐことです。

## 1. Resourceとは

- Texture、Render Target。
- Sound、BGM Stream。
- Font、Shader、Model。
- Collision Mesh、Animation Clip。
- 設定Data、Localization、Stage Data。

CPU MemoryだけのDataとDXライブラリHandleを持つResourceでは破棄制約が異なります。

## 2. 所有権の基本

- Unique ownership: 所有者は一つ。`unique_ptr`やMove-only RAII。
- Shared ownership: 複数利用者。`shared_ptr`。
- Non-owning reference: 借用。Reference、Pointer、ID。
- Weak reference: 生存を延長しない観察。`weak_ptr`。

「誰が最後に解放するか」を説明できない設計は危険です。

## 3. RAII

Resource Acquisition Is Initializationは、取得成功をObjectの有効状態へ結び付け、Destructorで解放する考え方です。早期`return`や例外でもScope終了時に解放されます。

## 4. 型付きHandle

画像も音声も`int`なので取り違えられます。

```cpp
template<class Tag>
class RawHandle final
{
public:
    constexpr RawHandle() noexcept = default;
    explicit constexpr RawHandle(int value) noexcept : value_(value) {}

    [[nodiscard]] constexpr int Value() const noexcept { return value_; }
    [[nodiscard]] constexpr bool IsValid() const noexcept { return value_ != -1; }

    friend constexpr bool operator==(RawHandle, RawHandle) noexcept = default;

private:
    int value_ = -1;
};

struct GraphTag;
struct SoundTag;
using GraphHandle = RawHandle<GraphTag>;
using SoundHandle = RawHandle<SoundTag>;
```

`GraphHandle`をSound関数へ誤って渡すには明示変換が必要になります。

## 5. Deleter Policy

```cpp
struct GraphDeleter final
{
    using Handle = GraphHandle;
    static void Destroy(Handle h) noexcept
    {
        if (h.IsValid()) DeleteGraph(h.Value());
    }
};

struct SoundDeleter final
{
    using Handle = SoundHandle;
    static void Destroy(Handle h) noexcept
    {
        if (!h.IsValid()) return;
        StopSoundMem(h.Value());
        DeleteSoundMem(h.Value());
    }
};
```

Resource種別ごとの破棄手順をPolicyへ閉じ込めます。

## 6. Generic Unique Handle

```cpp
#include <utility>

template<class Deleter>
class UniqueHandle final
{
public:
    using Handle = typename Deleter::Handle;

    UniqueHandle() noexcept = default;
    explicit UniqueHandle(Handle h) noexcept : handle_(h) {}
    ~UniqueHandle() { Reset(); }

    UniqueHandle(const UniqueHandle&) = delete;
    UniqueHandle& operator=(const UniqueHandle&) = delete;

    UniqueHandle(UniqueHandle&& rhs) noexcept
        : handle_(std::exchange(rhs.handle_, Handle{})) {}

    UniqueHandle& operator=(UniqueHandle&& rhs) noexcept
    {
        if (this != &rhs)
        {
            Reset();
            handle_ = std::exchange(rhs.handle_, Handle{});
        }
        return *this;
    }

    void Reset(Handle replacement = Handle{}) noexcept
    {
        Deleter::Destroy(handle_);
        handle_ = replacement;
    }

    [[nodiscard]] Handle Get() const noexcept { return handle_; }
    [[nodiscard]] bool IsValid() const noexcept { return handle_.IsValid(); }

private:
    Handle handle_{};
};

using UniqueGraph = UniqueHandle<GraphDeleter>;
using UniqueSound = UniqueHandle<SoundDeleter>;
```

## 7. Rule of Zero

Resource classがRAII memberだけを持てば、独自Destructorを多数書かずに済みます。

```cpp
struct TextureResource final
{
    UniqueGraph graph{};
    IntSize size{};
    std::string sourcePath{};
};
```

## 8. `Release`

所有権を外へ渡す`Release`を追加する場合、呼出側が必ず新所有者になる契約が必要です。安易に公開するとLeakを作りやすいため、必要になるまで実装しません。

## 9. 失敗をValueで表す

```cpp
enum class ResourceErrorCode
{
    FileNotFound,
    DecodeFailed,
    ApiFailed,
    InvalidMetadata,
    Cancelled
};

struct ResourceError final
{
    ResourceErrorCode code{};
    std::string assetId{};
    std::string path{};
    std::string detail{};
};
```

`-1`だけでなく、どの段階で失敗したかを上位へ返します。C++23なら`std::expected`が候補です。

## 10. Factory

```cpp
[[nodiscard]] std::shared_ptr<const TextureResource>
LoadTextureResource(const std::string& normalizedPath,
                    ResourceError& error)
{
    const int raw = LoadGraph(normalizedPath.c_str());
    if (raw == -1)
    {
        error = {ResourceErrorCode::ApiFailed, {}, normalizedPath,
                 "LoadGraph failed"};
        return {};
    }

    UniqueGraph graph{GraphHandle{raw}};
    int width = 0;
    int height = 0;
    if (GetGraphSize(raw, &width, &height) == -1 || width <= 0 || height <= 0)
    {
        error = {ResourceErrorCode::InvalidMetadata, {}, normalizedPath,
                 "invalid graph size"};
        return {}; // graphが自動解放。
    }

    return std::make_shared<TextureResource>(
        TextureResource{std::move(graph), {width, height}, normalizedPath});
}
```

## 11. Resource Key

PathだけでなくLoad設定もResourceの同一性に含まれます。

```cpp
struct TextureKey final
{
    std::string normalizedPath{};
    bool useAlpha = true;
    bool premultipliedAlpha = false;

    friend bool operator==(const TextureKey&, const TextureKey&) = default;
};
```

同じFileでもPMA、Color Space、MipMap、用途が違えば別Resourceになり得ます。

## 12. Path正規化

区切り文字、`.`、`..`、大文字小文字、Unicode、相対基準を統一します。ただし存在しないPathを無理にCanonical化すると失敗するAPIもあるため、Lexical正規化と実File解決を分けます。

## 13. Asset ID

Game logicはPathではなく`TextureId::Player`のような論理IDを使い、ManifestがKeyへ解決します。Directory再編でGame Codeを修正せずに済みます。

## 14. Cache Entry

```cpp
template<class Resource>
struct CacheEntry final
{
    std::weak_ptr<const Resource> resource{};
    std::uint64_t lastAccessFrame = 0;
    std::size_t estimatedBytes = 0;
};
```

Weak参照なら利用者がいなくなったResourceの寿命を延長しません。

## 15. Weak Cache

```cpp
class TextureCache final
{
public:
    std::shared_ptr<const TextureResource> Get(const TextureKey& key);
    void RemoveExpired();

private:
    std::unordered_map<TextureKey,
        CacheEntry<TextureResource>, TextureKeyHash> entries_{};
};
```

`weak_ptr::lock()`成功なら共有、失敗なら再Loadします。

## 16. Strong Cache

Cacheが強参照を持つと再利用は速い一方、明示EvictionまでMemoryへ残ります。Scene開始前PreloadとLRU Budgetを組み合わせる場合に向きます。

## 17. Cache Policyは用途別

- 共通UI: Application中ずっとStrong。
- Stage Asset: Scene ScopeでStrong。
- 任意Effect: Weak Cache。
- Render Target: Pool管理。
- BGM Stream: 専用Controller所有。

全Resourceへ同じPolicyを適用しません。

## 18. Cache Stampede

同じ未Load Keyを複数箇所が同時要求すると重複Loadが起こります。Cache Entryへ`Loading`状態と共有Future/Jobを置き、進行中Requestを共有します。

## 19. Entry State

```cpp
enum class ResourceState { Unloaded, Loading, Ready, Failed };

struct AsyncEntry final
{
    ResourceState state = ResourceState::Unloaded;
    std::uint64_t generation = 0;
    ResourceError lastError{};
};
```

`Failed`を永久Cacheするか、Retry時間を設けるかを決めます。

## 20. Negative Cache

存在しないAssetを毎FrameLoadし直さないよう失敗も短時間Cacheできます。開発中のFile追加を検知できるよう、永久失敗にはしません。

## 21. Generation

Hot Reloadや再LoadごとにGenerationを増やします。古い非同期Jobが遅れて完了しても、現在Generationと違えば適用せず破棄します。

## 22. 非同期Pipeline

```text
Main: RequestとGeneration発行
Worker: File I/O・Decode
Main: Generation確認・DX Handle生成
Main: Metadata検証
Main: EntryへPublish
```

DXライブラリAPIのThread制約を公式仕様で確認します。

## 23. PublishのAtomicity

Resourceを部分初期化状態で公開しません。全必須fieldとHandleが有効になった後、一度にReady参照へ交換します。

## 24. Cancel

Scene終了でLoadingをCancelしてもWorker処理が即停止できるとは限りません。結果を捨てるCancellationと、処理自体を中断するCancellationを区別します。

## 25. Main Thread Queue

Workerから直接SceneやCache containerへ書かず、完了MessageをThread-safe Queueへ入れ、Main ThreadがFrame安全点で適用します。

## 26. Hot Reload

```text
変更検知
→ 新Resourceを別ObjectへLoad
→ 検証
→ 成功時だけCurrent参照を交換
→ 旧Resourceは既存利用者が離れた時に破棄
```

失敗時は旧版を維持し、Errorを表示します。

## 27. Stable Resource Reference

利用者が`shared_ptr<const TextureResource>`を持つ方式ではReload後も旧版を使い続けます。全利用者を即新規版へ切り替えたい場合は、Stable Slotの中身を交換する間接参照方式があります。

```cpp
template<class T>
struct ResourceSlot final
{
    std::shared_ptr<const T> current{};
    std::uint64_t generation = 0;
};
```

## 28. Const Resource

共有Resourceは`shared_ptr<const T>`で公開し、利用者がHandleやMetadataを書き換えないようにします。Animation再生位置など可変Instance状態はResource外へ置きます。

## 29. ResourceとInstance

- Resource: Texture、Clip定義、Model Data。
- Instance: Position、現在Frame、Material Parameter、再生時間。

一体化すると共有できず、一体の変更が全Characterへ波及します。

## 30. Dependency

MaterialがTexture、Animation SetがModelを参照するようにResource間依存があります。強参照Cycleを作らないよう所有方向を一方向にします。

## 31. 循環参照

`shared_ptr`同士が互いを所有すると参照Countが0になりません。親が子をStrong、子が親をWeakまたはIDで参照します。

## 32. Dependency Graph

Asset AのReloadが依存Asset Bの再構築を必要とする場合があります。依存関係をGraphで記録し、Cycle検出、Topological順のLoadを行います。

## 33. Placeholder

Texture欠落は市松模様、Sound欠落は無音Resource、Model欠落は簡易形状など型別Placeholderを用意できます。ただし必須Assetの失敗を隠さずLogと画面表示を残します。

## 34. Null Object

無効Handleを毎描画で分岐する代わりに有効なPlaceholderを返せます。無音Soundは再生成功扱いにするか、欠落統計を増やすかを決めます。

## 35. Memory Budget

```cpp
struct ResourceBudget final
{
    std::size_t textureBytes = 512ULL * 1024ULL * 1024ULL;
    std::size_t soundBytes = 256ULL * 1024ULL * 1024ULL;
};
```

推定値と実際のGPU/Audio Memoryは異なるため、API計測が可能なら併用します。

## 36. Eviction

Budget超過時は未使用Resourceから追い出します。使用中の強参照を強制破棄してはいけません。LRU、Scene Scope、Priorityを組み合わせます。

## 37. LRU

Least Recently Usedは最終利用が古いEntryを候補にします。しかし巨大Asset一個と小Asset多数のTrade-offがあるため、Sizeと再Load Costも考慮します。

## 38. Frame中Eviction

Rendererが借用HandleをCommandへ積んだ後にEvictすると使用中解放になります。Commandが共有参照を持つか、EvictionをFrame終了後へ限定します。

## 39. Deferred Destruction

GPUが前FrameのResourceをまだ使用中の場合、CPU上の参照が消えても即破棄できないBackendがあります。DXライブラリの保証に従い、DirectX編ではFenceとDeferred Release Queueを扱います。

## 40. Render Target Pool

Size、Format、Alpha、MSAAをKeyにして一時Targetを貸し出します。Frame終了時に返却し、同時使用中のTargetを二重貸出しません。

## 41. Pool Lease

```cpp
class RenderTargetLease final
{
public:
    ~RenderTargetLease(); // Poolへ返す。Deleteとは限らない。
    RenderTargetLease(const RenderTargetLease&) = delete;
    RenderTargetLease& operator=(const RenderTargetLease&) = delete;
};
```

所有と貸出を区別します。Lease DestructorはResource破棄でなくPool返却です。

## 42. `InitGraph`等の全削除

全Resource削除APIを使うと個別RAII Objectが古いHandleを保持します。その後Destructorが二重削除する可能性があります。個別管理と全削除を混在させず、Managerが全所有状態を同時にInvalid化できる場合だけ使います。

## 43. Device再生成

Source Path、Asset ID、Load Optionsを保持すればHandleを再作成できます。Runtime生成Targetには再生成Recipeを持たせます。

## 44. Shutdown順

```text
Scene停止
→ 非同期Job Cancel・Join
→ Render Command消費完了
→ Scene-local Resource解放
→ Cache/Pool解放
→ Audio/Renderer解放
→ DxLib_End
```

Workerが終了後にDX APIを呼ばないことを保証します。

## 45. Static/Global Destructor問題

Global RAII Resourceは`DxLib_End`後にDestructorが走る恐れがあります。Resource所有者をApplication Scopeへ置き、終了順をCodeで明示します。

## 46. Exception Safety

- Basic guarantee: Objectは有効だが値は変わり得る。
- Strong guarantee: 失敗時は変更前を維持。
- No-throw guarantee: 失敗しても例外を投げない。

Destructorは例外を外へ出さず、Hot Reload交換はStrong guaranteeを目指します。

## 47. Logging

```text
event=ResourceLoadFailed
assetId=player_texture
type=Texture
normalizedPath=assets/player.png
generation=4
thread=Main
error=LoadGraphFailed
```

User環境の絶対Pathや秘密情報は不用意に外部へ送信しません。

## 48. Metrics

- Type別Resource数・推定Bytes。
- Cache Hit/Miss率。
- 同時Loading数。
- Load時間、Decode時間、Main Thread生成時間。
- Eviction数、Reload数、失敗数。
- Placeholder使用数。
- 参照が残る旧Generation数。

## 49. Debug一覧

Asset ID、型、状態、参照Count、Size、最終利用Frame、Generationを画面へ表示し、Scene終了後も残るResourceを発見します。`shared_ptr::use_count`は診断用であり同期判断へ使いません。

## 50. Test Double

```cpp
struct FakeGraphApi final
{
    int nextHandle = 1;
    int loadCount = 0;
    int deleteCount = 0;
    bool failNext = false;
};
```

DX APIをAdapter経由にし、同じKeyを一度だけLoad、失敗時Leakなし、一度だけDeleteをTestします。

## 51. Fault Injection

1回目Load失敗、Metadata失敗、Memory Budget超過、Hot Reload失敗、Cancel直後完了を意図的に発生させます。正常系だけでは寿命Bugを見つけられません。

## 52. よくある不具合

- 二重解放: 所有型をCopyした、全削除とRAIIを混在。
- Leak: Cycle、永久Strong Cache、失敗分岐。
- 古い画像: Hot Reload後も旧共有参照を保持。
- 一瞬停止: Gameplay中に同期Load。
- 重複Load: Key未正規化、Loading中Requestを共有していない。
- 終了Crash: WorkerまたはGlobal DestructorがDX終了後に実行。
- 型違い: 裸の`int` Handleを渡した。

## 53. 設計チェックリスト

- [ ] Resource型ごとに型付きHandleを使う。
- [ ] 所有HandleはMove-only RAIIである。
- [ ] Load失敗に詳細Errorがある。
- [ ] KeyへPathとLoad Optionsを含めた。
- [ ] Pathを一貫して正規化した。
- [ ] Weak/Strong Cacheを用途で選んだ。
- [ ] 進行中の同一Loadを共有する。
- [ ] Hot Reload成功後だけ交換する。
- [ ] ResourceとInstanceを分離した。
- [ ] 強参照Cycleがない。
- [ ] Frame中に使用ResourceをEvictしない。
- [ ] Job終了後にCacheを破棄する。
- [ ] 全HandleをDX終了前に解放する。

## 54. 理解確認問題

1. 型付きHandleはどのBugをCompile時に防ぐか。
2. Move-onlyにする理由は何か。
3. Weak CacheとStrong Cacheの寿命差は何か。
4. Path以外のLoad OptionをKeyへ含める理由は何か。
5. Cache Stampedeとは何か。
6. Generationが古い非同期結果を防ぐ仕組みは何か。
7. Hot Reloadで旧Resourceを先に消さない理由は何か。
8. ResourceとInstanceを分ける理由は何か。
9. `InitGraph`と個別RAIIの混在が危険な理由は何か。
10. Shutdown時にJobを先に止める理由は何か。

## 55. 実践課題

1. Graph/Soundの型付きHandleとGeneric RAIIを作る。
2. Error詳細を返すTexture Factoryを作る。
3. 正規化KeyのWeak Cacheを作る。
4. Fake APIでLoad一回・Delete一回をTestする。
5. 同一Keyの非同期Requestを共有する。
6. Generation付きHot Reloadを作る。
7. Strong CacheへMemory BudgetとLRUを追加する。
8. Render Target PoolとMove-only Leaseを作る。
9. Resource Debug一覧とMetricsを表示する。
10. 終了順とFault Injectionを自動Testする。

## 56. 公式資料

- [DXライブラリ 関数リファレンス一覧](https://dxlib.xsrv.jp/dxfunc.html)
- [グラフィック関連関数](https://dxlib.xsrv.jp/function/dxfunc_graph1.html)
- [Sound関連関数](https://dxlib.xsrv.jp/function/dxfunc_sound.html)
- [その他関数・非同期読み込み](https://dxlib.xsrv.jp/function/dxfunc_other.html)

各Handleの失敗値、削除API、全削除API、非同期完了、Thread制約を利用中バージョンのHeaderと公式資料で確認してください。

## 57. 次章への接続

Resourceの型・寿命・失敗を統一できました。次章ではDebug Log・Gizmo・Error処理を扱い、これらの状態、Load時間、Scene遷移、Collision、描画Passを観測可能にします。
