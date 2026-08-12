# `optional`・`variant`・`any`

「値がない」「複数候補のどれか」「任意の型」を型安全に表す標準型です。無効値として`-1`や空文字を乱用するより、状態を型へ組み込めます。

## `std::optional`

値が存在するか、存在しないかを表します。

```cpp
#include <optional>

std::optional<int> FindNearestEnemyId();

if (std::optional<int> id{FindNearestEnemyId()})
{
    std::cout << *id;
}
else
{
    std::cout << "No enemy";
}
```

`has_value()`、`operator bool`で存在確認し、`*`、`value()`で取得します。空の`optional`へ`value()`を呼ぶと`std::bad_optional_access`例外です。`value_or(defaultValue)`で既定値を選べます。

`optional<T>`は通常`T`の格納領域を内部に持ち、値がない場合も`sizeof(T)`相当以上を占めます。動的確保を意味する型ではありません。

## `std::variant`

列挙された複数型のうち、常に一つを保持するタグ付き共用体です。

```cpp
#include <variant>

struct DamageEvent { int amount; };
struct HealEvent { int amount; };
struct DeathEvent {};

using GameEvent = std::variant<DamageEvent, HealEvent, DeathEvent>;

GameEvent event{DamageEvent{25}};

std::visit([](const auto& concreteEvent)
{
    // 実際に保持している型に応じた処理。
}, event);
```

基底クラスと仮想関数を使わず、候補型が閉じた集合の場合に値多態性を表せます。新しい操作の追加は訪問側を追加しやすく、新しい型の追加はすべての訪問処理へ影響し得ます。

`std::get<T>`で型が違うと例外、`std::get_if<T>`ならポインタで安全に検査できます。例外など特殊状況で`valueless_by_exception()`になる可能性があります。

## `std::monostate`

`variant`の最初の候補を「何もない状態」にしたい場合に使える空の型です。

```cpp
using Selection = std::variant<std::monostate, PlayerId, EnemyId>;
```

ただし、「対象なし」を許すことが本当に仕様かを検討します。

## `std::any`

コピー可能な任意型を一つ保持します。

```cpp
std::any value{42};
int number{std::any_cast<int>(value)};
```

候補型がコンパイル時に限定できないプラグインデータ等で役立ちますが、型安全性が実行時へ移り、誤った`any_cast`、動的確保、設計の不透明化につながります。候補が分かるなら`variant`、共通契約があるなら多態性、データ形式なら明示的スキーマを優先します。

## 選択指針

- `optional<T>`：Tがある／ない。
- `variant<A, B>`：AまたはBのどちらか一つ。
- 継承・仮想関数：候補が拡張され、共通契約で操作する。
- `any`：候補を列挙できず、実行時型消去が本当に必要。
- エラー型：成功値または失敗理由。C++23の`std::expected`など。
