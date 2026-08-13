# 21 ロックオンシステムの設計

## 1. ロックオンは4段階に分ける

```text
候補収集 → 候補検証 → Score評価・選択 → 維持・解除・切替
```

Camera制御やCharacter回転は「現在Target」を消費する側です。候補検索とCamera処理を一つのTick関数へ詰め込まないでください。

## 2. Targetableを表す

具体的なEnemyクラスへCastするより、InterfaceまたはComponentで能力を表します。

```cpp
UINTERFACE(BlueprintType)
class UTargetable : public UInterface
{
    GENERATED_BODY()
};

class ITargetable
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
    bool CanBeTargeted() const;

    UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
    FVector GetTargetPoint() const;
};
```

大型Bossは複数Target Pointを持つ場合があります。その場合はActor一体ではなくTarget Point Component単位で候補を表します。

## 3. 候補収集

```cpp
TArray<FOverlapResult> Overlaps;
FCollisionObjectQueryParams ObjectQuery;
ObjectQuery.AddObjectTypesToQuery(TargetableObjectChannel);

GetWorld()->OverlapMultiByObjectType(
    Overlaps,
    OwnerLocation,
    FQuat::Identity,
    ObjectQuery,
    FCollisionShape::MakeSphere(SearchRadius));
```

この段階は粗く広く集めます。毎Frame全Actorを列挙するのではなく、Collision Query、登録済みTarget一覧、空間分割を使います。

## 4. 候補検証

候補ごとに次を確認します。

- `IsValid`か。
- 自分自身、味方、死亡済みでないか。
- 距離範囲内か。
- Camera前方Coneまたは画面範囲内か。
- Target Pointが有効か。
- Visibility Traceで遮蔽されていないか。

一時的な遮蔽ですぐ解除すると柱の裏でTargetが頻繁に外れます。遮蔽継続時間の猶予を持たせます。

## 5. Score計算

```cpp
float UTargetingComponent::CalculateScore(
    const FVector& CameraLocation,
    const FVector& CameraForward,
    const FVector& TargetPoint) const
{
    const FVector ToTarget = TargetPoint - CameraLocation;
    const float Distance = ToTarget.Size();
    const FVector Direction = ToTarget.GetSafeNormal();

    const float Alignment = FVector::DotProduct(CameraForward, Direction);
    const float Distance01 = FMath::Clamp(Distance / MaxLockDistance, 0.0f, 1.0f);

    // 前方一致度を加点し、遠距離を減点する単純例。
    return AlignmentWeight * Alignment - DistanceWeight * Distance01;
}
```

実用ではScreen Centerからの距離、現在Targetへの維持Bonus、Boss優先度、攻撃中の相手などを加えます。重みはData化し、Debug表示で各内訳を確認します。

## 6. Screen Space評価

World上で同じ角度でもAspect RatioやFOVで画面上の位置が変わります。`ProjectWorldLocationToScreen`等でTarget Pointを画面座標へ変換し、Viewport中心からの正規化距離を使います。

画面外候補を完全除外するか、わずかに外でも切替対象にするかは操作仕様です。UI Scale、Split Screen、Safe Zoneを考慮します。

## 7. Target切替

右Stick入力で切り替える場合、現在Targetから見て画面上の左右方向にある候補を選びます。

```cpp
const FVector2D Delta = CandidateScreenPosition - CurrentTargetScreenPosition;
if (SwitchInputX > 0.0f && Delta.X <= MinimumHorizontalSeparation)
{
    // 右切替なのに右側でない候補を除外。
    return InvalidScore;
}
```

単純な最短距離だけでは、上下にいる敵や現在Targetとほぼ同位置の敵へ飛びます。入力方向とのDot、画面距離、水平分離、直前Targetへ戻るPenaltyを組み合わせます。

## 8. 維持と解除

現在Targetは`TWeakObjectPtr`等で保持し、破棄を妨げません。

```cpp
void UTargetingComponent::SetTarget(AActor* NewTarget)
{
    ClearTarget();
    CurrentTarget = NewTarget;

    if (NewTarget)
    {
        NewTarget->OnDestroyed.AddDynamic(this, &UTargetingComponent::HandleTargetDestroyed);
    }
}
```

解除条件：Target破棄、死亡、最大維持距離超過、長時間遮蔽、Level遷移、Player死亡、手動解除。Character交代ではTargetをPlayerController／Party側で維持するか、新CharacterのTargeting Componentへ安全に引き継ぐかを決めます。

## 9. Lock On中のCamera

PlayerとTargetの中点を単純に見るだけでは、距離が広がった時に双方が画面外へ出ます。

- 注視点をPlayerとTargetの重み付き中間へする。
- 距離に応じてArm Length／FOVを変える。
- Pitchの上下限を設ける。
- Camera Collision後にもTargetが見えるか確認。
- 大型敵はTarget Pointの高さを使う。

CameraはTargeting Systemの現在Targetを読むだけにし、Target選択自体は行いません。

## 10. Lock On中の移動・攻撃

- Character FacingをTargetへ補間。
- 入力をTarget基準の前後左右へ変換。
- 後退とStrafe Animationを選択。
- 攻撃開始時だけ軽いTarget補正を掛ける。
- Targetが背後へ高速移動した場合の回転速度上限を決める。

Targetへ瞬間回転させ続けると、Animation Foot Slidingや不自然な追尾になります。攻撃ごとに追尾可能角度、距離、時間窓を定義します。

## 11. デバッグ表示

候補Sphere、Visibility Line、Target Point、画面座標、Score内訳、解除猶予Timerを表示します。

```text
Enemy_A: alignment 0.91, screen 0.12, distance 0.35, total 0.68
Enemy_B: alignment 0.82, screen 0.08, distance 0.20, total 0.71 ← selected
```

感覚だけでWeightを調整せず、「なぜその敵が選ばれたか」を再現可能にします。

## 12. テスト項目

- 同じ方向に敵が重なる。
- Targetが死亡／Destroyされる。
- 柱の裏へ一瞬隠れる。
- 大型Bossの足元と頭部を切り替える。
- Character交代中にTargetも移動する。
- 画面外から高速で敵が侵入する。
- Camera FOVとAspect Ratioを変更する。
- 30fpsでも切替が二重発火しない。

ロックオンの品質は検索距離より、選択の予測可能性、維持の安定性、解除理由の明確さで決まります。
