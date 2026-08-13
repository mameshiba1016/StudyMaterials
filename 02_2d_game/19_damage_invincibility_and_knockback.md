# ダメージ・無敵・ガード・ノックバック

Damage処理はHP減算だけでなく、攻撃側・防御側の状態、無敵、Armor、Guard、Critical、HitStun、演出を一度の解決結果へまとめます。

## Damage RequestとResult

```cpp
struct DamageRequest
{
    EntityId attacker{};
    EntityId target{};
    AttackInstanceId attackInstance{};
    int baseDamage{};
    DamageType type{};
    Vector2 hitDirection{};
    Vector2 knockback{};
};

struct DamageResult
{
    bool wasAccepted{};
    bool wasBlocked{};
    bool wasCritical{};
    bool causedDeath{};
    int finalDamage{};
    HitReaction reaction{};
};
```

VFXや音はResultを見て選びます。Damage関数内で直接すべて再生するとテストが難しくなります。

## 解決順序

一例：

```text
Target有効性
→ 同じAttackの再命中検査
→ 無敵・Team・Friendly Fire
→ Parry
→ Guard
→ Armor/Resistance
→ Critical・倍率
→ HP適用
→ Reaction・Death
```

順序は仕様です。Critical後にDefenseを引くか、Guard時も状態異常が入るかを固定します。

## 整数計算

```cpp
std::int64_t working{request.baseDamage};
working = working * attackPercent / 100;
working = working * defensePercent / 100;
working = std::clamp<std::int64_t>(working, 0, maximumDamage);
const int finalDamage{static_cast<int>(working)};
```

広い型で中間計算し、丸め順と上限を仕様化します。負DamageをHealとして暗黙利用せず別Operationへします。

## 無敵時間

```cpp
enum class InvincibilityTag : std::uint32_t
{
    None     = 0,
    Dodge    = 1u << 0,
    Respawn  = 1u << 1,
    Cutscene = 1u << 2
};
```

単一`bool`では複数理由が重なり、一方終了時に誤って無敵を解除します。Token/Tagごとに期間を管理します。攻撃Type別に無効化する場合はMaskも持ちます。

## Dodge i-frame

Animationの見た目と無敵WindowをAttack/Actionデータで同期します。

```text
Startup 2 ticks → Invincible 8 ticks → Recovery 6 ticks
```

回避開始直後から無敵か、入力受付中のHitとどちらが先かはUpdate順で決まります。

## Guard

攻撃方向と防御Facingの内積で正面判定できます。Guard Damage、Stamina削り、Chip Damage、Pushback、Guard BreakをResultへ記録します。背面攻撃は通常Hitへ回します。

## Parry

短い受付WindowでHitを無効化し、攻撃側へ専用Stunを与えます。同tick複数Hit、飛び道具Owner、BossのParry可否を定義します。Parry成功演出中も結果は一度だけ適用します。

## HitStunとKnockback

```cpp
velocity = TransformByFacing(request.knockback, attackerFacing);
hitStunRemainingTicks = result.hitStunTicks;
```

絶対速度へ設定するか現在速度へ加算するかで挙動が違います。地上・空中、重量、Super Armor、壁接触によるReactionを分けます。

## Hit Stop

攻撃側と防御側で停止時間を変えられます。複数HitStop要求は最大値、加算、上書きのどれかを決めます。Simulationを止めても命中VFXが動く時間レイヤーを用意します。

## Death

HPが0になった瞬間にEntityを削除すると、Damage Resultの演出や参照が壊れます。

```text
Alive → Dying → Dead/Removed
```

Dying状態でCollider無効化、死亡Animation、Drop生成を行い、安全な地点で削除します。死亡イベントの二重発火を防ぎます。

## 同時攻撃

Damageイベントを逐次適用すると順序で相打ち可否が変わります。同tickのHitを一度収集し、優先度または同時解決規則で処理します。競技性やリプレイが重要なら安定順序を保証します。
