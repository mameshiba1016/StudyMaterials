# テンプレート・ジェネリックプログラミング・Concepts

テンプレートは、型や値をパラメータとしてコードを生成する仕組みです。標準コンテナ、スマートポインタ、アルゴリズムの基盤です。

## 関数テンプレート

```cpp
template<typename T>
T Clamp(T value, T minimum, T maximum)
{
    if (value < minimum)
    {
        return minimum;
    }
    if (value > maximum)
    {
        return maximum;
    }
    return value;
}

int hp{Clamp(120, 0, 100)};           // Tはintと推論。
float volume{Clamp(1.2F, 0.0F, 1.0F)}; // Tはfloatと推論。
```

呼び出された型ごとに必要な特殊化が実体化されます。すべての型に無条件で使えるわけではなく、この例では`<`と`>`、コピーまたはムーブなどが必要です。

## クラステンプレート

```cpp
template<typename T, std::size_t Capacity>
class FixedPool
{
public:
    [[nodiscard]] constexpr std::size_t GetCapacity() const
    {
        return Capacity;
    }

private:
    std::array<T, Capacity> elements_{};
};

FixedPool<int, 128> integerPool{};
```

`T`は型テンプレート引数、`Capacity`は非型テンプレート引数です。容量が型の一部になるため、`FixedPool<int, 64>`と`FixedPool<int, 128>`は別の型です。

## テンプレート定義をヘッダーへ置く理由

コンパイラは実体化時に通常、テンプレート定義を見る必要があります。そのため宣言だけをヘッダー、定義を通常の`.cpp`へ分離するとリンクエラーになりやすいです。ヘッダーへ定義する、明示的実体化を使う、C++20 Modulesなどを検討します。

## `auto`

```cpp
auto score{100};             // int。
auto name{std::string{"A"}}; // std::string。
```

右辺から型を推論します。型を消す機能ではなく、コンパイル時に具体型が決まります。長いイテレータ型や、式と型が明白な場面で有用です。単位や符号の違いが重要な値まで何でも`auto`にすると、読み手が型を確認しづらくなる場合があります。

参照や`const`が推論で落ちる規則に注意します。

```cpp
const int original{42};
auto copied{original};       // int。値としてコピーされ、トップレベルconstは落ちる。
const auto& reference{original}; // const int&。
```

## Concepts（C++20）

Conceptsはテンプレート引数へ要求を記述し、意図とエラーメッセージを改善します。

```cpp
#include <concepts>

template<std::integral T>
T AddScore(T current, T addition)
{
    return current + addition;
}
```

この関数は整数型の概念を満たす型だけを受け取ります。Conceptは単なるコメントではなく、オーバーロード選択にも関与します。

## 可変長テンプレート

型パラメータを任意個受け取れます。`std::tuple`、フォーマット、イベント配送などで使われます。

```cpp
template<typename... Args>
void Log(Args&&... args)
{
    (std::cout << ... << args); // C++17の畳み込み式。
}
```

`Args&&...`は推論される文脈では転送参照となり、`std::forward<Args>(args)...`による完全転送へ発展します。誤用すると寿命やオーバーロードが難しくなるため、単純なAPIで不要に使いません。

## コンパイル時間とコードサイズ

多数の型で巨大テンプレートを実体化すると、コンパイル時間、デバッグ情報、実行ファイルサイズが増える可能性があります。実装の分離、明示的実体化、型消去、非テンプレート共通処理などを使い分けます。
