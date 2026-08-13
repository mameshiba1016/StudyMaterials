# 12 GameInstanceとSubsystem

## 1. 寿命から配置先を選ぶ

```text
Engine            アプリケーション全体
└─ GameInstance   ゲーム起動から終了まで。通常のレベル遷移を越える
   ├─ LocalPlayer ローカル参加者ごと
   └─ World       現在のMap／Worldごと
      └─ Actor    SpawnからEndPlayまで
```

GameInstanceは通常のMapロードを越えて残りますが、ネットワークで自動複製されません。サーバーと各Clientに別々の実体があるため、GameInstance変数へ書けば全員へ共有される、という理解は誤りです。

## 2. GameInstanceの用途

適する例：

- Save／Load処理の入口。
- メニューからゲームへ渡す選択内容。
- レベルを越えるセッション情報。
- GameInstance Subsystemの所有母体。

避けたい例：

- 現在Mapの敵Actorを強参照し続ける。
- 全機能を巨大GameInstanceへ詰め込む。
- 複製される戦闘状態として使う。

```cpp
UCLASS()
class MYGAME_API UMyGameInstance : public UGameInstance
{
    GENERATED_BODY()

public:
    virtual void Init() override;
    virtual void Shutdown() override;
};

void UMyGameInstance::Init()
{
    Super::Init();
    // アプリ内ゲームセッション全体に必要な初期化。
}
```

## 3. Subsystemとは

Subsystemはエンジンが所定のOwnerと同じ寿命で自動生成・破棄する機能単位です。EngineクラスやGameInstanceを機能ごとに巨大化させず、明確なAPIへ分割できます。

| Subsystem | 寿命・個数の基準 | 用途例 |
|---|---|---|
| `UEngineSubsystem` | Engine | アプリ全体のサービス |
| `UGameInstanceSubsystem` | GameInstance | Save、セッション、永続管理 |
| `UWorldSubsystem` | World | 現在Worldの戦闘Director、登録管理 |
| `ULocalPlayerSubsystem` | LocalPlayer | 入力設定、ローカルPlayer固有機能 |
| `UEditorSubsystem` | Editor | Editor専用ツール |

## 4. GameInstance Subsystemの例

```cpp
UCLASS()
class MYGAME_API USaveManagerSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    void RequestSave(int32 SlotIndex);
};
```

```cpp
void USaveManagerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    // Delegate登録や保存システムの準備。
}

void USaveManagerSubsystem::Deinitialize()
{
    // 非同期処理、Delegate、外部資源を安全に終了する。
    Super::Deinitialize();
}
```

取得側：

```cpp
if (UGameInstance* GI = GetGameInstance())
{
    if (USaveManagerSubsystem* SaveManager = GI->GetSubsystem<USaveManagerSubsystem>())
    {
        SaveManager->RequestSave(0);
    }
}
```

## 5. World Subsystemの例

敵の攻撃人数制御や戦闘参加者登録は現在Worldに閉じるため、World Subsystemが候補になります。

```cpp
UCLASS()
class MYGAME_API UCombatDirectorSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    void RegisterEnemy(AActor* Enemy);
    void UnregisterEnemy(AActor* Enemy);

private:
    // 敵を生存させる所有者にはならず、破棄を許す弱参照を使う。
    TArray<TWeakObjectPtr<AActor>> RegisteredEnemies;
};
```

GameInstance SubsystemへActor参照を置くとMap遷移を越えて古いWorldを参照しやすいため、World固有システムはWorldの寿命へ合わせます。

## 6. LocalPlayer Subsystem

LocalPlayer Subsystemは画面分割を含むローカルPlayerごとに存在します。Enhanced InputのMapping Contextも`UEnhancedInputLocalPlayerSubsystem`へ適用します。

「PlayerControllerが変わっても同じローカル利用者に属する設定」はLocalPlayer側、「Possess中Pawnに属する身体状態」はPawn側です。

## 7. Subsystemを万能Singletonにしない

Subsystem化が適さない例：

- 1体のCharacterだけが所有する戦闘状態。
- Transformを持つべき世界上の存在。
- Data Assetで十分な不変設定。
- 関数引数で渡せる短命な計算処理。

グローバルに取得できることは便利ですが、依存関係を隠します。利用クラスがどのSubsystemを必要とするかを初期化時に検証し、Subsystem同士の循環依存を避けます。

## 8. ZZZ系アクションでの配置例

- GameInstance Subsystem：Save、ユーザー設定、アセット事前ロード方針。
- LocalPlayer Subsystem：キー設定、入力Context、アクセシビリティ設定。
- World Subsystem：Combat Director、敵参加者、戦闘エリア管理。
- Party Component／専用Runtime Object：現在編成、交代順、支援状態。
- Character Component：体力、攻撃状態、ターゲット状態。

システム名ではなく「誰と同時に生まれ、いつ死ぬか」でOwnerを選びます。

## 参考

- [Programming Subsystems](https://dev.epicgames.com/documentation/unreal-engine/programming-subsystems-in-unreal-engine)
- [Gameplay Framework](https://dev.epicgames.com/documentation/unreal-engine/gameplay-framework-in-unreal-engine)
