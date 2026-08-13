# 22 Animation Blueprintと更新設計

## 1. Animation Blueprintの役割

Animation Blueprint（AnimBP）はSkeletal Meshへ出力する最終Poseを毎Frame評価します。主なGraphは次の2つです。

- Event Graph：速度、空中状態、向きなど、Pose計算へ渡す値を更新。
- Anim Graph：State Machine、Blend、IK、Slot等を組み合わせて最終Poseを生成。

```text
Gameplay State / CharacterMovement
  ↓ 読み取り
AnimInstanceの表示用変数
  ↓
Anim GraphでPose評価
  ↓
Skeletal Meshへ最終Pose
```

AnimBPは原則としてGameplay状態を表示する消費者です。AnimBPの遷移に入ったからCombat Stateを攻撃中にする、という逆向き依存を増やすと、Dedicated Server、Animation省略、Montage中断で状態が壊れます。

## 2. C++ AnimInstance

```cpp
UCLASS()
class MYGAME_API UActionAnimInstance : public UAnimInstance
{
    GENERATED_BODY()

public:
    virtual void NativeInitializeAnimation() override;
    virtual void NativeUpdateAnimation(float DeltaSeconds) override;

protected:
    UPROPERTY(BlueprintReadOnly, Category = "Locomotion")
    float GroundSpeed = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Locomotion")
    bool bIsFalling = false;

private:
    TWeakObjectPtr<AActionCharacter> OwnerCharacter;
};
```

```cpp
void UActionAnimInstance::NativeInitializeAnimation()
{
    Super::NativeInitializeAnimation();
    OwnerCharacter = Cast<AActionCharacter>(TryGetPawnOwner());
}

void UActionAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
    Super::NativeUpdateAnimation(DeltaSeconds);

    AActionCharacter* Character = OwnerCharacter.Get();
    if (!Character)
    {
        Character = Cast<AActionCharacter>(TryGetPawnOwner());
        OwnerCharacter = Character;
    }

    if (!Character)
    {
        return;
    }

    const FVector Velocity = Character->GetVelocity();
    GroundSpeed = Velocity.Size2D(); // 垂直速度を除いた地上速度。
    bIsFalling = Character->GetCharacterMovement()->IsFalling();
}
```

Editor PreviewではOwner Pawnが存在しない場合があるため、nullを正常系として扱います。

## 3. Thread Safety

Animation評価は並列化され得ます。Anim GraphのThread Safe Updateで任意のUObject状態を書き換える、World Queryを行う、Gameplay Eventを発火する設計は避けます。

安全な基本方針：

1. Game Thread側で必要値を取得。
2. AnimInstanceの単純な値へSnapshotする。
3. Animation ThreadはSnapshotからPoseを計算。
4. Gameplayの副作用はCharacter／Component側で実行。

Property Access等、Engineが提供するThread Safeな読み取り経路も利用しますが、利用中バージョンの制約を確認します。

## 4. 速度と方向

```cpp
const FVector LocalVelocity = Character->GetActorTransform()
    .InverseTransformVectorNoScale(Character->GetVelocity());

ForwardSpeed = LocalVelocity.X;
RightSpeed = LocalVelocity.Y;
```

World Velocityをそのまま使うと、Characterの向きが変わった時に前後左右が分かりません。Actor Localへ変換してStrafe用Blend Spaceへ渡します。

## 5. Gameplay Tag／Stateの橋渡し

AnimBPへCombat Component全体を渡すのではなく、表示に必要な状態を小さくまとめます。

```cpp
UENUM(BlueprintType)
enum class ECharacterAnimState : uint8
{
    Locomotion,
    Attacking,
    Dodging,
    Stunned,
    Dead
};
```

Gameplayの詳細な状態機械とAnimationの見せ方は粒度が異なります。10種類の攻撃状態をAnimBP側では`Attacking`＋Montage情報として扱うなど、翻訳層を作ります。

## 6. Update頻度と可視性

画面外のSkeletal Meshまで全骨を毎Frame評価すると負荷が増えます。Visibility Based Anim Tick Option、Update Rate Optimization、LOD、Bone削減を検討します。

ただしAnimation NotifyをGameplay判定の唯一の発生源にすると、Animation更新が省略された時に問題になります。重要な状態はGameplay側で保証し、Notifyは同期の合図として扱います。

## 7. Linked Anim Layer／Linked Anim Instance

キャラクターごとの武器姿勢や上半身Layerを差し替える場合、巨大AnimBPを複製せずLinked Anim Layer等で機能を分割できます。

```text
共通Locomotion Layer
  + Character固有Combat Layer
  + Weapon固有UpperBody Layer
  + Additive Aim / Hit Reaction
  → Final Pose
```

Layer境界では必要なPoseとParameterを明確にし、循環参照や同じ計算の重複を避けます。

## 8. デバッグ

- AnimBP Debug Filterで実行中Characterを選ぶ。
- State Machineの現在Stateと遷移を確認。
- Pose Watchで中間Poseを見る。
- `GroundSpeed`、Local Velocity、Movement Modeを同時表示。
- Montage、Slot Weight、Section名を記録。
- Editor Previewだけでなく実ゲームの低fps・LODでも確認。

## 参考

- [Animation Blueprints](https://dev.epicgames.com/documentation/en-us/unreal-engine/animation-blueprints-in-unreal-engine)
