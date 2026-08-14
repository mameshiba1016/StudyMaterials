# DirectX 11：Rasterizer・Cull・Scissor

この章では、Vertex Shader後のPrimitiveをPixel候補へ変換するRasterizer Stageを学びます。Fill Mode、Front Face、Culling、Depth Clip、Depth Bias、Multisample、Scissor Rectangle、State Cache、Shadow Map、UI、分割画面、Debug表示までを扱います。

## 1. Rasterizer Stageの位置

```text
Input Assembler
-> Vertex Shader
-> optional Tessellation / Geometry Shader
-> clipping
-> perspective divide / viewport transform
-> rasterization
-> Pixel Shader
-> Output Merger
```

PrimitiveからFragment候補を生成する段階です。

## 2. Rasterizationとは

点、線、三角形が画面上のどのSample/Pixelを覆うかを判定し、Pixel Shaderへ渡す入力を生成する処理です。

## 3. Fragmentと最終Pixel

Rasterizerが候補を作っても、Depth Test、Stencil Test、Blendなどで最終Render Targetへ書かれない場合があります。

## 4. Rasterizer State

塗りつぶし方、Cull方向、Front Face規約、Depth Bias、Clip、Scissor、Multisample関連設定をまとめたImmutable State Objectです。

## 5. Descriptor全体

```cpp
D3D11_RASTERIZER_DESC desc{};
desc.FillMode = D3D11_FILL_SOLID;
desc.CullMode = D3D11_CULL_BACK;
desc.FrontCounterClockwise = FALSE;
desc.DepthBias = 0;
desc.DepthBiasClamp = 0.0f;
desc.SlopeScaledDepthBias = 0.0f;
desc.DepthClipEnable = TRUE;
desc.ScissorEnable = FALSE;
desc.MultisampleEnable = FALSE;
desc.AntialiasedLineEnable = FALSE;
```

## 6. CreateRasterizerState

```cpp
Microsoft::WRL::ComPtr<ID3D11RasterizerState> state;
ThrowIfFailed(device->CreateRasterizerState(
    &desc,
    state.GetAddressOf()));
```

作成後にDescriptorは変更できません。

## 7. RSSetState

```cpp
context->RSSetState(state.Get());
```

Rasterizer StageのContext Stateとして、以降のDrawへ適用されます。

## 8. nullptr State

`RSSetState(nullptr)`はDirect3Dの既定Rasterizer Stateへ戻します。Engineでは暗黙既定へ頼らず、名前付きStateを明示する方が診断しやすくなります。

## 9. 既定Stateの考え方

既定値はSolid、Back Cull、Clockwise Front相当などですが、Projectの座標・Winding規約を自分のDescriptorで表現します。

## 10. Fill Mode

- `D3D11_FILL_SOLID`：三角形内部を塗りつぶす。
- `D3D11_FILL_WIREFRAME`：辺を線としてRasterizeする。

## 11. Solid Fill

通常の3D描画に使います。Material、Lighting、Textureを面全体へ適用します。

## 12. Wireframe

Topology、LOD、Tessellation、Mesh密度を可視化するDebug表示に使います。製品用Outline表現としては太さ・隠線・品質に制約があります。

## 13. WireframeとLine List

Wireframe Fillは三角形の辺をRasterizeし、Line ListはInput Primitive自体が線です。目的と入力Topologyが異なります。

## 14. Front Face

画面上へ投影された三角形の頂点順序がClockwiseかCounter-clockwiseかで表面を決めます。

## 15. FrontCounterClockwise

```text
FALSE -> clockwise winding is front-facing
TRUE  -> counter-clockwise winding is front-facing
```

Model Export、座標変換、Projection規約と一致させます。

## 16. Windingの例

```text
index order: 0, 1, 2
screen projected positions determine CW/CCW
```

3D座標だけでなく、最終的な画面上の向きが重要です。

## 17. Cull Mode

- `D3D11_CULL_NONE`：表裏とも残す。
- `D3D11_CULL_FRONT`：表面を捨てる。
- `D3D11_CULL_BACK`：裏面を捨てる。

## 18. Back-face Culling

閉じたMeshの内側向き三角形をPixel処理前に除外し、不要なRasterizationを減らします。

## 19. CullingはVisibility全体ではない

Back-face Cullは向きだけを見ます。Camera Frustum、Occlusion、Distance、PortalなどのObject単位Cullingとは別です。

## 20. Cull Noneの用途

葉、布、紙、Particle Quad、Debug Geometryなど両面表示が必要なMaterialに使います。ただし裏面LightingのNormal規約も必要です。

## 21. Double-sided Normal

両面Materialでは`SV_IsFrontFace`をPixel Shaderで受け、裏面Normalを反転する設計があります。単にCullを切るだけではLightingが不自然になる場合があります。

## 22. SV_IsFrontFace

```hlsl
float4 PSMain(
    PixelInput input,
    bool isFrontFace : SV_IsFrontFace) : SV_Target0
{
    float3 n = isFrontFace ? input.normal : -input.normal;
    // lighting...
}
```

## 23. Front Cullの用途

Shadow Volume、Outline用拡張Mesh、Cube内側描画など、裏面だけを描くPassで利用できます。

## 24. Negative Scale

World Matrixに負Scaleが含まれると座標系のHandednessが反転し、Windingが逆になる場合があります。

## 25. Determinantで反転を知る

World Transformの3×3部分のDeterminant符号からMirror変換を検出できます。Cull Stateを反転するか、Asset/Transform規約で負Scaleを制限します。

## 26. SkinningとWinding

通常のBone変形ではTriangle Index順自体は変わりませんが、極端な反転変形や負Scale Boneで面が裏返る可能性があります。

## 27. 座標系変換時の注意

右手系から左手系へ一Axisを反転するとWindingも反転します。Position、Normal、Tangent、Index順を一体で変換します。

## 28. Clippingとは

View Frustum境界をまたぐPrimitiveを境界で切り、新しい頂点を作って見える部分だけ残す処理です。

## 29. CullingとClippingの違い

```text
culling : primitive/object全体を条件で捨てる
clipping: primitiveを境界で切り、見える一部を残す
```

## 30. Direct3D Clip Volume

概念上、Homogeneous Clip Spaceで次の範囲が基本です。

```text
-w <= x <= w
-w <= y <= w
 0 <= z <= w
```

## 31. DepthClipEnable

`TRUE`でNear/Farに関するDepth Clipを有効にする通常設定です。X/Y Clipや`w`条件まで単純に全停止するFlagではありません。

## 32. Depth Clip無効の用途

Shadow Volume等の特殊Algorithmで使われます。一般Camera描画で無効にすると想定外のDepth値や表示になるため、目的を明確にします。

## 33. Near Plane問題

Cameraへ近すぎるGeometryが切れるのはNear Planeによる正常なClippingの場合があります。Depth Clipを切るよりProjection Near値とCamera Collisionを調整します。

## 34. User Clip Distance

Shaderから`SV_ClipDistance`を出し、任意PlaneでPrimitiveをClipできます。Mirror、Portal、水面などに利用します。

## 35. Cull Distance

`SV_CullDistance`はPrimitive全体を除外する距離条件をShaderから渡します。Clip Distanceとの挙動を区別します。

## 36. Depth Biasとは

Rasterized DepthへOffsetを加え、ほぼ同じ面同士のDepth競合を調整する機能です。Shadow MapのSelf-shadowing対策などで使います。

## 37. DepthBias

整数値による定数Biasです。Depth Formatに応じた最小表現単位へScaleされるため、World単位の距離ではありません。

## 38. SlopeScaledDepthBias

三角形のDepth勾配に応じてBiasを増やします。Lightに対して斜めのSurfaceでShadow Acneを抑えるのに役立ちます。

## 39. DepthBiasClamp

計算されたBiasの上限/下限を制限します。0はClamp無効として扱われる基本設定です。

## 40. Shadow Acne

Shadow Map作成時と参照時の量子化・Sample位置差により、Surfaceが自分自身をShadowと判定する縞状Artifactです。

## 41. Peter Panning

Biasが大きすぎるとShadowがObjectから浮いて見えます。Acneを消すためにBiasを無制限に増やしません。

## 42. Bias調整の軸

```text
shadow map resolution
light projection range
constant bias
slope-scaled bias
normal bias in shader
receiver comparison/filter
```

一つの値だけで全Sceneを解決しません。

## 43. Reversed-ZとBias

Depth比較方向とDepth分布が反転するため、Bias符号と調整規約も見直します。通常Zの値をそのまま流用しません。

## 44. Scissor Testとは

Screen上の矩形外にあるRasterizer出力を破棄します。Viewport変換後のPixel座標領域を切り取ります。

## 45. ScissorEnable

```cpp
desc.ScissorEnable = TRUE;
```

Rasterizer State側で有効にしない限り、Scissor Rectangleを設定しても切り取りは行われません。

## 46. D3D11_RECT

```cpp
D3D11_RECT rect{};
rect.left = 100;
rect.top = 50;
rect.right = 500;
rect.bottom = 350;
```

Signed整数のScreen座標です。

## 47. 右端と下端

Rectangleは左・上を含み、右・下を含まない半開区間として考えます。

```text
x in [left, right)
y in [top, bottom)
```

## 48. Width/Heightとの変換

```cpp
rect.right = rect.left + width;
rect.bottom = rect.top + height;
```

`right`へWidthそのものを入れるのではなく、絶対座標へ変換します。

## 49. RSSetScissorRects

```cpp
context->RSSetScissorRects(1, &rect);
```

Scissor Rectangle配列をContext Stateへ設定します。

## 50. ScissorとViewport

ViewportはClip/NDCからScreen座標への変換領域、Scissorはその後の矩形切取りです。二つは別設定です。

## 51. 実効描画領域

概念上、Render Target境界、Viewport、Scissorの共通部分が描画可能領域になります。

## 52. ScissorとClear

`ClearRenderTargetView`はScissor Rectangleへ制限されません。部分Clearが必要ならQuad描画や対応するClear拡張等、別手段を使います。

## 53. UI Clip

Scroll View、Panel、Inventory Slotなど矩形UIの子要素を親領域内へ切り取る用途に適します。

## 54. Nested UI Clip

親と子のScissor RectangleのIntersectionを計算し、最も狭い矩形をBindingします。StackでPush/Popすると管理しやすくなります。

## 55. 空Intersection

`right <= left`または`bottom <= top`ならDrawを省略できます。無効Rectangleを無理にAPIへ渡しません。

## 56. Render Target境界へClamp

UI計算で負座標や画面外が出ても、最終ScissorをRender Target範囲へClampします。整数変換Overflowも防ぎます。

## 57. DPIとScissor

UIの論理座標からBack Buffer Pixel座標へScaleしてからScissor Rectangleを作ります。DPI ScaleとRender Resolution Scaleを混同しません。

## 58. Fractional UI座標

Float境界を整数Scissorへ変換するとき、左上はFloor、右下はCeilなど、Contentを欠けさせない丸め規約を決めます。

## 59. Split Screen

CameraごとにViewportとScissorを設定し、隣の領域へEffectやFullscreen Triangleがはみ出すのを防ぎます。

## 60. Shadow Atlas

Atlas内のTileごとにViewportとScissorを設定します。Scissorは隣Tileへの書込みを防ぎ、PaddingもFilter境界用に確保します。

## 61. Post Process

部分ViewportへFullscreen Triangleを描く場合、Scissorも同領域へ合わせるとScreen全体への不要出力を防げます。

## 62. 複数ViewportとScissor

複数Viewportを使う高度な描画では対応するScissor配列とFeature Level制約を確認します。単一Passでの対応関係を明示します。

## 63. Scissor Stateの残留

UI Passの狭いScissorが次の3D Passへ残ると画面の一部しか描かれません。Pass開始時に必要Stateを明示します。

## 64. Scissor無効Stateへ戻す

Rectangleだけを全画面へ戻す方法と、`ScissorEnable = FALSE`のRasterizer Stateへ切り替える方法があります。Renderer規約を一つにします。

## 65. MultisampleEnable

MSAA Render Targetで三角形Rasterization規則に関係します。Sample Countを増やすResource設定とは別Fieldです。

## 66. MSAAはState一つで有効にならない

Color/Depth ResourceのSample Count、Resolve、Rasterizer設定などを合わせます。`MultisampleEnable = TRUE`だけでBack BufferがMSAA化されません。

## 67. AntialiasedLineEnable

Line Anti-aliasingに関係するFlagです。Fill Mode、Multisample設定、Feature Level、Line描画規則との条件を確認します。

## 68. Conservative Rasterization

三角形が少しでもPixelを覆えば候補に含める方式で、VoxelizationやCollision的なScreen Coverageに役立ちます。新しいDevice InterfaceとFeature Queryが必要なOptional機能です。

## 69. Conservative Rasterの区別

通常の`D3D11_RASTERIZER_DESC`だけでは設定できません。対応TierをQueryし、対応する新しいRasterizer Descriptor/Interfaceを使います。

## 70. Rasterizer State Cache Key

```text
fill mode
cull mode
front winding
depth bias fields
depth clip
scissor
multisample
line AA
```

全FieldをHash/比較へ含めます。

## 71. Stateの有限集合

```text
SolidBackCull
SolidNoCull
WireframeNoCull
ShadowBackCullBiased
UiScissorNoCull
```

用途別に名前を付けるとMaterialとPassの責任が見えます。

## 72. MaterialとPassのどちらが持つか

両面性はMaterial、Shadow BiasはShadow Pass、ScissorはUI Draw Commandなど、設定のSource of TruthをFieldごとに決めます。

## 73. State変更Cost

同じStateを連続Bindingする無駄は減らせますが、Cacheと実Context Stateがずれないよう、Context操作をRendererへ集約します。

## 74. Draw Sorting

Opaque DrawをRasterizer State、Shader、Material等で並べ替えるとState変更を減らせます。ただしDepth順やAnimation更新とのTrade-offを計測します。

## 75. Pipeline Key

Direct3D 11には単一PSOがありませんが、Shader、Input Layout、Rasterizer、Blend、Depth Stencil、Topologyの組を論理Pipeline Keyとして管理できます。

## 76. Debug Name

```cpp
SetDebugName(*state.Get(), "RS Solid BackCull CW");
```

Fill、Cull、Front、Scissor、Bias用途を識別できる名前にします。

## 77. Graphics Debuggerで見る項目

- Rasterizer State Descriptor。
- Front/Back判定。
- ViewportとScissor。
- Clipped/Culled Primitive数。
- Wireframe表示。
- Depth Bias値。

## 78. よくある失敗：すべてCull None

表示不具合を隠すため両面描画にし、Pixel処理とLighting問題を増やします。Winding規約を修正し、必要Materialだけ両面にします。

## 79. よくある失敗：ModelごとにWindingが違う

Import設定が統一されず、State切替やCull Noneが乱立します。Asset Build時に座標系とIndex順を正規化します。

## 80. よくある失敗：負Scaleで消える

Mirror TransformによりWindingが反転します。Transform制約、Index反転、Cull State反転のどれを採るか決めます。

## 81. よくある失敗：ScissorEnable忘れ

Rectangleを設定しただけで切り取られると思います。Scissor有効Rasterizer StateもBindingします。

## 82. よくある失敗：right/bottomをSizeで指定

Offset付きUIで領域が小さくなったり負になります。左上座標にWidth/Heightを加算します。

## 83. よくある失敗：ClearもScissorされると思う

Scissor設定後にRTV全体をClearしてしまいます。Clear APIとRasterized Drawの適用範囲を区別します。

## 84. よくある失敗：Biasを全Passへ適用

Shadow用Bias Stateが通常描画へ残り、Z-fightingや浮きが起きます。Pass開始時にStateを明示します。

## 85. Winding Test

- Clockwise/Counter-clockwise Triangleを並べる。
- Back/Front/None Cullを切り替える。
- FrontCounterClockwiseを反転する。
- Negative Scaleで結果を確認する。
- Import後の全Mesh規約を検証する。

## 86. Scissor Test

- 原点からの矩形を描く。
- Offset付き矩形を描く。
- 負座標と画面外をClampする。
- Nested ClipのIntersectionを確認する。
- Resize/DPI変更後にPixel領域を更新する。

## 87. Bias Test

- Bias 0でShadow Acneを観察する。
- Constant/Slope Biasを別々に変える。
- Peter Panningを確認する。
- Cascade/Light別に必要値を測る。
- Reversed-Z時に符号を再確認する。

## 88. State Lifecycle Test

- Descriptor全Fieldをゼロ初期化後に設定する。
- 同一DescriptorをCacheで共有する。
- Pass間でState残留がない。
- Device Lost後に再作成する。
- Graphics Debugger上のState名を確認する。

## 89. 完成確認表

- [ ] RasterizerがPrimitiveからFragment候補を作ると説明できる。
- [ ] SolidとWireframeを使い分けられる。
- [ ] Winding、Front Face、Cull Modeの関係を説明できる。
- [ ] Negative Scaleと座標系変換の反転を扱える。
- [ ] CullingとClippingを区別できる。
- [ ] Depth Bias三FieldをShadow用に調整できる。
- [ ] ViewportとScissorを区別できる。
- [ ] Scissorの半開区間と座標単位を説明できる。
- [ ] MSAA Resource設定とRasterizer Flagを区別できる。
- [ ] PassごとのRasterizer StateをCache・復元できる。

## 90. この章の要点

- RasterizerはPrimitive Coverageを求め、Pixel Shader入力を生成します。
- Fill Modeは面をSolidまたはWireframeで処理します。
- Front Faceは投影後のWindingと`FrontCounterClockwise`で決まります。
- Back-face Cullingは向きによるPrimitive除外で、Frustum Cullingとは別です。
- Depth BiasはShadow Acneを減らしますが、大きすぎるとPeter Panningを生みます。
- ScissorはViewport変換後の矩形切取りで、Rasterizer State側の有効化が必要です。
- Scissorの右・下境界は含まない半開区間として扱います。
- Pass開始時にRasterizer State、Viewport、Scissorを明示し、前Passの残留を防ぎます。

## 91. 公式資料

- [Rasterizer stage](https://learn.microsoft.com/en-us/windows/win32/direct3d11/d3d10-graphics-programming-guide-rasterizer-stage)
- [D3D11_RASTERIZER_DESC](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/ns-d3d11-d3d11_rasterizer_desc)
- [ID3D11Device::CreateRasterizerState](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11device-createrasterizerstate)
- [ID3D11DeviceContext::RSSetState](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11devicecontext-rssetstate)
- [ID3D11DeviceContext::RSSetScissorRects](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11devicecontext-rssetscissorrects)
- [D3D11_CULL_MODE](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/ne-d3d11-d3d11_cull_mode)
- [D3D11_FILL_MODE](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/ne-d3d11-d3d11_fill_mode)
- [Depth bias](https://learn.microsoft.com/en-us/windows/win32/direct3d11/d3d10-graphics-programming-guide-output-merger-stage-depth-bias)
- [Conservative rasterization](https://learn.microsoft.com/en-us/windows/win32/direct3d11/conservative-rasterization)

次章では、Pixel Shader出力とRender Target既存値をどの式で合成するかを決めるBlend State、Alpha、複数Render TargetのColor Write制御を扱います。
