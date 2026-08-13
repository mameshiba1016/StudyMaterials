# 31 BlackboardとBehavior Tree

## 1. Blackboardは共有Memory

Blackboard Key例：

```text
TargetActor          Object(Actor)
LastKnownLocation    Vector
HasLineOfSight       Bool
DesiredCombatRange   Float
CombatSlotLocation   Vector
CurrentIntent        Enum / GameplayTag相当
```

Blackboardは何でも保存する倉庫ではありません。長期Memoryの詳細はAIController／Memory Component、Tree分岐に必要な現在値だけをBlackboardへ写します。

## 2. Behavior TreeのNode

- Composite：Selector、Sequence等の流れ。
- Task：移動、待機、攻撃要求など実行Action。
- Decorator：枝を実行できる条件。
- Service：枝がActiveな間に一定間隔で値を更新。

UEのBehavior TreeはEvent駆動を活用し、Blackboard値の変更をDecoratorが監視して枝をAbortできます。

## 3. SelectorとSequence

```text
Root
└─ Selector
   ├─ [Dead] Sequence → Death処理
   ├─ [Stunned] Sequence → Stun待機
   ├─ [HasTarget] Combat Subtree
   └─ Patrol Subtree
```

Selectorは左から成功可能な枝を選び、Sequenceは子を順に成功させます。高優先度を左へ置きます。

## 4. Observer Aborts

`HasTarget`がfalseになったら実行中のMove／Attack枝を即座に止める、といった反応をDecoratorのObserver Abortsで作れます。

- Self：自身のSubtreeを中断。
- Lower Priority：右側の低優先度枝を中断。
- Both：両方。

設定しすぎると頻繁な再探索が起きます。どのKey変更が何を中断するかを図で管理します。

## 5. C++ Task

```cpp
UCLASS()
class UBTTask_RequestCombatAction : public UBTTaskNode
{
    GENERATED_BODY()

public:
    UBTTask_RequestCombatAction();
    virtual EBTNodeResult::Type ExecuteTask(
        UBehaviorTreeComponent& OwnerComp,
        uint8* NodeMemory) override;
};
```

```cpp
EBTNodeResult::Type UBTTask_RequestCombatAction::ExecuteTask(
    UBehaviorTreeComponent& OwnerComp,
    uint8* NodeMemory)
{
    AAIController* Controller = OwnerComp.GetAIOwner();
    AActionCharacter* Character = Controller
        ? Cast<AActionCharacter>(Controller->GetPawn())
        : nullptr;

    if (!Character)
    {
        return EBTNodeResult::Failed;
    }

    return Character->GetCombatComponent()->TryStartAction(ActionTag)
        ? EBTNodeResult::Succeeded
        : EBTNodeResult::Failed;
}
```

Action完了まで待つTaskでは`InProgress`を返し、Delegate完了時に`FinishLatentTask`します。Abort時にはDelegate解除とAction Cancel方針を実装します。

## 6. Node InstanceとMemory

Behavior Tree Node ObjectへAI個体ごとの可変状態を安易に置くと、Node共有方式により別AIと状態が混ざる危険があります。Node Memoryを使うか、Instance化のCostと意味を理解して選びます。

```cpp
struct FAttackTaskMemory
{
    int32 ActionSequenceId = 0;
    FDelegateHandle CompletionHandle;
};
```

## 7. Serviceを毎Frame処理にしない

Serviceは枝がActiveな間、設定間隔でBlackboard更新等を行います。Target距離や攻撃位置評価を0秒間隔にせず、重要度に応じたIntervalとRandom Deviationを設定します。

知覚Eventで更新できる値をServiceで再検索し続けないでください。

## 8. 攻撃Taskの責任

Behavior Treeは「Heavy Attackを試す」という意図を出します。Combat SystemがCost、Cooldown、Cancel、Target距離を最終検証します。

Task失敗時：別攻撃を選ぶ、位置調整する、短時間待つ、Slotを返却する、といった枝へ進めます。

## 9. Subtreeと再利用

```text
共通Target Acquisition Subtree
共通Approach Subtree
Melee Combat Subtree
Ranged Combat Subtree
Boss Phase固有Subtree
```

巨大Tree一枚より、責任別SubtreeとData差で敵種を増やします。

## 10. Debug

- 実行中NodeとBlackboard値。
- Decoratorがfalseの理由。
- Abort元Key。
- Task開始／成功／失敗／中断時刻。
- Combat SystemがActionを拒否した理由。

## 参考

- [Behavior Tree Overview](https://dev.epicgames.com/documentation/en-us/unreal-engine/behavior-tree-in-unreal-engine---overview)
- [Behavior Tree Tasks](https://dev.epicgames.com/documentation/en-us/unreal-engine/unreal-engine-behavior-tree-node-reference-tasks)
