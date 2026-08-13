# 2D座標・ベクトル・変換

2Dゲームでは位置、方向、速度、加速度、サイズを二成分で表します。すべて`Vector2`でも意味は異なるため、単位と座標空間を意識します。

## 座標系

画面APIでは左上原点、右が+X、下が+Yが一般的です。数学では上が+Yの場合が多く、角度・回転式の向きが変わります。

```text
(0,0) ─────→ +X
  │
  │
  ↓ +Y
```

プロジェクト内で世界座標の向きを決め、描画API境界で変換します。

## ベクトル型

```cpp
struct Vector2
{
    float x{};
    float y{};
};

Vector2 operator+(Vector2 left, Vector2 right)
{
    return {left.x + right.x, left.y + right.y};
}

Vector2 operator*(Vector2 value, float scalar)
{
    return {value.x * scalar, value.y * scalar};
}
```

位置`position`へ変位`velocity * deltaSeconds`を加えます。

```cpp
position = position + velocityPixelsPerSecond * deltaSeconds;
```

## 長さ

```cpp
float LengthSquared(Vector2 value)
{
    return value.x * value.x + value.y * value.y;
}

float Length(Vector2 value)
{
    return std::sqrt(LengthSquared(value));
}
```

距離比較だけなら平方根を避け、二乗距離同士を比較できます。

```cpp
if (LengthSquared(target - position) <= attackRange * attackRange)
{
    Attack();
}
```

## 正規化

方向だけを取り出し長さ1へします。

```cpp
Vector2 NormalizeOrZero(Vector2 value)
{
    const float lengthSquared{LengthSquared(value)};
    constexpr float epsilon{0.000001F};

    if (lengthSquared <= epsilon)
    {
        return {};
    }

    const float inverseLength{1.0F / std::sqrt(lengthSquared)};
    return value * inverseLength;
}
```

ゼロベクトルを除算するとNaN等になるため処理します。epsilonは用途の単位と精度から決めます。

## 内積

```cpp
float Dot(Vector2 a, Vector2 b)
{
    return a.x * b.x + a.y * b.y;
}
```

正規化済みなら、同方向で1、直交で0、反対方向で-1です。

- 敵がプレイヤーの前方にいるか。
- 速度の法線方向成分。
- ベクトル投影。
- 視野角判定。

角度比較だけなら`acos`で角度へ戻さず、閾値のcosと内積を比較できます。

## 2Dの外積相当

```cpp
float Cross(Vector2 a, Vector2 b)
{
    return a.x * b.y - a.y * b.x;
}
```

符号から左右関係、絶対値から平行四辺形面積を得られます。座標系のY方向によって画面上の回転解釈が反転する点に注意します。

## 回転

数学的なY上向き座標系の例です。

```cpp
Vector2 Rotate(Vector2 value, float radians)
{
    const float cosine{std::cos(radians)};
    const float sine{std::sin(radians)};

    return {
        value.x * cosine - value.y * sine,
        value.x * sine + value.y * cosine
    };
}
```

度とラジアンを混同しません。専用関数・型で境界を明示します。

## 線形補間

```cpp
Vector2 Lerp(Vector2 from, Vector2 to, float t)
{
    return from + (to - from) * t;
}
```

`t=0`でfrom、`t=1`でtoです。`t`を範囲外へ許せば外挿になります。カメラ追従へ毎フレーム固定`0.1`を使うとFPS依存になるため、時間ベースの指数減衰を使います。

```cpp
float factor{1.0F - std::exp(-sharpness * deltaSeconds)};
position = Lerp(position, target, factor);
```

## 座標空間

- Local：親やオブジェクト基準。
- World：ステージ全体基準。
- View/Camera：カメラ基準。
- Screen：ウィンドウ・内部解像度基準。
- Texture/UV：画像内の正規化座標等。

同じ`Vector2`でも空間が違えば直接加算できません。変数名、ラッパー型、変換関数で区別します。

## 親子変換

子のローカル位置は、親の平行移動・回転・拡縮を受けてワールド位置になります。2Dでも3×3同次座標行列を使うと一貫して合成できます。スプライト階層、武器ソケット、UIレイアウトで必要です。

## ピクセルとサブピクセル

位置を整数だけで持つと低速移動を表せません。シミュレーションはfloat、描画時にピクセルへ変換します。丸め方法がフレームごとに揺れを作る場合があるため、ピクセルアートではカメラとスプライトを整数グリッドへスナップする設計もあります。
