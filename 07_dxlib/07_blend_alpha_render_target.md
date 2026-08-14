# 第7章 Blend・Alpha・Render Target

この章では、半透明・発光・暗転・画面合成を扱います。Blendは「画像を透明にする機能」だけではなく、これから描く色（Source）と、既に描かれている色（Destination）をどの式で合成するかというGPUの状態です。Render Targetは、その計算結果をどの画像へ書くかを表します。

> TextureとHandleの寿命は第6章、座標・描画順・描画範囲は第5章を前提とします。

## 1. SourceとDestination

- Source: これから描こうとしているSpriteの色。
- Destination: 描画先に既に入っている色。
- Output: Blend計算後、描画先へ書かれる色。

```text
Output = Sourceに係数を掛けた値 + Destinationに係数を掛けた値
```

係数と演算を変えることでAlpha、加算、減算などを作ります。

## 2. 色成分の範囲

説明ではRGBとAlphaを0～1で扱います。8bit値なら0～255を255で割った値です。

```cpp
[[nodiscard]] constexpr float Normalize8(int value) noexcept
{
    return static_cast<float>(value) / 255.0F;
}
```

実際のFramebuffer形式、丸め、Color Spaceによって結果は完全には一致しません。

## 3. 通常のAlpha Blend

代表的な式は次です。

```text
Output.rgb = Source.rgb × Source.a
           + Destination.rgb × (1 - Source.a)
```

Alphaが1ならSource、0ならDestination、0.5なら両者が半分ずつ混ざります。

## 4. `SetDrawBlendMode`

```cpp
SetDrawBlendMode(DX_BLENDMODE_ALPHA, 128);
DrawGraph(100, 100, effectHandle, TRUE);

// 以後の描画へ状態を漏らさない。
SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
```

`Pal`は0～255で、255へ近いほど描画側が濃くなります。画像自身のAlphaと組み合わさるため、常に単純な「50%」になるとは限りません。

## 5. Texture Alphaと全体Alpha

Texture Alphaは画素ごと、Blendの`Pal`は描画全体へ作用する強さです。輪郭は0、内部は1というTextureへ`Pal=128`を指定すれば、形状を保ったまま全体を薄くできます。

## 6. 透過Flag

`DrawGraph(..., TRUE)` の透過指定と `SetDrawBlendMode` は役割が異なります。前者は画像の透明部分を扱う指定、後者は描画先との合成方式です。どちらか一方だけを覚えると、黒い矩形や不透明表示の原因になります。

## 7. Straight Alpha

Straight AlphaではRGBが透明度と独立して保存されます。半透明赤なら概念上 `(1, 0, 0, 0.5)` です。描画時にRGBへAlphaを掛けます。

## 8. Premultiplied Alpha

Premultiplied Alpha（PMA）はRGBへあらかじめAlphaを掛けて保存します。同じ半透明赤は `(0.5, 0, 0, 0.5)` です。

```text
PMA Output.rgb = Source.rgb + Destination.rgb × (1 - Source.a)
```

Texture形式とBlend Modeを一致させないと、暗い縁や過剰な明るさが出ます。DXライブラリには`DX_BLENDMODE_PMA_ALPHA`等があります。

## 9. Alpha Fringe

透明画素のRGB、Texture Filtering、Straight/PMAの不一致により輪郭へ黒・白の縁が出ます。

- Export設定とBlend Modeを一致させる。
- 透明画素周辺へ適切な色をPaddingする。
- Atlasの隣領域から色が混ざらないよう余白を置く。
- 拡大縮小時のFilteringを確認する。

## 10. 加算Blend

```text
Output.rgb = Source.rgb × Alpha + Destination.rgb
```

```cpp
SetDrawBlendMode(DX_BLENDMODE_ADD, 220);
DrawGraph(x, y, glowHandle, TRUE);
SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
```

炎、火花、発光、Energy表現に向きます。重ねるほど明るく飽和しやすく、背景が明るいと見えにくくなります。

## 11. 減算Blend

DestinationからSourceの明るさを引く暗い表現です。公式資料では表画面への減算描画に注意があり、裏画面を描画先にするよう示されています。

```cpp
SetDrawScreen(DX_SCREEN_BACK);
SetDrawBlendMode(DX_BLENDMODE_SUB, 180);
DrawGraph(x, y, darkEffectHandle, TRUE);
SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
```

## 12. 乗算Blend

```text
Output.rgb = Source.rgb × Destination.rgb
```

白は変化を与えず、黒へ近いほど暗くします。影、汚れ、暗転Tintに使えます。乗算だけで物理的に正しいLightingになるわけではありません。

## 13. 反転系Blend

背景色との関係で色を反転させる表現です。照準や選択Cursorなど、背景を問わず差を出したい場合がありますが、画面が激しく変化すると視認性も変動します。

## 14. SaturationとHDRの予習

8bit描画先では1を超えた加算結果が最大値へClampされ、色の差が失われます。HDR Render Targetでは1を超える輝度を保持し、後でTone MappingやBloomへ使えます。DXライブラリで利用する形式は生成設定と公式仕様を確認します。

## 15. GammaとLinear Color

sRGB値をそのまま足し算すると、人間の明るさ知覚と物理光量の両方に対して不自然になる場合があります。本格的なPipelineではTextureをLinearへDecodeし、Lighting・Blend後にsRGBへEncodeします。DirectX編でColor Spaceをさらに掘り下げます。

## 16. 描画順が結果を変える

通常Alpha Blendは順序交換できません。

```text
Aを描いてBを描く結果 ≠ Bを描いてAを描く結果
```

透明Spriteは原則として奥から手前へ描きます。加算は順序の影響が比較的小さい場合がありますが、他の状態やClampを含めて無条件に同一とは決めません。

## 17. OpaqueとTransparentを分ける

```text
不透明背景・地形
→ 不透明Character
→ 半透明Effect（奥から手前）
→ UI
→ Debug
```

不透明物は順序最適化しやすく、半透明物は見た目の順序制約が強いという違いがあります。

## 18. Blend StateはGlobal State

`SetDrawBlendMode`は後続描画へ残ります。関数内の早期returnや新しい分岐で復元が飛ばされると、関係のないHUDまで薄くなります。

## 19. Scopeで復元する

```cpp
class ScopedBlendMode final
{
public:
    ScopedBlendMode(int mode, int parameter)
    {
        // この簡易版は復元先を既定値とする契約。
        SetDrawBlendMode(mode, parameter);
    }

    ~ScopedBlendMode()
    {
        SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
    }

    ScopedBlendMode(const ScopedBlendMode&) = delete;
    ScopedBlendMode& operator=(const ScopedBlendMode&) = delete;
};

void DrawGlow(int handle, int x, int y)
{
    const ScopedBlendMode blend{DX_BLENDMODE_ADD, 220};
    DrawGraph(x, y, handle, TRUE);
}
```

入れ子が必要なら以前の状態をRenderContextがStackへ保存します。

## 20. Explicit State Cache

```cpp
struct BlendState final
{
    int mode = DX_BLENDMODE_NOBLEND;
    int parameter = 0;
};

class RenderStateCache final
{
public:
    void SetBlend(BlendState next)
    {
        if (next.mode == current_.mode &&
            next.parameter == current_.parameter) return;

        SetDrawBlendMode(next.mode, next.parameter);
        current_ = next;
    }

private:
    BlendState current_{};
};
```

無駄な状態変更を省けますが、DXライブラリをCache外から直接変更すると記録と実状態がずれます。状態変更の入口を一つにします。

## 21. 描画輝度 `SetDrawBright`

```cpp
SetDrawBright(255, 80, 80);
DrawGraph(x, y, characterHandle, TRUE);
SetDrawBright(255, 255, 255);
```

各RGB成分は0～255で、255が100%です。100%以上へ増幅する機能ではありません。Damage Flash等へ使えますが、後続描画へ戻し忘れないでください。

## 22. TintとAlphaを分ける

```cpp
struct SpriteColor final
{
    Rgb8 tint{255, 255, 255};
    std::uint8_t opacity = 255;
};
```

TintはRGB乗算、OpacityはBlend量です。同じ「薄く見える」結果でも意味が違います。

## 23. Render Targetとは

GPUが描画結果を書き込む画像です。通常は裏画面ですが、`MakeScreen`で描画可能画像を作り、そこへ一度描いてから別の画像として合成できます。Off-screen Bufferとも呼びます。

## 24. `MakeScreen`

```cpp
const int sceneTarget = MakeScreen(1280, 720, TRUE);
if (sceneTarget == -1)
{
    // サイズ、形式、GPU能力、Memory不足などを調査。
    return -1;
}
```

第3引数でAlpha Channelの有無を指定する形があります。作成設定の正確な意味は利用中バージョンの宣言を確認します。

## 25. Render TargetもHandle資源

`MakeScreen`の戻り値も最後に`DeleteGraph`します。第6章のMove-only RAIIを再利用できます。ただし通常Textureと「描画可能Texture」の用途差をMetadataに残します。

```cpp
enum class GraphUsage { SampleOnly, RenderTarget };
```

## 26. 描画先の切替

```cpp
SetDrawScreen(sceneTarget);
ClearDrawScreen();
DrawWorld();

SetDrawScreen(DX_SCREEN_BACK);
DrawGraph(0, 0, sceneTarget, FALSE);
```

`SetDrawScreen`後は描画範囲や3D Camera設定にも影響するため、Passごとに必要状態を設定します。

## 27. Render Pass

```cpp
struct RenderPass final
{
    int target = DX_SCREEN_BACK;
    Rgb8 clearColor{0, 0, 0};
    bool clear = true;
};

void BeginPass(const RenderPass& pass, int width, int height)
{
    SetDrawScreen(pass.target);
    SetDrawArea(0, 0, width, height);
    SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
    SetDrawBright(255, 255, 255);

    if (pass.clear)
    {
        SetBackgroundColor(pass.clearColor.r,
                           pass.clearColor.g,
                           pass.clearColor.b);
        ClearDrawScreen();
    }
}
```

Pass開始時に既知の状態へ揃えると、前Passの漏洩を減らせます。

## 28. Clearの意味

前Frameの内容を使わないTargetは必ずClearします。Clearしないと残像のような未定義の履歴が見える場合があります。一方、意図的なFeedback表現では前Frameを入力にしますが、別Targetを交互に使う方が安全です。

## 29. Alpha付きTargetのClear

透明な中間画像を作る場合、RGBだけでなくAlphaの初期値も重要です。`ClearDrawScreen`がAlphaをどう初期化するか、関連設定を公式仕様で確認します。不透明黒 `(0,0,0,1)` と透明黒 `(0,0,0,0)` は後の合成結果が異なります。

## 30. Feedback Loopを避ける

同じTextureを同時に「読み込み元」と「書き込み先」にする処理はGPU上で競合します。

```text
Target Aを読む → Target Bへ書く
次FrameはTarget Bを読む → Target Aへ書く
```

この交互利用をPing-Pong Bufferと呼びます。

## 31. Scene合成

```text
World Pass → worldTarget
Effect Pass → effectTarget
UI Pass → uiTarget
Composite Pass → DX_SCREEN_BACK
```

分けることでWorldだけ暗くし、UIは明るいままにするなどの制御ができます。ただしTarget数とMemory、切替Costが増えます。

## 32. 画面Fade

最も単純なFade Outは全描画後に半透明黒矩形を重ねます。

```cpp
void DrawFade(float normalized)
{
    const float t = std::clamp(normalized, 0.0F, 1.0F);
    const int alpha = static_cast<int>(t * 255.0F);
    const ScopedBlendMode blend{DX_BLENDMODE_ALPHA, alpha};
    DrawBox(0, 0, 1280, 720, GetColor(0, 0, 0), TRUE);
}
```

`<algorithm>`が必要です。UIもFadeさせるかは描画順で決まります。

## 33. Hit Flash

```cpp
const float flash = 1.0F - elapsed / duration;
const int greenBlue = static_cast<int>((1.0F - flash) * 255.0F);

SetDrawBright(255, greenBlue, greenBlue);
DrawCharacter();
SetDrawBright(255, 255, 255);
```

Simulationの無敵時間と表示時間を別の値として持ちます。点滅が判定そのものを決めてはいけません。

## 34. Ghost Trail（残像）

過去のTransformをRing Bufferへ保存し、古いものほどAlphaを下げて描きます。同じ現在位置を複数回描くだけでは残像になりません。

```cpp
struct GhostSample final
{
    Vec2 position{};
    float rotation = 0.0F;
    float age = 0.0F;
};
```

Animation Frameも保存するか、現在Frameを使うかで見え方が変わります。

## 35. Glowの多段合成

概念的なBloomは明部抽出、縮小、Blur、加算合成で作ります。

```text
Scene → Bright Extract Target
→ Horizontal Blur Target
→ Vertical Blur Target
→ Sceneへ加算
```

DXライブラリのGraph Filter系APIを利用する方法と、Shaderで実装する方法があります。

## 36. Blurの分離

2D Gaussian Blurを縦横に分けると、`N×N` Sampleを概ね`N+N` Sampleへ減らせます。この発想はDirectXのPost Processでも重要です。

## 37. 低解像度Effect Target

GlowやBlurは半分・4分の1解像度で処理すると高速化でき、自然にぼけます。ただし細部消失、座標変換、端のSampling、拡大時の品質を確認します。

## 38. Mini-map

Worldの簡略表現を小さいRender Targetへ描き、UIとして配置できます。Main Cameraの描画をそのまま縮小するか、専用Camera・専用Layerで描くかを選びます。

## 39. Damage Overlay

画面端の赤いVignetteはUI用TextureをAlpha合成します。HP比率をOpacityへ直接線形Mappingすると常時うるさくなるため、Curve、閾値、Pulseを設計します。

## 40. Pause Background

Pause直前のSceneをRender Targetへ保持し、暗転やBlurをかけ、その上にMenuを描けます。毎Frame停止Sceneを再描画する必要がない一方、Window Resize時の再生成を考えます。

## 41. Render Target Resize

Windowや論理解像度が変わったらTarget Sizeも再検討します。

```text
新SizeのTargetを一時作成
→ 成功確認
→ Pass参照を交換
→ GPUが古いTargetを使用中でない安全な時点で解放
```

失敗時は古いTargetを維持し、いきなり表示不能にしません。

## 42. Memory概算

RGBA8の1280×720 Targetは概算`1280×720×4 ≒ 3.52 MiB`です。Scene Color、Effect、UI、Ping-Pongを複数持つと加算されます。Depth、MSAA、HDR形式はさらに増えます。

## 43. MSAA付きTarget

`SetCreateDrawValidGraphMultiSample`等の設定を`MakeScreen`前に行う方式があります。MSAA TargetはSample数に応じてMemoryとResolve処理が増えます。2D Spriteだけで必要かを画質とCostで判断します。

## 44. Resolveの予習

Multi-sample Targetは、そのまま通常TextureとしてSampleできず、単一Sample画像へResolveが必要なPipelineがあります。DXライブラリがどこまで自動処理するかを公式仕様で確認し、DirectX編では明示的に扱います。

## 45. `GetDrawScreenGraph`

現在の描画先領域をグラフィックへ取得するAPIがあります。Screenshotや一時Captureに使えますが、GPUからCPUへのReadbackを伴う使い方は同期Costが高くなり得ます。毎Frame安易にCaptureせず計測します。

## 46. Custom Blendの予習

`SetDrawCustomBlendMode`ではSource係数、Destination係数、演算を細かく指定できます。

```text
SrcFactor = SRC_ALPHA
DstFactor = INV_SRC_ALPHA
Operation = ADD
```

これは通常Alpha Blendの基本です。RGBとAlpha Channelで別の式を指定できるため、結果Alphaの意味も設計します。

## 47. Alpha Channelの保存式

RGBが正しく見えても、Render TargetのAlphaが不適切だと次の合成で破綻します。「最終画面で見えるか」だけでなく、中間TargetのRGBAを何として定義するか決めます。

## 48. Render Graphの考え方

```cpp
struct PassDependency final
{
    const char* passName = nullptr;
    std::vector<const char*> reads{};
    std::vector<const char*> writes{};
};
```

各Passが何を読み、何へ書くかを列挙すると、生成順、Target寿命、Feedback Loopを検出しやすくなります。

## 49. 一時Target Pool

同じSize・形式の中間TargetをEffectごとに永続作成せず、寿命が重ならないPass間で再利用できます。ただし使用中Targetの貸し出し、Frame境界、Resize、Device再生成を管理する必要があります。

## 50. State Sortingの限界

同じBlend ModeやTextureをまとめると状態変更を減らせます。しかし透明描画順を壊してはいけません。

```text
正しさを保つSort Key
= Pass → Layer → Depth/Order → 交換可能範囲内のState
```

## 51. Effect Data

```cpp
enum class BlendKind { Opaque, Alpha, Additive, Multiply };

struct EffectRenderData final
{
    int textureHandle = -1;
    Vec2 position{};
    float rotation = 0.0F;
    float scale = 1.0F;
    std::uint8_t opacity = 255;
    BlendKind blend = BlendKind::Alpha;
    DrawLayer layer = DrawLayer::Effect;
};
```

Game ObjectがDX定数を直接持たず、Renderer境界で変換するとBackend交換が容易です。

## 52. Blend変換

```cpp
[[nodiscard]] int ToDxBlendMode(BlendKind kind)
{
    switch (kind)
    {
    case BlendKind::Opaque: return DX_BLENDMODE_NOBLEND;
    case BlendKind::Alpha: return DX_BLENDMODE_ALPHA;
    case BlendKind::Additive: return DX_BLENDMODE_ADD;
    case BlendKind::Multiply: return DX_BLENDMODE_MULA;
    }
    return DX_BLENDMODE_NOBLEND;
}
```

定数名は利用中DXライブラリのHeaderを正としてください。版やAPI例で表記差がある場合は実環境に合わせます。

## 53. Render Command実行

```cpp
void ExecuteEffect(const EffectRenderData& data)
{
    const int mode = ToDxBlendMode(data.blend);
    const int parameter = (data.blend == BlendKind::Opaque)
        ? 0 : data.opacity;

    SetDrawBlendMode(mode, parameter);
    DrawRotaGraph(RoundToPixel(data.position.x),
                  RoundToPixel(data.position.y),
                  data.scale, data.rotation,
                  data.textureHandle, TRUE);
    SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
}
```

## 54. Debug表示

現在のPass名、Target Handle、Size、Blend Mode、描画Command数、Target Memory概算を画面に表示すると、状態漏洩と過剰な中間画像を発見しやすくなります。

## 55. Blend確認画面

同じSpriteを暗背景・明背景・色背景へ、Opaque、Alpha、Add、Sub、Multiply、PMAで並べて表示します。Effectは背景依存なので、透明格子だけで判断しません。

## 56. よくある不具合: 全部半透明

Blend Modeを既定へ戻していない可能性が最優先です。Renderer外からの直接状態変更、早期return、Pass開始時の初期化不足を確認します。

## 57. よくある不具合: 黒い縁

Straight/PMA不一致、透明画素RGB、Atlas Padding、Filteringを確認します。Blend parameterだけを変えても根本解決しません。

## 58. よくある不具合: Effectが白飛び

加算Spriteの重ね過ぎ、Alpha過大、8bit TargetのClamp、Texture RGBが既に明るすぎる可能性があります。Layer数、Opacity、Target形式を調べます。

## 59. よくある不具合: Targetが真っ黒

描画先切替後にClearしただけで描画していない、元Targetへ戻していない、Targetを透明Flag付きで描く契約が違う、描画範囲が古い、Handle生成失敗を確認します。

## 60. よくある不具合: 前Frameが残る

TargetをClearしていない、Clear対象を切替前後で間違えた、意図せずFeedbackしている可能性があります。各Pass冒頭でTarget名とClear有無をLogします。

## 61. よくある不具合: UIまで暗い

WorldとUIの合成順を確認します。World Targetへ暗転を掛けてからUIを描けばUIは明るいまま、UI後に全画面矩形を描けばUIも暗くなります。どちらが仕様か決めます。

## 62. 性能計測

- Pass数とTarget切替回数。
- Blend Mode・Texture切替回数。
- 各Passの描画Command数。
- TargetのSize・形式・Memory概算。
- Blur Sample数と解像度。
- Screenshot/Readback回数。
- CPU提出時間とGPU時間。

見た目を変える最適化は比較画像も保存します。

## 63. Pure FunctionのTest

```cpp
struct FloatRgb { float r, g, b; };

[[nodiscard]] constexpr FloatRgb AlphaBlend(
    FloatRgb source, FloatRgb destination, float alpha) noexcept
{
    return {source.r * alpha + destination.r * (1.0F - alpha),
            source.g * alpha + destination.g * (1.0F - alpha),
            source.b * alpha + destination.b * (1.0F - alpha)};
}

constexpr FloatRgb mixed = AlphaBlend({1, 0, 0}, {0, 0, 1}, 0.5F);
static_assert(mixed.r == 0.5F && mixed.b == 0.5F);
```

実GPU結果との差ではなく、設計した式とParameter変換の検証に使います。

## 64. 1FrameのPass例

```text
Begin WorldTarget（Clear）
→ Opaque World
→ Alpha Character
→ Additive Effect
Begin BackBuffer（Clear）
→ WorldTargetを描画
→ Damage Overlay
→ UI
→ Fade
→ Debug
→ ScreenFlip
```

## 65. 設計チェックリスト

- [ ] Source、Destination、Outputを説明できる。
- [ ] Texture Alphaと全体Opacityを区別した。
- [ ] Straight AlphaとPMAをBlend Modeへ一致させた。
- [ ] 半透明の描画順を仕様化した。
- [ ] BlendとBrightを必ず復元する。
- [ ] Pass開始時に必要状態を明示する。
- [ ] Render TargetをDX終了前に一度だけ破棄する。
- [ ] Targetを必要な値でClearする。
- [ ] 同じTargetを同時に読み書きしない。
- [ ] WorldとUIのEffect範囲を分けた。
- [ ] Resize失敗時に旧Targetを維持する。
- [ ] Target MemoryとPass Costを計測する。

## 66. 理解確認問題

1. Source Alphaが0の通常Alpha Blend結果は何か。
2. Straight AlphaとPMAの半透明赤のRGBはどう違うか。
3. 加算Effectが明背景で見えにくい理由は何か。
4. 半透明Spriteを無条件にTexture順へSortできない理由は何か。
5. `SetDrawBlendMode`を戻さないと何が起きるか。
6. Render TargetをClearしない場合に何が見えるか。
7. 同じTargetを読み書きしてはいけない理由は何か。
8. UIを暗転させないPass順はどうなるか。
9. 低解像度Blurの利点と欠点は何か。
10. 1280×720 RGBA8 Targetの概算Memoryはいくらか。

## 67. 実践課題

1. Blend Mode比較画面を暗・明・色背景で作る。
2. `ScopedBlendMode`とRender State Cacheを実装する。
3. Alpha付きTextureのStraight/PMA不一致を再現し修正する。
4. Worldを`MakeScreen`へ描き、裏画面へ合成する。
5. UIを除外できるFadeと含めるFadeを作る。
6. 過去Transformを使った残像を実装する。
7. 半解像度Targetを用いたGlowを作る。
8. Pause時のScene Captureと暗転Menuを作る。
9. Target Memoryと状態変更回数をDebug表示する。
10. Resize時にTargetを安全交換する。

## 68. 公式資料

- [DXライブラリ グラフィック関連関数](https://dxlib.xsrv.jp/function/dxfunc_graph1.html)
- [DXライブラリ その他関数・Custom Blend](https://dxlib.xsrv.jp/function/dxfunc_other.html)
- [DXライブラリ 関数リファレンス一覧](https://dxlib.xsrv.jp/dxfunc.html)

定数名、Alpha付きTargetのClear、PMA画像のLoad、Custom Blendの各係数、MSAA Targetの制約は使用中バージョンの公式資料とHeaderを正としてください。

## 69. 次章への接続

画像を読み、状態を切り替え、複数Targetへ描き、最後に合成する流れができました。次章ではSound・Music・Voiceを、Handle寿命、同時再生、Volume、Pitch、Category、Streaming、優先度というAudio Systemとして設計します。
