# 3Dベクトル・内積・外積・幾何

Vectorは位置差、方向、速度、法線、力を表します。同じ`Vector3`でも意味・単位・空間が異なります。

## 基本演算

```cpp
Vector3 operator+(Vector3 a, Vector3 b)
{
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}

Vector3 operator*(Vector3 value, float scalar)
{
    return {value.x * scalar, value.y * scalar, value.z * scalar};
}
```

PositionへVelocity×Secondsを加えます。

## 長さと正規化

```cpp
float LengthSquared(Vector3 v)
{
    return v.x * v.x + v.y * v.y + v.z * v.z;
}

Vector3 NormalizeOrZero(Vector3 v)
{
    const float lengthSquared{LengthSquared(v)};
    if (lengthSquared <= 1.0e-12F)
    {
        return {};
    }

    return v * (1.0F / std::sqrt(lengthSquared));
}
```

0 Vectorの正規化を防ぎます。単位Scaleに合う閾値を選びます。

## 内積

```cpp
float Dot(Vector3 a, Vector3 b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}
```

正規化済みなら二方向の角度cosです。

- 前方判定：`Dot(forward, toTarget) > threshold`。
- Planeへの射影。
- 速度のNormal成分。
- LightingのLambert項。

角度だけを比較するなら`acos`を避け、cos閾値と比較します。

## 外積

```cpp
Vector3 Cross(Vector3 a, Vector3 b)
{
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}
```

両方へ垂直なVectorを返します。順序を逆にすると符号が反転します。座標系Conventionと組み合わせてRight/Upを作ります。

## 基底の作成

```cpp
Vector3 forward{NormalizeOrZero(target - position)};
Vector3 right{NormalizeOrZero(Cross(worldUp, forward))};
Vector3 up{Cross(forward, right)};
```

forwardがworldUpと平行に近いと外積がゼロになります。CameraのLookAtでは代替Up軸を選ぶ必要があります。

## 射影

Vector `v`を正規化方向`n`へ射影します。

```cpp
Vector3 Project(Vector3 v, Vector3 normalizedN)
{
    return normalizedN * Dot(v, normalizedN);
}
```

Plane上の成分は`v - Project(v, normal)`です。Slope上の移動方向に使えます。

## Plane

```cpp
struct Plane
{
    Vector3 normal{}; // 正規化する契約。
    float distance{}; // 式 Dot(normal, point) + distance = 0 のConvention。
};

float SignedDistance(const Plane& plane, Vector3 point)
{
    return Dot(plane.normal, point) + plane.distance;
}
```

Plane式の符号Conventionを統一します。Frustum Cullingでは各Planeの内側が正か負かを固定します。

## Ray

```cpp
struct Ray
{
    Vector3 origin{};
    Vector3 direction{}; // 正規化済みかを契約化。
};

Vector3 PointAt(const Ray& ray, float distance)
{
    return ray.origin + ray.direction * distance;
}
```

directionが正規化されていればParameterがWorld距離になります。

## 最近点

Segment AB上でPoint Pへ最も近い点を求めます。

```cpp
Vector3 ClosestPointOnSegment(Vector3 a, Vector3 b, Vector3 p)
{
    const Vector3 ab{b - a};
    const float denominator{Dot(ab, ab)};
    if (denominator <= 1.0e-12F)
    {
        return a;
    }

    const float t{std::clamp(Dot(p - a, ab) / denominator, 0.0F, 1.0F)};
    return a + ab * t;
}
```

Capsule衝突等の基礎です。

## Barycentric Coordinate

Triangle内の点を三頂点の重み`u+v+w=1`で表します。位置だけでなくUV、Normal、Vertex Colorの補間に使われます。RasterizerがPerspective-correct interpolationを行う理由へつながります。

## 反射

正規化Normal `n`に対するVector `v`の反射です。

```cpp
Vector3 Reflect(Vector3 v, Vector3 n)
{
    return v - n * (2.0F * Dot(v, n));
}
```

Normal方向と`v`の向きConventionを確認します。

## 数値安定性

平行判定を完全な0で行わず、Scaleに合うToleranceを使います。ただし各所で異なる適当epsilonを置かず、用途別に意味を定めます。巨大座標では差分を取ってCamera-relativeに計算します。
