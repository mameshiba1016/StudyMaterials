# コピー・ムーブ・値カテゴリ

C++では、オブジェクトを複製するコピーと、保持しているリソースを別オブジェクトへ移すムーブを型ごとに設計できます。ゲームでは文字列、コンテナ、GPUリソースのラッパー、大量データの受け渡しに関係します。

## コピー

```cpp
std::string original{"Player"};
std::string copied{original}; // コピーコンストラクタ。別の文字列として同じ内容を持つ。

copied += " Two";
// originalは"Player"のまま。
```

値型のコピー後は、通常それぞれ独立して正しく使用できることが期待されます。生ポインタだけをメンバーに持つクラスを既定コピーすると、アドレスだけが複製され、二重解放や意図しない共有を起こす可能性があります。

## コピーコンストラクタとコピー代入

```cpp
class Score
{
public:
    Score(int value) : value_{value} {}

    Score(const Score& other)
        : value_{other.value_}
    {
        // 新しいオブジェクトを既存otherから構築する。
    }

    Score& operator=(const Score& other)
    {
        if (this != &other) // 自己代入にも正しく対応する。
        {
            value_ = other.value_;
        }
        return *this;
    }

private:
    int value_{};
};
```

このような単純型では自作不要です。コンパイラ生成の処理が正しく、高速で保守しやすいためです。

## ムーブ

```cpp
std::vector<int> source(100000, 42);
std::vector<int> destination{std::move(source)};
```

典型的な`vector`のムーブは、大量要素を一個ずつ複製せず、動的領域の管理情報を移します。移動後の`source`は「有効だが状態は規定されない」範囲にあり、破棄や再代入は可能です。空であると一般化してはいけません（個別型が保証する場合を除く）。

## `std::move`の正体

`std::move`は実際に移動処理を行う関数ではなく、引数をムーブ候補となる右辺値へキャストします。その後、利用可能ならムーブコンストラクタやムーブ代入が選ばれます。ムーブ処理がなければコピーされる場合もあります。

```cpp
const std::string name{"Player"};
std::string result{std::move(name)};
```

`name`は`const`なので、通常のムーブコンストラクタが要求する非`const`右辺値参照へ渡せず、コピーになるのが一般的です。移動元を変更して空の状態へできないためです。

## 値カテゴリ

厳密な規則は複雑ですが、最初は次の役割で理解します。

- 左辺値：識別可能で、式の後も存在する対象を表すことが多い。
- 右辺値：一時値や、リソースを再利用してよいと明示された値を表すことが多い。
- `std::move(x)`：名前付きオブジェクト`x`を右辺値として扱う許可を与える。

名前を持つ右辺値参照変数は、式として使うと左辺値です。

## コピー省略と戻り値

```cpp
std::vector<int> BuildValues()
{
    std::vector<int> values{1, 2, 3};
    return values;
}
```

値で返しても、コピー省略やムーブにより効率的に構築できます。ローカル変数へ`return std::move(values);`と書くと、名前付き戻り値最適化を妨げる可能性があるため通常は書きません。

## Rule of Zero・Five

- Rule of Zero：所有を標準コンテナやRAIIメンバーへ任せ、デストラクタやコピー・ムーブを自作しない。
- Rule of Five：特別なリソース管理のため一つを自作するなら、デストラクタ、コピー構築、コピー代入、ムーブ構築、ムーブ代入をまとめて検討する。

コピー禁止の排他的所有型は明示できます。

```cpp
class GpuBuffer
{
public:
    GpuBuffer(const GpuBuffer&) = delete;
    GpuBuffer& operator=(const GpuBuffer&) = delete;
    GpuBuffer(GpuBuffer&&) noexcept = default;
    GpuBuffer& operator=(GpuBuffer&&) noexcept = default;
};
```

ムーブが`noexcept`なら、`vector`の再確保時に安全なムーブを選択しやすくなります。ただし、本当に例外を送出しない実装だけに指定します。

## 不要なコピーを探す

- 大きな引数を値渡ししていないか。
- 範囲`for`で要素を毎回コピーしていないか。
- getterが巨大コンテナを不用意に値返ししていないか。
- 毎フレーム文字列や一時コンテナを生成していないか。

一方、小さな型まで参照だらけにすると別名関係が増え、最適化と理解を妨げることがあります。プロファイラで実コストを確認します。
