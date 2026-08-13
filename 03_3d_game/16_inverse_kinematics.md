# IK・Procedural Animation

Forward Kinematicsは親から子へTransformを計算します。Inverse Kinematicsは手・足などEnd Effectorの目標から、途中Joint回転を求めます。

## Two Bone IK

Upper Arm、Lower Arm、Hand、またはThigh、Shin、Footに使います。

入力：

- Root位置。
- Bone長2本。
- Target位置。
- Bend方向を決めるPole Vector。

Target距離を到達可能範囲へClampし、余弦定理からJoint角を求めます。TargetがRootと同位置、完全伸展、Poleが軸と平行な特異状態を処理します。

## CCD

EndからRoot方向へ各Jointを順に回し、EffectorをTargetへ近づける反復法です。任意Chainへ使えますが、Iteration数、収束閾値、Joint制限が必要です。

```text
for iteration:
  for joint from end-parent to root:
    currentDirection = effector - joint
    targetDirection  = target - joint
    jointをFrom-To回転
```

## FABRIK

Joint位置を前向き・後向きPassでTargetとRootへ拘束し、Bone長を保ちます。Position解決後にRotationへ変換します。Joint Limitは追加処理が必要です。

## Foot IK

各足の予測位置から下へRay/Sphere Castし、地面Hit位置とNormalを得ます。

```text
足Target = hitPoint + normal * soleOffset
足Rotation = ground normalへ整列
Pelvis Offset = 両足の必要下げ量から決定
```

階段端でRayが外れる、足が急に切り替わる問題へSphere Cast、履歴平滑化、最大補正速度を使います。

## Foot Lock

接地中の足World位置を保持し、Root移動による滑りを減らします。Animation Curveで足接地Weightを持ち、離地時に解除します。Character Teleport時はLockをResetします。

## Aim IK

Spine、Chest、Neck、Headへ目標回転を分配します。各BoneのYaw/Pitch制限、Weight、ねじれを設定します。Targetが背後ならCharacter全体のTurnへ移行します。

## Hand IK

Weaponを主手へAttachし、副手をWeapon SocketへIKします。両手が互いを循環参照しない所有順序にします。

```text
Character Pose
→ Weaponを主手Socketへ
→ Weapon副手Targetを取得
→ 副腕IK
```

## Look At

Eye/Head/Spineへ回転を分配し、最大角度と追従速度を設けます。Target切替時にSlerpし、0距離やUp平行を処理します。

## Joint Limit

人体として不可能な曲げを防ぎます。Swing-Twist分解、Cone制限、Euler制限等があります。QuaternionをEulerへ往復して不連続を起こさないようにします。

## IK適用順

```text
Animation Blend
→ Root/Body補正
→ Foot IK
→ Aim/Hand IK
→ Secondary Motion
→ Final Pose
```

後段が前段の結果を上書きします。Layerと優先順位を設計します。

## Gameplayとの境界

IKで手が敵へ届いてもHit Boxの権威位置をどうするか決めます。見た目だけ補正するか、Gameplay SocketもFinal Poseから取得するかで遅延・決定性が変わります。

## LOD

遠距離CharacterではIKを停止・低頻度化します。復帰時にWeightをBlendしてPoseの跳びを防ぎます。
