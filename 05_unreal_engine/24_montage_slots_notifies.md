# 24 Animation Montage、Slot、Notify

## 1. Montageの役割

Animation Montageは複数Sequenceをまとめ、Section、Slot、Notifyを使って実行時に制御できるAssetです。攻撃、回避、被弾、必殺技のような一時Actionに向きます。

```text
Combat SystemがAction開始を決定
  ↓
AnimInstanceへMontage再生要求
  ↓
Section／SlotでPoseを再生
  ↓
Notifyで判定窓・演出時点を通知
  ↓
Blend Out／Interrupted／EndedをCombat Systemへ返す
```

Montageが再生できたかを確認してからAction状態を確定するか、失敗時にRollbackする設計が必要です。

## 2. 再生と終了Delegate

```cpp
UAnimInstance* AnimInstance = Character->GetMesh()->GetAnimInstance();
if (!AnimInstance || !AttackMontage)
{
    return false;
}

const float Duration = AnimInstance->Montage_Play(AttackMontage, PlayRate);
if (Duration <= 0.0f)
{
    return false;
}

FOnMontageEnded EndDelegate;
EndDelegate.BindUObject(this, &UCombatComponent::HandleMontageEnded);
AnimInstance->Montage_SetEndDelegate(EndDelegate, AttackMontage);
return true;
```

```cpp
void UCombatComponent::HandleMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
    if (Montage != CurrentMontage)
    {
        return; // 古いMontageのCallbackを現在Actionへ適用しない。
    }

    CloseAllCombatWindows();
    FinishCurrentAction(bInterrupted ? EActionResult::Interrupted : EActionResult::Completed);
}
```

Action IDやMontage参照で古いCallbackを識別します。

## 3. SectionでComboをつなぐ

```cpp
AnimInstance->Montage_JumpToSection(TEXT("Attack_02"), AttackMontage);
```

または次Sectionを設定し、現在Section終了時につなげます。入力受付、次攻撃決定、Stamina消費をMontage Section名だけへ依存させず、Combat Dataと対応表を持ちます。

`FName`の文字列打ち間違いは実行時まで分からないことがあるため、定数化・Data検証を行います。

## 4. Slot

MontageはAnim Graph内の対応Slot Nodeを通らないと最終Poseへ反映されません。

```text
Locomotion Pose
  ├─ DefaultSlot / FullBody Montage
  └─ UpperBodySlot → Layered Blend Per Bone
```

Slot Group内のMontageは競合時に互いを止めることがあります。攻撃、被弾、表情をどのGroup／Slotへ置くか、優先順位と中断規則を設計します。

## 5. Anim NotifyとNotify State

- Notify：特定時点の一回イベント。足音、Attack Cue等。
- Notify State：開始から終了までの時間窓。Hitbox、Cancel Window、無敵等。

```cpp
UCLASS()
class UAnimNotifyState_AttackWindow : public UAnimNotifyState
{
    GENERATED_BODY()

public:
    virtual void NotifyBegin(
        USkeletalMeshComponent* MeshComp,
        UAnimSequenceBase* Animation,
        float TotalDuration,
        const FAnimNotifyEventReference& EventReference) override;

    virtual void NotifyEnd(
        USkeletalMeshComponent* MeshComp,
        UAnimSequenceBase* Animation,
        const FAnimNotifyEventReference& EventReference) override;
};
```

Notify Asset自体へCharacterごとの実行状態を保存してはいけません。同じNotify Objectが複数再生Instanceから使われ得るため、状態はCombat Component側でAction IDと共に管理します。

## 6. Notifyは必ず来るとは限らない

MontageのBlend Out、強制停止、LOD、Server上のAnimation設定などにより、想定した終了Notifyだけに頼ると窓が閉じない危険があります。

対策：

- Action終了時に全Windowを閉じる。
- Montage End／Blend Out／Interruptedを処理。
- WindowをAction IDへ紐付ける。
- Gameplay上重要な期限にはTimer／状態機械のFail-safeも持つ。
- Notifyは「状態を許可する合図」、Combat Systemは最終Authorityとする。

## 7. Branching Pointと時刻精度

Montage NotifyのTick Typeには精度と負荷のTrade-offがあります。Branching Pointは正確な分岐に向きますが、すべての演出Notifyへ乱用しません。足音やVFXと、Combo分岐やRoot Motion制御では要求精度が違います。

## 8. Montage中断規則

```text
Attack Montage
  ├─ Dodge入力 + Cancel Window → Dodge Montageへ中断
  ├─ 被弾 + Super Armorなし   → Hit Montageへ中断
  ├─ Character交代            → 終了処理後に新PawnへPossess
  └─ 正常終了                  → Locomotionへ復帰
```

どのMontageがどれを止められるかをSlotの偶然の競合へ任せず、Combat Ruleで先に判定します。

## 9. Data検証

- 必須Montageがnullでない。
- 必須Sectionが存在する。
- Attack／Cancel Notify Windowが範囲内。
- Root Motion設定がAction定義と一致。
- Slot名がAnimBPと一致。
- Comboの次Sectionが循環・欠落していない。

Editor Data Validationへ組み込むと、キャラクター数が増えても再生時まで不備を持ち越しにくくなります。

## 参考

- [Animation Montage](https://dev.epicgames.com/documentation/en-us/unreal-engine/animation-montage-in-unreal-engine)
- [Animation Notifies](https://dev.epicgames.com/documentation/en-us/unreal-engine/animation-notifies-in-unreal-engine)
