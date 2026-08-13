# AI性能・デバッグ・初稿完了チェック

AIの問題は「頭が悪い」ではなく、知覚、記憶、Path、Decision、Actionのどこで期待と違ったかへ分解します。

## 性能予算

- Perception Ray/Overlap数。
- Path Request数と展開Node。
- BT/Utility評価Node数。
- Spatial近隣Query。
- Agent数。
- FrameあたりAI時間。

平均だけでなく最大とP95/P99を測ります。

## Frame分散

AI IDから更新位相を割当て、重いPerception/Decisionを複数Frameへ分散します。ただし被弾、死亡、Target無効等のCritical Eventは即時処理します。

## Query Batch

Raycast、Nav QueryをBatch化・Job化します。JobへWorldのMutable Pointerを渡さずSnapshotとHandleを使います。完了時にRequester/World/Nav Versionを検査します。

## Cache

Path、Target Candidate、Line of Sightを短期間Cacheできますが、無効化条件が必要です。古い値を使える最大時間を仕様化します。

## AI LOD

距離だけでなく画面、戦闘参加、重要度でFull/Reduced/Dormantを選びます。Boss、Projectile回避等は遠くても重要な場合があります。

## Decision Trace

```cpp
struct DecisionTrace
{
    int tick{};
    EntityHandle agent{};
    DecisionId selected{};
    std::vector<CandidateScore> candidates{};
    DecisionReason reason{};
};
```

Ring Bufferへ保存し、問題発生前の履歴を確認します。Releaseでの容量と機密Dataに注意します。

## Visual Debug

- FOV、Ray、Memory。
- FSM/BT Current Node。
- Utility Score。
- NavMesh、Path、Steering。
- Attack Token、Slot、Role。
- Boss Phase/Pattern。

## Test

- 固定Contextに対するState遷移。
- A*の到達/不能/最適Cost。
- BT Abort時Cleanup。
- Utility境界値。
- Target破棄後Handle。
- Token漏れ。
- 同Seed再現。

## 初稿完了チェック

- [ ] 認識・記憶・判断・行動を分離する。
- [ ] PlayerとAIが共通Action APIを使う。
- [ ] 壁越し位置を無条件に知らない。
- [ ] FSM、BT、Utilityの選択理由を説明できる。
- [ ] A*、NavMesh、Steeringの責任を分ける。
- [ ] 非同期Path結果の世代・Versionを検査する。
- [ ] 集団攻撃をToken/Budgetで制御する。
- [ ] Boss PatternにTelegraphとRecoveryがある。
- [ ] 難易度をHP倍率以外でも調整する。
- [ ] Decision TraceとWorld可視化がある。
- [ ] AI時間とQuery数を計測する。

次はこれらの原理をUnreal EngineとUnityのAI機能へ接続します。
