# ゲームAIの全体構造

AIを一つの巨大な`Update()`へ書かず、認識、記憶、意思決定、行動へ分けます。

```text
World Snapshot
→ Perception
→ Memory / Blackboard
→ Decision
→ Action Request
→ Character Controller / Combat System
```

AIはTransformやDamageを直接書き換えず、人間入力と同じCommand/Action APIを利用します。

## Context

```cpp
struct AiContext
{
    EntityHandle self{};
    std::optional<EntityHandle> target{};
    Vector3 selfPosition{};
    Vector3 lastKnownTargetPosition{};
    float targetDistance{};
    bool hasLineOfSight{};
    int simulationTick{};
};
```

World全体への無制限参照ではなく、必要なSnapshotとQueryを渡します。非同期Job中にWorld Objectを直接読む事故を防ぎます。

## 更新頻度

Animation・Movementは毎tick必要でも、遠い敵のDecisionは毎tick不要な場合があります。

```text
Perception：5～10 Hz
Decision  ：2～10 Hz
Movement  ：Simulation tick
Animation ：描画/Simulation要件
```

全敵を同Frameで更新せず、ID等で位相をずらします。重要な被弾・Target死亡Eventは低頻度更新を待たず即通知します。

## Command

```cpp
struct AiCommand
{
    Vector3 desiredMoveDirection{};
    bool requestLightAttack{};
    bool requestDodge{};
    bool requestGuard{};
};
```

AIとPlayerが同じCharacter Actionを通れば、Collision、Cancel、Animation規則を二重実装しません。

## 決定性

乱数Seed、候補順、Event順、同Score時Tie-breakを固定します。再現用に入力、AI Decision、World要約を記録します。

## Data-driven

視野、反応Delay、Attack距離、Cooldown、重みを設定Dataへ置きます。型・単位・範囲をLoad時検証し、文字列名の誤りを見逃しません。

## AI LOD

- Full：画面内戦闘。
- Reduced：低頻度知覚・Decision。
- Dormant：Eventだけ受ける。

復帰時にTarget、Path、Cooldown、位置妥当性を再検査します。

## 公平性

AIが内部のPlayer入力、Camera外情報、壁越し位置を無条件で知ると不公平です。知覚情報とGame Design上許された情報を区別します。

## デバッグ

状態、Target、記憶、選択候補、Score、Path、Cooldown、更新LOD、Decision理由をWorld上へ表示します。
