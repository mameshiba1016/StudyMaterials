# Combat Director・集団戦闘AI

各敵が個別に最善攻撃を選ぶと、全員が同時攻撃して理不尽になります。Combat DirectorがEncounter全体の攻撃量、位置、役割を調整します。

## Attack Token

```cpp
struct AttackToken
{
    TokenId id{};
    EntityHandle owner{};
    AttackCategory category{};
    int expireTick{};
};
```

攻撃開始前にTokenを取得し、終了・中断・死亡でRAII的に返却します。Timeoutで漏れを回収します。

## Budget

```text
同時近接攻撃：2
同時遠距離攻撃：1
画面外攻撃：0または予告付き1
Heavy Attack Cost：2 points
Light Attack Cost：1 point
```

難易度、Player状態、敵数でBudgetを調整します。

## Slot

Player周囲へ角度・距離のCombat Slotを配置し、敵が予約します。NavMeshへProjectし、壁・他敵・Camera可視性を検査します。

```text
Front Left / Front Right / Side / Rear
```

予約Timeout、到達不能、Player移動による再配置を扱います。

## Role

- Attacker：Tokenを持ち攻撃。
- Approacher：Slotへ移動。
- Support：遠距離・Buff。
- Waiter：威嚇、周回。

役割を固定しすぎず、Cooldownと公平性を保って交代します。

## Telegraph Queue

同時に複数の強いTelegraphが重ならないよう開始時刻を調整します。音の識別性も含めます。

## Camera外

画面外からの攻撃を制限し、Camera内へ入る移動や音による予告を優先します。World SpaceだけでなくScreen ProjectionをDirectorが参照します。

## Player Pressure

連続被弾、低HP、Dodge直後等に短い猶予を与えられます。ただし露骨な手加減を避け、Recoveryや位置調整として表現します。

## Spawn Director

同時敵数、Wave、Spawn位置を管理します。Player視界内の突然Spawn、Nav到達不能、全敵同時Activeを避けます。

## 死亡・中断

敵が死亡・HitStun・Scene退出したらToken/Slot/Roleを確実に解放します。Handle無効化だけに頼らずDirectorのCleanup Phaseを持ちます。

## Debug

Budget、Token所有者、Slot、Role、待機理由、Screen内外、Pressure値、攻撃Timelineを表示します。
