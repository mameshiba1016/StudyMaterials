# 敵・ボスの設計

敵AIは最適に勝つためではなく、予兆が読め、対処でき、プレイヤーへ判断を要求するために設計します。AIアルゴリズムの詳細は`04_game_ai`で扱います。

## 感知・意思決定・行動

```text
Perception：Target位置、距離、視線、被弾
Decision  ：追跡、待機、攻撃、退却を選ぶ
Action    ：移動、Attack Stateを実行
```

三層を分けると、壁越しに常にPlayer位置を知る不自然さや、Animation中のDecision上書きを防げます。

## Enemy Context

```cpp
struct EnemyContext
{
    EntityId self{};
    std::optional<EntityId> target{};
    Vector2 lastKnownTargetPosition{};
    float distanceToTarget{};
    bool hasLineOfSight{};
    int currentTick{};
};
```

World全体を自由に触らせず、必要なSnapshotとCommand APIを渡します。

## Telegraph

強攻撃には次の段階を持たせます。

```text
Anticipation（構え・予兆）
Active（判定）
Recovery（反撃機会）
```

色だけでなく姿勢、音、エフェクトで知らせます。難易度上昇は単に予兆0や速度倍ではなく、組合せ、位置取り、Recovery差を使います。

## Attack選択

距離だけでなく、Cooldown、直前行動、地形、味方数、Player状態を条件にします。同じ技の連続を抑える履歴重みやshuffle bagを使えます。

## 攻撃予約

多数の敵が同時に攻撃すると理不尽になります。Combat Directorが近接Attack Slotや同時攻撃Tokenを配ります。

```text
Approach可能な敵は多い
Active Attack Tokenは2体まで
他は威嚇・位置調整
```

画面外攻撃の制限、飛び道具上限も設定します。

## ナビゲーション

Platformerでは、床SegmentとJump/Drop接続をGraph化し、到達可能性を探索できます。単にPlayerへ直進すると壁へ走り続けます。移動能力（Jump高さ、幅）からEdgeを生成します。

## Boss Phase

```cpp
struct BossPhaseDefinition
{
    float enterHealthRatio{};
    std::vector<AttackId> attacks{};
    float speedMultiplier{1.0F};
};
```

HP閾値を一フレームで複数跨いだ場合にPhaseを順に演出するか、直接最新へ移るかを決めます。Phase Transition中の無敵、Collider、Damage持越しも定義します。

## Boss Timelineを固定しすぎない

完全固定順は学習可能ですが単調、完全Randomは対策不能になり得ます。Pattern単位は読め、Pattern選択に制約付き変化を入れると公平性と多様性を両立できます。

## 画面外・非Active時

Camera外だから全更新停止すると、飛び道具や状態時間が止まります。

- Full：画面内・戦闘中。
- Reduced：低頻度Decision、移動は継続。
- Dormant：遠距離で状態を簡略化。

Active化時に不正位置や古いTargetを再検査します。

## 難易度

- Telegraph時間。
- 同時攻撃数。
- Aim精度。
- Recovery。
- Damage/HP。
- AI反応Delay。

HPだけ増やすと冗長になりやすいです。Assistとして受けるDamage、Parry Window、ゲーム速度等も検討します。

## デバッグ

現在状態、Target、視線、選択Score、Cooldown、Attack Token、経路、Phaseを表示します。Decision理由をログに残し、同じseed・入力で再現できるようにします。
