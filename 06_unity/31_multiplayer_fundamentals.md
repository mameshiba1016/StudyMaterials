# Multiplayer基礎・同期・予測・権限設計

> 対象: Unity 6.0、Netcode for GameObjects 2.xを具体例にします。APIはpackage versionで変わるため、原理と導入versionの公式資料を併読してください。

## 1. Multiplayerは同じSceneを共有する機能ではない

各machineは別々のmemory、clock、physics world、Sceneを持ちます。networkを通って届くのは選んでserializeしたdataだけです。

```text
Client A memory ── packet ── Server memory ── packet ── Client B memory
```

`GameObject`参照、delegate、Coroutine、Collider接触状態が自動で他machineへ移ることはありません。

## 2. 難しさの正体

- 通信には遅延がある。
- packetは失われ、重複し、順序が入れ替わる場合がある。
- bandwidthには上限がある。
- clientは信用できない。
- clockは一致しない。
- 同じfloat計算・physicsが完全一致するとは限らない。
- 接続・切断は任意の時点で起こる。

single-playerの「今このframeの真実」を全員へ即時共有できないのが出発点です。

## 3. 用語

- Host: serverとclientを同一processで動かす形。
- Dedicated Server: 描画を持たない独立server process。
- Client: 入力と表示を担当し、serverへ接続。
- Authority: あるstateを最終決定する権限。
- Ownership: network objectを誰に関連付けるか。
- Replication: authoritative stateを他peerへ複製。
- RPC: 別peer側で処理要求を届ける仕組み。
- Snapshot: あるtick時点のworld state。

Ownershipとauthorityは同義とは限りません。所有clientの入力を受けても、damageの最終権限はserverに置けます。

## 4. Topology

### Client-Server

```text
Client A ─┐
Client B ─┼→ Authoritative Server
Client C ─┘
```

不正耐性と整合性を作りやすい一方、server費用と遅延があります。

### Host/Listen Server

一人のclientがserverも担当します。導入しやすい一方、host有利、host切断、migration、性能差が問題になります。

### Distributed Authority / Peer系

authorityを複数参加者へ分散します。用途次第で遅延を減らせますが、競合・不正・host migrationの設計が複雑です。

## 5. Unityの主な選択肢

- Netcode for GameObjects: GameObject/MonoBehaviour向け高level SDK。
- Netcode for Entities: DOTS向け、client predictionを含むserver-authoritative基盤。
- Unity Transport: UDP/WebSocket等の低level通信層。
- Multiplayer Services: Session、Lobby、Matchmaker、Relay等。
- Dedicated Server: server build workflow。
- Multiplayer Tools: network simulator/profiler等。
- Multiplayer Play Mode: 同一開発機で複数playerを試験。

LobbyやRelayはgameplay state同期そのものではありません。

## 6. Session接続とGameplay同期を分ける

```text
Services初期化
→ Authentication
→ Lobby/Matchmaking/Join Code
→ endpoint/Relay割当
→ Netcode接続
→ player承認
→ gameplay Scene load
→ world snapshot同期
```

「Lobbyへ参加できた」と「game worldへspawn済み」は別状態です。

## 7. Server Authority

高速actionでも基本は:

```text
Client: 入力commandを送る
Server: 実行可否・移動・命中・damageを決定
Clients: 結果を表示
```

clientから「敵に100 damageを与えた」と送らせず、「tick 120でAttack Aを押した」と送ります。

## 8. 信頼境界

clientから届く全値を検証します。

- 送信者がそのplayerを操作できるか。
- command sequence/tickが妥当か。
- 連打頻度が上限内か。
- stamina/cooldown/stateが許可するか。
- position差が許容範囲か。
- targetが存在し、攻撃範囲内だったか。
- payload size/string長が制限内か。

RPCが呼べることは、その要求が正しいことを意味しません。

## 9. Network Tick

描画frameとsimulation/network tickを分けます。

```text
Render:  60～144 fpsで可変
Sim:     60 tick/sなど固定
Network: 20～60 send/sなど
```

tick番号を付けると入力、snapshot、ack、巻き戻しを同じ時間軸で扱えます。

```csharp
public readonly record struct InputCommand(
    uint Tick,
    ushort Sequence,
    Vector2 Move,
    bool AttackPressed,
    bool DodgePressed);
```

## 10. Sequence number

連番で重複・古いcommandを検出します。固定幅整数はwrap-aroundするため単純な`a > b`だけで比較しない実装が必要です。

commandにはclient時刻だけでなくserverが対応付けるtickを持たせ、未来過ぎ・過去過ぎを拒否します。

## 11. Snapshot

```csharp
public readonly record struct PlayerSnapshot(
    uint ServerTick,
    Vector3 Position,
    Quaternion Rotation,
    Vector3 Velocity,
    byte CombatState,
    ushort ActionId,
    ushort Health);
```

毎回全componentを送らず、表示・予測・判定に必要な最小stateを選びます。

## 12. Serialization

network dataはbyte列へ変換されます。

- field順・型・versionを一致させる。
- 文字列・Listを無制限に送らない。
- `int`で足りる値へ`string` IDを送らない。
- positionを必要精度へquantizeする。
- optional fieldをbit mask化する。
- endian/formatはlibraryの契約に従う。

Inspectorで見えるclassを丸ごとserializeする発想を避けます。

## 13. Bandwidthの概算

```text
bytes/packet × packets/second × recipients
```

50 bytesのsnapshotを30Hzで9人へ送ると、header等を除いても一objectあたり約13.5KB/sです。object数とprotocol overheadを含めてbudget化します。

## 14. ReliableとUnreliable

### Reliable

重要eventを再送・順序保証しますが、失われたpacket待ちで後続が遅れる場合があります。

用途例: inventory確定、Scene遷移、spawn/despawn。

### Unreliable

古いdataを待たず最新へ進めます。

用途例: 高頻度position/snapshot、照準方向。

「重要そうだから全部Reliable」は遅延増加を招きます。

## 15. EventとState

- State: 現在HP、door open、position。後参加者にも必要。
- Event: 一回の音、impact、animation trigger。

重要eventを一瞬のboolだけで送るとpacket lossやlate joinで失います。持続する結果はstateへ反映し、presentation eventは重複許容IDを付けます。

## 16. Idempotency

同じmessageが二度届いても結果が二重適用されない性質です。

```text
DamageEvent { AttackInstanceId, VictimId, Amount }
```

Victim側は処理済み`AttackInstanceId`を短期間記録し、重複を無視できます。通貨・購入はtransaction IDを永続管理します。

## 17. NetworkObjectとNetworkBehaviour

Netcode for GameObjectsではnetwork identityを持つobjectに`NetworkObject`、network callback/RPC/stateを扱うcomponentに`NetworkBehaviour`を使います。

通常の`Instantiate`だけではnetwork spawnになりません。誰がspawnを許可され、prefabが登録済みか、despawnと破棄の関係を確認します。

## 18. Spawn Authority

server-authoritative構成ではserverがnetwork objectをspawn/despawnします。

```csharp
public sealed class ServerProjectileSpawner : NetworkBehaviour
{
    [SerializeField] private NetworkObject projectilePrefab;

    public void SpawnValidatedProjectile(Vector3 position, Quaternion rotation)
    {
        if (!IsServer)
            return;

        NetworkObject instance = Instantiate(projectilePrefab, position, rotation);
        instance.Spawn(); // network IDを割り当て、接続clientへspawnを通知。
    }
}
```

実際には入力所有者、cooldown、位置、方向もserverで検証します。

## 19. Ownership

`IsOwner`でlocal入力・camera・AudioListenerなどを有効化できます。ただしownerだから任意のNetworkVariableを書けるとは限らず、設定されたwrite permissionとauthorityに従います。

```csharp
public override void OnNetworkSpawn()
{
    localInput.enabled = IsOwner;
    localCamera.enabled = IsOwner;
}
```

Awake時点ではnetwork spawn/ownershipが確定していない場合があるためnetwork lifecycleを使います。

## 20. RPC

RPCは「処理要求/通知」です。過去の接続者へ自動再現されるstate保存ではありません。

```csharp
[Rpc(SendTo.Server)]
private void SubmitAttackRpc(ushort actionId, uint clientTick)
{
    // このmethodがserverで動いても、入力はclient由来なので信用しない。
    if (!ValidateAttackRequest(actionId, clientTick))
        return;

    authoritativeCombat.TryStart(actionId);
}
```

RPC attribute/APIはNetcode package versionに合わせて確認します。

## 21. RPC payloadの原則

- GameObject参照でなくNetworkObject ID/NetworkObjectReference。
- 大きなassetでなく安定ID。
- client計算済みdamageでなく入力意図。
- 可変長payloadに上限。
- sender identityはtransport/network contextから取得し、payloadの自己申告を信用しない。

## 22. NetworkVariable

持続stateの同期に使います。read/write permission、初期同期、change callback、late joinを考えます。

```csharp
private readonly NetworkVariable<int> health = new(
    100,
    NetworkVariableReadPermission.Everyone,
    NetworkVariableWritePermission.Server);

public override void OnNetworkSpawn()
{
    health.OnValueChanged += OnHealthChanged;
    OnHealthChanged(health.Value, health.Value); // 初期表示も明示的に更新。
}

public override void OnNetworkDespawn()
{
    health.OnValueChanged -= OnHealthChanged;
}
```

## 23. Dirty stateと送信頻度

値が変わるたびに必ず即時packetになるとは限りません。network tickで差分がまとめられることがあります。途中の全変化が必要ならevent/sequence、最終値だけならstate同期を使います。

## 24. Late Join

途中参加者は過去RPCを見ていません。現在worldを復元できるstateが必要です。

- 現在player一覧。
- HP/state/phase。
- destructible状態。
- 現在Scene/session phase。
- 残存projectile/敵。
- timerのserver基準終了時刻。

「最初からいたclientだけ正しく見える」はstate設計不足です。

## 25. Scene同期

network Scene遷移はserverが進行を統制し、全clientのload完了を追跡します。localに`SceneManager.LoadScene`しただけでは同期されません。

```text
Serverがtransition開始
→ gameplay入力停止
→ 各client load
→ ready acknowledgement
→ player/world spawn
→ countdown
→ simulation開始
```

timeout、失敗、途中切断も扱います。

## 26. Interpolation

remote characterは受信snapshotを少し過去にbufferして補間します。

```text
受信: tick 100 -------- tick 103
表示時刻:      tick 101.5 を補間
```

遅延を少し増やす代わりにjitterとpacket間の飛びを滑らかにします。

```csharp
public static Vector3 Interpolate(
    in PlayerSnapshot older,
    in PlayerSnapshot newer,
    double renderTick)
{
    double span = newer.ServerTick - older.ServerTick;
    float t = span <= 0 ? 1f : (float)((renderTick - older.ServerTick) / span);
    return Vector3.LerpUnclamped(older.Position, newer.Position, t);
}
```

実装ではtick wrap、teleport、欠損、buffer上限を扱います。

## 27. Extrapolation

新snapshotが来ない間、velocityから未来位置を推定できますが、方向転換時に外れます。外挿時間へ上限を設け、超えたら停止/補間buffer維持などに切り替えます。

## 28. Client-side Prediction

local playerは入力直後に自分でsimulationし、server確認を待ちません。

```text
Client tick 120: inputを保存してlocal simulate
→ Server: tick 120をauthoritative simulate
→ Client: server state受信
→ 差があればtick 120へ戻す
→ 121以降の未確認inputを再simulation
```

見た目の遅延を減らしますが、再実行可能なsimulationが必要です。

## 29. Prediction buffer

```csharp
public readonly record struct PredictedFrame(
    uint Tick,
    InputCommand Input,
    Vector3 Position,
    Vector3 Velocity,
    byte State);
```

ring bufferへ一定tick数だけ保存します。server ackより古いframeを捨て、遅延上限を超えた場合のfull correctionを用意します。

## 30. Reconciliation

```text
authoritative positionとの差 < epsilon: 何もしない
差が小さい: visualだけ滑らかに補正
差が大きい: simulation stateを即修正し、未確認inputを再生
teleport: interpolationを切りsnap
```

simulation transformとvisual transformを分けると、正しさを即修正しつつ見た目を滑らかにできます。

## 31. Determinism

同じ入力から完全に同じ結果が必要なpredictionでは:

- 固定tick。
- 更新順。
- random seed。
- float差。
- Physics engineの非決定性。
- object iteration順。

が問題になります。Unity Physicsをそのまま全platform完全決定論として仮定しません。server correctionを前提にするか、限定した独自motorを使います。

## 32. Remote playerは予測対象か

通常、local ownerはprediction、remote playerはsnapshot interpolationです。他playerの入力を知らないため完全予測できません。短い外挿はできますが、誤差を許容します。

## 33. Lag Compensation

射撃clientが見た過去時点へserverのhit targetを巻き戻して判定する仕組みです。

```text
Client: server tick 200相当を見て射撃
Server現在: tick 206
Server: target collider historyをtick 200へ一時復元
→ ray判定
→ 現在状態へ戻す
```

client申告tickを無制限に信用せず、巻き戻し可能windowを制限します。

## 34. Meleeの補償

近接でも攻撃者・victimのhistoryから判定できますが、どちらの見え方を優先するか難しいです。

- attacker favor: 当てた感覚は良いがvictimには遠くから当たる。
- defender favor: 回避感覚は良いがattackerには命中消失。
- hybrid: 小範囲だけ補償、無敵tickはserver authority。

game designとして決めます。

## 35. Network Combat Action

攻撃をanimation trigger一個で同期せず、安定したaction IDと開始server tickを送ります。

```csharp
public readonly record struct NetworkActionState(
    ushort ActionId,
    uint StartTick,
    byte Phase,
    ushort Sequence);
```

受信側は現在server tickとの差からanimation normalized timeを推定できます。

## 36. Damage Authority

```text
Client input
→ server action validation
→ server hit detection / lag compensation
→ server DamageResolver
→ authoritative HP更新
→ result replication
→ clientsがHit VFX/UI表示
```

第29章の`DamageRequest/Result`をserver simulationで再利用し、network層へdamage規則を重複させません。

## 37. Projectile

方式を選びます。

- serverだけsimulationしsnapshot表示。
- ownerがpredicted projectile、serverが確認。
- deterministic trajectoryをstart stateから各client再生。
- visual projectileとauthoritative hit queryを分離。

大量projectileを全てNetworkObjectにするとspawn/bandwidth costが増えるため、seed・start tick・trajectory IDで再構築する手法もあります。

## 38. Animation同期

全Animator parameterを毎tick同期しません。gameplay action/state、速度、開始tickを送り、各clientで表示を構築します。

transitionの途中参加、speed変更、hit reaction上書き、Root Motion authorityを決めます。Animation Eventをauthoritative damageへ使いません。

## 39. Physics Authority

Rigidbodyを全peerで自由にsimulateすると分岐します。

- server simulates、client interpolates。
- owner predicts、server corrects。
- cosmetic physicsはlocal限定。

樽の重要damage判定はserver、破片の見た目は各client localなど、重要度で分けられます。

## 40. Network Time

localの`Time.time`をsession共通時刻として使いません。Netcodeの同期clock/server timeを使い:

```text
matchEndServerTime = currentServerTime + 180 seconds
remaining = matchEndServerTime - estimatedServerTime
```

とすれば、毎秒timer値を送らずに表示できます。clock補正時のjumpも平滑化します。

## 41. Pause

online matchで一clientの`Time.timeScale = 0`がserverを止めることは通常ありません。menuを開いてもlocal入力だけ停止し、simulation/network受信は継続します。HostとDedicatedでも挙動を一致させます。

## 42. Disconnect

切断は例外でなく通常遷移です。

- matchmaking中。
- Scene load中。
- spawn直後。
- attack中。
- host切断。
- server shutdown。

UI理由、object cleanup、slot/token解放、party復帰、再接続tokenを扱います。

## 43. Reconnection

connection IDは再接続後に変わり得ます。player account/session player IDと一時connectionを分けます。

```text
authenticate
→ reconnect token検証
→ 以前のplayer stateを再関連付け
→ current snapshot送信
→ client ready
```

切断中もcharacterを残すか、AI代理にするか、即消すかをgame rule化します。

## 44. Host Migration

Host型でhostが抜けた場合、新hostへ:

- authoritative world state。
- object ownership。
- random state。
- timers/ticks。
- connection再確立。

を引き継ぐ必要があります。libraryが自動提供する範囲を確認し、非対応ならsession終了も明示的な選択肢です。

## 45. Relay

Relayは参加者間を中継し、直接IP公開やNAT traversalの問題を軽減します。ただしDedicated authoritative serverの代わりとは限りません。Hostがauthorityならhost advantage/切断問題は残ります。

## 46. Lobby・Matchmaker

- Lobby: room metadata、member、検索・join。
- Matchmaker: 条件から適切なmatch/serverへ割当。
- Session: 接続参加者とnetwork接続をまとめる抽象。

Lobby dataを高頻度gameplay synchronizationに使いません。

## 47. Interest Management

各clientにworld全体を送らず、関心範囲だけreplicateします。

- 距離/grid。
- team。
- room/portal。
- visibility。
- gameplay relevance。

範囲外objectをdespawn扱いにするか、休眠stateとして保持するかで再出現処理が変わります。

## 48. Delta Compression

前snapshotとの差だけ送ると帯域を削減できますが、基準snapshotを失った場合に復元できません。ackされたbaseline、定期full snapshot、sequence管理が必要です。高level libraryが行う範囲をProfilerで確認します。

## 49. Quantization

arena内positionを32-bit float×3でなく、範囲内整数へ量子化できます。

```text
x range -128..128m, precision 1cm
→ 25600段階なので15bit程度
```

精度、範囲外clamp、origin shiftを仕様化します。見た目の補間が量子化stepを隠せる場合もあります。

## 50. Security

- client build内の秘密鍵を秘密とみなさない。
- serverでrate limit。
- packet/RPC size上限。
- authentication/authorization分離。
- server logへplayer/action/tick/reason。
- sensitive dataを不要に送らない。
- dependency/packageを更新。
- denial-of-serviceを想定した早期拒否。

暗号化があってもclientの嘘が正しくなるわけではありません。

## 51. Connection Approval

接続時にprotocol version、build/content version、auth token、定員、ban、session phaseを検証します。passwordやtokenを平文logへ出しません。approval処理を長時間blockせずtimeoutを設けます。

## 52. Version Compatibility

client/serverの:

- protocol version。
- game build version。
- content/catalog version。
- serialization schema。
- gameplay data hash。

を確認します。field追加で古いclientがbyte列を誤読しないversioningを行います。

## 53. Network Error Handling

「接続失敗」一種類にせず:

- timeout。
- version mismatch。
- authentication failure。
- session full。
- server shutdown。
- kicked。
- relay allocation失効。

を内部reason codeと安全なuser messageへ分けます。

## 54. Testing Matrix

- Host + 1 client。
- Dedicated Server + 複数client。
- late join。
- client/server別frame rate。
- 50/100/200ms latency。
- jitter。
- packet loss/reordering。
- 一時切断/再接続。
- Scene loadが遅いclient。
- version mismatch。
- malicious RPC頻度/値。

localhostの0ms環境だけで完成判定しません。

## 55. Multiplayer Play ModeとTools

Unity 6のMultiplayer Play Modeでは一台で複数playerを試験できます。Multiplayer Toolsのnetwork simulator/profiler等を使い、latency、jitter、loss、message量を確認します。

Editor内試験後、必ず実build・別process・可能なら別machineでも確認します。

## 56. Network Metrics

記録する例:

- RTT/latency。
- jitter。
- packet loss。
- bytes/second in/out。
- message/RPC種別別bytes。
- snapshot age/interpolation buffer。
- prediction error/correction距離。
- server tick time。
- connected player数。

平均だけでなくpercentileとpeakを見ます。

## 57. Logging

```text
[ServerTick=4812]
[Connection=17]
[Player=AccountHash]
[Command=Attack]
[Result=Rejected]
[Reason=Cooldown]
```

個人情報やtokenを含めず、時系列を再構成できる構造化logにします。client logだけでauthoritative原因を判断しません。

## 58. ReplayとNetwork Debug

入力commandとauthoritative snapshot/eventを記録すると、desyncや不正疑いを再生できます。protocol version、random seed、gameplay data hashも保存します。

映像replayとdeterministic simulation replayは別物です。

## 59. Architecture例

```text
Game.Combat.Domain
  InputCommand / CombatAction / DamageResolver

Game.Network.Contracts
  IDs / serialized messages / protocol version

Game.Network.Server
  validation / authority / world simulation

Game.Network.Client
  input send / prediction / interpolation / presentation

Game.Network.Unity
  NGO adapter / Transport / NetworkObject mapping
```

domain ruleから`NetworkBehaviour`を直接参照しないことでoffline、server test、別network backendへ再利用しやすくします。

## 60. 最小実装順

1. Hostとclientの接続・切断UI。
2. NetworkObject spawn/ownership。
3. server-authoritative一変数。
4. 入力commandとserver移動。
5. remote snapshot interpolation。
6. local prediction/reconciliation。
7. action ID/start tick同期。
8. server damage/hit validation。
9. late join/Scene transition。
10. latency/loss試験、metrics、security validation。

一度にLobby、Relay、prediction、Boss戦を統合せず、小さいvertical sliceで検証します。

## 61. よくある失敗

### Transformを毎frameReliable送信

帯域とhead-of-line delayを増やす。snapshot頻度、unreliable、補間を設計する。

### Clientがdamageを決定

改ざん可能。clientは入力を送りserverが解決する。

### RPCだけでworldを構築

late joinが過去eventを知らない。持続結果をstate化する。

### Update frame番号を共通tick扱い

machineごとにframe rateが違う。network/simulation tickを使う。

### Remoteもlocal同様に直接操作

packet間で震える。remoteはsnapshot bufferから補間する。

### Hostだけでtest

同一processの低遅延で問題が隠れる。Dedicated/remote条件を試す。

### Lobbyをgame state同期へ使用

用途・更新頻度が違う。Netcode replicationを使う。

## 62. 完成確認表

- [ ] authorityとownershipを各stateに定義した。
- [ ] client入力をserverで検証する。
- [ ] render/simulation/network tickを区別した。
- [ ] Reliable/Unreliableを用途別に選んだ。
- [ ] RPC eventとpersistent stateを区別した。
- [ ] late joinだけでもworldを復元できる。
- [ ] remote entityをsnapshot補間する。
- [ ] local predictionの履歴とreconciliationがある。
- [ ] correctionとteleportを区別した。
- [ ] damage/HPはserver authorityである。
- [ ] disconnect/reconnect中のcleanupがある。
- [ ] message sizeとrateに上限がある。
- [ ] latency/jitter/lossを注入してtestした。
- [ ] Network Profiler/metricsで帯域を計測した。
- [ ] package/protocol versionを固定・記録した。

## 63. 確認問題

1. OwnershipとAuthorityの違いは何か。
2. Lobby参加とgameplay同期開始はなぜ別状態か。
3. Reliableでpositionを高頻度送信する問題は何か。
4. StateとEventをどう使い分けるか。
5. Snapshot Interpolationが意図的に遅延を加える理由は何か。
6. Client-side PredictionとReconciliationを説明してください。
7. Unity Physicsを完全決定論と仮定できない理由は何か。
8. Lag Compensationでclient申告時刻を制限すべき理由は何か。
9. Late Joinへ過去RPCがなくてもworldを見せるには何が必要か。
10. Relayを使ってもserver authority問題が自動解決しない理由は何か。

## 64. 公式資料

- [Unity 6 Multiplayer概要](https://docs.unity3d.com/6000.0/Documentation/Manual/multiplayer-overview.html)
- [Unity 6 Netcode for GameObjects](https://docs.unity3d.com/6000.0/Documentation/Manual/com.unity.netcode.gameobjects.html)
- [Unity Multiplayer documentation](https://docs-multiplayer.unity3d.com/)
- [Unity 6 Multiplayer Play Mode](https://docs.unity3d.com/6000.0/Documentation/Manual/com.unity.multiplayer.playmode.html)
- [Unity Transport](https://docs.unity3d.com/6000.0/Documentation/Manual/com.unity.transport.html)
- [Dedicated Server](https://docs.unity3d.com/6000.0/Documentation/Manual/dedicated-server.html)

Netcode API、RPC属性、権限modeは更新されるため、導入したpackage versionのAPI referenceとchangelogを必ず確認してください。
