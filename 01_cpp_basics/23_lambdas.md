# ラムダ式・関数オブジェクト・コールバック

ラムダ式は、その場で呼び出し可能な関数オブジェクトを作る構文です。アルゴリズムの条件、イベント、非同期処理、カスタム比較などに使います。

## 基本構文

```cpp
auto add{[](int left, int right) -> int
{
    return left + right;
}};

int result{add(2, 3)};
```

- `[]`：キャプチャリスト。
- `(int left, int right)`：引数。
- `-> int`：戻り値型。多くの場合は推論でき、省略可能。
- `{}`：関数本体。

ラムダ式ごとに名前のない固有のクロージャ型が生成されます。`auto`で受け取れば型を明示する必要がありません。

## 値キャプチャ

```cpp
int minimumDamage{10};

auto clampDamage{[minimumDamage](int damage)
{
    return damage < minimumDamage ? minimumDamage : damage;
}};
```

作成時点の`minimumDamage`をクロージャ内部へコピーします。元の変数を後で変更しても、キャプチャ済みの値は変化しません。

## 参照キャプチャ

```cpp
int score{0};

auto addScore{[&score](int amount)
{
    score += amount;
}};
```

元の`score`を参照して変更します。ラムダが`score`より長生きするとダングリング参照になります。イベントシステムへ長期登録するラムダで、ローカル変数や`this`を参照キャプチャするのは典型的な事故です。

## `[=]`と`[&]`

- `[=]`：使用する外側変数を原則値キャプチャ。
- `[&]`：使用する外側変数を原則参照キャプチャ。

便利ですが、何を保持しているか見えづらく寿命監査が難しくなります。長寿命コールバックでは明示キャプチャを推奨します。

## `this`のキャプチャ

メンバー関数内の`[this]`はオブジェクトへのポインタを保持し、オブジェクトの寿命を延長しません。コールバック実行前に所有者が破棄されると危険です。`[*this]`はオブジェクトを値としてコピーしますが、コピーコストと意味が異なります。共有所有型なら`weak_ptr`をキャプチャして実行時に`lock()`する設計もあります。

## `mutable`

値キャプチャしたメンバーは既定でラムダ本体から変更できません。

```cpp
int count{0};
auto counter{[count]() mutable
{
    return ++count; // クロージャ内部のコピーを変更。外側countは変化しない。
}};
```

## ジェネリックラムダ

```cpp
auto maximum{[](const auto& left, const auto& right)
{
    return left < right ? right : left;
}};
```

`auto`引数により、呼び出し演算子がテンプレート化されます。

## `std::function`

```cpp
std::function<void(int)> callback;
callback = [](int damage) { std::cout << damage; };
```

異なる呼び出し可能型を同じ型として保存する型消去ラッパーです。便利ですが、間接呼び出し、サイズ、場合によって動的確保のコストがあります。テンプレート引数、関数ポインタ、専用デリゲートなどと使い分けます。

## イベント解除

イベントへラムダを登録するAPIでは、登録先がキャプチャ対象より長生きし得ます。登録トークンをRAIIで解除する、所有者破棄時に購読解除する、弱い参照を使うなど、登録と解除を対にします。Unreal EngineのDelegateにも固有のバインド方式と寿命規則があります。
