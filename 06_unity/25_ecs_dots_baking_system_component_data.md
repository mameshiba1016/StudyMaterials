# ECS・DOTS・Baking・System・Component Data

> 対象: Unity 6.0、released版Entities 1.4系。Entities APIは更新が速いため、古い0.x/1.0記事ではなくProjectで固定したPackage Versionの資料を確認すること。

## 1. DOTSとECS

DOTSはdata-oriented technology群の総称で、主にEntities、C# Job System、Burst、Collections等を組み合わせます。

ECSの役割:

```text
Entity    = identity
Component = data
System    = queryしたdataへの処理
```

GameObject/MonoBehaviourを単に別名へ変える仕組みではありません。

## 2. ECSを使う理由

- 同じcomponent構成の大量entity。
- 連続memoryとcache locality。
- queryによるbatch処理。
- Job/Burstとの連携。
- data dependencyをsystem schedulingへ表す。

少数の複雑なcharacterだけならMonoBehaviourの方が簡潔な場合があります。

## 3. Entity

`Entity`はindex/version等からなる軽量handleです。class instanceそのものではありません。

- componentを持つidentity。
-破棄後の古いhandle再利用をversionで検出。
- save/network向け永続IDとは別。
- Entityを長期保存する場合はWorld/lifetimeを考える。

## 4. Component Data

```csharp
using Unity.Entities;
using Unity.Mathematics;

public struct MoveSpeed : IComponentData
{
    public float Value;
}

public struct MoveDirection : IComponentData
{
    public float3 Value;
}
```

componentはbehavior methodを詰めるobjectでなく、unmanagedなstateとして小さく明確にします。

## 5. System

Systemは必要componentを持つentityをqueryし、dataを更新します。

```text
MovementSystem:
  query LocalTransform + MoveSpeed + MoveDirection
  each entityのpositionを更新
```

dataと処理を分離することで、同じloopを連続dataへ適用しやすくなります。

## 6. Archetype

Archetypeはcomponent typeの組み合わせです。

```text
Archetype A: LocalTransform, MoveSpeed, MoveDirection
Archetype B: LocalTransform, Health, EnemyTag
```

componentの値ではなく「型の集合」で決まります。component add/removeはArchetype変更です。

## 7. Chunk

同じArchetypeのentity/component dataはchunkへまとめて格納されます。

```text
Chunk
 ├─ Entity[]
 ├─ LocalTransform[]
 ├─ MoveSpeed[]
 └─ MoveDirection[]
```

Systemが必要な列を連続走査し、cache効率を得ます。Chunk容量はcomponent size等で決まります。

## 8. Structural Change

次はentityの構造を変え、chunk間移動を起こし得ます。

- Entity create/destroy。
- component add/remove。
- shared component値変更。

頻繁に行うとsync pointやmemory copyを生みます。単なるcomponent値更新とはcostが違います。

## 9. Tag Component

```csharp
public struct EnemyTag : IComponentData { }
public struct DeadTag : IComponentData { }
```

data fieldなしでquery分類に使えます。ただしTag add/removeはstructural changeです。頻繁なstate toggleにはenableable component等を検討します。

## 10. Enableable Component

```csharp
public struct Stunned : IComponentData, IEnableableComponent
{
    public float RemainingTime;
}
```

componentをArchetypeからremoveせずenabled状態を変え、query matchingを切り替えられます。

- structural changeを避けやすい。
- enabled maskによるquery cost。
- component dataは存在したまま。
- API/Job access ruleを確認。

## 11. Buffer Element

```csharp
public struct StatusEffectElement : IBufferElementData
{
    public int EffectId;
    public float RemainingTime;
}
```

`DynamicBuffer<T>`でentityごとの可変長dataを表します。内部buffer容量を超えると外部allocationへ移るため、element sizeとInternalBufferCapacityを設計します。

## 12. Blob Asset

大量entityで共有するimmutable dataにはBlob Assetが向きます。

例:

- attack definition table。
- navigation graph。
- animation curve samples。
- immutable config。

relative pointerを持つimmutable memoryで、build/baking時に構築し、lifetimeを管理します。

## 13. Shared Component

同じ値を共有するentityをchunk groupingへ利用できますが、値の種類が増えるとchunk fragmentationを招きます。

renderer/material等の分類用途は有効でも、entityごとにunique値をshared componentへ入れません。

## 14. Chunk Component

chunk単位のdataを持つ機能があります。同じchunkのentityで共有するLOD/aggregate state等に使えますが、chunk再編成とqueryを理解してから利用します。

## 15. Cleanup Component

Entity destruction後のresource cleanupを段階的に扱うcomponent categoryがあります。external/native resourceを持つownerが最後に後始末できるようにします。

通常のdestroyだけで全external stateが自動解放されると思わないでください。

## 16. World

WorldはEntityManagerとSystemsの集合です。

```text
Default World
Server World
Client World
Test World
Preview/Baking World
```

EntityはWorldを跨いでそのまま有効ではありません。staticにEntityだけ保存しないでください。

## 17. EntityManager

Entity/componentのcreate、destroy、add/remove、read/write等を行う中心APIです。

便利ですがmain thread structural operationを大量loopで行うと遅くなります。Query、Job、ECBを使い分けます。

## 18. SystemBaseとISystem

### `SystemBase`

class basedでmanaged fieldを持て、扱いやすいがmanaged systemです。

### `ISystem`

unmanaged struct systemでBurstとの親和性が高いです。

Entities 1.4の推奨API/対応機能を確認し、必要なmanaged stateがあるかで選びます。

## 19. ISystem lifecycle

```csharp
using Unity.Burst;
using Unity.Entities;
using Unity.Transforms;

[BurstCompile]
public partial struct MovementSystem : ISystem
{
    public void OnCreate(ref SystemState state)
    {
        // 必要componentが一つもない間はsystem更新をskipする。
        state.RequireForUpdate<MoveSpeed>();
    }

    [BurstCompile]
    public void OnUpdate(ref SystemState state)
    {
        float deltaTime = SystemAPI.Time.DeltaTime;

        foreach (var (transform, speed, direction) in
                 SystemAPI.Query<
                     RefRW<LocalTransform>,
                     RefRO<MoveSpeed>,
                     RefRO<MoveDirection>>())
        {
            transform.ValueRW.Position +=
                direction.ValueRO.Value *
                speed.ValueRO.Value *
                deltaTime;
        }
    }

    public void OnDestroy(ref SystemState state) { }
}
```

`RefRO`/`RefRW`でread/write intentを表し、scheduler/Burstへdependency情報を与えます。

## 20. SystemAPI.Query

source generationを通じてquery codeを生成します。

-必要component type。
- read-only/read-write。
- optional filters。
- Entity取得。
- enabled componentの扱い。

APIはEntities Versionで変わるためcompile error時に古い記事のsyntaxを混ぜません。

## 21. WithEntityAccess

query resultと一緒にEntity handleが必要な場合に使えるquery modifierがあります。

```csharp
foreach (var (health, entity) in
         SystemAPI.Query<RefRO<Health>>()
                  .WithEntityAccess())
{
    // entityをECB commandのtarget等に使う。
}
```

具体的なtuple order/syntaxは固定Package Versionで確認します。

## 22. Query filter

- `WithAll<T>`。
- `WithAny<T>`。
- `WithNone<T>`。
- change filter。
- shared component filter。
- enabled state。

filterを細かくし過ぎるとquery/archetype管理が複雑になります。意図をsystem名とtestに残します。

## 23. Change Filter

componentが変更されたchunkだけ処理するoptimizationです。

注意:

- entity単位でなくchunk単位のversion判定。
- RW accessを取るだけでchange versionへ影響し得る。
-「値が実際に違う」ことと同義ではない。

更新頻度が低いdataに有効ですが、誤解すると余計な処理が残ります。

## 24. IJobEntity

```csharp
[BurstCompile]
public partial struct IntegrateMovementJob : IJobEntity
{
    public float DeltaTime;

    private void Execute(
        ref LocalTransform transform,
        in MoveSpeed speed,
        in MoveDirection direction)
    {
        transform.Position +=
            direction.Value * speed.Value * DeltaTime;
    }
}
```

Execute signatureからqueryが生成されます。SystemからScheduleし、`state.Dependency`へhandleを繋ぎます。

## 25. ScheduleParallel

```csharp
var job = new IntegrateMovementJob
{
    DeltaTime = SystemAPI.Time.DeltaTime
};

state.Dependency = job.ScheduleParallel(state.Dependency);
```

同じcomponentへ競合writeする他System/Jobとのdependencyをschedulerへ正しく伝えます。Schedule後にmain threadからdataへ触らないこと。

## 26. EntityQueryをcacheする

複雑なqueryを`OnCreate`で作り、fieldへ保持する方法があります。query lifetimeはSystem/Worldに従います。

`RequireForUpdate(query)`で対象がない時system全体をskipできます。

## 27. System Group

代表的なsimulation/presentation group等の中でSystem順を制御します。

```csharp
[UpdateInGroup(typeof(SimulationSystemGroup))]
[UpdateAfter(typeof(InputCommandSystem))]
[UpdateBefore(typeof(DamageResolutionSystem))]
public partial struct MovementSystem : ISystem { }
```

attributeで全Systemを直列鎖にせず、本当に必要な順だけ指定します。

## 28. DependencyとSystem順

System update順とJob完了順は関連しますが同一ではありません。

- System AがJobをschedule。
- dependencyを`state.Dependency`へ登録。
- System Bが同dataへaccess。
- ECS schedulerがdependencyを接続。

明示Completeを増やすとparallel性を失います。

## 29. Structural ChangeとSync Point

query iteration中にEntityManagerでadd/remove/destroyすると、実行中Jobをcompleteさせるsync pointやiterator invalidationを招きます。

Structural changeを集めて後で再生するEntityCommandBufferを使います。

## 30. EntityCommandBuffer

```csharp
public partial struct DestroyDeadJob : IJobEntity
{
    public EntityCommandBuffer.ParallelWriter Commands;

    private void Execute(
        [EntityIndexInQuery] int sortKey,
        Entity entity,
        in Health health)
    {
        if (health.Value <= 0)
        {
            Commands.DestroyEntity(sortKey, entity);
        }
    }
}
```

Job中はcommandを記録し、対応するECB systemのplayback時に構造変更します。API attribute/sort keyはVersionを確認します。

## 31. ECB Playback

commandをいつ再生するかでgameplay timingが変わります。

```text
BeginSimulation ECB
Systems...
EndSimulation ECB
Presentation...
```

spawn/destroy/component addが同frameのどのsystemから見えるかを明示します。

## 32. sortKey

ParallelWriterのsortKeyは並列記録commandの再生順を決める材料です。

- unique/stableなchunk/entity index等を使う。
-同じkey大量使用によるordering/costを確認。
- gameplay determinismが必要なら明示orderを設計。

## 33. Instantiate

Entity prefabをECB/EntityManagerでinstantiateできます。Prefab entityはBaking/Authoringから作成し、reference componentへ保持します。

大量spawnでもcomponent初期値設定とstructural costがあるため、batch/ECBを使いProfilerで測ります。

## 34. Entity poolingは必要か

ECSのcreate/destroyはGameObjectより軽い場合がありますが無料ではありません。

- enabled componentでinactive化。
- chunk resident memory。
- reset漏れ。
- Archetype fragmentation。

まずECB create/destroyを測り、必要ならpool/enable方式を選びます。

## 35. Authoring Component

DesignerはGameObject/Inspectorで編集し、runtime ECS dataへ変換します。

```csharp
using Unity.Entities;
using UnityEngine;

public sealed class MoveAuthoring : MonoBehaviour
{
    [Min(0f)] public float Speed = 5f;

    private sealed class Baker : Baker<MoveAuthoring>
    {
        public override void Bake(MoveAuthoring authoring)
        {
            Entity entity = GetEntity(
                TransformUsageFlags.Dynamic);

            AddComponent(entity, new MoveSpeed
            {
                Value = authoring.Speed
            });
        }
    }
}
```

## 36. Baking

BakingはAuthoring data/GameObjectからruntime Entity/component dataを生成します。

```text
Authoring Scene/Prefab
 → Baker
 → Baking World
 → dependencies tracking
 → runtime Entity Scene data
```

旧Conversion Workflowの記事と混同しません。

## 37. Baker dependency

Bakerが参照するAsset/Authoring propertyを適切なBaker API経由で読み、変更時のincremental rebake dependencyへ登録します。

直接global検索や隠れたstatic dataに依存すると、変更してもrebakeされない問題を招きます。

## 38. TransformUsageFlags

Entityがruntime transformを必要とするか、rendering/parenting等の用途をBakerへ伝えます。

不用意にDynamicを全Entityへ付けると不要component/costを増やします。None/Renderable/Dynamic等の現行意味を公式資料で確認します。

## 39. SubScene

SubSceneはauthoring SceneをEntity Sceneとしてbake/streamする単位です。

- section streaming。
- open/closed SubScene。
- authoringとruntime representation。
- cross-scene Entity reference。
- content build。

通常SceneのGameObject loadと同じだと思わないでください。

## 40. BakingだけのEntity

中間dataやBaking処理だけに使いruntimeへ残さないcomponent/entityがあります。Authoring補助dataをPlayer runtimeへ持ち込まないよう、baking typeのcontractを使います。

## 41. LocalTransform

Entities Transformでは`LocalTransform`等を使います。

- Position。
- Rotation。
- Scale（uniform中心）。
- parent/child hierarchy。
- local-to-world計算。

non-uniform scale等は追加component/APIをVersionごとに確認します。

## 42. Transform write競合

複数Systemが同じLocalTransformを書けばdependencyで直列化され、logicも競合します。

```text
Movement writes Position
Knockback writes Position
RootMotion writes Position
```

各systemが直接最終Transformを書かず、intent/velocityをcomponentへ蓄積し、最後の一Systemがcommitする設計が有効です。

## 43. Presentationとの境界

```text
ECS Simulation
 → Hit/Spawn/Audio presentation request buffer
 → Bridge/System
 → GameObject Camera/UI/Audio/VFX
```

simulation systemからMonoBehaviour singletonへ直接callbackを乱発せず、data requestを境界で変換します。

## 44. Hybrid構成

高速3D actionでの例:

- Player/Bossの複雑animation: GameObject。
- 大量projectile/crowd perception: ECS。
- Camera/UI/Timeline: GameObject。
- background NPC/ambient VFX state: ECS。
- presentation bridge: 明示System。

全Projectを一度にECS化する必要はありません。

## 45. Managed Component

Entitiesにmanaged object/componentを持たせる機能はありますが、Burst、chunk、serialization、performance上の制約があります。

UnityEngine.Object bridge等に限定し、hot simulation dataをmanaged componentへ戻さないようにします。

## 46. Companion/Hybrid Object

GameObject componentをEntityと併用する仕組みはPackage Versionごとに対応が変わります。古いCompanionLink記事をそのまま使わず、Entities Graphics/Hybrid Rendererの現行workflowを確認します。

## 47. Entity Graphics

大量Entityをrenderingへ接続します。

- render mesh/material data。
- bounds/culling。
- transform。
- LOD。
- material property override。
- batch/instancing。

simulationが速くてもrenderingがGPU/CPU bottleneckにならないかProfiler/Frame Debuggerで測ります。

## 48. Chunk fragmentation

Archetype数、shared component値、entity数の端数によりchunkの空きが増えます。

- optional component組合せ爆発。
-一entityだけのArchetype。
- shared value過多。
- frequent structural change。

Entities Hierarchy/Systems/Archetypes系debug windowとProfilerで確認します。

## 49. Component size

大きなcomponentはchunk当たりentity数を減らします。

- rarely used dataを別component/Blobへ。
- byte alignment/padding。
- large fixed buffer。
- debug data。

細分化し過ぎるとArchetype/query complexityが増えるため、access patternで決めます。

## 50. Query explosion

stateごとにTagをadd/removeし大量のArchetypeを作るより、enableable componentやstate valueが適する場合があります。

ただし巨大enum stateを全Systemがbranchする設計も非効率です。処理集合と変更頻度から選びます。

## 51. Aspect

関連component accessを一つのviewへまとめるAspectがあります。

- domain単位のaccess表現。
- query readability。
- source generation。
- nested/reference rule。

API/推奨状況はEntities 1.4資料を確認し、hidden write accessを増やさないようにします。

## 52. Singleton Component

World全体設定/時刻/command buffer reference等にsingleton componentを使えます。

global mutable stateなので、read/write dependency、複数該当entity、test World初期化を明示します。

## 53. System StateとRequireForUpdate

Systemが必要dataのないframeに走らないよう:

```csharp
public void OnCreate(ref SystemState state)
{
    state.RequireForUpdate<GameRunning>();
}
```

singleton/entity presenceをgame mode gateにできます。enable状態とのquery semanticsを確認します。

## 54. Fixed Step

fixed simulation groupを使う場合:

- timestep。
- catch-up回数。
- physics group順。
- input sampling。
- interpolation。
- network tick。

MonoBehaviour FixedUpdateとECS fixed groupを無計画に二重管理しません。

## 55. Determinism

ECS/Job/Burstでも:

- parallel iteration順。
- ECB playback order。
- floating point。
- hash map order。
- structural change。

によりbit determinismは自動ではありません。stable sort key、explicit tick、fixed-point等を必要性に応じて設計します。

## 56. Debugging

- Entities Hierarchy。
- Systems window。
- Entity Inspector。
- Archetypes/chunk情報。
- Journaling。
- Burst Inspector。
- Profiler Timeline。
- Entity command playback位置。

debug機能のoverheadとDevelopment限定設定を確認します。

## 57. Journaling

Entity/componentのcreate、destroy、add/remove、set等の履歴追跡に役立ちます。

「誰がcomponentを消したか」を調べられますが、recording overhead/data量があるため必要時に限定します。

## 58. Testing World

System testでは専用Worldを作り、component inputを配置し、system update後のdataを検証します。

- testごとにWorld dispose。
- global Default Worldへ依存しない。
- ECB playback。
- Job complete。
- deterministic fixture。
- Burst on/off差。

## 59. Migration戦略

1. Profilerで大量data hotspotを特定。
2. pure data/modelをMonoBehaviourから分離。
3. Job + NativeArrayでalgorithmを検証。
4. ECS component/queryへ移す。
5. GameObject bridgeを維持。
6. rendering/physics移行は必要なら後段。

全面書換えではなく、測定可能なvertical sliceで進めます。

## 60. ECSを使わない判断

- entity数が少ない。
- teamがECS tooling未習熟。
- middlewareがGameObject前提。
- authoring/debug costが利益を超える。
- bottleneckがGPU/asset load。
- schedule/gather costで改善しない。

ECSを使わないことも技術判断です。

## 61. よくある失敗

### Componentへbehaviorを詰める

componentはdata、処理はSystemへ。

### 毎frameadd/remove

structural change/chunk移動を大量発生。enableable/state dataを検討。

### Entityを永続IDにする

World/lifetimeで無効化。stable ID componentを別に持つ。

### System順attributeを増やし続ける

直列化と循環dependency。data ownershipを整理。

### Managed Componentだらけ

Burst/cacheの利点を失う。bridgeに限定。

### ECS化だけで速いと思う

Archetype、chunk、query、render、gather/bridgeをProfilerで測る。

## 62. Performance checklist

- entity/archetype/chunk数。
- chunk utilization。
- System time/call count。
- Job worker/Complete wait。
- structural change数。
- ECB playback cost。
- query filtering cost。
- component size。
- managed allocation。
- rendering batches/GPU time。
- SubScene streaming peak。

## 63. Correctness checklist

- component owner/systemが明確か。
- read/write intentが正しいか。
- System orderを必要最小限にしたか。
- ECB playback時機を仕様化したか。
- Entity handleのWorld/lifetimeを守るか。
- stable gameplay IDを別に持つか。
- enableable stateとdataをresetするか。
- Baking dependencyを登録したか。
- Test WorldをDisposeするか。
- parallel結果順へ依存していないか。

## 64. 学習確認問題

1. Entity、Component、Systemの役割は何か。
2. Archetypeは何によって決まるか。
3. Chunk配置がcacheに有利な理由は何か。
4. component値変更とstructural changeの違いは何か。
5. enableable componentを使う理由は何か。
6. Bakingとruntime conversionは何が違うか。
7. ECBがsync point削減に役立つ理由は何か。
8. System順とJob dependencyは同じか。
9. ECSとMonoBehaviourを共存させる例は何か。
10. Entity handleをsave IDにできない理由は何か。

## 65. 公式資料

- [Unity Manual: Entities package](https://docs.unity3d.com/6000.0/Documentation/Manual/com.unity.entities.html)
- [Entities 1.4 Manual](https://docs.unity3d.com/Packages/com.unity.entities@1.4/manual/index.html)
- [Entities concepts](https://docs.unity3d.com/Packages/com.unity.entities@1.4/manual/concepts-intro.html)
- [Systems](https://docs.unity3d.com/Packages/com.unity.entities@1.4/manual/systems-intro.html)
- [Components](https://docs.unity3d.com/Packages/com.unity.entities@1.4/manual/components-intro.html)
- [Archetypes](https://docs.unity3d.com/Packages/com.unity.entities@1.4/manual/concepts-archetypes.html)
- [Baking](https://docs.unity3d.com/Packages/com.unity.entities@1.4/manual/baking.html)
- [Entity Command Buffers](https://docs.unity3d.com/Packages/com.unity.entities@1.4/manual/systems-entity-command-buffers.html)
- [Enableable components](https://docs.unity3d.com/Packages/com.unity.entities@1.4/manual/components-enableable-intro.html)
- [Entities Graphics](https://docs.unity3d.com/Packages/com.unity.entities.graphics@latest/)

## 66. まとめ

- ECSはEntity identity、Component data、System処理を分離するdata-oriented architecture。
- 同じArchetypeのentityはChunkへ列ごとに格納され、queryの連続処理に向く。
- add/remove/create/destroyはstructural changeで、値更新より高costになり得る。
- Authoring GameObjectをBakerがruntime Entity dataへ変換する。
- `ISystem`、SystemAPI Query、IJobEntity、Burstで大量dataを処理する。
-構造変更はECBへ記録し、playback timingとorderingを設計する。
- MonoBehaviourとECSを役割分担し、測定できる範囲から導入する。
