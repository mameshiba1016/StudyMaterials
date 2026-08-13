# Rigid Body Physics・Solver・固定Step

Rigid Bodyは変形しない物体の位置・回転・Linear/Angular Velocity、Mass、InertiaをSimulationします。

## Body Type

- Static：動かず無限質量。地形。
- Dynamic：ForceとCollisionで動く。
- Kinematic：Gameplayが目標Transformを与え、他へ影響する。

Static Bodyを毎Frame移動せず、動く床はKinematicとして扱います。

## 状態

```cpp
struct RigidBodyState
{
    Vector3 position{};
    Quaternion rotation{};
    Vector3 linearVelocity{};
    Vector3 angularVelocity{};
    float inverseMass{};
    Matrix3 inverseInertiaLocal{};
};
```

Mass 0で除算する代わりStaticはInverse Mass 0として扱えます。

## ForceとImpulse

- Force：時間にわたり加速。`Δv = F * inverseMass * dt`。
- Impulse：瞬間的運動量変化。`Δv = J * inverseMass`。
- Torque/Angular Impulse：回転へ作用。

Jumpは一定時間ForceよりImpulse/Velocity設定が制御しやすい場合があります。

## Inertia Tensor

回転しにくさは形状と質量分布で軸ごとに異なります。Local Inverse InertiaをRotationでWorldへ変換します。Center of Massから離れた点へのImpulseはTorqueを生みます。

```text
angularImpulse = cross(contactPoint - centerOfMass, impulse)
```

## 固定Step

```cpp
accumulator += frameDelta;
while (accumulator >= fixedDelta)
{
    PhysicsStep(fixedDelta);
    accumulator -= fixedDelta;
}
```

可変dtはConstraint安定性と再現性を悪化させます。最大Substep数でSpiral of Deathを防ぎます。

## Stepの概略

```text
Force積分
→ Broad Phase
→ Narrow Phase / Contact生成
→ Island構築
→ Constraint Solver反復
→ Velocity/Position積分
→ Sleep判定
→ Event生成
```

Engineにより順序と手法は異なります。

## Impulse Solver

接触Normal方向の相対速度を止め、Restitutionで反発させるImpulseを求めます。負方向へ引き寄せるImpulseを作らないようClampします。

Frictionは接線方向ImpulseをCoulomb制限内へClampします。

```text
|frictionImpulse| <= frictionCoefficient * normalImpulse
```

## Iterative Solver

Constraintを複数回反復し近似解を得ます。Iteration数を増やすと安定性は上がるがCPU Costも増えます。Stackした箱、Mass比、Joint数で必要量が変わります。

## Position Correction

PenetrationをVelocity Bias、Split Impulse、Position Projection等で修正します。全Depthを一度に押し戻すとJitterやEnergy注入が起きるため、Slopと割合を使います。

## RestitutionとFriction

Material組合せから値を決めます。平均、最小、最大、乗算等のCombine Ruleを固定します。低速接触でRestitutionを無効化し微振動を防ぎます。

## Joint

Fixed、Hinge、Slider、Distance、Cone Twist等がBody間の自由度を制約します。Limit、Motor、Break Forceを持ちます。Constraintの循環と大きなMass比は不安定要因です。

## Sleeping

速度が小さいBodyを停止扱いにして計算を省きます。Force、接触、Kinematic接近でWakeします。GameplayがTransformを直接変更した場合もWakeとBroad Phase更新が必要です。

## Determinism

Pair順、Island順、Floating Point、Thread実行で結果が変わります。同一Machineでも完全決定性を保証しないLibraryがあります。NetworkではServer権威、State同期、Prediction/Reconciliation等を使います。

## Characterとの分離

操作CharacterをDynamic Bodyだけで作ると坂・段差・押し合いで制御が難しくなります。専用Kinematic Character ControllerとDynamic Objectの相互作用を設計します。

## Event

Physics callback中にBodyを削除せず、Enter/Stay/ExitやHitをQueueへ積みStep後に処理します。同じContactから毎Step SEを鳴らさずImpact速度とCooldownで制限します。

## Debug・計測

Center of Mass、Velocity、Inertia軸、Contact、Constraint、Island、Sleep、CCD、Solver反復時間を表示します。
