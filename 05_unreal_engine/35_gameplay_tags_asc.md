# 35 Gameplay TagとASCの所有設計

## 1. Gameplay Tag

Gameplay Tagは階層化された意味ラベルです。

```text
State.Attacking
State.Dodging
State.Stunned
Ability.Attack.Light
Ability.Attack.Heavy
Event.Combat.Hit
Data.Damage.Fire
```

`State.Attacking.Light01`は親`State.Attacking`にも一致させられます。単なる文字列比較ではありません。

## 2. Native Tag定義

```cpp
// Header
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_Stunned);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Ability_Attack_Light);

// Source
UE_DEFINE_GAMEPLAY_TAG_COMMENT(
    TAG_State_Stunned,
    "State.Stunned",
    "Actor cannot execute normal actions while stunned");
```

頻繁にCode参照するTagはNative定義し、毎回文字列からRequestしません。Tag Dictionaryへ登録された正式Tagを使います。

## 3. ContainerとQuery

```cpp
FGameplayTagContainer RequiredTags;
RequiredTags.AddTag(TAG_State_InCombat);

if (AbilitySystem->HasMatchingGameplayTag(TAG_State_Stunned))
{
    return false;
}
```

複雑なAny／All／None条件は`FGameplayTagQuery`でData化できます。大量boolの組合せをTagへ置き換えられますが、数値や寿命をTagだけで表現しません。

## 4. Tag Stack

複数Effectが同じTagを付ける場合、一方が消えても他方が残る必要があります。ASCはEffect由来TagをCountとして扱います。独自にTagを追加・削除する場合もSource重複を壊さない方法を使います。

単一bool無敵より`State.Invulnerable`のCountが、回避と必殺演出の重なりに強い理由です。

## 5. AbilityのTag関係

- Activation Required：必要Tag。
- Activation Blocked：存在すると発動不可。
- Cancel Abilities With Tag：発動時に対象AbilityをCancel。
- Block Abilities With Tag：実行中に対象AbilityのActivationをBlock。
- Activation Owned Tags：実行中Ownerへ付与する状態Tag。

攻撃・回避・StunのCancel表をTagで表せますが、優先順位やWindowまで全てTag名へ埋め込まず、Rule Dataと併用します。

## 6. ASCをCharacterへ置く場合

```cpp
AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("ASC"));
AbilitySystemComponent->SetIsReplicated(true);
```

CharacterのSpawn／Destroyと共にAbility・Effectが消える構成です。敵や身体固有状態に自然です。

## 7. ASCをPlayerStateへ置く場合

Character死亡・Respawn・交代を越えてAbility／Attributeを維持したいPlayerで候補になります。

```cpp
UAbilitySystemComponent* AActionCharacter::GetAbilitySystemComponent() const
{
    const ACombatPlayerState* PS = GetPlayerState<ACombatPlayerState>();
    return PS ? PS->GetAbilitySystemComponent() : nullptr;
}

void AActionCharacter::PossessedBy(AController* NewController)
{
    Super::PossessedBy(NewController);
    InitializeAbilityActorInfo();
}

void AActionCharacter::OnRep_PlayerState()
{
    Super::OnRep_PlayerState();
    InitializeAbilityActorInfo(); // ClientでもPlayerState到着後に初期化。
}
```

## 8. キャラクター交代の難所

Party全体で1つのASCを共有すると、Character固有Ability、個別HP、個別Cooldownの分離が難しくなります。

候補：

- Party／Player ASC＋各Character ASCの二層。
- 各Character ASCを維持し、Possess先だけ入力を受ける。
- PlayerState ASCへAbility Source ObjectでCharacter差を付ける。

どのAttribute／Effectが交代後も残るかを表にして選びます。

## 9. Debug

現在TagとCount、付与元Effect、実行中Ability、Blocked理由を表示します。Tagがあるかだけでなく「誰が付けたか」を追跡できるようにします。

## 参考

- [Gameplay Tags](https://dev.epicgames.com/documentation/en-us/unreal-engine/using-gameplay-tags-in-unreal-engine)
- [ASC and Attributes](https://dev.epicgames.com/documentation/unreal-engine/gameplay-ability-system-component-and-gameplay-attributes-in-unreal-engine)
