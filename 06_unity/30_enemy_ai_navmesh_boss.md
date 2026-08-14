# 敵AI・NavMesh・Boss設計

> 対象: Unity 6.0、AI Navigation 2.0系。敵AIを「毎frameプレイヤーへ走るscript」ではなく、知覚、記憶、意思決定、移動、戦闘、演出を分離したsystemとして学びます。

## 1. AIの処理pipeline

```text
World / Player
  → Perception（見えた・聞こえた）
  → Memory / Blackboard（何を知っているか）
  → Decision（何をしたいか）
  → Task / Action（どう実行するか）
  → Navigation / Combat / Animation
  → Resultを再び記憶へ反映
```

NavMeshは移動可能面と経路を扱います。敵の作戦、攻撃選択、Boss Phaseを決めるAIそのものではありません。

## 2. 責任分割

```text
EnemyPerception       視覚・聴覚・damage通知
EnemyMemory           targetと最終既知位置
EnemyBrain            goal/action選択
EnemyStateMachine     排他的な大状態
EnemyNavigation       path要求と到着判定
EnemyMotor            最終移動・回転
EnemyCombat           攻撃実行可否
EnemyPresenter        Animator・Audio・VFX
EncounterDirector     複数敵の攻撃枠・出現管理
BossPhaseController   phase遷移規則
```

## 3. World Stateを明示する

```csharp
public readonly record struct EnemyWorldState(
    bool HasTarget,
    bool CanSeeTarget,
    Vector3 LastKnownTargetPosition,
    float DistanceToTarget,
    float HealthRatio,
    float StaminaRatio,
    bool IsTargetAttacking,
    double TimeSinceTargetSeen);
```

判断codeがScene中から情報を探し回らず、snapshotを入力として受け取るとtestできます。

## 4. 知覚と真実は違う

Playerの現在位置を常にAIへ渡すと、壁越しでも完全追跡します。AIが利用するのは知覚・記憶した情報です。

```text
真実: Playerは部屋Bへ移動
知覚: 最後に廊下入口で見た
記憶: 廊下入口を調査
再発見: 新しい位置へ更新
```

## 5. 視覚判定

視覚は段階的に安いfilterから行います。

1. targetがalive/targetableか。
2. 距離内か。
3. 視野角内か。
4. raycastで遮蔽されていないか。
5. 発見時間や警戒度を満たすか。

## 6. 視野角

```csharp
public static bool IsInsideVisionCone(
    Vector3 observerForward,
    Vector3 directionToTarget,
    float halfAngleDegrees)
{
    Vector3 forward = Vector3.ProjectOnPlane(observerForward, Vector3.up).normalized;
    Vector3 direction = Vector3.ProjectOnPlane(directionToTarget, Vector3.up).normalized;

    // cos値との比較はVector3.Angleより軽く、角度の意味も明示できる。
    float threshold = Mathf.Cos(halfAngleDegrees * Mathf.Deg2Rad);
    return Vector3.Dot(forward, direction) >= threshold;
}
```

目の位置から胸・頭など複数pointを見るか、一点だけにするかで遮蔽挙動が変わります。

## 7. Line of Sight

```csharp
public bool HasLineOfSight(Vector3 eye, Transform target, LayerMask blockingMask)
{
    Vector3 targetPoint = target.position + Vector3.up;
    Vector3 delta = targetPoint - eye;

    // rayが最初に何へ当たるかを見る。自身のColliderをLayerで除外する。
    if (!Physics.Raycast(
            eye,
            delta.normalized,
            out RaycastHit hit,
            delta.magnitude,
            blockingMask,
            QueryTriggerInteraction.Ignore))
        return true; // blockerがなければ見える。

    return hit.transform == target || hit.transform.IsChildOf(target);
}
```

LayerMaskへtargetも含める方式と、blockerだけを含める方式を混在させません。

## 8. 視覚を毎frame全敵で行わない

100体×複数rayを毎frame発行すると高価です。

- perception updateを5～10Hzへ落とす。
- 敵ごとに更新frameをずらす。
- 距離外では頻度を下げる。
- 空間partitionで候補を絞る。
- 必要ならRaycastCommandでbatch化する。

ただし近接戦闘中の反応は遅延が目立つため、状態別に頻度を変えます。

## 9. 聴覚event

音を実際のAudioSource volumeから逆算せず、gameplay eventとして発行します。

```csharp
public readonly record struct NoiseEvent(
    Vector3 Position,
    float Loudness,
    GameObject Source,
    NoiseType Type,
    double Time);

public enum NoiseType { Footstep, Impact, Explosion, Ability }
```

壁の減衰、距離、敵の聴覚能力から検知し、最終既知位置を更新します。

## 10. Memory

```csharp
public sealed class EnemyMemory
{
    public Transform? Target { get; private set; }
    public Vector3 LastKnownPosition { get; private set; }
    public double LastSeenTime { get; private set; }

    public void Observe(Transform target, Vector3 position, double now)
    {
        Target = target;
        LastKnownPosition = position;
        LastSeenTime = now;
    }

    public bool IsFresh(double now, double forgetAfterSeconds)
        => Target != null && now - LastSeenTime <= forgetAfterSeconds;

    public void Forget() => Target = null;
}
```

UnityEngine.Objectの破棄済みnull semanticsにも注意し、target消滅時を必ず扱います。

## 11. Alert level

```text
Unaware → Suspicious → Alerted → Combat
                  ↘ Search → Unaware
```

一瞬rayが外れただけでCombatからIdleへ戻さず、認識蓄積値とmemory timeoutを使います。

## 12. FSMの役割

大きな排他的状態にはFSMが分かりやすいです。

```csharp
public enum EnemyState
{
    Idle,
    Patrol,
    Investigate,
    Chase,
    Combat,
    Stunned,
    Dead
}
```

`Dead`への遷移はどのstateからも起こり得ます。global transitionとして扱います。

## 13. State interface

```csharp
public interface IEnemyState
{
    void Enter();
    void Tick(float deltaTime);
    void Exit();
}
```

`Enter`で購読・開始、`Exit`で解除・停止します。遷移中に直接別遷移を再入させないよう、次state要求をqueueする設計も有効です。

## 14. Behavior Tree

Behavior Treeは選択と手順の階層に向きます。

```text
Selector
├─ Sequence [Dead?, PlayDeath]
├─ Sequence [Stunned?, Recover]
├─ Sequence [CanAttack?, SelectAttack, ExecuteAttack]
├─ Sequence [HasTarget?, MoveToRange]
└─ Patrol
```

- Selector: 成功する子を探す。
- Sequence: 全子が成功するまで順番に進める。
- Decorator: 条件、反転、repeat、cooldown。
- Leaf/Task: 実際のaction。

毎tick最初から再評価するか、running nodeを保持するかを明示します。

## 15. Utility AI

複数の妥当な攻撃から連続値で選ぶ場合に有効です。

```text
AttackScore = distanceCurve(distance)
            × angleCurve(angle)
            × staminaCurve(stamina)
            × cooldownAvailability
            × repetitionPenalty
```

最大scoreだけを常に選ぶと単調になるため、上位候補のweighted randomや慣性を加えます。乱数seedを記録すると再現できます。

## 16. GOAPの位置付け

Goal-Oriented Action Planningはprecondition/effectからaction列を探索します。複雑な世界操作に強い反面、近接action一つ一つのframe制御には過剰です。大目標をplanner、実行中戦闘をFSM/BTで扱うhybridが現実的です。

## 17. NavMeshとは

NavMeshはAgentが歩行可能なpolygon集合です。AI Navigation packageはEditor/runtimeでのbuild、動的障害物、NavMesh間を結ぶLinkなどを提供します。

NavMeshが保証するのは概ね「設定されたAgent Typeが通れる面」です。animation、重力、攻撃距離、味方との戦術は別systemです。

## 18. AI Navigation package

Unity 6.0では`com.unity.ai.navigation` 2.0系が提供されています。Package Managerで導入し、使用中のUnity/Package versionに対応するdocumentを参照します。

主なcomponent:

- NavMeshSurface。
- NavMeshModifier / ModifierVolume。
- NavMeshLink。
- NavMeshAgent。
- NavMeshObstacle。

## 19. NavMeshSurface

Surfaceは収集対象、Agent Type、Layer、build範囲を定義します。

- Scene全体を無条件に収集しない。
- visual meshとwalkable collisionを区別する。
- Agent半径・高さ・step・slopeを実寸に合わせる。
- 小さ過ぎるAgent設定で本来通れない隙間を通さない。

異なる体格の敵は別Agent Type/NavMeshが必要になる場合があります。

## 20. NavMesh Areaとcost

Areaは歩行可能/不可だけでなく移動costを持てます。

```text
Walkable cost 1
Mud      cost 3
Danger   cost 8
Jump     Link経由
Not Walkable
```

costは「禁止」ではありません。高costでも他経路がなければ通る可能性があります。禁止にはarea maskを使います。

## 21. Agent配置

Agent有効化時、TransformがNavMesh上にないと`SetDestination`が失敗します。spawn位置を`NavMesh.SamplePosition`で検証します。

```csharp
public static bool TryFindNavMeshPoint(
    Vector3 desired,
    float maxDistance,
    int areaMask,
    out Vector3 point)
{
    if (NavMesh.SamplePosition(desired, out NavMeshHit hit, maxDistance, areaMask))
    {
        point = hit.position;
        return true;
    }

    point = default;
    return false;
}
```

大きな半径でSampleすると別floorを拾うことがあるため、万能な救済処理にしません。

## 22. SetDestinationは要求

`SetDestination`直後にpathが完成しているとは限りません。`pathPending`中は結果待ちです。

```csharp
public enum ArrivalStatus { Calculating, Moving, Arrived, Partial, Invalid }

public static ArrivalStatus GetArrivalStatus(NavMeshAgent agent)
{
    if (agent.pathPending)
        return ArrivalStatus.Calculating;

    if (agent.pathStatus == NavMeshPathStatus.PathInvalid)
        return ArrivalStatus.Invalid;

    if (agent.pathStatus == NavMeshPathStatus.PathPartial)
        return ArrivalStatus.Partial;

    // remainingDistanceだけでなく速度やpath有無もgame仕様に応じて確認する。
    return agent.remainingDistance <= agent.stoppingDistance
        ? ArrivalStatus.Arrived
        : ArrivalStatus.Moving;
}
```

## 23. destination更新頻度

移動targetへ毎frame`SetDestination`する必要は通常ありません。

- targetが一定距離動いた。
- 再計算intervalが来た。
- pathがpartial/invalidになった。
- state/goalが変わった。

場合に更新します。多数Agentの同時path requestを時間分散します。

## 24. Complete・Partial・Invalid

- Complete: destinationまで経路あり。
- Partial: destinationそのものへ届かず途中まで。
- Invalid: 有効な経路なし。

Partialを永遠に追わせず、到達可能な末端で待つ、射撃へ切り替える、別goalを選ぶ、teleport防止などのfallbackを決めます。

## 25. 到着判定の罠

`remainingDistance <= stoppingDistance`だけでは:

- pathPending中に不正値。
- partial path末端を到着扱い。
- Agentが詰まっている。
- stoppingDistance内だが相手が壁の向こう。

があり得ます。path status、直線距離、line of sight、速度、経過時間を目的別に組み合わせます。

## 26. NavMeshAgentとAnimator

### Agent authority

`updatePosition/updateRotation = true`でAgentがTransformを動かし、Animatorへ速度を渡します。安定しやすい方式です。

### Animation authority

Root MotionでTransformを動かし、Agentの`nextPosition`と同期します。見た目は良い反面、simulation位置との差を管理する必要があります。

一つのTransformをAgent、Animator、combat motorが同時に書かないようauthorityを決めます。

## 27. Agent速度からAnimatorへ

```csharp
private void UpdateAnimator(NavMeshAgent agent, Animator animator)
{
    Vector3 localVelocity = transform.InverseTransformDirection(agent.velocity);
    animator.SetFloat(SpeedXHash, localVelocity.x);
    animator.SetFloat(SpeedZHash, localVelocity.z);
}
```

parameter hashは事前計算し、速度はdampingして見た目の揺れを抑えます。

## 28. 攻撃中のAgent

攻撃開始時に単純にcomponentをdisableするとNavMesh状態復帰が複雑になります。多くの場合:

- `isStopped = true`。
- `updateRotation = false`。
- pathを保持するかResetPathするか決める。
- attack motorが踏み込みを制御。
- 終了時にNavMesh上へ整合させる。

を明示します。

## 29. Local avoidance

NavMeshAgentのavoidanceは近傍Agentとの局所回避です。大域的な戦術配置ではありません。

- radius。
- avoidance priority。
- obstacle avoidance quality。
- speed/acceleration/angularSpeed。

を調整します。全Agentを最高品質にするとCPU costが増えます。

## 30. 詰まり検出

```csharp
public sealed class StuckDetector
{
    private Vector3 previousPosition;
    private float stationaryTime;

    public bool Tick(Vector3 position, bool expectsMovement, float dt)
    {
        float moved = Vector3.Distance(position, previousPosition);
        previousPosition = position;

        stationaryTime = expectsMovement && moved < 0.01f
            ? stationaryTime + dt
            : 0f;

        return stationaryTime >= 1.0f;
    }
}
```

詰まり時はrepath、別slot、短時間avoidance priority変更、goal断念を試します。いきなりteleportすると視覚的不自然や壁抜けを起こします。

## 31. NavMeshObstacleとCarving

Obstacleは局所回避対象、Carving有効時はNavMeshにholeを作ります。動く物体を常時高頻度carveすると更新costが増えます。

`Carve Only Stationary`では移動中は局所回避、一定時間停止後にholeが更新されます。carving変更のNavMesh queryへの反映には遅延がある点も考慮します。

Agent自身とObstacleを同時に競合させない構成を確認します。

## 32. NavMeshLink

分離したNavMesh間のjump、drop、door、ladderなどを表します。自動横断を無効にすれば、専用animationで移動し最後に`CompleteOffMeshLink()`できます。

```text
AgentがLinkへ到達
→ 通常navigation停止
→ jump animation/action開始
→ authored trajectoryで移動
→ landing確認
→ CompleteOffMeshLink
→ navigation再開
```

中断、死亡、Link無効化時のcleanupが必要です。

## 33. 戦闘距離へ移動する

Player位置そのものをdestinationにすると敵が重なります。攻撃rangeと角度を満たす位置を選びます。

```text
target中心のring上へ候補生成
→ NavMeshへsample
→ path到達可能性
→ 味方予約済みでない
→ camera/arena制約
→ score最良点を予約
```

## 34. Combat Slot

複数敵が一斉に同じ場所へ押し寄せないよう、target周囲にslotを置きます。

```csharp
public readonly record struct CombatSlot(
    int Id,
    Vector3 Position,
    Vector3 FacingDirection);
```

slotにはowner、予約期限、距離適性を持たせます。target移動時に全予約を毎frame破棄せず追従・再評価します。

## 35. Attack Token

画面外を含む全敵が同時攻撃すると理不尽になります。Encounter Directorが攻撃tokenを制限します。

- 通常攻撃cost 1。
- 大技cost 2。
- 全体budget 2～4。
- 攻撃終了/中断/死亡で返却。
- 長時間保持にはtimeout。

これは敵を弱くするだけでなく、攻撃の読みやすいリズムを作ります。

## 36. Attack Definition

第29章の戦闘action定義を敵も共有します。AI用の追加評価dataだけ別にします。

```csharp
[Serializable]
public sealed class AiAttackOption
{
    public CombatActionDefinition Action;
    public AnimationCurve DistanceScore = new();
    public float CooldownSeconds;
    public float MinimumAngleScore;
    public int TokenCost = 1;
}
```

damage、hitbox、animation timingをAI側で二重定義しません。

## 37. 攻撃選択

```csharp
public interface IAiConsideration
{
    // 0～1へ正規化したscoreを返す。入力snapshotだけで評価可能にする。
    float Score(in EnemyWorldState world);
}
```

scoreを掛け算すると一条件0で候補を除外できますが、要素が増えるほど全体が小さくなります。補正曲線とweightをDebug表示します。

## 38. 反応速度を人間らしくする

AIがPlayer入力を同frameで読んで完全counterすると不公平です。

- perception delay。
- decision interval。
- attack telegraph。
- turn速度上限。
- commitment/cancel制限。
- 難易度別の予測誤差。

を入れます。高難度は単なるHP増加でなく、選択の質や連携を段階的に上げます。

## 39. Telegraph

強い攻撃ほど、姿勢、色、音、溜め、camera compositionで予告します。hitbox active時刻と見た目を一致させます。予告を短くするだけの難易度上昇はaccessibilityを損ねます。

## 40. Bossは巨大な一classではない

Bossも通常のperception、navigation、combat、damageを利用し、固有部分を追加します。

```text
BossPhaseController
BossAttackSelector
BossArenaController
BossWeakPointController
BossPresentation
```

通常敵の基盤を複製してBoss専用に改造しません。

## 41. Phase遷移

```csharp
public readonly record struct PhaseContext(
    float HealthRatio,
    float ElapsedSeconds,
    int BrokenParts,
    bool IntroFinished);

public interface IPhaseCondition
{
    bool IsSatisfied(in PhaseContext context);
}
```

HP 50%だけでなく、部位破壊、時間、特定action完了を組み合わせられます。遷移は一度だけか、戻れるかを定義します。

## 42. HP閾値の飛び越し

大damageで80%から30%へ落ちると、70%と50% phase eventをどう扱うか決めます。

- 順に全遷移を実行。
- 最終phaseへ直接移る。
- 一撃damageをgateで止める。
- phase演出中はdamageを蓄積。

これはbalanceと演出の仕様であり、偶然の`if`順に任せません。

## 43. Phase transitionの安全性

遷移時に:

- 現attackをcancel/完走。
- hitboxを無効化。
- tokenを返却。
- navigationを停止。
- invulnerabilityを設定。
- addをspawn/despawn。
- arena obstacleを変更。
- camera/UI/audioへevent。

をtransactionのように揃えます。演出skipやBoss死亡でもcleanupできるようにします。

## 44. 部位破壊

HurtboxはBoss本体のHealthへdamageを渡しつつ、部位固有posture/HPも管理できます。

```text
Arm Hurtbox
  → PartHealth減少
  → Boss本体へ倍率付きdamage
  → 0でArmBroken event
  → 攻撃候補・mesh・VFXを変更
```

破壊後Collider、lock-on point、NavMesh obstacle、animation rigの整合性を更新します。

## 45. Arena管理

Arena Controllerは入口封鎖、camera bounds、落下復帰、spawn point、NavMesh変更を管理します。Boss Brainからdoor objectを直接検索しません。

dynamic obstacle/carving変更はnavigationへ即時反映されない可能性があるため、phase直後のpath再評価を設計します。

## 46. 複数AIの更新budget

全systemを毎frame同じ精度で動かしません。

| 距離・状態 | Perception | Decision | Animation/Combat |
|---|---:|---:|---:|
| 近距離戦闘 | 高頻度 | 高頻度 | full |
| 近距離非戦闘 | 中頻度 | 中頻度 | full |
| 遠距離 | 低頻度 | 低頻度 | simplified |
| 非表示・休眠 | event中心 | 極低頻度 | optional |

時間分散schedulerを使い、一frameへpeakが集中しないようにします。

## 47. AI LOD

AI LODは単にUpdateを止めるのではなく:

- 視覚ray本数。
- path再計算頻度。
- decision tree評価頻度。
- animation更新mode。
- avoidance quality。
- 遠距離combat simulation。

を段階化します。LOD切替時にstateを失わないようにします。

## 48. Object PoolとAI

敵をpoolする場合、return時に:

- NavMeshAgentのpath/velocity停止。
- perception targetとmemory消去。
- event購読解除。
- coroutine/task cancellation。
- attack token/slot返却。
- Health、FSM、Animator parameter初期化。
- Boss固有phaseをreset。

します。`OnEnable`だけで完全初期化できるかtestします。

## 49. 非同期処理と寿命

AI taskがawait/coroutine中に敵が死亡・pool返却されることがあります。CancellationTokenまたは世代IDで古い完了通知を捨てます。

```csharp
private int generation;

public void ResetForSpawn()
{
    generation++; // 以前の非同期処理が同じinstanceへ結果を書かないための世代。
}
```

## 50. Debug visualization

- 視野coneとray。
- hearing radius。
- alert値とmemory残り時間。
- 現state/BT running node。
- Utility候補score。
- NavMesh path corners/status。
- combat slot/token owner。
- Boss phase条件。

Scene viewだけでなく選択敵のruntime overlay/logも用意します。

## 51. NavMesh path描画

```csharp
private void OnDrawGizmosSelected()
{
    if (agent == null || !agent.hasPath)
        return;

    Vector3[] corners = agent.path.corners;
    Gizmos.color = agent.pathStatus == NavMeshPathStatus.PathComplete
        ? Color.green
        : Color.yellow;

    for (int i = 1; i < corners.Length; i++)
        Gizmos.DrawLine(corners[i - 1], corners[i]);
}
```

Debug用の`path.corners`取得costも大量Agentへ常時行わず、選択時だけにします。

## 52. Test

### Edit Mode

- FSM遷移表。
- memory timeout。
- Utility score。
- attack cooldown/token。
- Boss phase閾値飛び越し。
- slot予約/解放。

### Play Mode

- spawn地点がNavMesh上か。
- Complete/Partial/Invalid path。
- Link中断・死亡。
- moving obstacle/carving。
- AgentとRoot Motion同期。
- pool再利用。

## 53. Performance計測

Profilerで:

- Physics ray/query。
- NavMesh/path request。
- Animator。
- GC allocation。
- AI decision tick。
- instantiate/destroy。

をmarker別に確認します。「AIが重い」では原因が粗過ぎます。自作systemへProfilerMarkerを入れます。

## 54. よくある失敗

### Player Transformを常時読む

壁越し追跡になる。PerceptionとMemoryを経由する。

### 毎frame SetDestination

多数敵でpath更新が集中する。移動量とintervalで制御する。

### remainingDistanceだけで到着

pending/partial/invalidを誤判定する。statusを合わせて見る。

### 全敵がPlayer位置へ移動

重なりと押し合いになる。combat slotを使う。

### 全敵が同時攻撃

読みづらい。token/budgetとtelegraphを設計する。

### Bossだけ別system

重複と不整合になる。通常戦闘基盤へphase等を追加する。

### Animator/Agent/ScriptがTransformを同時更新

震え・warpになる。movement authorityを一つにする。

## 55. 実装順

1. 一体のIdle/Chase。
2. 視覚・memory・捜索。
3. NavMesh path statusと到着判定。
4. Combat range/slot。
5. 一攻撃とcooldown。
6. Utility選択。
7. 複数敵token。
8. obstacle/link/stuck recovery。
9. Boss phase/部位/arena。
10. AI LOD、debug、test、profile。

## 56. 完成確認表

- [ ] 知覚情報とworldの真実を分離した。
- [ ] memory timeoutとlast known positionがある。
- [ ] AI判断をsnapshot入力でtestできる。
- [ ] Agent Typeを実寸で設定した。
- [ ] pathPending/statusを確認している。
- [ ] Partial/Invalid pathのfallbackがある。
- [ ] destination更新を時間分散した。
- [ ] movement/rotation authorityが一つである。
- [ ] combat slotとattack tokenを必ず解放する。
- [ ] reaction delayとtelegraphがある。
- [ ] Boss threshold飛び越し仕様がある。
- [ ] Link/死亡/pool時のcleanupがある。
- [ ] AI LOD切替でstateを失わない。
- [ ] Debug表示とProfiler計測がある。

## 57. 確認問題

1. NavMeshと意思決定AIの責任はどう違うか。
2. なぜPlayerのTransformを常時AIへ渡してはいけないか。
3. 視覚判定をどの順番で絞るべきか。
4. FSM、Behavior Tree、Utility AIの適性を説明してください。
5. `SetDestination`直後に到着判定できない理由は何か。
6. Partial pathをCompleteと同じ扱いにできない理由は何か。
7. AgentとRoot Motionを併用する際のauthority問題は何か。
8. Carvingとlocal avoidanceの違いは何か。
9. Combat SlotとAttack Tokenはそれぞれ何を解決するか。
10. BossのHP閾値を一撃で複数跨いだ場合、何を仕様化すべきか。

## 58. 公式資料

- [Unity 6 AI Navigation package](https://docs.unity3d.com/6000.0/Documentation/Manual/com.unity.ai.navigation.html)
- [NavMesh scripting API](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/AI.NavMesh.html)
- [NavMeshAgent scripting API](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/AI.NavMeshAgent.html)
- [NavMeshAgent CompleteOffMeshLink](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/AI.NavMeshAgent.CompleteOffMeshLink.html)
- [NavMesh Obstacle](https://docs.unity3d.com/2023.1/Documentation/Manual/class-NavMeshObstacle.html)

Package APIとcomponent仕様は、導入したUnity EditorおよびPackage versionに一致する資料を確認してください。
