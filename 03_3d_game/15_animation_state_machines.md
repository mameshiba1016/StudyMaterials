# Animation State Machine・Blend Tree

Animation State MachineはGameplay状態から表示Poseを選び、遷移・Blendを管理します。Gameplay State Machineと同一にすると、見た目変更が戦闘規則を壊します。

## 分離

```text
Gameplay：Grounded / Airborne / Attack / HitStun
Animation：Idle / Walk / Run / JumpStart / Fall / Attack01
```

Gameplayが「攻撃可能か」を決め、Animationは「どう見せるか」を決めます。Animation Eventだけを権威あるDamage判定にしません。

## Parameter

```cpp
struct AnimationParameters
{
    float planarSpeed{};
    float verticalSpeed{};
    bool isGrounded{};
    bool isAttacking{};
    int attackIndex{};
};
```

World Objectへ自由にアクセスさせず、毎tick作ったSnapshotを渡します。

## Transition

```cpp
struct Transition
{
    StateId destination{};
    float blendSeconds{};
    int priority{};
    bool canInterrupt{};
};
```

複数条件成立時のPriority、遷移中断、最低滞在時間、Exit Timeを定義します。遷移を配列順へ偶然依存させません。

## Exit Time

Clipの正規化時刻が閾値へ達したら遷移します。Loop Clipの何周目か、Play Rate、Blend中のSource時間を扱います。Attack終了はGameplay tickと同期させ、Animationだけ遅れても操作不能時間が変化しないようにします。

## Blend Tree

一つ以上のParameterから複数Clip Weightを求めます。

- 1D：SpeedでIdle/Walk/Run。
- 2D Cartesian：前後左右速度。
- Directional：角度と速度。

Weight合計を1へ正規化し、入力点の外側をClamp/Extrapolateする規則を決めます。

## Locomotion

Camera-relative Local VelocityをCharacter Localへ変換し、Blend Treeへ渡します。World X/Zをそのまま使うとCharacter回転でAnimation方向がずれます。

## Layer

```text
Base Layer：Locomotion
Upper Body：Aim / Attack
Additive：Breathing / Recoil
Facial：表情
```

Bone MaskとWeightで合成します。Layer順序が結果を変えます。

## Sync Marker

WalkとRunを単純な正規化時刻でBlendすると左右足がずれます。LeftFootDown、RightFootDown等のMarker Phaseを合わせます。

## State Behavior

Enter/Exit callbackでGameplay Objectを破棄・遷移させるとAnimation Graphと強結合します。通知をEvent Queueへ積み、安全なGame Phaseで処理します。

## Update順

```text
Gameplay Fixed Update
→ Animation Parameter Snapshot
→ Graph評価
→ Root Motion抽出
→ Character移動・衝突との調停
→ IK/Procedural Pass
→ Skinning Palette
```

Root MotionをPhysics前後どこへ適用するかを固定します。

## Debug

現在State、Source/Target、Blend Weight、Clip時間、Parameter、Layer Weight、Marker、遷移拒否理由を表示します。Graphを一Frameずつ進めます。
