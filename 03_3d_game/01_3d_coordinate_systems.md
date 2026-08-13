# 3D座標系・空間・Handedness

3Dでは同じ三つの数値でも、どの空間の位置・方向かで意味が異なります。座標系の契約を曖昧にすると、Modelが反転し、Cameraが逆を向き、NormalやCullが壊れます。

## 三軸

```cpp
struct Vector3
{
    float x{};
    float y{};
    float z{};
};
```

どの軸を上・前とするかはEngine次第です。

例：

- Y-up / Z-forward。
- Z-up / X-forward。
- Y-up / -Z-forward。

「3DではZが奥」と一般化せず、ProjectのConventionを文書化します。

## 右手系と左手系

Handednessは三軸の向き関係です。右手系では右手のXからYへ指を曲げた時の親指方向がZという説明が使われますが、軸名とForward定義を同時に確認します。

外積の向き、三角形のWinding、View Matrix、Projectionへ影響します。Asset ToolとEngineで異なる場合、Import境界で一度だけ変換します。

## 主な座標空間

```text
Model / Local Space
    ↓ Model Matrix
World Space
    ↓ View Matrix
View / Camera Space
    ↓ Projection Matrix
Clip Space
    ↓ Perspective Divide
NDC
    ↓ Viewport Transform
Screen / Window Space
```

### Model Space

Mesh作成時の原点・軸です。Characterの足元、WeaponのGripなど、Asset Conventionを決めます。

### World Space

Scene全体の共通空間です。Gameplay位置、Light、Collisionを比較します。

### View Space

Cameraを原点とする空間です。WorldをCameraの逆Transformで動かした結果と考えられます。

### Clip Space

Projection後の四次元同次座標です。まだ`w`で除算していません。Frustum境界に対してClipされます。

### NDC

`x/w, y/w, z/w`のPerspective Divide後です。Z範囲がOpenGL系とDirect3D/Vulkan系で異なる場合があり、Y方向もAPI・Viewport設定で違います。

## 点と方向

同次座標では、点を`w=1`、方向を`w=0`として表せます。平行移動は点に影響し、方向には影響しません。

```text
Point     (x, y, z, 1)
Direction (x, y, z, 0)
```

位置同士を加算する数学的意味は薄く、`Point - Point = Direction`、`Point + Direction = Point`として考えます。強い型で区別する設計もあります。

## Local・Parent・World

Scene GraphではChildのLocal TransformをParent World Transformと合成します。

```text
Character World
  └─ Hand Local
      └─ Weapon Local
```

Parent変更時にWorld位置を維持するかLocal値を維持するかをAPIで指定します。

## 単位

1 Unitを1 m、1 cm等へ決めます。Physics、Gravity、Animation、Camera Near/Far、Sound attenuationが同じScaleへ依存します。Model Import時にScale 100を常用するとTransform・Physics精度・Tool連携が複雑になります。

## 角度

内部計算はRadian、Editor表示はDegreeという分離が一般的です。

```cpp
constexpr float DegreesToRadians(float degrees)
{
    return degrees * std::numbers::pi_v<float> / 180.0F;
}
```

変数名へ単位を付け、二重変換を防ぎます。

## 大規模World

原点から遠い`float`座標は細部精度を失います。World Origin Rebasing、Cell+Local座標、Double精度、Camera-relative Renderingを使います。Physics EngineとGPU Shaderが異なる精度を使う場合、変換境界が必要です。

## Convention文書

最低限をProject READMEへ固定します。

- Handedness。
- Up/Forward/Right軸。
- World Unit。
- Rotation正方向。
- MatrixのVector乗算側。
- Matrix Memory Layout。
- NDC Depth範囲。
- Triangle Winding。
- UV原点とTexture V方向。
