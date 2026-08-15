# C++実習06：条件分岐

## 目的

Characterの行動決定を題材に、条件の優先順位と境界を`if`、Early Return、`switch`、三項演算子で表現します。

## 使い分け

- `if`：任意の真偽条件を判定する。
- `else if`：相互排他的な範囲を上から順に分類する。
- Early Return：致死・行動不能など、後続判定が不要な条件を先に終了する。
- `switch`：一つのEnum値ごとの処理を一覧化する。
- `条件 ? 真 : 偽`：単純な値選択に限定する。

## Visual StudioでBuild・Test・実行

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\build.ps1 -Configuration Debug -Run
```

生成後は`build/cpp_conditionals.sln`をVisual Studioで開けます。

## 期待結果

```text
選択Action: HeavyAttack
Poise Damage 75はKnockback: true
```

CTest内では死亡、Stun、各入力、Stamina境界、Hit Reaction境界の10条件を検証します。

## 重要点

死亡判定を入力判定より先に置くことで、HP 0のCharacterが攻撃する矛盾を防ぎます。条件分岐は構文だけでなく「どの規則を優先するか」というGame仕様そのものです。

## よくある失敗

- 境界で`>`と`>=`を間違える。
- 独立した`if`を並べ、複数Actionを同時実行する。
- `switch`の`break`漏れを起こす。
- 深いNestで優先順位を読めなくする。
- 複雑な三項演算子を重ねる。

## 変更課題

1. Guard Actionと必要Staminaを追加する。
2. `canAct=false`専用Decisionを追加する。
3. 全Enum値が処理されるTestを追加する。
4. 条件順を意図的に変え、死亡優先Testが失敗することを確認して戻す。
