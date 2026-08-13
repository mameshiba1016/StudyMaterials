# 30 AIControllerとAI Perception

## 1. AIControllerは意思、Characterは身体

PlayerとAIが同じCombat Componentを使い、命令の生成元だけを変えます。

```text
PlayerController: 物理入力 → Combat Command
AIController:     知覚・判断 → Combat Command
                              ↓
                  共通Character／Combat System
```

AIControllerがHPを直接減らす、Montageを直接強制する、といった規則迂回を避けます。

## 2. AIControllerの構成

```cpp
UCLASS()
class MYGAME_API ACombatAIController : public AAIController
{
    GENERATED_BODY()

public:
    ACombatAIController();
    virtual void OnPossess(APawn* InPawn) override;
    virtual void OnUnPossess() override;

private:
    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UAIPerceptionComponent> PerceptionComponent;

    UPROPERTY()
    TObjectPtr<UAISenseConfig_Sight> SightConfig;
};
```

```cpp
ACombatAIController::ACombatAIController()
{
    PerceptionComponent = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("Perception"));
    SetPerceptionComponent(*PerceptionComponent);

    SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("SightConfig"));
    SightConfig->SightRadius = 1800.0f;
    SightConfig->LoseSightRadius = 2200.0f; // 発見距離より広くし、境界での振動を抑える。
    SightConfig->PeripheralVisionAngleDegrees = 70.0f;
    SightConfig->SetMaxAge(3.0f);

    PerceptionComponent->ConfigureSense(*SightConfig);
    PerceptionComponent->SetDominantSense(SightConfig->GetSenseImplementation());
}
```

所属検知設定はTeam Systemと一致させます。

## 3. Possessで開始する

```cpp
void ACombatAIController::OnPossess(APawn* InPawn)
{
    Super::OnPossess(InPawn);

    if (BehaviorTreeAsset)
    {
        RunBehaviorTree(BehaviorTreeAsset);
    }
}

void ACombatAIController::OnUnPossess()
{
    BrainComponent->StopLogic(TEXT("Pawn unpossessed"));
    ClearFocus(EAIFocusPriority::Gameplay);
    Super::OnUnPossess();
}
```

Behavior Treeの実行、Blackboard初期化、Delegate接続をPossess状態と合わせます。

## 4. Perception更新

```cpp
void ACombatAIController::HandleTargetPerceptionUpdated(
    AActor* Actor,
    FAIStimulus Stimulus)
{
    if (!IsValid(Actor))
    {
        return;
    }

    FTargetMemory& Memory = TargetMemories.FindOrAdd(Actor);
    Memory.LastKnownLocation = Stimulus.StimulusLocation;
    Memory.LastSensedTime = GetWorld()->GetTimeSeconds();
    Memory.bCurrentlySensed = Stimulus.WasSuccessfullySensed();

    ReevaluatePrimaryTarget();
}
```

「見えなくなった」と「存在を忘れた」は別です。最後の既知位置、最終知覚時刻、現在視認中を記録し、探索へ遷移します。

## 5. Senseの用途

- Sight：視野、距離、遮蔽。
- Hearing：Noise Eventから位置を知覚。
- Damage：攻撃を受けた相手を認識。
- Team／独自Sense：プロジェクト固有の刺激。

知覚結果は事実であり、行動決定ではありません。音を聞いたら必ず攻撃ではなく、Memory更新後にBehavior Tree／Utilityで判断します。

## 6. Target選択

```text
候補Memory
  ↓ 有効・敵対・生存確認
現在視認 Bonus
距離 Score
直近攻撃者 Bonus
Combat Director指定 Bonus
  ↓
Primary Target
```

毎知覚EventでTargetを即座に交換するとAIが振動します。現在Target維持Bonusと切替閾値を持たせます。

## 7. Focusと身体回転

`SetFocus`／`SetFocalPoint`でControllerの注視方向を設定できます。ただしCharacterMovementの`bOrientRotationToMovement`、Controller Desired Rotation、攻撃Motion Warpingが競合しないよう、Combat状態別の回転Policyを決めます。

## 8. 性能

- Sense半径を必要以上に広げない。
- 全AIが同Frameに再評価しないよう間隔を分散。
- 視線TraceのChannelと遮蔽物を整理。
- 遠距離AIは知覚更新頻度・行動精度を落とす。
- Event駆動を使い、毎Tick全候補を走査しない。

## 9. デバッグ

AI DebuggerでPerception、現在Target、最後の既知位置を表示し、Visual LoggerへTarget切替理由を残します。

## 参考

- [AI Perception](https://dev.epicgames.com/documentation/en-us/unreal-engine/ai-perception-in-unreal-engine)
- [AI Debugging](https://dev.epicgames.com/documentation/unreal-engine/ai-debugging-in-unreal-engine)
