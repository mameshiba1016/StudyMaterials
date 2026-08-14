# DirectX 11：Constant Buffer・Alignment・更新

この章では、CPUからShaderへ行列、色、時間、Camera、Material値を渡すConstant Bufferを学びます。16 Byte境界、HLSL Packing、C++ Layout、作成、更新、Binding、更新頻度別分割、部分Bindingを扱います。

## 1. Constant Bufferとは

DrawやFrame単位の小さな定数をShaderへ渡すBufferです。頂点配列を渡すVertex Bufferとは用途が異なります。

## 2. HLSL宣言

```hlsl
cbuffer PerObject : register(b0)
{
    float4x4 worldViewProjection;
    float4 baseColor;
};
```

`b0`はConstant Buffer Slot 0です。

## 3. C++対応型

```cpp
struct alignas(16) PerObjectConstants
{
    DirectX::XMFLOAT4X4 worldViewProjection;
    DirectX::XMFLOAT4 baseColor;
};
```

## 4. 16 Byte規則

Constant Bufferの`ByteWidth`は16の倍数でなければなりません。

```cpp
static_assert(sizeof(PerObjectConstants) % 16 == 0);
```

## 5. HLSL Packing Register

HLSL定数は概念上16 ByteのRegisterへPackingされます。VariableがRegister境界をまたぐ場合、次のRegisterから始まります。

## 6. float3の罠

`float3`が常にC++側12 Byteを隙間なく連続配置できるとは考えません。隣接FieldとRegister境界を確認し、明示Paddingや`float4`を使います。

## 7. ArrayのPacking

HLSLのScalar/Vector Array要素は通常それぞれ16 Byte Registerを消費します。C++の密な`float[]`とSizeが一致しない場合があります。

## 8. boolの罠

C++の`bool`とHLSLの`bool`を生Byte対応させません。32-bit整数を使い、0/1規約を定めます。

## 9. Matrix Layout

HLSL MatrixはPacking規約と`row_major`/`column_major`指定が関係します。CPU側の格納、転置、`mul`順を一つの規約へ統一します。

## 10. 転置例

```cpp
DirectX::XMStoreFloat4x4(
    &constants.worldViewProjection,
    DirectX::XMMatrixTranspose(world * view * projection));
```

これは採用規約の一例で、HLSL側`mul`順と合わせます。

## 11. Buffer作成

```cpp
D3D11_BUFFER_DESC desc{};
desc.ByteWidth = sizeof(PerObjectConstants);
desc.Usage = D3D11_USAGE_DYNAMIC;
desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

ThrowIfFailed(device->CreateBuffer(
    &desc, nullptr, constantBuffer.GetAddressOf()));
```

## 12. 最大Size

Direct3D 11の一つのConstant Bufferは4096個の16 Byte Constant、つまり64 KiBまでShaderから参照する基本制限があります。

## 13. Dynamic更新

```cpp
D3D11_MAPPED_SUBRESOURCE mapped{};
ThrowIfFailed(context->Map(
    constantBuffer.Get(), 0,
    D3D11_MAP_WRITE_DISCARD, 0, &mapped));
std::memcpy(mapped.pData, &constants, sizeof(constants));
context->Unmap(constantBuffer.Get(), 0);
```

## 14. WRITE_DISCARD

以前の内容を保持せず全体を書き直します。DriverがGPU使用中Memoryとは別領域を返せるため、頻繁な更新に向きます。

## 15. Default更新

`D3D11_USAGE_DEFAULT`と`UpdateSubresource`も使えます。更新頻度、競合、Copy Costを測定して選びます。

## 16. Immutable Constant

作成後に変化しない値ならImmutableも可能ですが、一般的なCamera/Object値は変化するためDynamicまたはDefaultが中心です。

## 17. VS Binding

```cpp
ID3D11Buffer* buffers[] = {constantBuffer.Get()};
context->VSSetConstantBuffers(0, 1, buffers);
```

HLSLの`register(b0)`とSlot 0を一致させます。

## 18. PS Binding

```cpp
context->PSSetConstantBuffers(0, 1, buffers);
```

StageごとにBinding Tableが別です。VSへ設定してもPSから自動では見えません。

## 19. 複数Stage共有

同じBufferをVSとPSへBindingできます。COM Objectは同じでも、各Stage Slotへの設定が必要です。

## 20. 更新とBinding

Buffer内容更新とSlot Bindingは別操作です。既にBinding済みなら更新後もObject Bindingは残りますが、明確なPass設計を優先します。

## 21. 更新頻度別分割

```text
PerFrame    : time, global lighting
PerView     : view/projection, camera
PerMaterial : color, roughness
PerObject   : world matrix, object ID
```

変化しないDataまで毎Draw Copyしない構造にします。

## 22. 巨大Buffer一つの問題

一Field変更だけで全体を更新し、Stageへ不要Dataを渡します。責任と頻度で適切に分割します。

## 23. 細分化しすぎの問題

小Bufferを大量に作成・BindingするとCPU Costと管理量が増えます。計測して粒度を決めます。

## 24. Object Buffer

World、WorldViewProjection、Object ID、Effect ParameterなどDrawごとの値を持ちます。

## 25. Camera Buffer

View、Projection、Camera Position、Near/FarなどCamera単位の値を持ちます。Resize時はProjectionを更新します。

## 26. Material Buffer

Base Color、Metallic、Roughness、Emission、Alpha CutoffなどMaterial単位の値を持ちます。

## 27. 時間値

累積時間を単精度Floatだけで長時間増やすと精度が落ちます。周期値、Frame内Delta、分割値など用途別に渡します。

## 28. Paddingを初期化する

```cpp
PerObjectConstants constants{};
```

Paddingを含めゼロ初期化し、未初期化ByteをGPUへ送らないようにします。

## 29. Reflection検証

Shader ReflectionのConstant Buffer Size、Variable Offset、SizeをC++の`sizeof`と`offsetof`へ照合します。

## 30. Slot規約

```text
b0 PerFrame
b1 PerView
b2 PerMaterial
b3 PerObject
```

全Shaderで共通規約を持つとBinding間違いを減らせます。

## 31. Slotの上書き

後から同じStage/Slotへ別Bufferを設定すると以前のBufferは置換されます。Pass切替で必要Slotを明示します。

## 32. Null解除

```cpp
ID3D11Buffer* nullBuffer = nullptr;
context->VSSetConstantBuffers(3, 1, &nullBuffer);
```

DebugやResource寿命境界で明示解除できます。

## 33. Context1の部分Binding

`ID3D11DeviceContext1::VSSetConstantBuffers1`等では、大きなBufferの一部を16 Byte Constant単位でBindingできます。

## 34. Ring Buffer

Frame内の多数Draw用定数を大きなDynamic Bufferへ順番に配置し、範囲Bindingする方式です。Alignment、Wrap、GPU使用中範囲を管理します。

## 35. Feature対応

Context1 InterfaceとConstant Buffer Offset機能をQueryし、非対応環境では個別Buffer方式へFallbackします。

## 36. Double/Triple buffering

Frameごとに更新領域を分け、GPUが前FrameのDataを読む間にCPUが次Frame分を書く設計です。

## 37. Dirty Flag

Material値が変わったときだけUploadするなど、CPU側VersionとGPU反映Versionを比較できます。

## 38. Debug Name

```cpp
SetDebugName(*constantBuffer.Get(), "PerObject ConstantBuffer");
```

頻度と用途を名前に含めます。

## 39. よくある失敗：16 Byte倍数でない

CreateBufferが失敗します。Compile時`static_assert`と共通Align関数で防ぎます。

## 40. よくある失敗：C++とHLSLのField順違い

Compileは成功しても色や行列が壊れます。ReflectionによるOffset検証を行います。

## 41. よくある失敗：Slot違い

HLSLは`b3`、C++はSlot 2へBindingしています。名前ではなく最終Bind Pointを照合します。

## 42. よくある失敗：Map後Unmap忘れ

GPUがResourceを使えません。Map成功Scopeを小さくし、失敗時経路も整理します。

## 43. よくある失敗：毎Draw Buffer作成

Resource作成をFrame Loopで繰り返します。Bufferを再利用し、内容だけ更新します。

## 44. テスト

- Sizeが16 Byte倍数である。
- Reflection Size/Offsetが一致する。
- VS/PS各Slotが一致する。
- 行列の移動・回転・透視を確認する。
- 数千Drawの更新Costを計測する。
- Hot Reload後もLayout契約を再検証する。

## 45. 完成確認表

- [ ] Constant Bufferの用途を説明できる。
- [ ] 16 Byte Packingを理解している。
- [ ] C++とHLSL Layoutを一致させられる。
- [ ] Dynamic Map/Unmapで更新できる。
- [ ] StageごとのSlotへBindingできる。
- [ ] 更新頻度でBufferを分けられる。
- [ ] Matrix規約を統一できる。
- [ ] ReflectionでSize/Offsetを検証できる。
- [ ] Ring BufferとFallbackを設計できる。
- [ ] Resourceを毎Draw作成しない。

## 46. この章の要点

- Constant BufferはCPUの小さな定数をShaderへ渡します。
- ByteWidthとHLSL Packingは16 Byte境界を基準にします。
- C++の`sizeof`だけでなくField Offsetも検証します。
- Dynamic更新はWRITE_DISCARDで全体を書き直すのが基本です。
- Binding TableはShader Stageごとに別です。
- Frame、View、Material、Objectの更新頻度で分けます。
- 大量Drawでは範囲BindingとRing Bufferを検討します。
- Reflectionを契約検査へ利用します。

## 47. 公式資料

- [Introduction to buffers](https://learn.microsoft.com/en-us/windows/win32/direct3d11/overviews-direct3d-11-resources-buffers-intro)
- [Packing rules for constant variables](https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/dx-graphics-hlsl-packing-rules)
- [ID3D11DeviceContext::VSSetConstantBuffers](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11devicecontext-vssetconstantbuffers)
- [ID3D11DeviceContext::PSSetConstantBuffers](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11devicecontext-pssetconstantbuffers)
- [ID3D11DeviceContext1::VSSetConstantBuffers1](https://learn.microsoft.com/en-us/windows/win32/api/d3d11_1/nf-d3d11_1-id3d11devicecontext1-vssetconstantbuffers1)

次章では、画像DataをTextureへ読み込み、Shader Resource ViewとしてPixel Shaderから参照する流れを扱います。
