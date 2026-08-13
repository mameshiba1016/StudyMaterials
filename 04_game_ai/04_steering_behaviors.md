# Steering Behavior

Steeringは目標速度・加速度を生成し、滑らかな移動を作ります。Pathfindingが「どこを通るか」、Steeringが「どう動くか」を担当します。

## Seek

```cpp
Vector3 desiredVelocity{NormalizeOrZero(target - position) * maxSpeed};
Vector3 steering{desiredVelocity - velocity};
steering = ClampLength(steering, maxAcceleration);
```

毎tick即座に速度設定するより加速度制限で滑らかになります。

## Arrive

Target付近で減速します。

```text
distance >= slowRadius → maxSpeed
distance <  slowRadius → maxSpeed * distance/slowRadius
distance <= stopRadius → 0
```

停止RadiusとHysteresisで振動を防ぎます。

## Pursuit

Targetの現在位置でなく、速度から未来位置を予測します。

```text
predictionTime = clamp(distance / ownSpeed, 0, maxPrediction)
future = targetPosition + targetVelocity * predictionTime
```

Playerが急旋回する場合に過剰予測しない上限が必要です。

## Separation

近隣Agentから離れる力を加えます。距離0を防ぎ、近隣はSpatial Gridで抽出します。強すぎるとPathから外れ、弱すぎると重なります。

## Obstacle Avoidance

移動方向へ複数Ray/Shape Castし、Hit Normalと左右候補から回避方向を作ります。Local回避だけでは袋小路を解けないためGlobal Pathと組み合わせます。

## Behavior合成

- Weighted Blend：各Steeringを重み付き加算。
- Priority：最重要Behaviorが有効ならそれを使う。
- Arbitration：候補を評価して選ぶ。

SeparationとSeekを単純加算すると相殺・振動するため、Priorityや制限を設計します。

## Facing

速度方向、Target方向、Aim方向のどれへ向くかをStateで選びます。停止時に0速度からRotationを作らず前回Facingを保持します。

## Root Motionとの接続

Steering速度をAnimation Parameterへ渡し、Root Motion結果をCollisionへ通します。Desired速度と実速度の差を次DecisionへFeedbackします。

## Crowd

大量AgentではRVO/ORCA等の相互回避を使えますが、戦闘中はAttack SlotとPosition予約の方が意図した配置を作りやすい場合があります。

## Debug

Desired Velocity、各Steering Vector、近隣、Ray、最終加速度、実速度を色分け表示します。
