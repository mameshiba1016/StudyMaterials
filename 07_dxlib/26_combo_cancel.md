# DXライブラリ：Combo・Cancel

この章では、攻撃を一つずつ再生する仕組みから、入力・命中状況・方向・Resource・時間Windowによって次の技へ派生するCombo Systemを作ります。ComboはAnimation Clipの順番ではなく、Action Nodeと条件付きTransitionで表現します。

## 1. Comboの構成

```text
Command Buffer
 -> current Attack Node
 -> transition candidates
 -> cancel/window/condition validation
 -> deterministic selection
 -> resource and state commit
 -> next Attack Node
```

入力受付、派生選択、Animation、Hitbox、移動を分離します。

## 2. Attack ID

```cpp
struct AttackId final
{
    std::uint32_t value{};
    auto operator<=>(const AttackId&) const = default;
};
```

Animation番号や配列Indexを外部IDにしません。Data上の安定IDからRuntime Indexへ解決します。

## 3. Attack Node

```cpp
struct AttackNode final
{
    AttackId id{};
    std::uint32_t animationId{};
    std::uint32_t totalTicks{};
    int staminaCost{};
    int energyGainOnHit{};
    float rootMotionScale{1.0f};
    bool requiresGrounded{};
    bool allowsTargetCorrection{};
};
```

Nodeは一つの攻撃Action、Edgeは次のActionへ移れる条件です。

## 4. Combo Edge

```cpp
enum class ComboCondition : std::uint32_t
{
    None          = 0,
    HitConfirmed  = 1u << 0,
    Whiffed       = 1u << 1,
    Grounded      = 1u << 2,
    Airborne      = 1u << 3,
    HasTarget     = 1u << 4,
    TargetLaunched= 1u << 5
};

struct ComboEdge final
{
    AttackId from{};
    AttackId to{};
    CombatCommandType command{};
    std::uint32_t openTick{};
    std::uint32_t closeTick{};
    std::uint32_t requiredConditions{};
    std::uint32_t forbiddenConditions{};
    int priority{};
    int additionalCost{};
};
```

## 5. Graphとして考える

```text
Light1 --Light--> Light2 --Light--> Light3
   |                 |
   +--Heavy--> Launcher
                     +--Light in air--> Air1
```

配列の次Indexだけでは分岐、戻り、空中派生、Character固有Routeを表しにくくなります。

## 6. Cancelとは

現在Actionの残りを省略し、別Actionへ移ることです。

- Chain Cancel：Combo内の次攻撃。
- Dodge Cancel：回避へ移る。
- Guard Cancel：Guardへ移る。
- Skill Cancel：Skillへ移る。
- Jump Cancel：空中Actionへ移る。
- Character Switch Cancel：交代へ移る。

可能だから常に無料とは限らず、Window、Hit Confirm、Resource、回数制限を持ちます。

## 7. Cancel Rule

```cpp
struct CancelRule final
{
    AttackId source{};
    CombatCommandType destinationCommand{};
    std::uint32_t openTick{};
    std::uint32_t closeTick{};
    bool requireHitConfirm{};
    bool allowOnBlock{};
    bool allowOnWhiff{};
    int resourceCost{};
    int priority{};
};
```

Combo Edgeと汎用Cancel Ruleを分けると、全攻撃共通のDodge Cancel等を表しやすくなります。

## 8. Window境界

```cpp
bool ContainsTick(std::uint32_t tick,
                  std::uint32_t begin,
                  std::uint32_t end)
{
    return tick >= begin && tick <= end;
}
```

両端を含むか、Animation速度変更時にTickがどう進むかを統一します。

## 9. Hit Confirm

```cpp
struct AttackRuntime final
{
    AttackId attackId{};
    std::uint32_t elapsedTicks{};
    std::uint32_t generation{};
    int uniqueTargetsHit{};
    bool hitConfirmed{};
    bool blocked{};
    bool parried{};
    bool hitboxActivated{};
};
```

Hit Confirmは「Hitboxを出した」ではなく、有効なTargetへHit Eventが確定した状態です。

## 10. Whiff

Active Windowが終わりHit ConfirmがなければWhiffと判定できます。攻撃開始直後からWhiff扱いにすると、命中前に空振り専用派生が選ばれます。

## 11. Block Confirm

命中、Guard、Parryを分けます。

```cpp
enum class ContactResult
{
    None,
    Hit,
    Blocked,
    Parried,
    Invulnerable,
    ArmorAbsorbed
};
```

Edgeごとに許可結果を定義します。

## 12. Command評価

```cpp
struct ComboEvaluationContext final
{
    const AttackRuntime& attack;
    bool grounded{};
    bool hasTarget{};
    bool targetLaunched{};
    int stamina{};
    int energy{};
};
```

Buffer内Commandを変更せず、候補Edgeを列挙してから一つを選びます。

## 13. Edge検証

```cpp
enum class ComboRejectReason
{
    None,
    WrongSource,
    WrongCommand,
    WindowClosed,
    MissingCondition,
    ForbiddenCondition,
    InsufficientResource,
    DestinationUnavailable,
    LoopLimit,
    AlreadyConsumed
};
```

falseだけでなく理由をDebug表示します。

## 14. Deterministic Selection

候補が複数なら次で決めます。

1. Rule Priority降順。
2. より具体的な条件数が多いEdge。
3. Commandの発行Tick昇順。
4. Command Sequence昇順。
5. Destination Attack ID昇順。

Data並び順へ偶然依存しません。

## 15. 派生入力方向

```cpp
enum class DirectionRequirement
{
    Any,
    Neutral,
    Forward,
    Backward,
    Left,
    Right,
    TowardTarget,
    AwayFromTarget
};
```

入力時のCamera Basisと実行時のTarget方向のどちらで判定するかをRuleへ持たせます。

## 16. Neutral判定

Stick Driftで方向派生が誤発動しないよう、Dead Zoneより大きい専用Thresholdを使います。Neutral Routeを優先するか方向Routeを優先するかも仕様化します。

## 17. Hold派生

Lightを押し続けると別技へ派生する場合、CommandのHeld時間または現在Held状態を検証します。BufferされたCommandの発行後に離されている可能性も考慮します。

## 18. Delay Combo

一定時間入力を待つと別派生になる仕組みです。

```text
early window: normal chain
delay window: delayed finisher
```

Window間の空白、Buffer寿命、Animationの待機LoopをDataとして揃えます。

## 19. Mashと正確入力

- Mash friendly：広いWindow、同種Command Coalesce。
- Timing based：狭いBonus Window、失敗しても通常派生。
- Strict：狭いWindow外は派生しない。

基本操作は寛容にし、高度なTimingは追加Rewardへ使うと学習しやすくなります。

## 20. Perfect Timing

```cpp
struct TimingWindow final
{
    std::uint32_t normalBegin{};
    std::uint32_t perfectBegin{};
    std::uint32_t perfectEnd{};
    std::uint32_t normalEnd{};
};
```

Perfect成功時だけDamage、Resource、Effectを強化し、入力自体を無効にしない設計も可能です。

## 21. Combo Counterとの違い

- Combo Route：攻撃者がどの技を連結したか。
- Combo Counter：Targetが一定時間内に何Hit受けたか。

別Systemです。複数Targetや複数Attackerで混同しないようにします。

## 22. Attack Generation

同じAttack IDを再度使っても別実行です。GenerationをHit Event、VFX Event、Command Sourceへ含め、前回の遅延Eventを現在攻撃へ適用しません。

## 23. Multi-hit

```cpp
struct HitRegistrationKey final
{
    std::uint32_t attackGeneration{};
    std::uint32_t hitboxId{};
    std::uint32_t targetId{};
};
```

一Hitboxが同Targetへ何回当たれるか、再Hit間隔をAttack Dataへ持たせます。

## 24. Hit Confirmの確定時点

Collision検出直後ではなく、Invulnerability、Team、Guard、Damage Ruleを解決後に確定します。無敵Targetへ触れただけでHit派生しないようにします。

## 25. Resource Flow

- Attack開始時にCostを支払う。
- Hit時にEnergyを得る。
- Cancel時に追加Costを払う。
- Interrupt時のRefund Policyを決める。
- Commit前に全Costを再検証する。

支払いと遷移をTransactionにします。

## 26. Commit Plan

```cpp
struct ComboTransitionPlan final
{
    AttackId source{};
    AttackId destination{};
    std::uint64_t commandSequence{};
    int staminaCost{};
    int energyCost{};
    bool perfectTiming{};
    StateExitReason exitReason{StateExitReason::Canceled};
};
```

Plan生成中はRuntimeを変更せず、Commit成功時だけCommand削除・Cost支払・State遷移を行います。

## 27. Animation Blend

派生ごとにBlend時間を持ちます。

```cpp
struct ComboVisualTransition final
{
    float blendSeconds{0.08f};
    bool preserveNormalizedTime{};
    bool preserveRootVelocity{};
};
```

短すぎるBlendはPoseが飛び、長すぎるBlendは攻撃Timingと見た目を曖昧にします。

## 28. Root Motion接続

前攻撃の残りRoot Motionを破棄し、次攻撃のRoot MotionをControllerへ渡します。Visual Rootを二重適用しません。Cancel瞬間のVelocity継承をRule化します。

## 29. Target Correction

Combo継続時のTarget補正は一撃目より弱くする、距離上限を設ける、壁越し補正を拒否する等の制約を持ちます。

```cpp
float ComputeCorrectionWeight(int comboDepth)
{
    return std::clamp(1.0f - comboDepth * 0.15f, 0.25f, 1.0f);
}
```

## 30. Target切替Combo

入力方向とScreen候補から次攻撃だけ別Targetへ向けられます。Current Lockを変更するか、一時Attack Targetだけ変更するかを分けます。

## 31. Air Combo

- GroundからLauncherへ派生。
- TargetのLaunch確定後だけJump Follow可能。
- Air Action回数を制限。
- Gravity ScaleをActionごとに調整。
- LandingでRouteを終了またはLanding Attackへ派生。

Air StateとAction Stateの権限を整理します。

## 32. Launcher

HitしたTargetがArmor等で浮かなかった場合、Air Followへ派生できるかを `TargetLaunched` 条件で判断します。予測ではなく確定Reaction Eventを使います。

## 33. Ground Bounce・Wall Bounce

Reaction側がBounce Eventを生成し、Attack側はEventを条件としてFollow-upを開きます。同一Targetへ回数制限を持たせ、無限拘束を防ぎます。

## 34. Loop防止

- 一Combo中の最大Node数。
- 同じEdgeの最大利用回数。
- Air Action回数。
- Bounce回数。
- Resource Cost。
- Combo Timeout。

GraphにCycleがあってもRuntime Budgetで終了を保証します。

## 35. Combo Runtime

```cpp
struct ComboRuntime final
{
    AttackId currentAttack{};
    std::uint32_t attackGeneration{};
    int comboDepth{};
    int airActionCount{};
    int groundBounceCount{};
    int wallBounceCount{};
    std::uint32_t lastTransitionTick{};
    std::vector<AttackId> routeHistory;
};
```

配布用Runtimeでは履歴を固定容量Ringにできます。

## 36. Combo終了

- Natural Recovery完了。
- Hit Reactionで中断。
- Dodge等でRoute外へCancel。
- Timeout。
- Landing/Death/Character Switch。
- Target消失で専用Ruleがない。

終了時にRoute固有Bonus、Counter、補正をResetします。

## 37. Combo Timeout

入力Buffer期限とは別です。攻撃間の最大空白を超えたら新Comboとして扱います。Hit Stop中にTimeoutを進めるかを決めます。

## 38. Style/Rank

同じ技の連打を減点し、多様なRoute、Perfect Timing、危険回避を加点できます。Damage計算とは分け、表示・演出用評価として設計します。

## 39. Repetition Penalty

```cpp
float RepetitionMultiplier(int recentUseCount)
{
    return std::max(0.25f, 1.0f - recentUseCount * 0.2f);
}
```

強制ではなく多様な操作を促す仕組みにします。

## 40. Data-driven Graph

```text
attacks.json
combo_edges.json
cancel_rules.json
```

文字列IDをLoad時に整数Indexへ解決し、Runtimeで文字列検索しません。

## 41. Data Validation

- 重複Attack ID。
- 存在しないFrom/To。
- open > close。
- WindowがSource総Tickを超える。
- 負のCost。
- 同条件・同Priorityの曖昧Edge。
- 到達不能Node。
- 終了不能Cycle。
- Ground専用からAir専用への条件不足。

Build時にErrorとして報告します。

## 42. Graph解析

DFS/BFSで開始Nodeから到達可能性を調べます。Strongly Connected ComponentsでCycleを検出し、Cycle内に退出条件やBudgetがあるか警告します。

## 43. Combo Editorを見据えた形式

Node位置等のEditor表示DataとGameplay Dataを分けます。見た目の配置変更でRuntime Hashが変わらないようにします。

## 44. Hot Reload

Data再読込は新Graphを別領域へ構築・検証し、成功時に交換します。実行中Attackは旧Versionを完了させるか安全Stateへ戻し、Indexが別Nodeを指さないようGenerationを使います。

## 45. Character差分

共通Attack Archetypeを継承しすぎると依存が複雑になります。CharacterごとにGraphを持ち、共通Cancel RuleやTagを参照するCompositionを優先します。

## 46. Weapon/Form差分

Weapon変更時にGraph全体を切り替える、Layerを追加する、一部EdgeをOverrideする方法があります。切替途中の現在Nodeをどう扱うか明示します。

## 47. Unlock条件

未習得AttackへのEdgeをRuntime Filterします。Save DataにはAttack IDを保存し、Data更新でIndexが変わっても壊れないようにします。

## 48. AIとCombo

AIはRouteを直接強制せずCommandを生成し、Playerと同じEdge検証を通します。高Level AIは目的に応じて到達NodeまでPath Searchできます。

```text
goal: launcher
current: Light1
path: Light1 -> HeavyBranch -> Launcher
```

## 49. Combo Planning

Edge Costに時間、Resource、Risk、距離条件を使います。実行中にTarget状態が変われば再計画します。AIが不可能な入力を予約し続けないようReject理由を返します。

## 50. ReplayとNetwork

Graph Version/Content HashをReplayへ保存します。同じCommand列でもDataが変われば結果が異なるためです。NetworkではAttack ID、Generation、Transition Tickを同期・検証します。

## 51. Rollback

Combo Runtime、Command Buffer、Resource、Target ID、Timeline CursorをSnapshot可能にします。VFX/AudioはEvent IDで重複再生を防ぎます。

## 52. Debug Overlay

```text
Attack: Light2 generation=81 elapsed=14/36
Contact: HitConfirmed target=42
Open edges:
  Light -> Light3 [12..24] valid
  Heavy -> Launcher [10..18] valid
Buffer: Heavy #918 expires=2022
Chosen: Launcher priority=60
Route: Light1 > Light2 > Launcher
```

## 53. Graph Debug Draw

現在Nodeを強調し、開いているEdgeを緑、条件不足を黄、Window外を灰、選択Edgeを青で表示します。

## 54. Telemetry

- Attack別利用回数・命中率。
- Edge別利用回数・Reject理由。
- 平均Combo Depth。
- Window入力Timing分布。
- Whiff Cancel率。
- 同技連打率。
- Resource不足による失敗率。
- 入力から派生開始までのLatency。

調整の根拠にします。

## 55. よくある不具合：違う技が出る

- Edge Priorityが曖昧。
- Directionを実行時Cameraで再解釈した。
- 同TickCommandの順序が非決定的。
- Hold CommandとPress Commandが両方残った。
- Dataの文字列ID解決が違うNodeを指した。

候補全てとScore/Reject理由を表示します。

## 56. よくある不具合：Comboが続かない

- Buffer期限がWindowより前に切れた。
- Hit Confirmの確定が遅い。
- Animation時間とGameplay Tickがずれた。
- Source Generation不一致。
- Resourceを前の入力時に二重消費した。
- DestinationのGround/Air条件が誤っている。

## 57. よくある不具合：無限Combo

- Cycleに回数制限がない。
- Bounce CountをAttackごとにResetした。
- Resource GainがCost以上で永久回復する。
- Hit Stun減衰がない。
- 同Target再Hit制限がない。
- Combo TimeoutをHit StopごとにResetした。

## 58. よくある不具合：見た目が飛ぶ

- Blend時間が0。
- Cancel時にPose Snapshotを使っていない。
- Root Motionを二重適用した。
- AnimationとGameplayの開始Tickが一Frame違う。
- Visual Target補正が瞬間移動した。

## 59. Unit Test

- Window両端。
- Hit/Block/Whiff条件。
- PriorityとTie-break。
- Resource Commit失敗時のRollback。
- Direction派生。
- Perfect Timing境界。
- Loop/Depth上限。
- Invalid Target fallback。
- Combo終了Reset。

## 60. Graph Validation Test

意図的に壊したDataを読み込み、重複ID、未知Destination、逆Window、曖昧Edge、退出不能Cycleを検出できるか確認します。

## 61. Scenario Test

- Light連打。
- Light→Heavy派生。
- Hit Confirm専用派生を空振りする。
- Block専用Cancel。
- Launcher→Air Combo→Landing。
- Window開始前・両端・終了後入力。
- DodgeとCombo派生を同Tick入力。
- Targetが派生直前に死亡。
- Hit Stop中に次入力。
- 30/60/120fps Replay比較。

## 62. Combo Service

```cpp
class ComboService final
{
public:
    std::optional<ComboTransitionPlan> Evaluate(
        const ComboEvaluationContext& context,
        std::span<const CombatCommand> commands) const;

    bool Commit(const ComboTransitionPlan& plan,
                CombatRuntime& combat,
                ComboRuntime& combo) const;

private:
    const ComboGraph& graph_;
};
```

Evaluateは副作用なし、CommitだけがRuntimeを変更します。

## 63. 1 Tickの統合順

```text
1. Hit/Block/Parry EventをAttack Runtimeへ反映
2. TimelineとWindowを進める
3. Command Bufferを整理
4. Combo Edgeと汎用Cancel候補を列挙
5. 強制Reaction候補とArbitration
6. 選択Planを再検証
7. Resource・Command・StateをCommit
8. Animation/Movement/Hitbox Requestを発行
9. Debug/Replay Hashを記録
```

## 64. 実装チェックリスト

- [ ] Attack NodeとAnimation Clipを別IDで管理した。
- [ ] Comboを条件付きGraphとして表現した。
- [ ] Combo Edgeと汎用Cancel Ruleを分けた。
- [ ] Hit/Block/Parry/Whiffを区別した。
- [ ] Window境界をTickで統一した。
- [ ] 候補選択が決定的である。
- [ ] 入力方向の解釈時点をRule化した。
- [ ] Resourceと遷移をTransactionでCommitした。
- [ ] Root MotionとTarget補正に上限がある。
- [ ] Air Action、Bounce、Loopに上限がある。
- [ ] GraphをLoad時に検証した。
- [ ] ReplayへGraph Hashを保存した。
- [ ] 候補・Reject理由・RouteをDebug表示できる。
- [ ] Window/Graph/Scenario Testを行った。

## 65. 練習課題

1. Light1→Light2→Light3のGraphを作る。
2. Light1からHeavy派生を追加する。
3. Hit Confirm専用Cancelを作る。
4. Whiff時だけDodge可能なRuleを作る。
5. 前入力とNeutral入力で派生を分ける。
6. Perfect Timing Windowを追加する。
7. Launcher→Air Attack→Landingを作る。
8. Combo最大DepthとBounce回数制限を作る。
9. Graph Validatorを作る。
10. Current NodeとEdge状態をDebug表示する。
11. 30/60/120fpsで同じReplay Hashを比較する。

## 66. 理解確認

1. Comboを配列の次Indexだけで表す弱点は何ですか。
2. Hit ConfirmとHitbox有効化の違いは何ですか。
3. Buffer期限とCancel Windowの違いは何ですか。
4. EvaluateとCommitを分ける理由は何ですか。
5. Directionを入力時・実行時のどちらで解釈するかが重要な理由は何ですか。
6. Generationが遅延Eventを防ぐ仕組みは何ですか。
7. Cycleを許すGraphにRuntime上限が必要な理由は何ですか。
8. Animation時間だけでWindowを管理しない理由は何ですか。
9. Graph HashをReplayへ保存する理由は何ですか。
10. Reject理由のTelemetryで何を改善できますか。

## 67. この章の到達点

- Attack Nodeと条件付きEdgeからCombo Graphを構築できる。
- Hit、Block、Whiff、方向、Resourceによる派生を作れる。
- Chain/Dodge/Guard/Skill等のCancel Ruleを設計できる。
- 候補を決定的に評価しTransactionとして遷移できる。
- Root Motion、Target補正、Air Combo、Bounceを安全に統合できる。
- Loop、無限拘束、二重Hit、Data不整合を防げる。
- AI、Replay、Networkで同じCombo Dataを利用できる。
- Debug Graph、Telemetry、Validation、Scenario Testで調整できる。

## 68. 関連ノート

- [Combat State・入力Buffer](25_combat_state_input_buffer.md)
- [MV1 Animation・Blend](16_mv1_animation_blend.md)
- [Character Controller](23_character_controller.md)
- [Action Camera・Target Lock](24_action_camera_target_lock.md)

次章ではDodge、Guard、Parryの時間Window、無敵、Guard方向、Just判定、失敗時のReactionを統合します。
