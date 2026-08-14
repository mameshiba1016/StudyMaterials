# 第13章 3D数学・座標・行列

3D数学は公式を暗記する科目ではなく、位置・方向・回転・座標変換を曖昧なく表す言語です。本章では純粋な計算を理解してからDXライブラリの`VECTOR`・`MATRIX`へ接続します。

## 1. 3D Vector

```cpp
struct Vec3 final
{
    float x = 0.0F;
    float y = 0.0F;
    float z = 0.0F;

    [[nodiscard]] constexpr Vec3 operator+(Vec3 r) const noexcept
    { return {x + r.x, y + r.y, z + r.z}; }

    [[nodiscard]] constexpr Vec3 operator-(Vec3 r) const noexcept
    { return {x - r.x, y - r.y, z - r.z}; }

    [[nodiscard]] constexpr Vec3 operator*(float s) const noexcept
    { return {x * s, y * s, z * s}; }
};
```

同じ3成分でもPoint、Direction、Velocity、Scale、Colorは意味が違います。変数名と型で区別します。

## 2. 位置と方向

- 位置－位置＝方向。
- 位置＋方向＝位置。
- 方向＋方向＝方向。
- 位置＋位置は数学上計算できても、Game上の意味を確認する。

## 3. DXライブラリの`VECTOR`

```cpp
const VECTOR dx = VGet(1.0F, 2.0F, 3.0F);

[[nodiscard]] VECTOR ToDx(Vec3 v) noexcept { return VGet(v.x, v.y, v.z); }
[[nodiscard]] Vec3 FromDx(VECTOR v) noexcept { return {v.x, v.y, v.z}; }
```

境界で変換すればGame数学型を特定Libraryから独立させられます。

## 4. 長さ

```cpp
[[nodiscard]] constexpr float LengthSquared(Vec3 v) noexcept
{ return v.x * v.x + v.y * v.y + v.z * v.z; }

[[nodiscard]] float Length(Vec3 v) noexcept
{ return std::sqrt(LengthSquared(v)); }
```

距離比較だけならSquare Rootを避けて二乗値を比較します。

## 5. 正規化

```cpp
[[nodiscard]] Vec3 NormalizeOr(Vec3 v, Vec3 fallback) noexcept
{
    const float lengthSq = LengthSquared(v);
    if (lengthSq <= 1.0e-12F) return fallback;
    return v * (1.0F / std::sqrt(lengthSq));
}
```

Zero Vectorを正規化するとDivision by zeroになります。Fallbackの意味を用途ごとに決めます。

## 6. 内積

```cpp
[[nodiscard]] constexpr float Dot(Vec3 a, Vec3 b) noexcept
{ return a.x*b.x + a.y*b.y + a.z*b.z; }
```

正規化Vector同士なら、1は同方向、0は直交、-1は逆方向です。

## 7. 視野判定

```cpp
const Vec3 toTarget = NormalizeOr(target - position, forward);
const float threshold = std::cos(ToRadians(halfFovDegrees));
const bool inside = Dot(forward, toTarget) >= threshold;
```

Full FOVではなく半角を使います。距離条件も別に確認します。

## 8. 射影

Unit Vector`n`への射影です。

```cpp
[[nodiscard]] constexpr Vec3 Project(Vec3 v, Vec3 unitAxis) noexcept
{ return unitAxis * Dot(v, unitAxis); }

[[nodiscard]] constexpr Vec3 Reject(Vec3 v, Vec3 unitAxis) noexcept
{ return v - Project(v, unitAxis); }
```

地面Normalから速度の面方向成分を得るときに使えます。

## 9. 外積

```cpp
[[nodiscard]] constexpr Vec3 Cross(Vec3 a, Vec3 b) noexcept
{
    return {a.y*b.z - a.z*b.y,
            a.z*b.x - a.x*b.z,
            a.x*b.y - a.y*b.x};
}
```

両Vectorに垂直なVectorを返します。順序を逆にすると符号が反転します。

## 10. 左手系と右手系

座標系と外積規約を混ぜるとRight、Normal、回転方向が反転します。DXライブラリのCamera初期状態は+Z方向を見る説明になっています。使用APIとAsset変換の規約をProjectで固定します。

## 11. 基底Vector

```cpp
struct Basis final
{
    Vec3 right{1, 0, 0};
    Vec3 up{0, 1, 0};
    Vec3 forward{0, 0, 1};
};
```

外積から基底を作るときは座標系に合う順序をTestします。

## 12. Orthonormal Basis

各軸が長さ1で互いに直交する基底です。誤差で崩れたらGram-Schmidt法などで再直交化します。

```cpp
Basis MakeBasis(Vec3 forward, Vec3 worldUp)
{
    forward = NormalizeOr(forward, {0, 0, 1});
    Vec3 right = NormalizeOr(Cross(worldUp, forward), {1, 0, 0});
    Vec3 up = NormalizeOr(Cross(forward, right), {0, 1, 0});
    return {right, up, forward};
}
```

ForwardとWorld Upが平行に近い場合のFallbackが必要です。

## 13. Radian

```cpp
#include <numbers>

[[nodiscard]] constexpr float ToRadians(float degrees) noexcept
{ return degrees * std::numbers::pi_v<float> / 180.0F; }

[[nodiscard]] constexpr float ToDegrees(float radians) noexcept
{ return radians * 180.0F / std::numbers::pi_v<float>; }
```

角度変数名へ`Radians`を含め、度との混同を防ぎます。

## 14. `atan2`

```cpp
const float yaw = std::atan2(direction.x, direction.z);
```

二引数`atan2`はQuadrantを識別できます。引数順とどの軸を0度にするかは座標規約に依存します。

## 15. 最短角度差

```cpp
[[nodiscard]] float WrapPi(float angle) noexcept
{
    return std::remainder(angle, 2.0F * std::numbers::pi_v<float>);
}

[[nodiscard]] float DeltaAngle(float from, float to) noexcept
{ return WrapPi(to - from); }
```

359度から1度へ358度回らず、最短の2度を選べます。

## 16. 線形補間

```cpp
[[nodiscard]] constexpr Vec3 Lerp(Vec3 a, Vec3 b, float t) noexcept
{ return a + (b - a) * t; }
```

`t`を0～1へClampするか外挿を許すかを関数契約で分けます。

## 17. Frame-rate independent減衰

毎Frame固定割合で近づけるとFPS依存になります。

```cpp
[[nodiscard]] float ExpSmoothingFactor(float sharpness, float delta) noexcept
{ return 1.0F - std::exp(-sharpness * delta); }
```

Camera追従やAim方向の平滑化に使えます。

## 18. Affine Transform

3D Objectの基本変換はScale、Rotation、Translationです。これらを4×4行列でまとめます。

## 19. 4次元同次座標

位置を`(x,y,z,1)`、方向を`(x,y,z,0)`として扱います。平行移動成分は位置に作用し、方向には作用しません。

DXライブラリの`VTransform`は位置相当、`VTransformSR`は第4成分0でScale・Rotationのみという契約です。

## 20. 単位行列

何も変換しない行列です。

```cpp
const MATRIX identity = MGetIdent();
```

Transform初期値や積の単位元に使います。

## 21. Scale行列

```cpp
const MATRIX scale = MGetScale(VGet(2.0F, 1.0F, 0.5F));
```

非一様ScaleはNormal変換と回転合成を複雑にします。負Scaleは反転と面の向きを変えます。

## 22. Translation行列

```cpp
const MATRIX translation = MGetTranslate(VGet(10.0F, 0.0F, 20.0F));
```

方向Vectorへ平行移動を適用しないため`VTransformSR`を使い分けます。

## 23. 回転行列

```cpp
const MATRIX rotateY = MGetRotY(ToRadians(90.0F));
const MATRIX rotateAxis = MGetRotAxis(VGet(0, 1, 0), ToRadians(45.0F));
```

軸は正規化が必要かを公式契約で確認します。

## 24. 行Vector規約

DXライブラリ公式説明ではVectorを1×4行列として行列の左から乗算します。

```text
v' = v × M
```

列Vectorを使う教科書では`v'=M×v`となり、合成順が逆に見えます。式を混ぜません。

## 25. 合成順

行Vector規約でLocal PointへScale→Rotation→Translationを適用するなら、概念的に次の順です。

```cpp
const MATRIX sr = MMult(scale, rotation);
const MATRIX world = MMult(sr, translation);
const VECTOR worldPoint = VTransform(localPoint, world);
```

実際の`MMult`引数契約を公式資料と単体Testで確認します。

## 26. 順序は交換できない

`Scale×Translation`と`Translation×Scale`、`Rotate×Translate`と逆順は通常異なります。Objectを原点周りで回すか、World原点周りで回すかが変わります。

## 27. Transform Data

```cpp
struct Transform3D final
{
    Vec3 position{};
    Quaternion rotation{};
    Vec3 scale{1, 1, 1};
};
```

Matrixだけを正にすると編集が難しく、TRSだけを正にするとShearを表せません。Game ObjectはTRS、Renderer境界でMatrix生成する構成が一般的です。

## 28. LocalとWorld

ChildのLocal TransformはParent座標系で表します。

```text
childWorld = childLocal × parentWorld
```

これは行Vector規約の概念式です。階層更新はParentからChildへ行います。

## 29. Transform Hierarchy

親変更時に全子孫のWorld MatrixがDirtyになります。毎Frame全Nodeを再計算するか、Dirty flagで必要部分だけ更新するかを計測で選びます。

## 30. Cycle

Transform親子関係にCycleがあると無限再帰します。親設定時に自分自身または子孫を親にしない検証が必要です。

## 31. 逆行列

WorldからLocalへ戻すにはWorld Matrixの逆行列を使います。

```cpp
const MATRIX worldToLocal = MInverse(world);
```

Scaleが0など特異行列は逆行列を持ちません。失敗表現や行列式を確認します。

## 32. 転置行列

行と列を交換します。規約変換やNormal Matrixで登場します。

```cpp
const MATRIX transposed = MTranspose(matrix);
```

## 33. Normal変換

非一様Scaleを含むMatrixでNormalを単純変換すると面へ垂直でなくなります。World Matrixの逆転置の3×3部分を使い、最後に正規化します。

## 34. View行列

World座標をCamera座標へ変換します。CameraのWorld Transformそのものではなく、その逆変換です。

```text
local → World → View(Camera space)
```

次章でDXライブラリCamera APIと詳しく接続します。

## 35. Projection行列

Camera空間をClip空間へ変換します。Perspectiveでは遠い物ほど小さく、Orthographicでは距離でSizeが変わりません。

## 36. 変換Pipeline

```text
Local Position
× World Matrix
× View Matrix
× Projection Matrix
= Clip Position
→ Perspective Divide
= NDC
→ Viewport Transform
= Screen Position
```

## 37. Clip座標とw

Projection後は4成分`(x,y,z,w)`です。`x/w、y/w、z/w`でNDCへ進みます。Camera後方や`w≈0`では単純に画面座標化できません。

## 38. NDC

Normalized Device CoordinatesのZ範囲やY方向はGraphics API規約で異なります。OpenGLの知識をDirectXへ無条件に適用しません。

## 39. Perspective Divide

遠いPointほど`w`が大きくなり、除算後に中心へ近づくことで遠近感が生まれます。これは単なる「Zで割る」の一般化です。

## 40. Quaternion

```cpp
struct Quaternion final
{
    float x = 0.0F;
    float y = 0.0F;
    float z = 0.0F;
    float w = 1.0F;
};
```

回転を4成分で表します。位置や任意4D Vectorではありません。

## 41. Quaternionの利点

- Euler角よりGimbal Lockを避けやすい。
- 回転合成が可能。
- Slerpで滑らかに補間できる。
- Matrixより少ない成分。

ただし正規化、積順、座標系規約が必要です。

## 42. 単位Quaternion

`(0,0,0,1)`は回転なしです。長さ1のUnit Quaternionとして回転を表します。計算誤差で長さが崩れたら正規化します。

## 43. Axis-Angle

Unit軸`n`、角度`θ`から概念的に`(n×sin(θ/2), cos(θ/2))`を作ります。半角である点が重要です。

## 44. Quaternion積

積は回転合成ですが交換できません。`a*b`が「aの後b」か逆かはVector規約と実装で異なるため、既知の90度回転Testで固定します。

## 45. Vector回転

Quaternionで方向を回します。位置の平行移動は別です。毎VectorでQuaternion→Matrix変換するか直接式を使うかはAPIと計測で決めます。

## 46. NlerpとSlerp

- Nlerp: 線形補間後に正規化。安価で近似。
- Slerp: 球面線形補間。角速度が一定。

Dotが負なら片方の符号を反転し、同じ回転を表す短い経路を選びます。

## 47. Euler角

Yaw、Pitch、Rollは人間が編集しやすい一方、回転順で結果が変わりGimbal Lockがあります。UI入力はEuler、内部回転はQuaternionという分担が可能です。

## 48. Gimbal Lock

二つの回転軸が一致して自由度を失う現象です。Quaternionは表現上回避しやすいですが、Eulerへ変換して毎Frame編集すれば再び問題を持ち込みます。

## 49. Look Rotation

ForwardとUpからOrthonormal Basisを作り、MatrixまたはQuaternionへ変換します。ForwardとUpが平行な特異Caseを処理します。

## 50. Ground平面への方向投影

Target Lock移動ではCamera ForwardからUp成分を除きます。

```cpp
Vec3 FlatDirection(Vec3 direction, Vec3 unitUp)
{
    return NormalizeOr(Reject(direction, unitUp), {0, 0, 1});
}
```

## 51. Tangent方向

地面Normalと移動方向から接平面上の方向を作ります。Slope移動や壁沿いSlideに利用します。外積順で左右が反転するためGizmoで確認します。

## 52. Plane

```cpp
struct Plane final
{
    Vec3 unitNormal{0, 1, 0};
    float distance = 0.0F;
};

[[nodiscard]] constexpr float SignedDistance(Plane p, Vec3 point) noexcept
{ return Dot(p.unitNormal, point) + p.distance; }
```

NormalがUnitでない場合、戻り値は実距離になりません。

## 53. RayとPlane

分母`dot(normal,direction)`が0付近なら平行です。Hit距離が負ならRay始点の後方です。Toleranceと最大距離を確認します。

## 54. Barycentric座標

三角形上のPointを3頂点の重みで表します。重みの合計は1です。三角形内判定、UV・Normal補間に使います。

## 55. AABBとOBB

- AABB: World軸に平行。高速だが回転Objectへ余白。
- OBB: Object軸に沿う。密だが判定が複雑。

Broad PhaseはAABB、Narrow PhaseはOBB等に分けられます。

## 56. Sphere

中心と半径だけで回転の影響を受けず高速です。非一様Scaleでは半径をどう変換するかPolicyが必要です。

## 57. Floating Point誤差

```cpp
[[nodiscard]] bool NearlyEqual(
    float a, float b, float absTolerance, float relTolerance) noexcept
{
    const float difference = std::abs(a - b);
    const float scale = std::max(std::abs(a), std::abs(b));
    return difference <= std::max(absTolerance, relTolerance * scale);
}
```

絶対誤差と相対誤差を用途に応じて組み合わせます。

## 58. World Scale

1 Unitを1mなどに統一します。極端に巨大・微小な値を混ぜると精度、Physics、Camera Near/Farが不安定になります。

## 59. Large World

Cameraから遠い巨大座標ではfloat精度が低下します。Floating Origin、区画座標＋Local座標、double Simulationなどの方法があります。

## 60. NaNとInfinity

Zero除算や不正なSquare RootでNaNが生じ、比較がFalseになって広がります。

```cpp
[[nodiscard]] bool IsFinite(Vec3 v) noexcept
{
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}
```

System境界でAssert・Logします。

## 61. Gizmo

```cpp
void DrawBasis(Vec3 origin, const Basis& b, float length)
{
    DrawLine3D(ToDx(origin), ToDx(origin + b.right * length),
               GetColor(255, 64, 64));
    DrawLine3D(ToDx(origin), ToDx(origin + b.up * length),
               GetColor(64, 255, 64));
    DrawLine3D(ToDx(origin), ToDx(origin + b.forward * length),
               GetColor(64, 128, 255));
}
```

RGBをXYZへ対応させ、Local軸とWorld軸を表示します。

## 62. 行列表示

4×4各成分、Position、軸長、軸同士のDot、行列式をDebug UIへ表示するとScale混入や直交崩れを発見できます。

## 63. Test

```cpp
static_assert(Dot(Vec3{1,0,0}, Vec3{0,1,0}) == 0.0F);
static_assert(Cross(Vec3{1,0,0}, Vec3{0,1,0}).z == 1.0F);
static_assert(LengthSquared(Vec3{3,4,0}) == 25.0F);
```

DX行列は既知PointをScale→回転→平行移動し、期待座標と比較して積順を固定します。

## 64. よくある不具合

- Objectが公転する: TranslationとRotationの積順が逆。
- 方向にも位置Offset: `VTransform`と`VTransformSR`の誤用。
- 左右反転: 外積順または座標系不一致。
- 回転が遠回り: 角度WrapやQuaternion符号未処理。
- Normalがおかしい: 非一様Scaleへ逆転置を使っていない。
- 突然NaN: Zero Vector正規化や特異逆行列。
- Assetだけ逆向き: Import座標規約の変換不足。

## 65. 設計チェックリスト

- [ ] 位置と方向を意味で区別する。
- [ ] Zero Vector正規化を処理する。
- [ ] 内積・外積の用途を説明できる。
- [ ] 座標系とForward/Upを固定した。
- [ ] 度とRadianを名前で区別した。
- [ ] 行Vector/列Vector規約を混ぜない。
- [ ] TRS順を既知値Testで固定した。
- [ ] 位置と方向の行列変換を使い分ける。
- [ ] 非一様ScaleのNormalを逆転置する。
- [ ] Quaternionを正規化し最短補間する。
- [ ] IsFiniteとGizmoで異常を検出する。

## 66. 理解確認問題

1. 位置－位置の結果は何か。
2. 距離比較で二乗値を使う利点は何か。
3. 内積が負なら方向関係はどうなるか。
4. 外積の引数を逆にすると何が起こるか。
5. 方向Vectorへ平行移動を適用しない理由は何か。
6. 行列乗算順を暗記だけで決めてはいけない理由は何か。
7. View MatrixがCamera World Matrixの逆になる理由は何か。
8. Perspective Divideの役割は何か。
9. Quaternionの`q`と`-q`はどういう関係か。
10. 非一様Scale時にNormal Matrixが必要な理由は何か。

## 67. 実践課題

1. Vec3、Dot、Cross、NormalizeOrを作りTestする。
2. 視野判定とGround平面への方向投影を作る。
3. DX行列のTRS順を既知値で検証する。
4. Local/World階層とDirty更新を作る。
5. EulerとQuaternion補間を比較する。
6. RayとPlaneの交点を実装する。
7. Basis、Normal、TransformをGizmo表示する。
8. NaNを故意に発生させ境界で検出する。

## 68. 公式資料

- [DXライブラリ 3D算術演算関数](https://dxlib.xsrv.jp/function/dxfunc_3d_math.html)
- [DXライブラリ 3D関数一覧](https://dxlib.xsrv.jp/function/dxfunc_3d.html)
- [DXライブラリ 3D図形描画](https://dxlib.xsrv.jp/function/dxfunc_3d_draw.html)
- [DXライブラリ Camera関数](https://dxlib.xsrv.jp/function/dxfunc_3d_camera.html)

行列の配置、積順、座標系、角度単位、逆行列の失敗条件は利用中バージョンの公式資料と既知値Testを正としてください。

## 69. 次章への接続

次章では3D Camera・Projectionを扱い、View/Projection、FOV、Near/Far、Frustum、World↔Screen変換、Target Lock Camera、Camera Collisionへ進みます。
