# 11 GameMode、GameState、PlayerState

## 1. 3種類の責任を分ける

```text
GameMode   = ゲームの規則と進行判断。マルチプレイではサーバーだけ。
GameState  = 全参加者が知るゲーム全体の現在状態。サーバーから複製。
PlayerState = 各参加者に属し、他者にも共有される現在状態。
```

例として、制限時間を決めて勝敗を判定するのはGameMode、現在の残り時間やチーム得点はGameState、各プレイヤーの名前や得点はPlayerStateです。GameModeの変数をClient UIが直接読む構造にすると、ネットワーク時に実体がなく破綻します。

## 2. AGameModeBaseとAGameMode

- `AGameModeBase`：簡潔な基底。独自のゲーム進行を組み立てやすい。
- `AGameMode`：Match Stateの状態機械を追加した派生型。対応する`AGameState`と組み合わせる。

```cpp
UCLASS()
class MYGAME_API ACombatGameMode : public AGameModeBase
{
    GENERATED_BODY()

public:
    ACombatGameMode();

    virtual void InitGame(
        const FString& MapName,
        const FString& Options,
        FString& ErrorMessage) override;

    virtual void RestartPlayer(AController* NewPlayer) override;
};
```

```cpp
ACombatGameMode::ACombatGameMode()
{
    // 使用するFrameworkクラスを既定値として指定する。
    GameStateClass = ACombatGameState::StaticClass();
    PlayerStateClass = ACombatPlayerState::StaticClass();
    DefaultPawnClass = AActionCharacter::StaticClass();
    PlayerControllerClass = APartyPlayerController::StaticClass();
}

void ACombatGameMode::InitGame(
    const FString& MapName,
    const FString& Options,
    FString& ErrorMessage)
{
    // InitGameはActorの通常初期化より早い段階でゲーム規則を準備する。
    Super::InitGame(MapName, Options, ErrorMessage);
}
```

Blueprint派生でクラスを差し替える場合、C++へアセットのハードパスを大量に直書きするより、GameMode Blueprintまたは設定用Data Assetで構成する方が調整しやすい場合があります。

## 3. Player生成の流れ

代表的な流れは次の通りです。

```text
PreLogin → Login → PostLogin → HandleStartingNewPlayer
                                  ↓
                              RestartPlayer
                                  ↓
                         PawnをSpawnしてPossess
```

`PostLogin`は接続済みPlayerControllerへ複製関数を安全に呼び始められる地点です。Pawn生成位置を変えるならPlayerStart選択や`RestartPlayerAtTransform`、Pawnクラスを変えるなら対応するGameMode関数を責任に沿って上書きします。

## 4. GameStateは共有される現在状態

```cpp
UCLASS()
class MYGAME_API ACombatGameState : public AGameStateBase
{
    GENERATED_BODY()

public:
    UPROPERTY(ReplicatedUsing = OnRep_CombatPhase, BlueprintReadOnly)
    ECombatPhase CombatPhase = ECombatPhase::Preparing;

protected:
    UFUNCTION()
    void OnRep_CombatPhase();
};
```

GameStateには、戦闘Phase、共有目標、全体スコアなど「全Clientが表示・判断材料として知る状態」を置きます。GameModeがAuthorityとして変更し、GameStateが結果を配布する関係です。

```cpp
void ACombatGameMode::StartCombat()
{
    if (ACombatGameState* State = GetGameState<ACombatGameState>())
    {
        State->CombatPhase = ECombatPhase::InCombat;
        // 実際には複製設定と更新通知も正しく実装する。
    }
}
```

## 5. PlayerStateとPawn状態を区別する

PlayerStateはPawnが破壊・再Spawnされても参加者に属して維持すべき情報に向きます。

```cpp
UCLASS()
class MYGAME_API ACombatPlayerState : public APlayerState
{
    GENERATED_BODY()

public:
    UPROPERTY(Replicated, BlueprintReadOnly)
    int32 TeamId = 0;

    UPROPERTY(Replicated, BlueprintReadOnly)
    int32 Score = 0;
};
```

一方、現在のAnimation、床状態、攻撃硬直、当たり判定はPawn／Component側です。体力や編成はゲーム仕様次第で、Pawn固有かPlayer固有かを決めます。「死亡して身体を交換しても残るか」が一つの判断基準です。

## 6. シングルプレイでも責任分離は有効

シングルプレイだから全部Characterへ置いてよいわけではありません。

- GameMode：ステージ開始、勝敗、敵Wave規則。
- GameState：現在Phase、残敵数、ステージ経過時間。
- PlayerStateまたは専用Party System：プレイヤーのラン中状態。
- Character：現在操作中の身体と戦闘状態。

この分離はテスト、リトライ、キャラクター交代、将来の協力プレイ対応にも効きます。

## 7. よくある誤り

- GameModeをClientから取得してUI表示する。
- GameStateへ全ゲームロジックを集約する。
- PlayerControllerとPlayerStateを同一視する。
- レベルをまたぐ永続設定をGameModeへ保存する。
- Character死亡と同時にプレイヤーの全情報を失う。
- 共有状態を各Clientが独自計算し、結果がずれる。

## 参考

- [Game Mode and Game State](https://dev.epicgames.com/documentation/en-us/unreal-engine/game-mode-and-game-state-in-unreal-engine)
- [Gameplay Framework](https://dev.epicgames.com/documentation/unreal-engine/gameplay-framework-in-unreal-engine)
