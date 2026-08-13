# 20 アクションカメラとPlayerCameraManager

## 1. Cameraの責任Chain

最終的な画面は単純に`UCameraComponent`のTransformだけでは決まりません。PlayerController、View Target、Actorの`CalcCamera`、Camera Component、PlayerCameraManagerが協力して最終`FMinimalViewInfo`を作ります。

```text
View Target（通常はPossess Pawn）
  ↓ CalcCamera / Camera Component
基本POV（Location、Rotation、FOV）
  ↓ PlayerCameraManager
Blend・Camera Shake・Post Process・制限
  ↓ LocalPlayer / Renderer
最終画面
```

`APlayerCameraManager`はPlayerごとの「仮想の目」であり、View Target間のBlendや最終視点効果を管理します。

## 2. Third Personの基本構成

```cpp
AActionCharacter::AActionCharacter()
{
    CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
    CameraBoom->SetupAttachment(GetRootComponent());
    CameraBoom->TargetArmLength = 350.0f;
    CameraBoom->bUsePawnControlRotation = true;
    CameraBoom->bDoCollisionTest = true;

    FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
    FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
    FollowCamera->bUsePawnControlRotation = false;
}
```

Spring ArmはTargetから一定距離へCameraを置き、障害物時にArmを縮められます。Camera Lagは便利ですが、入力遅延に感じられる場合があるため、位置Lagと回転Lagを目的別に調整します。

## 3. Camera Rotation入力

```cpp
void APartyPlayerController::HandleLook(const FInputActionValue& Value)
{
    const FVector2D Look = Value.Get<FVector2D>();
    AddYawInput(Look.X);
    AddPitchInput(Look.Y);
}
```

Deviceの感度、反転、Dead Zone、Frame依存の有無を整理します。Mouse DeltaとStickの値は性質が異なるため、同じ係数だけで済ませずDevice別Curveを検討します。

## 4. Camera Rigの状態化

高速アクションでは状況別にCamera設定が変わります。

```cpp
USTRUCT(BlueprintType)
struct FActionCameraMode
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere)
    float ArmLength = 350.0f;

    UPROPERTY(EditAnywhere)
    FVector SocketOffset = FVector(0.0f, 50.0f, 60.0f);

    UPROPERTY(EditAnywhere)
    float FieldOfView = 80.0f;

    UPROPERTY(EditAnywhere)
    float BlendSpeed = 8.0f;
};
```

探索、通常戦闘、Lock On、必殺技、狭所といったModeをData化し、現在値を目標へBlendします。各攻撃がCameraの数値を直接変更すると、終了時に元へ戻せない問題が起きます。

## 5. View Targetの切替

```cpp
SetViewTargetWithBlend(
    CinematicCameraActor,
    0.35f,
    EViewTargetBlendFunction::VTBlend_Cubic);
```

Character交代でView Targetも切り替える場合、旧Camera位置から新CharacterのCameraへBlendします。ただしBlend中に新Characterが移動するため、開始TransformのSnapshotと追従目標をどう混ぜるか決めます。

## 6. Camera Collision

Spring ArmのCollisionだけでは、狭所で急激なZoom、Characterへの埋まり、壁越しTarget表示が起きることがあります。

- Camera専用Trace Channelを定義。
- CharacterやTargetはCamera TraceをIgnoreする。
- Arm縮小と復帰で異なる速度を使う。
- Camera位置から注視点への遮蔽も確認。
- 壁の薄さや角での振動を複数Sample／安定化で抑える。

## 7. Camera ShakeとHit Stop

Camera Shakeは攻撃の強さを伝えますが、毎Hitで無制限に加算すると画面が読めなくなります。

```cpp
if (APlayerCameraManager* CameraManager = PlayerCameraManager)
{
    CameraManager->StartCameraShake(HeavyHitShakeClass, ShakeScale);
}
```

距離、攻撃強度、Player自身の被弾か、連続Hit頻度、アクセシビリティ設定でScaleを調整します。Hit Stop中もCameraだけ動かすか止めるかは演出仕様です。

## 8. Character RotationとCameraを分離する

非Lock On時：Camera Yaw基準で移動し、Characterは移動方向へ向く。

Lock On時：CameraはPlayerとTargetを構図に収め、CharacterはTargetへ向く。

攻撃中：攻撃側のFacing補正を行いつつ、Camera入力は完全には奪わない。

これらを全部`ControlRotation`一つへ強制すると、Cameraと身体が互いを引っ張ります。Camera Desired Rotation、Control Rotation、Character Facingを別値として最後に調停します。

## 9. Camera品質チェック

- 敵がPlayerの真上・真下・背後にいる。
- 壁際、柱の周囲、狭い廊下。
- 大型Bossと小型敵が同時にいる。
- Character交代を連続する。
- 画面端のTargetへ切り替える。
- 低Frame Rateでも補間が同じ時間で完了する。
- Camera Shake、Motion Blur、FOV変化で酔わない設定を用意する。

## 参考

- [Cameras in Unreal Engine](https://dev.epicgames.com/documentation/unreal-engine/cameras-in-unreal-engine)
- [APlayerCameraManager API](https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/Engine/APlayerCameraManager)
