# 17 Line Trace、Shape Sweep、Overlap

## 1. 3種類の空間問い合わせ

- Line Trace：StartからEndまで線を飛ばす。
- Shape Sweep：Sphere、Capsule、Box等をStartからEndへ移動させる。
- Overlap：指定地点の形状内に現在存在する対象を列挙する。

高速な武器はFrame間を飛び越えるため、現在位置だけのOverlapより、前回位置から現在位置へのSweepが安定します。

## 2. LineTraceSingleByChannel

```cpp
FHitResult Hit;
FCollisionQueryParams Params(SCENE_QUERY_STAT(TargetVisibility), false);
Params.AddIgnoredActor(this);

const bool bHit = GetWorld()->LineTraceSingleByChannel(
    Hit,
    CameraLocation,
    TargetLocation,
    ECC_Visibility,
    Params);

if (!bHit || Hit.GetActor() == TargetActor)
{
    // 遮蔽物なし、または最初に当たった物が対象自身。
}
```

`FHitResult`にはActorだけでなく、Component、Impact Point、Impact Normal、距離、Bone Name等が含まれます。目的に必要な項目を読みます。

## 3. SingleとMulti

- Single：最初のBlocking Hitを中心に1件取得。
- Multi by Channel：Overlapを列挙し、最初のBlockまで取得する性質を理解する。
- Multi for Objects：指定Object Typeに一致する複数対象を取得。

「Multiなら線上のすべてを無条件に返す」と覚えないでください。Channel／Object QueryとResponseによって結果が変わります。

## 4. Sphere Sweep

```cpp
TArray<FHitResult> Hits;
FCollisionShape Shape = FCollisionShape::MakeSphere(AttackRadius);

FCollisionQueryParams Params(SCENE_QUERY_STAT(WeaponSweep), false, GetOwner());

const bool bAnyHit = GetWorld()->SweepMultiByChannel(
    Hits,
    PreviousTipLocation,
    CurrentTipLocation,
    FQuat::Identity,
    AttackTraceChannel,
    Shape,
    Params);

if (bAnyHit)
{
    for (const FHitResult& Hit : Hits)
    {
        TryApplyHit(Hit);
    }
}
```

武器の根元と先端を複数Sweepする、Boxを武器姿勢へ回転させるなどで刃全体を近似できます。精度を上げるほどQuery数が増えるため、可視化して必要最小限を探します。

## 5. OverlapMultiByObjectType

```cpp
TArray<FOverlapResult> Results;

FCollisionObjectQueryParams Objects;
Objects.AddObjectTypesToQuery(ECC_Pawn);

const bool bFound = GetWorld()->OverlapMultiByObjectType(
    Results,
    SearchCenter,
    FQuat::Identity,
    Objects,
    FCollisionShape::MakeSphere(SearchRadius));
```

ロックオン候補の粗い収集に向きます。その後にTeam、死亡状態、距離、画面内、視線遮蔽などで絞り込みます。Overlap結果の順序を仕様として信用せず、自分でScore計算と安定Sortを行います。

## 6. Query Params

```cpp
FCollisionQueryParams Params(SCENE_QUERY_STAT(AttackQuery), false);
Params.AddIgnoredActor(Attacker);
Params.AddIgnoredActors(ActorsToIgnore);
Params.bReturnPhysicalMaterial = true;
```

`SCENE_QUERY_STAT`へ意味のある名前を付けるとProfilerで識別しやすくなります。`bTraceComplex`、無視対象、Physical Material返却などは必要な時だけ有効化します。

## 7. Debug Draw

```cpp
DrawDebugLine(
    GetWorld(),
    Start,
    End,
    bHit ? FColor::Red : FColor::Green,
    false,
    1.0f,
    0,
    1.5f);
```

Start／End、形状、Hit Point、Normalを描画します。ただしShippingへ常時残さず、Debug設定またはConsole Variableで切り替えます。

## 8. 攻撃判定Pipeline

```text
Animation更新後のSocket位置を取得
  ↓
前回位置から現在位置へSweep
  ↓
Collision Channelで粗く除外
  ↓
Team・無敵・死亡・同一Hit判定
  ↓
HitDataを構築
  ↓
Damage／Reaction Systemへ要求
  ↓
VFX、SFX、Hit StopをPresentationへ通知
```

Trace関数の中で全処理を直接行わず、検出、Gameplay検証、結果適用、演出通知を分けるとテストしやすくなります。

## 9. 性能と正しさ

- 毎Frame全敵へ個別Line Traceせず、候補を空間Queryで絞る。
- 同じ遮蔽結果を必要範囲で短時間Cacheする。
- Complex Traceを必要な攻撃だけに限定する。
- Multi結果の配列確保を把握する。
- 非同期Scene Queryは結果が後で届くため、要求時のActor寿命と状態IDを検証する。
- Debug Draw自体の負荷も計測する。

## 参考

- [Traces Overview](https://dev.epicgames.com/documentation/en-us/unreal-engine/traces-in-unreal-engine---overview)
