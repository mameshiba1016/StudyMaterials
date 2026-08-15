# C++実習03：型・変数・初期化

## 目的

Character Statusを題材に、値の意味に合う型を選び、未初期化値を残さず、不正入力を境界内へ正規化する方法を実行して確認します。

## 使用する型

- `std::int32_t`：LevelやHPなど幅を明確にした整数
- `float`：描画・移動で頻繁に使う単精度値
- `double`：長時間累積で誤差を抑えたい値
- `bool`：真偽状態
- `enum class`：属性の有限な選択肢
- `std::string`：所有権を持つ可変長文字列
- `struct`：関連値をまとめたValue Object

## 初期化の方針

- 変数は宣言と同時に初期化します。
- `{}`とMember初期化子を使い、Default構築でも有効値にします。
- `const`は初期化後に変えない値へ付けます。
- `auto`は右辺から型が明白な場合に使い、意味まで曖昧にしません。
- Narrowing Conversionを防ぐためBrace初期化を優先します。

## Visual StudioでBuild・Test・実行

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\build.ps1 -Configuration Debug -Run
```

生成後は`build/cpp_types_variables_initialization.sln`をVisual Studioで開けます。

## 期待結果

```text
名前: ActionHero
Level: 12
HP: 850
移動速度: 7.5
Player操作: true
属性: Electric
int32_tのSize: 4 bytes
int32_tの最大値: 2147483647
```

CTestはTest Executable一件を起動し、内部12条件を確認します。

## 内部処理

`CreateCharacterStats`は値を受け取り、Levelを1～100、HPと速度を0以上へ正規化します。戻り値は完全初期化された`CharacterStats`であり、呼出し側へ未初期化Memberを渡しません。

## よくある失敗

- `int value;`のまま読む：未初期化値を読むため未定義動作です。
- Signed/Unsignedを無計画に比較：負値が巨大なUnsignedへ変換され得ます。
- `float`を完全一致比較：丸め誤差を考え、用途に応じ許容誤差を使います。
- `enum`を範囲検査なしで整数Cast：未定義の列挙値が入り得ます。
- 狭い整数型へ大きい値を変換：Overflow/切捨て前にRangeを検査します。

## Debug方法

Visual Studioで`CreateCharacterStats`へBreakpointを置き、入力値、正規化後の値、`sizeof`、型表示をWatchします。

## 変更課題

1. `std::uint32_t experience`を追加し、Signed/Unsigned変換を安全に扱う。
2. HP上限`maxHealth`を追加し、`health <= maxHealth`を保証する。
3. 不正な値をClampせずErrorとして返す設計と比較する。
4. `double`と`float`で長時間Delta Timeを加算し誤差を比較する。
