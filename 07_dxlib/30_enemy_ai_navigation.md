# DXライブラリ：Enemy AI・Navigation

この章では、敵がプレイヤーを認識し、状況を判断し、移動し、適切な攻撃を選ぶまでを実装します。AIを巨大なif文にせず、知覚、記憶、意思決定、移動、戦闘命令へ分離します。

## 1. AI全体の流れ

```text
World Snapshot
 -> Perception
 -> Memory / Blackboard
 -> Decision
 -> Navigation / Steering
 -> Combat Command
 -> Character Controller / Combat State
```

AIは移動や攻撃を直接実行せず、「移動したい」「技を使いたい」という意図を既存システムへ渡します。

## 2. 更新と描画を分離する

敵AIは描画関数で更新しません。固定更新で判断し、描画は確定済み状態を読むだけにします。画面外でも必要な敵の行動が描画有無で変化しません。

## 3. Entity ID

```cpp
#include <cstdint>

struct EntityId final
{
    std::uint32_t index{};
    std::uint32_t generation{};

    bool operator==(const EntityId&) const = default;
};
```

Indexだけでは破棄後に同じSlotへ生成された別Entityを誤認します。Generationも照合します。

## 4. World Snapshot

```cpp
#include <DxLib.h>

struct ActorSnapshot final
{
    EntityId id{};
    VECTOR position{};
    VECTOR velocity{};
    VECTOR forward{VGet(0, 0, 1)};
    int team{};
    int hp{};
    int maximumHp{1};
    bool targetable{true};
    bool dead{};
};
```

AIは可変なActor本体を各所から読む代わりに、そのTickで固定されたSnapshotを参照します。

## 5. 知覚結果

```cpp
struct PerceivedTarget final
{
    EntityId id{};
    VECTOR lastKnownPosition{};
    VECTOR lastKnownVelocity{};
    std::uint64_t lastSeenTick{};
    float distanceSquared{};
    bool visible{};
    bool audible{};
};
```

最後に見た位置を残すと、視界から消えた瞬間に完全に忘れる敵ではなくなります。

## 6. 距離は二乗で比較する

```cpp
float LengthSquared(VECTOR value)
{
    return VDot(value, value);
}

bool IsWithinRange(VECTOR from, VECTOR to, float range)
{
    const VECTOR offset = VSub(to, from);
    return LengthSquared(offset) <= range * range;
}
```

単純な範囲比較では平方根を省略できます。実距離が必要な箇所だけ平方根を使います。

## 7. 視野角判定

```cpp
#include <algorithm>
#include <cmath>

bool IsInsideFieldOfView(VECTOR observerForward,
                         VECTOR toTarget,
                         float halfAngleRadians)
{
    if (LengthSquared(toTarget) <= 0.000001f)
        return true;

    const VECTOR forward = VNorm(observerForward);
    const VECTOR direction = VNorm(toTarget);
    const float minimumDot = std::cos(halfAngleRadians);
    return VDot(forward, direction) >= minimumDot;
}
```

角度を毎回`acos`で求めず、Cosとの内積比較にします。DegreeとRadianを混同しません。

## 8. 視線Ray

```cpp
bool HasLineOfSight(VECTOR eye, VECTOR target,
                    int stageModelHandle)
{
    const MV1_COLL_RESULT_POLY hit =
        MV1CollCheck_Line(stageModelHandle, -1, eye, target);
    return hit.HitFlag == FALSE;
}
```

実際には対象Colliderへの命中を許可し、壁Layerだけを遮蔽物として扱います。Ray回数は計測します。

## 9. 目の位置

足元からRayを飛ばすと低い障害物に遮られます。敵と対象のEye Socketまたは身長Offsetを使い、必要なら胸と頭へ複数Rayを飛ばします。

## 10. 聴覚Stimulus

```cpp
struct SoundStimulus final
{
    VECTOR position{};
    float loudness{1.0f};
    float maximumRange{10.0f};
    int team{};
    std::uint64_t tick{};
};
```

Audio再生そのものを検索せず、足音や攻撃がGameplay用Stimulusを発行します。音量設定0でもAIの聴覚は変わりません。

## 11. 聴覚判定

```cpp
bool CanHear(VECTOR listenerPosition,
             const SoundStimulus& sound,
             float hearingScale)
{
    const float range = sound.maximumRange * sound.loudness * hearingScale;
    return IsWithinRange(listenerPosition, sound.position,
                         std::max(range, 0.0f));
}
```

壁による減衰を加える場合も、視覚Rayとは別の規則にします。

## 12. Blackboard

```cpp
#include <optional>

struct EnemyBlackboard final
{
    std::optional<EntityId> targetId{};
    VECTOR lastKnownTargetPosition{};
    VECTOR homePosition{};
    float targetDistance{};
    float healthRatio{1.0f};
    std::uint64_t targetLastSeenTick{};
    bool hasLineOfSight{};
    bool recentlyDamaged{};
    bool attackSlotGranted{};
};
```

判断に必要な共有情報を明示します。任意の文字列Keyと型なし値だけのBlackboardはタイプミスを見つけにくくなります。

## 13. 記憶時間

```cpp
bool HasFreshTargetMemory(const EnemyBlackboard& board,
                          std::uint64_t currentTick,
                          std::uint64_t memoryTicks)
{
    if (!board.targetId)
        return false;
    return currentTick - board.targetLastSeenTick <= memoryTicks;
}
```

Tickの周回を許容する符号なし整数の差を使います。非常に長い稼働時間も考慮します。

## 14. Target候補のScore

```cpp
float TargetScore(const ActorSnapshot& self,
                  const PerceivedTarget& target,
                  bool damagedByTarget)
{
    float score = 0.0f;
    score += target.visible ? 100.0f : 0.0f;
    score += target.audible ? 20.0f : 0.0f;
    score += damagedByTarget ? 150.0f : 0.0f;
    score -= target.distanceSquared * 0.01f;
    return score;
}
```

最寄りだけでなく、視認、挑発、被ダメージ、役割などをScoreへできます。

## 15. Target切替のHysteresis

```cpp
bool ShouldSwitchTarget(float currentScore, float candidateScore,
                        float requiredAdvantage)
{
    return candidateScore >= currentScore + requiredAdvantage;
}
```

少しScoreが変わるたび対象を往復しないよう、新候補が明確に優れる場合だけ切り替えます。

## 16. AI State

```cpp
enum class EnemyState
{
    Idle,
    Patrol,
    Investigate,
    Chase,
    Strafe,
    PrepareAttack,
    Attack,
    Recover,
    Retreat,
    Stunned,
    ReturnHome,
    Dead
};
```

Stateは現在何をしているかを表し、遷移条件は一箇所に集めます。

## 17. State Runtime

```cpp
struct EnemyStateRuntime final
{
    EnemyState current{EnemyState::Idle};
    EnemyState previous{EnemyState::Idle};
    std::uint32_t elapsedTicks{};
    std::uint32_t generation{};
};
```

Generationを持つと、非同期結果が古いState要求だったか判定できます。

## 18. State遷移

```cpp
void ChangeState(EnemyStateRuntime& state, EnemyState next)
{
    if (state.current == next)
        return;
    state.previous = state.current;
    state.current = next;
    state.elapsedTicks = 0;
    ++state.generation;
}
```

EnterとExit処理が必要なら、変更関数内で決まった順番に呼びます。

## 19. 最優先遷移

死亡、強制怯み、ガードブレイクなどは通常判断より先に処理します。攻撃選択後に死亡判定すると、死亡した敵が攻撃命令を出します。

```cpp
bool ApplyForcedTransition(bool dead, bool stunned,
                           EnemyStateRuntime& state)
{
    if (dead)
    {
        ChangeState(state, EnemyState::Dead);
        return true;
    }
    if (stunned)
    {
        ChangeState(state, EnemyState::Stunned);
        return true;
    }
    return false;
}
```

## 20. 意図を返す

```cpp
struct EnemyIntent final
{
    VECTOR desiredMoveDirection{};
    VECTOR desiredFacingDirection{};
    float desiredSpeedScale{};
    std::optional<std::uint16_t> requestedAttackId{};
    bool guard{};
    bool dodge{};
};
```

AIがCharacter座標やCombat Stateを直接変更せず、Player入力と似た命令形式へ変換します。

## 21. IdleとPatrol

Idleでは一定時間周囲を観察し、PatrolではWaypoint間を移動します。Waypoint到達判定には完全一致でなく半径を使います。

```cpp
bool ReachedPoint(VECTOR position, VECTOR point, float radius)
{
    return IsWithinRange(position, point, radius);
}
```

## 22. Investigate

音を聞いたが対象を見ていない場合、音源の位置へ移動し、到達後に周囲を確認します。新しいStimulusが来たら目的地を更新します。

## 23. Chase

対象の現在位置へ一直線に進むだけでは障害物へ詰まります。視線が通り、近距離なら直接Steering、遮蔽時や遠距離ならPathへ切り替えます。

## 24. 攻撃距離の帯

```cpp
struct DesiredRange final
{
    float minimum{1.5f};
    float preferred{2.5f};
    float maximum{3.5f};
};
```

最大距離だけでなく最小距離を持つと、近すぎる場合に後退できます。

## 25. Range Intent

```cpp
float RangeMoveSign(float distance, const DesiredRange& range)
{
    if (distance < range.minimum)
        return -1.0f;
    if (distance > range.maximum)
        return 1.0f;
    return 0.0f;
}
```

境界付近の振動を防ぐにはEnterとExitで異なる距離を使うHysteresisも加えます。

## 26. Strafe

```cpp
VECTOR BuildStrafeDirection(VECTOR up, VECTOR toTarget,
                            float sideSign)
{
    if (LengthSquared(toTarget) <= 0.000001f)
        return VGet(0, 0, 0);
    return VScale(VNorm(VCross(up, VNorm(toTarget))), sideSign);
}
```

左右方向を頻繁に反転せず、一定時間または障害物まで同じSideを維持します。

## 27. Facing

移動方向と向きを別にします。Strafe中は横移動しながら対象を向き、Retreat中は後退しながら対象を監視できます。

## 28. 旋回速度

```cpp
float MoveAngleToward(float current, float target,
                      float maximumDelta)
{
    float delta = target - current;
    constexpr float pi = 3.14159265359f;
    constexpr float twoPi = pi * 2.0f;
    while (delta > pi) delta -= twoPi;
    while (delta < -pi) delta += twoPi;
    delta = std::clamp(delta, -maximumDelta, maximumDelta);
    return current + delta;
}
```

瞬時に回転せず、敵ごとの最大旋回速度を適用します。

## 29. Attack Definition for AI

```cpp
struct AttackChoice final
{
    std::uint16_t attackId{};
    float minimumRange{};
    float maximumRange{};
    float minimumAngleDot{-1.0f};
    float baseWeight{1.0f};
    std::uint32_t cooldownTicks{};
    std::uint32_t preparationTicks{};
    bool requiresLineOfSight{true};
    bool requiresAttackSlot{true};
};
```

Animation名でなくCombat Stateが理解するAttack IDを要求します。

## 30. 攻撃可能判定

```cpp
bool CanUseAttack(const AttackChoice& attack,
                  float distance,
                  float facingDot,
                  bool lineOfSight,
                  bool slotGranted,
                  bool onCooldown)
{
    if (distance < attack.minimumRange ||
        distance > attack.maximumRange)
        return false;
    if (facingDot < attack.minimumAngleDot)
        return false;
    if (attack.requiresLineOfSight && !lineOfSight)
        return false;
    if (attack.requiresAttackSlot && !slotGranted)
        return false;
    return !onCooldown;
}
```

選択と実行可能性を分け、実行直前にも条件を再確認します。

## 31. Weighted Selection

```cpp
struct WeightedAttack final
{
    std::uint16_t id{};
    float weight{};
};

std::optional<std::uint16_t> SelectWeightedAttack(
    const std::vector<WeightedAttack>& choices, float random01)
{
    float total = 0.0f;
    for (const auto& choice : choices)
        total += std::max(choice.weight, 0.0f);
    if (total <= 0.0f)
        return std::nullopt;

    float cursor = std::clamp(random01, 0.0f, 0.999999f) * total;
    for (const auto& choice : choices)
    {
        cursor -= std::max(choice.weight, 0.0f);
        if (cursor < 0.0f)
            return choice.id;
    }
    return choices.back().id;
}
```

乱数器を注入し、Seed固定で選択を再現できるようにします。

## 32. Utility Score

Weightへ距離適合、対象状態、直前に使った技、味方との役割を掛けます。完全な最大値だけを選ぶと同じ技に偏るため、上位候補から重み付き選択する方法があります。

## 33. Cooldown

```cpp
struct AttackCooldown final
{
    std::uint16_t attackId{};
    std::uint64_t readyTick{};

    bool Ready(std::uint64_t currentTick) const
    {
        return currentTick >= readyTick;
    }
};
```

Animation終了ではなくCombat上の実行確定時にCooldownを開始します。

## 34. Telegraph

強攻撃はPrepareAttack状態で予備動作を見せます。予備動作中に対象位置を追従し続けるか、途中で狙いを固定するかが避けやすさを左右します。

## 35. Aim Lock Tick

```cpp
struct AttackPreparation final
{
    std::uint32_t elapsedTicks{};
    std::uint32_t aimLockTick{};
    VECTOR lockedTargetPosition{};
    bool aimLocked{};
};
```

狙い固定後は対象の瞬間移動へ追従せず、予兆と実攻撃の整合性を保ちます。

## 36. 攻撃失敗

Combat Stateが攻撃要求を拒否する場合があります。AIは成功を仮定せず、受付結果を読み、失敗なら短い再判断Delayを入れます。

## 37. Navigation Graph

```cpp
struct NavNode final
{
    std::uint32_t id{};
    VECTOR position{};
    std::vector<std::uint32_t> neighbors{};
};

struct NavGraph final
{
    std::vector<NavNode> nodes{};
};
```

最初は手置きWaypoint Graphでも構いません。後でNavMeshへ置換できるよう、AIはPath Query interfaceだけを使います。

## 38. A*のRecord

```cpp
struct AStarRecord final
{
    std::uint32_t nodeId{};
    float costFromStart{};
    float estimatedTotal{};
    std::uint32_t parentId{};
    bool hasParent{};
    bool closed{};
};
```

`costFromStart`がg、残距離推定がh、`estimatedTotal`がg+hです。

## 39. Heuristic

```cpp
float EuclideanHeuristic(VECTOR from, VECTOR to)
{
    const VECTOR delta = VSub(to, from);
    return std::sqrt(LengthSquared(delta));
}
```

Heuristicが実際の最小Costを過大評価しないと、A*は最短経路を保証しやすくなります。

## 40. Path Result

```cpp
enum class PathStatus
{
    Success,
    Partial,
    Unreachable,
    InvalidStart,
    InvalidGoal,
    BudgetExceeded
};

struct PathResult final
{
    PathStatus status{PathStatus::Unreachable};
    std::vector<VECTOR> points{};
};
```

失敗を空配列だけで表さず、理由を返します。

## 41. Nearest Node

全Node線形探索は小規模なら十分です。大規模LevelではGrid、BVH、KD-treeなどで近傍検索を高速化します。最適化前にNode数とQuery時間を測ります。

## 42. Path追従

```cpp
struct PathFollower final
{
    std::vector<VECTOR> points{};
    std::size_t nextIndex{};
    float acceptanceRadius{0.5f};

    bool Finished() const
    {
        return nextIndex >= points.size();
    }
};
```

Waypointへ完全一致するまで進むと行き過ぎて往復します。到達半径で次へ進めます。

## 43. Path再計算

毎Frame再探索しません。対象が一定距離以上移動、Pathが塞がれた、一定時間経過、現在経路から大きく外れた場合に再計算します。

## 44. Path Request ID

```cpp
struct PathRequestId final
{
    EntityId owner{};
    std::uint32_t stateGeneration{};
    std::uint32_t requestGeneration{};
};
```

探索完了時にStateが変わっていたら古い結果を捨てます。非同期化前からID設計を入れておくと安全です。

## 45. Partial Path

Goalへ到達不能でも、最も近い到達点までのPartial Pathを返せます。敵はそこで待機、遠距離攻撃、別Target選択などへ移ります。

## 46. Path Smoothing

隣接Pointをそのまま辿ると角張ります。現在位置から先のPointへLine of Sightが通るなら中間点を飛ばします。ただしCharacter半径を考慮したSweepが必要です。

## 47. Steering

```cpp
VECTOR SeekVelocity(VECTOR position, VECTOR target,
                    float maximumSpeed)
{
    const VECTOR offset = VSub(target, position);
    if (LengthSquared(offset) <= 0.000001f)
        return VGet(0, 0, 0);
    return VScale(VNorm(offset), std::max(maximumSpeed, 0.0f));
}
```

Steeringは希望速度を返し、Character Controllerが加速度と衝突を処理します。

## 48. Arrival

```cpp
VECTOR ArriveVelocity(VECTOR position, VECTOR target,
                      float slowRadius, float maximumSpeed)
{
    const VECTOR offset = VSub(target, position);
    const float distance = std::sqrt(LengthSquared(offset));
    if (distance <= 0.0001f)
        return VGet(0, 0, 0);
    const float scale = std::clamp(distance /
        std::max(slowRadius, 0.001f), 0.0f, 1.0f);
    return VScale(offset, maximumSpeed * scale / distance);
}
```

到達前に減速し、Waypoint周辺の振動を減らします。

## 49. Separation

```cpp
VECTOR Separation(VECTOR selfPosition,
                  const std::vector<VECTOR>& neighbors,
                  float radius)
{
    VECTOR force = VGet(0, 0, 0);
    for (VECTOR neighbor : neighbors)
    {
        VECTOR away = VSub(selfPosition, neighbor);
        const float distanceSq = LengthSquared(away);
        if (distanceSq <= 0.000001f || distanceSq > radius * radius)
            continue;
        force = VAdd(force, VScale(away, 1.0f / distanceSq));
    }
    return force;
}
```

近い味方ほど強く押し離します。最終速度はClampします。

## 50. Local Avoidance

Pathは静的な大域経路、Local Avoidanceは他の敵や一時障害物を避ける短期処理です。両者を同一処理へ混ぜず、希望速度を合成します。

## 51. 壁回避Probe

進行方向、左右斜めへ短いRayやSphere Castを行い、塞がれた側と逆へSteeringを加えます。点RayだけではCharacter幅を考慮できません。

## 52. 崖の回避

足元の少し先から下向きRayを飛ばし、歩行可能な地面がなければ前進を止めます。落下可能な敵はAgent設定で無効化します。

## 53. Stuck検出

```cpp
struct StuckMonitor final
{
    VECTOR samplePosition{};
    std::uint64_t sampleTick{};
    std::uint32_t failedRecoveries{};
};
```

移動意図があるのに一定時間ほぼ移動していなければ、Path再計算、横移動、Home復帰の順で回復を試みます。

## 54. HomeとLeash

敵が初期位置から追跡できる最大距離をLeashとします。超えたらReturnHomeへ移り、回復量や無敵化の有無を仕様として決めます。

## 55. Off-Mesh Link

段差ジャンプ、梯子、扉、飛び降りは通常Edgeでなく特別なLinkとして表します。Link通過中はNavigationから専用Actionへ所有権を渡します。

## 56. Spatial Grid

知覚とSeparationで全Entity総当たりを避けるため、WorldをCellへ分割します。自分の周辺Cellだけを検索し、Entity移動時に所属Cellを更新します。

## 57. AI LOD

```cpp
enum class AiLod
{
    Full,
    Reduced,
    Minimal,
    Sleeping
};
```

近距離は毎Tick、遠距離は数Tickごと、非戦闘の遠方はSleepingにします。ただしProjectile衝突や重要Eventには反応できるようにします。

## 58. 分散更新

```cpp
bool ShouldUpdateThisTick(std::uint64_t tick,
                          std::uint32_t entityIndex,
                          std::uint32_t interval)
{
    interval = std::max(interval, 1u);
    return tick % interval == entityIndex % interval;
}
```

全AIが同じFrameに重い知覚処理を行わないよう、Entity IDで更新Frameを分散します。

## 59. Decision Frequency

移動Steeringは高頻度、攻撃選択は低頻度でも動きます。知覚、Target選択、Path探索、Steeringを異なる頻度へ分けます。

## 60. Event Driven Wakeup

被弾、近距離Stimulus、Target死亡など重要Eventを受けたら、次の通常更新を待たずAIを起こします。Pollingだけに依存しません。

## 61. Debug Draw

- 視野角を扇形で描く。
- 視線Rayを可視・遮蔽で色分けする。
- Targetと最後の既知位置を線で結ぶ。
- Path Nodeと現在Pointを描く。
- 希望速度、分離Force、壁回避Forceを別色で描く。
- 攻撃可能距離の最小・最大円を描く。

## 62. Debug Text

```cpp
void DrawEnemyAiDebug(int x, int y,
                      EnemyState state,
                      float targetDistance,
                      bool hasLineOfSight)
{
    DrawFormatString(x, y, GetColor(255, 255, 255),
        "state=%d distance=%.2f los=%d",
        static_cast<int>(state), targetDistance,
        hasLineOfSight ? 1 : 0);
}
```

State滞在Tick、選んだ技、拒否理由、Path Statusも表示します。

## 63. Decision Log

リングバッファへ「いつ、どのStateから、何の条件で、どこへ遷移したか」を保存します。最終Stateだけを見るより原因を追跡できます。

## 64. 再現可能な乱数

敵ごとにSeedまたはRandom Streamを持ち、攻撃選択結果をログへ残します。フレーム時刻を直接Seedにすると同じ不具合を再現できません。

## 65. よくある失敗：巨大Update

知覚、判断、移動、Animation、攻撃実行を一つの関数へ詰めると変更が波及します。Snapshot入力とIntent出力で境界を作ります。

## 66. よくある失敗：毎Frame A*

対象の小さな移動で経路を毎回捨てると高負荷になります。再計算距離とCooldownを設けます。

## 67. よくある失敗：現在位置を書き換える

AIが壁回避のため座標を直接移動するとCharacter Controllerの接地や衝突が壊れます。希望速度だけを渡します。

## 68. よくある失敗：Target生ポインタ

Target死亡後のDangling Pointerを避けるため、Entity IDとGenerationで毎回有効性を確認します。

## 69. よくある失敗：完全な反応

AIがプレイヤー入力と同じTickで常に最適回避すると不公平に感じます。知覚遅延、反応時間、予備動作、判断CooldownをDataとして持たせます。

## 70. 知覚テスト

- 視野角の境界内外を正しく判定する。
- 壁の裏では視認しない。
- 視界から消えた後、記憶時間だけ追跡する。
- 音量0の設定でもGameplay Stimulusは届く。
- Target破棄とIndex再利用を誤認しない。

## 71. 判断テスト

- 死亡がすべての通常Stateより優先される。
- 距離外の技を選ばない。
- Cooldown中の技を選ばない。
- 固定乱数で同じ技を選ぶ。
- Target Scoreが僅差なら切り替えない。
- Attack拒否時に状態が停止しない。

## 72. Navigationテスト

- StartとGoalが同じNodeでも成功する。
- 到達不能を明示的に返す。
- Partial Pathが最寄り到達点で終わる。
- 古いRequest Generationの結果を捨てる。
- Waypoint到達半径で次へ進む。
- Stuckから有限回で回復または諦める。

## 73. 負荷テスト

敵数を段階的に増やし、知覚Ray数、Path Query数、判断時間、Steering時間を計測します。平均だけでなく最大Frame時間も確認します。

## 74. 実装順序

1. Snapshot、Blackboard、Intentを定義する。
2. 距離・視野角・視線でTargetを認識する。
3. Idle、Chase、Attack、Deadの最小Stateを作る。
4. Attack Definitionから実行可能な技を選ぶ。
5. Waypoint GraphとA*を作る。
6. Path追従、Arrival、Separationを加える。
7. Investigate、Retreat、ReturnHomeを加える。
8. Debug DrawとDecision Logを作る。
9. AI LOD、分散更新、Budgetを加える。

## 75. 完成確認表

- [ ] 視認、聴覚、記憶が別に動く。
- [ ] Target破棄後に安全に無効化される。
- [ ] Forced Stateが通常判断より優先される。
- [ ] AIはIntentだけを出し座標を直接変更しない。
- [ ] 攻撃距離・角度・Cooldownを守る。
- [ ] 予備動作後の狙い固定が機能する。
- [ ] 障害物をPathで迂回できる。
- [ ] 味方同士が完全に重ならない。
- [ ] Path失敗とStuckから回復できる。
- [ ] 遠距離AIの更新頻度を安全に落とせる。
- [ ] 固定Seedで攻撃選択を再現できる。

## 76. この章の要点

- AIを知覚、記憶、判断、Navigation、戦闘命令へ分離します。
- 可変Worldを直接読み続けず、Tick単位のSnapshotを使います。
- TargetはIDとGenerationで追跡し、ScoreとHysteresisで切り替えます。
- AIはIntentを返し、Character ControllerとCombat Stateが実行します。
- 大域Pathと局所Steeringを組み合わせます。
- 更新頻度を処理別に分け、敵ごとにFrameを分散します。
- Debug Draw、Decision Log、固定乱数で判断過程を再現します。

次章では、多数の敵が同時に攻撃しすぎないよう制御するCombat Directorと複数敵戦闘を扱います。
