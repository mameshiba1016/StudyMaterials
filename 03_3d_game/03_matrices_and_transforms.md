# 行列・Transform・階層

行列は線形変換と平行移動を統一して表し、複数変換を合成します。式の順序はVectorを行列の左・右どちらへ置くConventionで変わります。

## 4×4同次行列

3Dの平行移動を含めるため四次元同次座標を使います。

```text
Point     (x, y, z, 1)
Direction (x, y, z, 0)
```

本章の説明ではColumn Vectorへ左から行列を掛けるConventionを想定します。

```text
p_world = M_world * p_local
```

利用LibraryがRow Vectorなら積順が反対に見えます。

## TRS

Translation、Rotation、Scaleを合成します。

```text
M = T * R * S
```

Column Vector Conventionでは、PointへS、R、Tの順で作用します。積は交換できません。

```text
Rotate * Translate ≠ Translate * Rotate
```

## Transform構造

```cpp
struct Transform
{
    Vector3 position{};
    Quaternion rotation{Quaternion::Identity()};
    Vector3 scale{1.0F, 1.0F, 1.0F};
};
```

Gameplay・EditorではTRSが扱いやすく、RendererへMatrixをCacheしてDirty時だけ再計算できます。

## 階層合成

```text
childWorld = parentWorld * childLocal
```

Parentが非一様Scaleを持ちChildが回転するとShearが生じ、結果を純粋なTRSへ正確に分解できない場合があります。SkeletonやCamera階層では非一様Scaleを制限する設計があります。

## 逆行列

WorldからLocalへ戻すにはWorld MatrixのInverseを使います。

```text
p_local = inverse(M_world) * p_world
```

Scale成分が0なら行列は特異でInverseが存在しません。一般4×4Inverseは高コストなので、Rigid TransformならRotation転置と平行移動から高速に求められます。

## Rotation Matrix

正規直交Rotation Matrixでは、各基底軸は単位長で互いに直交し、InverseはTransposeです。Scaleや数値誤差が混入するとこの性質は崩れます。

## Normal Matrix

Positionと同じWorld MatrixをNormalへ掛けると、非一様Scaleで垂直性が壊れます。NormalはModel Matrixの3×3部分のInverse Transposeで変換します。

```text
n_world = transpose(inverse(M3x3)) * n_local
```

その後正規化します。Uniform Scaleだけなら簡略化できます。

## Memory LayoutとMathematical Convention

Row-major/Column-majorはMemory配置、Row/Column Vectorは数学上の掛け方です。関連はありますが同一概念ではありません。ShaderへのUpload時にTransposeが必要か、CompilerのMatrix packingも確認します。

## View Matrix

CameraのWorld Transformそのものではなく、そのInverseです。

```text
view = inverse(cameraWorld)
```

Cameraを右へ動かすとWorldが画面上左へ動くのはInverseのためです。

## Projection Matrix

Perspectiveでは`w`へDepthに関係する値を入れ、Perspective Divideで遠いものを小さくします。Affine Transformではないため、Projection後に単純なWorld距離を扱いません。

## Matrix分解

任意MatrixからTRSを取り出す処理は、負Scale、Shear、特異行列で曖昧です。ImportやEditor Gizmoでは分解失敗を扱います。Runtimeで毎フレームMatrix→TRS→Matrixを繰り返すと誤差が蓄積します。

## Dirty Propagation

Parent Transformが変われば全Descendant World MatrixがDirtyです。

```text
SetLocal → mark self and descendants dirty
GetWorld → if dirty, parent worldから再計算
```

深い階層の再帰、循環Parent、Entity破棄を検査します。更新時に階層順へ一括計算する方式もあります。

## Transform補間

Position/ScaleはLerp、RotationはQuaternion Slerp/Nlerpを使います。Matrix要素を直接Lerpすると基底が直交しなくなり、ScaleやShearが不自然になります。
