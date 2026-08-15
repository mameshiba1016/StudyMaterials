# C++実習10：配列

固定長Combo表を`std::array`で表し、0始まりIndex、`size`、`front/back`、Range走査、`at`の範囲検査、Nested Arrayを学びます。

## Visual StudioでBuild・Test・実行

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\build.ps1 -Configuration Debug -Run
```

生成後は`build/cpp_arrays.sln`をVisual Studioで開けます。

## 期待結果

```text
要素数: 4
先頭Damage: 20
合計Damage: 195
Bonus後の最終段: 95
2次元配列合計: 21
```

Testでは7条件を検証します。`operator[]`の範囲外は未定義動作ですが、`at()`は例外で検出できます。固定長は`std::array`、実行時可変長は後の`std::vector`実習で扱います。

## 変更課題

1. 最大DamageのIndexを返す。
2. 3×3 Gridへ拡張する。
3. `std::span`で任意長配列を借用する関数を作る。
4. 空の`std::array<int,0>`で使えない操作を調べる。
