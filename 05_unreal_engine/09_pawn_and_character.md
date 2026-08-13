# 09 PawnとCharacterの設計

## 1. Actor、Pawn、Character

```text
AActor
└─ APawn                 PlayerまたはAIが操作できる世界上の主体
   └─ ACharacter         直立型キャラクター向け機能を備えたPawn
```

`APawn`は操作可能な主体の基底です。人型とは限らず、車、ドローン、砲台にも使えます。`ACharacter`はCapsule Collision、Skeletal Mesh、`UCharacterMovementComponent`などを標準で持つ、直立型キャラクター向けのPawnです。

高速な人型3Dアクションなら、まず`ACharacter`を土台にするのが合理的です。ただし独自物理の乗り物や特殊な非人型移動では`APawn`＋独自Movement Componentが適することがあります。

## 2. Characterの基本構成

```cpp
UCLASS()
class MYGAME_API AActionCharacter : public ACharacter
{
    GENERATED_BODY()

public:
    AActionCharacter();

protected:
    virtual void BeginPlay() override;

private:
    UPROPERTY(VisibleAnywhere, Category = "Combat")
    TObjectPtr<UCombatComponent> CombatComponent;
};
```

```cpp
AActionCharacter::AActionCharacter()
{
    PrimaryActorTick.bCanEverTick = false;

    CombatComponent = CreateDefaultSubobject<UCombatComponent>(TEXT("CombatComponent"));

    // ControllerのYawをActorへ直接適用せず、移動方向へ向ける例。
    bUseControllerRotationYaw = false;
    GetCharacterMovement()->bOrientRotationToMovement = true;
    GetCharacterMovement()->RotationRate = FRotator(0.0f, 720.0f, 0.0f);
    GetCharacterMovement()->MaxWalkSpeed = 600.0f;
}
```

## 3. AddMovementInputの内部的な意味

```cpp
void AActionCharacter::Move(const FVector2D& Input)
{
    const FRotator ControlRotation = GetControlRotation();
    const FRotator YawOnly(0.0f, ControlRotation.Yaw, 0.0f);

    const FVector Forward = FRotationMatrix(YawOnly).GetUnitAxis(EAxis::X);
    const FVector Right = FRotationMatrix(YawOnly).GetUnitAxis(EAxis::Y);

    AddMovementInput(Forward, Input.Y);
    AddMovementInput(Right, Input.X);
}
```

`AddMovementInput`はその場でActor位置を直接変更する命令ではなく、Pawnへ移動入力ベクトルを蓄積します。`CharacterMovementComponent`がその入力、加速度、摩擦、重力、床判定、移動Mode等を使って移動を計算します。

入力処理と位置更新を分離することで、AI、ネットワーク予測、Movement Mode、衝突応答へつながります。

## 4. 操作方向とキャラクター方向

アクションゲームでは少なくとも3方向を区別します。

- **カメラ方向**：プレイヤーが見ている方向。
- **移動方向**：入力から求めた進行方向。
- **注視／攻撃方向**：ロックオン対象や照準へ向く方向。

非ロックオン時は移動方向へ回転し、ロックオン中は対象へ向きながら横移動する、といった状態別方針が必要です。`bOrientRotationToMovement`と`bUseControllerRotationYaw`を場当たり的に両方切り替えるのではなく、回転Policyを1か所で管理します。

## 5. CharacterMovementを直接上書きしすぎない

```cpp
UCharacterMovementComponent* Movement = GetCharacterMovement();
Movement->MaxWalkSpeed = bIsSprinting ? SprintSpeed : WalkSpeed;
```

通常移動の速度設定はMovement Componentへ渡します。一方、回避や攻撃移動では次の選択肢があります。

- Movement入力と一時的な速度／加速度変更。
- Root Motion Montage。
- Root Motion Source。
- LaunchやImpulse。
- 独自Movement Mode。

毎フレーム`SetActorLocation`でCharacterMovementの結果を上書きすると、床、段差、衝突、ネットワーク予測を壊しやすくなります。

## 6. PawnのPossess状態

```cpp
void AActionCharacter::PossessedBy(AController* NewController)
{
    Super::PossessedBy(NewController);
    // Controllerが確定した後のサーバー側初期化など。
}

void AActionCharacter::UnPossessed()
{
    // 入力主体を失う前後で、継続中アクションを安全に整理する。
    CombatComponent->CancelActions(EActionCancelReason::Unpossessed);
    Super::UnPossessed();
}
```

SpawnしたPawnが常に自動でPossessされるとは限りません。レベル配置時のAuto Possess設定、GameModeのPlayer Spawn、AI Controller設定、手動`Possess`など、誰がいつ操作権を与えるかを明確にします。

## 7. キャラクター交代へつながる設計

キャラクター交代では、PlayerControllerを維持し、操作対象Pawnを切り替える構成が基本候補になります。

```text
PlayerController（プレイヤーの意思・入力・カメラ方針）
        ↓ Possess
Active Character（現在の肉体・移動・戦闘）

Party System（編成、交代Cooldown、控え状態）
```

入力マッピング、HUD、カメラ、ロックオンをCharacterへ完全に閉じ込めると、交代ごとに作り直しや状態移送が増えます。一方、攻撃中の状態、体力、Animationは各Character側に置くべきです。寿命と責任から配置を決めます。

## 8. Characterに詰め込みすぎない

Character本体の役割は、Componentの組み立て、Pawn／Characterライフサイクルの仲介、身体に直結する操作の窓口に絞ります。

避けたい状態：

- 入力、UI、全攻撃データ、AI判断、Save、VFX生成を1クラスへ集約。
- Animation BlueprintがCharacterのprivate状態を大量に直接変更。
- 敵とPlayerで同じ能力なのに別実装。

共通能力はComponentやInterface、設定差はData Asset、操作主体差はControllerへ分離します。

## 参考

- [Pawn in Unreal Engine](https://dev.epicgames.com/documentation/en-us/unreal-engine/pawn-in-unreal-engine)
- [Gameplay Framework Quick Reference](https://dev.epicgames.com/documentation/unreal-engine/gameplay-framework-quick-reference-in-unreal-engine)
