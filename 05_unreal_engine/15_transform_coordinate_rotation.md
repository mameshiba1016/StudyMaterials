# 15 Transform、座標空間、回転

## 1. UEの座標系

UEでは既定でXが前、Yが右、Zが上です。距離の基本単位はセンチメートルとして扱われます。`100.0`は通常1メートル相当です。

```cpp
const FVector Forward = FVector::ForwardVector; // (1, 0, 0)
const FVector Right   = FVector::RightVector;   // (0, 1, 0)
const FVector Up      = FVector::UpVector;      // (0, 0, 1)
```

DCCツールや他Engineからデータを持ち込むと軸・単位・回転方向が異なる場合があります。見た目で補正を重ねる前に、Import設定と基準座標を確認します。

## 2. WorldとLocal

- World Space：Level全体を基準にした座標。
- Local／Relative Space：親Componentを基準にした座標。

```cpp
const FVector WorldLocation = WeaponMesh->GetComponentLocation();
const FVector RelativeLocation = WeaponMesh->GetRelativeLocation();
```

親が動けばLocal値が同じでもWorld値は変わります。Socketへ付いた武器の攻撃判定は、武器Local点をMesh TransformでWorldへ変換して使用します。

```cpp
const FVector LocalTip(100.0, 0.0, 0.0);
const FVector WorldTip = WeaponMesh->GetComponentTransform().TransformPosition(LocalTip);

// 方向には平行移動を加えないためTransformVector系を使う。
const FVector WorldDirection = WeaponMesh->GetComponentTransform()
    .TransformVectorNoScale(FVector::ForwardVector)
    .GetSafeNormal();
```

位置に`TransformVector`、方向に`TransformPosition`を使うと、平行移動の有無が逆になり壊れます。

## 3. FTransform

`FTransform`はTranslation、Rotation（`FQuat`）、Scaleをまとめます。

```cpp
const FTransform SocketTransform = GetMesh()->GetSocketTransform(
    TEXT("weapon_r"),
    RTS_World);

const FVector SpawnLocation = SocketTransform.GetLocation();
const FQuat SpawnRotation = SocketTransform.GetRotation();
```

Transformの合成順序は「どちらの空間からどちらへ変換するか」を文章にしてから書きます。順序を入れ替えると、回転した親の下でのみずれるバグになります。

## 4. FRotatorとFQuat

`FRotator`はPitch、Yaw、Rollで読みやすくEditorにも適します。`FQuat`は回転合成や滑らかな補間で扱いやすく、Gimbal Lock問題を避けやすい表現です。

```cpp
const FVector ToTarget = TargetLocation - GetActorLocation();
if (!ToTarget.IsNearlyZero())
{
    const FRotator DesiredRotation = ToTarget.Rotation();
    const FRotator YawOnly(0.0f, DesiredRotation.Yaw, 0.0f);
    SetActorRotation(YawOnly);
}
```

正規化不能なゼロVectorを方向として使わないでください。

## 5. 補間は速度ではなく到達規則

```cpp
const FRotator NewRotation = FMath::RInterpTo(
    CurrentRotation,
    DesiredRotation,
    DeltaSeconds,
    RotationInterpSpeed);
```

`RInterpTo`は差が大きいほど速く、目標へ減速しながら近づく補間です。一定角速度が必要なら`RInterpConstantTo`を検討します。補間値の意味を知らず「大きいほど速い数字」とだけ覚えないでください。

## 6. DotとCross

```cpp
const FVector Forward2D = GetActorForwardVector().GetSafeNormal2D();
const FVector ToEnemy2D = (EnemyLocation - GetActorLocation()).GetSafeNormal2D();

const double FacingDot = FVector::DotProduct(Forward2D, ToEnemy2D);
const double SideSign = FVector::CrossProduct(Forward2D, ToEnemy2D).Z;
```

- Dot：方向の一致度。1に近いと前、0付近は横、-1付近は後ろ。
- CrossのZ符号：水平面上で左右の判定に利用可能。

ロックオン候補評価では距離だけでなく、Camera前方とのDot、遮蔽、画面中心距離を組み合わせます。

## 7. 角度比較の安全な方法

```cpp
const double MinDot = FMath::Cos(FMath::DegreesToRadians(MaxTargetAngleDegrees));
const bool bInsideCone = FVector::DotProduct(ViewForward, ToTarget) >= MinDot;
```

毎候補で`acos`して角度へ戻すより、Cos閾値とDotを直接比較できます。入力Vectorは正規化されている必要があります。

## 8. 浮動小数点を完全一致比較しない

```cpp
if (FMath::IsNearlyEqual(CurrentHealth, MaxHealth, KINDA_SMALL_NUMBER))
{
    // 誤差を許容した比較。
}
```

座標、回転、時間を`==`で比較すると演算誤差で成立しない場合があります。目的に合う許容値を決めます。巨大Worldでは座標精度と原点からの距離も考慮します。

## 9. 高速アクションでの注意

- Camera基準移動はPitchを除いたYaw平面を使う。
- Target方向が真上に近い場合の水平回転を定義する。
- Root Motionと手動Transform更新を競合させない。
- 武器Traceは前Frameと現在FrameのSocket位置を結ぶ。
- Scale込み方向変換で攻撃方向が歪まないようにする。
- Character交代時は出現TransformとCamera Transformを別々に補間する。

座標バグは「数値」より「どの空間の値か」を型名や変数名へ含めると減らせます。
