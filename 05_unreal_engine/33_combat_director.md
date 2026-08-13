# 33 Combat Directorと複数敵制御

## 1. 個体AIだけでは集団戦を制御できない

各敵が独立して「攻撃可能なら攻撃」すると、全敵が同時攻撃し、重なり、画面外から理不尽に攻撃します。Combat Directorは集団全体の攻撃権、位置Slot、圧力を調整します。

```text
各AIの意図要求
  ↓
Combat Director
  ├─ Attack Token
  ├─ Position Slot
  ├─ Cooldown / Pressure Budget
  └─ Fairness Rule
  ↓ 許可／待機／別行動
各AIのBehavior Tree
```

## 2. World Subsystemとしての候補

```cpp
UCLASS()
class MYGAME_API UCombatDirectorSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    bool RequestAttackToken(const FAttackTokenRequest& Request, FAttackToken& OutToken);
    void ReleaseAttackToken(const FAttackToken& Token);

private:
    TArray<TWeakObjectPtr<AActor>> RegisteredEnemies;
    TMap<int32, FActiveAttackToken> ActiveTokens;
    int32 NextTokenId = 1;
};
```

現在Worldの戦闘へ閉じるためWorld Subsystemが候補ですが、戦闘Area単位のActor／Componentとして複数Directorを置く設計もあります。

## 3. Attack Token

```cpp
struct FAttackTokenRequest
{
    TWeakObjectPtr<AActor> Requester;
    TWeakObjectPtr<AActor> Target;
    EEnemyAttackRole Role = EEnemyAttackRole::Melee;
    float Threat = 0.0f;
    float EstimatedDuration = 0.0f;
};
```

Token数を近接2、遠距離1のようにRole別管理できます。Token取得後にAction開始失敗したら即返却し、終了・中断・死亡・EndPlayでも返却します。期限付きLeaseにすると返却漏れへ強くなります。

## 4. Token付与Score

- Playerからの距離。
- 最後に攻撃してからの時間。
- 現在画面内か。
- 攻撃予告が見えるか。
- 敵Roleと現在構成。
- 同じ敵ばかりにならないFairness。
- Difficultyの同時攻撃上限。

一番近い敵だけへ与え続けず、待機時間Bonusを加えます。

## 5. Position Slot

Player周囲を角度Sector／Ringとして予約します。

```text
        Ranged Slot
   [2]      Player      [0]
        [1]       [7]
```

SlotはWorld固定でなくTargetの移動に追従し、Nav／Collisionで有効性を再確認します。大型敵は複数Slot幅を占有します。

## 6. 攻撃予告と公平性

- 画面外攻撃は強い音・Indicatorを出すか制限。
- Player被弾直後は追撃Budgetを抑える。
- 強攻撃同士を同時開始させない。
- Parry可能攻撃の間隔を保証。
- Cameraで把握不能な背後Attackへ制約。

難易度は敵HPだけでなく、Token数、予告時間、攻撃頻度、連携精度で調整できます。

## 7. Intentと実行を分ける

AIは`Request Heavy Attack`をDirectorへ送り、許可後もCombat Componentが最終実行可否を検証します。

```text
Behavior Tree選択
  ↓ Token Request
Director許可
  ↓
Combat System TryStartAction
  ├─ 成功 → TokenをActionへ紐付け
  └─ 失敗 → Token返却
```

## 8. Bossと雑魚

Bossは自身のPhase State Machineが主導し、Directorは雑魚の攻撃をBoss演出へ合わせて抑制できます。

- Boss大技中は雑魚Tokenを停止。
- Boss Stagger中は雑魚が距離を取る。
- Add召喚直後は攻撃猶予。
- Phase移行Camera中は全攻撃を凍結。

## 9. Character交代との関係

Player Characterが交代してもPlayer戦闘主体IDを維持し、敵のTarget参照を新Pawnへ更新します。各敵が旧Pawn破棄Eventへ一斉依存するだけでなく、Party／DirectorからTarget Changed Eventを通知します。

## 10. 性能

- 全敵ペアの距離計算を毎Frameしない。
- 登録／解除Eventで一覧管理。
- Token再評価を固定間隔で分散。
- 画面外・遠距離AIを低詳細化。
- Slot Query結果を短時間Cache。

## 11. デバッグ表示

敵頭上へRole、Intent、Token ID、待機時間を表示し、Player周囲にSlotと予約者を描画します。

```text
Enemy_A: MeleeToken #14, Attacking
Enemy_B: Waiting 1.2s, score 0.73
Enemy_C: Reposition Slot 3
```

## 12. テスト

- Token保有敵が死亡／Destroy。
- Action開始直前にStun。
- Player交代・死亡・復活。
- 狭所でSlotが無効。
- 大量敵Spawn／Despawn。
- Boss Phase変更と同Frameの攻撃要求。
- 低fpsでもToken期限が正しい。
- Difficulty変更時に既存Tokenが整合する。
