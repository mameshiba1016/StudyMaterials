# 3Dカメラ・View・Projection・Depth

CameraはPosition、Orientation、Projection、Viewportを持ち、WorldをClip Spaceへ変換します。

## View Matrix

```text
view = inverse(cameraWorldTransform)
```

Rigid Cameraなら、Rotation MatrixのTransposeと変換済み負Translationから求められます。非一様ScaleをCameraへ許さない方が安全です。

## Perspective Projection

主なParameter：

- Vertical FOV。
- Aspect Ratio = width / height。
- Near Plane。
- Far PlaneまたはInfinite Far。

FOVをDegree/Radianで混同しません。Horizontal FOVはAspectで変わるため、どちらを固定するかを決めます。

## Perspective Divide

Vertex Shader出力Clip Position `(x,y,z,w)`をRasterizerが`w`で割りNDCへします。Perspectiveにより遠い物体が小さくなります。`w<=0`付近やCamera背面はClip規則に注意します。

## Orthographic Projection

距離で大きさが変わりません。3D Editor、UI、Shadow Map、Isometric表現に使います。幅・高さ・Near/Farを指定します。

## Depth Buffer

各PixelのDepthを保存し、より手前のFragmentだけ通します。Perspective ProjectionではDepth精度がNear付近へ偏ります。

- Nearを0にできない。
- Nearを極端に小さくすると遠方精度が悪化。
- Far/Near比を抑える。
- 可能ならReversed-ZとFloating Depthを使う。

同一面に近いPolygonは精度不足でZ-fightingします。

## Reversed-Z

Nearを大きいDepth、Farを小さいDepthへし、Depth比較を反転します。Floating Point分布を活用し遠距離精度を改善します。Projection Matrix、Clear値、Depth Compareを一式変更します。

## Frustum

View Volumeは左、右、上、下、Near、FarのPlaneで表せます。Object BoundsがPlaneの外なら描画候補から除外します。

```cpp
bool IsOutside(const Plane& plane, const Sphere& sphere)
{
    return SignedDistance(plane, sphere.center) < -sphere.radius;
}
```

Plane内側の符号Conventionを統一します。

## World to Screen

```text
clip = projection * view * worldPosition
ndc  = clip.xyz / clip.w
screen.x = viewport.x + (ndc.x * 0.5 + 0.5) * viewport.width
```

Screen Y変換とNDC ZはAPI規約に合わせます。`clip.w`が背面を示す場合、画面端Markerへ使う処理を分けます。

## Screen to Ray

Mouse PickingではScreen→NDCへ戻し、Near/Far点をInverse ViewProjectionでWorldへUnprojectします。

```text
world = inverse(viewProjection) * clip
world /= world.w
rayDirection = normalize(farWorld - nearWorld)
```

Camera PositionをOriginとするかNear Plane点を使うか、OrthographicかPerspectiveかで変わります。

## Jitter

TAAではProjectionへSubpixel Jitterを加えます。Gameplay RaycastやUI投影にJittered Matrixを使うと揺れるため、Jittered/Unjittered Matrixを分けます。

## Camera-relative Rendering

大規模WorldではObject PositionからCamera PositionをCPUまたはShaderで引き、原点付近の値として描画します。Gameplay Double座標とGPU Float座標の境界を設計します。

## ExposureとCamera

物理Camera表現ではAperture、Shutter Speed、ISOからExposureを決めます。ゲームプレイCameraではFOV、Motion Blur、揺れが酔いへ影響するため設定で調整可能にします。
