# 有限状態機械（FSM）

FSMは現在Stateを一つ持ち、条件で別Stateへ遷移します。小～中規模の敵行動を明確に表せます。

```cpp
enum class EnemyState
{
    Idle,
    Patrol,
    Chase,
    Attack,
    Search,
    HitStun,
    Dead
};
```

## Lifecycle

```cpp
class IEnemyState
{
public:
    virtual ~IEnemyState() = default;
    virtual void Enter(EnemyContext&) = 0;
    virtual void Update(EnemyContext&, float fixedDelta) = 0;
    virtual void Exit(EnemyContext&) = 0;
};
```

小規模ならenum+switchで十分です。Class化はState固有Dataと複雑性が増えた時に検討します。

## 遷移を遅延する

Stateの`Update`中に自分を破棄せずRequestを返し、安全地点で`Exit → Enter`します。同tick複数RequestのPriorityを定めます。

```text
Dead > HitStun > Defensive > Attack > Movement
```

## Guard Condition

Attackへ遷移する条件：Target有効、距離、視線、Cooldown、Token、現在ActionがCancel可能など。条件を複数箇所へ重複させません。

## State Explosion

`AlertedRunningAttacking`のように直交する状態を一FSMへ組み合わせると爆発します。

- High-level：Combat/NonCombat/Dead。
- Action：Move/Attack/Hit。
- Animation：表示State。

へ分け、通信規則を定めます。

## Hierarchical FSM

Combat親Stateの下にApproach、Strafe、Attackを置き、共通のTarget喪失遷移を親へ定義します。深くしすぎると遷移追跡が難しくなります。

## Timer

State内経過時間はSimulation tickまたは秒で管理します。Frame数と表示FPSを混同しません。State再Enter時に必ずResetします。

## Animationとの分離

Enemy Attack StateがAnimation Clip終了Eventだけを待つと、Blend/LODで壊れます。Attack DefinitionのGameplay tickを権威にし、Animationは同期表示します。

## Debug/Test

現在State、前State、滞在時間、遷移条件、拒否理由を表示します。各StateへContextを与え、期待するRequestを単体Testします。
