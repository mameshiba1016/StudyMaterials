# C++実習12：列挙型

## 目的

`enum class`による型安全な状態表現、基底型、`switch`、明示的な型変換、ビットフラグを、ゲームの戦闘状態を題材に確認します。

## ファイルの役割

- `include/BattleTypes.h`：列挙型と外部から利用する関数・演算子を宣言します。
- `src/BattleTypes.cpp`：文字列変換と状態遷移規則を定義します。
- `src/main.cpp`：状態遷移、属性、状態異常フラグを実際に使用します。
- `tests/BattleTypesTests.cpp`：通常経路と拒否経路を11条件で検証します。

`.h`は「何を利用できるか」を公開し、`.cpp`は「どう処理するか」を隠します。`constexpr`の短いフラグ関数は、呼び出し側でもコンパイル時評価できるようHeaderへ定義しています。

## Visual Studioでビルド・テスト・実行

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\build.ps1 -Configuration Debug -Run
```

生成後は`build/cpp_enumerations.sln`をVisual Studioで開き、`enumerations`をスタートアッププロジェクトに指定して実行できます。

## 期待結果

```text
現在状態: Attacking
要求状態: Dodging
遷移可能: true
攻撃属性: Electric
毒状態: true
凍結状態: false
毒解除後: false
```

## 読み方の要点

- `ActionState::Idle`の`::`は、`ActionState`のScope内にある`Idle`を選ぶ記号です。
- `enum class`は整数へ暗黙変換されないため、無関係な列挙型や整数との比較ミスを防ぎます。
- `: std::uint8_t`は基底型の指定です。ただし構造体のPaddingやABIまで自動的に固定するものではありません。
- 列挙値を保存データへ直接書く場合、後から番号を変更すると互換性が壊れます。永続化形式では値を明示してVersion管理します。
- Flag用の値は`1, 2, 4, 8...`のように各bitが重ならない値へします。
- 全列挙子を処理する`switch`では安易に`default`を置かず、Compilerの未処理警告を利用する設計があります。
- 外部入力からの整数を`static_cast`しても、その値が正規の列挙子である保証はありません。境界で検証が必要です。

## よくある失敗とデバッグ

- `ActionState state = 0;`は`enum class`では不正です。目的が明確な場合だけ`static_cast<ActionState>(0)`を使います。
- Flagへ`3, 5, 6`のような複数bitの値を単独列挙子として混ぜると判定が曖昧になります。
- 新しい状態を追加したら`ToString`、状態遷移、テスト、保存形式を一緒に見直します。
- Test失敗時は`build/Testing/Temporary/LastTest.log`とVisual Studioの出力を確認します。

## 変更課題

1. `ActionState::Dead`を追加し、他状態へ戻れない遷移規則を作る。
2. `operator&`と`operator|=`を型安全に実装する。
3. `StatusEffect`の有効な状態異常数を数える関数を追加する。
4. 外部から受け取った整数が有効な`Element`か検証する関数を作る。

