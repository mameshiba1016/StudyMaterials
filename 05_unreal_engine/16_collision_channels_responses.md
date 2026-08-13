# 16 CollisionのChannel、Object Type、Response

## 1. Collisionは両者の設定で決まる

衝突Componentは、自分が何者かを示すObject Typeと、相手の各Type／Trace Channelへどう反応するかを持ちます。

反応は次の3種類です。

- `Ignore`：無視する。
- `Overlap`：通過を許し、条件を満たせばOverlapイベントを発生。
- `Block`：物理的に遮り、条件を満たせばHitイベントを発生。

2つの物体の最終的な相互作用は、片側の設定だけではなく両側のResponseから決まります。

## 2. Collision Enabled

| 設定 | Query | Physics Simulation |
|---|---:|---:|
| `NoCollision` | 無効 | 無効 |
| `QueryOnly` | Trace／Overlap有効 | 無効 |
| `PhysicsOnly` | 無効 | 物理有効 |
| `QueryAndPhysics` | 有効 | 有効 |

攻撃判定は通常Query中心です。見えないHitboxを物理Simulationさせる必要はありません。用途を絞ることで不要な処理と予期しない反応を減らせます。

## 3. Object ChannelとTrace Channel

- Object Channel：そのComponentが何者か。Pawn、WorldStatic、PhysicsBody等。
- Trace Channel：問い合わせの目的。Visibility、Camera、独自AttackTrace等。

「敵だけ探す」ならObject Type検索、「Camera視線を遮る物を調べる」ならTrace Channelが自然です。Project Settingsで独自ChannelとPresetを定義し、意味のある名前を付けます。

## 4. Presetで構成を統一する

```cpp
Hitbox->SetCollisionProfileName(TEXT("PlayerAttackHitbox"));
```

コード各所でResponseを1項目ずつ変更すると、設定差が追跡困難になります。`PlayerAttackHitbox`、`EnemyHurtbox`、`Targetable`などのPresetをProject設定で定義し、例外だけコードで変更します。

```cpp
Hitbox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
Hitbox->SetCollisionObjectType(ECC_WorldDynamic);
Hitbox->SetCollisionResponseToAllChannels(ECR_Ignore);
Hitbox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
```

上記は仕組みを示す例です。本番では独自ChannelとPresetを使い、Player自身や味方をQuery Parameter、Team判定、Gameplay Tag等でも除外します。

## 5. HitとOverlapのイベント条件

```cpp
Hitbox->SetGenerateOverlapEvents(true);
Hitbox->OnComponentBeginOverlap.AddDynamic(
    this,
    &UAttackHitboxComponent::HandleBeginOverlap);
```

Overlap Responseだけ設定しても、Generate Overlap Eventsや相手側設定、Component登録状態などが不正ならイベントは来ません。HitイベントもBlockingと通知設定を区別します。

イベントが来ないときは次を確認します。

1. 両ComponentのCollision Enabled。
2. 両者のObject Typeと相互Response。
3. Overlap／Hitイベント生成設定。
4. 実際にCollision形状が存在するか。
5. すでにOverlap中に有効化していないか。
6. Collisionを持つComponentが想定と同じか。

## 6. SimpleとComplex Collision

- Simple：Box、Sphere、Capsule、凸形状など、軽量な代理形状。
- Complex：描画Triangleに近い形状をQueryに使う。

移動や物理SimulationではSimple形状が基本です。Complex Traceは表面の詳細が必要な用途に限定し、攻撃判定へ無条件に使わないでください。見た目通りの細かさと安定したGameplay判定は別の目標です。

## 7. HurtboxとHitbox

```text
Character Capsule = 移動とWorld衝突
Hurtbox群         = 攻撃を受ける領域
Attack Hitbox     = 攻撃中だけ有効な検出領域
Targetable        = ロックオン候補用の代表点／形状
```

1つのCapsuleですべて判定すると、足元への攻撃、部位、演出上の当たりやすさを調整できません。ただし骨ごとに大量Componentを常時Overlapさせると負荷が増えるため、用途と精度に応じてPhysics Asset QueryやSweepを選びます。

## 8. 同一攻撃の多重Hitを防ぐ

```cpp
if (HitActors.Contains(OtherActor))
{
    return;
}

HitActors.Add(OtherActor);
ApplyAttackTo(OtherActor);
```

Overlapは複数Component、再侵入、複数Frameで発生し得ます。「攻撃Instanceごとに一度」「多段攻撃の各Hitごとに一度」など、重複規則をAttack IDとHit Setで管理します。

## 9. Collisionを一時的に切り替える

```cpp
void UAttackHitboxComponent::OpenHitbox()
{
    HitActors.Reset();
    SetCollisionEnabled(ECollisionEnabled::QueryOnly);
}

void UAttackHitboxComponent::CloseHitbox()
{
    SetCollisionEnabled(ECollisionEnabled::NoCollision);
}
```

Montage中断、被弾、Character交代、EndPlayのすべてで閉じるFail-safeが必要です。Notifyだけに依存すると、終了Notifyへ到達しなかった際にHitboxが残ります。

## 参考

- [Collision Overview](https://dev.epicgames.com/documentation/unreal-engine/collision-in-unreal-engine---overview)
- [Collision Response Reference](https://dev.epicgames.com/documentation/unreal-engine/collision-response-reference-in-unreal-engine)
