# 28 回避、無敵、ガード、パリィ

## 1. 4つの仕組みを区別する

- 回避移動：位置を変えるAction。
- Invulnerability：Damageを無効化する状態。
- Guard：条件を満たすHitを軽減・無効化しResourceを消費。
- Parry：短いWindowで攻撃を受け止め、専用結果へ反転。

回避Montageが再生中だから無敵、と暗黙に結び付けず、Combat StateでWindowを管理します。

## 2. Damage受付Pipeline

```text
Hit到着
  ↓ Targetable / Team / 重複確認
Invulnerable? → 無効結果
  ↓ no
Parry成立?    → Parry結果
  ↓ no
Guard成立?    → Guard結果
  ↓ no
通常Damage    → Hit Reaction
```

評価順を固定します。同時に複数成立した時の優先順位が仕様になります。

## 3. 無敵Window

```cpp
void UDefenseComponent::BeginInvulnerability(int32 ActionSequenceId)
{
    InvulnerabilitySources.Add(ActionSequenceId);
}

void UDefenseComponent::EndInvulnerability(int32 ActionSequenceId)
{
    InvulnerabilitySources.Remove(ActionSequenceId);
}

bool UDefenseComponent::IsInvulnerable() const
{
    return !InvulnerabilitySources.IsEmpty();
}
```

単一boolでは、複数Sourceが重なった時に片方の終了で無敵を誤解除します。Source IDまたはTag Stackで管理します。Action終了時には該当SourceをFail-safe解除します。

## 4. 回避方向

```cpp
FVector UCombatMovement::ResolveDodgeDirection(
    const FVector2D& Input,
    const FRotator& CameraRotation,
    const AActor* LockTarget)
{
    // 入力あり：Camera基準方向。
    // 入力なし＋Lock On：Targetから離れる後方。
    // どちらもなし：Character前方。
}
```

入力なし回避、Lock On中、空中回避、壁際での規則を決めます。Animation方向と実際のMovement方向を同じ解決結果から作ります。

## 5. Guard角度

```cpp
const FVector Forward = Defender->GetActorForwardVector().GetSafeNormal2D();
const FVector ToAttacker = (AttackerLocation - DefenderLocation).GetSafeNormal2D();
const float MinimumDot = FMath::Cos(FMath::DegreesToRadians(GuardHalfAngle));
const bool bInsideGuardArc = FVector::DotProduct(Forward, ToAttacker) >= MinimumDot;
```

ProjectileではAttacker位置でなく飛来方向を使う場合があります。背後攻撃をGuardできるか、全方位BarrierかをDefense Dataへ持たせます。

## 6. Guard Resource

```text
Guard Damageを受ける
  ↓
Guard Gauge減少
  ├─ 残量あり → Guard Reaction
  └─ 0以下    → Guard Break / Stun
```

HP軽減率、削りDamage、Stamina回復開始遅延、連続Guard時の挙動をData化します。

## 7. Parry Window

Parryは「Buttonを押した瞬間」ではなく、Startup、Active、Recoveryを持つActionとして扱います。

```cpp
bool UDefenseComponent::CanParry(const FCombatHitContext& Hit) const
{
    return bParryWindowOpen
        && HitProperties.CanBeParried()
        && IsWithinParryAngle(Hit)
        && !AlreadyParriedSequences.Contains(Hit.ActionSequenceId);
}
```

同じ攻撃の複数Hurtbox／Multi TraceでParryが複数回成立しないよう、攻撃Sequenceを記録します。

## 8. Parry結果は双方へ適用する

```text
Defender: 成功Animation、Resource回復、無敵、反撃入力Window
Attacker: 弾かれ、Poise Damage、Action中断または専用Reaction
World:    Hit Stop、VFX、SFX、Camera Shake
```

双方の状態遷移を一つの解決Resultから適用します。片方だけ失敗した場合に整合性が崩れない順序を決めます。

## 9. Perfect Dodge

Perfect Dodgeは無敵中にHit Candidateが接触した事実を検出し、Damage無効とは別に一度だけ成功Eventを発生させます。

```text
Hit検出 → InvulnerableなのでDamage 0
        → Perfect Dodge対象か
        → 同Attackで未発生なら成功
```

Hitbox自体をCollision Ignoreにすると「避けた攻撃」を検出できません。Queryは受けつつDamage Pipelineで無効化する構成が候補です。

## 10. 公平性とFeedback

- Parry不能攻撃を色・音・Animationで予告。
- Window開始前と終了後の失敗理由を一貫させる。
- Hit Stop中の入力受付を仕様化。
- 大型Bossでも攻撃方向を認識可能にする。
- 難易度でWindowを変えてもAnimationと表示を合わせる。

## 11. テスト

- Window境界の直前・直後。
- 同Frameに複数攻撃。
- 背後、真上、Projectile。
- Montage中断後に無敵が残らない。
- Time Dilation中のWindow時間。
- Character交代中のHit。
- Parry成功後に攻撃者がDestroyされる。
