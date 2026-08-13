# 29 Hit Stop、演出通知、キャラクター交代

## 1. Hit Stopの目的

Hit Stopは命中瞬間の動きを短時間止め、重さと認識時間を与える演出です。単にゲーム全体を長くPauseする仕組みではありません。

影響候補：

- 攻撃者Animation。
- 被攻撃者Animation。
- Character Movement。
- Camera Shake。
- Particle／Niagara。
- 入力Buffer。
- AI思考。
- World全体。

何を止め、何を動かすかを明示します。

## 2. Global Time DilationとCustom Time Dilation

- Global：World全体へ影響しやすい。
- Actor Custom：特定ActorのTick速度を変える。
- Montage Play Rate／Animation Pause：Animationに限定。

Global Time Dilationを連続Hitごとに上書きすると、Timerや入力猶予、物理へ広く影響します。局所Hit Stopなら攻撃者・被攻撃者のAnimation／Movementを専用Managerで一時制御する方法も検討します。

## 3. Hit Stop要求を集約する

```cpp
USTRUCT()
struct FHitStopRequest
{
    GENERATED_BODY()
    float DurationSeconds = 0.05f;
    float TimeScale = 0.05f;
    int32 Priority = 0;
    int32 SourceSequenceId = 0;
};
```

```text
複数Hit Stop要求
  ↓ HitStop Manager
Priority・残り時間・重複規則を解決
  ↓
対象へ適用
  ↓
必ず元の状態へ復元
```

各攻撃が直接Time Dilationを0と1へ切り替えると、重なった要求の片方が早く解除します。

## 4. 実時間とGame時間

Time Dilationの影響下ではGame Time Timerも遅くなるため、「0.05秒止めた後に戻すTimer」が同じTime Dilationで遅延する問題があります。Hit Stop解除には影響を受けない実時間、Ticker、専用更新経路などを検討します。

入力Bufferの期限をHit Stop中も消費するか停止するかも操作感に直結します。

## 5. PresentationをEvent化する

```cpp
USTRUCT()
struct FCombatPresentationEvent
{
    GENERATED_BODY()
    FVector Location = FVector::ZeroVector;
    FVector Normal = FVector::UpVector;
    FGameplayTag CueTag;
    float Intensity = 1.0f;
};
```

Damage Systemは「重攻撃命中」というCueを通知し、Presentation SystemがVFX、SFX、Shake、Rumbleを選びます。GameplayクラスがNiagara Assetや音声を大量に直接参照する構造を避けられます。

## 6. キャラクター交代の責任

```text
Party System      編成、交代順、Cooldown、控え状態
PlayerController  Possess、Camera、入力の接続
Character         身体、HP、Combat、Animation
Combat Director   敵Targetや戦闘参加状態
```

Character自身が「次の仲間をSpawnしてPossessする」全責任を持つと、死亡・Destroy時に処理が壊れます。Party RuntimeをCharacterより長寿命のOwnerへ置きます。

## 7. 交代Transaction

```cpp
bool UPartyComponent::TrySwitchCharacter(int32 DesiredIndex)
{
    if (!CanSwitchTo(DesiredIndex))
    {
        return false;
    }

    FSwitchTransaction Transaction = BuildSwitchTransaction(DesiredIndex);
    if (!PrepareOutgoingCharacter(Transaction))
    {
        return false;
    }

    if (!PrepareIncomingCharacter(Transaction))
    {
        RollbackOutgoingCharacter(Transaction);
        return false;
    }

    CommitPossession(Transaction);
    CommitCameraAndHUD(Transaction);
    return true;
}
```

途中失敗で操作対象が消えないよう、PrepareとCommitを分けます。

## 8. 出現位置

候補位置は旧Character周辺、Target相対、Camera外などから求めます。

1. 希望Transformを計算。
2. Capsule Sweepで空きを確認。
3. 床をTrace。
4. Nav／移動可能面を確認。
5. 見つからなければ安全なFallback。

敵内部や壁内へSpawnしてからTeleportで直すのではなく、Commit前に検証します。

## 9. 退場Character

選択肢：

- 非表示・Collision無効で待機。
- World外のParty管理領域へ移動。
- Actorを破棄しRuntime Dataだけ保存。
- AIへ引き渡し短時間支援後に退場。

毎回Spawn／DestroyするかInstanceを維持するかは、状態量、ロード費用、Animation／VFX初期化、Memoryから選びます。

## 10. 支援攻撃

交代と同時の支援攻撃は次を一つのAction Sequenceとして管理します。

```text
交代受付
  ↓ 安全位置決定
Incoming Character出現
  ↓ Possessまたは一時AI Command
Support Montage / Motion Warp
  ↓ Hit Window
Camera・HUD・Lock Target引継ぎ
  ↓
Outgoing Character退場
```

どの時点から新CharacterがDamageを受けるか、旧Characterの無敵が終わるかをWindowとして定義します。

## 11. 状態の引継ぎ

引き継ぐ候補：Current Lock Target、Camera Mode、入力Bufferの一部、Player UI。

引き継がない候補：旧CharacterのMontage、Hit済み集合、個体固有Cooldown、Movement Mode。

入力Bufferを丸ごと渡すと旧Character向け攻撃が新Characterで発火します。交代Command以外を破棄する、Action Tagを再解決するなどの規則が必要です。

## 12. テスト

- Hit Stop中に交代入力。
- 交代先が死亡／ロード未完了。
- 壁際・空中・狭所で交代。
- 同Frameに被弾と交代。
- Support Attack中にTarget死亡。
- 連続交代要求とCooldown。
- Camera Blend中に再交代。
- Level終了時にHit Stopが残らない。
