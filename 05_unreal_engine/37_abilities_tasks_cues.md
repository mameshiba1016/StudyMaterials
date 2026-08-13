# 37 Gameplay Ability、Ability Task、Gameplay Cue

## 1. Abilityを自己完結したActionにする

```cpp
UCLASS()
class UGA_LightAttack : public UGameplayAbility
{
    GENERATED_BODY()

public:
    virtual void ActivateAbility(
        const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo,
        const FGameplayEventData* TriggerEventData) override;
};
```

AbilityはActivation検証、Commit、Task開始、Effect適用、終了・Cancel後始末を調停します。長い計算や全戦闘DataをAbility Blueprintへ直書きせず、再利用ServiceとData Assetへ分けます。

## 2. Commit

```cpp
if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
{
    EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
    return;
}
```

CommitはCostとCooldown等を正式適用します。Ability開始時、Target確定時、Hit時のどこでCommitするかは仕様です。失敗したAbilityがCostだけ消費しないようにします。

## 3. Instancing Policy

- Non-Instanced：Instance状態を持てず高効率。
- Instanced Per Actor：OwnerごとにInstance。
- Instanced Per Execution：発動ごとにInstance。

Ability ObjectのMemberへRuntime状態を保存するならPolicyを理解します。同一Ownerで並行発動可能かも考慮します。

## 4. Ability Task

Ability Taskは複数Frameに渡る非同期処理をAbilityへ接続します。

- Montage再生と終了待機。
- Gameplay Event待機。
- Input Release待機。
- Target Data待機。
- Movement Mode変更待機。

```text
Activate Ability
  ↓ PlayMontageAndWait Task
OnCompleted  → EndAbility(false)
OnInterrupted→ EndAbility(cancelled)
OnCancelled  → EndAbility(cancelled)
```

すべての終了Pinを処理します。

## 5. Custom Ability Task

攻撃Window、Target Lock、Motion Warp等を複数Abilityで再利用するならCustom Taskが候補です。

```cpp
UCLASS()
class UAbilityTask_WaitCombatEvent : public UAbilityTask
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintAssignable)
    FWaitCombatEventDelegate OnEvent;

    virtual void Activate() override;
    virtual void OnDestroy(bool bInOwnerFinished) override;
};
```

`OnDestroy`でDelegate解除、Timer取消、外部登録解除を行います。Ability終了時にTaskも安全に終了できる設計にします。

## 6. Gameplay Event

Tag付きEvent DataでAbilityを発動または実行中Abilityへ通知できます。

```text
Event.Combat.HitConfirmed
Event.Combat.ParrySuccess
Event.Animation.ComboWindow
Event.Character.SwitchRequested
```

Event PayloadへInstigator、Target、Magnitude、Target Dataを入れます。Event名の乱立をTag階層と命名規則で管理します。

## 7. Target Data

Client選択TargetやTrace結果を`FGameplayAbilityTargetDataHandle`でAbilityへ渡せますが、ServerでValidationが必要です。

- 距離範囲。
- 視線。
- Team。
- Target生存。
- Ability発動時刻との整合。
- Hit形状と許容誤差。

## 8. Gameplay Cue

Gameplay CueはTagに対応するCosmetic表現です。

```text
GameplayCue.Combat.Hit.Light
GameplayCue.Combat.Hit.Heavy
GameplayCue.State.Invulnerable
GameplayCue.Status.Burn
```

瞬間Cueと継続Cueを使い分け、Location、Normal、Magnitude等をParameterで渡します。

Gameplay Cueは演出専用です。Cueが届かなかった場合でもDamage、無敵、状態解除が正しく進む必要があります。

## 9. 高速アクションAbilityの流れ

```text
Input Tag Pressed
  ↓ ASCがAbilityを検索・Activation
Tag／Cost／Cooldown検証
  ↓ Commit
Motion Warp Target設定
  ↓ Montage Task
Notify EventでHit Window
  ↓ Trace → Server検証
Damage Gameplay Effect
  ↓ Gameplay Cue
Combo Event／Cancel Event待機
  ↓
EndAbilityでTask・Tag・Windowを清掃
```

## 10. 失敗経路Checklist

- Actor Infoが未初期化。
- Avatar交代後も旧Character参照。
- Commit失敗後にAbilityが残る。
- Montage中断Pinを処理していない。
- Task Delegate解除漏れ。
- Client予測CueがServer Cueと二重再生。
- Cancel時にMotion Warp／Hitboxが残る。
- Ability終了後もActivation Owned Tagが残る設計ミス。

## 参考

- [Using Gameplay Abilities](https://dev.epicgames.com/documentation/unreal-engine/using-gameplay-abilities-in-unreal-engine)
- [Gameplay Ability Tasks](https://dev.epicgames.com/documentation/en-us/unreal-engine/gameplay-ability-tasks-in-unreal-engine)
