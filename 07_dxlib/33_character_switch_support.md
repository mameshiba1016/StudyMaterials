# DXライブラリ：Character交代・Support

この章では、操作Characterの交代とSupport Attackを扱います。交代はModelを入れ替えるだけではありません。入力の所有権、位置、向き、速度、Target、Camera、無敵、攻撃状態、Resource、Cooldownを決まった順序で移譲するTransactionです。

## 1. 交代システムの責務

- Party Memberと現在操作Characterを管理する。
- 交代要求を検証し、実行可能な時点までBufferする。
- 退場側と登場側の状態を安全に切り替える。
- 位置、向き、速度、Target、Cameraを引き継ぐ。
- 交代攻撃、回避交代、Parry Supportなどを開始する。
- Cooldown、Energy、死亡、場外状態を考慮する。
- 中断、Scene終了、再戦でも所有権を残さない。

## 2. Party Member ID

```cpp
#include <cstdint>

struct PartyMemberId final
{
    std::uint16_t slot{};
    std::uint16_t generation{};
    bool operator==(const PartyMemberId&) const = default;
};
```

Slotだけでは編成変更後の別Characterを誤認するため、Generationも照合します。

## 3. Party Member状態

```cpp
enum class MemberPresence
{
    Inactive,
    Entering,
    Active,
    Supporting,
    Exiting,
    KnockedOut
};

struct PartyMemberRuntime final
{
    PartyMemberId id{};
    MemberPresence presence{MemberPresence::Inactive};
    int currentHp{};
    int maximumHp{1};
    float energy{};
    std::uint64_t switchReadyTick{};
    std::uint32_t presenceGeneration{};
};
```

Modelの表示状態だけでなく、Gameplay上の存在状態を明示します。

## 4. Party Runtime

```cpp
#include <vector>
#include <optional>

struct PartyRuntime final
{
    std::vector<PartyMemberRuntime> members{};
    std::optional<PartyMemberId> activeMember{};
    std::uint32_t controlGeneration{};
};
```

操作対象変更時に`controlGeneration`を増やし、古い入力命令を無効化します。

## 5. 交代要求

```cpp
enum class SwitchReason
{
    Manual,
    DodgeAssist,
    ParryAssist,
    FollowUp,
    ActiveMemberKnockedOut,
    Scripted
};

struct SwitchRequest final
{
    PartyMemberId from{};
    PartyMemberId to{};
    SwitchReason reason{SwitchReason::Manual};
    std::uint64_t requestedTick{};
    std::uint64_t expireTick{};
    int priority{};
};
```

入力を受けた瞬間に交代できない場合、期限付きRequestとして保持します。

## 6. 交代拒否理由

```cpp
enum class SwitchRejectReason
{
    None,
    InvalidMember,
    SameMember,
    KnockedOut,
    Cooldown,
    StateLocked,
    InsufficientResource,
    UnsafeSpawnPosition,
    AlreadySwitching,
    RequestExpired
};
```

Boolだけでなく理由を返すと、UI表示とBuffer継続判断ができます。

## 7. 検証結果

```cpp
struct SwitchValidation final
{
    bool allowed{};
    bool bufferable{};
    SwitchRejectReason reason{SwitchRejectReason::None};
};
```

Cooldown中はBuffer可能、死亡中はBuffer不可などを区別できます。

## 8. 基本検証

```cpp
SwitchValidation ValidateSwitch(const PartyMemberRuntime& from,
                                const PartyMemberRuntime& to,
                                std::uint64_t currentTick,
                                bool stateLocked)
{
    if (from.id == to.id)
        return {false, false, SwitchRejectReason::SameMember};
    if (to.presence == MemberPresence::KnockedOut || to.currentHp <= 0)
        return {false, false, SwitchRejectReason::KnockedOut};
    if (currentTick < to.switchReadyTick)
        return {false, true, SwitchRejectReason::Cooldown};
    if (stateLocked)
        return {false, true, SwitchRejectReason::StateLocked};
    return {true, false, SwitchRejectReason::None};
}
```

実行直前にもう一度検証します。

## 9. 交代Transaction

```text
Validate request
 -> reserve incoming member
 -> choose safe spawn transform
 -> close outgoing control
 -> cancel or hand over combat state
 -> apply incoming transform and target
 -> grant control and protection
 -> start switch action
 -> update camera / UI
 -> retire outgoing member
 -> commit cooldown and event
```

途中失敗時に予約とLockを戻せるRollbackを用意します。

## 10. 交代Flow State

```cpp
enum class SwitchFlowState
{
    Idle,
    Requested,
    WaitingForCancelWindow,
    PreparingIncoming,
    TransferringControl,
    PlayingEntryAction,
    RetiringOutgoing,
    Completed,
    Failed
};

struct SwitchRuntime final
{
    SwitchFlowState state{SwitchFlowState::Idle};
    std::optional<SwitchRequest> request{};
    std::uint32_t generation{};
    std::uint32_t elapsedTicks{};
};
```

## 11. Cancel Window

通常攻撃、被弾、回避、必殺技ごとに交代可能WindowをDataで定義します。Animationの見た目だけで判断せず、Combat Stateの固定Tickを参照します。

## 12. 強制交代

操作Characterが戦闘不能になった場合は通常Cancel制限を無視できます。ただしGrab中やScript中など、外部所有権を安全に解除してから実行します。

## 13. 入力の所有権

```cpp
struct ControlToken final
{
    PartyMemberId owner{};
    std::uint32_t controlGeneration{};
};

bool IsCurrentControl(const ControlToken& token,
                      const PartyRuntime& party)
{
    return party.activeMember &&
           *party.activeMember == token.owner &&
           token.controlGeneration == party.controlGeneration;
}
```

古いCharacterへ遅延入力が届くことを防ぎます。

## 14. Input Buffer引き継ぎ

交代Button自体は消費します。移動入力、Camera入力、交代直後に許可する攻撃入力を引き継ぐかはCommand種類ごとに決めます。旧CharacterのCombo専用Commandは破棄します。

## 15. Transform Snapshot

```cpp
#include <DxLib.h>

struct SwitchTransform final
{
    VECTOR position{};
    VECTOR forward{VGet(0, 0, 1)};
    VECTOR velocity{};
    bool grounded{};
};
```

位置だけでなく向きと速度を扱います。

## 16. 交代位置Policy

```cpp
enum class SwitchPositionPolicy
{
    SamePosition,
    BehindOutgoing,
    TowardTarget,
    AwayFromAttack,
    FixedSupportOffset
};
```

交代種類に応じて候補位置を作ります。

## 17. 安全な方向

```cpp
VECTOR SafeDirection(VECTOR value, VECTOR fallback)
{
    if (VDot(value, value) > 0.000001f)
        return VNorm(value);
    if (VDot(fallback, fallback) > 0.000001f)
        return VNorm(fallback);
    return VGet(0, 0, 1);
}
```

対象と同座標でもNaNを作りません。

## 18. 候補位置

```cpp
VECTOR BuildSwitchCandidate(const SwitchTransform& outgoing,
                            VECTOR targetPosition,
                            SwitchPositionPolicy policy,
                            float offset)
{
    const VECTOR toTarget = SafeDirection(
        VSub(targetPosition, outgoing.position), outgoing.forward);
    switch (policy)
    {
    case SwitchPositionPolicy::BehindOutgoing:
        return VSub(outgoing.position, VScale(outgoing.forward, offset));
    case SwitchPositionPolicy::TowardTarget:
        return VAdd(outgoing.position, VScale(toTarget, offset));
    case SwitchPositionPolicy::AwayFromAttack:
        return VSub(outgoing.position, VScale(toTarget, offset));
    default:
        return outgoing.position;
    }
}
```

## 19. Spawn位置検証

候補位置でCapsule Sweep、地面Ray、Nav領域、崖、壁めり込みを検査します。一点が失敗したら半径と角度を変えた複数候補を試します。

## 20. Candidate Search

```cpp
struct SpawnCandidate final
{
    VECTOR position{};
    float score{};
    bool valid{};
};
```

対象距離、Camera内、壁からの余裕、敵Hit Boxとの重なりをScore化します。

## 21. 安全位置がない場合

現在位置へ一時的な衝突補正付きで出す、交代を短くBufferする、緊急用地点へ出すなどのFallbackを明示します。無限探索しません。

## 22. 地面への投影

登場位置の上から下へRayを飛ばし、歩行可能法線か検証します。空中交代ではOutgoingの高度と落下速度を引き継ぐPolicyも必要です。

## 23. 速度引き継ぎ

```cpp
VECTOR TransferVelocity(VECTOR outgoingVelocity,
                        VECTOR incomingForward,
                        float retainedHorizontal,
                        bool retainVertical)
{
    VECTOR result = VScale(outgoingVelocity,
        std::clamp(retainedHorizontal, 0.0f, 1.0f));
    if (!retainVertical)
        result.y = 0.0f;
    return result;
}
```

Character差が大きい場合は最大速度へClampします。

## 24. Target引き継ぎ

Lock-on対象のEntity IDとGenerationを検証し、有効なら引き継ぎます。登場Characterの攻撃距離やCamera設定に合わせ、Target自体を勝手に変更しません。

## 25. Camera引き継ぎ

Cameraの位置を瞬時にCharacterへSnapすると酔いやすいため、追従Targetだけを切り替え、位置と回転は短時間補間します。

```cpp
struct CameraHandover final
{
    PartyMemberId from{};
    PartyMemberId to{};
    float elapsed{};
    float duration{0.15f};
};
```

## 26. Camera RequestのGeneration

連続交代中に古い補間が完了してCamera対象を戻さないよう、Control GenerationをCamera Requestへ含めます。

## 27. 退場側の状態

退場開始時に攻撃Hit Boxを停止し、Grab、Root Motion、Collision、無敵Token、Combat Director Tokenを整理します。ただし交代攻撃の残存判定を仕様で許可する場合は所有者を明示します。

## 28. 登場側の状態

Inactive中もHP、Energy、Cooldownは保持します。登場時にCombat Stateを既知のEntry Stateへ設定し、前回退場時のAnimation状態を再利用しません。

## 29. Model表示の順序

TransformとAnimationを設定してからModelを表示します。原点やBind Poseが一Frame見えることを防ぎます。

## 30. Collisionの順序

位置補正前にCollisionを有効化すると壁内から押し出されます。安全位置確定、Transform反映、Controller同期後にGameplay Collisionを有効化します。

## 31. 交代無敵

```cpp
struct SwitchProtection final
{
    std::uint32_t invulnerableBeginTick{};
    std::uint32_t invulnerableEndTick{};
    bool ignoresReaction{};
};
```

無敵、Damage軽減、怯み無効を別に定義します。

## 32. 無敵Token

交代理由付きTokenを発行し、Entry State終了または中断で返します。他の無敵理由を誤って解除しません。

## 33. 交代Cooldown

Cooldown開始点はButton押下でなく、交代Commit時にします。失敗要求でCooldownを消費しません。

```cpp
void CommitSwitchCooldown(PartyMemberRuntime& incoming,
                          std::uint64_t currentTick,
                          std::uint64_t cooldownTicks)
{
    incoming.switchReadyTick = currentTick + cooldownTicks;
}
```

## 34. 共通Cooldownと個別Cooldown

Party全体の連打制限と、Character個別の再登場制限を別に持てます。最終Ready Tickは両方の最大値です。

## 35. Resource Cost

Energyを要求する特殊交代は、検証時には予約、Commit時に消費します。途中失敗時は予約を解放し、二重消費を防ぎます。

## 36. Supportとは何か

Supportは操作権を完全に移さず、非Active Memberを一時的にWorldへ出し、決まったActionを実行させる仕組みです。

## 37. Support要求

```cpp
enum class SupportType
{
    Attack,
    Defensive,
    Parry,
    Heal,
    FollowUp
};

struct SupportRequest final
{
    PartyMemberId member{};
    SupportType type{SupportType::Attack};
    std::optional<EntityId> target{};
    VECTOR requestedPosition{};
    std::uint64_t expireTick{};
    int priority{};
};
```

## 38. Support Runtime

```cpp
enum class SupportState
{
    Inactive,
    Spawning,
    Acting,
    Recovering,
    Despawning,
    Interrupted
};

struct SupportRuntime final
{
    SupportState state{SupportState::Inactive};
    PartyMemberId member{};
    std::uint32_t generation{};
    std::uint32_t elapsedTicks{};
};
```

## 39. ActiveとSupportの同一Member禁止

現在操作中のMemberを同時にSupportとして生成しません。連続交代でSupport中のMemberへ操作権を移す場合は、Support Actionを交代Actionへ昇格する明示的な経路が必要です。

## 40. Support位置

Active Member、Target、Cameraを基準に候補を作り、Capsule検査を行います。画面外へ出す場合も攻撃予兆が認識できる位置を選びます。

## 41. Support Target

要求時のTargetが死亡したら、近い有効Targetへ再選択するかSupportを中止します。生ポインタを保持しません。

## 42. Support Attackの所有権

Supportが退場した後もProjectileが残る場合、Projectileは独立したEntityとして必要な攻撃Snapshotを所有します。退場Character本体への参照を残しません。

## 43. Defensive Support

攻撃を検知して自動発動する場合、敵の予兆Event、被弾予測、受付Windowを使用します。完全な自動防御にならないCooldownとResource制限が必要です。

## 44. Parry Support

```cpp
struct ParryAssistWindow final
{
    std::uint64_t beginTick{};
    std::uint64_t endTick{};
    EntityId threateningAttack{};

    bool Contains(std::uint64_t tick) const
    {
        return tick >= beginTick && tick <= endTick;
    }
};
```

敵Animationではなく確定したThreat Attack IDとWindowへ結び付けます。

## 45. Follow-Up Window

Stagger、Launch、Parry成功などのGameplay Eventから短いSupport受付Windowを開きます。UI表示と実受付は同じWindow Dataを参照します。

## 46. Support Cooldown

交代CooldownとSupport Cooldownを同じにするか別にするかを仕様化します。Support失敗で消費せず、Action開始Commit時に消費します。

## 47. Support Budget

同時にWorldへ存在できるParty Member数、Support Hit Box数、Projectile数を制限します。Pool上限到達でもActive Memberを消してはいけません。

## 48. Friendly Collision

ActiveとSupportのCharacter同士は押し合いを無効または弱くし、同じ位置で詰まらないようにします。敵へのCollisionと分離します。

## 49. Damageを受けるSupport

Support中に被弾可能か、無敵か、Damageだけ共有するかを決めます。見た目だけ存在して当たり判定がない場合も一貫したVFXを用意します。

## 50. Support中断

Scene終了、Member戦闘不能、強制交代、Target消失で中断します。Hit Box、Root Motion、無敵、Camera Request、Resource予約をCleanupします。

## 51. Character死亡時の自動交代

残存Memberから生存、Cooldown、編成順を評価して次を選びます。全滅ならGame Overへ遷移し、無効な交代を繰り返しません。

```cpp
std::optional<PartyMemberId> FindNextAlive(
    const PartyRuntime& party, PartyMemberId current)
{
    for (const auto& member : party.members)
        if (!(member.id == current) && member.currentHp > 0)
            return member.id;
    return std::nullopt;
}
```

## 52. 連続交代

前の交代Transaction完了前に新しい要求が来た場合、Priorityで置換、次としてQueue、拒否のいずれかを選びます。複数Transactionを同時実行しません。

## 53. 同一Tickの要求

手動交代、Parry Support、自動死亡交代が同Tickに来た場合、死亡交代を最優先にするなど明示的なPriority表を持ちます。

## 54. Event

```cpp
struct CharacterSwitchedEvent final
{
    PartyMemberId from{};
    PartyMemberId to{};
    SwitchReason reason{};
    VECTOR position{};
    std::uint32_t controlGeneration{};
    std::uint64_t tick{};
};
```

UI、Camera、Audioは確定Eventを購読します。

## 55. UI

Party IconへHP、Energy、交代Cooldown、戦闘不能、Support受付を表示します。見た目のCooldownと実際のReady Tickは同じRuntime値から計算します。

## 56. AudioとVoice

交代VoiceはCommit時に一度だけ再生します。要求時に鳴らすと失敗交代でもVoiceが出ます。連続VoiceにはGroupごとの再生制限を入れます。

## 57. VFX

退場と登場EffectはPresentation Eventから生成します。VFX生成失敗が操作権移譲を止めないようにします。

## 58. Hit Stop中の交代

入力受付はReal Timeでも、交代Commitは固定Tick境界で行います。Hit Stop解除用Timerまで止めないようTime Domainを確認します。

## 59. PauseとScene遷移

Pause中は新規交代をCommitせず、要求寿命を進めるか停止するかを統一します。Scene遷移ではすべてのTransactionとSupportをCancelします。

## 60. 保存

Party編成、HP、Energy、Cooldown残量は保存対象になり得ます。再生中のEntry Action、Model Handle、Support Pointerは保存せず、ロード後に安全なIdle状態へ再構築します。

## 61. Debug表示

- Active MemberとControl Generation。
- Switch Flow Stateと保留要求。
- 拒否理由、残りBuffer Tick、Cooldown。
- 候補Spawn位置と失敗理由。
- Support State、Target、残りAction Tick。
- Cameraと無敵Tokenの所有者。

## 62. Debug Draw

現在位置、各Spawn候補、Capsule、地面Ray、Target方向、Support移動経路を色分けします。

## 63. Transaction Log

要求、検証、待機、Commit、完了、失敗、RollbackをGeneration付きでリングバッファへ保存します。

## 64. よくある失敗：Active IDだけ変える

入力、Camera、Collision、Combat Stateの所有権が旧Characterへ残ります。交代を段階的Transactionとして扱います。

## 65. よくある失敗：安全位置を確認しない

壁内、崖外、敵Collider内へ登場します。複数候補とFallbackを用意します。

## 66. よくある失敗：無敵Boolを直接変更

交代終了時に他の無敵まで解除します。理由別Tokenで管理します。

## 67. よくある失敗：Support退場でProjectile消失

Projectileへ必要な攻撃Snapshotと所有者IDをコピーし、発射者のWorld存在と寿命を分けます。

## 68. 交代テスト

- 同一Memberへの交代を拒否する。
- Cooldown中の要求を期限までBufferする。
- Cancel Windowで正確にCommitする。
- 壁際と崖際で安全位置を選ぶ。
- 空中速度を仕様どおり引き継ぐ。
- 連続要求で操作権が二重にならない。
- 戦闘不能時に次の生存Memberへ移る。

## 69. Supportテスト

- Active Member自身をSupport生成しない。
- Target死亡時に再選択または中止する。
- Support中断時にHit Boxを消す。
- 退場後もProjectileが安全に残る。
- CooldownとResourceをCommit時だけ消費する。
- 同時Support上限を超えない。

## 70. 所有権テスト

- 古いControl Generationの入力を拒否する。
- 古いCamera補間が新Targetを上書きしない。
- Scene終了時に全Tokenを返す。
- Rollback後にIncoming予約が残らない。
- VFXやAudio失敗でも交代が完了する。

## 71. 実装順序

1. Party RuntimeとActive Memberを作る。
2. Switch Request、検証、Bufferを作る。
3. 安全な位置・向き・速度移譲を作る。
4. Control、Camera、TargetをGeneration付きで渡す。
5. Entry・Exit Actionと無敵Tokenを作る。
6. CooldownとResource予約を作る。
7. Support Runtimeと一種類のAttackを作る。
8. Parry・Follow-Up Windowを接続する。
9. Debug表示、Rollback、境界テストを追加する。

## 72. 完成確認表

- [ ] 交代を段階的Transactionとして処理する。
- [ ] 実行直前にもMemberとStateを検証する。
- [ ] 古い入力が前Characterへ届かない。
- [ ] 壁、崖、敵内部へ登場しない。
- [ ] 位置、向き、速度、Targetを仕様どおり移譲する。
- [ ] Cameraが急激にSnapしない。
- [ ] 無敵を理由別Tokenで管理する。
- [ ] CooldownとResourceをCommit時だけ消費する。
- [ ] Support退場後もProjectileが安全に動く。
- [ ] 中断とScene終了で全所有権を解放する。
- [ ] 固定Tickで交代結果を再現できる。

## 73. この章の要点

- 交代は操作権と多数の状態を移すTransactionです。
- Request、検証、Buffer、Commit、Cleanupを分離します。
- Control Generationで古い入力やCamera要求を無効化します。
- 登場位置はCollisionと地面を検査し、複数Fallbackを持ちます。
- Active交代と一時的Supportを別のFlowとして管理します。
- CooldownとResourceは成功Commit時にだけ消費します。
- 無敵、Camera、Combat所有権はTokenで管理します。
- 中断、死亡、Scene終了のすべてでCleanupします。

次章では、これまでのCamera、Character、Combat、AI、VFXを一つの高速3D戦闘Loopへ統合します。
