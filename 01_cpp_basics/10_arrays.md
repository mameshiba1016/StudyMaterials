# 配列

配列は同じ型の要素を連続して並べたものです。固定長配列、`std::array`、動的配列`std::vector`を目的に応じて使います。

## 組み込み配列

```cpp
// intを3個連続して保持し、各要素を指定値で初期化します。
int enemyHp[3]{100, 80, 120};

// 添字は0から始まります。
int first{enemyHp[0]};
enemyHp[1] -= 20;
```

有効な添字は0、1、2です。`enemyHp[3]`へのアクセスは境界外で未定義動作です。C++の組み込み添字アクセスは通常、実行時境界検査を行いません。

## `std::array`

```cpp
#include <array>

std::array<int, 3> enemyHp{100, 80, 120};

std::size_t count{enemyHp.size()}; // 要素数3。
int first{enemyHp[0]};             // 高速だが通常は境界検査なし。
int checked{enemyHp.at(1)};        // 範囲外ならstd::out_of_range例外。
```

要素数がコンパイル時に決まる固定長コンテナです。組み込み配列よりサイズ取得、代入、STLアルゴリズムとの連携が扱いやすいため、固定長では基本候補になります。

## `std::vector`

```cpp
#include <vector>

std::vector<int> bullets{};
bullets.push_back(10); // 末尾へ要素を追加。
bullets.push_back(20);

for (const int damage : bullets)
{
    std::cout << damage << '\n';
}
```

実行時に要素数を増減でき、要素を連続メモリへ保持します。通常、オブジェクト本体は要素領域へのポインタ、サイズ、容量などを管理します。

## `size`と`capacity`

- `size()`：現在存在する要素数。
- `capacity()`：再確保せず保持できる要素数。

`push_back`で容量を超えると、より大きな領域を確保して既存要素を移動またはコピーし、古い領域を解放することがあります。この再確保により、要素へのポインタ・参照・イテレータが無効化されます。

```cpp
std::vector<int> values{};
values.reserve(100); // 要素は増やさず、少なくとも100個分の容量を事前確保。
```

最大数の見積もりがある弾やパーティクルでは再確保を抑えられます。ただし、むやみに巨大な容量を予約するとメモリを浪費します。

## 多次元データ

```cpp
constexpr int width{4};
constexpr int height{3};
std::array<int, width * height> tiles{};

int x{2};
int y{1};
int& tile{tiles[y * width + x]};
```

2Dグリッドを一次元の連続領域へ配置する方法です。`y * width + x`で位置を求めます。座標の範囲検査、オーバーフロー、行優先の配置を理解する必要があります。連続配置は各行を別々に確保する構造よりキャッシュ効率とシリアライズで有利な場合があります。

## 配列を関数へ渡す

組み込み配列を関数引数へ普通に書くと、多くの文脈で先頭要素へのポインタへ調整され、要素数情報が失われます。固定長なら`std::array<T, N>&`、連続した任意長の非所有ビューならC++20の`std::span<T>`、所有・可変長なら`std::vector<T>`など、意図を型へ表します。
