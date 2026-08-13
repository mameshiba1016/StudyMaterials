# 知覚・視野・記憶

PerceptionはWorldの事実をAIが知る情報へ変換します。TargetのTransformを直接読むだけでは、遮蔽・音・記憶を表せません。

## 視覚

候補抽出後、距離、視野角、遮蔽を検査します。

```cpp
const Vector3 toTarget{targetPosition - eyePosition};
const float distanceSquared{LengthSquared(toTarget)};
const Vector3 direction{NormalizeOrZero(toTarget)};
const bool insideAngle{Dot(forward, direction) >= fieldOfViewCosine};
```

`acos`せずcos閾値を使います。最終的にEyeから複数Target PointへRaycastします。中心一点だけでは細い遮蔽物で不自然になります。

## Candidate抽出

全Entityを比較せずSpatial Query、Faction、Targetable Layerで候補を減らします。視覚更新をFrame分散します。

## 聴覚

```cpp
struct SoundStimulus
{
    Vector3 position{};
    float loudness{};
    SoundStimulusType type{};
    EntityHandle source{};
    int emittedTick{};
};
```

距離減衰、壁、音Type、AI聴力から検知します。Audio波形を解析する必要はなく、Gameplay Eventとして発行します。

## Damage知覚

見えていない相手から被弾した場合、攻撃方向やHit位置を記憶できます。ただし正確なPlayer位置を自動取得するかは難易度仕様です。

## Memory

```cpp
struct TargetMemory
{
    EntityHandle target{};
    Vector3 lastKnownPosition{};
    Vector3 lastKnownVelocity{};
    int lastSeenTick{};
    float confidence{};
};
```

見失った瞬間にIdleへ戻らず、最後の位置へ移動・探索します。Confidenceを時間で減衰させます。

## 視界のHysteresis

発見角度と見失う角度、発見時間と喪失時間を別にし、境界で見える/見えないが毎tick反転するのを防ぎます。

## Awareness段階

```text
Unaware → Suspicious → Alerted → Combat → Searching
```

刺激強度を蓄積・減衰し、段階ごとにAnimation、音、味方通知を変えます。

## Faction

Self、Ally、Enemy、NeutralをRelation Tableで判定します。Team番号の等否だけではCharmや一時同盟へ対応しづらくなります。

## 共有情報

味方AIへTarget情報を伝える場合、距離、通信Delay、Alert範囲を設けます。一体が見た瞬間にMap全員が正確な位置を知る動作を避けます。

## 忘却と無効Handle

Target死亡・破棄後は世代付きHandleの有効性を確認し、Memoryを終了します。最後の位置情報は残せてもEntity参照は使いません。

## Debug

FOV Cone、Ray、Stimulus、Memory位置、Confidence、Awareness Meter、情報共有線を表示します。
