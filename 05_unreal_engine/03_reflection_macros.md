# 03 リフレクションと主要マクロ

## 1. リフレクションとは

通常のC++は、実行時に「このオブジェクトにはどんなプロパティがあるか」をエディタが自由に列挙できるほど豊富な型情報を標準では保持しません。UEはUHTが生成するメタデータと登録コードにより、選択した型・関数・値をエンジンから認識できるようにします。

この仕組みが支える代表例は次の通りです。

- Detailsパネルでの編集
- Blueprintからの読み書き・関数呼び出し
- シリアライズとアセット保存
- Garbage Collectionの参照追跡
- ネットワーク複製
- Delegateやイベント
- エディタ検索、カテゴリ、ツールチップ

マクロを付ければ必ず全機能が有効になるわけではありません。各Specifierで「何を許可するか」を指定します。

## 2. 型を登録するマクロ

```cpp
UENUM(BlueprintType)
enum class ECombatState : uint8
{
    Idle,
    Attacking,
    Dodging,
    Stunned
};

USTRUCT(BlueprintType)
struct FAttackData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat")
    float Damage = 10.0f;
};

UCLASS(Blueprintable)
class MYGAME_API UCombatDefinition : public UObject
{
    GENERATED_BODY()
};
```

- `UENUM`：列挙型をUEへ登録。
- `USTRUCT`：値として扱いやすい構造体をUEへ登録。通常は`F`接頭辞。
- `UCLASS`：`UObject`系クラスをUEへ登録。通常は`U`、Actor系なら`A`接頭辞。
- `GENERATED_BODY()`：UHT生成コードを型宣言へ接続。

## 3. UPROPERTYを読む

```cpp
UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat", meta = (ClampMin = "0.0"))
float BaseDamage = 10.0f;
```

左から機械的に読みます。

- `EditDefaultsOnly`：クラスのデフォルト値側で編集可能。レベル上の個体ごとの変更はさせない。
- `BlueprintReadOnly`：Blueprintから読み取り可能、直接書き換え不可。
- `Category`：Details／Blueprint上の分類。
- `ClampMin`：エディタ入力値の下限を示すメタデータ。
- `BaseDamage`：C++上のメンバ名。
- `= 10.0f`：C++の既定値。

よく使う編集指定を区別します。

| 指定 | 意味 |
|---|---|
| `VisibleAnywhere` | 表示するが編集させない |
| `EditAnywhere` | デフォルトとインスタンスで編集可能 |
| `EditDefaultsOnly` | デフォルトだけ編集可能 |
| `EditInstanceOnly` | 配置した個体だけ編集可能 |
| `BlueprintReadOnly` | Blueprintから読める |
| `BlueprintReadWrite` | Blueprintから読み書きできる |
| `Transient` | 通常の永続保存対象にしない一時値 |

すべてを`EditAnywhere, BlueprintReadWrite`にすると、どこからでも変更でき、状態の不変条件が崩れます。「調整値」「実行中の状態」「内部キャッシュ」を区別して最小限だけ公開します。

## 4. UFUNCTIONを読む

```cpp
UFUNCTION(BlueprintCallable, Category = "Combat")
bool TryStartAttack(int32 AttackIndex);

UFUNCTION(BlueprintPure, Category = "Combat")
float GetStaminaRatio() const;

UFUNCTION(BlueprintImplementableEvent, Category = "Combat")
void PlayAttackPresentation();

UFUNCTION(BlueprintNativeEvent, Category = "Combat")
void OnGuardBroken();
```

- `BlueprintCallable`：実行ピンを持つ関数として呼べます。
- `BlueprintPure`：原則として外部状態を変更しない値取得。乱用すると評価回数が見えづらくなります。
- `BlueprintImplementableEvent`：宣言をC++、実装をBlueprintに置くイベント。
- `BlueprintNativeEvent`：C++既定実装を持ち、Blueprintで上書き可能。C++側は通常`OnGuardBroken_Implementation()`を実装します。

```cpp
void UMyCombatComponent::OnGuardBroken_Implementation()
{
    // Blueprintが上書きしない場合に使われる既定処理。
}
```

## 5. CDO（Class Default Object）

UEは各`UClass`に、そのクラスの既定値を持つClass Default Objectを用意します。BlueprintのClass DefaultsやC++コンストラクタで設定した値は、インスタンス生成時の原型として関係します。

このためコンストラクタは「ゲーム開始後のWorld状態へアクセスして行う処理」の場所ではありません。コンストラクタは既定サブオブジェクト生成と初期値設定を中心にし、Worldや他Actorが必要な初期化は適切なライフサイクル関数へ置きます。

```cpp
AMyCharacter::AMyCharacter()
{
    // CDO構築時にも通るため、既定構成を定義する場所。
    CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
    CameraBoom->SetupAttachment(RootComponent);
}

void AMyCharacter::BeginPlay()
{
    Super::BeginPlay();
    // 実際のゲームWorldに参加した後で必要な初期化。
}
```

## 6. マクロとC++アクセス権は別

`BlueprintReadOnly`はBlueprint上の権限であり、C++の`private:`とは別です。次のように、C++ではprivateを保ちつつエディタへ限定公開できます。

```cpp
private:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat", meta = (AllowPrivateAccess = "true"))
    float MaxStamina = 100.0f;
```

ただし`AllowPrivateAccess`を大量に付ける前に、データの責任と変更経路が適切かを設計してください。

## 参考：Epic Games公式ドキュメント

- [Reflection System](https://dev.epicgames.com/documentation/unreal-engine/reflection-system-in-unreal-engine)
- [Programming with C++](https://dev.epicgames.com/documentation/en-us/unreal-engine/programming-with-cpp-in-unreal-engine)
