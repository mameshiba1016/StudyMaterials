# Boss AI・Phase・Pattern

Boss AIは単にHPとDamageが大きい敵ではありません。予兆、Pattern、反撃機会、Phase変化を通じて学習と対処を作ります。

## Phase

```cpp
struct BossPhaseDefinition
{
    PhaseId id{};
    float enterHealthRatio{};
    std::vector<PatternId> allowedPatterns{};
    float speedMultiplier{1.0F};
};
```

HPを一度に大きく減らして複数Thresholdを跨いだ場合、全Transitionを順に実行するか最新Phaseへ移るかを仕様化します。

## Phase Transition

```text
現在Actionを終了/中断
→ Damage受付規則変更
→ 位置・Camera Framing
→ Transition演出
→ 新Phase設定適用
→ 戦闘再開
```

無敵、Hit Stop、Player操作、召喚Entity、途中Skipを定義します。Animation Eventだけを完了条件にしません。

## Pattern

Patternは複数Actionの読めるまとまりです。

```text
Approach → Sweep → Delay → Slam → Recovery
```

Pattern内は学習可能、Pattern選択は状況依存で変化させます。完全Randomでは予測不能、完全固定では単調になり得ます。

## Pattern選択

距離、角度、直前Pattern、Cooldown、Arena位置、Player状態、Phaseから候補をFilter/Scoreします。同じPattern連続禁止、shuffle bag、最低出現回数を使えます。

## TelegraphとRecovery

強い攻撃ほど姿勢、音、Effectで明確なTelegraphを与えます。Active後のRecoveryをPlayerの反撃Windowにします。難易度を上げてもTelegraphを完全に消さず、組合せや判断量で難しくします。

## Arena

Boss自身だけでなくArena hazard、破壊物、壁、CameraをEncounter Controllerが管理します。Boss StateからScene Objectを直接探さず安定Handleと専用APIを使います。

## Summon

同時数、Spawn位置、Nav有効性、死亡Cleanup、Boss死亡時処理をDirectorが管理します。召喚中もBoss攻撃Budgetを調整します。

## Stagger・Break

HPとは別のGaugeを持ち、Break時に反撃Windowへ移ります。Gauge回復、Phase持越し、Overkill、Break中の再Breakを定義します。

## Cutscene耐性

演出中もScene終了、Retry、Pause、Asset失敗が起こり得ます。Boss LogicとPresentation Timelineを分離し、安全なSkip/Reset Pointを用意します。

## Debug

Phase、Pattern候補/Score、現在Step、Telegraph/Active/Recovery tick、Cooldown、Break、Arena EventをTimeline表示します。
