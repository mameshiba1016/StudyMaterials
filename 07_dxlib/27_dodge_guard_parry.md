# DXライブラリ：Dodge・Guard・Parry

この章では、敵の攻撃へ能動的に対応する回避・Guard・Parryを、入力、時間Window、方向、Resource、Damage解決、Animation、演出へ統合します。見た目だけ無敵にするのではなく、攻撃と防御のRuleを同じHit解決Pipelineで判定することが目的です。

## 1. 防御Actionの違い

- Dodge：位置を変え、特定Windowで攻撃を無効化する。
- Guard：方向とResource条件を満たす攻撃を継続的に軽減・無効化する。
- Just Guard：Guard開始直後の狭いWindowで強いRewardを得る。
- Parry：攻撃を弾き、攻撃側にもReactionを与える。
- Armor：Damageを受けても自分のActionを中断しない。

Invulnerability、Damage Reduction、Reaction Immunityを一つのFlagへまとめません。

## 2. 解決Pipeline

```text
Hit candidate
 -> attacker/defender validity
 -> duplicate hit prevention
 -> invulnerability
 -> parry
 -> guard / just guard
 -> armor
 -> damage and reaction
 -> events
```

判定順序を固定し、同じ攻撃がSystemごとに違う結果にならないようにします。

## 3. Defense Result

```cpp
enum class DefenseResult
{
    Miss,
    Invulnerable,
    Dodged,
    Parried,
    JustGuarded,
    Guarded,
    GuardBroken,
    ArmorAbsorbed,
    Hit
};
```

結果はDamage、Reaction、VFX、Audio、Camera、Resourceへ共有します。

## 4. Dodge Data

```cpp
struct DodgeDefinition final
{
    std::uint32_t totalTicks{};
    std::uint32_t invulnerableBeginTick{};
    std::uint32_t invulnerableEndTick{};
    std::uint32_t chainBeginTick{};
    std::uint32_t chainEndTick{};
    int staminaCost{};
    float distance{};
    float targetCorrectionLimit{};
    int maximumChainCount{2};
};
```

総時間、無敵、連続回避受付を別Windowとして持ちます。

## 5. Dodge Runtime

```cpp
struct DodgeRuntime final
{
    std::uint32_t generation{};
    std::uint32_t elapsedTicks{};
    VECTOR direction{};
    int chainCount{};
    bool crossedAttackThisDodge{};
};
```

方向を開始時に固定するか、後半だけ入力補正を許すかをData化します。

## 6. Dodge方向

```cpp
VECTOR ChooseDodgeDirection(VECTOR moveInput,
                            VECTOR characterForward,
                            float inputThreshold)
{
    if (VDot(moveInput, moveInput) >= inputThreshold * inputThreshold)
        return VNorm(moveInput);

    return VScale(VNorm(characterForward), -1.0f); // Neutral時は後方回避。
}
```

Free Camera、Target Lock、攻撃中で方向の基準を変えます。

## 7. 無敵Window

```cpp
bool IsDodgeInvulnerable(const DodgeRuntime& runtime,
                         const DodgeDefinition& definition)
{
    return runtime.elapsedTicks >= definition.invulnerableBeginTick &&
           runtime.elapsedTicks <= definition.invulnerableEndTick;
}
```

Animationの見た目とWindowが大きくずれると不公平に感じられます。Debug時にCharacterへ色を重ねて可視化します。

## 8. Invulnerabilityの種類

```cpp
enum class InvulnerabilityMask : std::uint32_t
{
    None        = 0,
    Melee       = 1u << 0,
    Projectile  = 1u << 1,
    Area        = 1u << 2,
    Grab        = 1u << 3,
    Environment = 1u << 4
};
```

通常DodgeでGrabやStage Hazardまで無効にするかはAttack Categoryごとに決めます。

## 9. Hurtbox無効化との違い

Hurtboxそのものを消すと、攻撃が通過した事実を検出できません。Hurtboxは残し、Defense解決でInvulnerableを返せばPerfect Dodge判定や演出に利用できます。

## 10. Perfect Dodge

無敵Window中に有効攻撃がHurtboxを横切った場合、Perfect Dodge Eventを一回発行します。

```cpp
if (isInvulnerable && !runtime.crossedAttackThisDodge)
{
    runtime.crossedAttackThisDodge = true;
    events.Push(PerfectDodgeEvent{defenderId, attackerId, attackId});
}
```

一つの多段攻撃で何度もRewardしないようGeneration単位で制限します。

## 11. Dodge移動とCollision

Root MotionやCurveから得た移動をCharacter Controllerへ渡し、壁で止めます。無敵だから壁を通過できるわけではありません。

- 開始時の方向を保存する。
- 一Tickの移動量をSweepする。
- 壁面へSlideするか停止する。
- 崖から落ちられるか設定する。
- Targetへの吸着量に上限を持つ。

## 12. 連続Dodge

連続回避は入力BufferとChain Windowを使います。ChainごとにStamina Cost増加、無敵短縮、移動距離減少を入れられます。

```cpp
float ChainDistanceMultiplier(int chainCount)
{
    return std::max(0.55f, 1.0f - chainCount * 0.15f);
}
```

## 13. Dodge Spam防止

- Stamina Cost。
- Chain回数上限。
- 無敵Window間のGap。
- Recovery。
- 同方向連打の距離低下。
- 攻撃側のTrackingやDelay変化。

操作を無効化しすぎず、攻撃へ戻る選択を有利にします。

## 14. Air Dodge

地上Dodgeと別定義にします。

- 空中利用回数。
- Gravity停止時間。
- Vertical方向を許すか。
- 着地Recovery。
- Air Combo回数との共有Budget。
- Wall Collision後の挙動。

Landing時に無敵や移動権限を必ず解除します。

## 15. Guard Data

```cpp
struct GuardDefinition final
{
    float maximumAngleDegrees{100.0f};
    float damageMultiplier{0.15f};
    float guardDamageMultiplier{1.0f};
    int minimumStaminaCost{1};
    std::uint32_t justGuardTicks{5};
    std::uint32_t guardRaiseTicks{3};
    bool blocksProjectiles{true};
};
```

Guard可能角度は正面Coneの全角か半角かを明記します。

## 16. Guard Runtime

```cpp
struct GuardRuntime final
{
    std::uint32_t beganTick{};
    bool held{};
    bool active{};
    float guardGauge{};
    std::uint32_t lastGuardedTick{};
};
```

Button Releasedを失っても、現在Held=falseならGuard終了できるようにします。

## 17. Guard方向判定

攻撃が来る方向をDefenderからAttackerまたはHit Originへの水平Vectorで求めます。

```cpp
bool IsInsideGuardArc(VECTOR defenderForward,
                      VECTOR defenderPosition,
                      VECTOR attackOrigin,
                      float halfAngleRadians)
{
    VECTOR toAttack = VSub(attackOrigin, defenderPosition);
    toAttack.y = 0.0f;
    defenderForward.y = 0.0f;

    if (VDot(toAttack, toAttack) < 0.000001f)
        return true; // 同位置攻撃のPolicyを明示する。

    const float alignment = VDot(VNorm(defenderForward), VNorm(toAttack));
    return alignment >= std::cos(halfAngleRadians);
}
```

Projectileは現在Attacker位置ではなく飛来方向を使う方が自然です。

## 18. 3D方向と高さ

上空・地下からの攻撃を水平角度だけでGuardできるかを決めます。必要なら垂直Angleも検証します。Area Attackは方向Guard不可、全方向Guard可能などCategoryで分けます。

## 19. Guard開始

Input BufferからGuardBeginを消費し、Guard Raiseを経てActiveになります。開始直後をJust Guardにする場合、Raise中でもJustだけ成立させるかを仕様化します。

## 20. Guard維持

```cpp
bool CanMaintainGuard(const GuardRuntime& guard,
                      int stamina,
                      bool stateAllowsGuard)
{
    return guard.held && stamina > 0 && stateAllowsGuard;
}
```

維持自体にStaminaを消費するか、Hit時だけ消費するかを選びます。

## 21. Guard Damage

```cpp
int ComputeGuardCost(float attackGuardDamage,
                     float defenderMultiplier,
                     int minimumCost)
{
    const float raw = attackGuardDamage * defenderMultiplier;
    return std::max(minimumCost, static_cast<int>(std::ceil(raw)));
}
```

整数丸め規則を統一します。

## 22. Chip Damage

Guard成功時も一部HP Damageを通す場合：

```cpp
int ComputeChipDamage(int originalDamage, float multiplier)
{
    return std::max(0,
        static_cast<int>(std::floor(originalDamage *
                                    std::clamp(multiplier, 0.0f, 1.0f))));
}
```

ChipでHPが0になるか最低1残るかをAttack/Ruleで決めます。

## 23. Guard Break

必要Guard Costが現在Staminaを超えた場合、Guard Breakへ遷移します。

- Staminaを0へする。
- Damageを受けるか軽減するか。
- 長いHit Stun。
- Knockback。
- 一定時間Stamina回復停止。
- 専用VFX/Audio/Camera Event。

Resource消費とReactionを一Transactionで確定します。

## 24. Guard Gauge回復

- Guard中は回復停止。
- Guard Hit後にDelay。
- 非Guard中に秒基準で回復。
- Break後は長いDelay。
- Character/EquipmentでRate変更。

表示値ではなくGameplay値を使います。

## 25. Just Guard

Guard開始から一定Tick以内にGuard成立した場合です。

```cpp
bool IsJustGuard(std::uint32_t currentTick,
                 const GuardRuntime& guard,
                 const GuardDefinition& definition)
{
    return currentTick - guard.beganTick <= definition.justGuardTicks;
}
```

早押しを許すPre-bufferと開始Tickの関係をTestします。

## 26. Just Guard Reward

- Guard Cost 0または軽減。
- Chip Damage 0。
- Attackerへ短いStagger。
- DefenderのResource回復。
- Cancel可能Windowを即時開く。
- Slow Motion、Flash、Sound。

Rewardを積みすぎると通常Guardが無意味になるため役割を分けます。

## 27. Parry Data

```cpp
struct ParryDefinition final
{
    std::uint32_t startupTicks{};
    std::uint32_t activeBeginTick{};
    std::uint32_t activeEndTick{};
    std::uint32_t recoveryTicks{};
    float maximumAngleDegrees{120.0f};
    int staminaCost{};
    int attackerStaggerLevel{};
};
```

Parryは持続GuardよりRiskが高く、失敗Recoveryを持つActionとして扱います。

## 28. Parry Window

```cpp
bool IsParryActive(std::uint32_t elapsedTick,
                   const ParryDefinition& definition)
{
    return elapsedTick >= definition.activeBeginTick &&
           elapsedTick <= definition.activeEndTick;
}
```

Animation Eventだけで開閉せず、Gameplay TickをAuthorityにします。

## 29. Parry可能Attack

```cpp
enum class AttackDefenseFlags : std::uint32_t
{
    None          = 0,
    Dodgeable     = 1u << 0,
    Guardable     = 1u << 1,
    Parryable     = 1u << 2,
    Unblockable   = 1u << 3,
    Grab          = 1u << 4,
    Projectile    = 1u << 5
};
```

色だけでなく形・音・AnimationでUnblockableを予告します。

## 30. Parry成立順

ParryとDodge無敵が同時の場合の優先を決めます。

- Parryを先に評価：意図したParry Rewardを得やすい。
- Invulnerabilityを先に評価：安全だがParryが出ない。

入力StateとAttack Categoryを含め、結果を一意にします。

## 31. Attacker Reaction

Parry成功時にAttackerへ `Parried` Eventを送り、Armor Levelと攻撃のParry ResistanceからStaggerを決めます。DefenderがAttacker Stateを直接変更しません。

## 32. Projectile Parry

選択肢：

- Projectileを消す。
- OwnershipとTeamを変更して反射する。
- 速度方向を反転する。
- Targetを元Attackerへ変更する。
- Damage倍率を変更する。

同じProjectileを同Tickに複数回反射しないようGenerationを更新します。

## 33. Parry Freeze

成功時の短いHit StopはAttacker、Defender、Projectile、Worldのどれを止めるかをEventで指定します。Input Samplingは継続し、反撃入力をBufferできます。

## 34. Counter Window

Parry/Just Guard成功後に専用Counter Commandを許可します。

```cpp
struct CounterOpportunity final
{
    std::uint32_t sourceAttackId{};
    std::uint32_t expiresTick{};
    std::uint32_t targetId{};
    std::uint32_t counterAttackId{};
};
```

Targetが無効になった場合のFallbackを持ちます。

## 35. Grab

Grabは通常Guard不可、Dodge可能など別Categoryにします。Grab成立時には相互位置、両者State、Camera、Animation同期が必要です。無敵MaskがGrabを含むか確認します。

## 36. Area Attack

Attack Originが中心と一致しない場合、Guard方向をExplosion中心・Wave進行方向・Attacker方向のどれで判定するかDataへ持たせます。

## 37. Multi-hit攻撃

- Dodge Generation中は全Hitを無効にするか各Hit判定か。
- Guard Costを毎Hit払うか上限を設けるか。
- Parryで攻撃全体を停止するか一Hitだけ弾くか。
- Just Guard Rewardを一Attack Generation一回にする。

Hit Registration Keyで重複を管理します。

## 38. Armor

ArmorはDamageを受けながらReactionを抑えます。

```cpp
struct ArmorState final
{
    int level{};
    float remainingPoise{};
    bool preventsKnockback{};
};
```

GuardやParryより後、通常Reactionより前に評価します。

## 39. Super ArmorとInvulnerability

- Invulnerability：Damage 0、Reactionなし。
- Super Armor：Damageあり、Reaction軽減/なし。
- Damage Reduction：Damage減少、Reactionはあり得る。

UI、VFX、Hit Soundも結果に合わせます。

## 40. Defense Transaction

```cpp
struct DefenseResolution final
{
    DefenseResult result{DefenseResult::Hit};
    int healthDamage{};
    int staminaDamage{};
    int poiseDamage{};
    std::optional<CombatStateId> defenderReaction;
    std::optional<CombatStateId> attackerReaction;
    bool consumeHit{};
};
```

Resolve中はRuntimeを変えず、確定結果をCommitします。

## 41. ResolveとCommit

```text
Resolve (pure): 状態Snapshot + Attack -> DefenseResolution
Commit        : HP/Stamina/State/Eventを一度だけ変更
```

VFX再生後にCommit失敗するような順序を避けます。

## 42. 複数Defender

一つの攻撃が複数Entityへ当たる場合、DefenderごとにDefenseを解決します。誰かにParryされたら攻撃全体を止めるか、他TargetへのHitを維持するかをAttack Dataで決めます。

## 43. Simultaneous Hit

同Tickに相互攻撃、Parry、死亡が起きる場合、Hit Eventを収集してから安定順で解決します。

```text
sort by simulation tick
 -> attack priority
 -> attacker stable ID
 -> defender stable ID
 -> hitbox ID
```

途中で死亡したEntityの残りHitを有効にするかを仕様化します。

## 44. Resource不足

- Dodge開始時にStaminaを支払う。
- Guard Hit時にCostを支払う。
- Parry開始時または成功時に支払う。

開始時Costなら空振りにもRiskがあり、成功時Costなら連打しやすくなります。

## 45. Input Bufferとの統合

Dodge/Guard/Parry Commandは第25章のBufferを通します。

- Dodgeは短い期限と高Priority。
- GuardBeginは短い期限、Held状態で維持。
- GuardEndは必須解除Event。
- ParryはPress Edgeだけ。
- Hit Stop中も入力を保存。

## 46. Cancelとの統合

攻撃NodeごとにDodge/Guard/Parry Cancel Ruleを定義します。Defense側が勝手に現在Attackを中断せず、第26章のTransition Arbitrationを通します。

## 47. Reactionとの競合

同TickにDodge CommandとHit Eventがある場合、入力Sample、Dodge State Enter、Hit解決の順序で結果が変わります。Simulation Pipelineを固定し、Dodge無敵はState Enter後から有効等のRuleを明記します。

## 48. Animationとの同期

- Gameplay TickがWindow Authority。
- AnimationはWindowを視覚的に示す。
- Playback Speed変更時もRuleを維持。
- Blend中のPoseでもDefense Stateは一意。
- Guard盾や武器の見た目はFrame位置へ追従。

## 49. Root Motion

Dodge/Parry StepのRoot MotionをCharacter Controllerへ渡します。壁で停止しても無敵時間を維持するか短縮するかを決めます。距離不足をTeleportで補いません。

## 50. Camera・VFX・Audio

Defense ResultごとにEventを発行します。

```text
Dodged      -> subtle whoosh / optional slow
Guarded     -> block spark / low sound / small shake
JustGuarded -> sharp flash / high sound / short freeze
Parried     -> directional spark / strong freeze / counter cue
GuardBroken -> large break effect / long camera response
```

Gameplay判定と演出を直接結合しません。

## 51. Accessibility

- Parry予告の色以外の形・音。
- Flash強度調整。
- Camera Shake強度調整。
- Input Buffer幅のAssist設定。
- Hold GuardをToggleに変更。
- Perfect Dodge Slow Motion強度。

判定Ruleを隠れて変更するAssistは明示表示します。

## 52. Debug Overlay

```text
Defense state: Dodging elapsed=4/22
I-frame: [3..10] ACTIVE
Incoming: attack=HeavySlash flags=Guardable|Parryable
Direction dot=0.72 guard limit=0.50
Result: Dodged
Stamina: 64 -> 44
```

## 53. Debug Draw

- Guard Cone。
- Attack incoming方向。
- HurtboxとHitbox。
- Dodge軌道とSweep。
- 無敵中のCharacter色。
- Parry Range/Arc。
- Projectile反射方向。

## 54. Telemetry

- Dodge回数、Perfect Dodge率。
- 無敵Window内のHit時刻分布。
- Guard率、Guard Break率。
- Just Guard/Parry成功率。
- 攻撃Category別失敗率。
- Defense入力からState開始までのLatency。
- Stamina不足回数。
- 同じAttackによる多重Reward抑止回数。

## 55. よくある不具合：回避したのにHitする

- Visualと無敵Windowがずれている。
- Dodge Enterより先にHitを解決した。
- Attack CategoryがMask外。
- BufferされたDodgeがまだ未消費。
- Hurtboxを前Frame Transformで判定した。
- Hit Eventを生成済みでState変更後に再検証していない。

## 56. よくある不具合：背後もGuardする

- Attacker Forwardを使い、Defenderからの方向を使っていない。
- Projectileの現在Attacker位置を使った。
- Dot Productの符号・半角を間違えた。
- Y成分で正規化が歪んだ。
- 同位置Fallbackを常にtrueにした。

## 57. よくある不具合：Parryが二回出る

- Multi-hitのAttack Generationを記録していない。
- ResolveとCommitの両方でEvent発行した。
- Rollback再生EventをDeduplicateしていない。
- Projectile反射後も元Hitを残した。

## 58. よくある不具合：Guardが永久に続く

- Released Edgeを失った。
- Device切断時のHeldをResetしていない。
- Pause解除時にStateを再同期していない。
- Stamina 0でも維持Conditionがtrue。
- Guard End TransitionがHit Reactionに負け続けた。

## 59. Unit Test

- 無敵Windowの直前・両端・直後。
- Guard Cone境界。
- Front/Back/同位置攻撃。
- Hit/Block/Just/Parryの判定順。
- ResourceがCost未満・同値・超過。
- Guard Break Transaction。
- Multi-hit一回Reward。
- Projectile反射Ownership。
- Simultaneous Hitの安定順。

## 60. Scenario Test

- 地上/空中Dodge。
- 壁際Dodge。
- Hit Stop中のDodge入力。
- 前後同時攻撃をGuard。
- ProjectileをGuard/Parry。
- UnblockableとGrab。
- Multi-hitをGuard。
- Guard Gaugeぎりぎりで強攻撃。
- Just GuardからCounter。
- 30/60/120fps Replay比較。

## 61. Fuzz Test

RandomなAttack Category、方向、Damage、Defense Stateを入力し、次を検証します。

- HP/Staminaが規則外で負にならない。
- 一Hitを二度Commitしない。
- Parry不可AttackがParriedにならない。
- Guard範囲外がGuardedにならない。
- Dead StateからDodgeへ遷移しない。
- NaN方向でCrashしない。

## 62. Defense Resolver

```cpp
class DefenseResolver final
{
public:
    DefenseResolution Resolve(
        const IncomingAttack& attack,
        const DefenderSnapshot& defender,
        std::uint32_t simulationTick) const;

    bool Commit(const DefenseResolution& resolution,
                CombatRuntime& attacker,
                CombatRuntime& defender,
                CombatEventQueue& events) const;
};
```

Resolveを副作用なしにすると境界Testが容易です。

## 63. 1 Tickの統合順

```text
1. Input SnapshotとDefense Command生成
2. State Transition候補評価
3. Dodge/Guard/Parry State確定
4. MovementとHitbox Transform確定
5. Hit候補収集
6. Defense Resolve
7. 安定順でCommit
8. Damage/Reaction/Resource更新
9. VFX/Audio/Camera Event発行
10. Debug/Replay Hash記録
```

## 64. 実装チェックリスト

- [ ] Invulnerability、Guard、Armorを分離した。
- [ ] Defense解決順序を固定した。
- [ ] Dodgeの総時間・無敵・Chain Windowを分けた。
- [ ] 無敵でもHurtbox接触を検出できる。
- [ ] Dodge移動をCharacter Collisionへ通した。
- [ ] Guard方向を攻撃の飛来方向から求めた。
- [ ] Guard Cost、Chip、BreakをTransactionで処理した。
- [ ] Just GuardとParryのWindowをTick管理した。
- [ ] AttackごとにDodge/Guard/Parry可否がある。
- [ ] Multi-hit RewardをGenerationで制限した。
- [ ] Projectile反射のOwnershipを更新した。
- [ ] Defense CommandをBuffer/Cancel Systemへ統合した。
- [ ] Gameplay結果から演出Eventを生成した。
- [ ] Accessibility設定を用意した。
- [ ] Debug Cone、Window、結果、Resourceを表示できる。
- [ ] Unit/Scenario/Fuzz Testを行った。

## 65. 練習課題

1. Tick指定のDodge無敵Windowを作る。
2. Perfect Dodgeを一Generation一回発行する。
3. 前方100度のGuard Coneを作る。
4. Guard CostとGuard Breakを実装する。
5. Just Guard Windowを追加する。
6. Startup/Active/Recovery付きParryを作る。
7. Parry不可・Guard不可AttackをData化する。
8. Projectile反射を実装する。
9. Counter Opportunityを入力Bufferへ接続する。
10. Defense Resolve/Commitを分離する。
11. Window境界と方向境界のUnit Testを書く。
12. Debug TimelineとConeを表示する。

## 66. 理解確認

1. InvulnerabilityとHurtbox削除の違いは何ですか。
2. Dodgeの見た目と無敵Windowを近づける理由は何ですか。
3. Guard方向にProjectileの飛来方向を使う理由は何ですか。
4. Just GuardとParryの役割の違いは何ですか。
5. Guard BreakをTransactionで処理する理由は何ですか。
6. Multi-hitへGenerationが必要な理由は何ですか。
7. ArmorとInvulnerabilityの違いは何ですか。
8. ResolveとCommitを分ける利点は何ですか。
9. 同Tick入力とHitの処理順が重要な理由は何ですか。
10. 防御演出をEventへ分離する理由は何ですか。

## 67. この章の到達点

- Dodge、Guard、Just Guard、Parry、Armorを別Ruleとして設計できる。
- 無敵・受付・Chain WindowをTick単位で管理できる。
- 方向、Attack Category、ResourceからGuard結果を解決できる。
- Guard Break、Counter、Projectile反射を実装できる。
- Multi-hit、同時Hit、Hit Stop、Cancelとの競合を一意に処理できる。
- Resolve/CommitとEventでGameplayと演出を分離できる。
- Debug表示、Telemetry、Unit/Scenario/Fuzz Testで防御判定を検証できる。

## 68. 関連ノート

- [Combat State・入力Buffer](25_combat_state_input_buffer.md)
- [Combo・Cancel](26_combo_cancel.md)
- [Character Controller](23_character_controller.md)
- [Action Camera・Target Lock](24_action_camera_target_lock.md)

次章ではHit候補、Damage式、Reaction、Knockback、Armor、Deathまでを攻撃側・防御側の共通Pipelineへ統合します。
