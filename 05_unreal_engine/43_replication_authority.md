# 43 Replication、Authority、RepNotify

## 1. Serverが正式状態を所有する

```text
Client入力 → Serverへ要求 → Serverが検証・状態変更
                              ↓
                        ClientへReplication
```

Clientは表示と局所予測を行いますが、Damage、死亡、交代成立、Item取得等の正式結果はServerが決めます。

## 2. ActorとPropertyの複製

```cpp
ACombatActor::ACombatActor()
{
    bReplicates = true;
    SetReplicateMovement(true);
}

UPROPERTY(ReplicatedUsing = OnRep_CombatState)
ECombatState CombatState;

void ACombatActor::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ACombatActor, CombatState);
}
```

`bReplicates`だけでは全PropertyやComponentが自動同期されません。必要な状態を明示します。

## 3. RepNotify

```cpp
void ACombatActor::OnRep_CombatState(ECombatState PreviousState)
{
    ApplyCombatPresentation(PreviousState, CombatState);
}
```

RepNotifyはClientが新状態を受けた時の反応に使います。Serverでも同じ反応が必要なら状態変更関数を共通化し、RepNotifyがServerで自動的に同様に呼ばれると仮定しません。

## 4. Roleと所有権

- Authority：Server上の正式Actor。
- Autonomous Proxy：所有Clientが操作・予測するPawn。
- Simulated Proxy：他者を受信表示するActor。

AuthorityとActor Owner／Owning Connectionは別概念です。RPC、Relevancy、条件付きReplicationでは所有Connectionが重要です。

## 5. Relevancy、頻度、Dormancy

全Actorを全Clientへ毎Frame送らないため、距離・Owner・頻度・Dormancyで対象を絞ります。静止した宝箱等はDormantにし、状態変更時に起こします。

## 6. 高速戦闘で複製するもの

複製候補：正式HP、Combat State、Action Sequence、Targetの必要情報、Character編成、死亡。

各Clientで再生候補：Animation Pose、Niagara、音、Camera Shake。これらを駆動する小さな状態／Eventを同期します。

## 7. 配列と大量状態

頻繁に変わるItem一覧等は全配列を毎回送るよりFast Array Serializer等を検討します。差分、順序、追加削除IDを設計します。

## 8. テスト

Listen ServerだけでなくDedicated Server、複数Client、遅延、Packet Loss、途中参加で確認します。Server画面で動くこととRemote Clientで正しいことは別です。

## 参考

- [Networking Overview](https://dev.epicgames.com/documentation/unreal-engine/networking-overview-for-unreal-engine)
- [Actor Owner and Owning Connection](https://dev.epicgames.com/documentation/en-us/unreal-engine/actor-owner-and-owning-connection-in-unreal-engine)
