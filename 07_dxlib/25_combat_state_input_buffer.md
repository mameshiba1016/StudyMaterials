# DXライブラリ：Combat State・入力Buffer

この章では、Device入力を戦闘Commandへ変換し、現在Stateで実行できなかった入力を短時間保持し、許可された瞬間に一度だけ消費する仕組みを作ります。Action Gameの操作感はAnimationの見た目だけでなく、「何Frame前の入力まで拾うか」「同時入力で何を優先するか」「いつCancel可能か」で大きく変わります。

> 第4章ではPhysical InputからLogical Actionまでを扱いました。本章は、そのAction SnapshotをCombat Commandへ変換した後に集中します。

## 1. 戦闘入力Pipeline

```text
Physical device
 -> Logical Action Snapshot
 -> Input interpretation
 -> Combat Command
 -> Command Buffer
 -> State/Resource/Target validation
 -> Command consumption
 -> Combat State transition
 -> Animation/Movement/Hitbox requests
```

Player ClassがKey Codeを直接読む構造にしません。AI、Replay、Network、TestもCombat Commandを生成できます。

## 2. Raw InputとCommandの違い

```text
Raw/Action input: Attack button pressed
Combat command : LightAttack toward target 42 at simulation tick 1208
```

Commandには戦闘判断に必要な時刻、方向、Target候補等を保存します。ただし実行時に再検証すべき情報まで固定しないようにします。

## 3. Action Snapshot

```cpp
enum class ActionId : std::uint8_t
{
    Move,
    CameraLook,
    LightAttack,
    HeavyAttack,
    Skill,
    Dodge,
    Guard,
    Jump,
    LockOn,
    SwitchTarget
};

struct ButtonSnapshot final
{
    bool pressed{};
    bool held{};
    bool released{};
    float heldSeconds{};
};

struct CombatInputSnapshot final
{
    std::uint32_t simulationTick{};
    VECTOR moveDirection{};
    VECTOR cameraDirection{};
    ButtonSnapshot lightAttack{};
    ButtonSnapshot heavyAttack{};
    ButtonSnapshot dodge{};
    ButtonSnapshot guard{};
    ButtonSnapshot skill{};
};
```

一Simulation Tickに一つのSnapshotを確定し、Frame途中でDeviceを再読込しません。

## 4. Edge・Hold・Release

- Pressed：押した瞬間。攻撃開始、回避など。
- Held：押し続け。Guard、Charge、Aimなど。
- Released：離した瞬間。Charge解放など。

一つのButtonから複数の意味を作る場合、長押しThreshold前に短押しを確定してはいけない設計もあります。入力遅延とのTrade-offを明示します。

## 5. Combat Command

```cpp
enum class CombatCommandType : std::uint8_t
{
    LightAttack,
    HeavyAttack,
    ReleaseCharge,
    Skill,
    Dodge,
    GuardBegin,
    GuardEnd,
    Jump,
    LockToggle,
    SwitchTargetLeft,
    SwitchTargetRight
};

struct CombatCommand final
{
    CombatCommandType type{};
    std::uint32_t issuedTick{};
    std::uint32_t expireTick{};
    std::uint64_t sequence{};
    VECTOR inputDirection{};
    std::optional<std::uint32_t> suggestedTargetId;
    float analogMagnitude{};
};
```

`sequence` は同Tick入力の安定順序とReplay診断に使います。

## 6. Commandを値として持つ

CommandへDevice Pointer、Entity Pointer、Animation Pointerを保存しません。安定IDと値だけにすると、寿命事故を減らし、Serialize・Replay・Testが容易になります。

## 7. 入力Bufferとは

現在攻撃中で次の攻撃をまだ開始できなくても、受付Window直前の入力を短時間覚えます。

```text
Tick 100: Attack input
Tick 100: current state cannot transition yet -> buffer
Tick 103: cancel window opens
Tick 103: buffered attack is valid -> consume once
```

Bufferがないと、正確な一FrameだけButtonを押す必要があり操作が硬くなります。

## 8. Ring Buffer

```cpp
template<std::size_t Capacity>
class CombatCommandBuffer final
{
public:
    bool Push(const CombatCommand& command)
    {
        if (size_ == Capacity) return false;
        entries_[(head_ + size_) % Capacity] = command;
        ++size_;
        return true;
    }

    [[nodiscard]] std::size_t Size() const noexcept { return size_; }

private:
    std::array<CombatCommand, Capacity> entries_{};
    std::size_t head_{};
    std::size_t size_{};
};
```

固定容量なら戦闘中のAllocationを避けられます。満杯時のPolicyを必ず決めます。

## 9. 満杯時のPolicy

- 最古Commandを捨てる。
- 最低Priorityを捨てる。
- 同種Commandを置換する。
- 新規Commandを拒否する。
- Guard End等の必須Commandは専用Stateとして保持する。

入力を黙って失う場合はDebug Counterへ記録します。

## 10. 有効期限

秒ではなくSimulation Tickで管理するとReplayしやすくなります。

```cpp
constexpr std::uint32_t MillisecondsToTicks(
    std::uint32_t milliseconds,
    std::uint32_t ticksPerSecond)
{
    // 端数を切り上げ、設定より短くならないようにする。
    return (milliseconds * ticksPerSecond + 999u) / 1000u;
}
```

60Hzで100msなら6Tickです。攻撃・回避等で期限を別設定にできます。

## 11. Expireの判定

```cpp
bool IsExpired(const CombatCommand& command, std::uint32_t currentTick)
{
    return currentTick > command.expireTick;
}
```

Tick Wrapを長時間実行で考慮するならUnsigned差分を使う規約を定めます。比較方式をProject全体で統一します。

## 12. Bufferへ入れる瞬間

入力時に即実行可能でも、一度Commandへ変換して同じ消費経路を通します。

```text
Snapshot -> Command生成 -> Bufferへ追加 -> 同TickのConsumerが評価
```

即時入力だけ別経路にすると、優先度やReplay結果がずれます。

## 13. Command生成

```cpp
void InterpretCombatInput(const CombatInputSnapshot& input,
                          std::uint64_t& nextSequence,
                          CombatCommandBuffer<16>& buffer)
{
    auto push = [&](CombatCommandType type, std::uint32_t lifeTicks)
    {
        CombatCommand command{};
        command.type = type;
        command.issuedTick = input.simulationTick;
        command.expireTick = input.simulationTick + lifeTicks;
        command.sequence = nextSequence++;
        command.inputDirection = input.moveDirection;
        buffer.Push(command);
    };

    if (input.lightAttack.pressed) push(CombatCommandType::LightAttack, 7);
    if (input.heavyAttack.pressed) push(CombatCommandType::HeavyAttack, 9);
    if (input.dodge.pressed)       push(CombatCommandType::Dodge, 6);
    if (input.guard.pressed)       push(CombatCommandType::GuardBegin, 3);
    if (input.guard.released)      push(CombatCommandType::GuardEnd, 1);
}
```

期限値はData化し、固定数値を実装へ散らしません。

## 14. 同種入力のCoalesce

Button連打で同じLight Attackを多数積むと、数秒先まで自動実行されます。

- 同種の未消費Commandは新しいものへ置換する。
- 最大予約数をCombo段数以内にする。
- Press Edgeだけを積み、Heldを毎Tick積まない。
- 連打回数を別Counterとして必要数だけ保持する。

## 15. Priority

```cpp
int CommandPriority(CombatCommandType type)
{
    switch (type)
    {
    case CombatCommandType::GuardEnd: return 100;
    case CombatCommandType::Dodge: return 80;
    case CombatCommandType::Skill: return 70;
    case CombatCommandType::HeavyAttack: return 50;
    case CombatCommandType::LightAttack: return 40;
    default: return 10;
    }
}
```

Priorityは常にDodgeが攻撃をCancelできるという意味ではありません。State側のTransition Ruleが許可した候補間で比較します。

## 16. Stable Order

同Priorityなら次で決めます。

1. より古い `issuedTick`。
2. より小さい `sequence`。
3. 最後にCommand Typeの固定順。

決定的な順序にするとReplayとTestが安定します。

## 17. Combat State

```cpp
enum class CombatStateId : std::uint16_t
{
    Locomotion,
    AttackStartup,
    AttackActive,
    AttackRecovery,
    Charging,
    Guarding,
    Dodging,
    Airborne,
    HitStun,
    Knockdown,
    Dead
};
```

Animation Clip名をState名として使わず、Gameplay上の意味で定義します。

## 18. State Interface

```cpp
class ICombatState
{
public:
    virtual ~ICombatState() = default;

    virtual void Enter(CombatContext& context) = 0;
    virtual void Update(CombatContext& context,
                        std::uint32_t simulationTick) = 0;
    virtual void Exit(CombatContext& context) = 0;

    virtual bool CanConsume(const CombatCommand& command,
                            const CombatContext& context) const = 0;
};
```

State Objectを毎遷移でHeap生成せず、静的Instanceや所有済みObjectを使えます。

## 19. Enter・Update・Exit

- Enter：Timer初期化、Animation要求、移動権限設定。
- Update：Phase進行、Event処理、Transition評価。
- Exit：一時Flag解除、Hitbox停止、移動権限返却。

Exit忘れで無敵・Hitbox・入力Lockが残らないよう、State Resourceを一箇所で管理します。

## 20. Transition Request

State内から直接別StateのEnterを呼ばず、Requestを返します。

```cpp
struct StateTransitionRequest final
{
    CombatStateId destination{};
    std::optional<CombatCommand> sourceCommand;
    int priority{};
    std::string_view reason;
};
```

Update終了時に一つを確定し、Exit→State交換→Enterの順で処理します。

## 21. Reentrant Transition防止

Enter中に別Transition、Exit中にEventが起きるとStateが壊れます。

- TransitionはTick末尾へDeferredする。
- 一Tick最大遷移数を制限する。
- Enter/Exit中は遷移Queueへ積む。
- 無限遷移を検出してLogする。

## 22. Hierarchical State

```text
Alive
├─ Grounded
│  ├─ Locomotion
│  ├─ GroundAttack
│  └─ Guard
├─ Airborne
│  ├─ AirMove
│  └─ AirAttack
└─ Disabled
   ├─ HitStun
   └─ Knockdown
```

共通Ruleを親へ置くと重複を減らせます。ただし深すぎる階層は遷移理由を追いにくくします。

## 23. Orthogonal State

移動、攻撃、状態異常を一つの巨大enumへ掛け合わせると組合せ爆発します。

```text
Locomotion state: Grounded / Airborne / Traversal
Action state    : None / Attack / Dodge / Guard
Reaction state  : Normal / HitStun / Knockdown / Dead
```

最終権限と優先順位を定義し、矛盾する組合せを禁止します。

## 24. State Tag

```cpp
enum class CombatTag : std::uint32_t
{
    None             = 0,
    CanMove          = 1u << 0,
    CanTurn          = 1u << 1,
    CanAttack        = 1u << 2,
    CanDodge         = 1u << 3,
    Invulnerable     = 1u << 4,
    SuperArmor       = 1u << 5,
    Airborne         = 1u << 6
};
```

Tagだけで複雑な時間Windowを表さず、粗いCapability判定に使います。

## 25. Transition Rule

```cpp
struct TransitionRule final
{
    CombatStateId from{};
    CombatCommandType command{};
    CombatStateId to{};
    std::uint32_t openTickInState{};
    std::uint32_t closeTickInState{};
    int priority{};
    bool requireGrounded{};
    int staminaCost{};
};
```

Data化すると受付WindowとCostを調整できます。無効なWindowや未知StateをLoad時に検証します。

## 26. State内Tick

```cpp
struct CombatStateRuntime final
{
    CombatStateId id{CombatStateId::Locomotion};
    std::uint32_t enteredSimulationTick{};
    std::uint32_t elapsedTicks{};
};
```

Animation再生時間とGameplay Tickは分離します。Animation Blendや速度変更で受付Windowがずれない設計にします。

## 27. Window

```cpp
bool IsWindowOpen(std::uint32_t elapsedTick,
                  std::uint32_t openTick,
                  std::uint32_t closeTick)
{
    return elapsedTick >= openTick && elapsedTick <= closeTick;
}
```

両端を含むかを明記し、Off-by-oneをTestします。

## 28. Startup・Active・Recovery

```text
Startup : 攻撃開始からHitbox発生前
Active  : Hitboxが有効
Recovery: Hitbox終了後、通常状態へ戻るまで
```

Cancel WindowはPhaseそのものとは別Dataです。Active中にDodge可能、Recovery後半だけ次攻撃可能などを表せます。

## 29. Command Validation

```cpp
enum class CommandRejectReason
{
    None,
    Expired,
    StateDisallows,
    WindowClosed,
    InsufficientResource,
    InvalidTarget,
    NotGrounded,
    Cooldown,
    Duplicate,
    HigherPriorityChosen
};
```

単なるfalseではなく理由を残すと操作感の調査が容易です。

## 30. Validation順序

安く一般的な条件から確認します。

```text
expired?
 -> state/tag?
 -> window?
 -> grounded/airborne?
 -> resource/cooldown?
 -> target/space/collision?
```

ただしUserへ示すReject理由のPriorityも決めます。

## 31. PeekとConsume

評価中にBufferから消してはいけません。

```text
Peek candidates
 -> validate all
 -> choose one deterministically
 -> commit state transition and resource cost
 -> remove exactly selected command
```

Transition失敗時にCommandだけ消える事故を防ぎます。

## 32. Transactionとしての消費

```cpp
struct CommandExecutionPlan final
{
    std::size_t bufferIndex{};
    CombatStateId destination{};
    int staminaCost{};
    std::optional<std::uint32_t> targetId;
};

bool CommitExecution(const CommandExecutionPlan& plan,
                     CombatRuntime& runtime)
{
    if (!runtime.resources.CanSpendStamina(plan.staminaCost))
        return false;

    runtime.resources.SpendStamina(plan.staminaCost);
    runtime.commandBuffer.RemoveAt(plan.bufferIndex);
    runtime.RequestTransition(plan.destination);
    return true;
}
```

Resource消費・Buffer削除・遷移要求を一貫した順序で行います。

## 33. 一Tick一Command

基本は一Tickに一つのAction Commandだけ消費します。ただしGuard EndとHit Reaction等、別ChannelのEventは同Tickに処理できます。

複数消費する場合、順序と最大数を固定します。

## 34. Held StateはBufferと分ける

Guard Holdは過去のCommandではなく現在の入力状態です。

```text
GuardBegin edge -> Action開始Command
Guard held      -> Guard維持条件
GuardEnd edge   -> Guard終了Command/Event
```

Releaseを失って永遠にGuardしないよう、現在Held=falseも安全条件に使います。

## 35. Charge

```cpp
struct ChargeRuntime final
{
    std::uint32_t beganTick{};
    std::uint32_t currentTicks{};
    bool fullyCharged{};
};
```

Heavy PressでChargingへ入り、Heldで蓄積、ReleasedでReleaseCharge Commandを生成します。最大時間を超えたら自動解放するか保持するかをData化します。

## 36. TapとHoldの競合

同じButtonでTap AttackとHold Chargeを行う場合：

- Release時にDurationで決める：Tap結果がReleaseまで遅れる。
- Threshold到達前はTap予約、到達時にChargeへ変換する。
- 別Buttonへ分ける。

操作遅延と機能数のTrade-offです。低Latencyが重要な通常攻撃を遅らせない選択も必要です。

## 37. Chord Input

複数Button同時押しを一Commandにする場合、厳密同Tickだけでは難しいため短いChord Windowを使います。

```text
Button A pressed at tick 100
Button B pressed at tick 102
Chord window = 3 ticks -> Special Command
```

Chord成立時に元の単独Commandを取消すか、先に実行済みなら成立不可とするかを決めます。

## 38. Sequence Input

方向→攻撃などのCommand Sequenceは履歴から認識します。

```cpp
struct ActionHistoryEntry final
{
    ActionId action{};
    std::uint32_t tick{};
    VECTOR direction{};
};
```

最大履歴長と時間Windowを固定し、古いEntryを削除します。

## 39. Double Tap

同方向Pressed Edge間のTick差で判定します。Analog StickではNeutralを挟んだか、方向DotがThreshold以上かも確認します。

移動の微小揺れをDouble Tapと誤認しないようDead Zone後の方向を使います。

## 40. Direction Snapshot

攻撃方向を入力時に固定するか、実行時に再取得するかで操作感が変わります。

- Input時固定：意図を保存するがCameraが変わっても古い方向。
- Execute時再取得：最新状況に追従するが入力意図が変わる。
- Hybrid：入力方向をCamera Basisと共に保存し、Targetだけ再検証。

CommandごとにPolicyを持ちます。

## 41. Target Snapshot

Commandの `suggestedTargetId` はHintです。実行時に生存、距離、遮蔽、Generationを再検証します。無効なら近いTargetへ再取得、入力方向へ攻撃、Command拒否のいずれかをAttack Dataで選びます。

## 42. Resource Reservation

Bufferへ入れただけでStaminaを減らしません。実行Commit時に消費します。ただしUIへ「予約後残量」を表示したい場合はReservationを別管理し、Command期限切れで戻します。

## 43. Cooldown

```cpp
struct Cooldown final
{
    std::uint32_t readyTick{};

    [[nodiscard]] bool IsReady(std::uint32_t currentTick) const noexcept
    {
        return currentTick >= readyTick;
    }
};
```

Cooldown開始をButton Press、Attack開始、Hit成立、Attack終了のどこにするかをDataで明示します。

## 44. Hit Stop中の入力

Hit Stop中もPhysical InputをSampleし、CommandをBufferへ入れると操作を拾えます。

時間には二種類あります。

```text
Simulation Tick: Hit Stop中に停止する場合がある
Input Tick     : Real/Unscaled時間で進む
```

期限をどちらで減らすかを決めます。Hit Stop中に期限切れしない設計が一般に操作しやすいです。

## 45. Pause中の入力

Pause Menu操作とCombat入力をContextで分離します。

- Pause開始時にCombat Bufferを保持・消去するPolicy。
- Resume ButtonをAttackとして再利用しない。
- Pause解除FrameのHeldをPressedと誤判定しない。
- Focus復帰時のDevice状態を再同期する。

## 46. Scene遷移

Scene Load、Respawn、Character交代時に古いCommandを持ち越さないよう、Buffer Generationまたは明示Clearを使います。どのCommandを保持するかを遷移理由で決めます。

## 47. Damage Reactionの優先度

Hit ReactionはUser CommandではなくCombat Eventですが、同じTransition Arbitrationへ参加できます。

```text
Death > Forced Cutscene > Knockdown > HitStun > ParrySuccess > Dodge > Skill > Attack
```

Super Armor、Invulnerability、GuardがReaction候補を変更します。

## 48. Transition Arbitration

```cpp
std::optional<StateTransitionRequest> ChooseTransition(
    std::span<const StateTransitionRequest> requests)
{
    if (requests.empty()) return std::nullopt;

    return *std::max_element(requests.begin(), requests.end(),
        [](const auto& left, const auto& right)
        {
            if (left.priority != right.priority)
                return left.priority < right.priority;
            return static_cast<int>(left.destination) >
                   static_cast<int>(right.destination);
        });
}
```

同Priority時のTie-breakを仕様化します。

## 49. InterruptとCancel

- Interrupt：外部Eventが現在Actionを強制中断する。
- Cancel：現在Actionが許可した別Actionへ移る。
- Natural End：Action完了で通常Stateへ戻る。

同じ遷移でも理由を分け、Animation Blend、Resource Refund、VFX停止を変えます。

## 50. State Exit Reason

```cpp
enum class StateExitReason
{
    Completed,
    Canceled,
    InterruptedByHit,
    InterruptedByDeath,
    SceneReset
};
```

AttackがHit前に中断された場合のCooldownや消費Resourceを仕様化します。

## 51. Animation Eventとの関係

Animation EventはVFX・Audio等の同期に便利ですが、State遷移の唯一の時計にしません。

- Gameplay WindowはTick/Dataで決める。
- Animation Eventは対応Tickで発火させる。
- Animation速度変更でもGameplay結果を安定させる。
- Event重複・飛びをIDで防ぐ。

## 52. Timeline Event

```cpp
enum class CombatTimelineEventType
{
    EnableHitbox,
    DisableHitbox,
    OpenCancelWindow,
    CloseCancelWindow,
    ApplyRootMotionScale,
    SpawnEffect,
    PlaySound
};

struct CombatTimelineEvent final
{
    std::uint32_t tick{};
    CombatTimelineEventType type{};
    std::uint32_t payloadId{};
};
```

前回Tickから今回Tickまでに跨いだEventを全て発火します。

## 53. Event Cursor

```cpp
while (nextEventIndex < events.size() &&
       events[nextEventIndex].tick <= elapsedTick)
{
    Execute(events[nextEventIndex]);
    ++nextEventIndex;
}
```

Deltaが大きくてもEventを飛ばしません。State Exit時に未発火Eventを実行しないようCursor寿命をStateへ結びます。

## 54. Input BufferとCancel Window

Buffer期限とCancel Windowは別概念です。

```text
Command lifespan: 入力を何Tick覚えるか
Cancel window   : 現Stateが何Tickで遷移を許可するか
```

CommandがWindow開始まで生きていれば実行できます。

## 55. Late Buffer

Windowが閉じた直後の入力を次のNatural Endで実行するかを決めます。

- 次Stateへ持ち越す。
- 現Action専用Commandとして期限切れにする。
- Recovery終端だけ別Windowを開く。

意図しない遅延攻撃を防ぐため、CommandへSource State Generationを保存できます。

## 56. State Generation

```cpp
struct CombatCommand final
{
    // 前述Fieldに追加する概念。
    std::uint32_t sourceStateGeneration{};
};
```

「この攻撃中だけ有効」なCommandはGeneration不一致で破棄します。Dodge等の汎用CommandはGenerationを無視できます。

## 57. Negative Edge

Buttonを離した瞬間をNegative Edgeと呼びます。Guard解除、Charge発射、Aim解除に使います。

押下をBufferしRelease前に実行した場合も、最新Held状態と組み合わせて矛盾を防ぎます。

## 58. Input Consumptionの層

```text
UI consumes menu input
Camera consumes look/switch target
Combat consumes attack/dodge/guard
Locomotion consumes move/jump
```

同じActionを複数層が消費する場合、ContextとPriorityを決めます。消費済みFlagをSnapshotそのものへ書かず、Routing結果を別に保持します。

## 59. AI Command Source

AIはButtonを押す必要がありません。

```cpp
CombatCommand BuildAiAttackCommand(
    std::uint32_t tick,
    VECTOR direction,
    std::uint32_t targetId,
    std::uint64_t sequence)
{
    return {
        CombatCommandType::LightAttack,
        tick,
        tick + 2,
        sequence,
        direction,
        targetId,
        1.0f
    };
}
```

Playerと同じValidation・State Machineを通すことでRuleの二重実装を避けます。

## 60. Replay

ReplayにはRaw Device状態よりLogical CommandまたはInput Snapshotを保存します。

```cpp
struct RecordedCombatInput final
{
    std::uint32_t tick{};
    std::uint32_t pressedBits{};
    std::uint32_t releasedBits{};
    std::int16_t moveX{};
    std::int16_t moveY{};
};
```

Analog値を量子化し、VersionとBinding-independentなAction IDを保存します。

## 61. Determinism

- Fixed Simulation Tick。
- Stable Command Sequence。
- Stable Target ID。
- Candidate・Transitionの安定Sort。
- 浮動小数Directionの量子化または許容差。
- Random Streamの分離。

各TickでState ID、elapsed Tick、Buffer Hash、ResourceをHash化しReplay比較します。

## 62. Networkを見据えたCommand

```cpp
struct NetworkCombatCommand final
{
    std::uint32_t tick{};
    std::uint16_t sequence{};
    std::uint8_t type{};
    std::int16_t directionX{};
    std::int16_t directionY{};
    std::uint32_t targetNetId{};
};
```

Client時刻ではなくSimulation Tickを使い、重複、欠落、順序逆転をSequenceで検出します。

## 63. PredictionとRollbackへの準備

State Machineが外部副作用を直接実行するとRollbackが困難です。

- State更新は決定的なState変更とEvent生成を行う。
- Audio/VFXはEvent IDで重複防止する。
- SaveやAchievementは確定Tick後に処理する。
- Snapshot/Restore可能なRuntime Dataにする。

Local GameでもReplay Testに有効です。

## 64. Combat Event

```cpp
struct CombatEvent final
{
    std::uint64_t eventId{};
    std::uint32_t tick{};
    std::uint32_t sourceEntityId{};
    std::uint32_t payloadId{};
};
```

Animation、VFX、Audio、Cameraは確定Eventを購読し、Combat State内部へ逆依存しません。

## 65. Debug Overlay

```text
Tick: 1250
State: AttackRecovery / elapsed 18
Tags: CanTurn | CanDodge
Open windows: Dodge [12..24], Light [20..30]
Buffer:
  #42 Dodge issued=1248 expires=1254 valid=yes
  #43 Light issued=1249 expires=1256 reject=WindowClosed
Chosen: Dodge -> Dodging
```

入力されたのに出なかった理由を一画面で確認できるようにします。

## 66. Input History Visualization

Frame/TickごとにPressed、Held、Released、生成Command、消費Commandを横Timelineで表示します。Hit Stop、低FPS、Pause境界の問題を発見しやすくなります。

## 67. State Graph Visualization

現在Stateを色付きNode、可能Transitionを線、閉じたWindowを灰色で描きます。全遷移履歴をRing Bufferへ保持します。

```cpp
struct TransitionHistoryEntry final
{
    std::uint32_t tick{};
    CombatStateId from{};
    CombatStateId to{};
    StateExitReason exitReason{};
    std::optional<std::uint64_t> commandSequence;
};
```

## 68. Telemetry

- Command生成数・消費数・期限切れ数。
- 種類別Buffer滞在Tick。
- Reject理由の回数。
- Buffer Overflow回数。
- 一Tick遷移数。
- 入力からAction開始までのLatency。
- Stateごとの滞在時間。
- Cancel Window利用率。

操作感を定量的に比較できます。

## 69. Input-to-Action Latency

```text
Device sample tick
 -> Command issued tick
 -> Command consumed tick
 -> Gameplay state entered tick
 -> Visual startup first visible frame
```

各地点を記録し、BufferではなくAnimation BlendやRender Queueが遅延原因でないか切り分けます。

## 70. よくある不具合：入力が二回出る

- Heldを毎TickPressed扱いした。
- Commandを消費後にBufferから削除していない。
- Replay入力とLive入力を同時に注入した。
- Chord成立後も単独Commandを残した。
- State Enterで前TickのCommandを再評価した。
- Animation EventをLoop境界で二重発火した。

Command SequenceをLogへ出します。

## 71. よくある不具合：入力が出ない

- Pressed EdgeをFixed Step間で失った。
- Buffer期限がWindow開始前に切れる。
- Priorityの高い無効Commandが評価を遮った。
- Reject時に全BufferをClearした。
- ResourceをBuffer時点で消費した。
- UI ContextがCombat Actionを消費した。

Reject理由と期限を可視化します。

## 72. よくある不具合：遅れて勝手に攻撃する

- Buffer期限が長すぎる。
- 同種Commandを無制限に積んだ。
- State Generationを跨いで持ち越した。
- Pause中のCommandをResume後も保持した。
- Target無効時に長時間再取得を待った。

Command種類ごとの寿命と持越Policyを短く明確にします。

## 73. よくある不具合：Guardが解除されない

- Released Edgeを失った。
- GuardEndが低Priorityで消えた。
- Device切断時にHeldがtrueのまま。
- Pause/Focus切替で状態再同期していない。
- Guard Stateが現在Held=falseを確認していない。

Edgeだけに依存せずCurrent Heldを安全網にします。

## 74. よくある不具合：低FPSだけ挙動が違う

- Render FrameごとにState Tickを一回しか進めていない。
- 跨いだTimeline Eventを一つだけ発火した。
- 秒→Tick変換を切捨てた。
- Device Press/Releaseが同Frame内で相殺された。
- Variable DeltaでWindow境界を飛び越えた。

Fixed Tickと入力Event Queueを使います。

## 75. Performance

- 固定容量Bufferを使う。
- State/Command名をID化し毎Tick文字列生成しない。
- Validationの安い条件を先にする。
- Target Queryは上位Commandだけに行う。
- Debug Historyは固定Ring Buffer。
- Data RuleをLoad時にIndex化する。
- State Objectを毎遷移Allocationしない。

主役だけでなく多数Enemyが同じState Machineを使う負荷を測ります。

## 76. Unit Test：Buffer

- Push順とStable順。
- 満杯Policy。
- 同種Coalesce。
- 期限境界の直前・同Tick・直後。
- RemoveAt後の順序。
- Tick Wrap規約。
- Sequence重複拒否。

## 77. Unit Test：State

- Enter/Exitが一回ずつ呼ばれる。
- 一Tick多重遷移上限。
- Window両端の判定。
- Resource不足時に未消費。
- Commit成功時だけResourceとCommandが減る。
- Interrupt/Cancel/Natural EndのExit Reason。
- Timelineを跨いだ全Event発火。

## 78. Scenario Test

- Recovery直前・Window開始・終了・直後にAttackする。
- DodgeとAttackを同Tickに押す。
- Guard押下と解放を同Frameに行う。
- Charge Thresholdの前後で離す。
- Hit Stop中に次攻撃を押す。
- Pause中にButtonを押してResumeする。
- TargetがBuffer中に死亡する。
- Hit ReactionとDodgeが同Tickに要求される。
- 30/60/120fpsで同じ入力Scriptを再生する。

## 79. Fuzz Test

RandomなPressed/Released、Damage Event、Resource値を大量Tick入力し、次をAssertします。

- 無限遷移しない。
- State IDが有効。
- Resourceが負にならない。
- Buffer容量を超えない。
- 同じCommandを二度消費しない。
- Deadから攻撃へ遷移しない。
- NaN方向を生成しない。

失敗Seedを保存して再現します。

## 80. Combat Runtime

```cpp
class CombatRuntime final
{
public:
    void Tick(const CombatInputSnapshot& input,
              std::span<const CombatExternalEvent> externalEvents);

    [[nodiscard]] CombatStateId CurrentState() const noexcept;
    [[nodiscard]] const CombatDebugSnapshot& DebugSnapshot() const noexcept;

private:
    void InterpretInput(const CombatInputSnapshot& input);
    void ApplyExternalEvents(std::span<const CombatExternalEvent> events);
    void UpdateCurrentState();
    void EvaluateBufferedCommands();
    void CommitTransition();

    CombatCommandBuffer<16> commandBuffer_{};
    CombatStateRuntime state_{};
    std::uint64_t nextCommandSequence_{};
};
```

順序を一Functionへ集約し、各Characterが異なる順序で処理しないようにします。

## 81. 1 Tickの推奨順序

```text
1. Input Snapshot受取
2. Press/ReleaseからCommand生成
3. 外部Combat Event適用
4. Current State Timeline更新
5. 強制Transition候補生成
6. Buffered Commandを期限整理・検証
7. Transition Arbitration
8. 選択CommandとResourceをCommit
9. Exit -> Enter
10. Movement/Animation/Hitbox Requestを出力
11. Debug Snapshot/Replay Hash記録
```

Hit Eventを入力より先に適用するかはGame仕様ですが、必ず固定します。

## 82. Data Validation

- Windowのopen <= close。
- StateとCommand IDが存在する。
- Destination Stateが存在する。
- Costが負でない。
- Command寿命が最大値以下。
- 同一条件のRuleが曖昧でない。
- Dead等から禁止Stateへの経路がない。
- 到達不能Stateを警告する。

State GraphをBuild時に解析できます。

## 83. 実装チェックリスト

- [ ] Device入力をCombat Stateから直接読んでいない。
- [ ] TickごとにInput Snapshotを一度確定した。
- [ ] CommandはPointerでなく値と安定IDを持つ。
- [ ] Buffer容量とOverflow Policyを定義した。
- [ ] Command種類ごとに寿命を設定した。
- [ ] 同種連打を無制限に積まない。
- [ ] PriorityとTie-breakが決定的である。
- [ ] Peek→Validate→Choose→Commit→Removeの順で消費した。
- [ ] Held維持とEdge Commandを分けた。
- [ ] Startup/Active/RecoveryとCancel Windowを分けた。
- [ ] Enter/Exitの一時状態を確実に解除した。
- [ ] TransitionをDeferredし再入を防いだ。
- [ ] TargetとResourceを実行時に再検証した。
- [ ] Hit Stop/Pause/Scene遷移中のBuffer Policyがある。
- [ ] Timelineで跨いだEventを全て発火した。
- [ ] Reject理由、Latency、Buffer内容をDebug表示できる。
- [ ] Replay HashとFixed Tick Testがある。
- [ ] Buffer/State/Scenario/Fuzz Testを行った。

## 84. 練習課題

1. Pressed/Held/ReleasedからCommandを生成する。
2. 固定容量Ring Bufferを実装する。
3. Command期限と同種Coalesceを作る。
4. PriorityとStable Tie-breakを実装する。
5. Locomotion/Attack/Dodge/GuardのStateを作る。
6. Startup/Active/Recovery Timelineを作る。
7. Recovery後半のAttack受付Windowを作る。
8. DodgeとAttack同時入力のArbitrationをTestする。
9. ChargeのPress/Hold/Releaseを作る。
10. Hit Stop中の入力保持を作る。
11. Reject理由とCommand SequenceをOverlay表示する。
12. 固定入力ReplayとState Hash比較を作る。
13. Window境界のUnit Testを書く。
14. Random入力10万TickのFuzz Testを行う。

## 85. 理解確認

1. Logical ActionとCombat Commandの違いは何ですか。
2. 即時実行可能な入力もBuffer経路へ通す理由は何ですか。
3. CommandへEntity Pointerを保存すべきでない理由は何ですか。
4. Buffer寿命とCancel Windowの違いは何ですか。
5. Held GuardをCommandだけで表すべきでない理由は何ですか。
6. Commandを評価中に削除してはいけない理由は何ですか。
7. Animation時間とGameplay Tickを分ける理由は何ですか。
8. Hit Stop中の入力をSampleし続ける利点は何ですか。
9. Stable Tie-breakがReplayへ必要な理由は何ですか。
10. Reject理由を記録すると何を改善できますか。

## 86. この章の到達点

- Device非依存のAction SnapshotからCombat Commandを生成できる。
- 固定容量・期限・優先度付き入力Bufferを実装できる。
- CommandをTransactionとして一度だけ安全に消費できる。
- Combat State、Phase、Timeline、Transition Windowを設計できる。
- Tap/Hold/Release、Chord、Sequence、Chargeを扱える。
- Hit Stop、Pause、Damage Reaction、Target消失へ対応できる。
- Player、AI、Replay、Networkが同じCommand経路を利用できる。
- Debug Timeline、Telemetry、Unit/Scenario/Fuzz Testで操作感を検証できる。

## 87. 関連ノート

- [Keyboard・Mouse・Gamepad・Action Mapping](04_keyboard_mouse_gamepad.md)
- [Delta Time・Fixed Step・Frame制御](03_delta_time_fixed_step_frame_control.md)
- [MV1 Animation・Blend](16_mv1_animation_blend.md)
- [Character Controller](23_character_controller.md)
- [Action Camera・Target Lock](24_action_camera_target_lock.md)

次章では、このStateと入力Bufferを土台に、Combo Route、Cancel Rule、派生攻撃、入力履歴を実際の連撃へ統合します。
