# コンボ・キャンセル・入力バッファ

コンボは「攻撃ボタン回数で番号を増やす」だけではありません。入力受付、遷移可能期間、派生条件、リセット条件をデータとして扱います。

## Command

```cpp
enum class CombatCommand
{
    LightAttack,
    HeavyAttack,
    Dodge,
    Jump,
    Skill
};

struct BufferedCommand
{
    CombatCommand command{};
    int issuedTick{};
    int expireTick{};
};
```

Raw Keyではなく意味Commandを保存します。リプレイ、AI、再割当へ流用できます。

## Bufferの消費

```cpp
std::optional<CombatCommand> FindConsumableCommand(
    int currentTick,
    const CancelRules& rules
);
```

期限切れを削除し、現在状態から実行可能な最も優先度の高いCommandを一度だけ消費します。実行不能でも期限まで保持するか、即破棄するかをCommandごとに決めます。

## Cancel Window

```cpp
struct CancelWindow
{
    int beginTick{};
    int endTick{};
    CommandMask allowedCommands{};
    bool requiresHit{};
};
```

範囲を`[begin, end)`へ統一すると境界が明確です。Hit Confirm時だけ次攻撃へ派生できる設定、Whiff時も可能な設定を分けます。

## Combo Graph

```text
Light1 ─Light→ Light2 ─Light→ Light3
   └Heavy→ Launcher
Light2 ─Dodge→ DodgeAttack
```

各NodeがAttackDefinition、EdgeがCommand・条件・Windowを持ちます。巨大switchよりデータ化しやすい一方、循環、到達不能Node、存在しないAttackをロード時に検証します。

## 優先順位

同tickにAttackとDodgeが入力された場合の優先順位を仕様化します。一般例：

```text
Forced State > Dodge/Defense > Skill > Heavy > Light
```

常にDodge優先が正しいとは限りません。攻撃予約をDodgeが消費するかも定めます。

## Hit Confirm

Attack Instanceへ`hasHit`、`wasBlocked`、`wasParried`等を記録し、Cancel条件へ使います。一回のAttackが複数対象へ当たった時も一貫するよう結果を集約します。

## Combo Counter

表示用Combo数と、Attack派生段数は別です。Combo Counterは一定時間内に敵へ有効Hitが連続した数、派生段数は現在Nodeです。敵が複数でも共有か対象別かを決めます。

## 同時押しとChord

複数ボタン技は入力順・許容tickを定義します。OS時刻ではなくSimulation tickへ割り当てます。単独技が先に発動してChordを奪わないよう短い保留が必要な場合がありますが、入力遅延との交換条件があります。

## コマンド入力

方向履歴を量子化し、時間制限内のパターンを新しいものから照合します。Facing反転時に前後をどう解釈するか、斜め許容、ニュートラル省略、重複Match優先度を定義します。

## Reset条件

- Recovery終了。
- 一定時間無入力。
- 被弾・死亡。
- 着地または空中移行。
- Target変更。

何でもIdleでリセットすると、着地派生や空中コンボが作れません。

## 調整用ログ

入力tick、Buffer追加・期限切れ・消費、Window開始終了、遷移成功/拒否理由を記録します。「押したのに出ない」を感覚ではなく証拠で調べられます。
