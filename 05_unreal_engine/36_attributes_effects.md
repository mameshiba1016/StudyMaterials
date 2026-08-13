# 36 Attribute SetとGameplay Effect

## 1. AttributeのBaseとCurrent

`FGameplayAttributeData`はBase ValueとCurrent Valueを持ちます。永続成長でBaseを変え、期間BuffでCurrentへModifierが積まれる、といった扱いができます。

```cpp
UCLASS()
class UCombatAttributeSet : public UAttributeSet
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Health)
    FGameplayAttributeData Health;
    ATTRIBUTE_ACCESSORS(UCombatAttributeSet, Health)

    UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxHealth)
    FGameplayAttributeData MaxHealth;
    ATTRIBUTE_ACCESSORS(UCombatAttributeSet, MaxHealth)

    UPROPERTY(BlueprintReadOnly)
    FGameplayAttributeData IncomingDamage; // 計算結果を受けるMeta Attribute例。
    ATTRIBUTE_ACCESSORS(UCombatAttributeSet, IncomingDamage)
};
```

`ATTRIBUTE_ACCESSORS`はGetter／Setter／Initializer等の補助関数を生成するプロジェクト側Macroとして定義する慣例があります。

## 2. Clamp

```cpp
void UCombatAttributeSet::PreAttributeChange(
    const FGameplayAttribute& Attribute,
    float& NewValue)
{
    Super::PreAttributeChange(Attribute, NewValue);

    if (Attribute == GetHealthAttribute())
    {
        NewValue = FMath::Clamp(NewValue, 0.0f, GetMaxHealth());
    }
}
```

Effect実行後の`PostGameplayEffectExecute`でもDamage処理、Clamp、死亡通知等を行います。どのHookがBase／Current／実行結果へ働くかを区別します。

## 3. Replication通知

```cpp
void UCombatAttributeSet::OnRep_Health(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UCombatAttributeSet, Health, OldValue);
}
```

`GetLifetimeReplicatedProps`へ登録し、予測補正を正しく通知できるMacroを使います。

## 4. Gameplay EffectのDuration Policy

- Instant：即時にBase値等を変更。Damage、回復。
- Duration：一定時間Modifierを維持。
- Infinite：明示削除まで維持。装備Buff、Aura等。

Duration Effect終了時にCurrentへのModifierが取り除かれます。

## 5. Modifier Operation

- Add：加算。
- Multiply：乗算。
- Divide：除算。
- Override：上書き。

複数Modifierの評価順序とAggregationを理解し、独自Damage式と二重計算しません。

## 6. Effect SpecとContext

Gameplay Effect Classは定義、`FGameplayEffectSpec`はLevel、SetByCaller値、Source Tag、Context等を持つ実行Instance情報です。

```cpp
FGameplayEffectContextHandle Context = SourceASC->MakeEffectContext();
Context.AddSourceObject(AttackDefinition);
Context.AddHitResult(HitResult);

FGameplayEffectSpecHandle Spec = SourceASC->MakeOutgoingSpec(
    DamageEffectClass,
    AbilityLevel,
    Context);

Spec.Data->SetSetByCallerMagnitude(TAG_Data_Damage, CalculatedDamage);
SourceASC->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), TargetASC);
```

nullとHandle有効性を実装では確認します。SetByCaller Tagの欠落もData Validation対象です。

## 7. Execution Calculation

Source Attack、Target Defense、Critical、属性等を捕捉して複雑なDamageを計算する場合、`UGameplayEffectExecutionCalculation`へ集約できます。

Snapshot Captureか実行時Captureかで、Ability開始時のAttack値を使うか、Hit時の値を使うかが変わります。

## 8. Meta Attribute

`IncomingDamage`へ計算結果を入れ、Attribute Set側でShield、Health、Poiseへ配分して0へ戻す設計があります。Damageを直接Health Modifierへすると、GuardやShieldの共通処理が分散しやすくなります。

## 9. EffectのStack

毒、攻撃Buff等ではStack上限、Duration更新、Source別集約、Overflowを設定します。「同じEffect」の識別方法を理解し、意図せず別SourceのDebuffを上書きしないようにします。

## 10. Attribute変更Delegate

UIはTickでHealthを読むのではなく、ASCのAttribute Value Change Delegateを購読します。Widget破棄時にはDelegateを解除します。

## 11. テスト

- Base／Currentと期間Buff終了。
- MaxHealth低下時のHealth Clamp。
- 複数SourceのStack。
- Prediction後のReplication補正。
- 致死Damageの一度だけの死亡遷移。
- SetByCaller欠落。
- Effect解除時にTag／Cueが残らない。

## 参考

- [Gameplay Attributes and Attribute Sets](https://dev.epicgames.com/documentation/en-us/unreal-engine/gameplay-attributes-and-attribute-sets-for-the-gameplay-ability-system-in-unreal-engine)
