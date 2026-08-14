# DirectX 11：Vertex Buffer・Input Layout

この章では、C++の頂点DataをGPU Resourceへ格納するVertex Bufferと、そのByte配置をVertex Shader入力へ接続するInput Layoutを学びます。構造体、Format、Offset、Stride、Usage、更新、複数Stream、Instancing、圧縮、検証を扱います。

## 1. 全体の接続

```text
C++ vertex array
-> ID3D11Buffer
-> IASetVertexBuffers
-> Input Layout decodes bytes
-> VSInput semantics
-> Vertex Shader
```

## 2. Vertex Bufferとは

頂点ごとの位置、Normal、UV、Color、Bone情報などを連続したGPU Resourceへ保存するBufferです。Buffer自身はFieldの意味を知りません。

## 3. Input Layoutとは

Vertex Buffer内のByte列を、どのFormat・Offset・Input Slotから読み、どのHLSL Semanticへ渡すか定義するDevice Objectです。

## 4. C++頂点構造体

```cpp
struct Vertex
{
    DirectX::XMFLOAT3 position;
    DirectX::XMFLOAT3 normal;
    DirectX::XMFLOAT2 uv;
};
```

一頂点は32 Byteです。ただし必ず`sizeof(Vertex)`と`offsetof`で確認します。

## 5. trivially copyable

GPUへ生Byte Copyする型にはVirtual関数、Pointer所有、`std::string`などを入れません。`std::is_trivially_copyable_v<Vertex>`を検査できます。

## 6. Position

Local Spaceの位置を保持します。`XMFLOAT3`なら通常3個の32-bit floatとしてInput Layoutへ対応させます。

## 7. Normal

Surface方向です。位置ではないため平行移動を適用せず、非一様ScaleではNormal Matrixを使います。

## 8. UV

Texture座標です。一般には2成分ですが、範囲は0から1に限定されずWrapやTilingにも使います。

## 9. Buffer Description

```cpp
D3D11_BUFFER_DESC desc{};
desc.ByteWidth = static_cast<UINT>(sizeof(Vertex) * vertices.size());
desc.Usage = D3D11_USAGE_IMMUTABLE;
desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
desc.CPUAccessFlags = 0;
desc.MiscFlags = 0;
desc.StructureByteStride = 0;
```

## 10. ByteWidth

Resource全体のByte数です。頂点数ではありません。乗算Overflowと`UINT`範囲を変換前に検証します。

## 11. 空配列

ByteWidth 0のBufferを作らず、空Meshとして扱います。`vertices.data()`が非Nullかだけでは要素有無を判定しません。

## 12. BindFlags

```cpp
desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
```

Input AssemblerのVertex Inputとして使うResourceであることを示します。

## 13. Immutable Usage

作成後に内容を変更しないStatic Meshに適します。初期Dataが必要で、CPU Access Flagは0です。

## 14. Default Usage

GPU中心の一般Resourceです。CPUからの変更は`UpdateSubresource`やCopy経路を使います。

## 15. Dynamic Usage

CPUが頻繁に書き換えるData用です。`D3D11_CPU_ACCESS_WRITE`を指定し、Map/Unmapで更新します。

## 16. Staging Usage

CPUとGPU間転送用で、PipelineへVertex Bufferとして直接Bindingしません。Bind Flagsは0です。

## 17. Usageの選び方

```text
never changes       -> IMMUTABLE
occasionally copied -> DEFAULT
frequent CPU write  -> DYNAMIC
readback/transfer   -> STAGING
```

名前の印象でなく更新頻度と方向で選びます。

## 18. Initial Data

```cpp
D3D11_SUBRESOURCE_DATA initial{};
initial.pSysMem = vertices.data();

ThrowIfFailed(device->CreateBuffer(
    &desc,
    &initial,
    vertexBuffer.GetAddressOf()));
```

Bufferでは`SysMemPitch`と`SysMemSlicePitch`は通常0です。

## 19. CreateBuffer

DeviceがResourceを作ります。成功後も元の`std::vector`を保持する必要はなく、Driverが必要Dataを取り込みます。

## 20. Immutableの制約

初期Dataなしで作成したり、後からMapして変更したりできません。編集可能Meshには別Usageを選びます。

## 21. Dynamic Buffer作成

```cpp
desc.Usage = D3D11_USAGE_DYNAMIC;
desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
```

最大必要容量を確保し、現在の使用頂点数を別途記録します。

## 22. Map Write Discard

```cpp
D3D11_MAPPED_SUBRESOURCE mapped{};
ThrowIfFailed(context->Map(
    vertexBuffer.Get(), 0,
    D3D11_MAP_WRITE_DISCARD, 0, &mapped));

std::memcpy(mapped.pData, vertices.data(), byteCount);
context->Unmap(vertexBuffer.Get(), 0);
```

古い内容を不要として新しいMemory領域を要求します。

## 23. Map中の規則

Map成功からUnmapまで、GPUへ同Resourceを使うCommandを出しません。取得PointerをUnmap後に保持しません。

## 24. Write No Overwrite

GPUがまだ読む範囲を上書きせず、未使用範囲へ追記する方式です。Ring BufferのOffset管理と同期保証が必要です。

## 25. UpdateSubresource

Default BufferへCPU MemoryからCopyできます。小規模・低頻度更新には簡潔ですが、競合時の内部Copy Costを計測します。

## 26. Input Element Description

```cpp
const D3D11_INPUT_ELEMENT_DESC elements[] =
{
    {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,
     0, offsetof(Vertex, position),
     D3D11_INPUT_PER_VERTEX_DATA, 0},

    {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT,
     0, offsetof(Vertex, normal),
     D3D11_INPUT_PER_VERTEX_DATA, 0},

    {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,
     0, offsetof(Vertex, uv),
     D3D11_INPUT_PER_VERTEX_DATA, 0}
};
```

## 27. SemanticName

HLSL入力の`POSITION`、`NORMAL`、`TEXCOORD`等と対応します。大文字小文字の扱いに頼らず表記を統一します。

## 28. SemanticIndex

`TEXCOORD0`、`TEXCOORD1`の末尾番号です。同じSemantic種別を複数渡すとき区別します。

## 29. Format

Memoryから何Byte読み、どの成分型としてShader値へ変換するか指定します。C++型のSizeと必ず一致させます。

## 30. Format対応例

```text
XMFLOAT2 -> R32G32_FLOAT
XMFLOAT3 -> R32G32B32_FLOAT
XMFLOAT4 -> R32G32B32A32_FLOAT
RGBA8 UNORM -> R8G8B8A8_UNORM
4 bone indices bytes -> R8G8B8A8_UINT
```

## 31. InputSlot

どのVertex Buffer Slotから読むかを指定します。Interleaved Buffer一つなら全要素Slot 0です。

## 32. AlignedByteOffset

一頂点内でFieldが始まるByte位置です。手書き数値より`offsetof(Vertex, field)`を使うと構造体変更へ追従できます。

## 33. APPEND_ALIGNED_ELEMENT

直前要素の直後へ自動配置する指定です。Paddingを含むC++構造体との一致が見えにくくなる場合があるため、`offsetof`が明確です。

## 34. InputSlotClass

- `D3D11_INPUT_PER_VERTEX_DATA`：頂点ごとに進む。
- `D3D11_INPUT_PER_INSTANCE_DATA`：Instanceごとに進む。

## 35. InstanceDataStepRate

Per-vertex要素では0です。Per-instance要素では、何InstanceごとにDataを一つ進めるか指定します。通常は1です。

## 36. CreateInputLayout

```cpp
ThrowIfFailed(device->CreateInputLayout(
    elements,
    static_cast<UINT>(std::size(elements)),
    vertexShaderBytecode->GetBufferPointer(),
    vertexShaderBytecode->GetBufferSize(),
    inputLayout.GetAddressOf()));
```

Vertex Shaderの入力Signatureと照合して作ります。

## 37. なぜVS Bytecodeが必要か

Input LayoutのSemantic、Index、型がVertex Shader入力Signatureと互換かRuntimeが検証するためです。

## 38. PS Bytecodeでは作れない

Input LayoutはInput AssemblerからVertex Shaderへの契約です。Pixel Shader Bytecodeを渡しません。

## 39. IASetInputLayout

```cpp
context->IASetInputLayout(inputLayout.Get());
```

以降のDrawが使用するContext Stateです。

## 40. IASetVertexBuffers

```cpp
ID3D11Buffer* buffers[] = {vertexBuffer.Get()};
UINT strides[] = {sizeof(Vertex)};
UINT offsets[] = {0};

context->IASetVertexBuffers(0, 1, buffers, strides, offsets);
```

## 41. Stride

次の頂点へ進むByte数です。Resource全体SizeでもField Sizeでもなく、通常`sizeof(Vertex)`です。

## 42. Offset

Buffer先頭から最初に読む頂点までのByte位置です。一つの大きなBuffer内へ複数Meshを詰める場合に使えます。

## 43. startVertexLocationとの違い

Buffer OffsetはByte単位、`Draw`のStart Vertexは頂点単位です。両方が最終Address計算へ影響します。

## 44. 複数Vertex Stream

```cpp
ID3D11Buffer* buffers[] = {positionBuffer.Get(), attributeBuffer.Get()};
UINT strides[] = {sizeof(PositionVertex), sizeof(AttributeVertex)};
UINT offsets[] = {0, 0};
context->IASetVertexBuffers(0, 2, buffers, strides, offsets);
```

Input ElementsのInput Slot 0/1を対応させます。

## 45. InterleavedとPlanar

Interleavedは一頂点の属性をまとめ、Planarは属性別Bufferに分けます。Cache、更新頻度、Passで使う属性、Asset形式を基に選びます。

## 46. Instancing

Mesh頂点をPer-vertex Stream、World Matrixや色をPer-instance Streamに分け、同じMeshを複数配置します。

## 47. Instance Matrix

4×4 Matrixは4個の`float4` Elementとして、同じSemantic名にIndex 0から3を付けます。各要素をPer-instance、Step Rate 1にします。

## 48. DrawInstanced

```cpp
context->DrawInstanced(
    vertexCountPerInstance,
    instanceCount,
    startVertex,
    startInstance);
```

Instance Buffer Bindingと対応するShader入力が必要です。

## 49. 頂点圧縮

NormalやTangentを16 bit/10 bit形式、Colorを8-bit UNORMへ圧縮するとMemory帯域を減らせます。精度、変換Cost、Asset Pipelineを測定します。

## 50. Bone Data

Bone Indexは整数Format、WeightはFloatまたはUNORM Formatを使います。Shader入力型が`uint`か`float`かをFormat解釈と合わせます。

## 51. Input Layout Cache

LayoutはVertex Shader入力SignatureとVertex Formatの組でCacheできます。HLSL File名だけでなくCompile済みSignatureをKeyへ含めます。

## 52. Vertex Format ID

```text
Position
PositionUv
StaticMesh
SkinnedMesh
Particle
DebugLine
```

用途別Formatを有限個へ整理するとLayoutとShader Variantを管理しやすくなります。

## 53. Buffer Viewは不要

Vertex BufferはTextureのRTV/SRVのようなView Objectを作らず、Buffer本体とStride/OffsetをIAへ直接Bindingします。

## 54. Buffer寿命

ContextへBindingすると参照が保持されます。Resourceを破棄・再利用する境界ではBindingとGPU使用状況を考慮します。

## 55. Debug Name

```cpp
SetDebugName(*vertexBuffer.Get(), "Player Body Vertex Buffer");
SetDebugName(*inputLayout.Get(), "Skinned Vertex Input Layout");
```

Mesh名、LOD、Stream用途を識別できる名前にします。

## 56. よくある失敗：ByteWidthに頂点数だけ指定

必要Byte数より小さいBufferになります。`sizeof(Vertex) * count`をOverflow検証付きで計算します。

## 57. よくある失敗：StrideがPosition Size

Interleaved構造体なのに12 Byteを指定し、次頂点ではなくNormal位置へ進みます。Strideは構造体全体Sizeです。

## 58. よくある失敗：FormatとField不一致

`XMFLOAT3`へ`R32G32_FLOAT`を指定するなど、読み取る成分数が違います。`sizeof`とReflectionで検査します。

## 59. よくある失敗：Semantic Index忘れ

複数UVがすべて`TEXCOORD0`になり、Shader契約と一致しません。名前とIndexを一組で扱います。

## 60. よくある失敗：Dynamic BufferへNO_OVERWRITE乱用

GPUが使用中の範囲を上書きし描画が壊れます。Ring Allocation範囲とFrame同期を証明できないならDiscardから始めます。

## 61. 作成テスト

- 空MeshをResource作成へ渡さない。
- ByteWidthが期待値と一致する。
- 全Field Offsetが`offsetof`と一致する。
- Input Layout作成に正しいVS Bytecodeを使う。
- Debug Layer Warningがない。

## 62. 描画テスト

- Positionだけの三角形を描く。
- Color、UV、Normalを可視化する。
- 複数Mesh Offsetを確認する。
- Dynamic更新を連続Frameで試す。
- InstanceごとのMatrixとColorを確認する。

## 63. 完成確認表

- [ ] Vertex BufferとInput Layoutの責任を区別できる。
- [ ] ByteWidth、Stride、Offsetを説明できる。
- [ ] UsageとCPU Accessの組合せを選べる。
- [ ] `D3D11_SUBRESOURCE_DATA`で初期化できる。
- [ ] Input Element全Fieldを説明できる。
- [ ] C++ FieldとDXGI Formatを一致させられる。
- [ ] VS BytecodeがLayout作成に必要な理由を説明できる。
- [ ] Dynamic BufferをMap/Unmapできる。
- [ ] 複数StreamとInstance StreamをBindingできる。
- [ ] 頂点圧縮を精度と帯域で評価できる。

## 64. この章の要点

- Vertex BufferはByte Data、Input Layoutはその解釈です。
- ByteWidthは全体Byte数、Strideは一頂点の間隔、Offsetは開始Byte位置です。
- Input LayoutはVertex Shader入力Signatureと照合して作成します。
- Semantic名・Index・Format・Slot・Offsetをすべて一致させます。
- Static、Default、Dynamicを更新方針で使い分けます。
- InstancingではPer-vertexとPer-instance Streamを分離します。
- 圧縮はMemory帯域を減らしますが、精度と変換Costを測定します。
- Debug Layerと属性可視化でLayout不一致を発見します。

## 65. 公式資料

- [Introduction to buffers in Direct3D 11](https://learn.microsoft.com/en-us/windows/win32/direct3d11/overviews-direct3d-11-resources-buffers-intro)
- [ID3D11Device::CreateBuffer](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11device-createbuffer)
- [D3D11_BUFFER_DESC](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/ns-d3d11-d3d11_buffer_desc)
- [D3D11_INPUT_ELEMENT_DESC](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/ns-d3d11-d3d11_input_element_desc)
- [ID3D11Device::CreateInputLayout](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11device-createinputlayout)
- [ID3D11DeviceContext::IASetVertexBuffers](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11devicecontext-iasetvertexbuffers)
- [ID3D11DeviceContext::Map](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11devicecontext-map)
- [ID3D11DeviceContext::DrawInstanced](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11devicecontext-drawinstanced)

次章では、頂点を再利用してMeshを構成するIndex Bufferと、点・線・三角形の組み立て方を決めるPrimitive Topologyを扱います。
