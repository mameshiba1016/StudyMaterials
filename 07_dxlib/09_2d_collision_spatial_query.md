# 第9章 2D Collision・Spatial Query

Collisionは「重なったか」だけではありません。候補を探し、形状を検査し、接触情報を作り、ゲームルールへEventを渡す仕組みです。本章の計算部分はDXライブラリから独立させ、描画APIは可視化だけに使います。

## 1. 用語

- Collider: 衝突判定用の形状と属性。
- Collision: 形状同士の接触。
- Trigger: 物理的に押し戻さずEventだけ発生。
- Query: Ray Castや範囲検索など世界への問い合わせ。
- Broad Phase: 安価な候補絞り込み。
- Narrow Phase: 正確な形状判定。

## 2. 見た目とCollider

Spriteの透明画素や輪郭を毎回調べるPixel Perfect判定は高Costで調整もしにくいため、通常は円・矩形・Capsuleなど単純形状を使います。見た目と判定は別Dataです。

## 3. 座標空間

Colliderは原則World座標で判定します。Screen座標はCameraで変化するため、Gameplay判定へ使いません。Debug描画時だけWorldからScreenへ変換します。

## 4. AABB

Axis-Aligned Bounding Boxは軸に平行な矩形です。

```cpp
struct Aabb final
{
    Vec2 min{}; // 左上を含む
    Vec2 max{}; // 右下を含まない半開区間
};

[[nodiscard]] constexpr bool Overlaps(const Aabb& a, const Aabb& b) noexcept
{
    return a.min.x < b.max.x && b.min.x < a.max.x &&
           a.min.y < b.max.y && b.min.y < a.max.y;
}
```

この式では辺が触れるだけなら非接触です。接触扱いにするなら`<=`へ変わります。どちらが正しいかを仕様化します。

## 5. CenterとHalf Extents

```cpp
struct CenterAabb final
{
    Vec2 center{};
    Vec2 half{};
};

[[nodiscard]] constexpr Aabb ToMinMax(const CenterAabb& b) noexcept
{
    return {b.center - b.half, b.center + b.half};
}
```

負のHalf Sizeを許しません。Load時または生成時に検証します。

## 6. 点とAABB

```cpp
[[nodiscard]] constexpr bool Contains(const Aabb& box, Vec2 p) noexcept
{
    return box.min.x <= p.x && p.x < box.max.x &&
           box.min.y <= p.y && p.y < box.max.y;
}
```

Mouse PickingではScreen座標をWorldへ逆変換してから使用します。

## 7. 円

```cpp
struct Circle final
{
    Vec2 center{};
    float radius = 0.0F;
};

[[nodiscard]] constexpr float LengthSquared(Vec2 v) noexcept
{
    return v.x * v.x + v.y * v.y;
}

[[nodiscard]] constexpr bool Overlaps(const Circle& a, const Circle& b) noexcept
{
    const float radiusSum = a.radius + b.radius;
    return LengthSquared(a.center - b.center) < radiusSum * radiusSum;
}
```

Square Rootを取らず二乗値を比較します。負半径は事前に拒否します。

## 8. 円とAABB

```cpp
#include <algorithm>

[[nodiscard]] constexpr bool Overlaps(const Circle& c, const Aabb& b) noexcept
{
    const Vec2 closest{
        std::clamp(c.center.x, b.min.x, b.max.x),
        std::clamp(c.center.y, b.min.y, b.max.y)
    };
    return LengthSquared(c.center - closest) < c.radius * c.radius;
}
```

AABB上の最接近点を求め、円中心との距離を比較します。端点の接触規則を全形状で揃えます。

## 9. 線分

```cpp
struct Segment final { Vec2 start{}; Vec2 end{}; };

[[nodiscard]] constexpr float Dot(Vec2 a, Vec2 b) noexcept
{
    return a.x * b.x + a.y * b.y;
}

[[nodiscard]] Vec2 ClosestPoint(const Segment& s, Vec2 p) noexcept
{
    const Vec2 ab = s.end - s.start;
    const float lengthSq = LengthSquared(ab);
    if (lengthSq <= 0.000001F) return s.start;
    const float t = std::clamp(Dot(p - s.start, ab) / lengthSq, 0.0F, 1.0F);
    return s.start + ab * t;
}
```

長さ0の線分をDivision by zeroにしません。

## 10. 線分と円

```cpp
[[nodiscard]] bool Overlaps(const Segment& s, const Circle& c) noexcept
{
    const Vec2 closest = ClosestPoint(s, c.center);
    return LengthSquared(closest - c.center) < c.radius * c.radius;
}
```

Laser、近接攻撃の軌跡、視線判定へ使えます。

## 11. Ray

Rayは始点と正規化方向、最大距離を持ちます。

```cpp
struct Ray2D final
{
    Vec2 origin{};
    Vec2 direction{1.0F, 0.0F};
    float maxDistance = 0.0F;
};

struct RayHit final
{
    bool hit = false;
    float distance = 0.0F;
    Vec2 point{};
    Vec2 normal{};
    std::uint32_t colliderId = 0;
};
```

方向を正規化しないなら`distance`の意味が変わるため、Query入口で検証します。

## 12. Normal

Normalは接触面から外向きの単位Vectorです。反射、押し戻し、地面判定に使います。どちらのColliderからどちらへ向くNormalかを契約に含めます。

## 13. Contact

```cpp
struct Contact final
{
    std::uint32_t a = 0;
    std::uint32_t b = 0;
    Vec2 normalFromAToB{};
    float penetration = 0.0F;
    Vec2 point{};
    bool trigger = false;
};
```

Booleanだけでなく、解決や演出に必要な情報を返します。

## 14. 最小押し戻し

AABB同士ではX軸とY軸の重なり量を求め、小さい方の軸へ押し戻せます。両者を半分ずつ動かすか、静的地形は動かさず動的物体だけ動かすかをBody種別で決めます。

## 15. Body種別

```cpp
enum class BodyType { Static, Kinematic, Dynamic };
```

- Static: 壁や床。動かない。
- Kinematic: Codeで動かすPlatform。
- Dynamic: 速度と衝突応答で動く。

Character Controllerは物理Bodyと異なる独自解決を使うことも多いです。

## 16. Trigger

攻撃範囲、Item取得、Checkpointは重なりEventだけ必要です。Triggerを押し戻し処理へ入れません。

## 17. LayerとMask

```cpp
enum CollisionLayer : std::uint32_t
{
    LayerPlayer        = 1u << 0,
    LayerEnemy         = 1u << 1,
    LayerWorld         = 1u << 2,
    LayerPlayerAttack  = 1u << 3,
    LayerEnemyAttack   = 1u << 4
};

[[nodiscard]] constexpr bool CanCollide(
    std::uint32_t layerA, std::uint32_t maskA,
    std::uint32_t layerB, std::uint32_t maskB) noexcept
{
    return (maskA & layerB) != 0 && (maskB & layerA) != 0;
}
```

片側だけでなく双方の許可を確認する契約例です。

## 18. Collider ID

PointerをEventへ保持するとObject破棄後にDanglingになります。世代付きHandle、Entity ID、Collider IDを使い、消費時に有効性を確認します。

## 19. Collision World

```cpp
struct Collider final
{
    std::uint32_t id = 0;
    std::uint32_t ownerId = 0;
    std::uint32_t layer = 0;
    std::uint32_t mask = 0;
    bool trigger = false;
    Aabb bounds{};
};

class CollisionWorld final
{
public:
    void Add(Collider collider);
    void Remove(std::uint32_t colliderId);
    void UpdateBounds(std::uint32_t colliderId, Aabb worldBounds);
    [[nodiscard]] std::vector<std::uint32_t> Query(const Aabb& area) const;
};
```

## 20. 登録変更の時期

Collision判定中にContainerへ直接追加・削除するとIteratorが無効化されます。変更CommandをQueueし、判定前後の安全な時点で反映します。

## 21. Broad Phase

全Collider組合せは`O(n²)`です。1000個なら約50万組あります。AABB、Uniform Grid、Sweep and Prune、Quadtreeで候補を減らします。

## 22. Uniform Grid

Worldを固定Size Cellへ分け、Colliderが重なるCellへIDを登録します。近傍Cellだけ検索できます。

```cpp
struct CellCoord final { int x = 0; int y = 0; };

[[nodiscard]] int ToCell(float position, float cellSize)
{
    return static_cast<int>(std::floor(position / cellSize));
}
```

負座標では切り捨てでなく`floor`が必要です。

## 23. Cell Size

小さすぎると一Colliderが多数Cellへ入り、大きすぎると候補が増えます。代表Collider Sizeと密度を基準にし、実際の候補数を計測します。

## 24. 重複Pair

複数Cellを共有する二Colliderは同じFrameに何度も候補になります。`(minId,maxId)`をPair Keyにし、一度だけNarrow Phaseへ送ります。

## 25. Query Filter

```cpp
struct QueryFilter final
{
    std::uint32_t layerMask = 0xFFFFFFFFu;
    std::uint32_t ignoredOwner = 0;
    bool includeTriggers = true;
};
```

攻撃者自身、味方、Triggerを除外する条件をQueryへ明示します。

## 26. Overlap Query

円範囲攻撃ではまず範囲AABBでBroad Phaseし、候補だけ円とのNarrow Phaseを行います。結果は距離順・ID順など必要な順へSortします。

## 27. Ray CastのClosestとAll

- Closest: 最も近い一件。射線や壁判定。
- All: 全Hitを距離順に返す。貫通Laser。
- Any: 一件でもあれば終了。遮蔽確認。

目的に合わせると不要な全件収集を避けられます。

## 28. Discrete Collisionのすり抜け

高速物体が1Frameで壁を飛び越えると、更新後位置で重ならず検出できません。これをTunnelingと呼びます。

## 29. Swept Query

前位置から新位置までの移動を線分・Swept Shapeとして検査し、最初の衝突時刻`time of impact`を求めます。弾丸、Dash、細い壁で重要です。

## 30. Sub-step

移動を複数小Stepへ分ける方法は簡単ですが、速度に応じてCostが増え、完全な解決ではありません。固定Step、Swept Query、Collider厚さを組み合わせます。

## 31. Collision Eventの段階

```cpp
enum class ContactPhase { Enter, Stay, Exit };
```

前Frame Pair集合と現在Pair集合を比較します。新規はEnter、継続はStay、消失はExitです。

## 32. Event Dispatch

判定中にDamageやObject破棄を即実行せず、Contact EventをQueueします。全判定後に安定した順序で消費し、同Frameの結果がContainer順へ依存しないようにします。

## 33. 攻撃判定の分類

- Hurtbox: Damageを受ける範囲。
- Hitbox: 攻撃が当たる範囲。
- Pushbox: Character同士の押し合い。
- Guard/Parry領域: 防御受付範囲・方向。

用途ごとにLayerとResponseを分けます。

## 34. 一回の攻撃で一度だけHit

```cpp
struct AttackInstance final
{
    std::uint64_t attackId = 0;
    std::vector<std::uint32_t> hitOwners{};

    [[nodiscard]] bool HasHit(std::uint32_t owner) const
    {
        return std::find(hitOwners.begin(), hitOwners.end(), owner)
            != hitOwners.end();
    }
};
```

Collider IDではなくOwner IDで記録すれば、同じ敵の複数Hurtboxへ同時に当たっても一回にできます。部位別Hitが必要ならPolicyを変えます。

## 35. Multi-hit

多段攻撃は無制限Stay Damageにせず、Hit間隔、最大回数、対象別CooldownをData化します。固定更新Tickまたは秒のどちらで管理するかを決めます。

## 36. Hit Stop中の判定

時間停止中に同じOverlapを毎Frame再処理しないよう、攻撃InstanceのHit履歴を使います。演出時間と判定寿命を別に持ちます。

## 37. Parry方向

位置の重なりだけでなく、攻撃方向と防御者ForwardのDot Product、受付時間、攻撃Tagを調べます。Collisionは候補Eventを出し、Combat Systemが最終判定します。

## 38. Ground Check

足元から短いRay/Castを下へ行い、Normalと距離で接地判定します。単一点Rayだけだと段差端で不安定なため、複数RayやBox/Capsule Castを検討します。

## 39. One-way Platform

上からのみ乗れる床は、前Frameの足位置、現在の移動方向、床上面を使います。単純Overlapだけでは下から通過できません。

## 40. Slope

接触Normalと上方向のDotから斜面角度を判断します。登坂可能角度を超える面は壁として扱います。2D AABBだけでは滑らかな斜面を表現できないため線分・Polygonが必要です。

## 41. PolygonとSAT

凸Polygon同士はSeparating Axis Theoremで判定できます。全辺Normal軸へ投影し、分離する軸が一つでもあれば非接触です。凹Polygonは凸分割するか別手法を使います。

## 42. 数値誤差

浮動小数点を完全一致比較せず、用途別Toleranceを使います。ただし何でも大きなEpsilonで直すと接触が膨らみます。World Scaleを統一します。

## 43. Resolution順序

複数壁との接触を順番に押し戻すと結果が順序依存になります。固定反復回数、安定Sort、拘束Solverなどが必要です。まず単純な構成で限界を記録します。

## 44. Update Pipeline

```text
入力
→ 速度計算
→ 予測移動
→ Broad Phase更新
→ Query/Narrow Phase
→ 位置・速度解決
→ Contact Event生成
→ Combat処理
→ 描画用Snapshot
```

## 45. Debug描画

```cpp
void DrawCollider(const Aabb& world, const RenderContext& context,
                  unsigned int color)
{
    const Vec2 a = context.ToScreen(world.min);
    const Vec2 b = context.ToScreen(world.max);
    DrawBoxAA(a.x, a.y, b.x, b.y, color, FALSE);
}
```

Layer、Trigger、接触中、Sleep状態を色分けし、Normalと接触点も描きます。

## 46. Debug統計

- Collider総数。
- Broad Phase候補Pair数。
- Narrow Phase検査数。
- Contact数。
- Query別所要時間。
- 最大Cell登録数。
- CCD/Swept検査数。

`n²`悪化を数値で発見します。

## 47. Determinism

`unordered_map`の反復順へ結果を依存させず、Pair IDで安定Sortします。浮動小数点、Compiler、Platformを跨ぐ完全決定性には追加設計が必要です。

## 48. よくある不具合

- 端で引っ掛かる: 接触規則、Epsilon、軸別解決を確認。
- 高速弾が抜ける: Swept QueryまたはSub-step。
- 一撃が毎Frame入る: Attack InstanceのHit履歴不足。
- Camera移動で判定がずれる: Screen座標で判定している。
- 同じEventが複数回: Grid間のPair重複を除去していない。
- 削除後Crash: Eventへ生Pointerを保持している。

## 49. Test

```cpp
static_assert(Overlaps(Aabb{{0, 0}, {10, 10}},
                           Aabb{{9, 9}, {20, 20}}));
static_assert(!Overlaps(Aabb{{0, 0}, {10, 10}},
                            Aabb{{10, 0}, {20, 10}}));
static_assert(Overlaps(Circle{{0, 0}, 5}, Circle{{8, 0}, 4}));
```

境界接触、包含、同一点、0 Size、負座標、高速移動をTestします。

## 50. チェックリスト

- [ ] World座標で判定する。
- [ ] 接触と非接触の境界規則が統一されている。
- [ ] Layer/Maskで不要Pairを除外する。
- [ ] Broad PhaseとNarrow Phaseを分ける。
- [ ] Query結果にID、点、Normal、距離がある。
- [ ] 判定中にContainerを変更しない。
- [ ] 高速物体へSwept Queryを使う。
- [ ] Triggerを押し戻さない。
- [ ] 攻撃InstanceごとにHit履歴を持つ。
- [ ] Debug形状と統計を表示する。

## 51. 実践課題

1. AABB、円、円対AABBを実装・Testする。
2. Layer/Mask付きCollision Worldを作る。
3. Uniform Gridで候補数を比較する。
4. Closest/All/Any Ray Queryを作る。
5. Enter/Stay/Exit Eventを作る。
6. Hurtbox/Hitboxと一回Hitを実装する。
7. 高速弾のすり抜けを再現しSwept判定で直す。
8. Collider、Normal、接触点、統計を描画する。

## 52. 参考資料

- [DXライブラリ 関数リファレンス](https://dxlib.xsrv.jp/dxfunc.html)

本章のCollision計算は標準C++で独立実装し、DXライブラリはDebug描画とGame Loop統合へ使用します。次章ではSceneとApplication Stateを扱い、Collision World、Audio、Resourceの生成・破棄順を統合します。
