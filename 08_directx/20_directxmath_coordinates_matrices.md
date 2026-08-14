# DirectX 11：DirectXMath・座標・行列

この章では、DirectXMathを使ってVector、Matrix、Quaternion、座標変換を正しく扱う方法を学びます。保存型と計算型、SIMD Alignment、PointとDirection、Dot/Cross、変換順、Handedness、CPU-HLSL転送、Normal Matrix、階層Transform、Quaternion補間、数値誤差までを扱います。

## 1. DirectXMathとは

Windows/DirectX向けのSIMD対応Math Libraryです。Vector、Matrix、Quaternion、Color、Collision等の関数と型を提供します。

## 2. Namespace

```cpp
#include <DirectXMath.h>

using namespace DirectX;
```

大規模Codeでは広域`using namespace`を避け、`DirectX::`または限定Aliasを使う方針も有効です。

## 3. 保存型と計算型

```text
XMFLOAT2/3/4, XMFLOAT4X4 : Memory保存、構造体Field、File I/O
XMVECTOR, XMMATRIX       : SIMD計算
```

用途を分けるのがDirectXMathの基本です。

## 4. XMFLOAT3

```cpp
DirectX::XMFLOAT3 position{1.0f, 2.0f, 3.0f};
```

12 Byteの保存型として頂点やComponent Dataに使います。

## 5. XMVECTOR

4成分SIMD計算型です。Fieldを直接編集するより、Load、Set、Vector関数、Storeを使います。

## 6. XMLoadFloat3

```cpp
XMVECTOR p = XMLoadFloat3(&position);
```

保存型から計算型へ読み込みます。`XMLoadFloat3`が読み込んだVectorのwは0です。Positionとして明示的なw=1が必要なら`XMVectorSetW`等を使い、`XMVector3TransformCoord`のようにPointとして扱う専用関数とは責任を区別します。

## 7. XMStoreFloat3

```cpp
XMFLOAT3 result{};
XMStoreFloat3(&result, vectorValue);
```

計算結果を長期保存可能な型へ戻します。

## 8. XMVectorSet

```cpp
XMVECTOR value = XMVectorSet(x, y, z, w);
```

成分を明示してVectorを作ります。

## 9. XMVectorZeroとReplicate

```cpp
XMVECTOR zero = XMVectorZero();
XMVECTOR allTwo = XMVectorReplicate(2.0f);
```

初期化意図を明確にします。

## 10. Alignment

`XMVECTOR`と`XMMATRIX`はSIMD Alignment要件を持ちます。Stack Localや規約どおりの型を使い、Raw Byte配置やPacked構造体へ無理に埋め込みません。

## 11. Class Fieldの選択

長期保存Fieldには`XMFLOAT3`や`XMFLOAT4X4`を使い、計算時にLoadする方式が扱いやすくなります。Alignmentを保証できる場合はAligned型も検討できます。

## 12. STL Container

`XMVECTOR`を一般Containerへ保存する場合はAllocator/AlignmentとCopy規約を確認します。単純なData配列には`XMFLOAT`型を使います。

## 13. Calling Convention型

DirectXMathには`FXMVECTOR`、`GXMVECTOR`、`HXMVECTOR`、`CXMVECTOR`など引数位置に応じたAliasがあります。Platformごとの効率的な受渡しを表します。

## 14. 戻り値

計算関数は`XMVECTOR`や`XMMATRIX`を値で返せます。SIMD Registerで効率よく返す設計を利用します。

## 15. PositionとDirection

- Position：空間内の場所。平行移動の影響を受ける。
- Direction：向きや差分。平行移動の影響を受けない。

同じxyzでも意味が違います。

## 16. Homogeneous w

```text
position  -> w = 1
direction -> w = 0
```

4×4行列で平行移動をPositionだけへ適用できます。

## 17. CoordとNormal変換

```cpp
XMVECTOR transformedPoint = XMVector3TransformCoord(point, matrix);
XMVECTOR transformedVector = XMVector3TransformNormal(direction, matrix);
```

Coordは平行移動・w除算、Normalは平行移動なしとして用途が異なります。

## 18. Vector加算

```text
position + direction = position
direction + direction = direction
position - position = direction
```

型は同じでも意味上の演算規則を守ります。

## 19. 長さ

```cpp
XMVECTOR length = XMVector3Length(direction);
float scalarLength = XMVectorGetX(length);
```

結果が全成分へ複製される関数もあるためScalar抽出を明示します。

## 20. 長さの二乗

距離比較だけならSquare Rootを避け、`XMVector3LengthSq`と距離閾値の二乗を比較できます。

## 21. Normalize

```cpp
XMVECTOR unit = XMVector3Normalize(direction);
```

長さ1へします。Zero Vectorや極小VectorをそのままNormalizeしません。

## 22. Safe Normalize

Length SquaredをEpsilonと比較し、十分な長さがある場合だけNormalizeします。失敗時Fallback方向を決めます。

## 23. Dot Product

```cpp
float dot = XMVectorGetX(XMVector3Dot(a, b));
```

方向の類似、角度、Projection、前後判定に使います。

## 24. Dotの意味

Unit Vector同士なら結果は概ね`cos(theta)`です。

```text
 1 : same direction
 0 : perpendicular
-1 : opposite direction
```

## 25. 視野判定

Character ForwardとTarget DirectionのDotをFOV半角のCosineと比較します。毎回`acos`で角度へ戻す必要はありません。

## 26. Projection

Vector aをUnit Vector bへ射影する成分は`b * dot(a, b)`です。bがUnitでない場合は長さ二乗で割ります。

## 27. Cross Product

```cpp
XMVECTOR perpendicular = XMVector3Cross(a, b);
```

aとbの両方に垂直なVectorを作ります。引数順を逆にすると符号が反転します。

## 28. CrossとHandedness

Right/Up/Forward Basisを作るCross順序は座標系規約に依存します。式を暗記せずProject BasisでTestします。

## 29. Orthonormal Basis

互いに直交し長さ1のRight、Up、Forwardを作ります。Camera、Tangent Space、IK等で必要です。

## 30. Lerp

```cpp
XMVECTOR value = XMVectorLerp(a, b, t);
```

位置やScaleの線形補間です。tを0から1へClampするかExtrapolationを許すか決めます。

## 31. Smooth補間

補間係数へSmoothStep/Easingを適用できます。Variable Delta Time下でFrame依存の固定係数Lerpを繰り返さないようにします。

## 32. Matrixとは

座標系変換を表す数値配列です。Scale、Rotation、Translation、Projection等を合成できます。

## 33. XMMATRIX

4個のRow Vectorを持つ計算型です。保存には`XMFLOAT4X4`を使います。

## 34. Identity Matrix

```cpp
XMMATRIX identity = XMMatrixIdentity();
```

変換しない行列で、Transform初期値や合成開始値に使います。

## 35. Translation Matrix

```cpp
XMMATRIX translation = XMMatrixTranslation(x, y, z);
```

Positionを移動し、w=0のDirectionには影響しません。

## 36. Scaling Matrix

```cpp
XMMATRIX scale = XMMatrixScaling(sx, sy, sz);
```

非一様ScaleはNormal変換へ影響します。

## 37. Rotation Matrix

```cpp
XMMATRIX rotationY = XMMatrixRotationY(radians);
```

角度はRadianです。

## 38. DegreeとRadian

```cpp
float radians = XMConvertToRadians(degrees);
float degrees = XMConvertToDegrees(radians);
```

API境界で単位名を変数へ含めます。

## 39. Euler Rotation

Yaw/Pitch/Rollは理解しやすい一方、適用順、Axis規約、Gimbal Lockの問題があります。

## 40. Matrix乗算順

DirectXMathのRow Vector規約では、概念上`v * M`でVectorを変換します。`S * R * T`ならScale、Rotation、Translationの順に適用されます。

## 41. World Matrix例

```cpp
XMMATRIX world = scale * rotation * translation;
```

乗算は可換でないため、順序を入れ替えると結果が変わります。

## 42. Orbitと自転

TranslationとRotationの順序を変えると、Object中心で自転するか原点周りを公転するかが変わります。

## 43. Local Space

MeshがModel Authoring時に持つ座標系です。Objectの中心、Forward軸、単位Scale規約を定めます。

## 44. World Space

Scene全体で共有する座標系です。Local PositionへWorld Matrixを適用して得ます。

## 45. View Space

Cameraを原点として見た座標系です。WorldをCamera基準へ変換するView Matrixを使います。

## 46. Clip Space

Projection適用後、Perspective Divide前のHomogeneous空間です。Vertex Shaderが`SV_Position`へ出力します。

## 47. NDCとScreen Space

Perspective Divide後のNormalized Device CoordinatesがViewport変換でPixel座標へ移されます。Direct3DのDepth NDC範囲は0から1です。

## 48. World-View-Projection

```cpp
XMMATRIX wvp = world * view * projection;
```

Row Vector規約でLocalからClipへ順に変換します。

## 49. HLSL mul

```hlsl
float4 clipPosition =
    mul(float4(localPosition, 1.0f), worldViewProjection);
```

CPU側のRow Vector規約と式の向きを一致させます。

## 50. Memory Layoutと数学規約は別

Row-major/column-majorという保存配置と、Row Vector/Column Vectorという数学的乗算方向を混同しません。

## 51. HLSLへの転送規約A

CPUでDirectXMath行列をTransposeして、HLSL既定のColumn-major StorageへCopyし、`mul(vector, matrix)`で使う一般的な方式です。

```cpp
XMStoreFloat4x4(&gpuMatrix, XMMatrixTranspose(wvp));
```

## 52. HLSLへの転送規約B

HLSL Constant Buffer側へ`row_major`を明記し、CPU側行列をTransposeせず同じRow-major Storageとして渡す方式もあります。

## 53. 規約を混ぜない

ShaderごとにTranspose有無や`mul`順が違うと保守不能になります。Engine共通規約と自動Testを作ります。

## 54. XMFLOAT4X4保存

```cpp
XMFLOAT4X4 stored{};
XMStoreFloat4x4(&stored, matrix);
XMMATRIX loaded = XMLoadFloat4x4(&stored);
```

## 55. Matrix Transpose

```cpp
XMMATRIX transposed = XMMatrixTranspose(matrix);
```

Inverseとは違います。Row/Columnを交換するだけです。

## 56. Matrix Inverse

```cpp
XMVECTOR determinant{};
XMMATRIX inverse = XMMatrixInverse(&determinant, matrix);
```

逆変換を求めます。Singular Matrixへは有効なInverseがありません。

## 57. Determinant

行列が反転可能か、Mirror変換を含むか等の手掛かりになります。0に非常に近い場合の数値安定性へ注意します。

## 58. Singular Matrix

Scaleの一成分が0など、空間を潰す変換はInverseを持ちません。Camera/ViewやNormal Matrix計算へ渡す前に検証します。

## 59. Normal Matrix

非一様Scaleを含むWorld Matrixでは、Normalを位置と同じ行列で変換するとSurfaceへ垂直でなくなります。Worldの線形部分の逆転置を使います。

## 60. Normal変換後のNormalize

Matrix変換や頂点補間後、Normalを再NormalizeしてLightingへ使います。

## 61. Tangent Space

Tangent、Bitangent、NormalでTexture NormalをWorld/View Spaceへ変換します。三本の直交性とHandednessを保ちます。

## 62. Quaternionとは

Rotationを4成分で表す形式です。Euler角より補間と合成に適し、Gimbal Lockを避けられます。

## 63. Quaternion作成

```cpp
XMVECTOR q = XMQuaternionRotationRollPitchYaw(
    pitchRadians,
    yawRadians,
    rollRadians);
```

引数順とAxis規約を関数名・資料で確認します。

## 64. Quaternion Normalize

```cpp
q = XMQuaternionNormalize(q);
```

Rotation QuaternionはUnit長を保ちます。多数回合成後は誤差を補正します。

## 65. Quaternion乗算

Rotation合成順は重要です。Matrixと同様、可換ではありません。Local回転とWorld回転をTestで区別します。

## 66. Slerp

```cpp
XMVECTOR q = XMQuaternionSlerp(a, b, t);
```

球面線形補間で回転を滑らかに補間します。

## 67. Quaternionの二重表現

qと-qは同じRotationを表します。補間前にDot符号を考慮し、長い回転経路を選ばないようにします。DirectXMath関数の挙動も確認します。

## 68. Matrixへ変換

```cpp
XMMATRIX rotation = XMMatrixRotationQuaternion(q);
```

Transform合成やGPU転送に使えます。

## 69. Matrix Decompose

```cpp
XMVECTOR scale{};
XMVECTOR rotationQuaternion{};
XMVECTOR translation{};
bool ok = XMMatrixDecompose(
    &scale, &rotationQuaternion, &translation, matrix);
```

ShearやSingular変換では期待どおり分解できない場合があります。

## 70. Transform Component

```cpp
struct Transform
{
    XMFLOAT3 position{0, 0, 0};
    XMFLOAT4 rotation{0, 0, 0, 1};
    XMFLOAT3 scale{1, 1, 1};
};
```

保存はSRT、必要時にWorld Matrixを作る方式です。

## 71. Dirty Flag

Position/Rotation/Scaleが変化したときだけLocal/World Matrixを再計算します。Parent変更も子へ伝播します。

## 72. 親子Transform

Row Vector規約では、子Localから親空間、Worldへ進むよう行列を合成します。Project規約に基づき小さなTestで順序を固定します。

## 73. Hierarchy更新順

Parent Worldを先に確定し、その後Child Worldを更新します。Scene GraphのCycleを禁止・検出します。

## 74. Bone Matrix

概念上、Bind PoseからBone Localへ戻すInverse Bindと現在Bone World等を組み合わせ、Mesh頂点を現在Poseへ変換します。Format規約により式順が変わります。

## 75. Camera Basis

ForwardがUpとほぼ平行になるとCross Productが不安定になります。代替Upを選ぶなどDegenerate Caseを処理します。

## 76. Handedness

Left-handed/Right-handedはForward軸、Cross順、View/Projection関数、Winding、Asset Importに影響します。一つのProject規約へ正規化します。

## 77. LH/RH関数

DirectXMathには`XMMatrixLookAtLH/RH`や`XMMatrixPerspectiveFovLH/RH`があります。名前を省略せず座標系に合う方を使います。

## 78. 単位系

1 Unitを1 Meter等に定めます。Physics、Animation、Camera、Lighting Attenuation、Imported Model Scaleを一致させます。

## 79. Float精度

World原点から非常に遠い座標ではFloatの刻みが粗くなり、Camera揺れやSkinning誤差が見えます。

## 80. Large World対策

Floating Origin、Camera-relative Rendering、区画＋Local座標、CPU Double/GPU Float分離などを使います。

## 81. Epsilon比較

Floatを常に`==`で比較せず、問題のScaleに合うAbsolute/Relative Toleranceを使います。一つの万能Epsilonはありません。

## 82. NaNとInfinity

Zero Normalize、0除算、Singular Inverse、無効`acos`入力などで発生します。Debug BuildでFinite検査を入れます。

## 83. acos入力Clamp

Dotが丸め誤差で[-1,1]を少し超える場合があるため、角度変換前にClampします。

## 84. Debug可視化

Position、Basis軸、Normal、Velocity、Bone、BoundsをLine描画すると、数値だけで追いにくい座標系Bugを発見できます。

## 85. よくある失敗：XMFLOAT3で全計算

成分を手書きしSIMD関数と規約を失います。LoadしてXMVECTORで計算しStoreします。

## 86. よくある失敗：PointとDirectionを混同

Normalへ平行移動を適用したり、PositionをNormalizeしたりします。変数名とHelper関数で意味を表します。

## 87. よくある失敗：行列順を勘で直す

表示されるまで順序を入れ替え、別Objectで破綻します。単一変換と合成順をUnit Testします。

## 88. よくある失敗：Transposeを二回行う

CPUでTransposeし、HLSLも`row_major`へ変え、規約が二重反転します。転送境界を一か所にします。

## 89. よくある失敗：非一様ScaleでNormalを直接変換

Lightingが歪みます。Inverse TransposeとNormalizeを使います。

## 90. Vector Test

- Dotで同方向/直交/逆方向を確認する。
- Cross順序で符号が反転する。
- Zero Normalizeを安全に処理する。
- Point/DirectionでTranslation結果が異なる。
- Length Squared比較とLength比較が一致する。

## 91. Matrix Test

- Identityで値が変わらない。
- S/R/Tを個別に確認する。
- 合成順で自転/公転が変わる。
- Matrix×InverseがIdentity付近になる。
- Singular Matrixを拒否する。

## 92. CPU-HLSL Test

- 既知PointをCPUとShaderで同じ結果へ変換する。
- Translation、Rotation、Projectionを個別に試す。
- Transpose規約を一つに固定する。
- Window Resize後のProjectionを確認する。
- GPU CaptureでConstant Buffer行列を読む。

## 93. Quaternion/Hierarchy Test

- Identity Quaternionで回転しない。
- 90度回転のBasisを確認する。
- Slerp中間姿勢を確認する。
- Parent移動でChildが追従する。
- Negative ScaleとWindingを確認する。

## 94. 完成確認表

- [ ] 保存型と計算型を使い分けられる。
- [ ] AlignmentとLoad/Storeを説明できる。
- [ ] PointとDirectionのwを区別できる。
- [ ] Dot、Cross、Normalizeを安全に使える。
- [ ] S/R/Tの行列乗算順を説明できる。
- [ ] Local/World/View/Clip/NDCを順に説明できる。
- [ ] CPU-HLSLのStorage/乗算規約を統一できる。
- [ ] Normal Matrixが必要な理由を説明できる。
- [ ] Quaternionで回転を合成・補間できる。
- [ ] 階層Transformと数値誤差を管理できる。

## 95. この章の要点

- `XMFLOAT`は保存、`XMVECTOR/XMMATRIX`はSIMD計算に使います。
- Positionはw=1、Directionはw=0として平行移動の影響を分けます。
- Dotは方向関係、Crossは垂直方向、Normalizeは長さ1を作ります。
- DirectXMathのRow Vector規約では変換適用順に行列を左から合成します。
- Matrix Storageと数学上のVector方向を別概念として扱います。
- CPU-HLSL間のTranspose/`row_major`規約をProject全体で一つにします。
- 非一様Scale下のNormalにはInverse Transposeが必要です。
- Quaternionは回転の合成・補間、Hierarchyは親から子の更新順が重要です。

## 96. 公式資料

- [DirectXMath programming guide](https://learn.microsoft.com/en-us/windows/win32/dxmath/pg-xnamath-getting-started)
- [DirectXMath library reference](https://learn.microsoft.com/en-us/windows/win32/dxmath/directxmath-portal)
- [XMVECTOR data type](https://learn.microsoft.com/en-us/windows/win32/dxmath/xmvector-data-type)
- [XMMATRIX structure](https://learn.microsoft.com/en-us/windows/win32/api/directxmath/ns-directxmath-xmmatrix)
- [DirectXMath library internals](https://learn.microsoft.com/en-us/windows/win32/dxmath/pg-xnamath-internals)
- [XMVector3TransformCoord](https://learn.microsoft.com/en-us/windows/win32/api/directxmath/nf-directxmath-xmvector3transformcoord)
- [XMVector3TransformNormal](https://learn.microsoft.com/en-us/windows/win32/api/directxmath/nf-directxmath-xmvector3transformnormal)
- [XMMatrixInverse](https://learn.microsoft.com/en-us/windows/win32/api/directxmath/nf-directxmath-xmmatrixinverse)
- [XMQuaternionSlerp](https://learn.microsoft.com/en-us/windows/win32/api/directxmath/nf-directxmath-xmquaternionslerp)
- [HLSL matrix ordering](https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/dx-graphics-hlsl-per-component-math)

次章では、View Matrix、Perspective/Orthographic Projection、Camera操作、Screen Ray、Jitter、Camera Shake、追従Cameraを扱います。
