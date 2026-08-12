# 演算子オーバーロード

ユーザー定義型へ、既存演算子の意味を定義できます。ベクトル、行列、ハンドル、単位型などを自然に扱えますが、演算子本来の期待を裏切る意味にはしません。

## 二項演算子

```cpp
struct Vector2
{
    float x{};
    float y{};

    Vector2& operator+=(const Vector2& other)
    {
        x += other.x;
        y += other.y;
        return *this;
    }
};

Vector2 operator+(Vector2 left, const Vector2& right)
{
    left += right;
    return left;
}
```

`operator+=`を基本操作として実装し、`operator+`は左辺を値コピーして再利用します。戻り値最適化・ムーブが利用できます。非メンバーにすることで左右の変換が対称になりやすくなります。

## 比較

```cpp
struct EntityId
{
    std::uint32_t value{};
    auto operator<=>(const EntityId&) const = default;
};
```

C++20の三方比較演算子と既定比較により、メンバー順の比較を生成できます。ただし、その順序が型の意味として正しい場合だけ使います。浮動小数点メンバーはNaNにより全順序にならない点にも注意します。

## ストリーム出力

```cpp
std::ostream& operator<<(std::ostream& output, const Vector2& value)
{
    output << '(' << value.x << ", " << value.y << ')';
    return output;
}
```

ストリーム自身を参照で返すため、`std::cout << a << b;`の連鎖ができます。対象型と同じ名前空間へ置くとADLで見つかります。

## 添字演算子

```cpp
class GridRow
{
public:
    int& operator[](std::size_t index) { return cells_[index]; }
    const int& operator[](std::size_t index) const { return cells_[index]; }
private:
    std::array<int, 8> cells_{};
};
```

変更可能・読み取り専用のオーバーロードを用意します。通常の`operator[]`が境界検査を行うかは型の契約次第です。標準コンテナに合わせ、検査版`at()`を別途提供する設計もあります。

## 関数呼び出し演算子

```cpp
struct IsAlive
{
    bool operator()(const Enemy& enemy) const
    {
        return enemy.GetHealth() > 0;
    }
};
```

このような型を関数オブジェクトと呼びます。状態をメンバーとして保持でき、ラムダの仕組みとも関係します。

## 変換演算子

```cpp
class Handle
{
public:
    explicit operator bool() const noexcept
    {
        return IsValid();
    }
};
```

`explicit`により、意図しない数値演算などへの変換を防ぎつつ、`if (handle)`の条件文脈で使えます。

## オーバーロードできない・変えられないもの

演算子の優先順位、結合規則、オペランド数は変更できません。`.`、`::`、`?:`などオーバーロードできない演算子もあります。少なくとも一方のオペランドがユーザー定義型である必要があり、組み込み型同士の意味は変更できません。

## 設計原則

- `+`は元を変更せず新しい値、`+=`は左辺を変更する期待に合わせる。
- 比較演算子同士の整合性を保つ。
- 驚く副作用や高コスト処理を隠さない。
- `&&`・`||`のオーバーロードは組み込み演算子の短絡評価を再現しないため避ける。
- ゲームイベント送信など無関係な意味を`+`へ割り当てない。
