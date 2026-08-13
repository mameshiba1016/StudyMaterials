# 26 データ駆動コンボとキャンセル

## 1. 戦闘Actionを状態機械として扱う

```text
Idle
  ↓ Attack Command
Startup → Active → Recovery
           │         │
           ├─ Hit    └─ Cancel Window → 次Action
           └─ Miss
```

- Startup：攻撃判定が出る前。
- Active：Hitbox／Sweepが有効。
- Recovery：攻撃後の硬直。
- Cancel Window：特定Actionへ遷移を許す期間。

Montageの再生位置だけを状態として使わず、Combat Componentが現在Action、Phase、開始時刻、Windowを所有します。

## 2. Action定義と実行状態を分ける

```cpp
USTRUCT(BlueprintType)
struct FCombatActionDefinition
{
    GENERATED_BODY()

    UPROPERTY(EditDefaultsOnly)
    FGameplayTag ActionTag;

    UPROPERTY(EditDefaultsOnly)
    TObjectPtr<UAnimMontage> Montage;

    UPROPERTY(EditDefaultsOnly)
    float StaminaCost = 0.0f;

    UPROPERTY(EditDefaultsOnly)
    int32 Priority = 0;

    UPROPERTY(EditDefaultsOnly)
    TArray<FGameplayTag> AllowedFollowUps;
};
```

定義は共有可能な不変Dataです。現在Hit済みActor、残り時間、Action Sequence IDなどはInstanceごとのRuntime Stateへ置きます。

```cpp
struct FActiveCombatAction
{
    const FCombatActionDefinition* Definition = nullptr; // Data所有者の寿命を保証する。
    int32 SequenceId = 0;
    ECombatActionPhase Phase = ECombatActionPhase::None;
    TSet<TWeakObjectPtr<AActor>> HitActors;
};
```

## 3. Action開始を一つの入口へ集約する

```cpp
bool UCombatComponent::TryStartAction(FGameplayTag ActionTag)
{
    const FCombatActionDefinition* Definition = FindActionDefinition(ActionTag);
    if (!Definition || !CanStartAction(*Definition))
    {
        return false;
    }

    if (!ConsumeResources(Definition->StaminaCost))
    {
        return false;
    }

    const int32 NewSequenceId = ++LastSequenceId;
    if (!PlayActionMontage(*Definition, NewSequenceId))
    {
        RefundResources(Definition->StaminaCost); // 再生失敗時に整合性を戻す。
        return false;
    }

    ActiveAction = MakeRuntimeState(*Definition, NewSequenceId);
    return true;
}
```

Player入力、AI、Tutorial Scriptのすべてが同じ入口を使います。直接Montage再生して規則を迂回させません。

## 4. Cancel Rule

「攻撃中か否か」のboolだけでは不十分です。

```cpp
USTRUCT(BlueprintType)
struct FActionCancelRule
{
    GENERATED_BODY()

    UPROPERTY(EditDefaultsOnly)
    FGameplayTagContainer FromActions;

    UPROPERTY(EditDefaultsOnly)
    FGameplayTagContainer ToActions;

    UPROPERTY(EditDefaultsOnly)
    ECancelRequirement Requirement = ECancelRequirement::WindowOpen;

    UPROPERTY(EditDefaultsOnly)
    int32 MinimumPriority = 0;
};
```

回避は通常攻撃をCancelできる、必殺技は被弾以外をCancelできる、次ComboはHit時だけ早くつながる、といった規則をDataへ置きます。

## 5. Combo Graph

```text
Light01 ─Light→ Light02 ─Light→ Light03
   │                  └─Hold→ HeavyFinisher
   └─Dodge→ DodgeAttack
```

配列IndexだけのComboは分岐に弱いため、Action TagをNode、入力CommandをEdgeとするGraphとして扱います。

```cpp
const FGameplayTag NextAction = ComboGraph.Resolve(
    ActiveAction.Definition->ActionTag,
    BufferedCommand,
    CombatContext);
```

`CombatContext`にはHit確認、空中、Target距離、Resource等を渡しますが、Graph自体がWorldへ直接Queryする設計は避けます。

## 6. Windowの開閉

```cpp
void UCombatComponent::OpenCancelWindow(FGameplayTag WindowTag, int32 SequenceId)
{
    if (!IsCurrentAction(SequenceId))
    {
        return; // 中断済みの古いNotifyを無視する。
    }

    OpenWindows.Add(WindowTag);
    TryConsumeBufferedCommand();
}
```

Action終了時にはNotify Endへ到達しなくても`OpenWindows.Reset()`します。

## 7. 優先順位と同時入力

同一Frameに攻撃・回避・交代が来た場合の結果を定義します。

```text
Parry Assist > Dodge > Character Switch > Skill > Normal Attack
```

ただし常に固定ではなく、現在Window、Resource、Action Priorityで最終決定します。入力Callbackの呼ばれた順番へ依存させません。

## 8. Data Validation

- Montageと必須Sectionが存在する。
- Comboの遷移先Action Tagが登録済み。
- Costが負数でない。
- Graphに意図しない閉路・到達不能Nodeがない。
- Cancel Ruleが矛盾していない。
- Hitbox WindowよりDamage Dataが存在する。

## 9. テスト

- 各Phaseで全Commandを入力する。
- Hit時／Miss時の分岐。
- Montage中断と古いNotify。
- 低fpsで短いWindowを跨ぐ。
- Resource不足時に消費されない。
- AIとPlayerが同じ規則を通る。
- Character交代後に旧Bufferが発火しない。
