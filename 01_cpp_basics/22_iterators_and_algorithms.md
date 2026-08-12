# イテレータ・Ranges・標準アルゴリズム

イテレータはコンテナ要素の位置を表し、アルゴリズムとデータ構造を分離します。生ポインタも連続領域に対するイテレータとして振る舞えます。

## `begin`と`end`

```cpp
std::vector<int> values{4, 1, 3};

auto first{values.begin()};
auto last{values.end()};
```

`end()`は最後の要素ではなく、最後の一つ後を表す番兵位置です。間接参照してはいけません。半開区間`[first, last)`により、空範囲や範囲長を一貫して表せます。

## 検索

```cpp
#include <algorithm>

auto found{std::find(values.begin(), values.end(), 3)};
if (found != values.end())
{
    std::cout << *found;
}
```

線形検索なので要素数に比例します。頻繁なキー検索なら連想コンテナや別インデックスが適する場合があります。

## ソート

```cpp
std::sort(values.begin(), values.end());

std::sort(values.begin(), values.end(), [](int left, int right)
{
    return left > right; // 降順となるstrict weak orderingを提供。
});
```

比較関数は「同じならtrue」のように書いてはいけません。`a < b`に相当する厳密弱順序の契約を満たさないと、正しい動作が保証されません。

## erase-remove

```cpp
std::vector<int> hitPoints{100, 0, 30, 0};

auto newEnd{std::remove(hitPoints.begin(), hitPoints.end(), 0)};
hitPoints.erase(newEnd, hitPoints.end());
```

`std::remove`はコンテナサイズを変えず、残す要素を前へ移動し、新しい論理終端を返します。その後`erase`で末尾範囲を実際に削除します。C++20では次のように簡潔に書けます。

```cpp
std::erase(hitPoints, 0);
std::erase_if(hitPoints, [](int hp) { return hp <= 0; });
```

## 変換と集約

```cpp
std::vector<int> doubled(values.size());
std::transform(values.begin(), values.end(), doubled.begin(), [](int value)
{
    return value * 2;
});

int total{std::accumulate(values.begin(), values.end(), 0)};
```

`accumulate`の初期値型が計算型に影響します。浮動小数点合計へ整数`0`を渡すと意図しない変換が起こり得るため、`0.0`や適切な型を使います。

## Ranges（C++20）

```cpp
#include <ranges>

auto aliveView{enemies | std::views::filter([](const Enemy& enemy)
{
    return enemy.IsAlive();
})};

for (const Enemy& enemy : aliveView)
{
    enemy.Draw();
}
```

ビューは通常、要素を所有せず遅延評価します。元コンテナより長生きさせたり、反復中に元を変更して無効化したりしないよう寿命に注意します。

## アルゴリズムを使う利点

- 処理の目的が名前で現れる。
- 境界を統一して扱える。
- 十分に検証された実装を利用できる。
- 実装が最適化される余地がある。

ただし、複雑な状態更新を無理に一つの式へ詰め込む必要はありません。読みやすいループの方が意図を表す場合もあります。

## イテレータ無効化

コンテナ操作後もイテレータが有効かは、コンテナと操作ごとに異なります。`vector`の再確保、削除位置以降、`unordered_map`のリハッシュなどを確認します。無効化されたイテレータの比較・間接参照は危険です。
