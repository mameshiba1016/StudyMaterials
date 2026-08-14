# DXライブラリ：Hit・Damage・Reaction

この章では、攻撃判定が防御判定を通過した後の「命中結果の確定」を扱います。HPを減らすだけでなく、属性、部位、会心、姿勢、怯み、吹き飛ばし、ダウン、死亡、演出通知を一貫した順序で処理します。

## 1. 命中処理の責務

- 同じ攻撃の意図しない多重命中を防ぐ。
- 攻撃力、防御力、倍率、属性から最終ダメージを求める。
- HP、姿勢値、部位耐久値を一度だけ更新する。
- 怯み、打ち上げ、吹き飛ばし、ダウン、死亡を選ぶ。
- 命中位置、法線、方向、結果を演出側へ通知する。

計算と演出を混ぜず、「候補」「計算」「確定」「通知」へ分けます。

## 2. 推奨パイプライン

```text
Collision candidate
 -> validity and duplicate-hit rejection
 -> defense resolution
 -> hit-zone resolution
 -> damage calculation
 -> resource commit
 -> reaction and death resolution
 -> gameplay event
 -> VFX / audio / UI / camera
```

順序を固定すると、同じ入力と状態から同じ結果を再現しやすくなります。

## 3. 攻撃実行ID

```cpp
#include <cstdint>

struct AttackInstanceId final
{
    std::uint32_t generation{}; // 技を開始するたび増える世代番号。
    std::uint16_t hitBoxIndex{}; // 一つの技に含まれる攻撃判定番号。
    std::uint16_t pulseIndex{};  // マルチヒットの何発目か。

    bool operator==(const AttackInstanceId&) const = default;
};
```

技名だけでは連続して同じ技を使ったときに以前の履歴と衝突します。実行単位の世代を含めます。

## 4. ダメージ種別

```cpp
enum class DamageType
{
    Physical,
    Fire,
    Ice,
    Electric,
    Ether,
    TrueDamage // 防御計算を無視する特殊用途。乱用しません。
};
```

## 5. リアクション種別

```cpp
enum class ReactionType
{
    None,
    FlinchSmall,
    FlinchLarge,
    Stagger,
    Launch,
    KnockBack,
    KnockDown,
    WallSplat,
    GetUp,
    Death
};
```

戦闘計算にはアニメーション名でなく意味を保存し、キャラクター側が実際のモーションを選びます。

## 6. 攻撃定義

```cpp
struct AttackDefinition final
{
    int baseDamage{10};
    float attackScale{1.0f};
    float partBreakPower{};
    float staggerPower{10.0f};
    float knockBackSpeed{};
    float launchSpeed{};
    float criticalRate{};
    float criticalMultiplier{1.5f};
    DamageType damageType{DamageType::Physical};
    ReactionType requestedReaction{ReactionType::FlinchSmall};
    bool canKill{true};
    bool ignoresArmor{};
};
```

調整値を処理へ直書きせず、技ごとの定義データへまとめます。

## 7. 攻撃側スナップショット

```cpp
struct AttackerSnapshot final
{
    std::uint32_t entityId{};
    int attackPower{};
    float damageDealtMultiplier{1.0f};
    float criticalRateBonus{};
    float criticalDamageBonus{};
};
```

計算途中で装備やバフが変わっても値が揺れないよう、開始時点の値を保存します。

## 8. 防御側スナップショット

```cpp
struct DefenderSnapshot final
{
    std::uint32_t entityId{};
    int currentHp{};
    int maximumHp{};
    int defense{};
    float damageTakenMultiplier{1.0f};
    bool superArmor{};
    bool invincible{};
    bool dead{};
};
```

読み取りデータと書き換える実体を分離すると、計算関数を副作用なしでテストできます。

## 9. 部位データ

```cpp
struct HitZone final
{
    std::uint32_t ownerEntityId{};
    std::uint16_t zoneId{};
    float damageMultiplier{1.0f};
    float staggerMultiplier{1.0f};
    bool weakPoint{};
    bool breakable{};
};
```

コライダー自身へ本体HPを持たせず、所有者IDから戦闘本体へ到達させます。

## 10. 命中の幾何情報

```cpp
#include <DxLib.h>

struct HitGeometry final
{
    VECTOR worldPosition{};  // エフェクトを出す世界座標。
    VECTOR worldNormal{};    // 接触面の外向き法線。
    VECTOR attackDirection{};
    float penetrationDepth{};
};
```

## 11. ダメージ結果

```cpp
struct DamageResult final
{
    int rawDamage{};
    int mitigatedDamage{};
    int finalDamage{};
    float staggerDamage{};
    bool critical{};
    bool weakPoint{};
    bool killed{};
    ReactionType reaction{ReactionType::None};
};
```

途中値を残すと、最終値になった理由をデバッグ表示できます。

## 12. 数値の安全な丸め

```cpp
#include <algorithm>
#include <cmath>

int RoundNonNegativeDamage(float value)
{
    // NaNや負数がHPを回復させる事故を入口で防ぎます。
    if (!std::isfinite(value) || value <= 0.0f)
        return 0;

    return static_cast<int>(std::lround(value));
}
```

キャストは小数部を切り捨てます。丸め規則を仕様として統一します。

## 13. 防御力による軽減

```cpp
float DefenseMultiplier(int defense, float defenseConstant)
{
    const float d = static_cast<float>(std::max(defense, 0));
    const float k = std::max(defenseConstant, 1.0f);
    return k / (k + d);
}
```

単純な`攻撃力 - 防御力`は低威力攻撃がすべて0になりやすいため、狙う遊びに合う式をグラフでも確認します。

## 14. 会心判定を注入する

```cpp
struct Random01
{
    virtual ~Random01() = default;
    virtual float Next() = 0;
};

bool RollCritical(float rate, Random01& random)
{
    return random.Next() < std::clamp(rate, 0.0f, 1.0f);
}
```

乱数器を外から渡すと、テストで固定値を返して結果を再現できます。

## 15. 最終ダメージ計算

```cpp
DamageResult CalculateDamage(const AttackDefinition& attack,
                             const AttackerSnapshot& attacker,
                             const DefenderSnapshot& defender,
                             const HitZone& zone,
                             Random01& random)
{
    DamageResult result{};
    if (defender.dead || defender.invincible)
        return result;

    const float attackValue = static_cast<float>(attack.baseDamage) +
        static_cast<float>(attacker.attackPower) * attack.attackScale;
    result.rawDamage = RoundNonNegativeDamage(attackValue);

    const float defenseScale = attack.damageType == DamageType::TrueDamage
        ? 1.0f : DefenseMultiplier(defender.defense, 100.0f);
    result.mitigatedDamage = RoundNonNegativeDamage(attackValue * defenseScale);

    result.critical = RollCritical(
        attack.criticalRate + attacker.criticalRateBonus, random);
    result.weakPoint = zone.weakPoint;

    const float criticalScale = result.critical
        ? std::max(1.0f, attack.criticalMultiplier +
                         attacker.criticalDamageBonus)
        : 1.0f;

    const float finalValue = static_cast<float>(result.mitigatedDamage) *
        zone.damageMultiplier * attacker.damageDealtMultiplier *
        defender.damageTakenMultiplier * criticalScale;

    result.finalDamage = RoundNonNegativeDamage(finalValue);
    result.staggerDamage = std::max(
        0.0f, attack.staggerPower * zone.staggerMultiplier);
    result.killed = attack.canKill && result.finalDamage >= defender.currentHp;
    return result;
}
```

この関数はHPを書き換えず、計算結果だけを返します。

## 16. 最低保証ダメージ

```cpp
int ApplyMinimumDamage(int damage, bool normalHitSucceeded)
{
    if (!normalHitSucceeded)
        return 0;
    return std::max(damage, 1);
}
```

無敵や完全ガードへ無条件に最低1を適用してはいけません。

## 17. HPを安全に更新する

```cpp
struct Health final
{
    int current{};
    int maximum{1};

    int ApplyDamage(int amount)
    {
        const int before = current;
        current = std::clamp(current - std::max(amount, 0), 0, maximum);
        return before - current; // 要求値でなく実際に減った値。
    }

    bool IsDead() const { return current <= 0; }
};
```

残りHP3へ100を要求しても実減少量は3です。この二つを区別します。

## 18. 姿勢値と怯み

```cpp
struct PoiseState final
{
    float accumulated{};
    float threshold{100.0f};

    bool Add(float amount)
    {
        accumulated += std::max(amount, 0.0f);
        if (accumulated < threshold)
            return false;
        accumulated = 0.0f;
        return true;
    }
};
```

一発ごとに必ず怯ませず、蓄積が境界を超えたときだけ大きなリアクションを許可できます。

## 19. スーパーアーマー

スーパーアーマーはダメージ無効ではなく、一般には一定以下のリアクションだけを抑止します。

```cpp
bool SuppressesReaction(bool armor, bool ignoresArmor,
                        ReactionType requested)
{
    if (!armor || ignoresArmor)
        return false;
    return requested == ReactionType::FlinchSmall ||
           requested == ReactionType::FlinchLarge;
}
```

## 20. リアクション選択

```cpp
ReactionType SelectReaction(const AttackDefinition& attack,
                            const DefenderSnapshot& defender,
                            bool poiseBroken,
                            bool killed)
{
    if (killed)
        return ReactionType::Death;
    if (SuppressesReaction(defender.superArmor,
                           attack.ignoresArmor,
                           attack.requestedReaction))
        return ReactionType::None;
    if (poiseBroken)
        return ReactionType::Stagger;
    return attack.requestedReaction;
}
```

死亡を最優先にしないと、HP0なのに通常怯みへ遷移します。

## 21. 安全なノックバック方向

```cpp
VECTOR SafeHorizontalDirection(VECTOR direction, VECTOR fallback)
{
    direction.y = 0.0f;
    fallback.y = 0.0f;
    if (VDot(direction, direction) > 0.000001f)
        return VNorm(direction);
    if (VDot(fallback, fallback) > 0.000001f)
        return VNorm(fallback);
    return VGet(0.0f, 0.0f, 1.0f);
}
```

攻撃者と対象が同座標でも、ゼロベクトルの正規化によるNaNを作りません。

## 22. リアクション速度

```cpp
VECTOR BuildReactionVelocity(VECTOR direction,
                             float knockBackSpeed,
                             float launchSpeed)
{
    const VECTOR horizontal = VScale(
        SafeHorizontalDirection(direction, VGet(0, 0, 1)),
        std::max(knockBackSpeed, 0.0f));
    return VGet(horizontal.x, std::max(launchSpeed, 0.0f), horizontal.z);
}
```

位置を瞬間的に動かさず、速度としてCharacter Controllerへ渡して重力や壁衝突と統合します。

## 23. 命中イベント

```cpp
struct DamageAppliedEvent final
{
    std::uint32_t attackerId{};
    std::uint32_t defenderId{};
    AttackInstanceId attackId{};
    DamageResult result{};
    HitGeometry geometry{};
};
```

UI、音、VFX、カメラは戦闘本体を直接操作せず、確定イベントを購読します。

## 24. 重複命中を防ぐ

```cpp
#include <vector>

struct RegisteredHit final
{
    AttackInstanceId attackId{};
    std::uint32_t targetId{};
};

class HitRegistry final
{
public:
    bool TryRegister(AttackInstanceId id, std::uint32_t targetId)
    {
        for (const RegisteredHit& hit : hits_)
            if (hit.attackId == id && hit.targetId == targetId)
                return false;
        hits_.push_back({id, targetId});
        return true;
    }

    void Clear() { hits_.clear(); }

private:
    std::vector<RegisteredHit> hits_{};
};
```

小規模なら線形探索で十分です。大量の判定では計測後にハッシュ化します。

## 25. マルチヒット間隔

```cpp
struct MultiHitRule final
{
    std::uint32_t intervalTicks{1};
    std::uint16_t maximumHitsPerTarget{1};
};

struct PerTargetHitState final
{
    std::uint32_t targetId{};
    std::uint32_t lastHitTick{};
    std::uint16_t hitCount{};
};

bool CanHitAgain(const PerTargetHitState& state,
                 const MultiHitRule& rule,
                 std::uint32_t currentTick)
{
    if (state.hitCount >= rule.maximumHitsPerTarget)
        return false;
    return currentTick - state.lastHitTick >= rule.intervalTicks;
}
```

秒でなく固定更新Tickにすると、フレームレート差による命中回数の変化を抑えられます。

## 26. 二段階確定

```cpp
struct PendingDamage final
{
    std::uint32_t sequence{};
    std::uint32_t attackerId{};
    std::uint32_t defenderId{};
    DamageResult result{};
};
```

衝突Callbackで即座にHPを減らさず、候補を収集して決まった段階で一度だけ確定します。

## 27. 状態へCommitする

```cpp
struct CombatRuntime final
{
    Health health{};
    PoiseState poise{};
    bool dead{};
};

void CommitDamage(const AttackDefinition& attack,
                  const DefenderSnapshot& snapshot,
                  DamageResult& result,
                  CombatRuntime& defender)
{
    if (defender.dead)
        return;

    result.finalDamage = defender.health.ApplyDamage(result.finalDamage);
    const bool poiseBroken = defender.poise.Add(result.staggerDamage);
    result.killed = defender.health.IsDead() && attack.canKill;
    defender.dead = defender.dead || result.killed;
    result.reaction = SelectReaction(
        attack, snapshot, poiseBroken, result.killed);
}
```

候補収集後に別攻撃で死亡している可能性があるため、Commit直前にも対象を確認します。

## 28. 同時攻撃と相打ち

相打ちを許すならA→BとB→Aの候補を両方収集してから確定します。片方を先に確定して死亡者の候補を消すと、更新順で結果が変わります。

## 29. 過剰ダメージ

```cpp
int CalculateOverkillDamage(int hpBefore, int requestedDamage)
{
    return std::max(requestedDamage - std::max(hpBefore, 0), 0);
}
```

スコアや演出へ使う過剰分と、HPの実減少量は別に扱います。

## 30. 無敵と0ダメージ

無敵は命中不成立、防御計算後の0は命中成立だがHP減少なし、と区別できます。前者は空振り音、後者は装甲音など演出も変わります。

```cpp
enum class HitOutcome
{
    Rejected,
    Invulnerable,
    Guarded,
    Damaged,
    ZeroDamage,
    Killed
};
```

## 31. 属性耐性

```cpp
struct ResistanceTable final
{
    float physical{1.0f}, fire{1.0f}, ice{1.0f};
    float electric{1.0f}, ether{1.0f};

    float Multiplier(DamageType type) const
    {
        switch (type)
        {
        case DamageType::Physical: return physical;
        case DamageType::Fire: return fire;
        case DamageType::Ice: return ice;
        case DamageType::Electric: return electric;
        case DamageType::Ether: return ether;
        case DamageType::TrueDamage: return 1.0f;
        }
        return 1.0f;
    }
};
```

`0.8`を20%軽減、`1.2`を20%弱点と決め、意味を統一します。

## 32. 部位破壊

```cpp
struct BreakablePart final
{
    float durability{100.0f};
    bool broken{};

    bool Apply(float amount)
    {
        if (broken)
            return false;
        durability = std::max(0.0f,
            durability - std::max(amount, 0.0f));
        if (durability > 0.0f)
            return false;
        broken = true;
        return true; // 初めて破壊された瞬間だけtrue。
    }
};
```

破壊イベントは一度だけ発行し、報酬や演出の重複を防ぎます。

## 33. 壁叩きつけ

```cpp
bool IsHardWallImpact(VECTOR velocity, VECTOR wallNormal,
                      float minimumSpeed)
{
    const float intoWallSpeed = -VDot(velocity, wallNormal);
    return intoWallSpeed >= minimumSpeed;
}
```

吹き飛ばし状態中だけ判定します。通常移動中まで使うと壁へ歩くだけでWallSplatになります。

## 34. 地面衝突とダウン

```cpp
bool ShouldEnterKnockDown(bool wasAirborne,
                         bool groundedNow,
                         float verticalVelocity)
{
    return wasAirborne && groundedNow && verticalVelocity <= 0.0f;
}
```

見た目だけでなくCharacter Controllerの接地結果で判定します。

## 35. 起き上がり無敵

ダウン中、起き上がり中、起き上がり後の保護時間を別Windowにします。無敵理由ごとのTokenを使うと、別の無敵まで誤って解除する事故を防げます。

## 36. コンボ補正

```cpp
float ComboDamageScale(std::uint32_t hitCount)
{
    const float scale = 1.0f - static_cast<float>(hitCount) * 0.03f;
    return std::clamp(scale, 0.30f, 1.0f);
}
```

攻撃定義そのものを書き換えず、その命中だけの係数として使います。

## 37. 空中コンボ制限

対象ごとに再打ち上げ回数や滞空時間を追跡します。上限後は打ち上げを小怯みや叩き落としへ置換し、永久拘束を防ぎます。

## 38. リアクション優先度

列挙型の整数順を強さとして使わず、明示します。

```cpp
int ReactionPriority(ReactionType type)
{
    switch (type)
    {
    case ReactionType::Death: return 100;
    case ReactionType::WallSplat: return 80;
    case ReactionType::KnockDown: return 70;
    case ReactionType::Launch: return 60;
    case ReactionType::KnockBack: return 50;
    case ReactionType::Stagger: return 40;
    case ReactionType::FlinchLarge: return 20;
    case ReactionType::FlinchSmall: return 10;
    default: return 0;
    }
}
```

## 39. 演出要求

```cpp
struct HitPresentationRequest final
{
    VECTOR position{};
    VECTOR normal{};
    int damage{};
    std::uint16_t hitStopTicks{};
    float cameraShakeAmplitude{};
    bool critical{};
    bool weakPoint{};
};
```

戦闘側は要求を作り、実際の時間制御や描画は各専門システムが担当します。

## 40. イベントキュー

```cpp
class DamageEventQueue final
{
public:
    void Push(DamageAppliedEvent event)
    {
        events_.push_back(std::move(event));
    }

    const std::vector<DamageAppliedEvent>& Events() const
    {
        return events_;
    }

    void EndFrame() { events_.clear(); }

private:
    std::vector<DamageAppliedEvent> events_{};
};
```

購読中の配列へ追加する可能性がある場合は、書込用と読取用を二重化します。

## 41. Damage Number

ダメージ数字は確定イベントから生成します。小ダメージを一定時間まとめる処理、会心の色や大きさはUI側の責務です。

## 42. SoundとSurface

攻撃名だけでなく、肉体、金属、盾、弱点などのSurfaceとHitOutcomeから音を選びます。同一音の連打制限もAudio側で行います。

## 43. VFXの向き

火花は接触法線、斬撃軌跡は攻撃方向を使う場合があります。イベントへ両方残し、演出側が用途に合う基準を選びます。

## 44. カメラ通知

戦闘処理からカメラ座標を直接変更せず、Shake Preset、強度、距離減衰の材料をイベントで渡します。

## 45. 決定性と再現性

固定Tick、記録可能な乱数Seed、命中候補の安定した並び順を使います。完全な同期が不要でも、不具合を再現しやすくなります。

## 46. 所有権

イベントへ寿命不明の生ポインタを保存しません。Entity ID、小さな値型、共有寿命が保証された不変定義を保存します。

## 47. デバッグ表示

```cpp
void DrawDamageDebug(int x, int y, const DamageResult& r)
{
    DrawFormatString(x, y, GetColor(255, 255, 255),
        "raw=%d mitigated=%d final=%d critical=%d weak=%d",
        r.rawDamage, r.mitigatedDamage, r.finalDamage,
        r.critical ? 1 : 0, r.weakPoint ? 1 : 0);
}
```

ID、途中値、HP前後、リアクション、棄却理由、命中座標も確認できるようにします。

## 48. 固定乱数テスト

```cpp
class FixedRandom final : public Random01
{
public:
    explicit FixedRandom(float value) : value_(value) {}
    float Next() override { return value_; }

private:
    float value_{};
};
```

会心の成立・不成立を毎回同じ条件で検証できます。

## 49. 境界値テスト

- HP1へ1、0、100ダメージを与える。
- 防御力が0、負数、非常に大きい。
- 倍率が0、1、1より大きい。
- 会心率が0%、100%、範囲外。
- 攻撃者と対象が同一座標。
- 同じAttackInstanceIdが二度届く。
- 死亡済み対象へ候補が届く。
- 姿勢値が境界の直前、同値、直後になる。

## 50. 不変条件

```cpp
#include <cassert>

void ValidateHealth(const Health& health)
{
    assert(health.maximum > 0);
    assert(health.current >= 0);
    assert(health.current <= health.maximum);
}
```

異常を早く止めると、後から現れる不可解なアニメーション不具合を減らせます。

## 51. よくある失敗：Callback内ですべて行う

衝突Callback内でHP、モーション、音、VFX、カメラまで変更すると再入と順序依存が起きます。Callbackは候補収集までに留めます。

## 52. よくある失敗：演出で結果を決める

VFX生成や音声再生に失敗してもダメージは成立すべきです。演出リソース不足がゲームルールを変えない構造にします。

## 53. よくある失敗：概念を一つにする

無敵、ダメージ軽減、怯み無効、スーパーアーマー、死亡耐性は別概念です。一つの`isStrong`フラグへまとめません。

## 54. パフォーマンス

- Team MaskやLayerで候補を早期除外する。
- 毎命中のヒープ確保を避け、配列を再利用する。
- イベントには必要な値だけを持たせる。
- 候補数、確定数、重複棄却数を測ってから最適化する。

## 55. スレッド化

スナップショットだけを読む計算は並列化しやすくなります。HPやScene Objectの実体更新、DXライブラリAPIの呼び出しはメインスレッドへ集約します。

## 56. 実装順序

1. AttackInstanceIdと重複防止を作る。
2. 副作用なしの計算関数を作る。
3. HPへ一度だけCommitする。
4. 姿勢とリアクションを加える。
5. ノックバックをCharacter Controllerへ接続する。
6. 確定イベントからVFX、音、UI、カメラを動かす。
7. 部位、属性、会心、コンボ補正を加える。
8. 境界値テストとデバッグ表示を作る。

## 57. 完成確認表

- [ ] 同じ攻撃が意図せず二度当たらない。
- [ ] マルチヒットが間隔と最大回数を守る。
- [ ] 計算途中の値を表示できる。
- [ ] HPが常に0～最大値に収まる。
- [ ] 死亡が通常リアクションより優先される。
- [ ] スーパーアーマー中もHPダメージは入る。
- [ ] ゼロ方向でNaNが発生しない。
- [ ] 演出生成失敗が戦闘結果を変えない。
- [ ] 固定乱数で会心テストを再現できる。

## 58. この章の要点

- 候補、計算、状態確定、演出通知を分離します。
- 攻撃実行IDで重複命中を防ぎます。
- 計算はスナップショットから結果を返す純粋な処理にします。
- HP、姿勢、リアクション、死亡を別概念として順序を決めます。
- ノックバックは速度としてCharacter Controllerへ渡します。
- 確定イベントをUI、VFX、音、カメラへ配信します。
- 固定Tickと制御可能な乱数でデバッグを再現可能にします。

次章では、この命中イベントを使ってヒットストップ、VFX、効果音、カメラシェイクを統合します。
