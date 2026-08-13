# 19 Character Movementと特殊移動

## 1. CharacterMovementが位置更新の責任者

`UCharacterMovementComponent`は入力からAccelerationとVelocityを計算し、床、段差、斜面、重力、衝突、Movement Modeを考慮してCharacterを移動させます。`ACharacter`と一体でNetwork予測・補正にも対応します。

```text
AddMovementInput
  ↓ Pending Input Vector
CharacterMovement::TickComponent
  ↓ Acceleration・Velocity計算
PerformMovement
  ↓ Movement Mode別のPhysWalking／PhysFalling等
SafeMoveUpdatedComponent
  ↓ Sweep・Slide・床判定
Characterの最終Transform
```

この処理の後から毎Frame`SetActorLocation`で上書きすると、床判定やNetwork Smoothingと競合します。

## 2. 基本パラメータ

```cpp
UCharacterMovementComponent* Movement = GetCharacterMovement();

Movement->MaxWalkSpeed = 600.0f;          // 地上最高速度。
Movement->MaxAcceleration = 2400.0f;      // 目標速度へ近づく加速度。
Movement->BrakingDecelerationWalking = 2000.0f; // 入力がない時の減速。
Movement->GroundFriction = 8.0f;          // 地上方向転換・減速に影響。
Movement->AirControl = 0.35f;             // 空中での操作量。
Movement->GravityScale = 1.5f;            // Characterへ掛かる重力倍率。
Movement->RotationRate = FRotator(0.0f, 720.0f, 0.0f);
```

最高速度だけで操作感は決まりません。Acceleration、Friction、Braking、Rotation Rate、入力Curveをセットで調整します。

## 3. Movement Mode

標準Modeは`MOVE_Walking`、`MOVE_Falling`、`MOVE_Swimming`、`MOVE_Flying`、`MOVE_None`等です。Modeが変わると使用される物理計算も変わります。

```cpp
void AActionCharacter::OnMovementModeChanged(
    EMovementMode PreviousMovementMode,
    uint8 PreviousCustomMode)
{
    Super::OnMovementModeChanged(PreviousMovementMode, PreviousCustomMode);

    const UCharacterMovementComponent* Movement = GetCharacterMovement();
    if (Movement->IsFalling())
    {
        CombatComponent->NotifyEnteredAir();
    }
}
```

空中状態を独自boolだけで二重管理するとMovement Modeとずれます。Engine状態を真実とし、Combat側には必要な状態遷移を通知します。

## 4. 回避移動の選択肢

回避には複数の実装方法があります。

| 方法 | 長所 | 注意点 |
|---|---|---|
| Movement入力＋速度変更 | 標準移動と統合しやすい | 軌道の厳密制御が弱い |
| `LaunchCharacter` | 簡単な瞬間移動速度 | Falling移行や床処理を確認 |
| Root Motion Montage | Animationと軌道を一致 | 中断・Network・障害物対応 |
| Root Motion Source | Code駆動とMovement統合 | 学習と実装量が増える |
| Custom Movement Mode | 独自物理を完全制御 | Network予測まで含めると高度 |

無敵時間は移動方法と同一視せずCombat Stateが管理します。壁に止められても無敵だけ残るかなどを仕様化します。

## 5. Root Motion

Root MotionはAnimation Root Boneの移動をCharacter移動へ反映します。攻撃踏み込みと見た目を一致させやすい一方、Animation Assetが距離のAuthorityになります。

```text
AnimationからRoot Motion抽出
  ↓
CharacterMovementがWorld Spaceの移動へ変換
  ↓
Collisionを考慮してCharacterを移動
```

攻撃ごとに速度倍率を場当たり的に変更するとAnimationと判定がずれます。Motion Warping、Root Motion Source、攻撃Dataの移動定義など、目標位置へ適応する層を設けます。

## 6. Custom Movement Mode

`MOVE_Custom`中は標準Modeの物理を止め、`CustomMovementMode`の値で壁走り、特殊Dash等を分けられます。

```cpp
UENUM()
enum class ECustomMovementMode : uint8
{
    None,
    Dash,
    WallRun
};

Movement->SetMovementMode(
    MOVE_Custom,
    static_cast<uint8>(ECustomMovementMode::Dash));
```

独自`UCharacterMovementComponent`で`PhysCustom`をOverrideし、`DeltaTime`、残りSimulation Iteration、Collision、Mode終了条件を処理します。マルチプレイでは入力Flag、保存Move、Server検証、Correctionまで設計が必要です。

## 7. Network Movementの概念

ローカルPlayerは入力後すぐ予測移動し、ServerへMoveを送り、Server結果と違えば補正します。Remote Characterは受信間隔より描画Frameが多いためNetwork Smoothingで滑らかに表示します。

```text
Autonomous Proxy: 入力 → Client予測 → Server検証 → 必要なら補正
Authority:        正式なMovement Simulation
Simulated Proxy:  受信状態を補間して表示
```

Teleportを通常移動として補間させる、Clientだけ速度を変える、独自Dash情報をServerへ送らない、といった実装はずれの原因です。

## 8. 高速アクション向け状態分離

```text
Movement State: Walking / Falling / CustomDash / Disabled
Combat State:   Idle / Attacking / Dodging / Stunned
Animation State: Locomotion / Montage / Blend
```

これらは関係しますが同一ではありません。攻撃中でも歩ける、空中でも攻撃できる、Stun中はMovementを止める、といった組み合わせをRuleで制御します。

## 9. デバッグ項目

- Velocity、Acceleration、Movement Mode、床Normalを画面表示。
- Capsule SweepとStep Upを可視化。
- 30／60／120fpsで移動距離と回避猶予を比較。
- 斜面、階段、壁際、動く床で検証。
- Root Motion Montageを途中中断。
- Network遅延・Packet Loss下でDashと交代を確認。

## 参考

- [Movement Components](https://dev.epicgames.com/documentation/en-us/unreal-engine/movement-components-in-unreal-engine)
- [Networked Character Movement](https://dev.epicgames.com/documentation/en-us/unreal-engine/understanding-networked-movement-in-the-character-movement-component-for-unreal-engine)
