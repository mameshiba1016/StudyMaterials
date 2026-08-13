# 2D衝突図形

衝突判定は、画像の透明部分を含む四角形をそのまま比較する処理ではありません。ゲームルール用の単純な幾何図形を定義し、座標空間を揃えて交差を調べます。

## AABB

軸平行境界ボックス（Axis-Aligned Bounding Box）は回転しない長方形です。

```cpp
struct Aabb
{
    Vector2 minimum{}; // 左上または各軸の最小値。
    Vector2 maximum{}; // 右下または各軸の最大値。
};

bool Overlaps(const Aabb& a, const Aabb& b)
{
    // いずれかの軸で完全に離れていれば交差しない。
    if (a.maximum.x <= b.minimum.x || b.maximum.x <= a.minimum.x)
    {
        return false;
    }

    if (a.maximum.y <= b.minimum.y || b.maximum.y <= a.minimum.y)
    {
        return false;
    }

    return true;
}
```

この例では辺が触れるだけの場合を非衝突としています。`<`か`<=`かは仕様です。床への接地判定では接触を含めたい場合があります。

中心とhalf extentで持つ形式もあります。

```cpp
struct CenterAabb
{
    Vector2 center{};
    Vector2 halfSize{};
};
```

どちらを採用しても、負サイズを許さず正規化します。

## 円

```cpp
struct Circle
{
    Vector2 center{};
    float radius{};
};

bool Overlaps(const Circle& a, const Circle& b)
{
    const Vector2 difference{b.center - a.center};
    const float combinedRadius{a.radius + b.radius};
    return LengthSquared(difference) < combinedRadius * combinedRadius;
}
```

平方根を使わず二乗距離を比較します。半径が負でないことを作成時に検証します。

## 点と矩形

```cpp
bool Contains(const Aabb& box, Vector2 point)
{
    return point.x >= box.minimum.x &&
           point.x <  box.maximum.x &&
           point.y >= box.minimum.y &&
           point.y <  box.maximum.y;
}
```

UI選択やマウス判定に使えます。画面座標のマウスをWorld AABBへ使う場合、Cameraの逆変換が必要です。

## 円とAABB

円中心に最も近い矩形内の点を求め、距離を比較します。

```cpp
bool Overlaps(const Circle& circle, const Aabb& box)
{
    const Vector2 closest{
        std::clamp(circle.center.x, box.minimum.x, box.maximum.x),
        std::clamp(circle.center.y, box.minimum.y, box.maximum.y)
    };

    return LengthSquared(circle.center - closest) <
           circle.radius * circle.radius;
}
```

## 線分とRay

- Line：両方向へ無限。
- Ray：開始点から一方向へ無限。
- Segment：二端点間だけ。

同じ「線」と呼ばれても範囲が違います。弾の高速移動、視線、マウス選択に利用します。交点だけでなく、距離`time`、法線、対象IDを返すと後続処理に使えます。

```cpp
struct RaycastHit
{
    EntityId entity{};
    Vector2 point{};
    Vector2 normal{};
    float distance{};
};
```

## OBBとSAT

回転矩形（Oriented Bounding Box）はAABBより正確ですが高コストです。凸図形同士はSeparating Axis Theoremにより、候補軸への投影区間が全軸で重なるか調べられます。図形の辺法線を候補軸にし、一軸でも分離すれば非衝突です。

## 凸と凹

SATなど単純なアルゴリズムは凸図形を前提にします。凹形状は凸パーツへ分割する、タイル集合で表す、専用アルゴリズムを使う方法があります。複雑な輪郭を一個のColliderにするより、ゲーム上必要な単純形状へ近似する方が安定します。

## Hit Box・Hurt Box・Body Collider

- Hit Box：攻撃が当たる領域。
- Hurt Box：攻撃を受ける領域。
- Body Collider：地形・他キャラクターとの物理衝突。
- Trigger：押し戻さず侵入を通知する領域。

これらを同じ一個の矩形にすると、戦闘調整と移動調整が干渉します。用途、レイヤー、Ownerを分けます。

## ローカルからワールドへ

```cpp
Aabb MakeWorldAabb(const Aabb& local, Vector2 position, Facing facing)
{
    Aabb result{local};

    if (facing == Facing::Left)
    {
        // ローカルXを原点に対して反転し、min/maxを並べ直す。
        result.minimum.x = -local.maximum.x;
        result.maximum.x = -local.minimum.x;
    }

    result.minimum += position;
    result.maximum += position;
    return result;
}
```

描画スプライトの反転とCollider変換を同じFacingから導出します。

## デバッグ描画

Collider種別ごとに色を変え、法線、交点、Owner ID、接触状態を表示します。描画画像と判定形状のずれを目視できることが、調整速度を大きく左右します。
