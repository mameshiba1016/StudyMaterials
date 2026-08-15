# C++実習13：構造体

## 目的

`struct`で関連データを型としてまとめ、メンバ変数・メンバ関数・初期化・値渡しと参照渡し・結果オブジェクトを、戦闘データを使って確認します。

## ファイルの役割

- `include/CombatData.h`：座標、戦闘能力値、ダメージ結果の型と操作を宣言します。
- `src/CombatData.cpp`：移動、ダメージ計算、データ検証を定義します。
- `src/main.cpp`：構造体を生成してゲーム処理へ渡します。
- `tests/CombatDataTests.cpp`：正常値、境界値、不正値を11条件で検証します。

## Visual Studioでビルド・テスト・実行

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\build.ps1 -Configuration Debug -Run
```

生成された`build/cpp_structures.sln`をVisual Studioで開き、`structures`をスタートアッププロジェクトに指定して実行することもできます。

## 期待結果

```text
名前: Training Drone
位置: (3.5, 2)
入力Damage: 32
適用Damage: 27
残りHP: 73
撃破: false
Data有効: true
```

## 読み方の要点

- `enemy.position.x`の`.`は、実体が持つメンバへアクセスする演算子です。Pointerでは`->`を使います。
- `struct`と`class`の中心的な言語仕様上の違いは、既定のアクセス権と既定の継承方式です。`struct`も関数、Constructor、privateを持てます。
- `CombatStats&`は呼び出し元の実体を変更する参照、`const Vector2&`はCopyせず読み取る参照です。
- `DamageResult`のように戻り値を構造体にすると、複数結果をMember名付きで返せます。
- `{}`はMember初期化子を使って安全に初期化します。初期化子のない組込み型を`{}`なしで生成すると不定値になり得ます。
- C++20の指示付き初期化子`.member = value`は、Memberの宣言順で記述します。
- `sizeof(CombatStats)`はMemberの単純合計と一致するとは限りません。Alignmentを満たすPaddingが挿入されるためです。
- Memory、File、Networkへ構造体を丸ごとBinary Copyする設計は、Padding、Endianness、ABI、Version差の影響を受けます。

## structかclassか

単純な値の集合や、全Memberを公開しても不変条件が壊れない型には`struct`が自然です。HPの範囲などを常に強制したいDomain Objectでは、Memberを`private`にした`class`と検証済み操作を検討します。これは絶対規則ではなく、型の意図を読者へ伝える慣習です。

## よくある失敗とデバッグ

- Member追加後にAggregate初期化の値の順番がずれていないか確認します。指示付き初期化子も有効です。
- 変更したい引数を値渡しにするとCopyだけが変わり、元の実体は変化しません。
- 返された構造体への参照が、関数内のLocal変数を指していないか確認します。Local変数への参照は関数終了後に無効です。
- Test失敗時は`build/Testing/Temporary/LastTest.log`を確認します。

## 変更課題

1. `HealResult`構造体と、最大HPを超えない回復関数を追加する。
2. `Vector2`へ加算演算子を実装する。
3. `CombatStats`のMember順を変え、`sizeof`とPaddingの変化を調べる。
4. HPの不変条件を強制する`Character`クラスへ発展させる。

