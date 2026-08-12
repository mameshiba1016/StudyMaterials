# `const`・`constexpr`・`consteval`・`static`

これらはすべて「定数」という一語では説明できません。変更可能性、コンパイル時評価、記憶域期間、クラス共有メンバーなど異なる役割があります。

## `const`

```cpp
const int maximumHealth{100};
// maximumHealth = 200; // この名前を通じた変更は不可。
```

初期化後に変更しない契約を型へ付けます。実行時に決まる値も`const`にできます。

```cpp
const int windowWidth{ReadWidthFromConfig()};
```

これは必ずしもコンパイル時定数ではありません。

## const correctness

```cpp
class Player
{
public:
    [[nodiscard]] int GetHealth() const
    {
        return health_;
    }
private:
    int health_{100};
};
```

末尾`const`は`this`を通じて通常メンバーを変更しない契約です。読み取り操作を`const`化すると、`const Player&`から安全に呼べ、APIの意図が明確になります。

`mutable`メンバーは`const`関数からも変更できます。キャッシュやロックに使われますが、外から観測される論理状態を変えない用途へ限定します。

## `constexpr`

```cpp
constexpr int FramesPerSecond{60};

constexpr int Square(int value)
{
    return value * value;
}

static_assert(Square(4) == 16);
```

`constexpr`変数は定数式で初期化されます。`constexpr`関数は、定数式の条件を満たす引数と文脈ならコンパイル時に評価でき、通常の実行時引数でも呼べます。「必ずコンパイル時に実行される」という意味ではありません。

## `consteval`

```cpp
consteval int MakeId(int value)
{
    return value * 31;
}

constexpr int id{MakeId(7)};
```

C++20の即時関数で、評価が必ず定数式として行われる必要があります。コンパイル時検証、固定ID生成などに使えます。

## `constinit`

静的・スレッド記憶域期間の変数が静的初期化されることを要求し、静的初期化順序問題の一部を防ぐC++20の指定子です。変数自体を変更不能にする`const`とは別です。

```cpp
constinit int globalCounter{0}; // 実行時に変更可能だが、静的初期化を要求。
```

## `static`の複数の意味

### 関数ローカル

```cpp
int NextId()
{
    static int id{0}; // 初回到達時に初期化され、呼出後も存続。
    return id++;
}
```

初期化はC++11以降スレッドセーフですが、`id++`の同時実行はデータ競合です。

### クラスのstaticメンバー

```cpp
class Enemy
{
public:
    inline static int aliveCount{0}; // 全Enemyオブジェクトで一つを共有。
};
```

特定インスタンスに属さない共有状態です。staticメンバー関数には`this`がありません。

### 名前空間スコープ

名前へ内部リンケージを与える古い用法があります。`.cpp`内限定の関数・変数には無名名前空間も使われます。

## `volatile`はスレッド同期ではない

`volatile`は、実装が定める外部から変化し得るメモリ（メモリマップドI/O等）へのアクセス抑制に関係します。複数スレッド間の可視性、原子性、順序を保証しません。スレッド同期には`std::atomic`、mutexなどを使います。
