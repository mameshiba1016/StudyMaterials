# DirectX 11：Camera・Projection

この章では、WorldをCamera基準へ変換するView Matrixと、3D空間をClip Spaceへ写すProjection Matrixを学びます。LookAt、Perspective、Orthographic、Near/Far、Screen変換、Picking Ray、Frustum、追従Camera、Lock-on、Collision、Shake、Temporal Jitterまでを扱います。

## 1. Cameraは画面内Objectではない

CameraはSceneを見る座標系とProjection規則です。Cameraを動かす代わりに、World全体をCameraの逆変換でView Spaceへ移します。

## 2. Camera Transform

```text
position
orientation
projection settings
viewport
temporal history
```

Render用DataとGameplay制御Dataを分離します。

## 3. View Matrix

World Space PositionをCamera Spaceへ変換します。Camera World MatrixのInverseと考えられます。

## 4. Camera WorldとView

```cpp
XMMATRIX cameraWorld = rotation * translation;
XMMATRIX view = XMMatrixInverse(nullptr, cameraWorld);
```

ScaleをCamera Transformへ含めない設計が一般的です。

## 5. Basisから考える

CameraのRight、Up、Forwardを直交Unit Vectorとして持ち、Positionと合わせてView Matrixを作ります。

## 6. LookAt LH

```cpp
XMMATRIX view = XMMatrixLookAtLH(
    eyePosition,
    focusPosition,
    upDirection);
```

Left-handed座標系向けです。

## 7. LookAt RH

Right-handed Projectでは`XMMatrixLookAtRH`を使います。LH/RHをAssetやProjectionと混在させません。

## 8. EyeとFocus

EyeはCamera位置、Focusは見るWorld位置です。Forward方向そのものを渡す関数と取り違えません。

## 9. LookTo

```cpp
XMMATRIX view = XMMatrixLookToLH(
    eyePosition,
    forwardDirection,
    upDirection);
```

こちらはFocus位置ではなく方向を渡します。

## 10. LookAtの退化

EyeとFocusが同一点、またはForwardとUpが平行に近い場合、Basisを作れません。入力を検証しFallback方向を使います。

## 11. Up Vector

World Upを固定するCameraと、Rollを許してCamera自身のUpを使うCameraを分けます。

## 12. YawとPitch

Yawは水平回転、Pitchは上下回転として扱うのが一般的です。Axisと符号はProject座標系に合わせます。

## 13. Pitch Clamp

真上・真下を越えるとUp反転や操作逆転が起きやすいため、Third-person CameraではPitch範囲を制限します。

## 14. Roll

飛行、Dash、Damage演出等で使えます。Gameplay方向と画面演出Rollを分離すると操作が安定します。

## 15. Mouse入力

Mouse Deltaへ感度と時間規約を適用します。OS Mouse DeltaがすでにFrame間変位ならDelta Timeを二重に掛けないようにします。

## 16. Stick入力

Dead Zone、Response Curve、最大角速度、Delta Timeを使い、Frame Rate非依存の角度変化へします。

## 17. Quaternion Camera

OrientationをQuaternionで保存し、BasisやView Matrixを生成できます。EulerはUI/制約用に保持する方式もあります。

## 18. Projection Matrix

View SpaceをClip Spaceへ変換します。PerspectiveとOrthographicが代表です。

## 19. Perspective Projection

遠いObjectほど小さく見える投影です。3D Action GameのMain Cameraに使います。

## 20. Perspective FOV LH

```cpp
XMMATRIX projection = XMMatrixPerspectiveFovLH(
    verticalFovRadians,
    aspectRatio,
    nearZ,
    farZ);
```

## 21. Vertical FOV

`XMMatrixPerspectiveFov*`へ渡すFOVは垂直方向です。水平FOVとの変換ではAspect Ratioが必要です。

## 22. Aspect Ratio

```cpp
float aspect =
    static_cast<float>(width) /
    static_cast<float>(height);
```

Height 0を拒否し、Resize時に更新します。

## 23. FOVと画面比率

垂直FOV固定なら横長画面ほど水平範囲が広がります。Competitive/演出要件に応じてFOV Policyを決めます。

## 24. Near Plane

Nearより手前はClipされます。小さくしすぎると通常Depthの精度分布が悪化します。

## 25. Far Plane

描画可能な遠端です。必要以上に遠くするとDepth精度やCulling効率へ影響します。

## 26. Depth Precision

Perspective Depthは線形に分布しません。Near Plane付近へ多くの精度が割り当てられます。Nearを0へ近づけすぎません。

## 27. Reversed-Z

Depth Mapping、Clear、Comparisonを反転し、Float Depthの精度を有効利用します。Projection MatrixとDepth復元式も合わせます。

## 28. Infinite Far Plane

Farを無限遠として構成するProjectionがあります。Skyや広大Sceneに有効ですが、Frustum/Culling/Depth復元を対応させます。

## 29. Orthographic Projection

距離で大きさが変わらない投影です。UI、2D、Shadow Map、Editor View等で使います。

## 30. Orthographic LH

```cpp
XMMATRIX projection = XMMatrixOrthographicLH(
    viewWidth,
    viewHeight,
    nearZ,
    farZ);
```

## 31. Off-center Projection

左右上下境界を個別指定するProjectionは、Shadow Cascade、Stereo、Jitter、分割領域等に利用できます。

## 32. View-Projection

```cpp
XMMATRIX viewProjection = view * projection;
```

Row Vector規約でWorldからView、Clipへ進みます。

## 33. World-View-Projection

```cpp
XMMATRIX wvp = world * view * projection;
```

ObjectごとのWorldとCamera共通View/Projectionを組み合わせます。

## 34. Camera Constant Buffer

```cpp
struct alignas(16) CameraConstants
{
    XMFLOAT4X4 view;
    XMFLOAT4X4 projection;
    XMFLOAT4X4 viewProjection;
    XMFLOAT4X4 inverseViewProjection;
    XMFLOAT4 cameraWorldPosition;
};
```

Transpose規約と16 Byte Packingを統一します。

## 35. Inverse View Projection

Screen/NDCからWorldへ戻すRay生成やPosition復元に使います。Singular Matrixでないことを確認します。

## 36. World to Clip

```text
world position * viewProjection -> clip position
```

Clipのwを保持します。

## 37. Clip to NDC

```text
ndc = clip.xyz / clip.w
```

wが0付近、Camera後方、Clip範囲外を処理します。

## 38. NDC to Screen

ViewportのTopLeft、Width、Height、Min/Max Depthを使いPixel座標へ変換します。Y方向の符号規約へ注意します。

## 39. XMVector3Project

```cpp
XMVECTOR screen = XMVector3Project(
    worldPosition,
    viewportX, viewportY,
    viewportWidth, viewportHeight,
    minDepth, maxDepth,
    projection, view, world);
```

World-to-screen変換をまとめて行えます。

## 40. Screen上UI Marker

Enemy頭上Icon等はWorld位置をScreenへ変換します。Camera後方やFrustum外では非表示またはEdge Indicatorへ変換します。

## 41. Behind Camera判定

単純なScreen座標だけでなくClip wやView Space Forward Depthを確認します。後方Pointが画面内に折り返されるのを防ぎます。

## 42. XMVector3Unproject

Screen座標とDepthからWorld位置へ戻します。Viewport、Projection、View、Worldの組を描画時と一致させます。

## 43. Picking Ray

Mouse PixelのNear点とFar点をUnprojectし、NearをOrigin、正規化した差をDirectionにします。

## 44. Ray構築例

```text
nearWorld = unproject(mouseX, mouseY, minDepth)
farWorld  = unproject(mouseX, mouseY, maxDepth)
rayOrigin = nearWorld or camera position
rayDir    = normalize(farWorld - nearWorld)
```

## 45. PickingとDPI

Mouseの論理座標をBack Buffer Pixelへ変換し、実際のViewportへ合わせます。Window BorderやRender Scaleも考慮します。

## 46. Frustum

Camera Projectionが見えるVolumeです。Perspectiveでは切頭四角錐、Orthographicでは直方体です。

## 47. BoundingFrustum

DirectXCollisionの`BoundingFrustum`をProjectionから作り、Camera Worldへ変換してBoundsとの交差を調べられます。

## 48. Frustum Culling

Object BoundsがFrustum外ならDraw Commandを省略します。Mesh三角形単位でなくBounding Sphere/Box等を使います。

## 49. Conservative Bounds

Animation、Particle、Weapon、Effectを含むBoundsが小さすぎると画面端で突然消えます。必要範囲を安全側に含めます。

## 50. Camera-relative Rendering

Camera位置を原点付近へ移した相対座標でGPU描画すると、大きなWorld座標のFloat精度問題を減らせます。

## 51. Third-person Camera

```text
target anchor
orbit yaw/pitch
desired distance
shoulder offset
collision correction
smoothing
final camera pose
```

処理順を明示します。

## 52. Target Anchor

Character原点ではなく胸・頭付近の専用Anchorを使うと、足場やAnimationによるCamera揺れを減らせます。

## 53. Desired Position

Target Basis、Yaw/Pitch、Distance、Shoulder Offsetから衝突前の理想Camera位置を計算します。

## 54. Camera Collision

TargetからDesired PositionへSphere Cast等を行い、壁より手前へCameraを移します。RayだけよりCamera Near Planeの食込みを抑えられます。

## 55. Collision Margin

Hit Surfaceぴったりに置かず小さなMarginを保ち、浮動誤差と壁ちらつきを防ぎます。

## 56. 復帰速度

障害物で近づくときは速く、離れた後に戻るときは少し遅くするなど、別速度にするとCamera酔いを抑えられます。

## 57. Spring Arm

Targetから一定距離を保とうとする制御です。CollisionでArm長を縮め、障害がなくなれば滑らかに戻します。

## 58. Frame-independent Smoothing

```text
alpha = 1 - exp(-response * deltaTime)
current = lerp(current, target, alpha)
```

固定`0.1` LerpをFrameごとに使うよりFrame Rate差を減らせます。

## 59. PositionとRotationの応答

別のResponse値を持たせます。位置は滑らか、Aim方向は速いなどActionの操作感へ合わせます。

## 60. Lock-on Camera

PlayerとTargetの両方が画面内に入るFocus、Distance、FOVを求めます。Target喪失、遮蔽、距離上限、切替Hysteresisを設計します。

## 61. Lock-on構図

単純にTargetだけを見るとPlayerが画面外へ出ます。Player/Target中点と相対距離からFocusとDistanceを調整します。

## 62. Shoulder Swap

左右Offsetを切り替えます。壁際で自動切替する場合、頻繁な往復を防ぐHysteresisを使います。

## 63. Camera Shake

最終Camera Poseへ位置・回転Noise/Impulseを加えます。Gameplay Camera方向とVisual Shakeを分離します。

## 64. Shakeの合成

複数Impulseを時間、周波数、Amplitude、Falloffで合成します。単純Randomを毎Frame使うと不連続な震えになります。

## 65. AimとShake

照準RayをShake後Cameraから出すか、安定したGameplay Cameraから出すかで命中感が変わります。仕様として決めます。

## 66. FOV Kick

Dash、Sprint、Ultimate等でFOVを一時変化させます。視覚速度感と酔い、Aim精度への影響を調整します。

## 67. Camera Cut

瞬間移動、Scene切替、Camera Mode変更ではTemporal Historyを無効化します。前Frame Matrixをそのまま使うとMotion Vectorが巨大になります。

## 68. Previous Matrices

Motion Vector/TAA用に前FrameのView Projectionを保持します。更新順は「描画後にCurrentをPreviousへ」が基本ですが、EngineのFrame境界で統一します。

## 69. Temporal Jitter

ProjectionへSubpixel Offsetを加え、FrameごとにSample位置を変えます。TAAで複数Frameから情報を集めます。

## 70. Jitter単位

Halton等のOffsetをPixel単位からNDC/Projection単位へ変換します。Render Resolutionが変わればScaleも更新します。

## 71. JitteredとUnjittered Matrix

描画/TAA用Jittered Projectionと、Culling、UI Marker、Stable Ray等のUnjittered Projectionを分けて保持します。

## 72. Motion Vector

Current/Previous Clip Positionを比較します。Object Motion、Camera Motion、Jitter差、Camera Cutを正しく扱います。

## 73. Multiple Cameras

Main、Shadow、Reflection、Minimap、UI、Cinematic Cameraを別Viewとして管理します。Global Singleton Camera一つに依存しません。

## 74. Camera Stack

Base Scene、Weapon、UI、Overlay等を異なるProjection/Depth規則で合成する設計があります。ClearとTarget共有を明示します。

## 75. Split Screen

CameraごとにViewport、Scissor、Aspect、Constant Buffer、Frustumを持ちます。Window全体Aspectを流用しません。

## 76. Debug Camera

Free-fly Cameraを用意し、Gameplay Camera Frustum、Collision、Target Anchor、Desired Positionを外側から可視化します。

## 77. Camera Data型

```cpp
struct CameraView
{
    XMFLOAT3 position{};
    XMFLOAT4 orientation{0, 0, 0, 1};
    float verticalFovRadians = XM_PIDIV4;
    float nearZ = 0.1f;
    float farZ = 1000.0f;
    D3D11_VIEWPORT viewport{};
};
```

有効範囲を設定時に検証します。

## 78. Dirty管理

Pose、Projection Parameter、Viewportが変わったときだけMatrixとFrustumを再計算します。Previous Matrix更新とは別Flagで管理します。

## 79. よくある失敗：Camera WorldをViewとして使う

WorldをCameraと同じ方向へ動かし、見え方が逆になります。ViewはCamera WorldのInverseです。

## 80. よくある失敗：LookAt EyeとFocusが同じ

ForwardをNormalizeできずNaNになります。最小距離を検証します。

## 81. よくある失敗：Resize後Aspect未更新

映像が横長/縦長に歪みます。ViewportとProjectionを同じResize Transactionで更新します。

## 82. よくある失敗：Nearを極端に小さくする

武器のClipを避けるためNearを縮め、Scene全体のDepth精度を悪化させます。Weapon別PassやCamera Collisionも検討します。

## 83. よくある失敗：ShakeをGameplay方向へ混ぜる

入力、Aim、移動方向まで揺れ、操作しにくくなります。Logical CameraとRender Cameraを分離します。

## 84. Matrix Test

- Cameraを原点Identityで置く。
- Position移動と逆向きWorld移動を確認する。
- LH/RHを混在させない。
- View×InverseViewがIdentity付近になる。
- CPU/GPU View Projection結果を比較する。

## 85. Projection Test

- FOVを狭い/広い値で比較する。
- Aspect変更で円が歪まない。
- Near/Far境界のClipを確認する。
- Perspective/Orthographicを比較する。
- 通常Z/Reversed-Zを別々に検証する。

## 86. Screen変換Test

- Viewport中心RayがCamera Forwardと一致する。
- World-to-screenとUnprojectを往復する。
- Camera後方Pointを拒否する。
- DPI/Render Scaleを変える。
- Split Screen各ViewportでRayを作る。

## 87. Action Camera Test

- 壁際でCameraが埋まらない。
- 障害物解除後に滑らかに戻る。
- Lock-on両者が画面内に入る。
- Camera CutでTemporal HistoryをResetする。
- 30/60/120 FPSで応答時間が近い。

## 88. 完成確認表

- [ ] Camera WorldとView Matrixの関係を説明できる。
- [ ] LookAt/LookToの引数を区別できる。
- [ ] Perspective/Orthographicを使い分けられる。
- [ ] FOV、Aspect、Near、Farを説明できる。
- [ ] World-to-screenとPicking Rayを構築できる。
- [ ] Frustum Culling用Boundsを扱える。
- [ ] Third-person Camera Collisionを設計できる。
- [ ] Lock-on、Shake、FOV Kickを論理Cameraから分離できる。
- [ ] Jittered/Unjittered/Previous Matrixを管理できる。
- [ ] Resize、Camera Cut、複数Cameraを安全に扱える。

## 89. この章の要点

- View MatrixはCamera World TransformのInverseです。
- Perspectiveは遠近感、Orthographicは距離非依存の大きさを作ります。
- Nearを小さく、Farを大きくしすぎるとDepth精度が悪化します。
- Aspect、Viewport、ProjectionはResize時に一体で更新します。
- Inverse View ProjectionからScreen Picking Rayを作れます。
- Third-person Cameraは理想位置、Collision、Smoothingの順に解決します。
- Gameplay CameraとShake等のVisual Cameraを分離すると操作が安定します。
- Temporal描画ではJittered、Unjittered、Previous MatrixとCamera Cutを管理します。

## 90. 公式資料

- [XMMatrixLookAtLH](https://learn.microsoft.com/en-us/windows/win32/api/directxmath/nf-directxmath-xmmatrixlookatlh)
- [XMMatrixLookToLH](https://learn.microsoft.com/en-us/windows/win32/api/directxmath/nf-directxmath-xmmatrixlooktolh)
- [XMMatrixPerspectiveFovLH](https://learn.microsoft.com/en-us/windows/win32/api/directxmath/nf-directxmath-xmmatrixperspectivefovlh)
- [XMMatrixOrthographicLH](https://learn.microsoft.com/en-us/windows/win32/api/directxmath/nf-directxmath-xmmatrixorthographiclh)
- [XMVector3Project](https://learn.microsoft.com/en-us/windows/win32/api/directxmath/nf-directxmath-xmvector3project)
- [XMVector3Unproject](https://learn.microsoft.com/en-us/windows/win32/api/directxmath/nf-directxmath-xmvector3unproject)
- [BoundingFrustum class](https://learn.microsoft.com/en-us/windows/win32/api/directxcollision/ns-directxcollision-boundingfrustum)
- [D3D11_VIEWPORT](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/ns-d3d11-d3d11_viewport)

次章では、Light、Normal、Material Parameterを使い、Lambert、Specular、PBRへ進むためのLighting基礎を扱います。
