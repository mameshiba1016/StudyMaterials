# 高速3Dアクション戦闘の基礎

特定作品を複製せず、高速なCharacter Actionに共通する入力、状態、判定、Animation、Camera、演出の接続を学びます。

## Attack DefinitionとInstance

```cpp
struct AttackDefinition
{
    AttackId id{};
    int startupTicks{};
    int activeTicks{};
    int recoveryTicks{};
    int damage{};
    float trackingAngleRadians{};
    float trackingDistance{};
    std::vector<HitShapeKeyframe> hitShapes{};
    std::vector<CancelWindow> cancelWindows{};
};
```

Definitionは共有不変Data、Instanceは経過tick、命中済みTarget、Hit Confirm等を持ちます。

## 3D Hit Shape

Weapon軌跡にはBone間Capsule、前Frameから今FrameのSwept Shapeを使います。現在Pose一点だけでは高速Swordが敵をすり抜けます。

```text
previous socket transform
→ current socket transform
→ swept capsule/segment query
```

Animation更新後のSocket PoseとPhysics Queryの時間を揃えます。

## Startup・Active・Recovery

固定Simulation tickで管理します。Animation Play Rateを変えてもGameplay Windowが意図せず変化しない設計か、両方を同一Timeline Dataから生成します。

## ComboとCancel

入力Commandを短時間Bufferし、現在AttackのCancel Windowで次Nodeへ遷移します。

- Hit時のみ派生。
- Whiff時も派生。
- Dodge/Parry優先。
- Skill Resource条件。
- 地上/空中条件。

遷移拒否理由をDebug表示します。

## Attack Tracking

攻撃開始前またはStartup中にTarget方向へCharacterを回します。

```cpp
const float angle{AngleBetween(forward, toTarget)};
const float allowed{definition.trackingAngleRadians};
```

最大角度・回転速度・距離・遮蔽を制限し、Active中の不自然な180度回転を防ぎます。

## Dodge・無敵

移動WindowとInvincibility Windowを別に持ちます。複数無敵理由はTag/Tokenで管理し、一つの終了が別の無敵を解除しないようにします。

## Parry

短い受付WindowでIncoming HitをParry Resultへ変換します。

```text
Hit候補収集
→ Defender Parry Window判定
→ Attack Type/方向判定
→ Damage無効
→ Attacker Reaction + Camera/SE/VFX
```

同tick複数Hitを一度だけ成功扱いにする規則が必要です。

## Hit Reaction

Damage量だけでなくAttack Level、方向、Armor、空中状態からReactionを選びます。

- Light Stagger。
- Heavy Stagger。
- Launch。
- Knockdown。
- Wall reaction。
- Super ArmorでNo Stagger。

DamageとReactionを分離するとBossのArmorを作れます。

## Hit Stop

攻撃側、防御側、World、Effect、Cameraの時間Layerを分けます。重いHitほど長くしますが、連続Hitで操作不能にならない上限と合成規則を設けます。

## Homingと移動

Root Motion、Gameplay Translation、Target補正、Collision解決を一つの移動Pipelineへ通します。Targetへ強制Teleportせず、到達可能距離と壁を検査します。

## Resource

Energy、Cooldown、Gauge等はAction開始時にReserve/Consumeする時点を統一します。Animation開始後に不足判定して戻すと表示が跳びます。

## 交代・支援へつながる境界

Combat Commandの実行者とControlled Characterを分離します。交代処理では入力所有、Camera Target、Lock Target、無敵、旧Characterの退場Action、新Characterの登場ActionをAtomicなState Transitionとして扱います。専用章で詳しく扱います。

## 更新順

```text
Input Command
→ Action State/Cancel
→ Animation Pose/Root Motion
→ Character Movement/Collision
→ Hit Shape更新・Query
→ Hit結果収集
→ Parry/Guard/Damage解決
→ Reaction/Death
→ VFX/Audio/Camera Event
```

順序を安定させ、Entity配列順で相打ち結果が変わらないようにします。

## Debug

State、Attack tick、Buffer、Cancel Window、Tracking Cone、Hit Shape軌跡、命中済みTarget、無敵Tag、Hit Result、Root Motionを表示・記録します。
