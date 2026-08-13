# 2D UI・レイアウト・フォーカス

UIは画像を画面座標へ置くだけではありません。解像度、文字、入力方式、フォーカス、アクセシビリティ、ローカライズへ対応します。

## UI座標空間

World Cameraの影響を受けないUI Canvasを使います。設計解像度から実ウィンドウへScaleとViewport Offsetを計算します。

```cpp
struct ViewportTransform
{
    float scale{};
    Vector2 offsetPixels{};
};

Vector2 WindowToCanvas(Vector2 windowPoint, const ViewportTransform& transform)
{
    return (windowPoint - transform.offsetPixels) / transform.scale;
}
```

Letterbox余白上のクリックはCanvas外として扱います。

## Anchor・Pivot・Margin

- Anchor：親Rectのどの比率位置へ結び付けるか。
- Pivot：自分Rectのどこを配置基準にするか。
- Margin/Padding：外側・内側余白。

右上のHP表示ならAnchorを右上、Pivotも右上へすると解像度変更に追従します。

## Layout Pass

```text
Measure：子が必要とするサイズを求める
Arrange：親が各子の最終Rectを決める
Paint  ：確定Rectを描画する
```

文字変更やウィンドウリサイズでLayoutをDirtyにし、必要な部分だけ再計算します。描画中にLayoutを変更すると順序依存になるため分離します。

## Widget Treeと所有権

```cpp
class Widget
{
public:
    virtual ~Widget() = default;
    virtual Vector2 Measure(Vector2 available) = 0;
    virtual void Arrange(Rect finalRect) = 0;
    virtual void Draw(UiRenderer& renderer) const = 0;

private:
    std::vector<std::unique_ptr<Widget>> children_{};
};
```

親が子を所有すると寿命は明確ですが、外部が生ポインタを保存する場合、Widget再構築でダングリングします。安定ID、Handle、弱参照を使います。

## Hit Test

前面から背面へ、表示中・有効・入力可能なWidgetを調べます。親のClip、Transform、透明でも入力を受けるかを考慮します。描画順とHit Test順を一致させます。

## Event Routing

- Capture/Tunnel：RootからTargetへ。
- Target：対象自身。
- Bubble：TargetからRootへ。

Scroll内Buttonなど、子がClickを消費し親がDragを判断するために使います。Pointer Downを受けたWidgetへPointer Captureし、外へ移動してもUpを受け取れるようにします。

## Focus Navigation

ゲームパッド・キーボードでは一つのWidgetへFocusを置きます。

- Scene表示時の初期Focus。
- 無効化・削除時の代替Focus。
- 上下左右の遷移先。
- Dialog内から背面へFocusが漏れないFocus Trap。
- MouseとGamepadの最終入力デバイス表示。

画面座標だけで自動近傍検索すると意図しないボタンへ飛ぶため、重要画面は明示Navigationを設定します。

## Button状態

```text
Normal / Hovered / Pressed / Focused / Disabled
```

色だけで違いを表すと色覚多様性へ対応できません。形、枠、音、アニメーションも組み合わせます。連打による二重決定を防ぐため、遷移要求後に入力をロックします。

## テキスト

Font AtlasへGlyphを配置し、字形、Kerning、行送り、Wrap、Alignmentを計算します。UTF-8のbyte数は表示文字数ではありません。CJK、結合文字、絵文字、右から左、禁則処理には専門Text Shaping/Layout機能を使います。

## ローカライズ

固定幅Buttonに英語文言だけ合わせると他言語で溢れます。

- 文言キーと翻訳データ。
- 可変サイズ・Wrap。
- 複数形・文法。
- Font fallback。
- 擬似ローカライズで文字長を拡大。
- テキストを画像へ焼き込まない。

## HUDとModel

UIがPlayerオブジェクトへ毎箇所から直接アクセスすると結合します。View Modelや読み取りSnapshot、イベントで必要値を渡します。イベントだけでは初期状態を得られないため、購読時同期も必要です。

## Nine-Slice

Panel画像を四隅、辺、中央の9領域へ分け、角を伸ばさず任意サイズへ描きます。Border幅が出力Rectより大きい場合の縮小規則を決めます。

## アクセシビリティ

- UI Scale。
- 字幕、話者名、背景、サイズ。
- 高コントラスト。
- 点滅・揺れ軽減。
- 色以外の情報表現。
- 長押し・連打代替。
- 読み上げ用の意味ラベル（対応環境）。

UIはマウスだけでなくKeyboard、Gamepad、Touchを個別にテストします。
