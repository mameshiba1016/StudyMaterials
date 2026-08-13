# スプライトアニメーション

スプライトアニメーションは、時間に応じて表示する画像領域を切り替えます。見た目だけでなく、攻撃判定、足音、状態遷移との同期が必要です。

## クリップデータ

```cpp
struct AnimationFrame
{
    Rect sourcePixels{};
    double durationSeconds{};
};

struct AnimationClip
{
    std::vector<AnimationFrame> frames{};
    bool loops{true};
};
```

全フレーム同じ時間とは限りません。溜めを長く、命中瞬間を短くするなど、フレームごとのdurationを持てます。

## 再生状態

```cpp
class AnimationPlayer
{
public:
    void Play(const AnimationClip& clip, bool restartIfSame);
    void Update(double deltaSeconds);
    [[nodiscard]] const AnimationFrame& GetCurrentFrame() const;
    [[nodiscard]] bool IsFinished() const;

private:
    const AnimationClip* clip_{nullptr}; // Asset側がPlayerより長生きする契約。
    std::size_t frameIndex_{};
    double elapsedInFrameSeconds_{};
    bool isFinished_{};
};
```

`clip_`の寿命を保証できないなら、安定ハンドルや共有Asset管理を使います。

## フレーム進行

```cpp
void AnimationPlayer::Update(double deltaSeconds)
{
    if (clip_ == nullptr || clip_->frames.empty() || isFinished_)
    {
        return;
    }

    elapsedInFrameSeconds_ += deltaSeconds;

    // 大きなdeltaで複数フレーム分進む可能性があるためifではなくwhile。
    while (elapsedInFrameSeconds_ >= clip_->frames[frameIndex_].durationSeconds)
    {
        elapsedInFrameSeconds_ -= clip_->frames[frameIndex_].durationSeconds;
        ++frameIndex_;

        if (frameIndex_ >= clip_->frames.size())
        {
            if (clip_->loops)
            {
                frameIndex_ = 0;
            }
            else
            {
                frameIndex_ = clip_->frames.size() - 1;
                isFinished_ = true;
                break;
            }
        }
    }
}
```

durationが0以下だと無限ループになるため、Assetロード時に検証します。一フレームの最大遷移数を守る防御も考えられます。

## クリップ切替

毎フレーム`Play("Run")`を呼んで無条件再始動すると、常に最初のフレームになります。同じクリップなら継続し、状態が変わった瞬間だけ切り替えます。

```cpp
if (desiredClip != currentClip)
{
    animationPlayer.Play(desiredClip, true);
}
```

## アニメーション状態とゲーム状態

表示アニメーションをゲームルールの唯一の状態にすると、見た目変更で判定が壊れます。

```text
Gameplay State：Grounded / Airborne / Attack / HitStun
Animation State：Idle / Run / JumpRise / Attack01 / Hurt
```

ゲーム状態から表示クリップを選びつつ、攻撃の有効期間は明確な戦闘データと固定tickで管理します。小規模ゲームではフレームイベントと同期してもよいですが、データ契約を明示します。

## アニメーションイベント

特定時刻で足音、攻撃判定開始、VFX生成を通知します。

```cpp
struct AnimationEvent
{
    double timeSeconds{};
    AnimationEventType type{};
};
```

前回時刻から今回時刻の間に跨いだ全イベントを発火します。ループ境界、逆再生、速度変更、大きなdelta、スキップ時を扱います。名前文字列だけのイベントは誤字を検出しづらいため、型付きIDやロード検証を使います。

## 左右反転

同じ画像をX反転して左右移動に使えます。描画Origin、攻撃判定、武器ソケット、エフェクト方向も反転します。

```cpp
float facingSign{facing == Facing::Right ? 1.0F : -1.0F};
localHitBox.center.x *= facingSign;
```

ワールド座標を直接反転するのではなく、キャラクターローカルデータをFacingで変換します。

## Sprite SheetとAtlas

複数フレームを一画像へ詰めます。固定グリッドなら計算でRectを得られますが、トリミングAtlasでは元サイズ、Pivot、回転格納、余白のメタデータが必要です。隣画像の色混入を防ぐextrusionも重要です。

## 時間倍率

アニメーション速度を`playbackRate`で変えられます。

```cpp
elapsed += deltaSeconds * playbackRate;
```

攻撃アニメーションだけ速める場合、当たり判定タイミングも同じ時間軸へ追従させるかを決めます。見た目だけ速度変更すると判定とずれます。

## デバッグ

- 現在クリップ名、フレーム番号、経過時間を表示。
- 一時停止、一フレーム送り、速度変更。
- Pivot、Socket、HitBoxを重ねて表示。
- イベント発火ログ。

アニメーション調整ツールがあると、コード再ビルドせずデザイナーがタイミングを調整できます。
