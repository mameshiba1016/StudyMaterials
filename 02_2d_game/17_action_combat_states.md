# 2Dアクション戦闘の状態設計

戦闘は入力、状態、アニメーション、判定、ダメージ、演出が同じ時間軸で連携します。大量の`bool`では不可能な組合せが生まれるため、状態と能力条件を明示します。

## 状態例

```cpp
enum class ActionState
{
    Idle,
    Move,
    Jump,
    Fall,
    Attack,
    Dodge,
    Guard,
    HitStun,
    KnockDown,
    Dead
};
```

LocomotionとActionを別レイヤーにする設計もあります。状態を分けすぎると組合せ爆発、まとめすぎると巨大switchになるため、同時に成立できる責任で分割します。

## 状態ライフサイクル

```cpp
class IActionState
{
public:
    virtual ~IActionState() = default;
    virtual void Enter(ActionContext& context) = 0;
    virtual void Update(ActionContext& context, float fixedDelta) = 0;
    virtual void Exit(ActionContext& context) = 0;
};
```

小規模ならenumとswitchの方が見通しがよい場合があります。パターンではなく遷移数と状態固有データで判断します。

## 遷移要求

状態Update中に自分自身を即破棄せず、次状態を要求して安全地点で`Exit → Enter`します。複数要求の優先順位を定めます。

```text
Dead > HitStun > Dodge > Attack > Locomotion
```

ただし無敵中はHitStun不可、Armor中はDamageのみ受ける等、条件を一か所で評価します。

## Attack定義

```cpp
struct AttackDefinition
{
    AttackId id{};
    int damage{};
    int startupTicks{};
    int activeTicks{};
    int recoveryTicks{};
    Vector2 knockback{};
    HitStopDefinition hitStop{};
    std::vector<CancelWindow> cancelWindows{};
};
```

- Startup：入力から判定発生まで。
- Active：Hit Boxが有効。
- Recovery：判定終了後、行動不能な期間。

合計tickとAnimation長がずれる場合の再生倍率・終端規則を決めます。

## Attack Instance

定義は共有不変データ、Instanceは一回の攻撃状態です。

```cpp
struct AttackInstance
{
    const AttackDefinition* definition{};
    AttackInstanceId instanceId{};
    int elapsedTicks{};
    std::vector<EntityId> hitTargets{};
};
```

同じ定義の攻撃を複数Entityが同時に使うため、命中済み対象を定義側へ保存してはいけません。

## Update順

```text
Command確定
→ 状態遷移
→ Attack tick更新
→ Hit Box生成
→ Collision
→ Hit結果を収集
→ Damage解決
→ HitStop/VFX/SE
→ Death/次状態
```

順序は相打ち、回避成功、死亡時攻撃の有効性を決めます。

## Facing Lock

攻撃開始時にFacingを固定するか、途中まで追尾するかを技ごとに定義します。入力方向、Target方向、現在Facingの優先順位を明確にします。

## 攻撃中の移動

Root Motion相当の速度カーブ、Impulse、通常移動倍率をAttackデータへ持てます。位置をAnimation画像から推測せず、ゲームルール用の移動データを使います。

## スーパーアーマー

Damageは受けるがHitStunへ遷移しない状態です。残りArmor耐久、対象攻撃Level、期間を設計します。無敵と混同しません。

## デバッグ表示

現在状態、経過tick、入力バッファ、Cancel可能先、Hit/Hurt Box、命中済みID、Armor、無敵残りを一画面に表示します。一tick送りと攻撃速度変更も有用です。
