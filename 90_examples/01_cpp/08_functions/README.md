# C++実習08：関数

## 目的

Damage計算を小さな関数へ分割し、宣言・定義、値渡し、`const`参照、戻り値、Overload、Default引数、`noexcept`を確認します。

## Visual StudioでBuild・Test・実行

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\build.ps1 -Configuration Debug -Run
```

生成後は`build/cpp_functions.sln`をVisual Studioで開けます。

## 期待結果

```text
Raw Damage: 225
Reduced Damage: 195
Final Damage: 195
Hero dealt 195 damage.
Simple Overload: 30
```

CTestでは10条件を検証します。

## 関数設計

- 小さい数値は値渡しで独立Copyを渡す。
- 大きいObjectを読むだけなら`const T&`で借用する。
- 所有する結果は戻り値で返す。
- 一つの関数は一つの明確な責務へ寄せる。
- Default引数は宣言側へ一度だけ書く。
- `noexcept`は例外を投げない契約であり、違反すると`std::terminate`になる。
- Overloadは引数から意味が明確な場合に使う。

## よくある失敗

- Local変数への参照/Pointerを返してDanglingにする。
- 出力引数を増やし、何が変更されるか不明にする。
- Default引数を宣言と定義の両方へ書く。
- 暗黙変換により意図しないOverloadを選ぶ。
- 巨大関数へ全処理を詰め込みTest不能にする。

## 変更課題

1. 属性耐性を計算する関数を追加する。
2. Damage上限を呼出し側から指定する。
3. 不正入力を値の正規化ではなくErrorで返す版を設計する。
4. Visual StudioのCall Stackで関数呼出し順を確認する。
