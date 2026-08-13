# 05 UEの命名規則、型、文字列、コンテナ

## 1. 接頭辞から型の性質を読む

| 例 | おおよその意味 |
|---|---|
| `UCombatComponent` | `UObject`系クラス |
| `AMyCharacter` | `AActor`系クラス |
| `FVector` | 構造体・値型 |
| `ECombatState` | enum |
| `IInteractable` | Interface |
| `TArray<int32>` | テンプレート型 |
| `bIsAttacking` | bool値 |

接頭辞は飾りではなく、コードを見ただけで寿命、生成方法、値／参照の性質を推測する助けになります。

## 2. 固定幅整数と基本型

```cpp
int32 ComboIndex = 0;       // 符号付き32bit整数。
uint8 TeamId = 0;           // 符号なし8bit整数。
int64 Experience = 0;       // 符号付き64bit整数。
float Damage = 10.0f;       // 単精度浮動小数点。
double TimeSeconds = 0.0;   // 倍精度浮動小数点。
bool bCanDodge = true;      // 真偽値。UE規約ではb接頭辞。
```

保存、通信、バイナリ形式では幅が重要です。ただし数値型を選ぶ時は範囲だけでなく、APIが要求する型、アラインメント、変換、オーバーフローも考えます。

## 3. FString、FName、FText

```cpp
FString DebugMessage = TEXT("Damage: 100"); // 加工・結合する一般文字列。
FName SocketName = TEXT("weapon_r");        // 識別子として比較する名前。
FText DisplayName = NSLOCTEXT("Character", "HeroName", "Hero"); // 表示・ローカライズ対象。
```

- `FString`：可変文字列。組み立て、解析、ログなど。
- `FName`：名前テーブルを使う識別子。ソケット名、行名、タグ的なキーなど。表示文章用ではありません。
- `FText`：プレイヤーへ見せる文章とローカライズ。

用途を混ぜると、不要な変換、比較意図の不明瞭化、翻訳不能なUIが生まれます。

```cpp
UE_LOG(LogTemp, Log, TEXT("Target=%s Damage=%.1f"), *GetNameSafe(Target), Damage);
```

`*FString`は、ここでは書式指定が要求する文字ポインタを得るUEでよく見る記法です。通常のポインタ所有権を渡しているわけではありません。

## 4. TArray

```cpp
TArray<AActor*> Targets;
Targets.Reserve(32);      // 予想数が分かるなら再確保を減らす。
Targets.Add(NewTarget);   // 末尾へ追加。
Targets.Remove(Target);   // 一致要素を削除。順序維持等のコストを意識。

for (AActor* Target : Targets)
{
    if (IsValid(Target))
    {
        // 有効な対象を処理。
    }
}
```

`TArray`は連続領域を持つ動的配列で、キャッシュ効率と走査に優れます。追加で再確保が起きると、要素へのポインタや参照が無効になる可能性があります。走査中の削除にも注意します。

```cpp
for (int32 Index = Targets.Num() - 1; Index >= 0; --Index)
{
    if (!IsValid(Targets[Index]))
    {
        Targets.RemoveAtSwap(Index); // 順序不要なら末尾と交換して高速に削除。
    }
}
```

## 5. TMapとTSet

```cpp
TMap<FName, float> Cooldowns;
Cooldowns.Add(TEXT("Dodge"), 0.5f);

if (float* Duration = Cooldowns.Find(TEXT("Dodge")))
{
    // Findは値へのポインタを返す。見つからなければnullptr。
}

TSet<TWeakObjectPtr<AActor>> HitActors;
HitActors.Add(Target);
if (!HitActors.Contains(Target))
{
    // 同じ攻撃で未ヒットなら処理する、など。
}
```

- `TMap<Key, Value>`：キーから値を検索。
- `TSet<Value>`：重複しない値の集合。

ハッシュコンテナは便利ですが、毎フレームの小さな配列なら線形走査の方が単純で速い場合もあります。データ件数、検索頻度、順序の必要性、メモリを測って選びます。

## 6. 値渡し、参照、ポインタ

```cpp
void SetState(ECombatState NewState);               // 小さい値型は値渡し。
void ApplyAttack(const FAttackData& AttackData);    // 大きい構造体を変更せず借りる。
void ModifyAttack(FAttackData& AttackData);         // 呼び出し元の値を変更する。
void SetTarget(AActor* NewTarget);                  // nullptrを許し得るUObject参照。
```

`const&`を何にでも付けるのではなく、小さい算術型・enumは値渡しが明快です。ポインタ引数がnullptr可能か、保持するのか、その場だけ借りるのかを関数名・コメント・型で明確にします。

## 7. Castと型判定

```cpp
if (AMyEnemy* Enemy = Cast<AMyEnemy>(OtherActor))
{
    Enemy->ReceiveHit(HitData);
}
```

`Cast<T>`はUEの型情報を用いて`UObject`系を安全に確認します。失敗すれば`nullptr`です。ただし毎回具体クラスへCastすると結合が強くなるため、Interface、Component検索、Gameplay Tag、イベントなどが適する場面もあります。

## 8. 高速アクション向けデータ設計例

```cpp
USTRUCT(BlueprintType)
struct FAttackFrameData
{
    GENERATED_BODY()

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    FName AttackId;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (ClampMin = "0.0"))
    float Damage = 0.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (ClampMin = "0.0"))
    float HitStopSeconds = 0.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    TArray<FName> AllowedNextAttacks;
};
```

この構造体は「攻撃の定義データ」であり、現在のコンボ番号や残り硬直時間などの実行状態とは分けます。不変に近い定義と、フレームごとに変わる状態を分離すると、キャラクター追加、AI利用、デバッグ表示、リプレイ、ネットワーク対応へ発展させやすくなります。
