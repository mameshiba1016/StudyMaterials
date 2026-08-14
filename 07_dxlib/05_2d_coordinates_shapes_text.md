# 第5章 2D座標・図形・文字描画

この章では、DXライブラリの図形・文字APIを入口に、ゲーム座標が画面の画素になるまでを学びます。重要なのは関数名の暗記ではなく、座標空間、端点、描画順、描画状態の契約です。

> 初期化とGame Loopは第1章、文字コードは第2章、時間は第3章、入力は第4章を前提とします。

## 1. 画面座標の基本

通常は左上が原点 `(0, 0)`、右が `+X`、下が `+Y` です。1280×720画面の有効な画素添字は `x=0..1279`、`y=0..719` です。数学の「上が+Y」と混同しないでください。

```text
(0, 0) -----------------> +X
  |
  |       (x, y)
  |          *
  v
 +Y
```

## 2. 整数版とAA版

`DrawLine` などは整数座標、`DrawLineAA` など末尾がAAの関数は浮動小数点座標を受け取ります。AA（Anti-Aliasing）は斜線や曲線の階段状の輪郭を濃淡で滑らかにします。

```cpp
DrawLine(10, 20, 180, 95, GetColor(255, 255, 255), 1);

// サブピクセル位置と実数の太さを保持する。
DrawLineAA(10.25F, 20.75F, 180.5F, 95.0F,
           GetColor(255, 255, 255), 1.5F);
```

## 3. 座標を表す値型

```cpp
struct Vec2 final
{
    float x = 0.0F;
    float y = 0.0F;

    [[nodiscard]] constexpr Vec2 operator+(const Vec2& rhs) const noexcept
    { return {x + rhs.x, y + rhs.y}; }

    [[nodiscard]] constexpr Vec2 operator-(const Vec2& rhs) const noexcept
    { return {x - rhs.x, y - rhs.y}; }

    [[nodiscard]] constexpr Vec2 operator*(float scale) const noexcept
    { return {x * scale, y * scale}; }
};
```

裸の `x, y` を何本も渡すより、意味のある値としてまとめると引数順の事故を減らせます。

## 4. 半開区間の矩形

自作コードでは `[left, right) × [top, bottom)`、つまり右端と下端を含まない契約が扱いやすいです。

```cpp
struct RectF final
{
    float left = 0.0F;   // 含む
    float top = 0.0F;    // 含む
    float right = 0.0F;  // 含まない
    float bottom = 0.0F; // 含まない

    [[nodiscard]] constexpr float Width() const noexcept { return right - left; }
    [[nodiscard]] constexpr float Height() const noexcept { return bottom - top; }

    [[nodiscard]] constexpr bool Contains(Vec2 p) const noexcept
    {
        return left <= p.x && p.x < right &&
               top <= p.y && p.y < bottom;
    }
};
```

隣り合う矩形が境界を二重所有せず、幅は常に `right-left` になります。DXライブラリAPI固有の端点規則への変換は呼び出し境界で行います。

## 5. 四つの座標空間

- ローカル座標: キャラクターや部品自身を原点とする。
- ワールド座標: ステージ全体で共有する。
- スクリーン座標: 描画先左上を原点とする画素位置。
- UI座標: 仮想解像度上のHUD・メニュー位置。

変数名を `worldPosition`、`screenPosition` のようにし、「どの空間か」を必ず残します。同じ `Vec2` でも意味は互換ではありません。

## 6. ローカルからワールドへ

```cpp
[[nodiscard]] constexpr Vec2 LocalToWorld(
    Vec2 localPosition, Vec2 objectWorldPosition) noexcept
{
    // 回転と拡大がない最小ケースは平行移動だけ。
    return localPosition + objectWorldPosition;
}
```

回転・拡大を加える場合は、拡大→回転→平行移動など適用順を仕様化します。順序が違えば結果も違います。

## 7. 2Dカメラ

```cpp
struct Camera2D final
{
    // 画面左上に対応するワールド位置。
    Vec2 worldTopLeft{};
};

[[nodiscard]] constexpr Vec2 WorldToScreen(
    Vec2 world, const Camera2D& camera) noexcept
{
    // カメラが右へ進むと静止物体は画面上で左へ動く。
    return world - camera.worldTopLeft;
}

[[nodiscard]] constexpr Vec2 ScreenToWorld(
    Vec2 screen, const Camera2D& camera) noexcept
{
    // マウス選択やエディタに必要な逆変換。
    return screen + camera.worldTopLeft;
}
```

## 8. 画面中央基準のカメラ

```cpp
[[nodiscard]] constexpr Vec2 WorldToScreenCentered(
    Vec2 world, Vec2 cameraCenter, Vec2 screenSize) noexcept
{
    return world - cameraCenter + screenSize * 0.5F;
}
```

追従対象を画面中央以外へ置くなら、`screenSize*0.5F` を画面上の注視点へ置き換えます。

## 9. 固定更新と描画補間

```cpp
[[nodiscard]] constexpr Vec2 Lerp(Vec2 from, Vec2 to, float t) noexcept
{
    return from + (to - from) * t;
}

// alpha = accumulator / fixedDelta。ゲーム状態は変更しない。
const Vec2 renderCamera = Lerp(previousCamera, currentCamera, alpha);
```

物理位置とカメラ位置に異なる丸めを行うと揺れます。ワールド値は実数で保ち、最後に同じ方針で画面へ変換します。

## 10. 論理解像度とレターボックス

```cpp
struct Viewport final
{
    float x = 0.0F;
    float y = 0.0F;
    float width = 0.0F;
    float height = 0.0F;
    float scale = 1.0F;
};

[[nodiscard]] Viewport MakeLetterboxViewport(
    float windowWidth, float windowHeight,
    float logicalWidth, float logicalHeight)
{
    const float sx = windowWidth / logicalWidth;
    const float sy = windowHeight / logicalHeight;
    const float scale = (sx < sy) ? sx : sy; // 縦横比を維持。
    const float width = logicalWidth * scale;
    const float height = logicalHeight * scale;

    return {(windowWidth - width) * 0.5F,
            (windowHeight - height) * 0.5F,
            width, height, scale};
}
```

UIを1280×720などの論理解像度で設計し、実画面へ等比拡大します。余白は背景色で消去します。

## 11. UIの正変換とマウスの逆変換

```cpp
#include <optional>

[[nodiscard]] constexpr Vec2 LogicalToPhysical(
    Vec2 logical, const Viewport& viewport) noexcept
{
    return {viewport.x + logical.x * viewport.scale,
            viewport.y + logical.y * viewport.scale};
}

[[nodiscard]] std::optional<Vec2> PhysicalToLogical(
    Vec2 physical, const Viewport& viewport) noexcept
{
    // 黒帯の入力をゲーム画面内として扱わない。
    if (physical.x < viewport.x || physical.y < viewport.y ||
        physical.x >= viewport.x + viewport.width ||
        physical.y >= viewport.y + viewport.height)
        return std::nullopt;

    return Vec2{(physical.x - viewport.x) / viewport.scale,
                (physical.y - viewport.y) / viewport.scale};
}
```

## 12. 丸め方を統一する

```cpp
#include <cmath>

[[nodiscard]] int RoundToPixel(float value) noexcept
{
    return static_cast<int>(std::lround(value));
}
```

切り捨てと床関数は負数で異なります。`static_cast<int>(-1.8F)` は `-1`、`floor(-1.8F)` は `-2` です。場当たり的に混ぜません。

## 13. ピクセルパーフェクトと滑らかさ

ドット絵は整数倍拡大・整数位置で輪郭を安定させます。滑らかな移動はサブピクセル座標とAAが向きます。アート方針ごとに選び、同一物体で混在させません。

## 14. 色 `GetColor`

```cpp
const unsigned int white = GetColor(255, 255, 255);
const unsigned int accent = GetColor(255, 180, 40);
const unsigned int black = GetColor(0, 0, 0);
```

戻り値は現在の画面モードに対応する色コードです。色ビット深度を変更した後も古いコードを永続利用せず、論理RGBから必要時に変換します。

```cpp
#include <cstdint>

struct Rgb8 final
{
    std::uint8_t r = 255;
    std::uint8_t g = 255;
    std::uint8_t b = 255;
};

[[nodiscard]] unsigned int ToDxColor(Rgb8 c)
{
    return GetColor(c.r, c.g, c.b);
}
```

透明度とブレンド状態は第7章で扱います。

## 15. 背景色と毎フレームの消去

```cpp
SetBackgroundColor(18, 20, 28);

while (ProcessMessage() == 0)
{
    ClearDrawScreen(); // 現在の描画先を背景色で消去。
    // Update();
    // DrawWorld(); DrawUi(); DrawDebug();
    ScreenFlip();
}
```

## 16. 描画先の副作用

`SetDrawScreen` は裏画面や画像へ描画先を変更します。公式仕様上、描画範囲や3Dカメラ設定にも影響します。切り替え後は必要な描画状態を明示的に設定し直します。

## 17. 点・線

```cpp
DrawPixel(320, 180, GetColor(255, 220, 80));

void DrawDebugLine(Vec2 from, Vec2 to, Rgb8 color)
{
    DrawLineAA(from.x, from.y, to.x, to.y,
               ToDxColor(color), 2.0F);
}
```

線は速度、視線、法線、レイを可視化できます。描画した線自体に当たり判定が生まれるわけではありません。大量の画素を `DrawPixel` で一つずつ描く設計も避けます。

## 18. 矩形

```cpp
void DrawRect(const RectF& r, Rgb8 color, bool filled)
{
    DrawBoxAA(r.left, r.top, r.right, r.bottom,
              ToDxColor(color), filled ? TRUE : FALSE);
}
```

塗りつぶしは面、`FALSE` は輪郭です。見た目の図形とColliderデータは別物にし、Colliderを正としてデバッグ表示へ渡します。

## 19. 円・楕円

```cpp
DrawCircleAA(320.0F, 240.0F,
             48.0F, 32, // 中心、半径、分割数
             GetColor(100, 220, 255), FALSE);

DrawOvalAA(480.0F, 240.0F,
           80.0F, 40.0F, 32, // 横半径、縦半径、分割数
           GetColor(255, 100, 100), TRUE);
```

分割数を上げるほど輪郭は滑らかになりますが、処理量も増えます。攻撃予告範囲の簡易表示にも利用できます。

## 20. 三角形

```cpp
DrawTriangleAA(100.0F, 200.0F,
               180.0F, 200.0F,
               140.0F, 120.0F,
               GetColor(255, 180, 40), TRUE);
```

頂点順はDirectXのカリング学習へつながるため、時計回りか反時計回りへ統一します。

## 21. 太い輪郭

線には太さを指定できます。図形輪郭へ直接太さを指定できない場合は、線分への分解、複数サイズの重ね描き、帯状頂点の生成を検討します。重ね描きは角や透明度が不均一になり得ます。

## 22. 画面外カリング

```cpp
[[nodiscard]] constexpr bool Overlaps(const RectF& a, const RectF& b) noexcept
{
    return a.left < b.right && b.left < a.right &&
           a.top < b.bottom && b.top < a.bottom;
}

const RectF screen{0, 0, 1280, 720};
if (Overlaps(objectScreenBounds, screen))
{
    // 見える可能性がある場合だけ描画要求を出す。
}
```

## 23. `SetDrawArea` の端点

`SetDrawArea(x1, y1, x2, y2)` は `(x1, y1)` を含み、右下は `(x2-1, y2-1)` までです。

```cpp
// x=100..499、y=80..379 の半開区間へ制限。
SetDrawArea(100, 80, 500, 380);
```

スクロールリスト、ミニマップ、分割画面に使えます。戻し忘れると後続のHUDが消えます。

## 24. RAIIで描画範囲を戻す

```cpp
class ScopedDrawArea final
{
public:
    ScopedDrawArea(int l, int t, int r, int b,
                   int restoreWidth, int restoreHeight)
        : width_(restoreWidth), height_(restoreHeight)
    {
        SetDrawArea(l, t, r, b);
    }

    ~ScopedDrawArea()
    {
        // returnや例外があっても全画面へ復元。
        SetDrawArea(0, 0, width_, height_);
    }

    ScopedDrawArea(const ScopedDrawArea&) = delete;
    ScopedDrawArea& operator=(const ScopedDrawArea&) = delete;

private:
    int width_ = 0;
    int height_ = 0;
};
```

これは全画面へ戻す簡易版です。入れ子を許す場合はRenderContextで以前の範囲をスタック管理します。

## 25. 描画状態の漏洩

描画先、描画範囲、ブレンド、明るさ、フォントは後続描画へ残る状態です。変更した関数が復元するか、呼び出し側が所有するかを契約として決めます。

## 26. 描画順

2Dでは通常、後から描いたものが手前に見えます。

```text
背景 → 地形 → キャラクター → エフェクト → HUD → デバッグ
```

更新順はゲームルール、描画順は重なりで決めます。両者を一致させる必要はありません。

## 27. レイヤーと描画コマンド

```cpp
enum class DrawLayer : int
{
    Background = 0,
    World = 100,
    Character = 200,
    Effect = 300,
    Ui = 400,
    Debug = 1000
};

struct LineCommand { Vec2 from; Vec2 to; Rgb8 color; float thickness; };
struct RectCommand { RectF rect; Rgb8 color; bool filled; };
```

値の間隔を空けると中間層を追加できます。ゲーム側は「描画要求」を登録し、RendererがDXライブラリを呼ぶ構造にするとAPIが各所へ散りません。

## 28. 安定した順序付け

```cpp
#include <algorithm>

std::stable_sort(commands.begin(), commands.end(),
    [](const auto& a, const auto& b)
    {
        if (a.layer != b.layer)
            return static_cast<int>(a.layer) < static_cast<int>(b.layer);
        return a.orderWithinLayer < b.orderWithinLayer;
    });
```

`stable_sort` は同順位の登録順を保ちます。大量コマンドでは毎フレームのソート時間を計測します。

## 29. 文字描画

```cpp
DrawString(24, 24, "HP 100 / 100", GetColor(255, 255, 255));

const int hp = 87;
const int maxHp = 100;
DrawFormatString(24, 52, GetColor(255, 255, 255),
                 "HP %d / %d", hp, maxHp);
```

書式指定と実引数の型が一致しないと未定義動作になり得ます。外部入力をフォーマット文字列として渡してはいけません。

```cpp
// 危険: userText中の%が書式として解釈され得る。
// DrawFormatString(0, 0, color, userText.c_str());

// 通常文字列として表示する。
DrawString(0, 0, userText.c_str(), color);
```

## 30. 文字幅と揃え

```cpp
void DrawCenteredText(int centerX, int y,
                      const char* text, unsigned int color)
{
    const int width = GetDrawStringWidth(text, -1);
    DrawString(centerX - width / 2, y, text, color);
}

void DrawRightText(int right, int y,
                   const char* text, unsigned int color)
{
    DrawString(right - GetDrawStringWidth(text, -1), y, text, color);
}
```

現在のフォント設定による幅を使います。文字コード版と関数シグネチャは使用中の公式リファレンスで確認します。

## 31. 複数行レイアウト

```cpp
#include <string>
#include <vector>

void DrawLines(int x, int y, int lineHeight,
               const std::vector<std::string>& lines,
               unsigned int color)
{
    for (std::size_t i = 0; i < lines.size(); ++i)
    {
        DrawString(x, y + static_cast<int>(i) * lineHeight,
                   lines[i].c_str(), color);
    }
}
```

改行を行へ分割すると、行送り、最大行数、スクロールを明示的に管理できます。

## 32. フォントとグリフ

本文、見出し、数値など用途別のフォントハンドルを起動時に作り、終了時に破棄します。毎フレーム作成しません。フォントに必要なグリフがない場合は文字が欠落するため、日本語、英数、記号を実データで確認します。

## 33. 文字列計測のキャッシュ

静的メニューは幅を一度測れば十分です。毎フレーム変わるスコアは再計測できますが、まず計測してボトルネックか判断します。複雑なキャッシュを先に作らないでください。

## 34. UIアンカー

```cpp
enum class Anchor { TopLeft, TopRight, BottomLeft, BottomRight, Center };

[[nodiscard]] Vec2 ResolveAnchor(Anchor a, Vec2 size, Vec2 margin)
{
    switch (a)
    {
    case Anchor::TopLeft: return margin;
    case Anchor::TopRight: return {size.x - margin.x, margin.y};
    case Anchor::BottomLeft: return {margin.x, size.y - margin.y};
    case Anchor::BottomRight: return size - margin;
    case Anchor::Center: return size * 0.5F;
    }
    return {};
}
```

アンカーと要素自身のピボットは別です。右上揃えなら要素幅を引いて右端を合わせます。

## 35. ワールドUIと画面UI

敵頭上のHPはワールド座標から画面変換します。固定HUDはカメラの影響を受けないUI座標で描きます。同じ文字APIでも変換の入口が異なります。

## 36. デバッグ描画キュー

```cpp
class DebugDraw final
{
public:
    void LineWorld(Vec2 from, Vec2 to, Rgb8 color)
    {
        lines_.push_back({from, to, color, 1.0F});
    }

    void Flush(const Camera2D& camera)
    {
        for (const LineCommand& line : lines_)
        {
            const Vec2 from = WorldToScreen(line.from, camera);
            const Vec2 to = WorldToScreen(line.to, camera);
            DrawLineAA(from.x, from.y, to.x, to.y,
                       ToDxColor(line.color), line.thickness);
        }
        lines_.clear(); // 古い要求を次フレームへ残さない。
    }

private:
    std::vector<LineCommand> lines_;
};
```

`LineWorld` と `LineScreen` を別名にすると二重変換を防げます。Collider、速度、経路、ターゲット、攻撃範囲を色分けします。

## 37. デバッグ表示の寿命

1フレーム、指定秒、永続を区別します。指定秒をゲーム時間と実時間のどちらで減らすかも、一時停止時の要件で決めます。無効時は文字列整形やコンテナ確保も省きます。

## 38. グリッド

```cpp
void DrawGrid(int spacing, int width, int height, Rgb8 color)
{
    if (spacing <= 0) return;

    for (int x = 0; x < width; x += spacing)
        DrawLine(x, 0, x, height, ToDxColor(color));

    for (int y = 0; y < height; y += spacing)
        DrawLine(0, y, width, y, ToDxColor(color));
}
```

ズームで線が密集しすぎる場合は倍率に応じて主目盛りを切り替えます。

## 39. 更新と描画を分離する

描画関数内でHPを減らしたりAIを進めたりしません。描画されなかった敵のゲーム処理が止まる設計も誤りです。描画は現在状態を読む処理に寄せます。

## 40. RenderContext

```cpp
struct RenderContext final
{
    int screenWidth = 1280;
    int screenHeight = 720;
    Camera2D camera{};
    Viewport uiViewport{};

    [[nodiscard]] Vec2 ToScreen(Vec2 world) const noexcept
    {
        return WorldToScreen(world, camera);
    }
};

void DrawWorldRect(const RenderContext& context,
                   const RectF& worldRect, Rgb8 color, bool filled)
{
    const Vec2 a = context.ToScreen({worldRect.left, worldRect.top});
    const Vec2 b = context.ToScreen({worldRect.right, worldRect.bottom});
    DrawBoxAA(a.x, a.y, b.x, b.y,
              ToDxColor(color), filled ? TRUE : FALSE);
}
```

画面サイズやカメラをグローバルから読む代わりに、必要な文脈を明示的に渡します。関数名から入力座標空間も分かります。

## 41. 1フレームの順序

```text
OSメッセージ処理
→ 入力スナップショット
→ 固定刻み更新
→ 描画補間
→ 裏画面消去
→ World描画
→ UI描画
→ Debug描画
→ ScreenFlip
```

## 42. よくある不具合

### 全部が同じ量だけずれる

個別物体ではなく、カメラ、ビューポート、原点を疑います。一部だけなら座標変換の二重適用・未適用を疑います。

### 右端だけ欠ける

閉区間と半開区間の混同、幅を右端として渡した、`-1` を二重適用した可能性を調べます。

### HUDが消える

`SetDrawArea`、描画先、ブレンド、フォントなどの状態が戻っているか、描画順が正しいか確認します。

### 文字化けする

ソース保存形式、コンパイラの実行文字セット、DXライブラリ設定、外部ファイル、フォントのグリフを順番に切り分けます。

### カメラ移動で震える

物体とカメラの丸め方、固定更新値の補間、整数版とAA版の混在を確認します。

## 43. 性能設計

- 大量の点・線・文字を個別関数で描く前に、画面外を除外する。
- 変化しない文字列の生成・幅計測を毎フレーム繰り返さない。
- 同種の描画要求をまとめる。ただし複雑化の前にCPU/GPU時間を計測する。
- 描画専用乱数とゲームルール用乱数を分離する。描画回数で戦闘結果を変えない。

## 44. DXライブラリなしでテストできる部分

座標変換、矩形交差、アンカー、ソートキーは純粋関数としてテストできます。

```cpp
static_assert(RectF{0, 0, 10, 10}.Contains({0, 0}));
static_assert(!RectF{0, 0, 10, 10}.Contains({10, 5}));
static_assert(Overlaps(RectF{0, 0, 10, 10}, RectF{9, 9, 20, 20}));
```

API呼び出しを薄い境界へ閉じ込めるほど、目視だけに頼らず検証できます。

## 45. 設計チェックリスト

- [ ] 原点、軸方向、端点規則を説明できる。
- [ ] ローカル、ワールド、画面、UI座標を区別した。
- [ ] カメラ変換と入力の逆変換が対になっている。
- [ ] 黒帯上のマウス入力を処理した。
- [ ] 丸め方とAA方針を統一した。
- [ ] 色コードを画面モードより長く保持していない。
- [ ] 描画範囲と描画先を確実に復元する。
- [ ] 更新順と描画順を分けた。
- [ ] 書式文字列と引数型を一致させた。
- [ ] デバッグ描画がゲーム状態を変更しない。
- [ ] 最適化を計測結果に基づいて行う。

## 46. 理解確認問題

1. 1280×720画面の最大有効画素座標は何か。
2. `SetDrawArea(10, 20, 30, 40)` が含む最後の画素は何か。
3. カメラが右へ10動くと静止物体の画面Xはどうなるか。
4. 黒帯上のクリックから論理座標を返さない理由は何か。
5. 外部文字列をフォーマットとして渡す危険は何か。
6. Colliderと描画図形を分ける理由は何か。
7. `SetDrawScreen` 後に状態を確認する理由は何か。
8. 描画処理が戦闘用乱数を消費すると何が起きるか。

## 47. 実践課題

1. 2D追従カメラと逆変換を実装する。
2. 16:9論理解像度を4:3へレターボックス表示し、マウスを逆変換する。
3. 点、線、矩形、円、楕円、三角形の確認画面を作る。
4. レイヤーで安定ソートする描画コマンド列を作る。
5. Collider、速度、名前をワールドデバッグ表示する。
6. 中央・右揃えと複数行へ対応する文字レイアウトを作る。

## 48. 公式資料

- [DXライブラリ 関数リファレンス](https://dxlib.xsrv.jp/dxfunc.html)
- [グラフィック関連関数（図形・文字列）](https://dxlib.xsrv.jp/function/dxfunc_graph3.html)
- [グラフィック関連関数（画面・描画先）](https://dxlib.xsrv.jp/function/dxfunc_graph0.html)

使用中バージョンの引数型、戻り値、失敗条件、副作用を公式資料で確認してください。次章では画像の読み込み、ハンドル寿命、切り出し、原点、反転、拡大回転を備えた2D Spriteへ進みます。
