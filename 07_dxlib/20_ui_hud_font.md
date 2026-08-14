# DXライブラリ：UI・HUD・Font

この章では、画面サイズや入力機器が変わっても読みやすく、戦闘中に必要な情報を素早く伝えられるUIを設計します。単に画像と文字を座標指定で描くだけでなく、Layout、状態管理、Font、Animation、入力Focus、アクセシビリティ、負荷まで一つの仕組みとして扱います。

> API仕様はDXライブラリ公式リファレンスを基準にしています。文字コード、利用フォント、DXライブラリ版によって利用可能な関数や引数形式が異なる場合は、使用環境のヘッダーも確認してください。

## 1. UIとHUD

- UIは、メニュー、設定、Dialog、Inventoryなど操作と情報表示全般を指す。
- HUDは、HP、ゲージ、照準、敵情報などゲームプレイ中に重なる情報を指す。
- World-space UIは、ダメージ数字や頭上マーカーなど3D世界へ結び付く。
- Screen-space UIは、画面座標へ固定される。

UIの目的は装飾ではなく、現在状態、可能な操作、操作結果、次に注意すべき対象を短時間で伝えることです。

## 2. Immediate ModeとRetained Mode

```text
Immediate Mode:
毎フレーム「この位置へこれを描く」と命令する

Retained Mode:
Widget Treeが状態を保持し、Layoutと描画を行う
```

DXライブラリの描画関数はImmediate Mode寄りですが、ゲーム側ではWidgetを保持するRetained Mode風の構造を作れます。小規模なDebug UIはImmediate、大規模なメニューやHUDはWidget化すると管理しやすくなります。

## 3. UI描画の基本順序

```text
3D opaque
 -> 3D transparent/VFX
 -> World-space UI
 -> Screen-space HUD
 -> Menu/Dialog
 -> Cursor
 -> Debug overlay
```

UIへ入る前に3D用Depth、Shader、Blend、描画先を既知状態へ戻します。前のPassの状態へ依存しません。

## 4. 仮想解像度

座標を実画面へ直接固定すると、解像度ごとに配置が変わります。例えば1920×1080を設計座標として使います。

```cpp
struct UiViewport final
{
    float designWidth{1920.0f};
    float designHeight{1080.0f};
    int screenWidth{};
    int screenHeight{};
    float scale{};
    float offsetX{};
    float offsetY{};
};

UiViewport BuildUiViewport(int width, int height)
{
    UiViewport result{};
    result.screenWidth = width;
    result.screenHeight = height;

    const float scaleX = width / result.designWidth;
    const float scaleY = height / result.designHeight;
    result.scale = std::min(scaleX, scaleY); // 縦横比を維持する。

    result.offsetX = (width - result.designWidth * result.scale) * 0.5f;
    result.offsetY = (height - result.designHeight * result.scale) * 0.5f;
    return result;
}
```

余白をLetterboxとして残すのか、背景だけ画面全体へ広げるのかを決めます。

## 5. Design座標からScreen座標へ

```cpp
struct UiPoint final { float x{}; float y{}; };

UiPoint ToScreen(const UiViewport& viewport, UiPoint designPosition)
{
    return {
        viewport.offsetX + designPosition.x * viewport.scale,
        viewport.offsetY + designPosition.y * viewport.scale
    };
}
```

マウス入力は逆変換してDesign座標へ戻せば、LayoutとHit Testが同じ座標系になります。

```cpp
UiPoint ToDesign(const UiViewport& viewport, UiPoint screenPosition)
{
    return {
        (screenPosition.x - viewport.offsetX) / viewport.scale,
        (screenPosition.y - viewport.offsetY) / viewport.scale
    };
}
```

## 6. AnchorとPivot

Anchorは親矩形のどこを基準に置くか、PivotはWidget自身のどこを基準点とするかです。

```cpp
struct UiTransform final
{
    UiPoint anchor{0.5f, 0.5f}; // 親の正規化座標。中央は0.5, 0.5。
    UiPoint pivot{0.5f, 0.5f};  // 自分の中央を基準にする。
    UiPoint offset{};           // Design pixel単位の追加位置。
    UiPoint size{};
};
```

右上固定ならAnchorを `(1, 0)`、Pivotも `(1, 0)` にします。解像度変更後も端からの距離を維持できます。

## 7. Rectと階層Layout

```cpp
struct UiRect final
{
    float x{};
    float y{};
    float width{};
    float height{};

    [[nodiscard]] bool Contains(UiPoint point) const noexcept
    {
        return point.x >= x && point.x < x + width &&
               point.y >= y && point.y < y + height;
    }
};
```

親Rectから子Rectを計算します。絶対座標を全Widgetへ散らさず、Horizontal、Vertical、Grid、OverlayといったLayout規則を用意します。

## 8. Safe Area

画面端はDisplay Overscan、丸角、配信Overlay、字幕などと競合します。重要情報はSafe Area内へ置きます。

```cpp
UiRect ApplySafeMargin(UiRect screen, float horizontal, float vertical)
{
    return {
        screen.x + horizontal,
        screen.y + vertical,
        screen.width - horizontal * 2.0f,
        screen.height - vertical * 2.0f
    };
}
```

Safe Areaを設定画面から調整できると表示環境へ対応しやすくなります。

## 9. DPIと整数Pixel

UIを小数位置へ描くと、細い線やFontがぼやける場合があります。最終Screen座標を必要に応じて整数へ丸めます。一方、Animationはfloatで保持し、描画時だけ丸めることで動きの蓄積誤差を防ぎます。

## 10. Panel描画

単色Panelは `DrawBox` で描けます。

```cpp
void DrawPanel(const UiRect& rect, unsigned int color, int alpha)
{
    SetDrawBlendMode(DX_BLENDMODE_ALPHA, std::clamp(alpha, 0, 255));
    DrawBox(
        static_cast<int>(std::round(rect.x)),
        static_cast<int>(std::round(rect.y)),
        static_cast<int>(std::round(rect.x + rect.width)),
        static_cast<int>(std::round(rect.y + rect.height)),
        color,
        TRUE);
    SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 255);
}
```

角丸、枠、Gradientが必要なら9-slice画像やShaderを使います。

## 11. 9-slice

Panel画像を四隅、上下左右、中央の9領域へ分割します。角は拡大せず、辺は一方向、中央だけ両方向へ伸ばすため、枠線や角丸を壊さず可変サイズにできます。

```text
+---+-------+---+
| TL|  Top  | TR|
+---+-------+---+
| L |Center | R |
+---+-------+---+
| BL|Bottom | BR|
+---+-------+---+
```

小さすぎるRectでは左右・上下のCorner幅が重なるため、最小サイズを定義します。

## 12. ImageのScale Mode

- Stretch：Rect全体へ伸縮。縦横比が崩れる。
- Fit：縦横比を維持し全体を収める。余白ができる。
- Fill：縦横比を維持してRectを埋める。画像が切れる。
- Native：元画像Pixel数を使う。
- Tile：繰り返す。

アイコンはFit、背景はFill、PatternはTileなど意味に応じて使います。

## 13. ScissorとClip

Scroll Viewやゲージでは、子描画を矩形内へ制限します。DXライブラリの描画範囲設定を使う場合、開始前の範囲を保存し、終了後に必ず戻します。入れ子Clipでは親と子の交差矩形を使います。

```text
effectiveClip = intersect(parentClip, childClip)
```

Clipが空なら子を描かず、Draw Callを削減できます。

## 14. Font Handleの作成

```cpp
const int fontHandle = CreateFontToHandle(
    nullptr,                         // nullptrなら標準Font。
    32,                              // Font size。
    4,                               // 太さ。
    DX_FONTTYPE_ANTIALIASING_EDGE);  // 縁付きAntialias。

if (fontHandle == -1)
{
    throw std::runtime_error("Font creation failed");
}
```

公式仕様上、フォントハンドル作成後にサイズや太さを途中変更できません。本文、見出し、数字など用途別のHandleを起動・画面ロード時に作ります。毎フレーム作成してはいけません。

## 15. Font HandleのRAII

```cpp
class UniqueFont final
{
public:
    explicit UniqueFont(int handle = -1) noexcept : handle_(handle) {}
    ~UniqueFont() { Reset(); }

    UniqueFont(const UniqueFont&) = delete;
    UniqueFont& operator=(const UniqueFont&) = delete;

    UniqueFont(UniqueFont&& other) noexcept
        : handle_(std::exchange(other.handle_, -1)) {}

    UniqueFont& operator=(UniqueFont&& other) noexcept
    {
        if (this != &other)
        {
            Reset();
            handle_ = std::exchange(other.handle_, -1);
        }
        return *this;
    }

    void Reset() noexcept
    {
        if (handle_ != -1)
        {
            DeleteFontToHandle(handle_);
            handle_ = -1;
        }
    }

    [[nodiscard]] int Get() const noexcept { return handle_; }

private:
    int handle_{-1};
};
```

`InitFontToHandle` の全削除と個別RAIIを混在させると、RAIIへ削除済みHandleが残ります。

## 16. OS Font依存を避ける

名前指定したFontが全PCへ入っているとは限りません。選択肢は次のとおりです。

- 標準FontへFallbackする。
- `EnumFontName` で利用可能Fontを確認する。
- 配布許諾を確認したFontをゲームへ同梱する。
- `CreateDXFontData.exe` でDX Font Dataを作る。
- `LoadFontDataToHandle` で固定データを読む。

```cpp
int fontHandle = LoadFontDataToHandle("Data/Font/MainText.dft", 2);
if (fontHandle == -1)
{
    fontHandle = CreateFontToHandle(nullptr, 28, 3,
                                    DX_FONTTYPE_ANTIALIASING_EDGE);
}
```

FontのLicenseと再配布条件を必ず確認します。

## 17. 文字列描画

```cpp
DrawStringToHandle(
    100,
    80,
    "HP",
    GetColor(255, 255, 255),
    fontHandle,
    GetColor(0, 0, 0)); // 対応Font Typeなら縁色に利用する。
```

背景が頻繁に変化するHUDでは、縁取りや影でContrastを保ちます。ただし太すぎる縁は小さい文字を潰します。

## 18. 文字列の幅を測る

文字数が同じでもGlyph幅は一定ではありません。中央・右揃えは描画幅を取得して計算します。

```cpp
int MeasureTextWidth(std::string_view text, int fontHandle)
{
    return GetDrawStringWidthToHandle(
        text.data(),
        static_cast<int>(text.size()), // byte数。文字コードとの関係に注意。
        fontHandle);
}

void DrawCenteredText(int centerX, int y, std::string_view text,
                      unsigned int color, int fontHandle)
{
    const int width = MeasureTextWidth(text, fontHandle);
    DrawStringToHandle(centerX - width / 2, y, text.data(), color, fontHandle);
}
```

`string_view::data()` がNull終端を保証しない場合があります。APIが終端を読む関数では所有Stringへ変換するか、長さ指定版を選びます。

## 19. UTF-8・UTF-16・文字数

日本語の1文字はUTF-8で複数byteです。`std::string::size()` はGlyph数ではなくbyte数を返します。

```text
byte count != Unicode code point count != grapheme cluster count != glyph count
```

絵文字、結合文字、異体字、濁点結合では一層複雑です。途中でbyte列を切断しないよう、プロジェクトの文字コードとDXライブラリの設定・関数系を統一します。

## 20. 改行と折返し

自動折返しは文字数ではなく描画幅で判断します。

1. 単語・禁則単位へ分割する。
2. 候補を現在行へ足した幅を測る。
3. 最大幅を超えるなら改行する。
4. 行高を加算する。
5. 最大行数を超えるなら省略記号を付ける。

日本語では行頭禁則、行末禁則、句読点ぶら下げも考慮します。Dialog用とHUD短文用でLayout規則を分けても構いません。

## 21. Text Cache

毎フレーム変わらない長文を毎回Format・計測しないよう、文字列、Font、最大幅、言語、ScaleをKeyにLayout結果をCacheします。言語変更、Font変更、解像度変更時に無効化します。

頻繁に変わるTimerやDamage数字はCache探索より直接描画が軽い場合もあるため、実測します。

## 22. Format文字列の安全性

`DrawFormatStringToHandle` は便利ですが、型とFormat指定の不一致は未定義動作につながります。外部データをFormat文字列として直接渡してはいけません。

```cpp
// 翻訳文をFormatとして解釈させず、先に安全な文字列を構築する。
const std::string text = std::format("{:03}", currentCount);
DrawStringToHandle(x, y, text.c_str(), color, fontHandle);
```

利用Compilerが `std::format` に対応しない場合は、安全なFormat libraryや明示変換を使います。

## 23. Premultiplied Alpha

Font Cacheを乗算済みAlpha用にする場合は、作成前に `SetFontCacheUsePremulAlphaFlag(TRUE)` を設定し、描画時も対応する `DX_BLENDMODE_PMA_ALPHA` を使います。Straight Alpha用FontとBlend方式を混ぜると縁に黒・白のHaloが出ます。

Font作成状態は暗黙のGlobal設定になり得るため、Font Factoryへ集約します。

## 24. UI Color

色だけで状態を伝えないことが重要です。

```text
危険 = 赤色 + 警告Icon + 点滅/形状 + Text
選択 = 色変化 + 枠 + Scale + Cursor
使用不可 = 彩度低下 + Lock Icon + 説明
```

Contrast比、色覚特性、明るい背景・暗い背景の両方を確認します。点滅は頻度と強度を抑え、無効化設定を用意します。

## 25. Widgetの状態

```cpp
enum class InteractionState
{
    Normal,
    Hovered,
    Focused,
    Pressed,
    Disabled
};

struct ButtonVisual final
{
    unsigned int backgroundColor{};
    unsigned int textColor{};
    float scale{1.0f};
    float opacity{1.0f};
};
```

入力状態と見た目を分離し、StateからVisualを決めます。Disabledは入力を受けず、理由を説明できる設計にします。

## 26. Mouse Hit Test

上に描かれたWidgetを先に判定します。透明な親、Clip外、Disabled、非表示Widgetは判定対象から外します。

```cpp
Widget* FindTopmostWidget(UiPoint point, std::span<Widget*> drawOrder)
{
    for (auto iterator = drawOrder.rbegin(); iterator != drawOrder.rend(); ++iterator)
    {
        Widget* widget = *iterator;
        if (widget->IsHitTestVisible() && widget->WorldRect().Contains(point))
            return widget;
    }
    return nullptr;
}
```

押下開始Widgetと離したWidgetが同じ場合だけClickにするのが基本です。

## 27. Focus Navigation

Gamepad・KeyboardではFocusが必要です。

- 上下左右で次のFocus対象を決める。
- 決定でActivate、Cancelで一つ上へ戻る。
- Menuを開いたら妥当な初期Focusを置く。
- Widget削除時にFocusを安全な場所へ移す。
- 現在Focusを色だけでなく枠・Cursorでも示す。

単純な配列順では2D配置と合わないため、方向・距離・角度から候補をScore化します。

## 28. 入力方式の切替

最後に意味のある入力を行ったDeviceを記録し、Promptを切り替えます。

```cpp
enum class InputDevice
{
    KeyboardMouse,
    Gamepad
};

// Stick driftを入力方式変更と誤認しないようDead Zoneを超えた入力だけ採用する。
```

Mouseの微小移動やGamepadのDriftで表示が交互に切り替わらないよう、Thresholdと短いLock時間を設けます。

## 29. Input Prompt

Action名と実Keyを分離します。

```text
Gameplay action: Dodge
Binding: Keyboard Space / Gamepad East button
UI prompt: [Space] 回避 or [Button icon] 回避
```

再Binding、Controller種類、地域ごとの決定・Cancel配置へ追従させます。画像内へ操作文字を焼き込まないようにします。

## 30. Screen StackとModal

```text
HUD
  -> Pause Menu
      -> Settings Dialog
          -> Confirmation Modal
```

最上位Modalだけが入力を受け、背後は暗転・入力停止します。Push・Pop時にFocus、Pause状態、Cursor表示、入力Contextを復元します。

## 31. UI Event

WidgetからGameplayの具体的Classを直接呼ぶと依存が強くなります。

```cpp
struct UiCommand final
{
    enum class Type { Resume, Retry, OpenSettings, Quit } type{};
};

// UIはCommandを発行し、上位Controllerが意味を解釈する。
```

Click SoundやAnimation開始もEventとして一貫した時点で処理します。

## 32. HP Bar

```cpp
float SafeRatio(float current, float maximum)
{
    if (maximum <= 0.0f) return 0.0f;
    return std::clamp(current / maximum, 0.0f, 1.0f);
}

void DrawHorizontalGauge(UiRect rect, float ratio,
                         unsigned int background,
                         unsigned int foreground)
{
    DrawBox(static_cast<int>(rect.x), static_cast<int>(rect.y),
            static_cast<int>(rect.x + rect.width),
            static_cast<int>(rect.y + rect.height), background, TRUE);

    const float filledWidth = rect.width * std::clamp(ratio, 0.0f, 1.0f);
    DrawBox(static_cast<int>(rect.x), static_cast<int>(rect.y),
            static_cast<int>(rect.x + filledWidth),
            static_cast<int>(rect.y + rect.height), foreground, TRUE);
}
```

即時値と遅延追従値を重ねると、受けたDamage量を認識しやすくなります。

## 33. Gaugeの追従Animation

Frame rate依存の固定加算ではなく、時間または指数補間を使います。

```cpp
float Damp(float current, float target, float sharpness, float deltaSeconds)
{
    const float t = 1.0f - std::exp(-sharpness * deltaSeconds);
    return std::lerp(current, target, t);
}
```

実値は即時更新し、表示遅延値だけを補間します。死亡判定などGameplayを表示値で行ってはいけません。

## 34. Enemy HPとBoss HP

- 通常敵はTarget Lock中・Damage直後だけ表示する。
- 画面外なら方向Indicatorへ切り替える。
- Bossは名前、Phase、特殊Gaugeをまとめる。
- 多数の敵Barが重なる場合は優先度で省略する。
- 距離によりScaleとOpacityを調整する。

World位置をScreenへ投影し、カメラ後方、Near Plane外、Occlusion時の扱いを決めます。

## 35. Skill IconとCooldown

```text
Icon base
 -> unavailable dark overlay
 -> radial/vertical cooldown mask
 -> remaining seconds
 -> input prompt
 -> charge count
```

Cooldownの表示比率は `remaining / duration` を0～1へClampします。内部Timerの浮動小数誤差で `-0.0` 秒を表示しないよう丸めます。

## 36. Combo Counter

Combo数は変化時だけ強いAnimationを出し、常時揺らしません。

- 数字が増えた瞬間にScaleを上げて戻す。
- 閾値到達で色・Effectを変える。
- Combo猶予をRingやBarで示す。
- 終了後に短い保持時間を置いてFade Outする。

表示Animationが更新順によって一Frame遅れないよう、戦闘結果確定後にHUD View Modelを更新します。

## 37. Damage Number

Damage Eventから表示用Instanceを生成します。

```cpp
struct DamageNumber final
{
    std::string text;
    VECTOR worldPosition{};
    float ageSeconds{};
    float lifetimeSeconds{0.8f};
    float randomHorizontalOffset{};
    bool critical{};
};
```

同時発生時は位置をずらし、一定数を超えたら小さい値を統合・省略します。Object Poolで毎回のAllocationを減らします。

## 38. Target Marker

TargetのWorld位置をScreenへ投影し、Lock状態、距離、攻撃予兆を示します。

- 画面内：対象中心や弱点へMarkerを置く。
- 画面外：Safe Area端へ方向Arrowを置く。
- カメラ後方：投影結果を反転・Clampする前に後方判定する。
- 遮蔽物の裏：用途に応じて非表示、半透明、輪郭表示を選ぶ。

Markerの揺れを抑えるためScreen座標へDampingを適用できますが、遅れすぎるとAim情報が不正確になります。

## 39. Warning UI

攻撃予兆は色、形、方向、Timingを組み合わせます。画面外攻撃は端のIndicator、回避Timingは収縮Ring、危険度は音と振動も併用します。

UIだけがGameplayより早い・遅いと不公平になるため、同じAttack TimelineのEventから駆動します。

## 40. Subtitle

- 話者名と本文を分ける。
- 最大行幅、最大行数、表示時間を言語ごとに調整する。
- 背景Panelまたは文字OutlineでContrastを確保する。
- 字幕サイズ、背景Opacity、話者色を設定可能にする。
- 音だけの重要情報には効果音Captionを用意する。

音声時間だけでなく、文字数に基づく最低表示時間を設けます。

## 41. Localization

翻訳後の文章は元言語より大幅に長くなります。

- 固定幅Buttonへ長文を詰め込まない。
- Auto Sizeまたは折返しを使う。
- 単語順をコードで連結せず一文を翻訳Keyにする。
- 数量・性・格変化を単純な文字置換で扱わない。
- 日付、時刻、小数点、桁区切りをLocaleへ合わせる。
- Right-to-left Layoutも将来要件として分離する。

Pseudo Localizationで文字を長くし、未翻訳KeyとLayout破綻を早期検出します。

## 42. Accessibility

- UI Scaleを変更可能にする。
- 色覚Presetと色以外の識別手段を用意する。
- 点滅・画面揺れ・強いBlurを軽減できる。
- Hold操作をToggleへ変更できる。
- Button連打を長押しへ置換できる。
- 字幕、効果音Caption、背景Opacityを調整できる。
- Focus移動順が予測可能である。
- 重要情報を短すぎる時間で消さない。

後付けではなく、View ModelとStyleを分離した設計段階から対応します。

## 43. UI Animation

Animationは情報の変化と因果関係を伝えるために使います。

```cpp
float EaseOutCubic(float t)
{
    t = std::clamp(t, 0.0f, 1.0f);
    const float inverse = 1.0f - t;
    return 1.0f - inverse * inverse * inverse;
}
```

Position、Scale、Opacity、ColorをTrack化し、Unscaled Timeで動かすかGame Timeで停止するかをWidgetごとに決めます。

## 44. View Model

HUDがPlayerやEnemy内部へ直接アクセスすると結合が強くなります。

```cpp
struct CombatHudViewModel final
{
    float playerHealthRatio{};
    float playerEnergyRatio{};
    int comboCount{};
    float comboTimeRatio{};
    bool targetLocked{};
    std::string targetName;
};
```

Gameplay更新後にSnapshotを作り、UIは読み取り専用で描画します。UIを非表示にしてもGameplayが壊れません。

## 45. Dirty Flag

全WidgetのText計測とLayoutを毎Frameやり直す必要はありません。

```cpp
enum class DirtyFlags : unsigned int
{
    None   = 0,
    Layout = 1 << 0,
    Visual = 1 << 1,
    Text   = 1 << 2
};
```

Text・Size・子構造変更時だけLayoutを無効化します。親Sizeが変われば依存する子へ伝播します。

## 46. Draw CallとBatch

UIは小さな画像が多く、Texture切替でDraw Callが増えます。

- IconをTexture Atlasへまとめる。
- 同じTexture・Blend・Clipの描画を近づける。
- 描画順を壊さない範囲でBatchする。
- 見えないWidgetとClip外Widgetを除外する。
- 静的PanelをRender TargetへCacheする。

Batch最優先でZ順を変えると透明合成が壊れます。正しさを保った範囲で最適化します。

## 47. Debug Overlay

- Widget Rect、Anchor、Pivotを描く。
- Safe AreaとDesign Resolution境界を描く。
- Mouse Hit対象とFocus対象を色分けする。
- Clip Rectを表示する。
- TextのBaseline・計測幅を描く。
- Widget数、Draw Call、Font Handle数を表示する。
- 現在の入力Device、言語、UI Scaleを表示する。

Layoutの数値と結果を同時に見られるようにします。

## 48. よくある不具合：位置がずれる

- Design座標とScreen座標を混在させた。
- Mouse座標を逆変換していない。
- AnchorとPivotを取り違えた。
- 親Offsetを二重に加算した。
- Scale後と前で整数丸めを行う場所が違う。
- Letterbox Offsetを忘れた。

座標系ごとに型を分けると混在を減らせます。

## 49. よくある不具合：文字が欠ける

- 対象GlyphをFont Dataへ含めていない。
- 文字コードが不一致。
- Clip Rectが小さい。
- 縁取りを含むBoundsを確保していない。
- Fontが環境へ存在しない。
- UTF-8をbyte途中で切った。

未収録Glyphを目立つ記号で表示し、欠落文字をLogへ記録します。

## 50. よくある不具合：UIがぼやける

- 非整数座標へ描いた。
- 小さい画像を大きく拡大した。
- Filter Modeが用途と違う。
- Render Targetを低解像度で作った。
- PMAとStraight Alphaを混同した。
- 複数回の拡大縮小を行った。

元Asset解像度、最終Pixel位置、Sampler、描画先解像度を順に確認します。

## 51. よくある不具合：Clickできない

- Screen座標とDesign座標のHit Testが混ざった。
- 上の透明Widgetが入力を遮っている。
- Clip外でもHit可能になっている。
- Press開始とRelease先を区別していない。
- Modal背後が入力を受けている。
- MouseとGamepad Focusが競合している。

Hit Test順と入力RoutingをDebug表示します。

## 52. 計測項目

- UI Draw Call数、Texture・Blend・Clip切替数。
- Widget総数、可視数、Hit Test対象数。
- Layout・Text計測・描画のCPU時間。
- Font Handle数とGraph Handle数。
- Text CacheのHit率とMemory量。
- Damage Number等の同時Instance数。
- 解像度変更時のLayout再計算時間。

平均だけでなく、Menuを開いた瞬間や大量Damage表示時の最大値を測ります。

## 53. テスト

- 16:9、16:10、21:9、4:3でLayoutを確認する。
- 最小・最大解像度とWindow Resizeを試す。
- UI Scale最小・最大を試す。
- Keyboard、Mouse、複数種類のGamepadで操作する。
- 長い疑似翻訳、空文字、改行、未収録Glyphを試す。
- Focus対象削除、Modal多重化、Device切替を試す。
- HPの0、最大超過、maximum 0を試す。
- 30、60、120fpsでAnimation時間を比較する。
- 色覚Filter、Grayscale、明暗背景で判読性を確認する。

## 54. 実装チェックリスト

- [ ] Design Resolutionと実画面の変換を一箇所へ集約した。
- [ ] Mouse座標を同じUI座標系へ逆変換した。
- [ ] Anchor、Pivot、Safe Areaを定義した。
- [ ] Font Handleを毎Frame作成していない。
- [ ] Fontの存在・配布条件・Fallbackを確認した。
- [ ] 文字数ではなく描画幅で整列・折返しした。
- [ ] 文字コードと長さの単位を理解した。
- [ ] PMA FontとBlend Modeを一致させた。
- [ ] 色だけに情報を依存していない。
- [ ] MouseとGamepadの両方で操作できる。
- [ ] Modal背後へ入力を通していない。
- [ ] HUDがGameplay内部状態へ直接依存していない。
- [ ] Gauge値を0～1へClampした。
- [ ] 解像度、言語、UI Scale変更でCacheを無効化した。
- [ ] Layout、Clip、FocusをDebug表示できる。
- [ ] UI Draw Callと更新時間を計測した。

## 55. 練習課題

1. 仮想1920×1080の座標を複数解像度へ変換する。
2. 四隅AnchorのPanelを配置する。
3. 9-slice Panelを実装する。
4. 左・中央・右揃えTextを文字幅計測で描く。
5. Font HandleをRAIIとCacheで管理する。
6. Mouse Hover、Press、Clickを持つButtonを作る。
7. Gamepadの方向入力でFocusを移動する。
8. HPの即時Barと遅延Barを作る。
9. Cooldown Iconと残り秒数を表示する。
10. World位置追従のDamage NumberをPool管理する。
11. 画面外Target IndicatorをSafe Area端へ置く。
12. 長い疑似翻訳でMenuを検査する。
13. UI Scale、字幕背景、色覚Presetを設定化する。
14. Widget RectとClipのDebug Overlayを作る。

## 56. 理解確認

1. Design Resolutionを使う理由は何ですか。
2. AnchorとPivotの違いは何ですか。
3. Mouse座標へ逆変換が必要な理由は何ですか。
4. Font Handleを毎Frame作るべきでない理由は何ですか。
5. 文字数だけで中央揃えできない理由は何ですか。
6. UTF-8のbyte数と表示文字数が違う理由は何ですか。
7. PMA FontとBlend方式が一致しないと何が起きますか。
8. 半透明UIの描画順が重要な理由は何ですか。
9. HUD View Modelを挟む利点は何ですか。
10. Dirty Flagが何を節約しますか。

## 57. この章の到達点

- 解像度非依存の座標変換、Anchor、Pivot、Safe Areaを実装できる。
- Fontを安全に生成・読込・計測・解放できる。
- 日本語、文字コード、折返し、Localizationの問題を説明できる。
- MouseとGamepadに対応するFocus・Event・Modalを設計できる。
- HP、Skill、Combo、Damage Number、Target Markerを構築できる。
- Accessibilityと入力Promptを初期設計へ組み込める。
- View Model、Dirty Flag、Batch、Cacheで保守性と性能を両立できる。
- Debug表示とテストでLayout・文字・入力の不具合を切り分けられる。

## 58. 公式リファレンス

- [DXライブラリ：文字描画関係関数](https://dxlib.xsrv.jp/function/dxfunc_graph2.html)
- [DXライブラリ：図形描画関係関数](https://dxlib.xsrv.jp/function/dxfunc_graph0.html)
- [DXライブラリ：関数一覧](https://dxlib.xsrv.jp/dxfunc.html)
- [DXライブラリ：その他の関数](https://dxlib.xsrv.jp/function/dxfunc_other.html)

Font作成引数、文字コード、文字列長の単位、描画状態、戻り値を使用中の公式資料で再確認してください。
