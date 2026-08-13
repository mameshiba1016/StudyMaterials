# 25 Root MotionとMotion Warping

## 1. In-placeとRoot Motion

- In-place：Animation Rootは移動せず、CharacterMovementがCapsuleを動かす。
- Root Motion：Animation Rootの移動量を抽出し、Capsule移動へ反映する。

In-placeは速度をCodeで制御しやすく、Root Motionは踏み込み・回転とPoseを一致させやすい特徴があります。LocomotionはIn-place、攻撃ActionはRoot Motionという混合も一般的です。

## 2. なぜMeshだけ動かしてはいけないか

Animation内でMeshが前進してもCapsuleが残れば、見た目とCollisionが離れ、Animation終了時にMeshがCapsuleへ戻ります。Root Motionを使う目的は、抽出したRoot移動をCharacterMovementへ渡し、Gameplay上の身体も移動させることです。

```text
Animation Root Bone Delta
  ↓ Root Motion抽出
CharacterMovementへ適用
  ↓ Collision Sweep
Capsule移動
  ↓
Mesh PoseとCollisionが同じ身体として進む
```

## 3. Root Motion Mode

AnimBPにはRoot Motionをどこから抽出するかのModeがあります。Networkを考慮する場合、Montageだけから抽出する構成が選ばれることがあります。使用中UEバージョンとReplication要件を確認します。

Root Motion Asset側のRoot Bone、Enable Root Motion、Root Lock設定も結果へ影響します。

## 4. 攻撃距離が固定になる問題

Animationが3m踏み込む場合、敵が1m先でも3m移動しようとします。Collisionで止まる、敵を通り過ぎる、攻撃が届かないといった問題が起きます。

解決候補：

- 攻撃前に距離別Animationを選ぶ。
- Root Motion倍率を調整。
- Root Motion SourceでCode駆動。
- Motion Warpingで目標Transformへ合わせる。

## 5. Motion Warpingの構成

Motion WarpingはMontage内の指定WindowにあるRoot Motionを、名前付きWarp Targetへ到達・対面するよう補正します。

```text
Combat Systemが安全な目標Transformを計算
  ↓
MotionWarpingComponentへWarp Target登録
  ↓
Montage再生
  ↓ Motion Warping Notify Stateの期間
Root Motion Translation／Rotationを補正
  ↓
Window終了時に目標へ整合
```

## 6. ComponentとBuild.cs

```csharp
PrivateDependencyModuleNames.Add("MotionWarping");
```

```cpp
MotionWarpingComponent = CreateDefaultSubobject<UMotionWarpingComponent>(
    TEXT("MotionWarping"));
```

Pluginを有効化し、MontageへMotion Warping Notify StateとWarp Target Nameを設定します。

## 7. Warp Targetを設定する

```cpp
void UCombatComponent::PrepareAttackWarp(const FVector& TargetLocation)
{
    const FVector OwnerLocation = GetOwner()->GetActorLocation();
    FVector ToTarget = TargetLocation - OwnerLocation;
    ToTarget.Z = 0.0f;

    if (ToTarget.IsNearlyZero())
    {
        return;
    }

    const FVector Direction = ToTarget.GetSafeNormal();
    const FVector DesiredLocation = TargetLocation - Direction * DesiredAttackDistance;
    const FRotator DesiredRotation = Direction.Rotation();

    MotionWarpingComponent->AddOrUpdateWarpTargetFromTransform(
        TEXT("AttackTarget"),
        FTransform(DesiredRotation, DesiredLocation));
}
```

API名はEngineバージョンで確認します。重要なのはTarget Actorそのものではなく、攻撃終了時に立つべき安全なTransformを計算して渡すことです。

## 8. 安全なWarp位置

Targetの中心へWarpすると重なります。次を検証します。

- 自分のCapsule半径を含めた距離。
- 壁や段差へのSweep。
- Nav Meshまたは移動可能面。
- TargetのVelocityを考慮するか。
- 高低差と`Ignore ZAxis`方針。
- 最大Warp距離・最大回転角。

WarpはTeleportではなくWindow内のRoot Motionを変形しますが、無制限な補正は足滑りと不自然な加速になります。許容範囲外なら別Actionまたは接近移動へ切り替えます。

## 9. 動くTarget

Warp Targetを毎Frame更新すると追尾性は上がりますが、敵が高速移動すると軌道が曲がり続けます。

- Action開始時にSnapshotする。
- Window前半だけ更新する。
- 位置は更新するが回転補正に上限を設ける。
- 攻撃ごとにTracking StrengthをData化する。

「攻撃が絶対当たる」より、Playerが予測できる追尾規則が重要です。

## 10. Motion WarpingとCombat判定

Warp成功をDamage成立条件にしないでください。DamageはHitbox／Sweepが実際に対象を検出した結果です。Motion Warpingは適切な距離と向きへ移動を補助する層です。

## 11. 中断と後始末

Montage中断、被弾、回避、Character交代時に、古いWarp TargetやModifierを次Actionへ持ち越さないようにします。Action IDで現在要求を識別し、終了時にTargetを削除または更新します。

## 12. ZZZ系高速アクションでの用途

- 通常攻撃の軽い踏み込み。
- Finisherの位置合わせ。
- 支援攻撃で交代Characterを安全位置へ出す。
- Parry後の反撃位置合わせ。
- 大型BossのTarget Pointへ向く。

Cinematicな技でもCamera、敵Collision、複数Player、狭所で成立するか確認します。

## 13. テスト

- Targetが近すぎる／遠すぎる。
- Targetが壁際、段差上、空中にいる。
- Warping中にTargetが死亡する。
- MontageをWindow途中で中断する。
- 低Frame RateとNetwork遅延下で確認。
- 異なる身長へRetargetしたAnimationで距離を比較。
- Root Motionを無効にしたAssetをData Validationで検出。

## 参考

- [Motion Warping](https://dev.epicgames.com/documentation/en-us/unreal-engine/motion-warping-in-unreal-engine)
- [Motion Warping API](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Plugins/MotionWarping)
