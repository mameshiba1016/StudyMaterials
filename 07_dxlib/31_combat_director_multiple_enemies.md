# DXライブラリ：Combat Director・複数敵

この章では、複数の敵が同じ対象と戦うときの全体調整を扱います。個々のAIが独立して最適行動を選ぶだけでは、全員が同時に攻撃する、同じ位置へ集まる、画面外から理不尽に攻撃する問題が起こります。Combat Directorは個体AIの上に立ち、攻撃権、配置、役割、戦闘の圧力を調整します。

## 1. Combat Directorの責務

- 同時に攻撃できる敵数を制限する。
- 近接・遠距離・支援などの役割を配る。
- 対象周囲の立ち位置を予約する。
- 攻撃の間隔と戦闘全体の圧力を調整する。
- 画面外や背後からの攻撃規則を統一する。
- 敵の死亡、離脱、対象変更時に予約を解放する。

Directorは個別技のAnimationや移動座標を直接操作しません。

## 2. 個体AIとの境界

```text
Enemy AI -> request role / position / attack permission
Combat Director -> grant, deny, queue, revoke
Enemy AI -> build intent from granted permission
Combat State -> validate and execute attack
```

Directorの許可は攻撃成功を保証しません。実行直前のCombat State検証は残します。

## 3. 戦闘グループ

```cpp
#include <cstdint>
#include <vector>
#include <optional>
#include <DxLib.h>

struct EntityId final
{
    std::uint32_t index{};
    std::uint32_t generation{};
    bool operator==(const EntityId&) const = default;
};

struct CombatGroupId final
{
    std::uint32_t value{};
};
```

一つのLevelに複数の独立した戦闘がある場合、対象ごと・ArenaごとにGroupを分けます。

## 4. 参加者Snapshot

```cpp
enum class EnemyRole
{
    Unassigned,
    MeleeAttacker,
    RangedAttacker,
    Support,
    Flanker,
    Waiting
};

struct CombatantSnapshot final
{
    EntityId id{};
    VECTOR position{};
    VECTOR forward{VGet(0, 0, 1)};
    float healthRatio{1.0f};
    float distanceToTarget{};
    float threat{};
    EnemyRole preferredRole{EnemyRole::MeleeAttacker};
    bool alive{true};
    bool visibleOnScreen{};
    bool currentlyAttacking{};
    bool stunned{};
};
```

Directorは可変なEnemy本体でなく、Tick開始時のSnapshotを読みます。

## 5. Group Runtime

```cpp
struct CombatGroup final
{
    CombatGroupId id{};
    EntityId target{};
    std::vector<EntityId> members{};
    std::uint32_t generation{};
    std::uint64_t lastAttackGrantedTick{};
};
```

Targetが変わったときGenerationを増やし、以前の許可を無効化します。

## 6. 攻撃Token

```cpp
struct AttackToken final
{
    std::uint32_t tokenId{};
    std::uint32_t groupGeneration{};
    EntityId owner{};
    std::uint16_t attackId{};
    std::uint64_t grantedTick{};
    std::uint64_t expireTick{};
    float cost{1.0f};
    bool committed{};
};
```

Boolの`canAttack`だけでは、誰がいつまで何の技を許可されたか追跡できません。

## 7. Tokenの状態

```cpp
enum class TokenState
{
    Pending,
    Granted,
    Committed,
    Released,
    Expired,
    Revoked
};
```

要求、許可、攻撃開始、解放を区別すると、Token漏れを調査できます。

## 8. 攻撃予算

```cpp
struct AttackBudget final
{
    float capacity{2.0f};
    float used{};

    bool CanSpend(float cost) const
    {
        return cost >= 0.0f && used + cost <= capacity;
    }

    bool TrySpend(float cost)
    {
        if (!CanSpend(cost))
            return false;
        used += cost;
        return true;
    }

    void Refund(float cost)
    {
        used = std::max(0.0f, used - std::max(cost, 0.0f));
    }
};
```

軽攻撃を1、強攻撃を2、範囲攻撃を3とすれば、単純な人数以上に圧力を制御できます。

## 9. 攻撃要求

```cpp
struct AttackPermissionRequest final
{
    EntityId requester{};
    std::uint16_t attackId{};
    float cost{1.0f};
    float utility{};
    float distanceToTarget{};
    std::uint64_t requestTick{};
    bool visibleOnScreen{};
    bool fromBehind{};
};
```

個体AIが候補技を選び、Directorへ許可を要求します。

## 10. 要求Score

```cpp
float PermissionScore(const AttackPermissionRequest& request,
                      std::uint64_t currentTick)
{
    const float waitingBonus = static_cast<float>(
        currentTick - request.requestTick) * 0.02f;
    const float visibilityBonus = request.visibleOnScreen ? 10.0f : 0.0f;
    const float behindPenalty = request.fromBehind ? 15.0f : 0.0f;
    return request.utility + waitingBonus + visibilityBonus - behindPenalty;
}
```

待機Bonusにより、同じ強い敵だけがTokenを取り続ける飢餓を防ぎます。

## 11. 安定Sort

Scoreが同値なら要求Tick、Entity ID、連番で順序を固定します。コンテナの偶然の並びで許可結果が変わらないようにします。

## 12. Token付与

```cpp
std::optional<AttackToken> TryGrantToken(
    const AttackPermissionRequest& request,
    AttackBudget& budget,
    std::uint32_t groupGeneration,
    std::uint32_t tokenId,
    std::uint64_t currentTick,
    std::uint64_t lifetimeTicks)
{
    if (!budget.TrySpend(request.cost))
        return std::nullopt;

    return AttackToken{
        tokenId, groupGeneration, request.requester,
        request.attackId, currentTick,
        currentTick + lifetimeTicks, request.cost, false};
}
```

## 13. Token Commit

AIが許可を受けただけでは攻撃開始とは限りません。Combat Stateが技を受理した瞬間にCommittedへします。

```cpp
bool CommitToken(AttackToken& token, EntityId owner,
                 std::uint32_t groupGeneration)
{
    if (!(token.owner == owner))
        return false;
    if (token.groupGeneration != groupGeneration)
        return false;
    token.committed = true;
    return true;
}
```

## 14. Token解放

攻撃終了、Cancel、被弾中断、死亡、対象変更、期限切れのすべてでBudgetを返却します。一箇所の関数へ集約し、二重返却を防ぎます。

## 15. Scope Guardの考え方

Token所有ObjectのDestructorで解放通知を行うRAIIは漏れ防止に役立ちます。ただしDirectorが先に破棄される可能性があるため、生ポインタでのCallbackは避け、IDまたは安全な所有関係を使います。

## 16. Token期限

許可後に敵が壁へ詰まり続けてもBudgetを占有しないよう期限を設けます。Committed後の期限は攻撃最大時間に合わせて別にします。

## 17. 攻撃間隔

```cpp
bool GlobalAttackIntervalPassed(std::uint64_t currentTick,
                                std::uint64_t lastGrantedTick,
                                std::uint64_t minimumInterval)
{
    return currentTick - lastGrantedTick >= minimumInterval;
}
```

Budgetに空きがあっても、Playerへ攻撃が連続しすぎない最低間隔を設けられます。

## 18. Burst許可

常に一体ずつでは単調になります。特定状況だけ二体同時攻撃を許可し、その後長めの休止を入れるBurst PatternをData化します。

## 19. Pressure

```cpp
struct PressureState final
{
    float current{};
    float target{};
    float minimum{};
    float maximum{1.0f};
};
```

Pressureは現在の戦闘の忙しさを表します。敵数そのものではありません。

## 20. Pressureへの寄与

- 攻撃中の敵数。
- 予備動作中の強攻撃。
- 飛び道具が空中に存在する数。
- Playerの残りHP。
- 直近の被弾回数。
- Camera外の脅威。

Pressureが高い間は新規Token付与を遅らせます。

## 21. Pressure平滑化

```cpp
float MoveToward(float current, float target, float maximumDelta)
{
    if (current < target)
        return std::min(current + maximumDelta, target);
    return std::max(current - maximumDelta, target);
}
```

瞬時変化で許可が振動しないよう、上昇と下降を滑らかにします。

## 22. Player状態の利用

残りHPが低いとき圧力を弱める設計も、逆に緊張を高める設計も可能です。隠れた補正を入れる場合はDebug表示し、難易度設定と一貫させます。

## 23. 配置Slot

```cpp
struct CombatSlot final
{
    std::uint16_t index{};
    float angleRadians{};
    float radius{};
    VECTOR worldPosition{};
    std::optional<EntityId> occupant{};
    std::uint64_t reservationExpireTick{};
    bool reachable{true};
};
```

対象の周囲へ仮想的なSlotを置き、敵が同じ地点へ集まることを防ぎます。

## 24. Slot座標

```cpp
#include <cmath>

VECTOR BuildSlotPosition(VECTOR targetPosition,
                         float angleRadians,
                         float radius)
{
    return VAdd(targetPosition,
        VGet(std::sin(angleRadians) * radius,
             0.0f,
             std::cos(angleRadians) * radius));
}
```

地面の高さと歩行可能性はNavigation Queryで補正します。

## 25. Slot数

```cpp
std::vector<CombatSlot> BuildRingSlots(std::uint16_t count,
                                       float radius,
                                       VECTOR targetPosition)
{
    std::vector<CombatSlot> slots;
    slots.reserve(count);
    constexpr float twoPi = 6.28318530718f;
    for (std::uint16_t i = 0; i < count; ++i)
    {
        const float angle = twoPi * static_cast<float>(i) /
                            static_cast<float>(count);
        slots.push_back({i, angle, radius,
            BuildSlotPosition(targetPosition, angle, radius)});
    }
    return slots;
}
```

`count == 0`は呼び出し前に拒否します。

## 26. 複数Ring

近接用の内周、待機用の中周、遠距離用の外周を作ります。敵のRoleとAttack Rangeに合うRingだけを候補にします。

## 27. Slot Score

```cpp
float SlotScore(const CombatSlot& slot,
                VECTOR enemyPosition,
                VECTOR cameraForward,
                VECTOR targetPosition)
{
    const float travelCost = LengthSquared(
        VSub(slot.worldPosition, enemyPosition));
    const VECTOR fromTarget = VSub(slot.worldPosition, targetPosition);
    const float visibility = LengthSquared(fromTarget) > 0.000001f
        ? VDot(VNorm(fromTarget), cameraForward) : 0.0f;
    return -travelCost + visibility * 5.0f;
}
```

実際にはPath Cost、占有、味方距離、画面内外、役割を加えます。

## 28. Slot予約

敵が移動中もSlotを予約します。期限がない予約は、敵がStunや死亡したとき永久に残るためExpire Tickを持たせます。

## 29. Slot再配置

Targetが大きく移動したらSlot座標を更新します。ただし毎Tick別Slotを選び直すと敵が左右へ揺れるため、現在Slotに切替Bonusを与えます。

## 30. Slot Hysteresis

```cpp
bool ShouldChangeSlot(float currentScore,
                      float candidateScore,
                      float requiredAdvantage)
{
    return candidateScore >= currentScore + requiredAdvantage;
}
```

## 31. 到達不能Slot

地形外、壁内、崖の向こうのSlotをNavigation Queryで無効化します。全Slot到達不能なら直接追跡や待機へFallbackします。

## 32. Targetに近すぎる敵

内周より内側へ入った敵には後退Intentを与えます。ただし攻撃中のRoot MotionをDirectorが上書きしないよう、Combat Stateの移動所有権を尊重します。

## 33. Role割当

```cpp
struct RoleQuota final
{
    int maximumMelee{2};
    int maximumRanged{2};
    int maximumSupport{1};
    int maximumFlanker{1};
};
```

敵の能力、現在距離、空き枠、前回RoleをScore化して割り当てます。

## 34. Role固定時間

毎Tick役割を変えると移動が定まりません。最低保持時間を設け、死亡や到達不能など明確な理由でのみ早期解除します。

## 35. Melee Attacker

内周Slotと攻撃Tokenを要求します。Tokenを持たない近接敵はStrafe、威嚇、位置調整を行い、全員が棒立ちにならないようにします。

## 36. Ranged Attacker

味方や障害物を通さず撃てる射線、最小距離、Projectileの安全範囲を確認します。近すぎる場合は外周へ移動します。

## 37. Support

回復、Buff、設置物などは通常攻撃Budgetと別のSupport Budgetへできます。支援が重なりすぎないCooldownと対象予約を持ちます。

## 38. Flanker

Playerの背後へ瞬間移動させず、外周Slotから経路移動します。背後到達後すぐ攻撃せず、音や予兆で認識可能にします。

## 39. Waiting行動

攻撃権のない敵にも、歩幅調整、周回、フェイント、声、武器構えなど安全な行動を与えます。ただし視覚情報を過密にしない頻度制限が必要です。

## 40. 画面内判定

```cpp
struct ScreenVisibility final
{
    bool inFrontOfCamera{};
    bool insideViewport{};
    bool occluded{};

    bool ClearlyVisible() const
    {
        return inFrontOfCamera && insideViewport && !occluded;
    }
};
```

Viewport内だけでなくCameraの後方と遮蔽を区別します。

## 41. 画面外攻撃規則

- 強攻撃は原則として画面外から開始しない。
- 開始済みの攻撃がCamera移動で画面外になっても即Cancelしない。
- 遠距離攻撃は警告音やIndicatorを先に出す。
- 難易度やAccessibility設定で制限を変えられる。

## 42. 背後攻撃

画面外と背後は同じではありません。Player Forward、Camera Forward、画面投影を別々に評価します。背後攻撃へ追加予兆を要求できます。

## 43. Warning Event

```cpp
struct OffscreenThreatEvent final
{
    EntityId attacker{};
    VECTOR worldPosition{};
    std::uint16_t attackId{};
    std::uint32_t impactTick{};
    float urgency{};
};
```

UI矢印、警告音、Controller振動はこのEventから生成します。

## 44. Friendly Fire

敵同士の攻撃を無効にしても、Projectileや攻撃予測では味方を障害物として扱うかを決めます。味方を貫通する遠距離攻撃は不自然になりやすいため射線判定を入れます。

## 45. 攻撃予約位置

技によっては「現在Slotから実行可能か」だけでなく、攻撃終了位置が他の敵と重ならないかを予測します。Root Motionの概算軌道を予約できます。

## 46. Area Denial Budget

床範囲攻撃、設置罠、持続レーザーが同時に増えすぎないよう、攻撃Tokenとは別に画面占有や安全領域のBudgetを設けます。

## 47. Projectile Budget

```cpp
struct ProjectileBudget final
{
    int maximumActive{8};
    int active{};

    bool Available(int requested = 1) const
    {
        return requested >= 0 && active + requested <= maximumActive;
    }
};
```

Projectile消滅時に必ず返却し、Pool破棄やScene切替でもResetします。

## 48. Crowd Control状態

PlayerがStun、Down、Grab中なら新しい強攻撃を抑制できます。すでに発生済みの攻撃を消すかどうかは別規則にします。

## 49. Grace Period

Playerが大ダメージを受けた直後、起き上がった直後、Character交代直後に短い攻撃猶予を設けられます。無敵時間とは異なり、敵の新規許可を抑える仕組みです。

## 50. 難易度Profile

```cpp
struct DirectorDifficulty final
{
    float attackBudgetCapacity{2.0f};
    std::uint64_t minimumAttackIntervalTicks{20};
    float offscreenAttackPenalty{30.0f};
    int meleeQuota{2};
    int rangedQuota{2};
    float pressureRecoveryPerSecond{0.5f};
};
```

敵HPだけでなく協調の強さを難易度ごとに調整できます。

## 51. Dynamic Difficultyの注意

隠れた補正が露骨だと結果が不公平に感じられます。補正範囲を限定し、Debug表示とTelemetryで実際の動きを確認します。

## 52. Bossとの違い

Boss一体が複数攻撃源を持つ場合もBudgetは使えますが、Boss PhaseのScripted PatternをDirectorが勝手に崩さないよう、Boss専用GroupまたはOverrideを用意します。

## 53. 複数Target

Player側に複数Characterや召喚体がいる場合、ThreatでTargetを割り当てます。全敵が一人へ集中しないQuotaも設定できます。

## 54. Target死亡・交代

Target Generationが変わったらToken、Slot、Warningを無効化し、新TargetでGroupを再構築します。古いTarget位置を参照しません。

## 55. Enemy死亡

死亡Eventを受けた時点でRole、Slot、Token、Projectile予約を解放します。後で一覧を掃除するだけでは、そのFrame中にBudgetが詰まる場合があります。

## 56. Scene終了

Scene終了時は全Groupを破棄し、外部へ残ったToken Handleが無効になるようDirector Generationを増やします。

## 57. 更新順序

```text
1. Snapshotを固定
2. 死亡・離脱Eventを反映
3. Group membershipを更新
4. PressureとBudgetを更新
5. RoleとSlot要求を解決
6. 攻撃要求をScore順に解決
7. 許可結果を個体AIへ配布
8. 個体AIがIntentを作成
9. Combat Stateが最終検証
10. Commit・Release EventをDirectorへ返す
```

## 58. 同一Tickの公平性

要求を受け取るたび即許可すると、更新順が早い敵が有利です。同一Tickの要求を一度集め、まとめてScore評価します。

## 59. Command Buffer

Director更新中にEnemy死亡通知でmembers配列を変更するとIteratorが無効になります。追加・削除要求をBufferへ積み、更新境界で適用します。

## 60. Debug Draw

- Target中心に各RingとSlotを描く。
- Slotの空き、予約、到達不能を色分けする。
- Enemyから予約Slotへ線を引く。
- Token所有者を強調表示する。
- 画面外攻撃候補と拒否理由を表示する。
- PressureをGaugeで表示する。

## 61. Debug Text

```cpp
struct DirectorDebugStats final
{
    int memberCount{};
    int activeTokens{};
    int queuedRequests{};
    int occupiedSlots{};
    int rejectedByBudget{};
    int rejectedOffscreen{};
    float pressure{};
};
```

## 62. Token履歴

Token ID、所有者、技、要求Tick、付与Tick、Commit、解放理由をリングバッファへ記録します。Budget漏れの原因を追跡できます。

## 63. 拒否理由

```cpp
enum class PermissionRejectReason
{
    None,
    InvalidOwner,
    NoBudget,
    GlobalInterval,
    TooMuchPressure,
    OffscreenRestricted,
    RoleMismatch,
    GracePeriod,
    TargetInvalid
};
```

単なるfalseでなく理由を返すと、AIが待機・移動・別技選択を判断できます。

## 64. 再現可能性

要求の安定Sort、固定Tick、固定Seed、Event順の記録を行います。同じ戦闘を再生したときToken所有者が変わらないことを確認します。

## 65. 性能

- Slot再評価を毎Frame全員へ行わない。
- 距離は二乗で比較する。
- Path CostはNavigation Cacheを利用する。
- 画面判定をCamera更新後にまとめて行う。
- Group単位で配列容量を予約する。
- Debug統計で要求数と解決時間を計測する。

## 66. AI LODとの統合

Sleeping中の敵はTokenを要求しません。Tokenを持つ敵はFull更新へ昇格させます。遠距離でもProjectileを発射済みなら必要な更新を維持します。

## 67. よくある失敗：Directorが全部決める

Directorが技、Animation、移動経路まで決めると巨大化します。Directorは制約と許可、個体AIは具体的行動、Combat Stateは実行を担当します。

## 68. よくある失敗：Token漏れ

死亡時だけでなく、Stun、攻撃拒否、Path失敗、Target変更、Scene終了でも解放します。期限切れを最後の安全網にします。

## 69. よくある失敗：全敵を待機させる

攻撃権がない敵にも位置調整や安全な非攻撃行動を与えます。ただし派手なフェイントを同時に行わせすぎません。

## 70. よくある失敗：Cameraだけで公平性を決める

PlayerがCameraを回すだけで敵が攻撃不能になる設計は悪用できます。画面外Penalty、予兆、最低許可条件を組み合わせ、完全禁止を限定します。

## 71. Tokenテスト

- Budget範囲内だけTokenを付与する。
- 同じOwnerへ重複Tokenを付与しない。
- Expire後にBudgetが戻る。
- Commit済みと未Commitで期限を分ける。
- 死亡とScene終了で全予約が解放される。
- 二重ReleaseでBudgetが負数にならない。

## 72. Slotテスト

- count 0を安全に拒否する。
- Ring上へ等間隔に配置される。
- 同じSlotを二体へ付与しない。
- 到達不能Slotを候補から除く。
- 予約期限後に再利用できる。
- 僅かなScore差でSlotを切り替えない。

## 73. 公平性テスト

- 同じ敵だけがTokenを独占しない。
- 画面外強攻撃が設定どおり抑制される。
- Grace Period中に新規強攻撃が始まらない。
- 遠距離Projectile数が上限を超えない。
- Pressure上限時に新規許可を抑える。
- 固定入力で許可順を再現できる。

## 74. 負荷テスト

10、50、100体で要求生成、Slot評価、Sort、Path Cost計算を測ります。平均だけでなく最悪Frameと割り当ての偏りを確認します。

## 75. 実装順序

1. Combat GroupとAttack Tokenを作る。
2. Budgetと期限付き解放を作る。
3. 同一Tick要求をまとめてScore解決する。
4. 対象周囲の近接Slotを作る。
5. Roleと複数Ringを加える。
6. Pressureと全体攻撃間隔を加える。
7. 画面外攻撃とWarning Eventを加える。
8. Projectile・Area Denial Budgetを加える。
9. Debug Draw、履歴、負荷統計を加える。

## 76. 完成確認表

- [ ] 個体AIとDirectorの責務が分離されている。
- [ ] 攻撃許可がTokenとして追跡できる。
- [ ] 全終了経路でBudgetが返却される。
- [ ] 同一Tick要求をまとめて公平に評価する。
- [ ] 敵が同じ位置へ集まらない。
- [ ] 到達不能Slotから回復できる。
- [ ] 近接、遠距離、支援のQuotaを守る。
- [ ] 画面外攻撃に統一された規則がある。
- [ ] Pressureと難易度値をDebug表示できる。
- [ ] 固定条件で許可順を再現できる。

## 77. この章の要点

- Directorは制約と許可を担当し、個体AIの具体的行動を奪いません。
- 攻撃権を期限・所有者・Cost付きTokenとして管理します。
- 同一Tickの要求を収集してから安定した順序で解決します。
- 対象周囲のSlotと複数Ringで敵の位置を整理します。
- Role、Budget、Pressure、攻撃間隔で戦闘密度を調整します。
- 画面外攻撃には予兆と統一規則を設けます。
- 死亡、拒否、期限切れ、Scene終了の全経路で予約を解放します。
- Debug履歴と固定更新により協調判断を再現可能にします。

次章では、Boss固有のPhase、部位、攻撃Pattern、演出と戦闘状態の連携を扱います。
