# Behavior Tree

Behavior Tree（BT）はTaskを木構造で組み合わせ、成功・失敗・実行中の状態を返して行動を制御します。

```cpp
enum class NodeStatus
{
    Success,
    Failure,
    Running
};
```

## Composite

- Sequence：子を順に実行し、一つ失敗でFailure、全成功でSuccess。
- Selector：子を順に試し、一つ成功でSuccess、全失敗でFailure。
- Parallel：複数子を更新し成功/失敗Policyで判定。

## Leaf

- Condition：Targetが見えるか等。通常即時終了。
- Action：移動、攻撃、待機。複数tickRunningになり得る。

## Tick

```cpp
NodeStatus MoveToNode::Tick(AiContext& context)
{
    if (!context.target)
    {
        return NodeStatus::Failure;
    }

    if (ReachedGoal(context))
    {
        StopMovement(context);
        return NodeStatus::Success;
    }

    RequestMovement(context);
    return NodeStatus::Running;
}
```

## Abort

高優先Conditionが変化した時、現在Running Actionを中断します。Abort時にPath、Animation Request、予約Tokenを確実に解除します。

```cpp
virtual void Abort(AiContext& context) = 0;
```

## Reactive Selector

毎tick先頭条件から再評価すると反応性は高い一方、低優先Actionが進まずStarvationする場合があります。Memory型Compositeとの違いを明示します。

## Decorator

Cooldown、Repeat、Invert、Timeout、Blackboard条件を子へ付けます。Decoratorを重ねすぎると実行理由が読めなくなるためDebug Traceを用意します。

## Service

一定頻度でTarget探索等を更新します。全Nodeが同時に重いQueryを行わないよう更新位相を分散します。

## Node状態

Tree定義は複数AIで共有し、Running child indexやTimerはAIごとのInstance Memoryへ保存します。共有Nodeへ状態を置くと敵同士が干渉します。

## Gameplay Actionとの境界

BT Attack Nodeは「攻撃開始Request」を送り、Character Actionの完了/失敗を観測します。Damageを直接発生させません。

## Debug

現在Running Path、各Nodeの最終Status、Abort、Condition値、滞在時間をTree上で色分けします。
