# 18 Chaos物理、Force、Impulse、Ragdoll

## 1. Gameplay移動と物理Simulationを区別する

UE5の物理基盤はChaosです。Rigid Body、Constraint、Ragdoll、Destruction、Clothなどを扱います。しかし、すべてのゲーム移動を物理Simulationへ任せる必要はありません。

- Character：通常はCharacterMovementが運動を管理。
- 箱や破片：Primitive ComponentのPhysics Simulation。
- Knockback：CharacterのLaunch、Root Motion、独自状態など仕様に応じて選択。
- 死亡Ragdoll：Skeletal MeshのPhysics AssetをSimulation。

位置の責任者を同時に複数持たせないことが基本です。

## 2. Physics Simulation

```cpp
UStaticMeshComponent* Mesh = GetStaticMeshComponent();
Mesh->SetSimulatePhysics(true);
Mesh->SetEnableGravity(true);
Mesh->SetLinearDamping(0.2f);
Mesh->SetAngularDamping(0.5f);
```

Simulationには適切なCollision形状、Mobility、Mass、Collision Enabledが必要です。見た目Meshがあるだけでは安定した物理Bodyになりません。

## 3. ForceとImpulse

```cpp
// 継続的な力。通常は物理更新に合わせて繰り返し与える。
Mesh->AddForce(ForceVector);

// 瞬間的な運動量変化。爆発や打撃に向く。
Mesh->AddImpulse(ImpulseVector);
```

- Forceは時間を通して加速させる。
- Impulseは瞬間的にVelocityを変化させる。
- `bVelChange`相当の指定ではMassの影響を無視する選択もある。

攻撃ごとにMass依存で吹き飛ばしたいか、全対象を一定速度変化させたいかを仕様として決めます。

## 4. Torqueと回転

```cpp
Mesh->AddTorqueInRadians(TorqueVector);
Mesh->AddAngularImpulseInRadians(AngularImpulse);
```

単位がDegreesかRadiansか、ForceかImpulseかを関数名から確認します。巨大な値を試行錯誤する前に、Mass、慣性Tensor、Scale、Delta Timeを確認します。

## 5. Characterの吹き飛ばし

```cpp
const FVector LaunchVelocity = HitDirection.GetSafeNormal2D() * HorizontalSpeed
    + FVector::UpVector * VerticalSpeed;

LaunchCharacter(LaunchVelocity, true, true);
```

CharacterをPhysics Bodyとして直接Impulseするより、CharacterMovementの経路に合う`LaunchCharacter`等を使う方が床・移動Modeと統合しやすい場合があります。ただし攻撃中のRoot Motion、壁衝突、空中受身、Network Predictionとの規則を設計します。

## 6. Ragdollへの移行

```cpp
void AEnemyCharacter::EnterRagdoll()
{
    GetCharacterMovement()->DisableMovement();
    GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    USkeletalMeshComponent* CharacterMesh = GetMesh();
    CharacterMesh->SetCollisionProfileName(TEXT("Ragdoll"));
    CharacterMesh->SetSimulatePhysics(true);
}
```

実際には次も必要です。

- Physics AssetのBodyとConstraint調整。
- CapsuleとMeshのCollision競合防止。
- CameraやTarget Lock解除。
- Network上のAuthorityと同期方針。
- 復帰するならPelvis位置から安全なCapsule位置を検索。
- 遠距離Ragdollの更新頻度・停止・回収。

## 7. Physical Animation

完全Ragdollではなく、Animation Poseへ物理Bodyを追従させるPhysical Animationで、被弾時の揺れや部分物理を作れます。Gameplay判定のHurtboxまで不安定な物理Poseへ従わせるかは慎重に決めます。

## 8. Substeppingと時間刻み

Frameが長いと、高速物体が衝突を飛び越えたりConstraintが不安定になったりします。Physics Substepは1Frameを小さい時間刻みに分けて安定性を改善しますが、計算回数が増えます。

固定Step、可変Step、最大Substep数、Network Physicsの設定はプロジェクト要件とEngineバージョンで確認します。Render Frameの`DeltaSeconds`だけを前提に物理Callbackを設計しません。

## 9. Physical Material

Physical Materialには摩擦、反発、表面Typeなどを定義できます。Traceの`FHitResult`から表面を判定し、足音、着地VFX、打撃音を選択できます。

```cpp
if (UPhysicalMaterial* PhysMat = Hit.PhysMaterial.Get())
{
    const EPhysicalSurface Surface = UPhysicalMaterial::DetermineSurfaceType(PhysMat);
    PlayImpactForSurface(Surface, Hit.ImpactPoint, Hit.ImpactNormal);
}
```

Query ParametersでPhysical Material返却を要求しているか確認します。

## 10. 高速アクションで物理を使う境界

物理に向くもの：破片、装飾、小物、死亡後Ragdoll、部分的な揺れ。

決定的なGameplay規則として慎重に扱うもの：Combo成立、無敵Frame、正確な攻撃移動、敵の強制配置、Camera。

「物理的に自然」と「操作として予測可能」は一致しない場合があります。Gameplayの結果を先に決め、物理は反応と演出へ使う設計も有効です。

## 11. デバッグと性能

- Collision形状とCenter of Massを可視化。
- Mass、Velocity、Sleeping状態を記録。
- Chaos Visual Debuggerで物理Sceneを記録・再生。
- 大量破片はSleep／Disable／寿命で管理。
- Ragdollを全骨・全距離で常時計算しない。
- Network Physicsは遅延・補正・再Simulationを実機条件で確認。

## 参考

- [Physics in Unreal Engine](https://dev.epicgames.com/documentation/unreal-engine/physics-in-unreal-engine)
- [Physics Settings](https://dev.epicgames.com/documentation/en-us/unreal-engine/physics-settings-in-the-unreal-engine-project-settings)
