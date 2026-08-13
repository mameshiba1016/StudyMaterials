# 07 Component設計とアタッチ階層

## 1. ComponentはActorへ能力を組み込む部品

継承だけで機能を増やすと、`AFireFlyingBossCharacter`のような組み合わせ爆発が起きます。Componentを使えば、体力、戦闘、ロックオン候補、インタラクションなどをActorへ組み込めます。

主要な階層は次の通りです。

```text
UObject
└─ UActorComponent        位置を持たない論理機能
   └─ USceneComponent    Transformとアタッチ階層を持つ
      └─ UPrimitiveComponent  描画、衝突、物理の基盤
```

`UCombatComponent`は位置不要の論理Component、`UCameraComponent`は位置を持つSceneComponent、`UCapsuleComponent`やMesh ComponentはPrimitiveComponent系です。

## 2. 既定Componentを構築する

```cpp
AMyCharacter::AMyCharacter()
{
    CombatComponent = CreateDefaultSubobject<UCombatComponent>(TEXT("CombatComponent"));

    CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
    CameraBoom->SetupAttachment(GetRootComponent());
    CameraBoom->TargetArmLength = 350.0f;

    FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
    FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
}
```

`CreateDefaultSubobject`はコンストラクタでクラスの標準構成を定義するために使います。実行中に自由生成するComponentとは経路が異なります。

## 3. SetupAttachmentとAttachToComponent

- `SetupAttachment`：主にコンストラクタ中、まだ登録されていない既定Componentの親子関係を予約する。
- `AttachToComponent`：登録後、実行中にアタッチ関係を変更する。

```cpp
WeaponMesh->AttachToComponent(
    GetMesh(),
    FAttachmentTransformRules::SnapToTargetNotIncludingScale,
    TEXT("weapon_r"));
```

アタッチルールは結果を大きく変えます。

| ルール | 概要 |
|---|---|
| `KeepRelativeTransform` | 現在の相対Transformを維持 |
| `KeepWorldTransform` | 見た目上のワールドTransformを維持 |
| `SnapToTargetNotIncludingScale` | 親／Socketへ合わせるがScaleは引き継がない |
| `SnapToTargetIncludingScale` | Scaleも含めて合わせる |

武器装備ではSocketへSnap、動く足場への乗車ではワールド位置維持など、目的から選びます。

## 4. 独自ActorComponent

```cpp
UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class MYGAME_API UStaminaComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UStaminaComponent();

    bool TryConsume(float Amount);
    float GetCurrentStamina() const { return CurrentStamina; }

private:
    UPROPERTY(EditDefaultsOnly, Category = "Stamina", meta = (ClampMin = "0.0"))
    float MaxStamina = 100.0f;

    // 実行状態なのでデフォルト編集値とは分離する。
    UPROPERTY(VisibleInstanceOnly, Category = "Stamina")
    float CurrentStamina = 0.0f;
};
```

```cpp
UStaminaComponent::UStaminaComponent()
{
    // 回復をTickで行う設計でなければ無効化する。
    PrimaryComponentTick.bCanEverTick = false;
}

void UStaminaComponent::BeginPlay()
{
    Super::BeginPlay();
    CurrentStamina = MaxStamina;
}

bool UStaminaComponent::TryConsume(float Amount)
{
    // 負数消費による回復など、不正入力を境界で拒否する。
    if (Amount < 0.0f || CurrentStamina < Amount)
    {
        return false;
    }

    CurrentStamina -= Amount;
    return true;
}
```

## 5. Component間の通信

ComponentがOwnerの具体クラスへ毎回Castすると再利用性が落ちます。用途に応じて選びます。

- Ownerが必ず特定型という契約：初期化時に一度検証してキャッシュ。
- 任意Actorへ組み込む能力：Interfaceを要求。
- 状態変化を複数相手へ通知：Delegate。
- 同一Actorの別能力を取得：`FindComponentByClass`を初期化時に使う。
- 疎結合な分類：Gameplay TagやデータID。

```cpp
void UCombatComponent::BeginPlay()
{
    Super::BeginPlay();

    OwnerCharacter = Cast<AMyCharacter>(GetOwner());
    ensureMsgf(OwnerCharacter, TEXT("CombatComponent requires AMyCharacter owner"));
}
```

毎フレーム検索するのではなく、構成が変わらないなら初期化時に取得して契約違反を早期検出します。

## 6. 動的Component

```cpp
UShieldComponent* Shield = NewObject<UShieldComponent>(this, TEXT("RuntimeShield"));
if (Shield)
{
    Shield->RegisterComponent(); // WorldへComponentを登録し、使用可能な状態へ進める。
    AddInstanceComponent(Shield); // ActorのインスタンスComponentとして管理する意図を示す。
}
```

動的生成は`NewObject`だけで完成とは限りません。所有Actor、登録、アタッチ、複製、保存、破棄経路を設計します。固定構成なら既定Subobjectの方がエディタ・Blueprint継承でも扱いやすいです。

## 7. 責任分割の目安

良いComponentは「何でもできる万能箱」ではなく、まとまった能力と明確な公開APIを持ちます。

- `UHealthComponent`：体力、ダメージ適用、死亡通知。
- `UCombatComponent`：攻撃要求、コンボ状態、攻撃データ参照。
- `UTargetingComponent`：候補収集、評価、現在対象。
- `UCharacterPresentationComponent`：VFX／SFX要求の橋渡し。

ただし細分化しすぎて全Componentが互いを知る構造も危険です。状態の唯一の所有者を決め、通知は一方向に流します。

## 8. Tickを持たせる前の質問

1. 入力やイベント発生時だけ処理できないか。
2. Timerで低頻度にできないか。
3. Animation Notifyで正確な時点を通知できないか。
4. 距離に応じて更新頻度を落とせないか。
5. 本当に全インスタンスで必要か。

多数の敵が複数Component Tickを持つと、処理本体が軽くても呼び出し管理コストが積み上がります。

## 参考

- [Actors in Unreal Engine](https://dev.epicgames.com/documentation/unreal-engine/actors-in-unreal-engine)
- [Game Objects in Unreal Engine](https://dev.epicgames.com/documentation/en-us/unreal-engine/game-objects-in-unreal-engine)
