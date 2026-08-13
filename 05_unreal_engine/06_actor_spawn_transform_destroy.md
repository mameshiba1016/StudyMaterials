# 06 Actorの役割、生成、座標、破棄

## 1. Actorとは何か

`AActor`は、レベルへ配置または実行中にSpawnできるゲーム世界上の存在です。敵、弾、宝箱、カメラ、開始地点、ルール管理Actorなどが該当します。

重要なのは、Actor自身がTransformを直接保持するのではなく、存在する場合は`RootComponent`のTransformがActorの位置・回転・拡縮として扱われる点です。

```cpp
UCLASS()
class MYGAME_API ATrainingTarget : public AActor
{
    GENERATED_BODY()

public:
    ATrainingTarget();

private:
    // TObjectPtrによりUObject参照であることを明示する。
    // VisibleAnywhereなので構成は見えるが、参照そのものを自由に差し替えさせない。
    UPROPERTY(VisibleAnywhere, Category = "Components")
    TObjectPtr<USceneComponent> SceneRoot;

    UPROPERTY(VisibleAnywhere, Category = "Components")
    TObjectPtr<UStaticMeshComponent> Mesh;
};
```

```cpp
ATrainingTarget::ATrainingTarget()
{
    // 毎フレーム処理が不要なら明示的に無効化する。
    PrimaryActorTick.bCanEverTick = false;

    // コンストラクタでは既定サブオブジェクトを作る。
    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    SetRootComponent(SceneRoot);

    Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
    Mesh->SetupAttachment(SceneRoot);
}
```

## 2. レベル配置とSpawnの違い

- **レベル配置**：エディタで配置し、マップと一緒に保存する。固定ギミックや開始時から存在する物に向く。
- **Spawn**：実行中にクラスとTransformを指定して生成する。敵、弾、エフェクトなどに向く。

どちらも最終的にActorの初期化経路を通りますが、ロード済みActorには`PostLoad`、Spawn Actorには`PostActorCreated`など、経路固有のコールバックがあります。

## 3. SpawnActorの基本

```cpp
AProjectile* AMyCharacter::SpawnProjectile(
    TSubclassOf<AProjectile> ProjectileClass,
    const FTransform& SpawnTransform)
{
    // WorldがなければActorをゲーム世界へ生成できない。
    UWorld* World = GetWorld();
    if (!World || !ProjectileClass)
    {
        return nullptr;
    }

    FActorSpawnParameters Params;
    Params.Owner = this;                  // 主な所有Actor。ダメージ元や relevancy 等でも利用され得る。
    Params.Instigator = this;             // 行動を起こしたPawn。ダメージ処理等の文脈に使える。
    Params.SpawnCollisionHandlingOverride =
        ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding;

    // テンプレート引数により戻り値がAProjectile*になる。
    return World->SpawnActor<AProjectile>(ProjectileClass, SpawnTransform, Params);
}
```

Spawnは失敗し得るため、戻り値を確認します。衝突時の扱い、Authority、クラスの有効性、Worldの状態なども設計対象です。

## 4. Deferred Spawn

通常のSpawnでは、生成後すぐConstruction処理へ進みます。その前に必須値を渡したい場合はDeferred Spawnを使います。

```cpp
AProjectile* Projectile = GetWorld()->SpawnActorDeferred<AProjectile>(
    ProjectileClass,
    SpawnTransform,
    this,       // Owner
    this,       // Instigator
    ESpawnActorCollisionHandlingMethod::AlwaysSpawn);

if (Projectile)
{
    // Construction Scriptが完了する前に、生成条件となる値を設定できる。
    Projectile->SetAttackData(CurrentAttackData);

    // 必ずFinishSpawningを呼び、残りの生成処理を完了させる。
    Projectile->FinishSpawning(SpawnTransform);
}
```

Deferred Spawn中のActorを通常の完成済みActorとして外部公開しないこと、失敗経路でも処理を中途半端に残さないことが重要です。

## 5. Transformを読む

```cpp
const FVector Location = GetActorLocation();
const FRotator Rotation = GetActorRotation();
const FVector Scale = GetActorScale3D();
const FTransform Transform = GetActorTransform();
```

- `FVector`：位置、方向、速度などを表す3要素。
- `FRotator`：Pitch、Yaw、Rollのオイラー角表現。
- `FQuat`：回転合成や補間に適したQuaternion。
- `FTransform`：平行移動、回転、拡縮をまとめる。

ワールド座標とローカル座標を混ぜないでください。アタッチされたComponentの`RelativeTransform`は親基準、`ComponentTransform`はワールド基準です。

## 6. 移動APIと衝突

```cpp
FHitResult Hit;
const FVector Delta = MoveDirection.GetSafeNormal() * MoveSpeed * DeltaSeconds;

// bSweep=trueなら移動経路で衝突を調べる。
AddActorWorldOffset(Delta, true, &Hit);

if (Hit.bBlockingHit)
{
    // 衝突位置、法線、相手Component等を使って応答する。
}
```

Transformの直接変更は、物理、CharacterMovement、ネットワーク補間と競合することがあります。Characterは原則としてCharacterMovementの入力経路を使い、物理物体は物理APIを使うなど、移動の責任者を一つに決めます。

## 7. DestroyとEndPlay

```cpp
void AProjectile::HandleImpact()
{
    // 破棄要求。C++のdeleteではない。
    Destroy();
}

void AProjectile::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // Timer、Delegate、外部登録など、自分が残した接続を解除する。
    GetWorldTimerManager().ClearAllTimersForObject(this);
    Super::EndPlay(EndPlayReason);
}
```

Actorを`delete`してはいけません。`Destroy()`はゲーム世界からの破棄を要求し、その後エンジンの寿命管理へ進みます。`EndPlay`は明示Destroyだけでなくレベル遷移、Play終了などでも呼ばれ得るため、理由に依存しない後始末の中心になります。

## 8. 高速アクションでの設計注意

- 弾・ヒット演出を無制限にSpawn／Destroyすると負荷が尖るため、計測後にPoolingを検討する。
- 攻撃判定Actorと見た目Actorを分けすぎると同期が複雑になる。責任単位で分割する。
- Owner、Instigator、攻撃者、ダメージ計算者を混同しない。
- Teleport、Sweep移動、CharacterMovement、Root Motionを同時に位置の責任者にしない。
- Destroy後も遅延コールバックが来る可能性を考え、購読解除と弱参照を使う。

## 参考

- [Actors in Unreal Engine](https://dev.epicgames.com/documentation/unreal-engine/actors-in-unreal-engine)
- [Actor Lifecycle](https://dev.epicgames.com/documentation/en-us/unreal-engine/unreal-engine-actor-lifecycle)
