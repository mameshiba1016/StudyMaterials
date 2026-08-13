# Unity 2D/3D Physics・Collider・Layer・戦闘判定

> 対象: Unity 6系。3D Physicsと2D Physicsは別System・別Component・別APIです。名前が似ていても混在できません。

## 1. Physics Engineの役割

Physics Engineは、形状同士の接触候補を探し、接触点や法線を作り、速度・質量・摩擦・反発等から次の状態を求めます。

```text
Game code
  ↓ force / velocity / kinematic target
Broad phase（大まかな候補絞り込み）
  ↓
Narrow phase（形状同士の詳細判定）
  ↓
Contact generation
  ↓
Constraint solver（接触・Joint等を解く）
  ↓
Rigidbody state更新
  ↓
Collision/Trigger callback
```

Engineが自動で「攻撃のdamage」「敵味方」「無敵」を理解するわけではありません。Physicsは空間的な候補を返し、Gameplay層が意味を決めます。

## 2. 3Dと2Dは別世界

| 3D | 2D |
|---|---|
| `Rigidbody` | `Rigidbody2D` |
| `Collider` | `Collider2D` |
| `Physics` | `Physics2D` |
| `Collision` | `Collision2D` |
| `OnCollisionEnter` | `OnCollisionEnter2D` |
| `PhysicsMaterial` | `PhysicsMaterial2D` |

3D `BoxCollider`と2D `BoxCollider2D`は互いに衝突しません。2.5Dゲームでも、どちらをGameplay collisionの基準にするか先に決めます。

## 3. Colliderは形、Rigidbodyは運動状態

- Collider: 衝突形状、Trigger、Material、Layer override等を表す。
- Rigidbody: 位置・回転・速度・質量・重力・sleep等、物理bodyの状態を表す。
- Transform: Scene graph上の姿勢。Physics bodyと同期されるが、任意の瞬間に同じ責務で直接書き合わない。

見た目のMeshへ正確にColliderを合わせるほど良いとは限りません。Capsule、Box、Sphereの近似は高速・安定・調整しやすく、CharacterのGameplay判定に向きます。

## 4. Dynamic、Kinematic、Static

### Dynamic

simulationが力、重力、衝突から運動を決めます。通常は`AddForce`、velocity、Rigidbody APIで操作します。毎frame Transformをteleportし続けるとsolverの前提を壊します。

### Kinematic

game codeが運動を主導します。他bodyへどう影響し、何から影響を受けるかは3D/2Dと設定で異なります。Moving Platform、演出制御物、Character Motor等に使いますが「衝突しないbody」という意味ではありません。

### Static

動かないWorld geometry向けです。Static Colliderを頻繁にTransform移動すると、内部の空間構造更新が発生します。動く可能性があるならKinematic bodyを検討します。

## 5. Rigidbodyを動かす方法を混ぜない

```csharp
using UnityEngine;

[RequireComponent(typeof(Rigidbody))]
public sealed class ForceDrivenBody : MonoBehaviour
{
    [SerializeField] private float acceleration = 20.0f;
    private Rigidbody body;
    private Vector2 moveInput;

    private void Awake()
    {
        body = GetComponent<Rigidbody>();
    }

    public void SetInput(Vector2 input)
    {
        moveInput = Vector2.ClampMagnitude(input, 1.0f);
    }

    private void FixedUpdate()
    {
        Vector3 force = new(moveInput.x, 0.0f, moveInput.y);

        // 質量をGameplayに反映したいのでForceMode.Acceleration等の選択意図を確認する。
        body.AddForce(force * acceleration, ForceMode.Acceleration);
    }
}
```

操作方法は設計目的で選びます。

- `AddForce`: 物理的な加速・衝撃。
- velocity設定: 目標速度へ明示的に合わせたいmotor。
- `MovePosition`/`MoveRotation`: Kinematic motion等。利用条件をAPIで確認。
- Transform設定: teleport、初期配置、Physics同期を理解した限定用途。

## 6. 質量・Drag・Gravity・Constraint

- Mass: 同じforceに対する加速や衝突応答へ影響。
- Linear/Angular Damping: 速度を減衰させる。版によってInspector/API名称が変わることがある。
- Use Gravity / Gravity Scale: 3Dは重力利用、2Dは倍率等、Systemごとに異なる。
- Constraints: 軸移動・回転を固定する。
- Center of Mass / Inertia Tensor: 回転応答へ影響。

Characterを倒れないようFreeze Rotationするのは有効ですが、Root Motion、坂、Knockbackとの責務を確認します。

## 7. Colliderの種類

### 3D

- Box、Sphere、Capsule: primitive。高速で安定しやすい。
- Mesh Collider: 複雑形状。Convex、Rigidbodyとの組合せ、更新costに制約がある。
- Terrain Collider: Terrain用。
- Compound Collider: 一つのRigidbody配下に複数Colliderを配置。

### 2D

- Box、Circle、Capsule、Polygon、Edge。
- Composite Collider 2D: Tilemap等の多数形状を合成。
- Tilemap Collider 2D: tileから形状生成。変更時の再構築costを持つ。

scale、負scale、非一様scaleは形状・法線・性能へ影響することがあります。Collider形状自体の寸法を調整できるなら、Transform scaleだけへ頼りません。

## 8. CollisionとTrigger

通常Colliderはsolverが貫通を解消し、force応答を生成します。`isTrigger`を有効にしたColliderは物理的に押し返さず、重なりeventをGameplayへ知らせる用途です。

```csharp
using UnityEngine;

public sealed class PickupTrigger : MonoBehaviour
{
    private bool consumed;

    private void OnTriggerEnter(Collider other)
    {
        if (consumed || !other.TryGetComponent<PlayerMarker>(out _))
        {
            return;
        }

        // callbackの重複や複数Collider侵入に耐えるよう冪等にする。
        consumed = true;
        GrantReward();
        Destroy(gameObject);
    }
}
```

callbackが発生するRigidbody/Colliderの組合せには規則があります。「Colliderが二つあれば必ず呼ばれる」と暗記せず、利用版のCollision action matrixを確認します。

## 9. Enter、Stay、Exitの落とし穴

- Enter: 接触開始。1攻撃1hitの候補になるが、Colliderの再有効化等で再発する。
- Stay: 接触継続。毎Physics step呼ばれ得る。継続damageは独自intervalが必要。
- Exit: 接触終了。Object破棄、無効化、Layer変更等の全経路で必ず期待通り受け取れると仮定しない。

状態cleanupをExit callbackだけに依存させず、ownerのDisable/Destroyでも解除できるようにします。

## 10. Collision情報

`Collision`/`Collision2D`からother body、relative velocity、contact point、normal等を取得できます。ただしcallbackで不要な詳細を毎回処理するとcostになります。

```csharp
private void OnCollisionEnter(Collision collision)
{
    if (collision.contactCount == 0)
    {
        return;
    }

    ContactPoint contact = collision.GetContact(0);
    Vector3 hitPoint = contact.point;
    Vector3 surfaceNormal = contact.normal;

    // VFX向き等へ使用。Gameplay damage量は速度だけで無条件に決めない。
    SpawnImpact(hitPoint, surfaceNormal);
}
```

## 11. LayerとLayerMask

各GameObjectは一つのLayerへ属します。Layer Collision MatrixはLayer同士が物理接触するかの大域設定です。Camera culling、Raycast filter等でもLayerMaskを使いますが、用途別の意味を文書化します。

例:

```text
Default
World
PlayerBody
EnemyBody
PlayerAttack
EnemyAttack
Projectile
Pickup
GameplayQuery
```

Layer数は有限です。Character種別ごとに無限にLayerを増やさず、「衝突カテゴリ」を表し、Team/Owner/Invincible等の動的意味はGameplay dataで判定します。

## 12. LayerMaskのbit

Layer indexとLayerMaskは別です。

```csharp
int enemyLayerIndex = LayerMask.NameToLayer("EnemyBody");
int enemyMask = 1 << enemyLayerIndex;

// 複数LayerをInspectorから指定できる。
[SerializeField] private LayerMask hittableLayers;
```

`NameToLayer`が失敗すると`-1`になり得ます。`1 << -1`のような不正なmaskを作らないよう、起動時validatorでLayer存在を検査します。文字列を各所へ散らさず設定Assetへ集約します。

## 13. QueryTriggerInteraction

RaycastやOverlap queryがTriggerを含むかは、Project全体設定と`QueryTriggerInteraction`引数の組合せで決まります。戦闘queryでは`UseGlobal`に隠さず、`Collide`または`Ignore`を明示すると設定変更に強くなります。

```csharp
bool found = Physics.Raycast(
    origin,
    direction,
    out RaycastHit hit,
    maxDistance,
    hittableLayers,
    QueryTriggerInteraction.Ignore);
```

## 14. Raycast、Shape Cast、Overlap

- Raycast: 線分方向の最初または複数hit。視線、地面、弾道。
- Sphere/Capsule/Box Cast: 厚みのある形を移動させたsweep。Character移動や太い攻撃。
- Overlap: その瞬間に形状内へいるCollider集合。近接攻撃、範囲検索。
- ClosestPoint / ComputePenetration: 距離や重なり解消の補助。対応形状・制約を確認。

Ray一本で高速Characterの体積を表すと角をすり抜けます。Capsule Cast等、実際のbody形状に近いqueryを検討します。

## 15. NonAlloc query

頻繁な複数hit queryで毎回配列を生成するとGC Allocationになります。事前確保bufferへ書くNonAlloc APIを使えます。

```csharp
using UnityEngine;

public sealed class MeleeScanner : MonoBehaviour
{
    private readonly Collider[] results = new Collider[32];

    [SerializeField] private LayerMask targetLayers;

    public int Scan(Vector3 center, Vector3 halfExtents, Quaternion rotation)
    {
        int count = Physics.OverlapBoxNonAlloc(
            center,
            halfExtents,
            results,
            rotation,
            targetLayers,
            QueryTriggerInteraction.Collide);

        // countがbuffer長と同じならoverflowで結果が欠けた可能性を記録する。
        int safeCount = Mathf.Min(count, results.Length);
        for (int i = 0; i < safeCount; ++i)
        {
            Collider collider = results[i];
            if (collider != null)
            {
                ProcessCandidate(collider);
            }

            // 長寿命参照を残したくない場合は使用後にclearする。
            results[i] = null;
        }

        return safeCount;
    }
}
```

bufferを大きくすれば常に正解ではありません。

- 最大候補数をGameplay仕様から決める。
- 満杯をtelemetry/debugで検出する。
- overflow時に拡張、再query、打切りのどれか決める。
- hit順序を保証されたと思わない。必要なら明示的にsortする。

Unity 6公式Manualは、頻繁なqueryではNonAlloc版やJob Systemのbatch queryも検討するよう案内しています。最適化前後をProfilerで測ります。

## 16. Queryの同期タイミング

Transformを書き換えた直後、Physics Scene内部の形状がいつ同期されるかを理解しないと、queryが旧位置を読むことがあります。`Physics.SyncTransforms`で強制同期できてもcostがあるため、各所から乱用しません。

方針:

1. Physics bodyはRigidbody APIで固定stepに動かす。
2. queryするphaseを決める。
3. Transform teleportは集約する。
4. Auto Sync Transforms設定と利用版仕様を確認する。

## 17. DiscreteとContinuous Collision Detection

Discreteは各stepの位置で接触を調べるため、高速・小型物体が壁の反対側へ移動するtunnelingが起こり得ます。

```text
step N           step N+1
bullet ●   |wall|   ●
          接触位置を飛び越える
```

Continuous系はsweep/speculativeな方法で通り抜けを減らしますがCPU costと対応形状・body modeの制約があります。全objectへ最大設定を付けず、高速Projectile等へ限定し、3D/2Dそれぞれのmodeを確認します。

別案:

- ProjectileをRay/Sphere Castによる明示的sweepで動かす。
- 速度とCollider厚に応じてsubstepする。
- Gameplay上の最大速度を制限する。
- 見た目弾と即時hit queryを分離する。

## 18. Interpolation

Physicsは固定step、renderは可変frameなので、Rigidbody表示がかくつくことがあります。Interpolationは過去のphysics状態等からrender姿勢を滑らかにします。

注意:

- 見た目に一step相当の遅延を持つ方式がある。
- Cameraはinterpolated後の姿勢を追う必要がある。
- teleport時には補間履歴の扱いを確認する。
- interpolationはsimulation精度を上げる機能ではない。

## 19. Sleep

静止bodyをsleepさせることでsolver costを減らします。毎frame微小なTransform変更、force、property書換えをすると起こし続ける場合があります。`Never Sleep`を広範囲に使わず、必要性をProfilerで示します。

## 20. Physics Material

摩擦と反発を制御します。Character Controller用途では高い摩擦が壁張り付きや坂挙動を生むことがあります。

- Dynamic/Static Friction。
- Bounciness。
- Combine mode。
- 2Dと3DのMaterial Assetは別。

Gameplay accelerationとPhysics摩擦を同時に調整すると原因が読めなくなるため、Character Motorの速度制御とsurface materialの責務を分けます。

## 21. Hitbox、Hurtbox、Body Collider

```text
Character Root
├─ Body Collider   : World/Character同士の移動衝突
├─ Hurtbox         : 攻撃される領域
├─ Hitbox Root
│  └─ Active Hitbox: 攻撃中だけquery/有効化
└─ Sensor          : lock-on、地面、ledge等
```

一つのColliderへ全部の意味を背負わせないことで、移動衝突と戦闘判定を独立調整できます。

## 22. Physics callback方式と明示query方式

### Callback方式

Hitbox Triggerを有効にして`OnTriggerEnter`で候補を得ます。

長所:

- Unityが重なり開始を通知する。
- 継続領域を自然に表現できる。

短所:

- Physics step、enable timing、再入場、複数Colliderで発火管理が複雑。
- 攻撃一回につき一対象一hitの履歴が必要。

### 明示query方式

Combat tickでOverlap/Castを行います。

長所:

- いつ判定したか明示的。
- replay/debug用にquery形状と結果を記録しやすい。

短所:

- 自分でbuffer、重複排除、sweep、overflowを管理する。

高速アクションでは、移動はPhysics、攻撃は明示queryという分離も有効です。

## 23. 一攻撃一対象一hit

敵に複数Hurtboxがあると、一回のOverlapで同じ敵が複数回返ります。ColliderではなくCombat Entity IDで重複排除します。

```csharp
using System.Collections.Generic;

public readonly struct AttackInstanceId
{
    public AttackInstanceId(uint value) => Value = value;
    public uint Value { get; }
}

public sealed class HitLedger
{
    private readonly HashSet<int> damagedEntityIds = new();

    public void BeginAttack()
    {
        damagedEntityIds.Clear();
    }

    public bool TryRegister(int entityId)
    {
        return damagedEntityIds.Add(entityId);
    }
}
```

multi-hit attackなら、hit index、再hit interval、attack instance IDをkeyへ含めます。Collider instance IDだけを保存するとpool再利用や複数Hurtboxの意味を誤ります。

## 24. DamageをPhysics callback内で完結させない

```text
Physics候補
 → HurtboxからCombat Entityをresolve
 → Team/Owner/Invincible/既hitを検査
 → HitRequestをCombat Systemへ送る
 → Damage/Poise/Knockbackを確定
 → Hit Event
 → Animation/VFX/Audio/Camera/Hit Stop
```

Physics layerは早期filter、Gameplay ruleは最終判断です。Layerだけで味方攻撃、反射、召喚主、魅了状態等を全て表現しません。

## 25. Knockback

Dynamic RigidbodyへImpulseを与える方法と、Character Motorの外力channelへ速度を加える方法があります。

```csharp
public void ApplyKnockback(Vector3 direction, float speed)
{
    Vector3 horizontal = Vector3.ProjectOnPlane(direction, Vector3.up).normalized;

    // Character Motorが所有する外力速度へ加算する概念例。
    externalVelocity += horizontal * speed;
}
```

Animator、NavMeshAgent、CharacterController、Rigidbodyが同じTransformを奪い合わないよう、移動の最終所有者を一つにします。

## 26. Ground Check

足元Ray一本だけでは段差や坂の端で不安定です。Capsule/Sphere Cast、複数sample、接触normalを使います。

判定値:

- 接地距離。
- slope angle: `Vector3.Angle(normal, up)`。
- ground bodyの速度。
- grace time/coyote time。
- snap distance。
- one-way platform等のfilter。

Grounded boolだけでなく、GroundHit構造体としてnormal、point、body、timeを保持するとMotorとAnimationが利用できます。

## 27. Physics Scene

Sceneは3D/2D Physics Sceneを持ち得ます。複数Sceneのlocal physics、予測simulation、test隔離等に使えますが、通常SceneとPhysics Sceneの所属、manual simulation、query先を明示する必要があります。

defaultの`Physics.Raycast`が、意図するlocal Physics Sceneをqueryするとは限りません。高度な構成では`PhysicsScene`/`PhysicsScene2D` instanceへqueryします。

## 28. 2D特有の注意

- Z座標ではなくXY平面とZ回転が基本。
- `Rigidbody2D.bodyType`はDynamic/Kinematic/Static。
- `gravityScale`をbodyごとに持つ。
- `useFullKinematicContacts`等で接触組合せが変わる。
- Composite Collider 2DでTilemap境界を統合できる。
- Contact Filter 2DはLayer、depth、normal angle等をfilterできる。
- 3Dの設定名をそのまま当てはめない。

Dynamic Rigidbody2DをTransformで直接動かさず、force、velocity、MovePosition等を意図に応じて使います。

## 29. Layer Override

Unity 6系ではLayer Collision Matrixに加え、Collider/Rigidbody単位のinclude/exclude Layer overrideやpriorityがあります。局所例外を作れますが、両Colliderの判断が競合する場合の仲裁規則があります。

多用するとMatrixだけ見ても接触理由が分からなくなるため:

- 基本規則はMatrix。
- overrideは限定したPrefab。
- Inspector validatorで例外を一覧化。
- priority競合をtest。
- 利用Unity Versionの仕様を確認。

## 30. 性能

Physics costを増やす代表例:

- Dynamic body、Collider、contact数が多い。
- 複雑Mesh Collider、頻繁な形状変更。
- Layer filterが広すぎる。
- 毎frame大量のRay/Overlap。
- callback内の`GetComponent`、LINQ、配列生成、ログ。
- Continuous modeの過剰利用。
- sleepを妨げる微小更新。
- Static Colliderを頻繁に移動。
- Transform同期の乱用。

改善順序:

1. Physics Profiler/Profilerで測る。
2. Layer Matrixで不要なpairを切る。
3. Colliderをprimitive化する。
4. query頻度と範囲を下げる。
5. NonAlloc/batch化する。
6. simulation timestepを品質予算に合わせる。

## 31. Debug可視化

判定は見えなければ調整できません。

- Scene ViewのPhysics Debugger。
- GizmosでCastの始点・終点・半径を描く。
- hit時にpoint/normalを描く。
- Layer、Entity ID、Attack IDをoverlay表示。
- query件数、buffer overflow、重複排除数を記録。
- Replay可能な入力/tickと判定結果を保存。

```csharp
private void OnDrawGizmosSelected()
{
    Gizmos.color = Color.red;
    Gizmos.matrix = Matrix4x4.TRS(transform.position, transform.rotation, Vector3.one);

    // OverlapBoxへ渡すhalfExtentsに対し、Gizmos cubeはfull size。
    Gizmos.DrawWireCube(Vector3.forward, attackHalfExtents * 2.0f);
}
```

Editor表示と実際のquery引数が同じdata sourceを読むようにし、二重入力によるずれを防ぎます。

## 32. よくある不具合

- 2D Colliderへ3D callbackを書いて反応しない。
- Layer indexをLayerMaskとして渡す。
- TriggerをProject global設定任せでquery結果が変わる。
- Dynamic RigidbodyをTransformで毎frame移動する。
- FixedUpdateがrender frameごとに1回だと思う。
- 高速ProjectileがDiscreteで壁を抜ける。
- Enemyの複数Hurtboxへ多重damage。
- Exit callbackだけで状態を解除する。
- NonAlloc buffer満杯を無視して敵が判定から消える。
- Raycast hit順が距離順だと思う。
- 攻撃Colliderと移動Colliderを一つにする。
- Physics callback内でdamage、VFX、Cameraを全部直接呼ぶ。
- Root Motion、NavMeshAgent、RigidbodyがTransformを奪い合う。
- Prefab VariantごとのLayer設定漏れ。

## 33. Test Matrix

| 観点 | Test |
|---|---|
| Frame | 30/60/120 FPS、長いstall |
| Fixed step | 標準、細かい、粗い |
| Speed | 静止、通常、高速、teleport |
| Shape | primitive、compound、mesh |
| Body | Dynamic、Kinematic、Static |
| Layer | 許可、拒否、override競合 |
| Trigger | global、Collide、Ignore |
| Attack | 単hit、多段、範囲、同時hit |
| Target | 複数Hurtbox、死亡、無敵、交代中 |
| Buffer | 0件、満杯、overflow |
| Scene | load直後、unload中、local physics |

## 34. 設計チェックリスト

- 2D/3Dのどちらを採用するか明確か。
- 各bodyがDynamic/Kinematic/Staticである理由を説明できるか。
- TransformとPhysicsの最終所有者は一つか。
- Layer MatrixとPrefab layerをVersion Controlでreviewしたか。
- queryのTrigger方針を明示したか。
- Cast/Overlapのbuffer overflowを検出できるか。
- attack instanceごとの重複排除があるか。
- ColliderからCombat Entityを安全にresolveできるか。
- Hitbox/Hurtbox/Body Colliderを分離したか。
- Slow Motion、Hit Stop、PauseでPhysics挙動をtestしたか。
- Debug表示が実際の判定dataと一致するか。
- Profiler測定を行ったか。

## 公式資料

- [Unity Manual: 3D Physics](https://docs.unity3d.com/6000.0/Documentation/Manual/PhysicsSection.html)
- [Unity Manual: Rigidbody component reference](https://docs.unity3d.com/6000.0/Documentation/Manual/class-Rigidbody.html)
- [Unity API: Rigidbody.collisionDetectionMode](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/Rigidbody-collisionDetectionMode.html)
- [Unity Manual: Optimize physics queries](https://docs.unity3d.com/6000.0/Documentation/Manual/physics-optimization-raycasts-queries.html)
- [Unity API: Physics.OverlapBoxNonAlloc](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/Physics.OverlapBoxNonAlloc.html)
- [Unity Manual: Rigidbody 2D](https://docs.unity3d.com/6000.0/Documentation/Manual/2d-physics/rigidbody/rigidbody-2d-landing.html)
- [Unity Manual: Dynamic Rigidbody 2D](https://docs.unity3d.com/6000.0/Documentation/Manual/2d-physics/rigidbody/body-types/dynamic/dynamic-body-type-reference.html)
- [Unity API: Collider.includeLayers](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/Collider-includeLayers.html)
- [Unity API: Collider2D.layerOverridePriority](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/Collider2D-layerOverridePriority.html)

