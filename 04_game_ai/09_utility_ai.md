# Utility AI

Utility AIは行動候補ごとにScoreを計算し、状況に最も適した行動を選びます。多数の連続条件を柔軟に調整できます。

## Consideration

```cpp
struct Consideration
{
    float weight{1.0F};
    ResponseCurve curve{};
    ContextValueSource source{};
};
```

距離、HP、Cooldown、味方数等を0～1へ正規化し、CurveでScoreへ変換します。

## Response Curve

- Linear。
- Inverse Linear。
- Exponential。
- Logistic。
- Piecewise Curve。

距離の単位値を直接混ぜず、意味ある範囲へNormalizeします。

## 合成

```text
score = baseScore * c1 * c2 * ...
```

乗算では一つ0で候補を除外できますが、Consideration数が多い行動ほど不利になります。Compensation、幾何平均、加算、Gate条件を使い分けます。

## 候補例

```text
LightAttack：近距離、Cooldown済み、Tokenあり
HeavyAttack：中距離、Player隙大、低頻度
Dodge      ：Incoming threat、Staminaあり
Retreat    ：低HP、近距離、退路あり
```

実行不可能条件はScore計算前のFilterにします。

## Hysteresis

毎tick最高Scoreへ切り替えると行動が震えます。現在行動へBonus、最小実行時間、切替Thresholdを設けます。

## Randomness

上位候補から重み付き選択、Scoreへ小さいNoiseを加える等があります。固定Seedと安定候補順で再現可能にします。低Scoreの不合理行動が選ばれない範囲へ制限します。

## Commitment

選択後はCharacter ActionがCancel可能になるまで再Decisionしません。Utility AIがAnimation/Combat Stateを毎tick上書きしないようにします。

## UtilityとBT

Utilityで高水準Actionを選び、各Action内部をBT/State Machineで実行できます。両方が同じ決定を競合しない責任分担にします。

## Debug

全候補の最終Score、各Consideration入力/Curve出力、Filter理由、選択履歴を表示します。調整時に最重要の機能です。
