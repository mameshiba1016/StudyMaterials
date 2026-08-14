# 第6章 Texture・Handle・2D Sprite

この章では画像ファイルを読み込み、2D Spriteとして安全かつ柔軟に描画する方法を学びます。DXライブラリでは画像リソースを整数の「グラフィックハンドル」で参照します。しかし整数に見えても、内部ではCPU側の画像情報やGPU側のテクスチャなどの資源につながっています。単なる番号として雑にコピーせず、所有権・寿命・失敗を設計します。

> 座標、矩形、カメラ、描画順は第5章を前提とします。透明度とBlendの詳説は第7章です。

## 1. 画像・Texture・Spriteの違い

- **画像ファイル**: PNGなど、ストレージ上に圧縮・符号化されたデータ。
- **画像データ**: デコード後の画素列と幅・高さなどの情報。
- **Texture**: 主にGPUが描画時に参照できる画像資源。
- **グラフィックハンドル**: DXライブラリ内の画像資源を指す識別値。
- **Sprite**: Textureのどこを、どの位置・原点・回転・拡大率で描くかという描画情報。

同じTextureから多数のSpriteを描けます。キャラクター100体のために同じ画像を100回読み込む必要はありません。

## 2. 読み込みから描画までの内部処理

概念的には次の流れです。

```text
Path解決
→ ファイルを開く
→ PNG等をデコード
→ 画素形式を変換
→ GPU資源を作成・転送
→ Handleを登録
→ Draw要求で頂点・UVを作る
→ GPUがTextureをSampling
→ Framebufferへ書き込む
```

そのため `LoadGraph` はファイルを読むだけではなく、毎フレーム呼ぶには重い処理です。

## 3. 最小の読み込み

```cpp
#include <DxLib.h>

const int playerHandle = LoadGraph("assets/textures/player.png");
if (playerHandle == -1)
{
    // Path、作業ディレクトリ、文字コード、形式、権限を調査する。
    return -1;
}

DrawGraph(100, 200, playerHandle, TRUE);
DeleteGraph(playerHandle);
```

読み込み失敗を無視して `-1` を描画関数へ渡さないでください。

## 4. ハンドルはポインタではない

`int` の値から内部アドレスや形式を推測してはいけません。有効性は生成APIの成功、所有者の寿命、削除済みでないことによって保証します。

```cpp
int a = LoadGraph("player.png");
int b = a; // 画像が複製されたのではなく、同じ識別値をコピーしただけ。

DeleteGraph(a);
// bも同じ削除済み資源を指すため、以後使用してはいけない。
```

## 5. 所有・借用・非所有参照

- 所有者: 最後に `DeleteGraph` を一度だけ呼ぶ責任を持つ。
- 借用者: 描画中だけ使い、削除しない。
- Cache: 複数利用者の共有所有を仲介する。

関数がハンドルを受け取るだけなら、原則として借用です。受け取った関数が勝手に削除してはいけません。

## 6. 最小RAIIラッパー

```cpp
#include <utility>

class UniqueGraph final
{
public:
    UniqueGraph() noexcept = default;
    explicit UniqueGraph(int handle) noexcept : handle_(handle) {}

    ~UniqueGraph() { Reset(); }

    // 同じHandleを二つの所有者が削除しないようコピー禁止。
    UniqueGraph(const UniqueGraph&) = delete;
    UniqueGraph& operator=(const UniqueGraph&) = delete;

    UniqueGraph(UniqueGraph&& other) noexcept
        : handle_(std::exchange(other.handle_, -1)) {}

    UniqueGraph& operator=(UniqueGraph&& other) noexcept
    {
        if (this != &other)
        {
            Reset();
            handle_ = std::exchange(other.handle_, -1);
        }
        return *this;
    }

    [[nodiscard]] bool IsValid() const noexcept { return handle_ != -1; }
    [[nodiscard]] int Get() const noexcept { return handle_; }

    void Reset(int newHandle = -1) noexcept
    {
        if (handle_ != -1)
            DeleteGraph(handle_);
        handle_ = newHandle;
    }

private:
    int handle_ = -1;
};
```

## 7. RAIIの破棄順序

`UniqueGraph` のデストラクタはDXライブラリが利用可能な間に呼ばれる必要があります。`DxLib_End()` の後でグローバルな `UniqueGraph` が破棄される設計は危険です。

```cpp
if (DxLib_Init() < 0) return -1;
{
    UniqueGraph player{LoadGraph("assets/player.png")};
    // ApplicationやSceneもこのスコープ内で破棄される。
}
DxLib_End();
```

## 8. Factoryで失敗を表現する

```cpp
#include <optional>
#include <string>

[[nodiscard]] std::optional<UniqueGraph> LoadUniqueGraph(
    const std::string& path)
{
    const int handle = LoadGraph(path.c_str());
    if (handle == -1)
        return std::nullopt;
    return UniqueGraph{handle};
}
```

コンストラクタ内の失敗を隠すより、戻り値で呼び出し側へ判断を要求できます。

## 9. サイズの取得

```cpp
struct IntSize final { int width = 0; int height = 0; };

[[nodiscard]] std::optional<IntSize> QueryGraphSize(int handle)
{
    int width = 0;
    int height = 0;
    if (GetGraphSize(handle, &width, &height) == -1)
        return std::nullopt;
    return IntSize{width, height};
}
```

出力引数は関数へアドレスを渡し、関数が値を書き込みます。成功前の値を利用しません。

## 10. Texture資源とMetadata

```cpp
struct TextureResource final
{
    UniqueGraph graph{};
    IntSize size{};
    std::string normalizedPath{};
};
```

幅・高さを毎描画で問い合わせず、ロード成功時に検証してMetadataとして保持できます。

## 11. `DrawGraph` の基準点

```cpp
DrawGraph(100, 200, textureHandle, TRUE);
```

`(100, 200)` は画像を描く領域の左上です。Spriteの中心位置ではありません。`TRUE` は透明部分を有効にする指定です。詳細な透過・合成は次章で扱います。

## 12. 左上基準Sprite

```cpp
struct SpriteTopLeft final
{
    int graphHandle = -1; // 借用。所有しない。
    Vec2 position{};
};

int DrawSprite(const SpriteTopLeft& sprite)
{
    return DrawGraph(RoundToPixel(sprite.position.x),
                     RoundToPixel(sprite.position.y),
                     sprite.graphHandle, TRUE);
}
```

SpriteがHandleを削除しないことをコメントと型で明確にします。

## 13. Pivot（原点）

Pivotは「Spriteのpositionが画像内のどこを表すか」です。中央 `(0.5, 0.5)`、足元中央 `(0.5, 1.0)` などを正規化値で表せます。

```cpp
struct Sprite final
{
    int graphHandle = -1;
    Vec2 position{};       // ワールドまたは画面位置
    Vec2 normalizedPivot{0.5F, 0.5F};
    Vec2 scale{1.0F, 1.0F};
    float rotationRadians = 0.0F;
    bool flipX = false;
    bool flipY = false;
};
```

## 14. 正規化Pivotから画素Pivotへ

```cpp
[[nodiscard]] constexpr Vec2 PivotPixels(Vec2 normalized, IntSize size) noexcept
{
    return {normalized.x * static_cast<float>(size.width),
            normalized.y * static_cast<float>(size.height)};
}
```

正規化値なら画像サイズ変更後も中央・足元という意味を保てます。範囲外を許すと画像外Pivotも表現できますが、必要なければ0～1へ検証します。

## 15. `DrawRotaGraph`

`DrawRotaGraph` は指定位置を中心として、等倍拡大率と角度で描画します。

```cpp
#include <numbers>

DrawRotaGraph(320, 240,
              1.5,                       // 等方拡大率
              std::numbers::pi / 4.0,    // 45度をradianで指定
              handle, TRUE);
```

度とradianを混同しないよう型または変換関数を用意します。

## 16. 度からradianへ

```cpp
#include <numbers>

[[nodiscard]] constexpr double ToRadians(double degrees) noexcept
{
    return degrees * std::numbers::pi / 180.0;
}
```

## 17. `DrawRotaGraph2`

`DrawRotaGraph2` は画像内の回転中心 `cx, cy` を指定できます。

```cpp
const Vec2 pivot = PivotPixels({0.5F, 1.0F}, size);
DrawRotaGraph2(RoundToPixel(position.x), RoundToPixel(position.y),
               RoundToPixel(pivot.x), RoundToPixel(pivot.y),
               1.0, angleRadians, handle, TRUE);
```

足元をPivotにすると、画像サイズが違うアニメーションでも地面位置を保ちやすくなります。

## 18. `DrawRotaGraph3`

縦横別の拡大率が必要なら `DrawRotaGraph3` を使います。

```cpp
double scaleX = sprite.flipX ? -sprite.scale.x : sprite.scale.x;
double scaleY = sprite.flipY ? -sprite.scale.y : sprite.scale.y;

DrawRotaGraph3(RoundToPixel(sprite.position.x),
               RoundToPixel(sprite.position.y),
               RoundToPixel(pivot.x), RoundToPixel(pivot.y),
               scaleX, scaleY,
               sprite.rotationRadians,
               sprite.graphHandle, TRUE);
```

負の拡大率による反転が利用中バージョンと望むPivot挙動に合うか、実画像で検証します。専用反転APIを選ぶ設計も可能です。

## 19. 左右反転とゲーム状態

見た目の `flipX` とキャラクターの論理的な向き `Facing` を分けます。

```cpp
enum class Facing { Left, Right };

sprite.flipX = (facing == Facing::Left);
```

攻撃判定の向きは `Facing` から計算し、描画の負スケールから逆算しません。

## 20. `DrawExtendGraph`

```cpp
DrawExtendGraph(100, 100, 356, 228,
                handle, TRUE);
```

左上・右下で描画先矩形を指定して拡大縮小します。縦横比を変えると画像が歪みます。負の幅や端点規則を利用した反転は可読性が低いため、意図を包む関数を使います。

## 21. Sprite Sheet

複数フレームを一枚に並べた画像をSprite SheetまたはTexture Atlasと呼びます。

```text
+--------+--------+--------+--------+
| idle 0 | idle 1 | idle 2 | idle 3 |
+--------+--------+--------+--------+
```

Texture切り替えを減らせますが、余白、にじみ、フレームサイズ、Pivotを設計する必要があります。

## 22. Source Rectangle

```cpp
struct SourceRect final
{
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
};
```

左上と幅・高さで保持すると `DrawRectGraph` の引数へ対応させやすくなります。幅・高さは正値か検証します。

## 23. `DrawRectGraph`

```cpp
int DrawFrameTopLeft(Vec2 destination,
                     const SourceRect& source,
                     int sheetHandle)
{
    return DrawRectGraph(RoundToPixel(destination.x),
                         RoundToPixel(destination.y),
                         source.x, source.y,
                         source.width, source.height,
                         sheetHandle,
                         TRUE,  // 透過を有効化
                         FALSE);// 反転しない
}
```

元画像内矩形が範囲外にならないよう、ロード時にSheetサイズと照合します。

## 24. Gridからフレーム矩形を計算

```cpp
[[nodiscard]] constexpr SourceRect FrameFromGrid(
    int frameIndex, int columns, int frameWidth, int frameHeight)
{
    const int column = frameIndex % columns;
    const int row = frameIndex / columns;
    return {column * frameWidth, row * frameHeight,
            frameWidth, frameHeight};
}
```

`columns > 0`、`frameIndex >= 0` を事前条件として検証します。

## 25. `LoadDivGraph`

等間隔Gridを読み込み時に複数Handleへ分割できます。

```cpp
constexpr int frameCount = 8;
int frames[frameCount]{};

const int result = LoadDivGraph("assets/run.png",
                                frameCount,
                                4, 2,      // 横数、縦数
                                64, 64,    // 1フレーム幅・高さ
                                frames);
if (result == -1)
{
    // 全体を失敗として扱い、中途半端な配列を使用しない。
}
```

生成された各Handleの所有権をまとめて管理し、全て一度ずつ解放します。

## 26. 分割HandleのRAII

```cpp
#include <vector>

class GraphSequence final
{
public:
    ~GraphSequence()
    {
        for (int handle : handles_)
            if (handle != -1) DeleteGraph(handle);
    }

    GraphSequence(const GraphSequence&) = delete;
    GraphSequence& operator=(const GraphSequence&) = delete;
    GraphSequence() = default;

    [[nodiscard]] const std::vector<int>& Handles() const noexcept
    { return handles_; }

    std::vector<int>& MutableHandlesForLoad() noexcept
    { return handles_; }

private:
    std::vector<int> handles_{};
};
```

実装ではロード失敗時の部分生成契約を公式資料で確認し、一時配列から成功後に所有権を確定します。

## 27. `DerivationGraph`

既存画像の一部分から新しいグラフィックハンドルを作成できます。元Handleの単なるSourceRect描画と、新Handle生成は意味が違います。

```cpp
const int child = DerivationGraph(0, 0, 64, 64, sheetHandle);
if (child == -1)
{
    // 切り出し範囲と元Handleを確認。
}
```

新たなHandleには新たな削除責任があります。単に毎回一部分を描くだけなら `DrawRectGraph` の方が資源数を増やしません。

## 28. UV座標の予習

DirectXではTexture上の位置を多くの場合0～1のUVで表します。

```cpp
struct UvRect final
{
    float u0, v0, u1, v1;
};

[[nodiscard]] UvRect ToUv(SourceRect r, IntSize texture)
{
    return {static_cast<float>(r.x) / texture.width,
            static_cast<float>(r.y) / texture.height,
            static_cast<float>(r.x + r.width) / texture.width,
            static_cast<float>(r.y + r.height) / texture.height};
}
```

DXライブラリがSourceRectを受け取り内部で同等の情報を作る、と考えるとDirectXへ接続できます。

## 29. Texture Bleeding

Atlasの隣フレーム色が端へ混ざる現象です。線形Filtering、拡大縮小、小数UVが原因になります。

- フレーム間へPaddingを設ける。
- 端画素をPaddingへ複製する。
- Pixel Artでは適切なFiltering方針を選ぶ。
- UV・SourceRectが隣領域へはみ出していないか確認する。

## 30. Animation Clip

```cpp
#include <span>

struct AnimationFrame final
{
    SourceRect source{};
    float durationSeconds = 0.1F;
};

struct AnimationClip final
{
    std::vector<AnimationFrame> frames{};
    bool loop = true;
};
```

全フレーム同じ時間とは限りません。見た目の溜め・ヒット・戻りを個別時間で調整できます。

## 31. Animatorの状態

```cpp
class SpriteAnimator final
{
public:
    void SetClip(const AnimationClip* clip, bool restart)
    {
        if (clip_ == clip && !restart) return;
        clip_ = clip; // 借用。ClipはAnimatorより長生きすること。
        frameIndex_ = 0;
        elapsed_ = 0.0F;
        finished_ = false;
    }

private:
    const AnimationClip* clip_ = nullptr;
    std::size_t frameIndex_ = 0;
    float elapsed_ = 0.0F;
    bool finished_ = false;
};
```

Texture資源、Clip定義、Animator再生状態を分離します。

## 32. 大きいDeltaへの対応

```cpp
void Update(float deltaSeconds)
{
    if (!clip_ || clip_->frames.empty() || finished_) return;
    elapsed_ += deltaSeconds;

    // ifではなくwhile。長いフレームで複数コマ進む可能性がある。
    while (elapsed_ >= clip_->frames[frameIndex_].durationSeconds)
    {
        const float duration = clip_->frames[frameIndex_].durationSeconds;
        if (duration <= 0.0F) break; // 不正データによる無限Loopを防ぐ。
        elapsed_ -= duration;
        ++frameIndex_;

        if (frameIndex_ >= clip_->frames.size())
        {
            if (clip_->loop) frameIndex_ = 0;
            else
            {
                frameIndex_ = clip_->frames.size() - 1;
                finished_ = true;
                break;
            }
        }
    }
}
```

極端なDeltaに上限を設ける、最大進行回数を設けるなども検討します。

## 33. Animation Event

足音、攻撃判定開始、VFXなどを特定フレームへ関連付けられます。ただし「描画されたから攻撃する」設計にはしません。Animator更新がイベントを発行し、戦闘状態が消費します。

```cpp
enum class AnimationEventType { Footstep, AttackActive, SpawnEffect };

struct AnimationEvent final
{
    std::size_t frameIndex = 0;
    AnimationEventType type{};
};
```

ループや大Deltaでフレームを飛び越えた場合も、通過したイベントを正しい回数だけ発行します。

## 34. 戦闘ロジックとAnimationの分離

Animationは表現、Combat Stateはルールです。攻撃が成立する時間をAnimation画像番号だけに依存させると、画像差し替えでゲーム性が変わります。共通Timelineから両者を駆動するか、明確なEvent契約を使います。

## 35. Resource Cacheの目的

同じ正規化Pathの画像は一度だけ読み込み、共有します。

```cpp
#include <memory>
#include <unordered_map>

class TextureCache final
{
public:
    [[nodiscard]] std::shared_ptr<const TextureResource> Load(
        const std::string& normalizedPath);

private:
    std::unordered_map<std::string,
        std::weak_ptr<const TextureResource>> cache_{};
};
```

Cacheだけが永遠に強参照すると未使用資源も残るため、`weak_ptr` で利用者がいなくなった資源を解放できる設計例です。

## 36. Cache実装の流れ

```cpp
std::shared_ptr<const TextureResource> TextureCache::Load(
    const std::string& path)
{
    if (auto it = cache_.find(path); it != cache_.end())
        if (auto existing = it->second.lock()) return existing;

    const int raw = LoadGraph(path.c_str());
    if (raw == -1) return {};

    UniqueGraph graph{raw};
    const auto size = QueryGraphSize(raw);
    if (!size) return {}; // graphのDestructorが自動解放。

    auto resource = std::make_shared<TextureResource>(
        TextureResource{std::move(graph), *size, path});
    cache_[path] = resource;
    return resource;
}
```

## 37. PathをCache Keyにする注意

`player.png`、`./player.png`、`textures/../player.png` が同じファイルを指す場合があります。Pathを辞書Keyにする前に区切り、相対成分、大文字小文字方針を正規化します。第2章のPath設計を再利用します。

## 38. Asset ID

ゲームコードへ生Pathを散らさず、論理IDからManifestでPathを解決する方法があります。

```cpp
enum class TextureId { Player, Enemy, HitEffect, UiAtlas };

[[nodiscard]] std::string_view ResolvePath(TextureId id)
{
    switch (id)
    {
    case TextureId::Player: return "assets/characters/player.png";
    case TextureId::Enemy: return "assets/characters/enemy.png";
    case TextureId::HitEffect: return "assets/effects/hit.png";
    case TextureId::UiAtlas: return "assets/ui/ui_atlas.png";
    }
    return {};
}
```

実運用ではデータファイル化すると、再コンパイルせず割り当てを変更できます。

## 39. Placeholder

必須でない画像が失敗したとき、目立つ市松模様などのPlaceholderを表示すると欠落位置を発見できます。必須Assetの失敗は起動を止め、任意Assetは代替表示にするなど重要度を定義します。

## 40. Preloadと遅延Load

- Preload: Scene開始前に必要資源を揃え、戦闘中の停止を避ける。
- Lazy Load: 初回利用時に読み込み、起動時間と初期Memoryを減らす。
- Async Load: 読み込み完了前に処理を返すが、状態・同期・破棄が複雑になる。

高速アクション中に同期 `LoadGraph` を呼ばず、Loading画面やScene遷移中に準備します。

## 41. 非同期読み込みの状態機械

```cpp
enum class AssetState { Unloaded, Loading, Ready, Failed };

struct AsyncTexture final
{
    AssetState state = AssetState::Unloaded;
    int pendingHandle = -1;
    std::shared_ptr<const TextureResource> ready{};
};
```

「Handleが返った＝描画可能」と決めつけず、公式の非同期完了確認APIに従います。Scene破棄中に読み込みが完了する競合も設計します。

## 42. Memory量の概算

非圧縮RGBA8 Textureは概算で `width × height × 4 byte` です。

```text
4096 × 4096 × 4 = 67,108,864 byte ≒ 64 MiB
```

PNGファイルが数MiBでも、GPU上では大きく展開される可能性があります。MipMapや内部形式、複製も含め、実測します。

## 43. Texture AtlasのTrade-off

利点はTexture切替削減と一括管理です。欠点は巨大画像の常駐、差し替え範囲の拡大、にじみ、最大Textureサイズ、部分更新の難しさです。何でも一枚にせず、寿命・Scene・用途が近い画像をまとめます。

## 44. 描画コマンドへSpriteを追加

```cpp
struct SpriteCommand final
{
    std::shared_ptr<const TextureResource> texture{};
    SourceRect source{};
    Vec2 position{};
    Vec2 pivot{0.5F, 0.5F};
    Vec2 scale{1.0F, 1.0F};
    float rotation = 0.0F;
    DrawLayer layer = DrawLayer::World;
    int order = 0;
    bool flipX = false;
};
```

Commandが実行されるまでTextureを生存させるため、共有参照を保持する例です。毎Commandの参照Count操作が問題ならFrame単位のResource tableなどを計測後に検討します。

## 45. World Spriteの描画

```cpp
void SubmitWorldSprite(const RenderContext& context,
                       const Sprite& sprite,
                       IntSize size)
{
    const Vec2 screen = context.ToScreen(sprite.position);
    const Vec2 pivot = PivotPixels(sprite.normalizedPivot, size);
    const double sx = sprite.flipX ? -sprite.scale.x : sprite.scale.x;
    const double sy = sprite.flipY ? -sprite.scale.y : sprite.scale.y;

    DrawRotaGraph3(RoundToPixel(screen.x), RoundToPixel(screen.y),
                   RoundToPixel(pivot.x), RoundToPixel(pivot.y),
                   sx, sy, sprite.rotationRadians,
                   sprite.graphHandle, TRUE);
}
```

## 46. UI SpriteはCamera変換しない

```cpp
const Vec2 physical = LogicalToPhysical(uiPosition, context.uiViewport);
```

World用関数とUI用関数を名前で分け、Camera変換の二重適用を防ぎます。UIのScaleには論理解像度Scaleも合成します。

## 47. SortingとBatching

正しい見た目にはLayer順が必要ですが、性能には同じTextureをまとめることが有利です。透明Spriteは順番変更で見た目が変わるため、無条件にTexture順へ並べ替えられません。

```text
まず正しいLayer・奥行き順
→ 順序交換可能な範囲だけTextureでまとめる
→ 計測して効果を確認
```

## 48. Draw Callの考え方

DXライブラリ関数を1回呼ぶことと、必ずGPU Draw Callが1回発生することは同義とは限りません。内部Batchingがあり得るため、推測せずProfilerで確認します。ただしTextureやRender Stateの頻繁な切替はBatchを分断しやすい概念を覚えます。

## 49. State漏洩

Sprite描画前後にはBlend、明るさ、描画範囲、描画先などの状態が影響します。赤く点滅させるため `SetDrawBright` を変更したら、後続Spriteへ残さないよう復元します。次章でScoped Stateを詳しく作ります。

## 50. Hot Reload

開発中の画像差し替えを検知して再読み込みすると調整が速くなります。ただし既存Handleを削除してから新規Loadが失敗すると表示不能になります。

```text
新画像を一時HandleへLoad
→ サイズ・内容を検証
→ 成功したら参照を切替
→ 古いHandleを解放
```

交換を成功後に行うTransactionalな設計が安全です。

## 51. Device Lost・再生成の予習

GPU資源はOS、画面Mode、Device状態によって再生成が必要になる場合があります。Asset IDと元PathをResourceが保持していれば復旧しやすくなります。DirectX編ではDeviceとResourceの寿命をさらに明示します。

## 52. Thread Safety

「ファイル読込は別Threadでよい」と「DXライブラリのHandle生成APIを任意Threadから呼べる」は別問題です。公式仕様でThread制約を確認し、CPU側I/OとMain Thread上のGPU資源生成を分ける設計も検討します。

## 53. エラーLogに残す情報

```cpp
// 例: 実際のLoggerへ構造化して渡す。
// event=TextureLoadFailed
// logicalId=Player
// normalizedPath=assets/characters/player.png
// scene=Battle
// required=true
```

「Load失敗」だけでは再現できません。解決済みPath、作業Directory、Asset ID、重要度を残します。

## 54. よくある不具合: 画像が出ない

1. Handleが `-1` ではないか。
2. 作業Directoryを基準にPathが正しいか。
3. 描画先が裏画面か別画像のままではないか。
4. 座標が画面外ではないか。
5. 描画範囲が制限されたままではないか。
6. 透明色・Blend・明るさで見えなくなっていないか。
7. `ScreenFlip` より前に描いているか。

## 55. よくある不具合: 二重解放

Handleを所有するクラスをコピーした、SceneとCacheの両方が削除した、`InitGraph` 後に個別Destructorが削除した、などが原因です。所有者を一つにし、RAII型をMove-onlyにします。

## 56. よくある不具合: Memory Leak

エラー分岐で `DeleteGraph` を通らない、`DerivationGraph` の子Handleを忘れる、Cacheが永久に強参照する、Scene切替後もCommandが資源を保持する可能性を調べます。RAIIは早期returnにも強い設計です。

## 57. よくある不具合: Animationが速さ依存

「描画1回でframeIndex++」するとFPSで速度が変わります。秒単位のdurationとdelta timeで進めます。描画されない状態でもAnimationを進めるかは、ゲーム要件で決めます。

## 58. よくある不具合: 足元が揺れる

フレームごとに画像余白・高さ・Pivotが異なると、中央基準描画で足元が動きます。全フレームのCanvasを統一するか、フレーム別Pivot Metadataを持ちます。

## 59. よくある不具合: 反転時に位置が飛ぶ

負ScaleとPivotの関係、SourceRectの反転方向、ワールド側Facing補正の二重適用を確認します。Pivot位置を画面へDebug描画すると原因を見つけやすくなります。

## 60. Asset検証

起動時またはBuild工程で次を検証できます。

- ファイルが存在し読み込める。
- 幅・高さが正で、上限以下。
- SourceRectがTexture範囲内。
- Animation frameが空でない。
- durationが正。
- Asset IDが重複しない。
- 必須AssetにPlaceholder指定だけで済ませていない。

## 61. Pure Functionのテスト

```cpp
constexpr SourceRect f = FrameFromGrid(5, 4, 64, 32);
static_assert(f.x == 64); // 5 % 4 = 1列目
static_assert(f.y == 32); // 5 / 4 = 1行目

constexpr Vec2 p = PivotPixels({0.5F, 1.0F}, {100, 80});
static_assert(p.x == 50.0F && p.y == 80.0F);
```

DXライブラリ呼び出しと計算を分けると、SourceRectやPivotを自動Testできます。

## 62. 1Sceneの読み込み順

```text
Manifest読込
→ 必須Asset一覧を作る
→ Path正規化
→ Cacheから取得またはLoad
→ サイズ・SourceRect・Clip検証
→ 全必須Asset成功後にScene開始
→ Scene終了で利用者の共有参照を解放
→ 未使用Cache entryを掃除
```

戦闘開始後に初めて必須画像を読む設計を避けます。

## 63. 設計チェックリスト

- [ ] 画像、Texture、Handle、Spriteの違いを説明できる。
- [ ] Load失敗を `-1` で検出している。
- [ ] Handleの所有者と借用者が明確である。
- [ ] `DeleteGraph` が一度だけ呼ばれる。
- [ ] DXライブラリ終了前にResourceを破棄する。
- [ ] 同じPathを毎フレームLoadしない。
- [ ] PathをCache Keyにする前に正規化した。
- [ ] SourceRectをTexture範囲内に検証した。
- [ ] Pivotを仕様化し、フレーム間で足元が安定する。
- [ ] Animationを秒単位で更新する。
- [ ] 描画Eventと戦闘ルールを分離した。
- [ ] UI SpriteへCamera変換を適用していない。
- [ ] Hot Reloadは新規Load成功後に交換する。
- [ ] Memory使用量とDraw順を計測している。

## 64. 理解確認問題

1. Handleをコピーしても画像が複製されないのはなぜか。
2. `DxLib_End` 後にRAII Destructorを呼ぶ設計が危険なのはなぜか。
3. `DrawGraph` の座標は画像のどこを表すか。
4. `DrawRectGraph` と `DerivationGraph` の資源寿命の違いは何か。
5. Pivotを足元中央にする利点は何か。
6. 大きいdeltaでAnimator更新に `while` が必要なのはなぜか。
7. CacheがPathを正規化すべき理由は何か。
8. 透明SpriteをTexture順に無条件Sortingできないのはなぜか。
9. PNGファイルサイズとTexture Memory量が一致しないのはなぜか。
10. Hot Reloadで古いHandleを先に消してはいけない理由は何か。

## 65. 実践課題

1. Move-onlyな `UniqueGraph` と失敗を返すFactoryを作る。
2. `TextureResource` に幅・高さ・正規化Pathを保持する。
3. 左上、中央、足元中央Pivotの描画結果を比較する。
4. Sprite SheetからSourceRectを計算し、8frame Animationを再生する。
5. 非loop Clipの完了と大deltaをTestする。
6. 同じPathを一度だけ読む `TextureCache` を作る。
7. Asset欠落時に市松模様Placeholderを表示する。
8. World SpriteとUI Spriteの描画入口を分ける。
9. Pivot、SourceRect、Texture境界をDebug描画する。
10. Scene開始前PreloadとScene終了時解放を記録する。

## 66. 公式資料

- [DXライブラリ グラフィック関連関数](https://dxlib.xsrv.jp/function/dxfunc_graph1.html)
- [DXライブラリ 関数リファレンス一覧](https://dxlib.xsrv.jp/dxfunc.html)
- [DXライブラリ その他関数（非同期読み込み関連）](https://dxlib.xsrv.jp/function/dxfunc_other.html)

利用中バージョンの宣言、戻り値、部分失敗、Thread制約、副作用を必ず公式資料で確認してください。

## 67. 次章への接続

Textureの所有権、Spriteの座標・Pivot、Animation、Cacheがそろいました。次章ではAlpha、Blend Mode、描画輝度、Render Target、描画状態の保存・復元を扱い、半透明Effectや画面合成へ進みます。
