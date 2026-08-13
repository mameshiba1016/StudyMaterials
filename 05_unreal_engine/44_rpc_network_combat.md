# 44 RPC、所有Connection、戦闘同期

## 1. RPCの種類

- Server RPC：ClientからServerへ要求。
- Client RPC：Serverから特定Owning Clientへ。
- NetMulticast RPC：Serverから関連Clientへ実行。

RPCは一方向で戻り値を持ちません。結果はReplicated Stateや別Eventで受け取ります。

```cpp
UFUNCTION(Server, Reliable)
void ServerRequestAction(FGameplayTag ActionTag, int32 ClientSequence);

void AActionCharacter::ServerRequestAction_Implementation(
    FGameplayTag ActionTag,
    int32 ClientSequence)
{
    if (ValidateActionRequest(ActionTag, ClientSequence))
    {
        CombatComponent->TryStartAction(ActionTag);
    }
}
```

## 2. Reliableを乱用しない

Reliableは到達と順序を保証するためQueueを圧迫します。毎Frame入力や照準をReliable RPCで送らないでください。重要な低頻度要求と、失われても次Updateで置換できる高頻度Dataを分けます。

## 3. Server検証

Clientから届いたAction Tag、Target、Hitを信用しません。

- Abilityを所有しているか。
- Cooldown／Cost／State。
- Target距離とTeam。
- Action Sequenceと時間。
- Trace許容範囲。
- RPC頻度制限。

## 4. Predictionと照合

Clientは入力直後にAnimation等を予測開始し、Server承認後に継続します。拒否されたら状態、Montage、CostをRollbackします。Sequence IDでどの予測への応答か識別します。

## 5. Hit同期

Server Authority Traceが最も安全ですがLatencyを感じます。Client Hit報告を使う場合も、Server側の過去位置、距離、攻撃Windowを検証します。競技性とAction感に応じて方式を選びます。

## 6. Cosmetic Multicast

短命な演出はMulticast候補ですが、途中参加者には過去RPCが届きません。継続状態はReplicated Property／Effectで表し、RPCだけを真実にしません。

## 7. Character交代

Clientは交代希望IndexをServerへ送り、ServerがParty状態と位置を検証してPossess／状態更新します。CameraとLocal UIは所有Clientが即応できますが、正式AvatarはServer結果に合わせます。

## 8. Network Debug

Packet Lag／Lossを模擬し、Network Profiler／InsightsでRPC数とBandwidthを確認します。入力連打、Montage中断、同Frame Hit、途中参加を再現します。

## 参考

- [Remote Procedure Calls](https://dev.epicgames.com/documentation/unreal-engine/remote-procedure-calls-in-unreal-engine)
