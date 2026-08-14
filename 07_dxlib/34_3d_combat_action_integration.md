# DXライブラリ：3D戦闘Action統合

この章では、これまで個別に学んだ入力、Camera、Character Controller、Animation、Collision、Combat、AI、Character交代、VFX、Audioを一つの高速3D戦闘Loopへ統合します。重要なのは機能数ではなく、処理順序、所有権、データ境界、再現性です。

## 1. 統合で起こりやすい問題

- AnimationとHit Boxが一Frameずれる。
- Camera基準の入力方向とCharacter基準の攻撃方向が混ざる。
- 同じ命中を複数Systemが確定する。
- Hit Stop中に解除Timerまで止まる。
- AIとPlayer入力が座標を直接書き換える。
- Character交代後も旧Characterへ入力が届く。
- 描画順やフレームレートで戦闘結果が変わる。

## 2. 全体の責務

```text
Platform / DX Library
 -> Input
 -> Fixed Simulation
    -> Commands
    -> State machines
    -> Animation pose
    -> Hit boxes and collision
    -> Damage commit
    -> Events
 -> Presentation
 -> Render
```

描画は戦闘状態を読みますが、戦闘結果を決めません。

## 3. Application Loop

```cpp
while (ProcessMessage() == 0)
{
    const double realDelta = timer.Tick();
    input.Poll();
    accumulator.Add(realDelta);

    while (accumulator.CanStep())
    {
        game.FixedUpdate(accumulator.FixedSeconds());
        accumulator.ConsumeStep();
    }

    const float alpha = accumulator.InterpolationAlpha();
    game.UpdatePresentation(static_cast<float>(realDelta), alpha);

    ClearDrawScreen();
    game.Draw(alpha);
    ScreenFlip();
}
```

一Frameが遅れても固定更新回数には上限を設け、永遠に追いつけない状態を防ぎます。

## 4. Fixed Step設定

```cpp
struct SimulationSettings final
{
    double fixedSeconds{1.0 / 60.0};
    int maximumStepsPerFrame{5};
    double maximumFrameDelta{0.25};
};
```

一時停止やDebugger復帰時の巨大DeltaをClampします。

## 5. Game Context

```cpp
struct FixedUpdateContext final
{
    std::uint64_t tick{};
    float deltaSeconds{};
    float worldTimeScale{1.0f};
};
```

Global変数から時刻を読む代わりに、更新Contextを明示的に渡します。

## 6. 推奨する固定更新順

```text
1. Apply queued create/destroy commands
2. Build immutable world snapshot
3. Sample buffered player commands
4. Update perception and AI decisions
5. Resolve party switching and director permissions
6. Update combat state machines
7. Advance gameplay animation
8. Extract root motion
9. Update character controllers
10. Update model pose and hit-box transforms
11. Gather collision candidates
12. Resolve defense and damage
13. Commit health, reaction, death
14. Publish gameplay events
15. Queue presentation requests
16. Apply deferred structural changes
```

順序はProject全体で一つに固定します。

## 7. SnapshotとRuntime

判断中に他EntityのRuntimeを直接変更しません。Tick開始Snapshotを読み、CommandまたはEventとして変更要求を出します。

```cpp
struct CombatantSnapshot final
{
    std::uint32_t entityIndex{};
    std::uint32_t entityGeneration{};
    VECTOR position{};
    VECTOR forward{VGet(0, 0, 1)};
    int hp{};
    bool dead{};
};
```

## 8. Commandの統一

```cpp
enum class CombatCommandType
{
    Move,
    LightAttack,
    HeavyAttack,
    Dodge,
    Guard,
    SwitchCharacter,
    Support
};

struct CombatCommand final
{
    CombatCommandType type{};
    VECTOR direction{};
    std::uint64_t issuedTick{};
    std::uint64_t expireTick{};
    std::uint32_t controlGeneration{};
};
```

PlayerとAIは同じ命令形式を使えます。

## 9. Command Source

```cpp
enum class CommandSource
{
    Player,
    EnemyAi,
    Script,
    Replay,
    Debug
};
```

競合時のPriorityを定義します。死亡やCinematic Lockは通常入力より優先されます。

## 10. Camera基準入力

```cpp
VECTOR BuildCameraRelativeMove(float inputX, float inputY,
                               VECTOR cameraForward,
                               VECTOR cameraRight)
{
    cameraForward.y = 0.0f;
    cameraRight.y = 0.0f;
    if (VDot(cameraForward, cameraForward) > 0.000001f)
        cameraForward = VNorm(cameraForward);
    if (VDot(cameraRight, cameraRight) > 0.000001f)
        cameraRight = VNorm(cameraRight);

    VECTOR move = VAdd(VScale(cameraRight, inputX),
                       VScale(cameraForward, inputY));
    const float lengthSq = VDot(move, move);
    if (lengthSq > 1.0f)
        move = VNorm(move);
    return move;
}
```

斜め入力で速度が増えないよう長さを制限します。

## 11. Camera更新の参照時点

入力方向にはFrame開始時のCamera基底を使います。Simulation後のCameraを使うと、同じFrame内で循環依存が起こります。

## 12. Lock-on方向

移動方向、Characterの向き、攻撃照準を分けます。Lock-on中は攻撃だけ対象方向へ補正し、回避は入力方向を優先するなどActionごとにPolicyを持ちます。

## 13. Action State

```cpp
enum class ActionState
{
    Locomotion,
    Attack,
    Dodge,
    Guard,
    Reaction,
    KnockDown,
    Switch,
    Dead
};

struct ActionRuntime final
{
    ActionState state{ActionState::Locomotion};
    std::uint16_t actionId{};
    std::uint32_t elapsedTicks{};
    std::uint32_t generation{};
};
```

## 14. State所有権

Action Stateが移動、回転、Animation、Hit Boxのどれを所有するかを明示します。二つのSystemが同じ値を上書きしません。

```cpp
struct ActionAuthority final
{
    bool ownsTranslation{};
    bool ownsRotation{};
    bool ownsAnimation{};
    bool ownsHitBoxes{};
};
```

## 15. Action Definition

```cpp
struct ActionDefinition final
{
    std::uint16_t id{};
    std::uint32_t totalTicks{};
    std::uint16_t animationId{};
    bool usesRootMotion{};
    bool canTurn{};
    float turnRate{};
    std::vector<std::uint16_t> hitBoxIds{};
};
```

Window、Cancel、Damage、VFX Markerも定義側へ持たせます。

## 16. Window通過

```cpp
bool CrossedTick(std::uint32_t previous,
                 std::uint32_t current,
                 std::uint32_t marker)
{
    return previous < marker && current >= marker;
}
```

更新が複数Tick進んでもMarkerを取りこぼしません。

## 17. AnimationとGameplay

Gameplay WindowをAnimationの再生秒数へ直接依存させず、Actionの固定Tickへ結び付けます。Animationはその状態を視覚化します。

## 18. Pose更新順

Action Tickを進め、Animation時刻を設定し、Model Poseを更新してからBone追従Hit Boxを更新します。前Frame Poseを使わないようにします。

## 19. Root Motion

```cpp
struct RootMotionDelta final
{
    VECTOR translation{};
    float yawRadians{};
};
```

Animationから得た差分をWorldへ直接加えず、Character Controllerへ希望移動として渡します。

## 20. Root Motionと衝突

壁で移動できなかった距離を次Frameへ無条件で持ち越しません。実移動量をAnimation補正へ返し、足滑りを視覚側で処理します。

## 21. Character Controller入力

```cpp
struct MovementRequest final
{
    VECTOR desiredVelocity{};
    VECTOR rootMotionDelta{};
    float desiredYaw{};
    bool forceUngrounded{};
};
```

Player、AI、Knockback、Root Motionを一箇所で合成します。

## 22. 移動Priority

死亡、Grab、Knockback、Root Motion、通常移動などのPriorityを明示します。単純加算で異常速度を作りません。

## 23. Collision Layer

```cpp
enum class CollisionLayer : std::uint32_t
{
    Stage       = 1u << 0,
    PlayerBody  = 1u << 1,
    EnemyBody   = 1u << 2,
    PlayerHit   = 1u << 3,
    EnemyHit    = 1u << 4,
    Projectile  = 1u << 5,
    Trigger     = 1u << 6
};
```

移動Collisionと攻撃Collisionを分けます。

## 24. Hit Box Snapshot

```cpp
struct HitBoxSnapshot final
{
    std::uint32_t ownerIndex{};
    std::uint32_t ownerGeneration{};
    std::uint32_t attackGeneration{};
    std::uint16_t hitBoxId{};
    VECTOR previousCenter{};
    VECTOR currentCenter{};
    float radius{};
    bool active{};
};
```

高速攻撃では前回位置から現在位置までSweepします。

## 25. Broad Phase

すべてのHit BoxとHurt Boxを総当たりせず、Grid、距離、Team、Layerで候補を絞ります。その後にSphere、Capsule、OBBなどの詳細判定を行います。

## 26. 候補の収集

Collision Callback内でHPを減らしません。命中候補を値として配列へ積み、安定Sort後にDefenseとDamageを解決します。

```cpp
struct HitCandidate final
{
    std::uint64_t sequence{};
    std::uint32_t attackerIndex{};
    std::uint32_t defenderIndex{};
    std::uint32_t attackGeneration{};
    VECTOR position{};
    VECTOR normal{};
};
```

## 27. 命中解決順

```text
Validate entity generations
 -> reject duplicate hit
 -> invulnerability
 -> parry
 -> guard / just guard
 -> armor
 -> damage and stagger
 -> reaction / death
 -> gameplay event
```

## 28. Commit Point

Damage計算はSnapshotから結果を返し、Commit段階でHPを一度だけ変更します。同一Tickの相打ち規則もCommit前に決めます。

## 29. Structural Change

Entity生成・削除、Character交代、Boss Phase変更を配列走査中に行いません。Command Bufferへ積み、更新境界で適用します。

## 30. Event Queue

```cpp
template<class T>
class EventQueue final
{
public:
    void Push(T event) { write_.push_back(std::move(event)); }

    void SwapBuffers()
    {
        read_.clear();
        read_.swap(write_);
    }

    const std::vector<T>& Read() const { return read_; }

private:
    std::vector<T> write_{};
    std::vector<T> read_{};
};
```

購読中のQueueへ追加してIteratorを壊しません。

## 31. Eventの種類

- DamageApplied
- ParrySucceeded
- CharacterSwitched
- PartBroken
- EntityDied
- ActionStarted / ActionEnded
- PresentationRequested

Gameplay EventとPresentation Eventを分けます。

## 32. Hit Stop統合

Hit Stopは次のTick用Time Domainへ反映します。命中を検出した途中で現在Tickの一部だけを止めないようにします。

## 33. Time Domain

World、個体、Camera、Particle、UIへ別Scaleを配布します。解除TimerはReal Timeまたは停止しない固定管理で進めます。

## 34. AI統合

AIはSnapshotを読み、Combat CommandとMovement Requestを返します。座標、HP、Action Stateを直接変更しません。

## 35. Combat Director統合

AIが技候補を作り、Directorが攻撃Tokenを許可し、Combat Stateが最終検証します。技開始成功時だけTokenをCommitします。

## 36. Character交代統合

交代TransactionはControl Generationを変更し、旧Commandを拒否します。Active Entityの削除・表示切替は固定更新境界で行います。

## 37. Boss統合

Phase移行は通常ActionのSafe Pointを待ち、Hit BoxとDirector TokenをCleanupしてから新Phaseを適用します。死亡が最優先です。

## 38. Camera統合

SimulationではTarget、注視点、衝突用のCamera Requestを作ります。実Camera PoseはPresentation更新で補間し、最後にShakeを加えます。

## 39. Render Interpolation

```cpp
VECTOR LerpVector(VECTOR previous, VECTOR current, float alpha)
{
    alpha = std::clamp(alpha, 0.0f, 1.0f);
    return VAdd(previous, VScale(VSub(current, previous), alpha));
}
```

描画補間値はHit判定へ戻しません。

## 40. Teleportと補間

Character交代、Respawn、大Warpでは補間をResetし、前位置から画面を横切ることを防ぎます。

## 41. 描画Pass

```text
Shadow
 -> Opaque world
 -> Opaque characters
 -> Transparent world effects
 -> Transparent hit effects
 -> Post process
 -> World-space UI
 -> Screen UI
 -> Debug draw
```

各Pass開始時に既知のBlend、Depth、Shader状態を設定します。

## 42. Presentation Budget

VFX、Damage Number、Sound、Camera Shakeに上限を設けます。上限超過時に省略するのは演出であり、Combat Eventではありません。

## 43. Resource準備

戦闘開始前に必要Model、Animation、Texture、Sound、Shaderを読み込みます。初回攻撃時の同期LoadでFrameが止まることを避けます。

## 44. Handle検証

DXライブラリのLoad関数が失敗したHandleをCacheへ登録しません。必須Assetの失敗はScene開始を止め、任意演出はFallbackを使います。

## 45. 所有権

- SceneがWorldとSystemを所有する。
- Resource CacheがAsset Handleを所有する。
- Entity StoreがRuntime Componentを所有する。
- Eventは値またはIDを保持する。
- Presentation InstanceはPoolが所有する。

寿命不明の生ポインタを遅延Queueへ保存しません。

## 46. Entity破棄

死亡確定直後にMemoryを解放せず、死亡EventとCleanupを処理した更新境界で破棄します。IndexとGenerationで古い参照を拒否します。

## 47. Determinism

固定Tick、安定Sort、SystemごとのRandom Stream、明示的な丸め、記録可能なCommandを使います。描画Frame数を乱数Seedにしません。

## 48. Replay Frame

```cpp
struct ReplayTick final
{
    std::uint64_t tick{};
    std::vector<CombatCommand> playerCommands{};
    std::uint64_t randomStateHash{};
    std::uint64_t worldStateHash{};
};
```

すべてのWorld Stateを保存しなくても、入力とHashでずれたTickを特定できます。

## 49. State Hash

Entity ID、位置の量子化値、HP、Action、Random Stateなど重要値を安定順でHash化します。Pointer値やコンテナ容量は含めません。

## 50. Debug Overlay

- Fixed Tick、Accumulator、実行Step数。
- Active Entity、Hit Box、候補、確定Hit数。
- Action Stateと経過Tick。
- Control・Phase・Attack Generation。
- AI判断時間、Collision時間、描画時間。
- Event Queue件数とPresentation Drop数。

## 51. Frame Capture Log

問題発生前後数百TickのCommand、State遷移、Hit、Damage、Token、Random結果をリングバッファへ保存します。

## 52. Assert

```cpp
#include <cassert>

void ValidateCombatant(int hp, int maximumHp,
                       VECTOR position)
{
    assert(maximumHp > 0);
    assert(hp >= 0 && hp <= maximumHp);
    assert(std::isfinite(position.x));
    assert(std::isfinite(position.y));
    assert(std::isfinite(position.z));
}
```

NaNと不変条件違反を発生Tickで止めます。

## 53. Profiler Scope

Input、AI、State、Animation、Movement、Collision、Damage、Presentation、Renderを別Scopeで測ります。平均だけでなく最大時間と候補数も記録します。

## 54. 負荷段階

1体、10体、50体と増やし、どのSystemが非線形に増えるか確認します。VFXを切った測定、AIを止めた測定も行い原因を分離します。

## 55. よくある失敗：順序を暗黙にする

System登録順に偶然依存すると追加時に壊れます。固定更新Pipelineをコードと資料の両方で明文化します。

## 56. よくある失敗：描画座標で判定する

補間済みRender TransformをCombatへ使うとFrame Rateで結果が変わります。Simulation Transformだけを判定へ使います。

## 57. よくある失敗：Event内の生ポインタ

受信時には破棄済みの可能性があります。Entity IDとGeneration、または必要値のSnapshotを保存します。

## 58. よくある失敗：即時削除

走査中のContainerから死亡Entityを消すとIteratorや参照が壊れます。Deferred Destroyを使います。

## 59. よくある失敗：全SystemがDelta Timeを選ぶ

各Systemが勝手にReal、Game、Fixed Deltaを選ぶと停止挙動が不一致になります。Contextから目的に合うTime Domainを受け取ります。

## 60. 統合テスト：基本戦闘

- 入力から一度だけ攻撃が開始する。
- AnimationとHit Box Windowが同じTickになる。
- 一つの攻撃が一対象へ規定回数だけ当たる。
- Guard、Parry、無敵の優先順を守る。
- Damage、Reaction、死亡が一度だけCommitされる。

## 61. 統合テスト：移動とCamera

- Camera方向を変えても入力方向が正しい。
- Lock-on中の移動と攻撃方向が仕様どおり異なる。
- Root Motionが壁を貫通しない。
- KnockbackがCharacter Controllerと衝突する。
- Camera補間値がHit判定へ影響しない。

## 62. 統合テスト：複合Event

- Hit Stop中にCharacter交代入力をBufferする。
- Boss部位破壊とPhase閾値が同Tickに来る。
- AI攻撃Token付与後に対象が死亡する。
- Support Projectile発射後にCharacterが退場する。
- Scene終了時にCamera、無敵、Time Scale Tokenを返す。

## 63. フレームレートテスト

30、60、120、144fps表示で同じ固定入力を再生し、HP、位置、Action、Hit数、Random State Hashが一致するか確認します。

## 64. 長時間テスト

Tick周回、Generation周回、Pool再利用、Event Queue容量、Resource漏れを調べるため自動戦闘を長時間実行します。

## 65. 実装順序

1. 固定更新Loopと明示的Pipelineを作る。
2. PlayerとAIを共通Commandへ変換する。
3. Action StateとCharacter Controllerを接続する。
4. Animation PoseからHit Boxを更新する。
5. 候補収集、Defense、Damage Commitを接続する。
6. Gameplay EventとPresentation Eventを分ける。
7. Director、Boss、交代をCommand境界へ接続する。
8. Render補間とCamera Presentationを加える。
9. Replay Hash、Profiler、統合テストを加える。

## 66. 完成確認表

- [ ] 固定更新順序が一箇所に明記されている。
- [ ] PlayerとAIが共通Commandを使う。
- [ ] 各Actionの移動・回転・Animation所有権が明確である。
- [ ] Pose更新後にHit Boxを同期する。
- [ ] Collision CallbackでHPを変更しない。
- [ ] Damageを一度だけCommitする。
- [ ] Structural Changeを更新境界で適用する。
- [ ] Render TransformをCombat判定へ使わない。
- [ ] 古いIDとGenerationを拒否する。
- [ ] 演出上限が戦闘結果を変えない。
- [ ] 複数表示fpsでSimulation結果が一致する。
- [ ] LogとHashでずれたTickを特定できる。

## 67. この章の要点

- 高速3D戦闘の安定性は機能数より更新順序と所有権で決まります。
- 入力とAIをCommandへ変換し、Runtimeを直接変更させません。
- Action Tick、Animation Pose、Hit Box、Damage Commitの順を固定します。
- Gameplay EventとPresentation Eventを分離します。
- Simulation TransformとRender補間を混ぜません。
- Entity、Control、Attack、PhaseのGenerationで古い処理を拒否します。
- 固定Tick、安定Sort、Random Stream、State Hashで再現性を作ります。
- Profiler、Debug Overlay、統合テストを最初からPipelineへ含めます。

次章では、DXライブラリ編全体のArchitecture、Test、完成確認表をまとめます。
