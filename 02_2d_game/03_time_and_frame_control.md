# 2Dゲームの時間・フレーム制御

時間処理は速度、アニメーション、入力受付、物理、ヒットストップへ影響します。FPS依存コードは、PC性能やVSync設定でゲーム速度が変わります。

## フレーム依存の誤り

```cpp
position.x += 5.0F; // 一フレーム5ピクセル。FPSが倍なら一秒の移動距離も倍。
```

速度を「一秒あたり」で定義します。

```cpp
constexpr float speedPixelsPerSecond{300.0F};
position.x += speedPixelsPerSecond * deltaSeconds;
```

変数名へ単位を含めると、秒とミリ秒の混同を防ぎやすくなります。

## deltaの計測

```cpp
using Clock = std::chrono::steady_clock;

Clock::time_point previous{Clock::now()};

while (running)
{
    const Clock::time_point current{Clock::now()};
    const std::chrono::duration<double> elapsed{current - previous};
    previous = current;

    const double rawDeltaSeconds{elapsed.count()};
}
```

経過時間には単調増加する`steady_clock`を使います。

## deltaクランプ

ブレークポイントやウィンドウ移動後に2秒のdeltaを一度に渡すと、キャラクターが壁を貫通します。

```cpp
constexpr double maximumDeltaSeconds{0.1};
const double deltaSeconds{std::min(rawDeltaSeconds, maximumDeltaSeconds)};
```

クランプは失われた時間をどう扱うかという仕様判断です。オンライン同期や音楽ゲームでは単純に捨てられない場合があります。

## 固定更新

```cpp
constexpr double fixedDeltaSeconds{1.0 / 60.0};
double accumulator{};

accumulator += frameDeltaSeconds;

while (accumulator >= fixedDeltaSeconds)
{
    previousState = currentState;
    Simulate(fixedDeltaSeconds);
    accumulator -= fixedDeltaSeconds;
}

const double interpolationAlpha{accumulator / fixedDeltaSeconds};
RenderInterpolate(previousState, currentState, interpolationAlpha);
```

物理・ゲームルールを一定刻みで進め、描画だけ補間します。補間は過去と現在の確定状態間を描くため、表示は一tick分遅れる代わり滑らかになります。

## 可変更新が向くもの

UIアニメーション、純粋な視覚エフェクト、カメラの一部など、決定性や衝突へ影響しない表示処理は可変deltaで十分な場合があります。すべてを固定更新へ入れると不要な計算が増えます。

## タイムスケール

```cpp
const double scaledDelta{unscaledDelta * gameTimeScale};
```

- `gameTimeScale = 0`：ポーズ。
- `0 < scale < 1`：スロー。
- `scale > 1`：早送り。

UI、ポーズメニュー、ネットワーク、ロード進捗は非スケール時間を使うことがあります。物理エンジンへ極端に小さいdeltaを渡すより、更新停止や専用スロー表現が安定する場合があります。

## ヒットストップ

攻撃命中時に短時間、戦闘世界の進行を停止・減速し打撃感を出します。

```cpp
class HitStopController
{
public:
    void Request(double durationSeconds)
    {
        remainingSeconds_ = std::max(remainingSeconds_, durationSeconds);
    }

    bool Update(double unscaledDeltaSeconds)
    {
        remainingSeconds_ = std::max(0.0, remainingSeconds_ - unscaledDeltaSeconds);
        return remainingSeconds_ > 0.0;
    }

private:
    double remainingSeconds_{};
};
```

停止対象を明確にします。キャラクターと敵は止めても、命中エフェクト、カメラシェイク、UIは動かすといったレイヤー分けが必要です。

## タイマー

```cpp
remainingCooldownSeconds = std::max(
    0.0,
    remainingCooldownSeconds - deltaSeconds
);

if (remainingCooldownSeconds <= 0.0)
{
    canAttack = true;
}
```

`== 0`ではなく範囲で判定します。多数タイマーを毎フレーム減算する方式、期限時刻を保存する方式、タイマーホイールなどを規模で選びます。

## フレーム番号

アクションゲームでは「攻撃開始から5tick目」のように固定tickで受付・判定を管理すると再現しやすくなります。ただし表示FPSのフレームとシミュレーションtickを混同しません。調整データに秒とtickが混在する場合は変換規則を一つにします。

## FPS表示

一フレームの逆数だけを表示すると激しく変動します。一定期間のフレーム数、移動平均、中央値を使います。性能評価ではFPSよりミリ秒、最大値、1% low、CPU/GPU区間を見る方が原因を理解しやすいです。
