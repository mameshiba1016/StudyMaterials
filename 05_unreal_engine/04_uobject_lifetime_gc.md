# 04 UObjectの寿命、GC、参照方法

## 1. すべてのC++オブジェクトがGC対象ではない

UEのGarbage Collection（GC）が管理する中心は`UObject`派生オブジェクトです。普通のC++クラス、`new`で確保した生メモリ、OSハンドルなどをGCが自動で片付けるわけではありません。

| 対象 | 代表的な寿命管理 |
|---|---|
| `UObject`派生 | UEの参照追跡とGC、Outer、エンジン所有関係 |
| `AActor` | WorldへSpawnされ、Destroy要求後に破棄へ進む |
| `UActorComponent` | 所有Actorとの関係、登録・破棄ライフサイクル |
| 普通のC++オブジェクト | 値、RAII、`TUniquePtr`、`TSharedPtr`等 |
| アセット参照 | ハード参照またはソフト参照、ロード管理 |

## 2. GCの基本的な考え方

概念的には、エンジンがRoot Setなどの生存起点から認識可能な参照をたどり、到達できないGC対象を回収します。重要なのは「C++変数にアドレスが入っている」だけでは、必ずしもGCがその参照を認識できないことです。

```cpp
UCLASS()
class MYGAME_API UTargetTracker : public UObject
{
    GENERATED_BODY()

private:
    UPROPERTY()
    TObjectPtr<AActor> CurrentTarget; // UEが追跡できる強いオブジェクト参照。
};
```

`UPROPERTY`として認識された`UObject`参照は、参照追跡、シリアライズ、エディタ連携等の対象になり得ます。反対に、追跡されない生ポインタへ入れただけの参照を「これがあるから絶対に生存する」と考えてはいけません。

## 3. TObjectPtr、TWeakObjectPtr、TSoftObjectPtr

### TObjectPtr

```cpp
UPROPERTY()
TObjectPtr<UCombatComponent> CombatComponent;
```

所有クラスが強く参照し、生存中に直接利用する`UObject`参照に使います。`UPROPERTY`と組み合わせ、エンジンに見える形にするのが基本です。

### TWeakObjectPtr

```cpp
TWeakObjectPtr<AActor> LockedTarget;

if (AActor* Target = LockedTarget.Get())
{
    // この瞬間に有効だった対象を使用する。
}
```

対象を生かし続ける責任を持たず、「まだ存在するなら使う」参照です。ロックオン候補、観察対象、キャッシュなどに適します。使用時に有効性を確認します。

### TSoftObjectPtr

```cpp
UPROPERTY(EditDefaultsOnly)
TSoftObjectPtr<UAnimMontage> AttackMontage;
```

オブジェクトの直接参照ではなく、ロード可能なパスを中心に扱うソフト参照です。大量のキャラクターや演出アセットを必要になるまでロードしない設計に役立ちます。ただし実際に使う前には同期または非同期ロードが必要です。

### TSubclassOf

```cpp
UPROPERTY(EditDefaultsOnly)
TSubclassOf<AProjectile> ProjectileClass;
```

インスタンスではなく「指定基底型を継承したクラス」を保持します。SpawnするActorクラス、生成するEffectクラスなどの選択に使います。

## 4. UObjectを作る、Actorを出す

```cpp
// UObject派生を生成。Outerは所有・名前空間・寿命関係で重要。
UAttackRuntimeData* RuntimeData = NewObject<UAttackRuntimeData>(this);

// ActorはWorldへSpawnする。通常のnewで作らない。
FActorSpawnParameters Params;
Params.Owner = this;
AProjectile* Projectile = GetWorld()->SpawnActor<AProjectile>(ProjectileClass, SpawnTransform, Params);
```

`UObject`派生を通常の`new`で生成したり、Actorを`new AActor`したりしてはいけません。UEの登録、Outer、World参加、初期化ライフサイクルを通る生成APIを使います。

## 5. Outerは単純な所有権ポインタではない

`Outer`はオブジェクトの包含・名前・パッケージ階層などに関わります。「Outerを設定しただけで、どんな場合でもGCから守られる」という単純な理解は危険です。生存させたい参照は、エンジンが追跡できる所有関係や`UPROPERTY`等で明示します。

## 6. ActorのDestroyと参照

`Destroy()`を呼んだ直後にC++メモリがその場で消える、とだけ考えると誤ります。Actorは破棄予定となり、エンジンのライフサイクルに従って処理されます。他の場所に残った参照は、利用前に有効性を考える必要があります。

```cpp
if (IsValid(CurrentTarget))
{
    // nullptrでなく、破棄済み・破棄途中として無効でもない場合に使う。
    CurrentTarget->ApplyDamage(...);
}
```

ただし、あらゆる行で無条件に`IsValid`を付けるのではなく、誰が所有し、どのイベントで参照を解除するかも設計します。対象の`OnDestroyed`等を購読してロックオンを解除する方が、状態遷移として明確な場合があります。

## 7. 非UObjectにはRAIIを使う

```cpp
class FCombatSearchCache
{
public:
    // 値メンバやUEコンテナは、所有オブジェクトの破棄時に自動で後始末される。
    TArray<FVector> CandidatePositions;
};

TUniquePtr<FCombatSearchCache> Cache = MakeUnique<FCombatSearchCache>();
```

普通のC++資源にはRAIIが有効です。`TSharedPtr`は共有所有が本当に必要な場合だけ使い、循環所有を避けます。なお、一般的な`TSharedPtr`は`UObject`のGC管理を置き換えるものではありません。

## 8. 高速アクションで起きやすい寿命バグ

- 敵が倒された後もロックオン参照を使う。
- キャラクター交代後、旧キャラクターのComponentへTimerが発火する。
- 非同期ロード完了時には呼び出し元が破棄されている。
- Animation Notifyから、すでに終了した攻撃状態へアクセスする。
- レベル遷移後も前WorldのActorをSubsystem等に保持する。
- Lambdaが`this`を捕捉し、遅延実行時に無効になっている。

対策は単なるnullチェックだけではありません。弱参照、Delegate解除、Timer解除、非同期コールバックでの再検証、Worldごとの責任分離、状態IDによる古い結果の破棄を組み合わせます。

## 参考：Epic Games公式ドキュメント

- [Unreal Object Handling](https://dev.epicgames.com/documentation/en-us/unreal-engine/unreal-object-handling-in-unreal-engine)
- [Object Pointers](https://dev.epicgames.com/documentation/en-us/unreal-engine/object-pointers-in-unreal-engine)
