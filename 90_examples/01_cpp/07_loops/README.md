# C++実習07：反復処理

## 目的

敵一覧への範囲Damage、集計、探索、回復Simulationを通して、Loopの選択と終了条件を学びます。

## 使い分け

- 通常`for`：Indexや回数が必要。
- Range-based `for`：Container全要素を安全に処理。
- `while`：継続条件を満たす間処理。
- `do-while`：最低一回実行。
- `continue`：現在要素の残りを飛ばす。
- `break`：現在のLoopを終了。

## Visual StudioでBuild・Test・実行

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\build.ps1 -Configuration Debug -Run
```

生成後は`build/cpp_loops.sln`をVisual Studioで開けます。

## 期待結果

```text
生存Enemy数: 1
Boss Index: 1
3 Tick後HP: 70
do-while実行回数: 1
```

CTestでは9条件を検証します。

## 重要点

`index <= size`では末尾を越えるため`index < size`を使います。状態が変化しない`while`は無限Loopになるため、回復量0の`break`と最大Tick数の二重防御を入れています。

## 変更課題

1. Bossを全件検索してIndex一覧を返す。
2. 二重Loopで敵同士の距離判定を行い計算回数を数える。
3. `continue`を使わない同等実装と可読性を比較する。
4. 安全な最大反復回数を外した場合の危険を説明する（無限Loopは実行しない）。
