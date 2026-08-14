# DXライブラリ：Boss・Phase・部位

この章では、大型Bossに必要なPhase、部位、攻撃Pattern、移行演出、死亡、再戦を設計します。Bossを巨大な専用クラス一つへ詰め込まず、通常のCombat、AI、Animation、Cameraを再利用しながら、Boss固有の進行を上位Controllerが調整します。

## 1. Boss制御の責務

- 現在Phaseと移行条件を管理する。
- HP閾値、時間、部位破壊、Eventから移行要求を作る。
- Phaseごとの攻撃候補とPatternを選ぶ。
- 部位HP、本体HP、姿勢値の関係を定義する。
- 移行演出中の無敵、入力、Camera、Arena状態を制御する。
- 死亡、報酬、Checkpoint、再戦を一度だけ確定する。

## 2. Bossも通常Actorである

Boss専用処理でも、移動、当たり判定、Damage、Animation、VFX、Audioは既存システムを利用します。Boss ControllerはそれらへCommandとConstraintを渡します。

## 3. Boss ID

```cpp
#include <cstdint>

struct BossInstanceId final
{
    std::uint32_t index{};
    std::uint32_t generation{};
    bool operator==(const BossInstanceId&) const = default;
};
```

再戦で同じSlotを使っても、前回の遅延Eventを新しいBossへ適用しません。

## 4. Phase ID

```cpp
using BossPhaseId = std::uint16_t;
using BossPartId = std::uint16_t;
using BossPatternId = std::uint16_t;
```

文字列比較を更新Loopで繰り返さず、読み込み時に検証済みIDへ変換します。

## 5. Phase定義

```cpp
#include <vector>

struct BossPhaseDefinition final
{
    BossPhaseId id{};
    float enterAtHealthRatio{1.0f};
    std::vector<std::uint16_t> allowedAttackIds{};
    std::vector<BossPatternId> patternIds{};
    float movementSpeedScale{1.0f};
    float damageScale{1.0f};
    float staggerResistanceScale{1.0f};
    std::uint32_t minimumDurationTicks{};
    bool transitionInvulnerable{true};
};
```

Phaseごとの調整値をソースへ散らさず、不変な定義として保持します。

## 6. Runtime

```cpp
enum class BossFlowState
{
    Dormant,
    Intro,
    Fighting,
    PhaseTransition,
    Staggered,
    FinisherWindow,
    Dying,
    Dead
};

struct BossRuntime final
{
    BossInstanceId instanceId{};
    BossFlowState flow{BossFlowState::Dormant};
    BossPhaseId phaseId{};
    std::uint32_t phaseGeneration{};
    std::uint32_t elapsedPhaseTicks{};
    std::uint32_t elapsedStateTicks{};
    bool deathCommitted{};
};
```

Phaseと一時的なFlow Stateを分けます。

## 7. HP比率

```cpp
#include <algorithm>

float HealthRatio(int currentHp, int maximumHp)
{
    if (maximumHp <= 0)
        return 0.0f;
    return std::clamp(static_cast<float>(currentHp) /
                      static_cast<float>(maximumHp), 0.0f, 1.0f);
}
```

0除算と範囲外を防ぎます。

## 8. 閾値の通過

```cpp
bool CrossedDownward(float previousRatio, float currentRatio,
                     float threshold)
{
    return previousRatio > threshold && currentRatio <= threshold;
}
```

`current <= threshold`だけを毎Tick見ると、移行Eventを繰り返し発行します。

## 9. 大ダメージで複数閾値を跨ぐ

70%と40%にPhaseがあるとき、80%から30%へ一撃で減る可能性があります。すべて順番に再生するか、到達すべき最新Phaseへ直接移るかを仕様として決めます。

## 10. Phase決定

```cpp
BossPhaseId FindPhaseForHealth(
    float ratio,
    const std::vector<BossPhaseDefinition>& phases)
{
    BossPhaseId result = phases.empty() ? 0 : phases.front().id;
    for (const auto& phase : phases)
        if (ratio <= phase.enterAtHealthRatio)
            result = phase.id;
    return result;
}
```

定義は閾値順に検証し、同値や逆順を読み込み時に拒否します。

## 11. Phase移行要求

```cpp
enum class PhaseTransitionReason
{
    HealthThreshold,
    PartBroken,
    TimeElapsed,
    ScriptedEvent,
    DebugCommand
};

struct PhaseTransitionRequest final
{
    BossPhaseId from{};
    BossPhaseId to{};
    PhaseTransitionReason reason{};
    int priority{};
    std::uint64_t requestTick{};
};
```

即時変更せず、現在の攻撃終了や安全なCancel Pointまで保留できます。

## 12. 強制移行の優先度

死亡はPhase移行より優先します。部位破壊移行とHP移行が同時なら、明示したPriorityと安定したTie Breakで一つを選びます。

## 13. 移行中の段階

```cpp
enum class TransitionStep
{
    Requested,
    WaitingForSafePoint,
    ExitCurrentPhase,
    PlayingCinematic,
    ApplyNewPhase,
    ResumeCombat,
    Completed
};
```

一つのBoolでは、どこで止まったか分かりません。

## 14. Safe Point

攻撃判定が出ている途中でPhaseを切り替えるとHitBoxが残ります。Combat Stateが「安全にCancel可能」と返す地点で移行します。死亡だけは強制Cancelできます。

## 15. 移行Transaction

```text
Close attack acceptance
 -> cancel active hit boxes
 -> release combat tokens
 -> lock required controls
 -> play transition presentation
 -> apply phase definition
 -> reset phase-only cooldowns
 -> unlock controls
 -> resume AI
```

途中でScene終了してもLockやTokenが残らないCleanup経路を用意します。

## 16. 無敵理由

```cpp
enum class InvulnerabilityReason
{
    Dodge,
    SpawnProtection,
    PhaseTransition,
    Cinematic,
    Debug
};
```

Phase移行終了時に無敵Boolをfalseへすると、Dodge由来の無敵まで消す危険があります。理由別Tokenを使います。

## 17. Transition Token

```cpp
struct TransitionToken final
{
    BossInstanceId owner{};
    std::uint32_t phaseGeneration{};
    std::uint32_t tokenGeneration{};
};
```

Camera、UI、Time Scaleへ渡した要求もTokenで撤回できるようにします。

## 18. 部位定義

```cpp
enum class BossPartType
{
    Core,
    Head,
    Arm,
    Leg,
    Armor,
    Weapon,
    Tail
};

struct BossPartDefinition final
{
    BossPartId id{};
    BossPartType type{BossPartType::Armor};
    int maximumHp{100};
    float bodyDamageShare{};
    float staggerMultiplier{1.0f};
    float damageMultiplier{1.0f};
    bool breakable{true};
    bool disableHitZoneWhenBroken{};
};
```

## 19. 部位Runtime

```cpp
struct BossPartRuntime final
{
    BossPartId id{};
    int currentHp{};
    bool broken{};
    std::uint32_t breakGeneration{};
};
```

定義と現在状態を分けます。

## 20. 部位Damage

```cpp
struct PartDamageResult final
{
    int appliedToPart{};
    int forwardedToBody{};
    bool newlyBroken{};
};

PartDamageResult ApplyPartDamage(const BossPartDefinition& definition,
                                 BossPartRuntime& runtime,
                                 int requestedDamage)
{
    if (runtime.broken || !definition.breakable)
        return {};

    const int amount = std::max(requestedDamage, 0);
    const int before = runtime.currentHp;
    runtime.currentHp = std::max(0, runtime.currentHp - amount);
    const int applied = before - runtime.currentHp;
    const bool broke = before > 0 && runtime.currentHp == 0;
    if (broke)
    {
        runtime.broken = true;
        ++runtime.breakGeneration;
    }

    return {applied,
        static_cast<int>(applied * definition.bodyDamageShare), broke};
}
```

## 21. 本体Damage共有

部位へ100入り、本体共有率50%なら本体へ50を渡します。本体Damageを先に確定してから部位にも同じ100を入れると二重Damageになります。

## 22. 壊れた部位への攻撃

破壊後は本体へ直接Damage、倍率変更、Hit無効など複数仕様があります。部位定義へ`brokenBehavior`を持たせ、暗黙に決めません。

## 23. Break Event

```cpp
struct BossPartBrokenEvent final
{
    BossInstanceId boss{};
    BossPartId partId{};
    std::uint32_t breakGeneration{};
    VECTOR position{};
    std::uint64_t tick{};
};
```

初めて壊れた瞬間に一度だけ発行します。

## 24. 部位とModel Frame

MV1 ModelのFrame名は読み込み時にFrame Indexへ解決します。毎Frame文字列検索せず、存在しないFrameは初期化時に検出します。

## 25. Frame位置

```cpp
VECTOR GetPartWorldPosition(int modelHandle, int frameIndex)
{
    const MATRIX matrix = MV1GetFrameLocalWorldMatrix(
        modelHandle, frameIndex);
    return VGet(matrix.m[3][0], matrix.m[3][1], matrix.m[3][2]);
}
```

行列規約と平行移動成分の位置は利用中のAPI仕様で確認します。

## 26. Hit Zone同期

Animation更新後にBone行列を取得し、Collision更新を行います。古いPoseのHit Zoneで判定しない更新順が必要です。

## 27. 部位破壊後の見た目

- Materialを破損版へ切り替える。
- Mesh表示を切り替える。
- 破片Modelを生成する。
- SocketからSmoke VFXを出す。
- Attack Setを変更する。

見た目の失敗が破壊済み戦闘状態を戻してはいけません。

## 28. 切断破片

破片は元Bossへの生ポインタを持たず、生成時のTransform、速度、寿命を値として受け取ります。Scene終了時はEffect Poolと共に安全に破棄します。

## 29. 部位による能力低下

腕破壊で技禁止、脚破壊で旋回低下、装甲破壊で倍率上昇などをModifierとしてPhaseの基礎値へ合成します。

```cpp
float EffectiveSpeedScale(float phaseScale, bool legBroken,
                          float brokenLegScale)
{
    return phaseScale * (legBroken ? brokenLegScale : 1.0f);
}
```

## 30. 攻撃候補Filter

```cpp
struct BossAttackCondition final
{
    std::uint16_t attackId{};
    BossPhaseId minimumPhase{};
    std::optional<BossPartId> requiredPart{};
    std::optional<BossPartId> forbiddenBrokenPart{};
    float minimumRange{};
    float maximumRange{};
    float weight{1.0f};
};
```

実行直前にも部位、距離、Cooldownを再検証します。

## 31. Patternとは何か

Patternは技の固定配列だけでなく、条件分岐、待機、移動、選択を含む短い戦闘Scriptです。AI全体をPatternへ置き換えず、Bossの特徴的な連携だけに使います。

## 32. Pattern Step

```cpp
enum class PatternStepType
{
    MoveToRange,
    FaceTarget,
    PlayAttack,
    WaitTicks,
    BranchOnDistance,
    BranchOnPart,
    End
};

struct PatternStep final
{
    PatternStepType type{PatternStepType::End};
    std::uint16_t valueId{};
    std::uint32_t durationTicks{};
    float parameter{};
    std::uint16_t nextOnSuccess{};
    std::uint16_t nextOnFailure{};
};
```

## 33. Pattern Runtime

```cpp
struct PatternRuntime final
{
    BossPatternId patternId{};
    std::uint16_t stepIndex{};
    std::uint32_t elapsedStepTicks{};
    std::uint32_t generation{};
    bool active{};
};
```

Step数上限や実行Budgetを設け、循環Dataで同一Tickに無限Loopしないようにします。

## 34. Step結果

```cpp
enum class StepResult
{
    Running,
    Succeeded,
    Failed,
    Interrupted
};
```

Attack要求が拒否された場合を成功扱いせず、Failure Branchか再試行へ進めます。

## 35. Pattern中断

死亡、Phase移行、Stagger、部位破壊で中断できます。中断時はActive Hit Box、Root Motion所有権、Attack TokenをCleanupします。

## 36. Pattern選択

直前Patternの繰り返しPenalty、距離適合、破壊部位、Player状態、Arena位置をScoreへ加え、上位候補から固定Seed乱数で選びます。

## 37. Anti-Repetition

```cpp
float RepetitionPenalty(BossPatternId candidate,
                        BossPatternId previous,
                        int repeatedCount)
{
    if (candidate != previous)
        return 1.0f;
    return 1.0f / static_cast<float>(std::max(repeatedCount + 1, 1));
}
```

完全禁止では候補不足時に停止するため、重みを下げる方式があります。

## 38. Enrage

時間切れ、低HP、特定部位破壊などでEnrage Modifierを加えます。Phaseと独立させると、どのPhaseでも時間条件を適用できます。

## 39. Arena State

```cpp
struct ArenaRuntime final
{
    bool entranceLocked{};
    bool exitLocked{};
    bool hazardsEnabled{};
    std::uint16_t hazardPatternId{};
};
```

Bossが直接Door Handleを操作せず、Arena ControllerへEventを送ります。

## 40. Hazard管理

床攻撃、落下物、壁などはBossの攻撃Budgetと別に上限を持ちます。Phase終了時に古いHazardを残すか消すかを定義します。

## 41. Intro

IntroはCamera、UI、入力、Boss AI、Arena Doorを協調させます。Skip時にも最終状態を一括適用し、途中のLockを残しません。

## 42. Cinematic Timeline

```cpp
struct TimelineMarker final
{
    std::uint32_t tick{};
    std::uint16_t eventId{};
    bool fired{};
};
```

時刻を通過したMarkerを一度だけ発火します。Frame飛びでも`previous < tick && current >= tick`で検出します。

## 43. Skip Transaction

Skipは単にAnimationを終了するのではなく、Camera返却、入力解除、UI表示、Boss位置、Arena状態、Timeline Markerの最終状態を確定します。

## 44. Camera Ownership

通常Camera、Lock-on Camera、Boss Intro、Phase演出、死亡演出が競合します。Priority付きCamera RequestとTokenで所有権を管理します。

## 45. UI

Boss HP BarはBoss Spawn直後ではなく戦闘開始Eventで表示し、死亡確定またはArena終了で消します。部位GaugeやPhase区切りも確定状態から描画します。

## 46. HP Gate

Phase演出前にHPが次の閾値より下へ行かないGateを設ける場合があります。

```cpp
int ClampDamageAtGate(int currentHp, int requestedDamage,
                      int gateHp)
{
    const int minimumHp = std::max(gateHp, 0);
    const int resultHp = std::max(currentHp -
        std::max(requestedDamage, 0), minimumHp);
    return currentHp - resultHp;
}
```

Gateで捨てたDamageを後で持ち越すかは仕様化します。

## 47. Gateと無敵の違い

Gateは命中とDamage演出が成立しつつHP下限を制限できます。無敵は命中自体を拒否できます。Hit結果を区別します。

## 48. Finisher Window

姿勢Breakや特定部位破壊後に短い特殊攻撃受付を開きます。通常Damageで死亡可能か、Finisher必須かを明示します。

## 49. 死亡確定

```cpp
bool TryCommitBossDeath(BossRuntime& runtime, int currentHp)
{
    if (runtime.deathCommitted || currentHp > 0)
        return false;
    runtime.deathCommitted = true;
    runtime.flow = BossFlowState::Dying;
    ++runtime.phaseGeneration;
    return true;
}
```

報酬、Achievement、死亡演出を二重発行しません。

## 50. Dying中のCleanup

攻撃判定、Projectile、Grab、Camera Lock、攻撃Token、Navigation Requestを停止します。残す死亡VFXやArena Hazardは別の所有者へ移します。

## 51. 報酬Event

報酬はAnimation Markerでなく死亡Gameplay確定から一度だけ発行します。演出SkipやModel読み込み失敗でも結果が失われません。

## 52. Checkpoint Snapshot

```cpp
struct BossCheckpoint final
{
    BossPhaseId phaseId{};
    int bodyHp{};
    std::vector<int> partHp{};
    std::vector<bool> partsBroken{};
    std::uint32_t randomSeed{};
    std::uint64_t elapsedBattleTicks{};
};
```

保存対象はIDと値です。Model Handle、生ポインタ、再生中VFXは保存しません。

## 53. Checkpoint復元

定義Assetを先に読み込み、ID検証後にRuntimeへ値を適用します。破壊済み部位のMesh、能力Modifier、Hit Zoneも再構築します。

## 54. 再戦Reset

- HP、姿勢、Cooldown、Pattern履歴を初期化する。
- 部位とMesh表示を復元する。
- Projectile、Hazard、Effectを消す。
- CameraとTime Scale Tokenを返す。
- Arena DoorとUIを初期状態へ戻す。
- Instance Generationを増やす。

## 55. Determinism

固定Tick、Boss専用Random Stream、Pattern選択ログを使います。同じSeedと入力でPhase・Pattern順を再現できるようにします。

## 56. Debug Command

- 任意Phaseへ安全に移行する。
- HPを指定比率へ変更する。
- 任意部位を破壊・復元する。
- Patternを強制実行する。
- Stagger、Enrage、Finisher Windowを切り替える。
- Introと死亡演出をSkipする。

Debug Commandも通常のRequest経路を通し、本番と異なる壊れた状態を作らないようにします。

## 57. Debug Draw

部位Hit Zone、Part ID、残りHP、攻撃範囲、狙い固定位置、Arena Hazard、安全地点を色分けして表示します。

## 58. Debug Text

```cpp
struct BossDebugStats final
{
    BossPhaseId phaseId{};
    BossFlowState flow{};
    std::uint32_t phaseGeneration{};
    BossPatternId patternId{};
    std::uint16_t patternStep{};
    int activeHitBoxes{};
    int activeHazards{};
    int pendingTransitions{};
};
```

## 59. Transition Log

要求元、旧Phase、新Phase、理由、要求Tick、開始Tick、完了Tick、Skip有無をリングバッファへ保存します。

## 60. よくある失敗：HPだけでPhase管理

HP条件だけを見ると移行中も毎Tick要求されます。現在Phase、保留要求、Transition Stepを明示します。

## 61. よくある失敗：演出が状態を確定する

Animation終了通知だけでPhaseを変えると、Animation欠落やSkipで停止します。Gameplay側が状態を所有し、演出完了またはTimeoutで進めます。

## 62. よくある失敗：部位と本体の二重Damage

一つの命中Eventから部位と本体への配分を一度だけ計算し、Commit順を固定します。

## 63. よくある失敗：移行時のToken漏れ

Camera、入力、無敵、Time Scale、Combat DirectorのTokenをTransition Contextへまとめ、完了・中断・Scene終了の全経路で返却します。

## 64. よくある失敗：Pattern無限Loop

一Tickで進められる最大Step数を設けます。Data読み込み時に無条件循環も検査します。

## 65. Phaseテスト

- 閾値直前、同値、直後を判定する。
- 一撃で複数閾値を跨ぐ。
- 移行と死亡が同Tickに発生する。
- 移行中にScene終了する。
- Skip後にすべてのLockが解除される。
- 同じ移行Eventが二度発行されない。

## 66. 部位テスト

- 部位HPより大きいDamageを受ける。
- 破壊Eventが一度だけ出る。
- 破壊後の再攻撃規則を守る。
- 本体共有Damageが二重にならない。
- 壊れた部位依存技を選ばない。
- Checkpoint復元後の見た目と能力が一致する。

## 67. Patternテスト

- Attack拒否時にFailureへ進む。
- Staggerで中断しCleanupする。
- Phase Generationが違う古い結果を捨てる。
- 循環PatternがStep Budgetで停止する。
- 固定Seedで同じ候補を選ぶ。
- 候補0件でも安全なFallbackへ進む。

## 68. 死亡テスト

- HP0で一度だけ死亡確定する。
- 部位破壊と死亡が同時に成立する。
- Dying中の追加Damageを拒否する。
- 報酬Eventを一度だけ発行する。
- 死亡演出Skip後もArenaが終了する。
- 再戦時に前回Eventを受け付けない。

## 69. 負荷テスト

大型ModelのBone更新、複数Hit Zone、Hazard、VFX、ShadowのCPU・GPU時間を別々に測ります。画面上の派手さだけで原因を推測しません。

## 70. 実装順序

1. Boss RuntimeとPhase Definitionを作る。
2. HP閾値から安全なPhase移行を作る。
3. Intro、Fighting、Dyingの最小Flowを作る。
4. 部位HPと本体Damage共有を作る。
5. 部位破壊Eventと能力Modifierを接続する。
6. Pattern Stepと中断Cleanupを作る。
7. Camera、UI、ArenaをTokenで連携する。
8. Checkpoint、Reset、再戦を作る。
9. Debug Command、履歴、境界テストを追加する。

## 71. 完成確認表

- [ ] Phaseと一時Flow Stateが分離されている。
- [ ] 閾値通過を一度だけ検出する。
- [ ] 大Damageで複数閾値を跨いでも破綻しない。
- [ ] 移行中断時に全Tokenを解放する。
- [ ] 部位Damageと本体Damageが二重にならない。
- [ ] 部位破壊Eventが一度だけ発行される。
- [ ] 壊れた部位に依存する技を選ばない。
- [ ] Pattern拒否・中断・循環を安全に処理する。
- [ ] 死亡と報酬を一度だけ確定する。
- [ ] Skipと再戦後にLockや古いEventが残らない。
- [ ] 固定Seedで戦闘進行を再現できる。

## 72. この章の要点

- Boss固有進行は上位Controllerが持ち、通常CombatやAnimationを再利用します。
- PhaseとFlow Stateを分け、移行を段階的Transactionとして扱います。
- HP閾値は通過を検出し、同じEventを繰り返しません。
- 部位、本体、姿勢のDamage配分を一度だけ確定します。
- Patternは条件付きStepとして実行し、中断時に必ずCleanupします。
- Camera、入力、無敵、Time Scaleは理由別Tokenで所有します。
- 死亡、報酬、部位破壊を一度だけCommitします。
- Checkpointと再戦では値とIDからRuntimeを再構築します。

次章では、操作Characterの交代とSupport Attackを安全に接続するCharacter交代・Supportを扱います。
