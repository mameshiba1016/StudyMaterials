# 40 UMG UIとイベント駆動HUD

## 1. UIを3層に分ける

```text
Gameplay Model（Attribute、Party、Combat State）
  ↓ Event／View Model
UI表示用Data
  ↓
UMG Widget（Text、Bar、Animation）
```

Widgetが毎FrameWorld中のActorを検索し、HPを計算する構造を避けます。

## 2. Widget生成

```cpp
void APartyPlayerController::CreateCombatHUD()
{
    if (!IsLocalController() || !CombatHUDClass || CombatHUD)
    {
        return;
    }

    CombatHUD = CreateWidget<UCombatHUDWidget>(this, CombatHUDClass);
    if (CombatHUD)
    {
        CombatHUD->AddToViewport();
    }
}
```

UIはLocal Playerに属します。Dedicated Serverで作りません。

## 3. NativeConstructとNativeDestruct

```cpp
void UCombatHUDWidget::NativeConstruct()
{
    Super::NativeConstruct();
    BindToViewModel();
}

void UCombatHUDWidget::NativeDestruct()
{
    UnbindFromViewModel();
    Super::NativeDestruct();
}
```

Widgetは表示・非表示や再追加でConstructが複数回起こり得ます。Delegateを二重登録せず、解除を対にします。

## 4. Event駆動更新

```cpp
void UCombatHUDWidget::HandleHealthChanged(float Current, float Maximum)
{
    const float Ratio = Maximum > 0.0f ? Current / Maximum : 0.0f;
    HealthBar->SetPercent(FMath::Clamp(Ratio, 0.0f, 1.0f));
}
```

Attribute変更、Target変更、Party交代、Cooldown開始時に通知します。UMG Property Bindingは便利ですが頻繁に評価され得るため、大規模HUDでは明示更新を検討します。

## 5. Character交代

HUDは旧Characterへ直接永続Bindせず、Player／Party View Modelの`ActiveCharacterChanged`を受け、旧Delegate解除→新Delegate登録を行います。

```text
Active Character Changed
  ↓
旧Character Attribute Delegate解除
  ↓
新Character Attribute Delegate登録
  ↓
全表示をSnapshotで即時更新
```

## 6. World Space UI

敵Health BarはWidget Component等で表示できますが、全敵へ常時Widget Tickを持たせると高負荷です。

- 近距離・被弾中だけ表示。
- Screen Spaceへまとめる方式も検討。
- Object Pooling。
- Occlusionと画面外判定。
- LODで名前、Buff Iconを省略。

## 7. Input ModeとFocus

Menu表示時はGame／UI Input Mode、Mouse Cursor、Focus、Enhanced Input Mapping Contextを一貫して切り替えます。閉じた後にGameplay Contextを復元し忘れないでください。

## 8. DPIとSafe Zone

解像度、Aspect Ratio、DPI Scale、Console Safe Zone、Split Screenで確認します。Pixel座標固定ではなくAnchor、Size Box、Scale Boxを目的に応じて使います。

## 9. LocalizationとAccessibility

- 表示文章は`FText`。
- 文字列連結よりFormat Text。
- Font、行幅、言語別の長さを確認。
- Colorだけに依存しない状態表示。
- Camera Shake、Flash、字幕、入力Hold／Toggle設定。

## 10. UI性能

- Invalidationで変化しないTreeを再計算しない。
- Retainer Boxは描画頻度を下げられるがMemory／遅延を計測。
- Widget AnimationとMaterial数を抑える。
- HiddenとCollapsedのLayout Cost差を理解。
- Unreal Insights／Slate InsightsでTick、Paint、Invalidationを確認。

## 11. テスト

- Character交代を連打。
- Target死亡と同FrameにUI更新。
- Widget再生成でDelegate二重登録がない。
- 16:9、21:9、低解像度。
- Gamepadだけで全Focus移動。
- Language切替で文字が欠けない。
