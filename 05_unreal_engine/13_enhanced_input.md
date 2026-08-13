# 13 Enhanced Inputの構成と内部の流れ

## 1. キーではなくActionへ反応する

ゲームコードが「Spaceキー」ではなく「Jump Action」を受け取るようにします。

```text
物理入力（キー、ボタン、Stick）
  ↓ Input Mapping Context
Input Modifier（値を加工）
  ↓
Input Trigger（成立条件を評価）
  ↓
Input Action（Move、Attack、Dodgeという意味）
  ↓ Binding
C++／Blueprintの処理
```

これによりKeyboard、Gamepad、キー変更、長押し、同時押し、状況別操作を、ゲームロジックから分離できます。

## 2. 4つの中心概念

- `UInputAction`：プレイヤーが行う意味。値型はBool、Axis1D、Axis2D、Axis3D等。
- `UInputMappingContext`：物理入力とActionの対応をまとめたContext。
- Modifier：Dead Zone、反転、Scale、Swizzle等の値加工。
- Trigger：Pressed、Hold、Tap、Chord等の成立条件。

`IA_Attack`を「Mouse Left」に固定するのではなく、`IMC_Gameplay`内で対応付けます。

## 3. Build.csとヘッダ

```csharp
PrivateDependencyModuleNames.AddRange(new string[]
{
    "EnhancedInput"
});
```

```cpp
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputMappingContext.h"
```

ヘッダでポインタ宣言だけなら前方宣言を使い、`.cpp`で必要な定義をincludeすると依存を減らせます。

## 4. Mapping ContextをLocal Playerへ追加する

```cpp
void APartyPlayerController::BeginPlay()
{
    Super::BeginPlay();

    ULocalPlayer* LocalPlayer = GetLocalPlayer();
    if (!LocalPlayer || !GameplayMappingContext)
    {
        return;
    }

    UEnhancedInputLocalPlayerSubsystem* InputSubsystem =
        LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();

    if (InputSubsystem)
    {
        // 数値が高いContextほど優先される。
        InputSubsystem->AddMappingContext(GameplayMappingContext, 0);
    }
}
```

Mapping ContextはLocal Playerへ適用されます。サーバー上の全Playerへ一括設定するものではありません。UI、通常戦闘、乗り物、デバッグなどをContextとして追加・削除できます。

## 5. ActionをBindingする

```cpp
void AActionCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);

    UEnhancedInputComponent* EnhancedInput =
        CastChecked<UEnhancedInputComponent>(PlayerInputComponent);

    EnhancedInput->BindAction(
        MoveAction,
        ETriggerEvent::Triggered,
        this,
        &AActionCharacter::HandleMove);

    EnhancedInput->BindAction(
        AttackAction,
        ETriggerEvent::Started,
        this,
        &AActionCharacter::HandleAttackPressed);
}
```

```cpp
void AActionCharacter::HandleMove(const FInputActionValue& Value)
{
    const FVector2D Movement = Value.Get<FVector2D>();
    Move(Movement);
}
```

Actionの値型と`Get<T>()`の型を一致させます。不一致を曖昧な変換で隠さないでください。

## 6. Trigger Eventを区別する

| Event | 概要 |
|---|---|
| `Started` | 評価が開始した瞬間 |
| `Ongoing` | まだ成立していない継続評価中 |
| `Triggered` | Trigger条件が成立 |
| `Completed` | 成立後に終了 |
| `Canceled` | 成立せず中断 |

単発攻撃は`Started`または適切なPressed Trigger、移動Axisは`Triggered`、Chargeは開始・継続・完了を使い分けます。すべて`Triggered`へ接続すると、押下1回のつもりが複数Frame呼ばれることがあります。

## 7. ModifierとTrigger

移動Actionでは、WASDの1次元入力を2次元へ並べ替えるModifier、Stickの微小入力を除くDead Zoneを設定できます。長押し回避やTap／Holdの分岐はTriggerで表現できます。

ただし重要な戦闘規則をInput Triggerだけへ閉じ込めないでください。「Stamina不足」「現在硬直中」「回避Cancel可能」はCombat SystemがAuthorityを持って検証します。Inputは意図を生成する層です。

## 8. Contextの優先順位

```text
IMC_Common      Priority 0   常時操作
IMC_Combat      Priority 10  戦闘操作
IMC_Menu        Priority 100 UI操作
```

同じ物理入力が競合する場合、高Priority Contextが優先され得ます。Contextを追加したまま解除し忘れると、復帰後に入力不能や二重発火が起きます。追加したOwnerが解除責任も持つ設計にします。

## 9. デバッグ

- `showdebug enhancedinput`でActionやMapping状態を確認。
- 現在追加中のContextとPriorityを表示。
- ActionのTrigger Eventと値を一時ログ出力。
- GamepadのDead Zone、反転、感度を実機確認。
- UI表示中、Pause中、Possess直後、キャラクター交代時を検証。

## 参考

- [Enhanced Input](https://dev.epicgames.com/documentation/en-us/unreal-engine/enhanced-input-in-unreal-engine)
- [Enhanced Input API](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Plugins/EnhancedInput)
