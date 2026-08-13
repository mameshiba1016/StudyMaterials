# 08 ActorとComponentのライフサイクル

## 1. 「いつ呼ばれるか」が正しさを決める

同じ初期化コードでも、コンストラクタ、Construction、Component初期化、`BeginPlay`では利用できる情報が異なります。ライフサイクルを無視すると、エディタでは動くがパッケージで壊れる、Blueprint値が上書きされる、参照先がまだ存在しない、といった問題が起きます。

概略は次の通りです。

```text
C++コンストラクタ
    ↓
PostLoad（ロード経路）または PostActorCreated（Spawn経路）
    ↓
Construction / OnConstruction
    ↓
PreInitializeComponents
    ↓
ComponentのInitializeComponent
    ↓
PostInitializeComponents
    ↓
BeginPlay
    ↓
Tick・Timer・イベント
    ↓
EndPlay
    ↓
GCによる最終回収
```

厳密な経路は、レベルロード、PIE複製、通常Spawn、Deferred Spawnで異なります。

## 2. コンストラクタ

適するもの：

- `CreateDefaultSubobject`による標準Component構成。
- Tick可否などクラス既定設定。
- C++既定値。

避けるもの：

- 現在のプレイヤーやGameModeへの依存。
- World上のActor検索。
- 実行中だけ行う登録。
- 外部Actorへ副作用を起こす処理。

コンストラクタはCDO生成やエディタ上の再構築にも関係し、「ゲーム開始時に各Actorへ一度だけ」とは限りません。

## 3. OnConstruction

```cpp
void AAreaMarker::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);

    // 編集値から可視形状を再構成する例。
    MarkerMesh->SetWorldScale3D(FVector(Radius / 100.0f));
}
```

エディタでプロパティを変えた際にも繰り返し実行され得ます。外部ファイル保存、スコア加算、永続オブジェクト生成など、反復すると壊れる副作用を置かないでください。

## 4. PostInitializeComponentsとBeginPlay

```cpp
void AEnemyCharacter::PostInitializeComponents()
{
    Super::PostInitializeComponents();
    // 自身のComponent初期化完了後に必要な結線。
}

void AEnemyCharacter::BeginPlay()
{
    Super::BeginPlay();

    // ゲーム開始後のWorld参加、イベント購読、実行状態初期化。
    HealthComponent->OnDeath.AddDynamic(this, &AEnemyCharacter::HandleDeath);
}
```

`BeginPlay`の順序を異なるActor間で勝手に仮定しないでください。「管理ActorのBeginPlayが必ず先」といった暗黙依存を作らず、Subsystemから取得する、明示的に初期化する、準備完了イベントを使うなど契約化します。

## 5. Componentのライフサイクル

Componentでは主に次を区別します。

- `OnRegister`／`OnUnregister`：Worldへの登録と解除。エディタ操作でも複数回起こり得る。
- `InitializeComponent`：初期化対象Componentに対する初期化。
- `BeginPlay`：OwnerがPlayを開始した後。
- `EndPlay`：Play終了経路。
- `OnComponentDestroyed`：Component破棄時。

登録回数とゲーム開始回数を混同しないことが重要です。

## 6. Tick

```cpp
AActionActor::AActionActor()
{
    PrimaryActorTick.bCanEverTick = true;  // このクラスがTick可能か。
    PrimaryActorTick.bStartWithTickEnabled = false; // 初期状態では停止。
}

void AActionActor::StartTracking()
{
    SetActorTickEnabled(true); // 必要な期間だけ開始。
}

void AActionActor::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    UpdateTracking(DeltaSeconds);
}
```

- `bCanEverTick`：そもそもTickを許可するか。
- Tick enabled：現在実行するか。
- Tick interval：毎フレームでなく間隔更新するか。
- Tick group：物理の前後など、フレーム内の実行段階。
- Prerequisite：別Tickとの順序依存。

順序依存を増やすほど並列化と保守が難しくなります。データの読み書きタイミングを整理して最小化します。

## 7. EndPlayで後始末する

```cpp
void UTargetingComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (ObservedActor.IsValid())
    {
        ObservedActor->OnDestroyed.RemoveDynamic(this, &UTargetingComponent::HandleTargetDestroyed);
    }

    GetWorld()->GetTimerManager().ClearAllTimersForObject(this);
    ObservedActor.Reset();

    Super::EndPlay(EndPlayReason);
}
```

後始末対象：

- 自分が登録したDelegate。
- TimerとTicker。
- 入力Contextや外部Manager登録。
- 非同期要求のコールバック。
- 自分が所有する一時Actor／Component。

`EndPlayReason`にはDestroy、レベル遷移、PIE終了など複数理由があります。特定理由でのみ保存する場合は分岐しますが、安全な接続解除は通常どの経路でも必要です。

## 8. よくある初期化バグ

- コンストラクタで`GetWorld()`前提の検索を行う。
- `BeginPlay`でBlueprint設定値をC++既定値へ戻す。
- `OnConstruction`のたびにComponentを追加し続ける。
- `Super::BeginPlay()`を呼ばず親の契約を壊す。
- Tickで初期化完了を毎回ポーリングする。
- レベル終了後もTimerや非同期処理が古いActorを呼ぶ。
- PIEでだけ残る静的変数をゲーム状態として使う。

## 9. 初期化を段階として設計する

高速アクションキャラクターなら、次のように分けられます。

1. コンストラクタ：Capsule、Mesh、Camera、Combat Componentを構成。
2. データ設定：Blueprint／Data Assetで性能を指定。
3. Component初期化：内部キャッシュと必須依存を検証。
4. `BeginPlay`：イベント購読、現在値初期化。
5. Possess：入力またはAIの操作経路を接続。
6. 戦闘開始：敵一覧、Combat Director、ロックオン候補へ登録。

「オブジェクト生成完了」と「戦闘参加可能」を別状態にすると、ロード・交代・復活に対応しやすくなります。

## 参考

- [Actor Lifecycle](https://dev.epicgames.com/documentation/en-us/unreal-engine/unreal-engine-actor-lifecycle)
