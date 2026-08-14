# 第17章 3D Collision・Physics設計

3D Collisionは形状同士の重なり、Physicsは質量・速度・拘束から運動を解く仕組みです。Character操作では完全な剛体Simulationより、予測可能なCharacter ControllerとQueryの組合せが重要です。

## 1. Collision Pipeline

```text
Collider更新
→ Broad Phase候補
→ Narrow Phase
→ Contact生成
→ Trigger/Event
→ 位置・速度解決
→ Gameplayへ結果通知
```

## 2. 基本形状

```cpp
struct Sphere3D { Vec3 center{}; float radius = 0.0F; };
struct Aabb3D { Vec3 min{}; Vec3 max{}; };
struct Capsule3D { Vec3 a{}; Vec3 b{}; float radius = 0.0F; };

struct Obb3D final
{
    Vec3 center{};
    Basis axes{};
    Vec3 halfExtents{};
};
```

## 3. Sphere

回転の影響を受けず、距離二乗だけで高速に判定できます。Character全身には粗い一方、ProjectileやBroad Phaseに便利です。

```cpp
[[nodiscard]] constexpr bool Overlaps(Sphere3D a, Sphere3D b) noexcept
{
    const float sum = a.radius + b.radius;
    return LengthSquared(a.center - b.center) < sum * sum;
}
```

## 4. AABB

World軸へ平行な箱です。回転Objectを囲むと余白が増えますが、Broad Phaseに向きます。

```cpp
[[nodiscard]] constexpr bool Overlaps(Aabb3D a, Aabb3D b) noexcept
{
    return a.min.x < b.max.x && b.min.x < a.max.x &&
           a.min.y < b.max.y && b.min.y < a.max.y &&
           a.min.z < b.max.z && b.min.z < a.max.z;
}
```

## 5. OBB

Objectの回転軸に沿う箱です。AABBより密ですが、OBB同士はSeparating Axis Theoremで最大15軸を調べるなど複雑になります。

## 6. Capsule

線分を半径分膨らませた形状です。Characterは角へ引っ掛かりにくく、階段・斜面で安定しやすいためControllerに向きます。

## 7. Triangle

Stage Meshの最終Narrow Phase単位です。Triangle Soupを全件調べず、空間分割で候補を絞ります。

## 8. LayerとMask

```cpp
enum CollisionLayer3D : std::uint32_t
{
    Player       = 1u << 0,
    Enemy        = 1u << 1,
    World        = 1u << 2,
    PlayerAttack = 1u << 3,
    EnemyAttack  = 1u << 4,
    Projectile   = 1u << 5
};
```

味方、攻撃者自身、装飾Meshなど不要な組合せをNarrow Phase前に除外します。

## 9. TriggerとSolid

Triggerは重なりEventだけ、Solidは移動を制約します。Hurtbox/Hitboxを物理押し戻しへ使わず、PushboxやWorld Colliderと分けます。

## 10. Body Type

- Static: Stage、動かない壁。
- Kinematic: Code/Animationで動く床。
- Dynamic: 力と拘束で動く物体。

Staticとして登録したObjectを頻繁に動かすとBroad Phase構造の更新Costが増えます。

## 11. Queryの種類

- Overlap: 現在重なる形状。
- Ray Cast: 線上の最初/全Hit。
- Shape Cast: Sphere/Capsule/Boxを移動した軌跡。
- Closest Point: 表面の最近点。

目的に合う最小Queryを使います。

## 12. Ray

```cpp
struct Ray3D final
{
    Vec3 origin{};
    Vec3 unitDirection{0,0,1};
    float maxDistance = 0.0F;
};

struct Hit3D final
{
    bool hit = false;
    float distance = 0.0F;
    float fraction = 0.0F;
    Vec3 point{};
    Vec3 normal{};
    std::uint32_t colliderId = 0;
    int materialId = -1;
};
```

## 13. Closest/Any/All

遮蔽確認はAny、銃弾はClosest、貫通攻撃はAllを距離順で返します。常に全Hitを確保するとMemoryとSort Costが増えます。

## 14. Shape Cast

CameraやCharacterは体積があるためRayよりSphere/Capsule Castが適します。開始時点で既に重なっている場合の結果も仕様化します。

## 15. Contact

```cpp
struct Contact3D final
{
    std::uint32_t a = 0;
    std::uint32_t b = 0;
    Vec3 point{};
    Vec3 normalFromAToB{};
    float penetration = 0.0F;
    bool trigger = false;
};
```

Normal方向とPenetration符号を全形状で統一します。

## 16. Broad Phase

Dynamic AABB Tree、Sweep and Prune、Uniform Grid、BVHが候補です。Stage TriangleはBVHや空間分割、動的ObjectはDynamic Treeなど用途で分けられます。

## 17. Fat AABB

Collider AABBへ小Marginを足し、微小移動のたびにTreeへ再挿入しない手法です。速度予測分を拡張する方法もあります。

## 18. Narrow Phase

Sphere-Sphere、Capsule-Triangle、OBB-OBBなど形状Pair別Algorithmを使います。Pair分岐を中央Dispatcherへ集めます。

## 19. SAT

凸形状を候補軸へ投影し、区間が分離する軸が一つでもあれば非接触です。OBB同士では各Boxの3軸と外積9軸を候補にします。平行軸の数値誤差を処理します。

## 20. GJK/EPAの予習

GJKは凸形状の交差、EPAは侵入深度・Normalを求める手法です。実装難度と数値安定性が高いため、必要形状が少ない段階ではPair別実装も有効です。

## 21. Discrete Collision

更新後位置だけを判定すると高速物体が薄い壁を飛び越えます。これをTunnelingと呼びます。

## 22. Continuous Collision

前位置から次位置までのSwept Volumeを調べ、最初の衝突時刻（TOI）まで進めます。残り時間を接線方向へ進める反復が必要です。

## 23. Sub-step

大きな移動を小Stepへ分割する簡易策です。速度に応じCostが変わり、極端な高速には完全ではありません。固定StepとCastを併用します。

## 24. Character Controller State

```cpp
struct CharacterController3D final
{
    Vec3 position{};
    Vec3 velocity{};
    float radius = 0.35F;
    float height = 1.8F;
    float skinWidth = 0.02F;
    float maxSlopeRadians = ToRadians(45.0F);
    float stepHeight = 0.3F;
    bool grounded = false;
    Vec3 groundNormal{0,1,0};
};
```

## 25. Skin Width

Collider表面から小距離を保ち、数値誤差による接触・非接触の振動を減らします。見た目Scaleに対して大きすぎない値にします。

## 26. Move and Slide

希望DeltaをCapsule Castし、Hit直前へ移動し、残DeltaからNormal方向成分を除いて接線方向へ滑らせます。数回反復して複数壁を処理します。

```cpp
[[nodiscard]] constexpr Vec3 Slide(Vec3 motion, Vec3 unitNormal) noexcept
{
    return motion - unitNormal * Dot(motion, unitNormal);
}
```

## 27. Corner

二面に同時接触すると順次Slideが順序依存になります。Hit距離・Collider IDで安定Sortし、反復上限を設けます。

## 28. Ground Check

足元へ短いCapsule/Sphere Castを行い、Hit Normalと距離を調べます。下向きRay一本だけでは段差端で不安定です。

## 29. Slope判定

```cpp
const float minGroundDot = std::cos(maxSlopeRadians);
const bool walkable = Dot(hit.normal, Vec3{0,1,0}) >= minGroundDot;
```

角度を毎回`acos`せずDot閾値で比較できます。

## 30. Steep Slope

登坂角を超える面は壁としてSlideさせるか、重力方向へ滑らせます。Groundedにはしません。

## 31. Step Offset

低い障害物へ当たったら、上へCast→前へCast→下へCastして乗れる位置を探します。頭上空間、Slope、段差高さを検証します。

## 32. Snap to Ground

下り坂でわずかに浮くのを防ぐため、短距離だけ床へ吸着します。Jump中や上向き速度時は無効にします。

## 33. Ceiling

上方向Hit時は上向き速度を0にします。Capsule高さ変更で立ち上がる時も頭上Overlapを確認します。

## 34. Crouch

Capsuleの高さと中心を変え、足元位置を維持します。立ちへ戻す前に大Capsuleが重ならないかOverlap Queryします。

## 35. Moving Platform

床Objectの前Transformと現在Transformの差をCharacterへ適用します。平行移動だけでなく回転Platform上の相対位置も考慮します。

## 36. Platform所有

Ground Collider IDとLocal接触点を保持し、Platform破棄時にID有効性を確認します。生Pointerを長期保持しません。

## 37. Gravity

```cpp
velocity += gravity * fixedDelta;
const Vec3 desiredDelta = velocity * fixedDelta;
```

固定更新で積分します。Grounded時の小さい下向き速度をどう維持するか仕様化します。

## 38. Jump

希望到達高`h`と重力大きさ`g`から初速は概念的に`sqrt(2gh)`です。可変Jump、Coyote Time、入力BufferはController上位で管理します。

## 39. Dynamic Body

```cpp
struct RigidBody final
{
    float inverseMass = 0.0F;
    Vec3 linearVelocity{};
    Vec3 angularVelocity{};
    Vec3 accumulatedForce{};
};
```

Static BodyはInverse Mass 0として表現できます。

## 40. Impulse

瞬間的に速度を変える量です。Damage Knockbackを毎Frame Forceとして加えるか一回Impulseにするかで結果が違います。

## 41. RestitutionとFriction

Restitutionは反発、Frictionは接線方向速度への抵抗です。0～1の単純係数だけではすべてのMaterial組合せを表せないため、Combine Ruleを決めます。

## 42. Solver

Contact拘束を反復して速度・位置を修正します。反復数を増やすと安定しやすい一方Costが増えます。Character Controllerと一般Rigid Bodyを分ける構成も有効です。

## 43. Sleeping

速度が小さいBodyを一時停止しCostを減らします。接触、Force、Platform移動でWakeします。Gameplay上動く可能性のあるObjectを誤って眠らせ続けません。

## 44. MV1 Collision Setup

```cpp
if (MV1SetupCollInfo(stageModel, -1, 32, 8, 32) == -1)
{
    // Model全体のCollision構築失敗。
}
```

X/Y/Z分割数はMemoryと候補数のTrade-offです。Stage形状とQuery統計で調整します。

## 45. Frame Index契約

Setupで`-1`を使ったならQueryも`-1`を使います。個別FrameでSetupした情報とは別扱いです。Model Handle、Frame Index、Mesh条件をCollision Resource Keyとして扱います。

## 46. Collision情報の寿命

`MV1TerminateCollInfo`で後始末します。Model削除でも後始末されますが、RAIIと全体削除を混在させず所有者を決めます。

## 47. Refresh

Model TransformやAnimationでCollision対象が変わったら`MV1RefreshCollInfo`で更新します。Static Stageは毎FrameRefreshしません。

## 48. Line Query

```cpp
const MV1_COLL_RESULT_POLY hit = MV1CollCheck_Line(
    stageModel, -1, ToDx(start), ToDx(end));

if (hit.HitFlag == 1)
{
    const Vec3 point = FromDx(hit.HitPosition);
    const Vec3 normal = FromDx(hit.Normal);
}
```

結果にはFrame、Polygon、Material、Triangle三点、Normal等が含まれます。

## 49. Sphere/Capsule Query

複数Polygonに当たるため`MV1_COLL_RESULT_POLY_DIM`が返ります。`HitNum`が件数、`Dim`が配列です。

## 50. 結果MemoryのRAII

```cpp
class ScopedMv1PolyResults final
{
public:
    explicit ScopedMv1PolyResults(MV1_COLL_RESULT_POLY_DIM value)
        : value_(value) {}

    ~ScopedMv1PolyResults()
    {
        MV1CollResultPolyDimTerminate(value_);
    }

    ScopedMv1PolyResults(const ScopedMv1PolyResults&) = delete;
    ScopedMv1PolyResults& operator=(const ScopedMv1PolyResults&) = delete;

    [[nodiscard]] const MV1_COLL_RESULT_POLY_DIM& Get() const noexcept
    { return value_; }

private:
    MV1_COLL_RESULT_POLY_DIM value_{};
};
```

公式仕様上、複数結果は動的Memoryを確保するため必ずTerminateします。

## 51. 結果を外へ借用しない

RAII Scope終了後に`Dim`内Pointerを保持するとDanglingです。必要なPoint、Normal、Material IDを自作`Hit3D`へCopyします。

## 52. 最適Hitの選択

Sphere Queryは複数面へ当たります。移動方向、侵入深度、床Normal、距離を基準に解決候補を選びます。配列先頭が最適とは限りません。

## 53. Material ID

Hit Materialから足音、火花、摩擦、Damage倍率を解決できます。Render Material番号を直接Gameplay Ruleへ固定せず、対応Tableへ変換します。

## 54. Stage Collision Model

表示ModelとCollision Modelを分けると、Polygon数、穴、段差、Material TagをGameplay向けに簡略化できます。見た目変更で移動判定が変わりにくくなります。

## 55. Dynamic Character Hitbox

BoneへSphere/Capsuleを付け、Animation Pose設定後にWorld位置を更新します。Skinned Mesh Triangle全体で毎Frame判定するより制御しやすいです。

## 56. Attack Sweep

Weapon先端の前位置と現在位置をCapsule/Sphere Sweepし、高速な剣を取りこぼさないようにします。一Attack Instanceで同じOwnerへ一度だけHitさせます。

## 57. Ragdollとの境界

Gameplay中はController、死亡後はRagdollなど所有者を切り替えます。切替時にAnimation PoseからPhysics Body Transformを初期化し、復帰時は逆にBlendします。

## 58. MV1 Physics更新順

Model Transform、Animation Attach/Time、Bone補正を設定した後にMV1 Physicsを計算し、その後Socket/Collision/Drawへ進む公式制約を確認します。

## 59. Fixed Step

Physicsは固定刻みで更新します。大Deltaを一回で渡さずAccumulatorで複数Stepに分け、最大Step数でSpiral of Deathを防ぎます。

## 60. Determinism

Contact反復順、浮動小数点、並列化で結果が変わります。Collider IDで安定Sortし、Replayに必要な決定性Levelを定義します。

## 61. Debug Gizmo

- Collider形状とLayer色。
- Broad Phase AABB。
- Cast開始・終了とSwept体積。
- Hit Point、Normal、Distance。
- Ground NormalとSlope閾値。
- Contact PairとPenetration。
- MV1 Hit Triangle。

## 62. Metrics

- Collider/Body数。
- Broad Pair数。
- Narrow検査数。
- Contact数とSolver反復。
- Cast/Overlap回数。
- MV1 Query数、Hit Polygon数、確保量。
- Collision Setup/Refresh時間。
- CCD対象数。

## 63. よくある不具合

- Memory増加: `MV1CollResultPolyDimTerminate`忘れ。
- Model移動後ずれる: Coll InfoをRefreshしていない。
- Hitしない: SetupとQueryのFrame Index不一致。
- 壁を抜ける: Discrete判定だけ。
- 角で止まる: Capsule、Skin、Slide反復不足。
- 坂で跳ねる: Ground/Slope/Snap Policy不足。
- Bone Hitboxが遅れる: Pose設定前に更新。
- Physics結果が消える: Transform/Animation設定との順序違い。

## 64. Test

- Sphere/AABB境界規則。
- Zero長RayとCapsule。
- Ground角度の閾値前後。
- Step上限と頭上塞がり。
- 高速Projectileの薄壁Hit。
- Moving Platform回転追従。
- MV1結果RAIIが一度だけTerminate。
- Setup/Refresh/Query順序。

## 65. 設計チェックリスト

- [ ] CollisionとPhysicsを区別した。
- [ ] Layer/Maskで候補を減らした。
- [ ] CharacterにCapsuleとShape Castを使う。
- [ ] Ground、Slope、Step、Ceilingを別判定した。
- [ ] 高速物体へContinuous判定を使う。
- [ ] Physicsを固定刻みで更新する。
- [ ] MV1 Query前にColl InfoをSetupした。
- [ ] 動的ModelのColl InfoをRefreshした。
- [ ] SetupとQueryのFrame Indexを一致させた。
- [ ] 複数Polygon結果を必ずTerminateする。
- [ ] Bone HitboxをPose後に更新する。
- [ ] Debug統計を計測する。

## 66. 理解確認問題

1. CharacterにCapsuleが向く理由は何か。
2. RayとShape Castの違いは何か。
3. Tunnelingはなぜ起きるか。
4. Skin Widthの役割は何か。
5. Slope判定をDotで行う利点は何か。
6. Moving PlatformでLocal接触点が必要な理由は何か。
7. MV1 Coll Infoの分割数は何とTrade-offになるか。
8. SetupとQueryのFrame Indexを一致させる理由は何か。
9. `MV1_COLL_RESULT_POLY_DIM`を後始末する理由は何か。
10. 表示ModelとCollision Modelを分ける利点は何か。

## 67. 実践課題

1. Sphere/AABB/Capsule DataとGizmoを作る。
2. Layer/Mask付き3D Collision Worldを作る。
3. Capsule Move-and-Slideを実装する。
4. Ground/Slope/Step/Snapを実装する。
5. Moving Platform追従を作る。
6. Projectile Swept判定を作る。
7. MV1 Stage Coll InfoをRAII管理する。
8. Line/Sphere/Capsule結果を`Hit3D`へ変換する。
9. Bone HurtboxとWeapon Sweepを統合する。
10. Query数とHit Polygon数をProfiler表示する。

## 68. 公式資料

- [DXライブラリ 3D関数一覧](https://dxlib.xsrv.jp/function/dxfunc_3d.html)
- [MV1 Collision・Mesh関数](https://dxlib.xsrv.jp/function/dxfunc_3d_model_3.html)
- [DXライブラリ 3D Action Collisionサンプル](https://dxlib.xsrv.jp/program/dxprogram_3DAction.html)

Setup/Refresh/Terminate条件、Frame Index、結果構造体、分割数、Model物理の更新順は利用中バージョンの公式資料を正としてください。

## 69. 次章への接続

次章ではLighting・Shadowを扱い、Normal、Material、Directional/Point/Spot Light、Shadow Map、Bias、Cascades、動的CharacterとStageの照明をRender Passへ統合します。
