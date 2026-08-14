# DirectX 11：Depth Stencil State

この章では、Fragmentの奥行きを比較するDepth Testと、Pixel単位の整数Maskを使うStencil Testの規則を定義するDepth Stencil Stateを学びます。Depth比較、書込み、Stencil Read/Write Mask、三つの結果別Operation、Front/Back Face、Stencil Reference、Outline、Portal、Light Volume、Reversed-Zまでを扱います。

## 1. ResourceとStateを分ける

```text
Depth Stencil Texture / DSV : depth・stencil値を保存する場所
Depth Stencil State         : 比較・書込み・更新規則
Stencil Reference           : Draw時に渡す動的比較値
```

DSVを作っただけでは望む規則になりません。

## 2. Output Mergerでの役割

Rasterizerから来たFragmentについてDepth/Stencil条件を評価し、Color/Depth/Stencilへの書込み可否とStencil更新を決めます。

## 3. Depth Test

新しいFragment DepthとDepth Bufferの保存値を比較します。条件を通過したFragmentだけが後続の書込み候補になります。

## 4. Depth Write

Depth Testを通過した新DepthをBufferへ保存する機能です。Testの有効/無効とWriteの有効/無効は別です。

## 5. Descriptor全体

```cpp
D3D11_DEPTH_STENCIL_DESC desc{};
desc.DepthEnable = TRUE;
desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
desc.DepthFunc = D3D11_COMPARISON_LESS;
desc.StencilEnable = FALSE;
desc.StencilReadMask = D3D11_DEFAULT_STENCIL_READ_MASK;
desc.StencilWriteMask = D3D11_DEFAULT_STENCIL_WRITE_MASK;
```

Stencil Face設定も後で明示します。

## 6. CreateDepthStencilState

```cpp
Microsoft::WRL::ComPtr<ID3D11DepthStencilState> state;
ThrowIfFailed(device->CreateDepthStencilState(
    &desc,
    state.GetAddressOf()));
```

作成後のDescriptorは変更できません。

## 7. OMSetDepthStencilState

```cpp
context->OMSetDepthStencilState(state.Get(), stencilReference);
```

State Objectと動的Stencil Reference値を同時に設定します。

## 8. nullptr State

`nullptr`は既定Depth Stencil Stateを使います。EngineではPassごとの名前付きStateを明示すると残留Bugを減らせます。

## 9. DepthEnable

`TRUE`でDepth比較を有効にします。DSVがBindingされていなければ、保存値へ正常にTest/Writeできません。

## 10. DepthWriteMask ALL

Testを通過したDepthを書き込みます。不透明Geometryの基本です。

## 11. DepthWriteMask ZERO

Depthを比較しても保存値を変更しません。半透明物、Sky、Overlayなどで使います。

## 12. Test ON・Write OFF

透明Objectは不透明物の後ろなら隠れる必要があるためDepth Testを有効にし、透明物同士を完全遮断しないようWriteを無効にするのが基本です。

## 13. Test OFF・Write OFF

Depthに関係なく描くUIや一部Post Processで使います。3D PassのStateを引き継がないようにします。

## 14. Test OFF・Write設定の注意

Depth Test無効時のDepth書込みへ期待を持たず、Depthへ値を書きたいPassでは有効な比較規則を明示します。

## 15. Comparison Function

- `NEVER`：常に失敗。
- `LESS`：Sourceが保存値より小さい。
- `EQUAL`：等しい。
- `LESS_EQUAL`：小さいか等しい。
- `GREATER`：大きい。
- `NOT_EQUAL`：等しくない。
- `GREATER_EQUAL`：大きいか等しい。
- `ALWAYS`：常に成功。

## 16. 通常Z

Depthを1.0でClearし、Cameraに近いほど小さい値として`LESS`または`LESS_EQUAL`を使います。

## 17. LESSとLESS_EQUAL

同じDepthの再描画を通す必要があるかで選びます。完全一致へ依存すると浮動小数・Rasterization差で不安定になる場合があります。

## 18. Reversed-Z

Depthを0.0でClearし、Cameraに近いほど大きい値として`GREATER`または`GREATER_EQUAL`を使います。

## 19. Reversed-Zの一式

```text
projection mapping
clear depth = 0
comparison = GREATER/GREATER_EQUAL
depth bias sign/policy
depth reconstruction
```

比較関数だけ反転しても完成しません。

## 20. EQUALの用途

Depth Prepass後に同じGeometryをColor Passで描き、事前Depthと一致するFragmentだけ通す構成などがあります。

## 21. ALWAYSの用途

Depth Testを論理的に常時通しつつ、Stencil等を使う特殊Passに利用できます。DepthEnable自体を切る場合との違いを明示します。

## 22. Sky描画

Skyを遠端Depthへ描き、Depth Writeを無効、比較を`LESS_EQUAL`系にする方式があります。通常Z/Reversed-Zで遠端値と比較方向を変えます。

## 23. Depth Prepass

Colorをほぼ書かずDepthを先に作り、後続の重いPixel ShaderをEarly Depthで減らす方式です。Geometryを二回処理するCostとのTrade-offがあります。

## 24. Prepass State

```text
DepthEnable = true
DepthWrite = ALL
DepthFunc = LESS (normal Z)
ColorWriteMask = 0
Pixel Shader = optional/none where valid
```

## 25. Color Pass after Prepass

Depth Writeを無効にし、`EQUAL`または`LESS_EQUAL`を使う方式があります。PrepassとColor Passの頂点変形・Alpha Cutoutが一致する必要があります。

## 26. Early Depth Test

GPUがPixel Shader実行前にDepth判定できれば、隠れたFragmentのShader Costを省けます。

## 27. Early Depthを妨げる要因

Pixel ShaderのDepth出力、`discard/clip`、UAV副作用などはEarly Test/Write最適化へ影響する場合があります。GPU Captureで確認します。

## 28. Overdraw

同じPixelへ多数の面が重なる状態です。Front-to-back描画、Depth Prepass、Culling、LOD等で重いPixel Shader実行を減らせます。

## 29. Stencil Buffer

各Pixelに小さな整数値を保存し、Referenceとの比較と、Pass/Fail条件による更新を行います。一般的なD24S8では8 bitです。

## 30. StencilEnable

```cpp
desc.StencilEnable = TRUE;
```

Stencil付きDSV FormatをBindingし、規則を設定します。

## 31. Stencil Reference

`OMSetDepthStencilState`の第2引数でDraw時に渡す値です。State Objectを作り直さずObject/Region IDを変更できます。

## 32. StencilReadMask

保存Stencil値とReferenceの比較に使うBitを選びます。

```text
maskedStored = stored & readMask
maskedRef    = reference & readMask
```

## 33. StencilWriteMask

Stencil Operation結果を書き戻すBitを選び、Mask外の既存Bitを保持します。

## 34. Bit割当

```text
bit 0     : character outline mask
bit 1     : portal mask
bits 2-4  : lighting category
bits 5-7  : reserved
```

Subsystem同士が同じBitを破壊しない共通表を作ります。

## 35. FrontFaceとBackFace

Stencil規則は表面・裏面で別々に設定できます。

```cpp
desc.FrontFace = frontRules;
desc.BackFace = backRules;
```

## 36. D3D11_DEPTH_STENCILOP_DESC

```cpp
D3D11_DEPTH_STENCILOP_DESC rules{};
rules.StencilFailOp = D3D11_STENCIL_OP_KEEP;
rules.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
rules.StencilPassOp = D3D11_STENCIL_OP_REPLACE;
rules.StencilFunc = D3D11_COMPARISON_ALWAYS;
```

## 37. 三つのStencil Operation経路

```text
stencil test fails                  -> StencilFailOp
stencil passes, depth test fails    -> StencilDepthFailOp
stencil and depth both pass         -> StencilPassOp
```

どの条件で更新されるかを表で設計します。

## 38. KEEP

現在のStencil値を保持します。最も安全な既定Operationです。

## 39. ZERO

対象Bitを0へします。Write Maskが適用されます。

## 40. REPLACE

Stencil Reference値で置換します。Object/Region Maskを書き込む基本です。

## 41. INCR_SATとDECR_SAT

上限/下限で飽和する加算・減算です。255より増えず、0より減りません。

## 42. INCRとDECR

値が範囲を超えるとWrapする加算・減算です。飽和版と区別します。

## 43. INVERT

対象Bitを反転します。Parity判定などに利用できます。

## 44. Stencil Comparison

Depthと同じComparison列挙を使いますが、比較対象はMask適用済みReferenceと保存Stencil値です。

## 45. 比較の向き

API定義上どちらがSource/Reference側かを確認し、`LESS`等の向きを推測で決めません。既知値を描くTestを用意します。

## 46. Stencil Clear

```cpp
context->ClearDepthStencilView(
    dsv.Get(),
    D3D11_CLEAR_STENCIL,
    1.0f,
    0);
```

Frame/Pass開始時に必要な初期値へ戻します。

## 47. Depthと同時Clear

```cpp
context->ClearDepthStencilView(
    dsv.Get(),
    D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL,
    clearDepth,
    clearStencil);
```

## 48. ClearはWrite Maskを無視する

Clear APIはDepth Stencil StateのStencil Write Maskで部分Clearする操作ではありません。必要ならDrawによるMask更新を設計します。

## 49. Outline：Mask Pass

Character本体を描きながらStencilへReference 1を書きます。

```text
StencilFunc = ALWAYS
PassOp = REPLACE
Reference = 1
```

## 50. Outline：拡張Pass

少し拡張したMeshを、Stencilが1ではないPixelだけ描きます。本体領域を避けて輪郭だけ残します。

## 51. OutlineのDepth方針

壁越しOutlineを出すか、見える部分だけ出すかでDepth Testが変わります。Gameplay視認性と遮蔽表現を明示します。

## 52. Outlineの拡張方法

頂点Normal方向拡張、Screen-space Edge、Post Process等があります。Stencil Outlineは一方式で、Mesh形状や距離により太さが変わります。

## 53. Portal Mask

Portal形状をStencilへ書き、その値と一致するPixelだけ別Camera Sceneを描きます。Depth、Camera Clip、再帰、Clear範囲も必要です。

## 54. Mirror Mask

Mirror面をStencilへ記録し、その領域内だけ反射Cameraを描きます。反転TransformによるWinding/Cull反転にも注意します。

## 55. UI Mask

非矩形UI ClipをStencilへ描き、子要素をMask内に限定できます。矩形だけならScissorの方が単純です。

## 56. Light Volume

Deferred LightingでSphere/Cone VolumeとStencilを組み合わせ、Lightが影響するPixelだけ重いLightingを実行する方式があります。

## 57. Shadow Volume

Front/Back Face別のIncrement/Decrementを使い、Shadow Volumeへの出入りをStencil Countとして記録するAlgorithmがあります。

## 58. Z-passとZ-fail

Shadow VolumeでDepth Pass/FailのどちらにStencilを更新するかが異なります。CameraがVolume内に入る場合やNear ClipへのRobustnessを考慮します。

## 59. Front/Back別Operationの価値

Volumeの入口面と出口面でIncrement/Decrementを変え、PixelがVolume内かをCountできます。

## 60. Read-only DSV

Depth/StencilをRead-onlyとしてBindingできるViewを使うと、対応部分をSRVとして読む高度なPassを構成できます。Resource/View FormatとBinding競合を確認します。

## 61. Depth SRVとの併用

Writable DSVと同じDepth SubresourceをSRVで同時に読むことはできません。Read-only DSVまたはPass分離を使います。

## 62. State組合せ例

```text
OpaqueDepthWrite
OpaqueDepthReadOnly
TransparentDepthReadOnly
DepthDisabled
DepthPrepass
StencilWriteReplace
StencilTestEqual
OutlineOutsideMask
```

## 63. State Cache

Descriptor全FieldをKeyへ含め、同一Stateを共有します。Stencil ReferenceはState KeyではなくDraw時の動的値です。

## 64. ReferenceはState Objectに入らない

同じStencil Stateを使いながらReference 1、2、3をDrawごとに変えられます。State CacheをReferenceごとに増やしません。

## 65. Pipeline Key

Depth Stencil State ObjectとStencil Referenceの両方を論理Pipeline/Draw Stateへ含めます。Objectだけ比較してReference変更を忘れないようにします。

## 66. Pass開始時の明示

State Object、Stencil Reference、DSV、Depth Clear規約をPass開始時に設定します。前PassのMaskやReferenceを引き継ぎません。

## 67. DSV Formatとの整合

Stencil機能を使うならStencil Channelを持つFormatが必要です。`D32_FLOAT`だけのDSVにはStencilがありません。

## 68. MSAA Depth Stencil

Color TargetとDepth TextureのSample Count/Qualityを一致させます。Stencil値もSample単位のCoverageと関係します。

## 69. Depth Precision

Stateを変えても、Near/Far設定やDepth Formatによる精度問題は残ります。Projection、Reversed-Z、Formatを一体で設計します。

## 70. Depth Biasとの境界

Depth BiasはRasterizer State、Depth Compare/WriteはDepth Stencil Stateです。Shadow Artifact調整で変更先を混同しません。

## 71. Blendとの関係

Depth/Stencil Testを通ったFragmentがColor Blendへ進みます。透明物の問題はDepth StateとBlend Stateの両方を確認します。

## 72. Debug Name

```cpp
SetDebugName(*state.Get(), "DS Transparent Less ReadOnly");
```

Depth Enable/Write/FuncとStencil用途を名前に含めます。

## 73. Graphics Debuggerで確認する

- DSV FormatとClear値。
- Depth Enable/Write/Func。
- Stencil Read/Write Mask。
- Front/Back Operation。
- Stencil Reference。
- Test前後のDepth/Stencil値。

## 74. よくある失敗：DSVだけ作って満足

StateをBindingせず既定規則が使われます。Passの意図に合うStateを明示します。

## 75. よくある失敗：透明物がDepth Write

後続透明面が消えます。Depth TestとWrite Maskを別々に設定します。

## 76. よくある失敗：Reversed-ZでLESSのまま

ほとんどのGeometryがTestに失敗します。Clear、Projection、Compare、Biasを一式で反転します。

## 77. よくある失敗：Stencil Reference更新忘れ

前ObjectのReferenceが残り、Mask比較/置換値が意図と違います。Draw CommandへReferenceを含めます。

## 78. よくある失敗：Read/Write Mask混同

比較対象Bitと書換え対象Bitを同じものだと思います。Subsystem別Bit割当を表にします。

## 79. よくある失敗：FrontFaceだけ設定

BackFace Descriptorがゼロ値のまま意図しない規則になります。両面を明示設定します。

## 80. Depth Test

- Near/Farに三角形を重ねる。
- LESS/LESS_EQUAL/EQUALを比較する。
- Write ALL/ZEROを比較する。
- 通常Z/Reversed-Zを別Testにする。
- 透明物と不透明物の前後を確認する。

## 81. Stencil Operation Test

- ReferenceをREPLACEで書く。
- EQUAL/NOT_EQUALで領域を描き分ける。
- Read/Write MaskをBit単位で確認する。
- Fail/DepthFail/Pass各経路を意図的に発生させる。
- Front/Back FaceでIncrement/Decrementする。

## 82. Effect Test

- Outlineが本体外側だけに出る。
- 壁越し表示Policyが正しい。
- Portal/Mirror外へ描画が漏れない。
- UI MaskのNested領域が正しい。
- Light VolumeのStencil値を可視化する。

## 83. Lifecycle Test

- Frame開始時に必要Stencil値へClearする。
- Pass間でState/Referenceが残留しない。
- State CacheがReferenceを誤ってKey化しない。
- Resize後の新DSVへ正しくBindingする。
- Device Lost後にStateとResourceを再作成する。

## 84. 完成確認表

- [ ] DSV ResourceとDepth Stencil Stateを区別できる。
- [ ] Depth TestとDepth Writeを別々に設定できる。
- [ ] 全Comparison Functionを説明できる。
- [ ] 通常ZとReversed-Zを一式で構成できる。
- [ ] Stencil三結果のOperationを説明できる。
- [ ] Read Mask、Write Mask、Referenceを区別できる。
- [ ] Front/Back Face別規則を設定できる。
- [ ] Outlineを二Passで構成できる。
- [ ] Depth SRVとWritable DSVの競合を回避できる。
- [ ] State Objectと動的ReferenceをCache上で分離できる。

## 85. この章の要点

- Depth Stencil Textureは値を保存し、Stateは比較・更新規則を定義します。
- Depth TestとDepth Writeは独立して制御します。
- 通常Zは1 Clear＋LESS系、Reversed-Zは0 Clear＋GREATER系が基本です。
- StencilはReference、保存値、Read Maskを比較し、Write Mask付きで更新します。
- Stencil Fail、Depth Fail、両方Passで別Operationを選べます。
- Front/Back Face別OperationはVolume Algorithmで重要です。
- Outline、Portal、Mirror、UI Mask、Light VolumeへStencilを応用できます。
- PassごとにState、Reference、DSV、Clear規約を明示します。

## 86. 公式資料

- [Configuring depth-stencil functionality](https://learn.microsoft.com/en-us/windows/win32/direct3d11/d3d10-graphics-programming-guide-depth-stencil)
- [D3D11_DEPTH_STENCIL_DESC](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/ns-d3d11-d3d11_depth_stencil_desc)
- [D3D11_DEPTH_STENCILOP_DESC](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/ns-d3d11-d3d11_depth_stencilop_desc)
- [D3D11_COMPARISON_FUNC](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/ne-d3d11-d3d11_comparison_func)
- [D3D11_STENCIL_OP](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/ne-d3d11-d3d11_stencil_op)
- [ID3D11Device::CreateDepthStencilState](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11device-createdepthstencilstate)
- [ID3D11DeviceContext::OMSetDepthStencilState](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11devicecontext-omsetdepthstencilstate)
- [Depth-stencil testing](https://learn.microsoft.com/en-us/windows/win32/direct3d11/d3d10-graphics-programming-guide-depth-stencil)

次章では、Vector、Matrix、Quaternion、座標系、変換順序をDirectXMathで正しく扱う方法を学びます。
