# Object Pool・GC Allocation削減

> 対象: Unity 6.0。最適化はProfilerで問題を確認してから行い、EditorだけでなくDevelopment Buildと対象機で再計測すること。

## 1. この章の目的

高速actionでは平均FPSだけでなくframe timeの安定性が重要です。

```text
60 FPS  → 1 frame 約16.67 ms
120 FPS → 1 frame 約 8.33 ms
```

小さなmanaged allocationが毎frame続くとheapが増え、やがてGC処理がframe spikeを作ります。この章では次を区別します。

- managed allocationを減らす。
- `Instantiate/Destroy`等のnative/engine costを減らす。
- pooled objectを正しくresetする。
- 常駐memoryを増やし過ぎない。
- optimization前後を計測する。

## 2. Unityのmemory領域

```text
Managed memory
  C# object、array、string、delegate、List内部array
  → GCが回収

Unmanaged C# memory
  NativeArray等
  → Disposeが必要

Native engine memory
  Texture、Mesh、GameObject native側、Audio等
  → Unity engine / Destroy / resource lifetime

Graphics memory
  GPU texture、buffer等
```

`GC.Alloc`が0でもnative allocationやGPU uploadが重い場合があります。逆にObject Poolはmanaged GCだけでなくGameObject初期化costを避ける目的もあります。

## 3. Managed heapとGC

参照型objectを`new`すると通常managed heapへ領域が必要です。objectが到達不能になるとgarbageになり、GCが後で回収します。

```text
allocate → use → reference消失 → garbage → GC scan/collect → reusable memory
```

`null`代入時に即時freeされるわけではありません。

## 4. Allocation rateで考える

1 frame 1 KBは小さく見えます。

```text
1 KB/frame × 60 frame/s = 60 KB/s
60 KB/s × 60 s = 約3.5 MB/min
```

複数systemが積み重なるとGC頻度が上がります。重要なのは一回のsizeだけでなく、hot pathでのbytes/frameと回数です。

## 5. GC spike

GCはmanaged object graphを調べ、不要objectを回収します。heapやlive objectが増えるほど仕事が増え得ます。

症状:

- 一定間隔のCPU spike。
- input/cameraの一瞬の引っ掛かり。
- battle intensityで頻度が変化。
- `GC.Collect` sample。
- GC Allocが継続。

GCだけを疑わず、Profiler timelineでanimation、physics、render thread等も確認します。

## 6. Incremental GC

Incremental GCはmarking等の仕事を複数frameへ分散し、長い停止を短くする仕組みです。

- GC総仕事量を消すわけではない。
- allocationを無料にしない。
- reference書換えのwrite barrier costがある。
- platformにより対応差がある。
- workloadによってfull collectionへfallbackし得る。

まずallocation sourceを減らし、その上でincremental modeを対象機計測します。

## 7. GCを無効化すればよいか

GC disabled中もallocationすればheapが増え続け、枯渇します。限定されたloading/battle windowで完全にallocationを制御できる高度な用途以外では危険です。

`GC.Collect()`を毎stageや毎秒呼ぶのも解決ではありません。loading画面等で明示collectionする場合も、必要性と停止時間を測ります。

## 8. ProfilerでGC.Allocを見る

Unity ProfilerのCPU Usage:

1. Development Build + Autoconnect Profiler等で対象機を接続。
2. 問題操作を再現。
3. Timeline/Hierarchyでframeを選択。
4. `GC.Alloc`列を確認。
5. Allocation Call Stacksを必要な範囲で有効化。
6. source lineと呼出頻度を特定。

Editor固有allocationがあるため、Player buildの値を基準にします。

## 9. Deep Profileの注意

Deep Profileは詳細ですがinstrumentation overheadが大きく、timingとallocationを変え得ます。

- まず通常Profiler + call stack。
- code範囲を絞る。
- `ProfilerMarker`を入れる。
- 最後に必要箇所だけDeep Profile。

計測装置が現象を変えることを忘れません。

## 10. Optimizationの順番

```text
再現条件を固定
 → frame budgetを決める
 → Profilerでhotspot発見
 → 原因仮説
 → 一変更
 → 同条件で再計測
 → correctness test
```

「allocationしそう」という印象だけで全codeを複雑化しません。

## 11. Object Poolとは

使用済みinstanceを破棄せずinactive storageへ戻し、次回再利用します。

```text
Get
 Poolに在庫あり → reset/activate → 利用者へ
 Poolが空       → create → 利用者へ

Release
 利用終了 → reset/deactivate → Poolへ
 capacity超過 → destroy
```

poolはobjectの寿命を長くして作成・破棄回数を減らすtrade-offです。

## 12. Poolに向くobject

- hit VFX。
- projectile。
- damage number。
- enemy wave unit。
- short-lived AudioSource。
- frequently reused UI row。
- temporary query/result object。
- repeated command/event payload。

頻度が低く巨大なbossを常時poolするのが得とは限りません。

## 13. Poolに向かない場合

- ほぼ一度しか作らない。
- resetが極端に複雑。
- native resourceを大量保持する。
- variant数が多く各poolが空在庫を抱える。
- Addressables contentを解放したいのにinstanceが残る。
- creation costが問題でない。

poolは常駐memoryを増やします。CPU spikeとmemory budgetの交換です。

## 14. UnityEngine.Pool

Unity 6には`UnityEngine.Pool` namespaceがあります。

- `IObjectPool<T>`。
- `ObjectPool<T>`: stack based、thread-safeではない。
- `LinkedPool<T>`。
- `GenericPool<T>`等。

`ObjectPool<T>`はcreate/get/release/destroy callback、collection check、default capacity、max sizeを設定できます。

## 15. Projectile Pool例

```csharp
using UnityEngine;
using UnityEngine.Pool;

public sealed class ProjectilePool : MonoBehaviour
{
    [SerializeField] private Projectile prefab;
    [SerializeField] private Transform inactiveRoot;
    [SerializeField, Min(1)] private int defaultCapacity = 32;
    [SerializeField, Min(1)] private int maxSize = 128;

    private IObjectPool<Projectile> pool;

    private void Awake()
    {
        pool = new ObjectPool<Projectile>(
            createFunc: Create,
            actionOnGet: OnGet,
            actionOnRelease: OnRelease,
            actionOnDestroy: OnDestroyPooled,
            // Editorでは二重Release検出に役立つ。
            collectionCheck: true,
            defaultCapacity: defaultCapacity,
            maxSize: maxSize);
    }

    public Projectile Spawn(
        Vector3 position,
        Quaternion rotation,
        ProjectileSpawnData data)
    {
        Projectile projectile = pool.Get();

        // Get callbackでactiveにした後、spawn固有stateを明示的に注入する。
        projectile.transform.SetPositionAndRotation(position, rotation);
        projectile.Begin(data, Return);
        return projectile;
    }

    private Projectile Create()
    {
        Projectile instance = Instantiate(prefab, inactiveRoot);

        // Pool以外から勝手にDestroy/Releaseしないようownerを決める。
        instance.gameObject.SetActive(false);
        return instance;
    }

    private static void OnGet(Projectile projectile)
    {
        projectile.gameObject.SetActive(true);
    }

    private void OnRelease(Projectile projectile)
    {
        // 前回利用者のstateを次回へ漏らさない。
        projectile.ResetForPool();
        projectile.transform.SetParent(inactiveRoot, worldPositionStays: false);
        projectile.gameObject.SetActive(false);
    }

    private static void OnDestroyPooled(Projectile projectile)
    {
        // maxSize超過で返されたinstance等を最終破棄する。
        Destroy(projectile.gameObject);
    }

    private void Return(Projectile projectile)
    {
        pool.Release(projectile);
    }

    private void OnDestroy()
    {
        // inactive itemへdestroy callbackを適用する。
        pool?.Clear();
    }
}
```

## 16. Spawn dataを値として渡す

```csharp
public readonly struct ProjectileSpawnData
{
    public readonly int Damage;
    public readonly Vector3 Velocity;
    public readonly int TeamId;
    public readonly float Lifetime;

    public ProjectileSpawnData(
        int damage,
        Vector3 velocity,
        int teamId,
        float lifetime)
    {
        Damage = damage;
        Velocity = velocity;
        TeamId = teamId;
        Lifetime = lifetime;
    }
}
```

小さな`readonly struct`ならheap objectを毎回作らず、初期化漏れをconstructorで防げます。巨大structのcopy costには注意します。

## 17. Pooled Projectileのreset

```csharp
using System;
using UnityEngine;

public sealed class Projectile : MonoBehaviour
{
    private Vector3 velocity;
    private int damage;
    private int teamId;
    private float remainingLifetime;
    private Action<Projectile> returnAction;
    private bool activeLease;

    public void Begin(
        ProjectileSpawnData data,
        Action<Projectile> returnAction)
    {
        if (activeLease)
        {
            throw new InvalidOperationException("Projectile is already active.");
        }

        activeLease = true;
        velocity = data.Velocity;
        damage = data.Damage;
        teamId = data.TeamId;
        remainingLifetime = data.Lifetime;
        this.returnAction = returnAction;
    }

    private void Update()
    {
        if (!activeLease)
        {
            return;
        }

        float dt = Time.deltaTime;
        transform.position += velocity * dt;
        remainingLifetime -= dt;

        if (remainingLifetime <= 0f)
        {
            Despawn();
        }
    }

    public void Despawn()
    {
        if (!activeLease)
        {
            return; // 同frameのhitとtimeoutによる二重Releaseを防ぐ。
        }

        activeLease = false;
        Action<Projectile> callback = returnAction;
        returnAction = null;
        callback?.Invoke(this);
    }

    public void ResetForPool()
    {
        // referenceを切り、前回利用者をpoolが保持し続けないようにする。
        velocity = Vector3.zero;
        damage = 0;
        teamId = 0;
        remainingLifetime = 0f;
        returnAction = null;
        activeLease = false;
    }
}
```

## 18. Reset checklist

- position/rotation/scale。
- parent。
- velocity/angular velocity。
- Animator state/parameters。
- Particle System state。
- Trail Renderer clear。
- material property。
- health/status effect。
- timer/coroutine。
- event/delegate購読。
- cancellation token source。
- target/reference。
- layer/tag。
- collider enabled。
- NavMeshAgent path。
- AudioSource clip/time/loop。

`SetActive(false)`だけではstateは初期化されません。

## 19. OnEnable/OnDisable依存の危険

PoolのGet/Releaseで`SetActive`すると`OnEnable/OnDisable`が呼ばれます。ただしcallback順と初期化順が曖昧だと、`OnEnable`がSpawn data設定前に動きます。

```text
pool.Get
 → actionOnGet: SetActive(true)
   → OnEnable
 → Spawn method: Begin(data)
```

`OnEnable`がdataを必要とするなら順序が逆です。activationをBegin内へ移す、Get callbackでactiveにしない等、contractを揃えます。

## 20. 二重Release

同じinstanceを二度poolへ返すと、同一objectが二利用者へ貸し出される重大bugになります。

原因:

- timeoutとcollisionが同frame。
- `OnDisable`とexplicit Despawn。
- Scene cleanupとself return。
- async callback遅延。

active lease flag、generation、collection check、owner一本化で防ぎます。

## 21. Collection check

`ObjectPool<T>`のcollection checkは、既にpool内のitemをReleaseするとEditorで例外を出す助けになります。

- development中は有効候補。
- Player挙動/overheadを公式契約で確認。
- checkに頼らずstate machineでも防ぐ。
- `UnsafeGenericPool`はcheckを省くため、正しさを別途保証する。

## 22. maxSize

無制限poolは一度の極端なwaveで膨らみ、その後ずっとmemoryを保持します。

```text
usual active: 40
peak active: 90
pool max: 128
emergency burst: 300
→ 172個は返却時destroy
```

maxは通常peak、memory budget、再生成costを計測して決めます。

## 23. Prewarm

```csharp
public static void Prewarm<T>(IObjectPool<T> pool, int count)
    where T : class
{
    // 一時配列/stackのallocationもloading phase内で行う。
    var rented = new T[count];

    for (int i = 0; i < count; ++i)
    {
        rented[i] = pool.Get();
    }

    for (int i = 0; i < count; ++i)
    {
        pool.Release(rented[i]);
    }
}
```

一つずつGet→Releaseすると同じ一個だけを繰り返すため、同時に`count`個借りてから返します。prewarmはloading screenで行い、必要数以上を作らないでください。

## 24. Pool shrinking

Unity標準poolのpolicyだけで足りない場合、長時間inactive itemを段階的にdestroyするcustom poolを検討します。

- high-water mark。
- minimum retained count。
- last used time。
- memory pressure。
- Scene/content set変更。

戦闘中に大量destroyせず、遷移/idle時に縮小します。

## 25. PoolとAddressables

```text
Addressables prefab handle
  └─ Pool
      ├─ active instances
      └─ inactive instances
```

inactive instanceもPrefabのtexture/material等を参照します。poolに一つでもinstanceが残る間は、Asset leaseをreleaseしない設計にします。

content unload順:

1. new spawn停止。
2. active instance回収/破棄。
3. pool clear/dispose。
4. prefab Addressables handle release。

## 26. VariantごとのPool爆発

weapon skin × element × rarityごとに専用Prefab Poolを作ると、inactive在庫が膨大になります。

対策:

- common base + data差替え。
- renderer/material propertyだけ変更。
- active content setだけpool生成。
- LRUでpool解放。
- cosmetic assetを別lease。

reset complexityとmemoryの両方を測ります。

## 27. Particle System Pool

Particleは停止時callbackで返せます。

```csharp
public sealed class ReturnParticleToPool : MonoBehaviour
{
    private System.Action<ParticleSystem> returnAction;
    private ParticleSystem system;

    private void Awake()
    {
        system = GetComponent<ParticleSystem>();
        ParticleSystem.MainModule main = system.main;
        main.stopAction = ParticleSystemStopAction.Callback;
    }

    public void Play(System.Action<ParticleSystem> onStopped)
    {
        returnAction = onStopped;
        system.Play(withChildren: true);
    }

    private void OnParticleSystemStopped()
    {
        System.Action<ParticleSystem> callback = returnAction;
        returnAction = null;
        callback?.Invoke(system);
    }
}
```

loop particleは自動停止しないため明示Releaseが必要です。sub-emitter/trailもclearします。

## 28. Rigidbody Pool

返却時:

```csharp
rigidbody.linearVelocity = Vector3.zero;
rigidbody.angularVelocity = Vector3.zero;
rigidbody.Sleep();
rigidbody.position = inactivePosition;
rigidbody.rotation = Quaternion.identity;
```

Unity Versionによりvelocity property名を確認します。TransformとRigidbodyの移動を混在させず、collision callback中のdisable時機もtestします。

## 29. Coroutineのreset

inactive化でcomponent coroutineが止まる条件を決めつけません。owner上で開始したcoroutine、async Task、Tween libraryは別ownerに残る場合があります。

- `StopAllCoroutines()`の影響範囲。
- CancellationTokenSourceをleaseごとに作り、返却時cancel/dispose。
- completion callbackにgeneration ID。
- tween kill/reset。

古い非同期処理が再利用後のinstanceを変更するABA問題を防ぎます。

## 30. Generationによる古いcallback防止

```csharp
private int generation;

public int BeginLease()
{
    return ++generation;
}

public bool IsCurrentLease(int capturedGeneration)
{
    return capturedGeneration == generation;
}

public void EndLease()
{
    ++generation; // 旧callbackを全て無効化する。
}
```

同じGameObject referenceでも、lease世代が違えば別利用者です。

## 31. List allocation

```csharp
private readonly List<Enemy> nearbyEnemies = new(capacity: 64);

private void CollectNearbyEnemies()
{
    // List objectと内部arrayを毎frame作らず、同じbufferを再利用する。
    nearbyEnemies.Clear();

    // ClearはCountを0にする。capacityは通常保持される。
    // reference型要素は内部slotがclearされ、古いEnemyの保持を避ける。
}
```

`Clear`後もcapacity memoryは残ります。極端な一回で巨大化したlistは`TrimExcess`をhot path外で検討します。

## 32. List capacity

要素追加でcapacity不足になると、より大きい内部arrayを確保してcopyします。

```csharp
var hits = new List<HitResult>(128);
```

予想上限が分かるhot listは初期capacityを与えます。全listへ巨大capacityを設定すると常駐memoryを浪費します。

## 33. Array.Empty

空配列を返すたびに`new T[0]`しません。

```csharp
return System.Array.Empty<Enemy>();
```

共有empty arrayを返します。ただしcallerが配列を書換える契約にしないでください。

## 34. NonAlloc Physics Query

```csharp
private readonly Collider[] hitBuffer = new Collider[64];

private int QueryTargets(Vector3 center, float radius, int layerMask)
{
    int count = Physics.OverlapSphereNonAlloc(
        center,
        radius,
        hitBuffer,
        layerMask,
        QueryTriggerInteraction.Ignore);

    // count == buffer.Lengthなら結果が打ち切られた可能性がある。
    // overflow policyを決め、最大数を黙って無視しない。
    return count;
}
```

Unity 6で推奨されるquery APIと挙動を対象versionの公式資料で確認します。NonAlloc化はbuffer overflowという新しい設計課題を持ち込みます。

## 35. Query buffer overflow

選択肢:

- gameplay上の最大target数を定義して切る。
- bufferを段階的に拡張し、次回再利用。
- spatial partitionで候補を減らす。
- warning/telemetryを出す。
- critical queryは再試行。

順序が保証されない結果から近いN体を選ぶなら、距離計算とselectionも必要です。

## 36. GetComponentsの再利用

配列を返すAPIは呼出ごとにarray allocationする場合があります。利用可能ならListを受け取るoverload等を使い、bufferを再利用します。

```csharp
private readonly List<Collider> colliders = new(8);

private void RefreshColliders()
{
    colliders.Clear();
    GetComponentsInChildren(includeInactive: true, result: colliders);
}
```

APIのallocation特性はUnity Version/EditorとPlayerで違い得るため計測します。

## 37. Component lookupをcacheする

```csharp
private Rigidbody cachedBody;

private void Awake()
{
    cachedBody = GetComponent<Rigidbody>();
}
```

毎frame`GetComponent`を繰り返すよりcacheが分かりやすい場合があります。ただしUnity 6の実costを測り、object lifetimeとmissing component validationを扱います。

## 38. String allocation

`string`はimmutableです。連結、format、`ToString`で新しいstringが生じ得ます。

悪い例:

```csharp
scoreLabel.text = "Score: " + score + " / " + maxScore;
```

毎frame値が変わらないなら、変更event時だけ更新します。UI toolkit/TMP側のAPIや内部allocationもProfilerで確認します。

## 39. StringBuilder

```csharp
private readonly System.Text.StringBuilder builder =
    new(capacity: 64);

private string BuildDebugLine(int active, int inactive)
{
    builder.Clear();
    builder.Append("Active: ");
    builder.Append(active);
    builder.Append(" Inactive: ");
    builder.Append(inactive);
    return builder.ToString(); // 最終string自体は作られる。
}
```

`StringBuilder`も万能0 allocationではありません。capacity拡張と`ToString()`を伴います。短い低頻度文字列では単純連結の方が十分な場合があります。

## 40. Debug log

```csharp
Debug.Log($"Enemy {enemyId} state={state}");
```

log無効時でもargument/string構築が起こる書き方があります。

```csharp
#if DEVELOPMENT_BUILD || UNITY_EDITOR
Debug.Log($"Enemy {enemyId} state={state}");
#endif
```

production hot pathのlogをcompileから除く、rate limitする、structured counterにする等を検討します。

## 41. Boxing

value typeを`object`や非generic interfaceへ変換するとboxing allocationが起こり得ます。

```csharp
int value = 42;
object boxed = value; // int値を包むheap objectが必要。
```

発生例:

- `object`引数。
- 非generic collection。
- interface経由のvalue type呼出。
- format/log API。
- enum comparerの選択。

generic APIと適切なoverloadを使い、Profilerで確認します。

## 42. Enumとboxing

`Dictionary<EnumType, TValue>`は通常generic comparerを使えますが、古いruntime/APIや`Enum`/`object`へ渡す処理でboxingし得ます。

optimization記事を無条件に写さず、現在のUnity、Mono/IL2CPP、対象platformで測ります。

## 43. Closure

```csharp
int targetId = currentTargetId;
button.onClick.AddListener(() => SelectTarget(targetId));
```

lambdaがlocal変数をcaptureすると、compilerがclosure objectを生成する場合があります。登録が低頻度なら問題でないこともありますが、毎frame/大量生成箇所では注意します。

## 44. Captureしないcallback

```csharp
private void OnButtonClicked()
{
    SelectTarget(currentTargetId);
}

private void OnEnable()
{
    button.onClick.AddListener(OnButtonClicked);
}

private void OnDisable()
{
    button.onClick.RemoveListener(OnButtonClicked);
}
```

method groupでもdelegate生成/cache挙動はcontextによります。最大の利点は解除しやすく、lifetimeが明確なことです。

## 45. Delegate allocation

毎frame新しいdelegate/lambdaを作ってsort、event、job completionへ渡すとallocationし得ます。

```csharp
private static readonly Comparison<Enemy> CompareByPriority =
    static (left, right) => left.Priority.CompareTo(right.Priority);
```

stateをcaptureしない比較はstaticにcacheできます。sort自体の計算量と安定性も確認します。

## 46. LINQ

LINQは表現力が高い一方、iterator、delegate、collection materialization等がallocation/costを生む場合があります。

```csharp
var nearest = enemies
    .Where(x => x.IsAlive)
    .OrderBy(x => x.DistanceSquared)
    .FirstOrDefault();
```

Editor toolや低頻度setupでは読みやすさを優先できます。毎frame多数entityを走査するhot pathは明示loopと一pass selectionを検討します。

## 47. LINQを一passへ

```csharp
private static Enemy FindNearestAlive(
    IReadOnlyList<Enemy> enemies,
    Vector3 origin)
{
    Enemy best = null;
    float bestDistanceSq = float.PositiveInfinity;

    for (int i = 0; i < enemies.Count; ++i)
    {
        Enemy candidate = enemies[i];
        if (!candidate.IsAlive)
        {
            continue;
        }

        float distanceSq =
            (candidate.transform.position - origin).sqrMagnitude;

        if (distanceSq < bestDistanceSq)
        {
            best = candidate;
            bestDistanceSq = distanceSq;
        }
    }

    return best;
}
```

allocationだけでなくsortの`O(n log n)`を避け、nearest一体なら`O(n)`にします。

## 48. IEnumerableとiterator

`yield return`はstate machine objectを作る場合があります。API境界で便利ですが、毎frameの極端なhot pathではarray/span/index iteration等と比較します。

class enumerator、interface化、boxingの組み合わせは型によって異なります。実際のcall stackを見ます。

## 49. foreach

「foreachは必ずallocationする」は誤りです。arrayや`List<T>`の通常foreachはallocationしない形になり得ますが、`IEnumerable<T>` interface経由やcustom enumeratorで変わります。

可読性を捨てて全部forへ変える前に、compiler/runtimeとProfilerを確認します。

## 50. Params array

```csharp
void Emit(params object[] values)
```

呼び出しごとに配列が作られ、value typeはboxingも起こし得ます。hot logging/event APIではtyped overloadやstruct eventを使います。

## 51. Returning arrays

```csharp
Enemy[] GetVisibleEnemies()
```

毎回新配列を返すcontractはallocationを強制します。代案:

- caller-provided buffer。
- `List<T>`へ書込。
- read-onlyな内部snapshotを更新時だけ作る。
- `NativeArray`/`NativeList`と明示lifetime。
- callback visitor。

内部mutable collectionをそのまま公開して破壊されないようにします。

## 52. Dictionary reuse

`Clear()`して再利用できますがcapacityは残ります。key/valueがreferenceならClearが参照を外すか、runtime実装を確認します。

- capacityを見積もる。
- stable key comparer。
- lookup hot pathでstring生成しない。
- `ContainsKey`後のindexerという二重lookupを`TryGetValue`へ。

## 53. Hash key

毎frame座標からstring keyを作らないで、value type keyを使います。

```csharp
public readonly struct GridCell : System.IEquatable<GridCell>
{
    public readonly int X;
    public readonly int Z;

    public GridCell(int x, int z) { X = x; Z = z; }

    public bool Equals(GridCell other) => X == other.X && Z == other.Z;
    public override bool Equals(object obj) => obj is GridCell other && Equals(other);
    public override int GetHashCode() => System.HashCode.Combine(X, Z);
}
```

hash qualityとboxingしないgeneric useを確認します。

## 54. Interfaceとstruct

structをinterface型変数へ入れるとboxingする場合があります。

```csharp
IMyCommand command = new MoveCommand();
```

generic constraint、concrete type、Native container等を検討します。ただしarchitectureを複雑にする前に頻度とbytesを測ります。

## 55. async/await allocation

`async` methodはstate machine、Task、continuation等のcostを持ち得ます。I/OやScene loadには適切ですが、毎entity毎frameの短い処理へ使いません。

- 完了済みpath。
- `Task` vs `ValueTask`の正しい契約。
- cancellation source。
- exception。
- IL2CPP挙動。

非同期を消してmain threadをblockする方が悪い場合もあります。用途で判断します。

## 56. Coroutine allocation

Coroutine開始時にiterator object、`new WaitForSeconds(...)`等のyield instructionがallocationし得ます。

```csharp
private static readonly WaitForSeconds HalfSecond = new(0.5f);
```

固定時間なら共有候補ですが、timeScale、mutable state、Unity Versionの挙動を確認します。可変時間はtimer component/state machineの方が明確な場合があります。

## 57. Event payload

```csharp
public readonly struct DamageEvent
{
    public readonly int SourceId;
    public readonly int TargetId;
    public readonly int Amount;

    public DamageEvent(int sourceId, int targetId, int amount)
    {
        SourceId = sourceId;
        TargetId = targetId;
        Amount = amount;
    }
}
```

小さなimmutable struct eventはheap objectを減らせます。interface/event busがboxingしないか、queueがcopyし過ぎないか確認します。

## 58. Managed object pool

GameObjectだけでなくpure C# requestもpoolできます。ただし小さく短命なobjectを全部poolすると、reset漏れ、memory保持、complexityが増えます。

向く条件:

- creation頻度が高い。
- bufferを内部に持ち再利用価値が高い。
- owner/release地点が明確。
- Profilerで意味のあるallocation量。

## 59. ArrayPool

利用可能な.NET profileでは`System.Buffers.ArrayPool<T>`が候補です。

```csharp
int[] buffer = System.Buffers.ArrayPool<int>.Shared.Rent(minimumLength: 256);

try
{
    // Rent結果のLengthは要求以上の場合がある。
    // 有効要素数を別に管理する。
}
finally
{
    // 機密/reference dataならclearArrayを検討する。
    System.Buffers.ArrayPool<int>.Shared.Return(buffer, clearArray: false);
}
```

返却後のbufferへ触らない、二重返却しない、長期間借りっぱなしにしないこと。

## 60. NativeArray

`NativeArray<T>`はGC管理外のmemoryを使い、Job/Burstと連携できますが明示Disposeが必要です。

```csharp
var values = new Unity.Collections.NativeArray<float>(
    length: 1024,
    allocator: Unity.Collections.Allocator.TempJob);

try
{
    // 使用する。
}
finally
{
    if (values.IsCreated)
    {
        values.Dispose();
    }
}
```

GC AllocをNative leakへ置き換えないでください。Allocatorのlifetime ruleを守ります。

## 61. StackallocとSpan

対応するC#/.NET/IL2CPP環境では、小さい一時bufferに`stackalloc`/`Span<T>`が使える場合があります。

- stack sizeを超えない。
- method外へescapeしない。
- Burst/IL2CPP/platform対応を確認。
- 巨大/可変user入力sizeに使わない。

低水準最適化なので、まず再利用array/Listで十分か測ります。

## 62. Animator・Materialの隠れcost

- `renderer.material`はmaterial instanceを作る。
- Animator parameterをstringで毎回hash lookup。
- `Shader.PropertyToID`を毎回呼ぶ必要は通常ない。
- mesh/material配列getterがarrayを返す場合。

```csharp
private static readonly int SpeedId = Animator.StringToHash("Speed");
private static readonly int TintId = Shader.PropertyToID("_BaseColor");
```

共有Materialを変更せず`MaterialPropertyBlock`を再利用する等、render correctnessも守ります。

## 63. MaterialPropertyBlock reuse

```csharp
private readonly MaterialPropertyBlock block = new();

private void SetTint(Renderer target, Color color)
{
    target.GetPropertyBlock(block);
    block.SetColor(TintId, color);
    target.SetPropertyBlock(block);
    block.Clear();
}
```

Rendererごとの既存propertyを上書きしないか、SRP Batcher/instancingへの影響をFrame Debuggerで確認します。

## 64. UI allocation

- Layout rebuild。
- text再生成。
- event subscription。
- list item Instantiate/Destroy。
- closure。
- binding update。
- string formatting。

scroll listはvisible rowだけをvirtualize/recycleします。毎frame全HUD textを再設定せず、value変更eventで更新します。

## 65. Damage number Pool

damage numberは大量発生しますが、同時表示上限と可読性もあります。

```text
Damage events 200/s
 → merge policy
 → visible cap 40
 → pooled labels 48
 → oldest/low priorityをrecycle
```

ただpoolを300個作るより、presentation側で集約/優先度制御する方がCPU・UI・視認性すべてに効きます。

## 66. Enemy Poolの注意

enemyはreset対象が多いです。

- AI blackboard/behavior state。
- aggro target。
- NavMeshAgent path。
- status effects。
- animation。
- weapon/projectile ownership。
- event bus。
- health UI。
- loot/drop flag。
- async asset/VFX。

reset testが弱いなら、enemy全体ではなくprojectile/VFXだけpoolする方が安全な場合があります。

## 67. Scene lifetime

`DontDestroyOnLoad`なglobal poolへScene objectを返すと、破棄済みSceneへのparent/referenceを保持し得ます。

- global poolはglobal resourceだけ。
- stage poolはstage ownerと一緒にdispose。
- active leaseが残ったままScene unloadしない。
- Scene unload後にnull wrapperを在庫へ残さない。

Pool scopeをApplication、Game Mode、Stage、Wave等で明示します。

## 68. Domain Reload

EditorでDomain Reloadを無効にするとstatic pool/stateがPlay間に残る場合があります。

- runtime initialize methodでstaticをclear。
- Editor Play Mode testで複数回開始/停止。
- destroyed Unity Objectがstatic poolに残らない。
- event購読解除。

Playerでは別lifecycleなので、Editor便利設定をproduction挙動と混同しません。

## 69. Pool correctness invariant

常に成立させたい条件:

```text
CountAll = CountActive + CountInactive
一つのinstanceは active または inactive の片方だけ
active instanceにはownerが一つ
inactive instanceには外部callback/referenceがない
pool dispose後にGet/Releaseしない
```

development assertionとtestで検証します。

## 70. Pool telemetry

- CountAll/Active/Inactive。
- create count。
- reuse count。
- max concurrent active。
- capacity overflow destroy count。
- double release error。
- lease duration。
- content set別resident memory。

peak activeを長期間収集するとprewarm/default/maxを推測できます。

## 71. Allocation budget

目標例:

```text
Core combat steady state: 0 B/frame managed allocation
UI value change frame: bounded allocation
Loading/Scene transition: allocation許容、peak上限あり
Debug build: diagnostic allocationを別計測
```

全frame絶対0 Bを宗教にせず、入力/camera/combatなどlatency critical pathを優先します。

## 72. Before/After計測表

| 指標 | Before | After |
|---|---:|---:|
| GC Alloc/frame | 12 KB | 0.2 KB |
| Projectile create/s | 180 | 2 |
| Main thread p99 | 13 ms | 8 ms |
| Resident memory | 1.2 GB | 1.28 GB |
| Pool overflow/min | - | 3 |

Poolでmemoryが増えたことも隠さず記録します。平均だけでなくp95/p99とworst frameを見ます。

## 73. よくある失敗

### Poolを作ったが毎回newしている

Get/Release経路外でInstantiateし、効果がありません。creation countを計測します。

### Release時reset不足

前のtarget、色、event、velocityが次のleaseへ漏れます。

### inactive objectを無限保持

極端なpeak後にmemoryが戻りません。max/shrink policyを入れます。

### LINQを全部禁止

Editor/one-time codeまで読みにくくします。hot pathと低頻度pathを分けます。

### NonAlloc buffer overflowを無視

敵が多い時だけhit判定が欠落します。overflowを仕様化します。

### EditorのGC Allocだけを見る

Playerと違うallocationを追いかけます。対象機buildで確認します。

## 74. Object Pool test

- empty poolからcreate。
- 在庫からreuse。
- max size超過。
- double release。
- same-frame hit + timeout。
- Scene unload中release。
- pool dispose後callback。
- 10000回Get/Release。
- state reset全項目。
- async callbackが次leaseへ届かない。
- Addressables handleをpool clear前にreleaseしない。

## 75. Allocation test

- idle 1000 frames。
- combat steady state 1000 frames。
- 100 projectile/s。
- 50 target query/frame。
- damage number burst。
- locale/UI更新なしのHUD。
- input bindingなしの通常input。
- logging無効build。
- MonoとIL2CPP。
- low-end target device。

ProfilerRecorderやPerformance Testing packageによる回帰検査は後章で扱います。

## 76. Review checklist

- allocation call stackを確認したか。
- target Playerで測ったか。
- pool対象のcreate/destroy costが本当に高いか。
- reset contractが列挙されているか。
- 二重Releaseを防いだか。
- max capacityとoverflow policyがあるか。
- prewarmは必要同時数を作るか。
- inactive itemがAddressables Assetを保持することを考えたか。
- List/array bufferを再利用したか。
- NonAlloc結果のoverflowを扱ったか。
- boxing、closure、params、LINQをhot pathで確認したか。
- pool導入後のresident memoryを測ったか。
- correctness testをoptimization後に通したか。

## 77. 学習確認問題

1. `null`代入とGC回収はどう違うか。
2. Incremental GCはallocationを無くすか。
3. PoolがCPUを減らす代わりに増やすresourceは何か。
4. `SetActive(false)`だけではresetにならない理由は何か。
5. 同じinstanceの二重Releaseが危険な理由は何か。
6. Prewarmで一個ずつGet→Releaseしてはいけない理由は何か。
7. Addressables handleをpool clear前にreleaseできない理由は何か。
8. NonAlloc APIが追加する新しい問題は何か。
9. `foreach`やLINQを一律禁止すべきでない理由は何か。
10. GC Alloc削減後もnative/graphics memoryを測る理由は何か。

## 78. 公式資料

- [Unity Manual: Memory in Unity](https://docs.unity3d.com/6000.0/Documentation/Manual/performance-memory-overview.html)
- [Unity Manual: Garbage collector overview](https://docs.unity3d.com/6000.0/Documentation/Manual/performance-garbage-collector.html)
- [Unity Manual: Incremental garbage collection](https://docs.unity3d.com/6000.0/Documentation/Manual/performance-incremental-garbage-collection.html)
- [Unity Manual: Tracking garbage collection allocations](https://docs.unity3d.com/6000.0/Documentation/Manual/performance-track-garbage-collection.html)
- [Unity Scripting API: ObjectPool&lt;T&gt;](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/Pool.ObjectPool_1.html)
- [Unity Scripting API: IObjectPool&lt;T&gt;](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/Pool.IObjectPool_1.html)
- [Unity Scripting API: LinkedPool&lt;T&gt;](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/Pool.LinkedPool_1.html)
- [Unity Manual: Optimize physics memory usage](https://docs.unity3d.com/6000.0/Documentation/Manual/physics-optimization-memory.html)

## 79. まとめ

- managed allocationは直ちにfreeされず、蓄積するとGC workとframe spikeを生む。
- Poolは頻繁なcreate/destroyを再利用へ変えるが、reset complexityとresident memoryを増やす。
- Get/Release、active/inactive、Addressables leaseの所有権を明示する。
- allocation sourceには配列返却、string、boxing、closure、delegate、LINQ、async等がある。
- reusable List/array、NonAlloc query、event駆動UIを適切に使う。
- Incremental GCやpoolを魔法として扱わず、Player buildのProfilerでbefore/afterを比較する。
