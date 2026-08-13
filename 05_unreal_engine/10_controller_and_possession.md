# 10 Controller、Possess、操作主体の分離

## 1. Controllerは「意思」、Pawnは「身体」

`AController`は物理的な見た目を持たないActorで、PawnをPossessして操作します。

- `APlayerController`：人間の入力、ローカルPlayer、カメラ／HUDとの接点。
- `AAIController`：知覚、Behavior Tree、State Tree、Navigation等を使うAIの意思。
- `APawn`／`ACharacter`：世界に存在する身体、衝突、移動、Animation、身体能力。

この分離により、同じCharacterをPlayerとAIが操作する、Playerがキャラクターを交代する、死亡後にSpectatorへ移る、といった変更が可能になります。

## 2. PossessとUnPossess

```cpp
void APartyPlayerController::SwitchToCharacter(AActionCharacter* NextCharacter)
{
    if (!IsValid(NextCharacter) || GetPawn() == NextCharacter)
    {
        return;
    }

    APawn* PreviousPawn = GetPawn();

    // Authority上で操作主体を切り替える。ネットワーク対応では呼び出し場所を厳密にする。
    Possess(NextCharacter);

    // 旧Pawnを控え状態にする処理は、Party System等へ委譲する方が拡張しやすい。
    HandlePossessedCharacterChanged(PreviousPawn, NextCharacter);
}
```

PossessによりControllerのPawn参照とPawnのController参照が更新され、関連コールバックが発生します。ポインタを自分で代入して代用してはいけません。

```cpp
void APartyPlayerController::OnPossess(APawn* InPawn)
{
    Super::OnPossess(InPawn);
    // 新しいPawnへController側システムを接続する。
}

void APartyPlayerController::OnUnPossess()
{
    // 古いPawnに結び付けた購読を解除する。
    Super::OnUnPossess();
}
```

## 3. 入力から能力実行までの経路

```text
物理デバイス
  ↓
Enhanced Input
  ↓
PlayerControllerまたはPawnの入力ハンドラ
  ↓
「攻撃したい」というCommand
  ↓
Combat Componentが現在状態・コスト・キャンセル規則を検証
  ↓
Animation／Hit判定／VFXへ実行通知
```

入力ハンドラから直接「敵のHPを減らす」べきではありません。AIも同じ攻撃要求APIを使えるよう、入力解釈と能力実行を分けます。

```cpp
void APartyPlayerController::RequestLightAttack()
{
    if (AActionCharacter* Character = Cast<AActionCharacter>(GetPawn()))
    {
        Character->GetCombatComponent()->TryStartAction(TAG_Action_Attack_Light);
    }
}
```

## 4. Control Rotation

`ControlRotation`はControllerが意図する視線・照準方向です。Actor RotationやCamera Rotationと必ず一致するわけではありません。

```cpp
void APartyPlayerController::UpdateLockOnRotation(
    const FVector& CameraLocation,
    const FVector& TargetLocation,
    float DeltaSeconds)
{
    const FRotator Desired = (TargetLocation - CameraLocation).Rotation();
    const FRotator Smoothed = FMath::RInterpTo(
        GetControlRotation(), Desired, DeltaSeconds, LockOnRotationSpeed);

    SetControlRotation(Smoothed);
}
```

カメラ、Character、攻撃照準の全てをControl Rotationへ強制一致させると操作感が硬くなります。各消費者がPitch／Yawのどこを使うか、ロックオン状態でどう補間するかを設計します。

## 5. PlayerControllerに置くもの

置きやすいもの：

- ローカル入力の解釈。
- 現在Possess対象の管理。
- PlayerCameraManagerとの接続。
- プレイヤー固有HUDへの入口。
- Characterが変わっても継続する操作設定。

置くべきでない可能性が高いもの：

- Character固有の現在HPや硬直。
- 敵AIでも再利用する攻撃成立判定。
- World全体のゲームルール。
- 全プレイヤーへ共有する試合状態。

ネットワークではPlayerControllerの存在範囲が特殊です。全クライアントが全PlayerControllerを持つ前提にせず、共有状態はGameState／PlayerState等へ置きます。

## 6. AIControllerと共通Command

```cpp
UINTERFACE(BlueprintType)
class UCombatCommandReceiver : public UInterface
{
    GENERATED_BODY()
};

class ICombatCommandReceiver
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
    bool RequestCombatAction(FGameplayTag ActionTag);
};
```

PlayerControllerは入力をCommandへ変換し、AIControllerは判断結果を同じCommandへ変換します。実行可否はPawn側のCombat Systemが判断します。これにより、Playerだけ規則を無視する二重実装を防ぎます。

## 7. 交代処理をトランザクションとして考える

高速アクションの交代は`Possess`一行だけでは完成しません。

1. 交代可能条件を検証する。
2. 旧キャラクターの攻撃／無敵／入力状態をどう終了するか決める。
3. 出現位置と衝突可能位置を決定する。
4. 新キャラクターを有効化する。
5. Possessする。
6. Camera Target、HUD、ロックオンを再接続する。
7. 交代攻撃または支援行動を開始する。
8. 途中失敗なら整合した状態へ戻す。

途中で新Characterが無効、Spawn不可、死亡済みなどになっても、ControllerがPawnなしの壊れた状態に残らない設計が必要です。

## 8. よくある設計ミス

- Characterがキーボードの具体キーを直接知る。
- AIControllerがCharacter内部状態を書き換えて攻撃を強制する。
- Possess変更後も旧PawnのDelegateを購読し続ける。
- Controllerの`GetPawn()`が常に非nullと仮定する。
- CameraをCharacterへ固定し、交代時に視点が飛ぶ。
- ClientからAuthorityなしでPossessできると考える。

## 参考

- [Controllers in Unreal Engine](https://dev.epicgames.com/documentation/unreal-engine/controllers-in-unreal-engine)
- [AController API](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/AController)
- [Gameplay Framework](https://dev.epicgames.com/documentation/unreal-engine/gameplay-framework-in-unreal-engine)
