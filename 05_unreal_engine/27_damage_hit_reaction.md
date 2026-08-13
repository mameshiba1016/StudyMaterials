# 27 ダメージ、Hit判定、リアクション

## 1. 検出とDamageを分ける

```text
Trace／Overlapで接触検出
  ↓ Hit Candidate
Team・無敵・重複・角度を検証
  ↓ Valid Hit
Damage計算
  ↓ Damage Result
HP／Poise／状態異常へ適用
  ↓
Hit Reaction・VFX・SFX・Camera・UIへ通知
```

Trace関数内でHP、Montage、音、Cameraを全部直接操作しないでください。

## 2. Hit ContextとResult

```cpp
USTRUCT(BlueprintType)
struct FCombatHitContext
{
    GENERATED_BODY()

    UPROPERTY()
    TWeakObjectPtr<AActor> Attacker;

    UPROPERTY()
    TWeakObjectPtr<AActor> Target;

    UPROPERTY()
    FGameplayTag AttackTag;

    UPROPERTY()
    FVector ImpactPoint = FVector::ZeroVector;

    UPROPERTY()
    FVector AttackDirection = FVector::ForwardVector;

    UPROPERTY()
    float BaseDamage = 0.0f;

    int32 ActionSequenceId = 0;
};
```

```cpp
struct FCombatDamageResult
{
    float FinalHealthDamage = 0.0f;
    float PoiseDamage = 0.0f;
    bool bCritical = false;
    bool bGuarded = false;
    bool bKilled = false;
    EHitReactionType Reaction = EHitReactionType::None;
};
```

入力Dataと計算結果を明示すると、UIや演出が同じ結果を参照できます。

## 3. Damage計算順序

例：

```text
Base Damage
× 攻撃倍率
× Critical倍率
× 属性相性
− Defense軽減
× Guard倍率
= Final Damage
```

順番により結果が変わります。各Stepを関数化し、Clamp、丸め、最小Damage、Overflowを仕様化します。

```cpp
float CalculateFinalDamage(const FDamageParameters& Params)
{
    double Value = FMath::Max(0.0, static_cast<double>(Params.BaseDamage));
    Value *= Params.AttackMultiplier;
    Value *= Params.CriticalMultiplier;
    Value = ApplyDefense(Value, Params.Defense);
    return static_cast<float>(FMath::Clamp(Value, 0.0, Params.MaximumDamage));
}
```

## 4. UE標準Damage APIとの境界

`UGameplayStatics::ApplyDamage`、`TakeDamage`等は汎用入口として使えますが、高度なAction Combatでは独自ContextやGameplay Ability Systemを使う場合があります。

重要なのは、DamageのAuthority、Data、結果通知を一貫させることです。標準Damageと独自HP減算を混在させて二重適用しないでください。

## 5. 多重Hit防止

Actor単位だけでなく、攻撃の意味に応じてKeyを決めます。

```cpp
USTRUCT()
struct FHitUniquenessKey
{
    GENERATED_BODY()
    int32 ActionSequenceId = 0;
    int32 HitIndex = 0; // 多段技の何段目か。
    TWeakObjectPtr<AActor> Target;
};
```

多段攻撃はHit Indexが変われば再Hit可能、同じ段では複数Hurtboxに触れても一度、という規則を表現できます。

## 6. PoiseとSuper Armor

HP Damageと怯み成立を分けます。

- Health：死亡までのResource。
- Poise／Stagger：一定Damageで怯むResource。
- Super Armor：特定Action中の怯み耐性。
- Hyper Armor：Damageは受けるがActionを継続。

攻撃を受けたら必ずMontage停止、ではComboやBoss行動を作れません。

## 7. Hit方向とReaction

```cpp
const FVector DefenderForward = Defender->GetActorForwardVector().GetSafeNormal2D();
const FVector Incoming = (-Hit.AttackDirection).GetSafeNormal2D();
const float ForwardDot = FVector::DotProduct(DefenderForward, Incoming);
const float Side = FVector::CrossProduct(DefenderForward, Incoming).Z;
```

DotとCrossから前後左右Reactionを選べます。攻撃者位置だけで方向を求めると、Projectileの飛来方向とずれる場合があるためHit Contextへ実際の方向を入れます。

## 8. 死亡は一度だけ確定する

```cpp
if (!bDead && CurrentHealth <= 0.0f)
{
    bDead = true;
    OnDeath.Broadcast(DamageResult);
}
```

同Frameの複数Hitで死亡処理、Loot、Scoreが重複しないよう、状態遷移を原子的な一入口へ集約します。

## 9. Presentation Event

Damage確定後に次をEventとして発行します。

- Impact VFX／SFX。
- Damage Number。
- Camera Shake。
- Hit Stop要求。
- Controller Rumble。
- Hit Reaction Animation。

Dedicated Serverには画面演出が不要です。Gameplay結果とローカルPresentationを分けます。

## 10. テスト

- 味方攻撃、自己Hit、死亡済みTarget。
- 複数Hurtboxへの同時接触。
- CriticalとGuardの計算順序。
- Armor中のPoise Damage。
- 同Frame致死Hitの重複。
- 極端なDefense、負数、巨大Damage。
- NetworkでClient演出だけ二重再生しないか。
