# 構造体・クラス・カプセル化

クラスはデータと操作、不変条件、所有関係を一つの型として表します。`struct`と`class`は機能の大部分が同じで、主な言語上の違いは既定アクセスと既定継承アクセスです。

- `struct`：メンバーと基底クラスが既定で`public`。
- `class`：既定で`private`。

## 単純な値型

```cpp
struct Vector2
{
    float x{0.0F};
    float y{0.0F};
};

Vector2 position{10.0F, 20.0F};
```

メンバーを公開し、単純な値の集合として扱う型には`struct`が自然です。ただし常にすべて公開すべきという意味ではありません。

## 不変条件を守るクラス

```cpp
class Health
{
public:
    explicit Health(int maximum)
        : maximum_{maximum > 0 ? maximum : 1},
          current_{maximum_}
    {
    }

    void ApplyDamage(int amount)
    {
        if (amount <= 0)
        {
            return;
        }

        current_ -= amount;
        if (current_ < 0)
        {
            current_ = 0;
        }
    }

    [[nodiscard]] int GetCurrent() const
    {
        return current_;
    }

private:
    int maximum_{1};
    int current_{1};
};
```

`current_`を非公開にし、変更経路を制御することで`0 <= current_ <= maximum_`という不変条件を守れます。単に全メンバーへgetter/setterを付けるだけではカプセル化にならず、意味ある操作を公開することが重要です。

## コンストラクタとメンバー初期化リスト

コンストラクタ本体へ入る前に、基底クラスとメンバーは宣言順で初期化されます。初期化リストの記述順ではありません。

```cpp
class Example
{
public:
    Example(int value)
        : value_{value}
    {
    }

private:
    int value_;
};
```

本体内の`value_ = value;`は初期化後の代入です。参照、`const`メンバー、既定構築できない型は初期化リストが必要です。

## `explicit`

一引数コンストラクタなどによる意図しない暗黙変換を防ぎます。

```cpp
class Seconds
{
public:
    explicit Seconds(float value) : value_{value} {}
private:
    float value_{};
};

// Seconds time = 1.0F; // explicitなので暗黙変換不可。
Seconds time{1.0F};
```

ゲームでは秒、フレーム、メートルなど単位の混同を型で防ぐ設計へ発展します。

## `const`メンバー関数

```cpp
int GetCurrent() const;
```

末尾の`const`は、通常のメンバーをその関数経由で変更しない契約を表し、`const Health`からも呼べます。戻り値型の左に付く`const`とは位置と意味が異なります。

## `this`

非`static`メンバー関数では、現在のオブジェクトを指す`this`ポインタを利用できます。

```cpp
void SetValue(int value)
{
    this->value_ = value;
}
```

名前衝突の区別に使えますが、メンバー命名規則があれば常に明示する必要はありません。

## 特別メンバー関数

クラスにはデストラクタ、コピーコンストラクタ、コピー代入、ムーブコンストラクタ、ムーブ代入があります。コンパイラが暗黙生成する条件は相互に関係します。

- Rule of Zero：標準コンテナやRAII型に所有を任せ、特別メンバーを自作しない。
- Rule of Five：生リソースを直接所有して特別な破棄が必要なら、コピー・ムーブを含む五つの操作を検討。

通常はRule of Zeroを目指します。

## コンポジション

```cpp
class Player
{
private:
    Health health_{100};
    Movement movement_{};
    Inventory inventory_{};
};
```

「PlayerはHealth機能を持つ」というhas-a関係です。巨大な継承階層より、交換可能な小さな責任を組み合わせる方がゲームの仕様変更へ対応しやすい場合が多くあります。
