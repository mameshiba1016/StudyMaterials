# 型変換とキャスト

型変換は値の表現・範囲・意味を変える可能性があります。暗黙変換を理解し、意図的な変換はC++キャストで種類を明示します。

## 暗黙変換

```cpp
double precise{3.9};
int truncated = precise; // 小数部を捨てて3。波括弧初期化なら狭窄として拒否。
```

警告レベルを上げ、狭窄変換を見逃さないようにします。

## 整数昇格と符号

```cpp
int signedValue{-1};
std::size_t unsignedValue{1};

bool result{signedValue < unsignedValue};
```

通常の算術変換で負の`int`が巨大な符号なし値へ変換され、直感と異なる結果になる場合があります。コンテナの`size()`との比較にはC++20の`std::ssize`、適切な型、範囲検査を使います。

符号付き整数の範囲外演算は未定義動作です。符号なし整数は2のべき乗を法としてラップしますが、ゲームのHP計算でラップを期待してよいという意味ではありません。

## `static_cast`

関連が明確な数値変換、列挙と基礎型、検査不要と判断した継承変換などに使います。

```cpp
float ratio{static_cast<float>(defeatedEnemies) / totalEnemies};
```

変換を目立たせますが、安全性を自動保証しません。範囲外の浮動小数点から整数、誤ったダウンキャストなどには注意します。

## `dynamic_cast`

仮想関数を持つ多態的階層で、実行時型を検査します。

```cpp
Actor* actor{GetActor()};
if (Enemy* enemy{dynamic_cast<Enemy*>(actor)})
{
    enemy->Attack();
}
```

ポインタ変換失敗は`nullptr`、参照変換失敗は`std::bad_cast`例外です。頻繁な具体型判定は、仮想インターフェースやコンポーネント設計を見直す兆候になり得ます。

## `const_cast`

`const`・`volatile`の修飾だけを変更します。

```cpp
const int value{10};
int* writable{const_cast<int*>(&value)};
// *writable = 20; // 元が本当にconstなオブジェクトなので変更は未定義動作。
```

元のオブジェクトが非`const`で、別APIの都合で`const`経路になっている場合など限定用途です。設計上の`const`契約を破る道具として使いません。

## `reinterpret_cast`

ポインタ・整数・低水準表現間の実装依存性が高い変換です。通常の型変換には使いません。ハードウェア、システムAPI、シリアライズ内部などでも、アラインメント、寿命、厳密な別名規則、エンディアンを満たす必要があります。

オブジェクト表現を調べる用途には`std::bit_cast`（C++20）、`std::memcpy`、`std::byte`など規則に沿う手段を検討します。

## C形式キャスト

```cpp
int value = (int)3.5;
```

複数種類のC++キャストを順に試すため、どの危険な変換が行われたか分かりにくくなります。C++コードでは意図に合う名前付きキャストを使います。

## 単位変換

`float`同士でも秒とミリ秒、度とラジアン、メートルとセンチメートルは意味が違います。単なる`static_cast`では防げません。命名、専用型、`std::chrono`、単位ライブラリなどで意味を型・APIへ表します。
