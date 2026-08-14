# DirectX 11：Index Buffer・Primitive Topology

この章では、Vertex Buffer内の頂点を番号で参照するIndex Bufferと、頂点列を点・線・三角形へ組み立てるPrimitive Topologyを学びます。Format、Offset、`DrawIndexed`、Winding、Strip、Restart、Mesh分割、Vertex Cache最適化まで扱います。

## 1. Index Bufferとは

Vertex Bufferの要素番号を並べたBufferです。同じ頂点を複数Primitiveから参照し、Data重複を減らします。

## 2. 四角形の例

```text
vertices: 0 1 2 3
triangle 1: 0 1 2
triangle 2: 2 1 3
indices: 0 1 2 2 1 3
```

共有辺の頂点を再利用します。

## 3. Indexなしとの比較

Indexなしなら2三角形に6頂点、Indexありなら4頂点＋6 Indexです。属性が大きいほど再利用の効果が増えます。

## 4. 共有できない頂点

同じ位置でもUV、Normal、Tangent、Colorなど一つでも異なるCornerは別頂点です。位置だけを基準に統合しません。

## 5. 16-bit Index

```cpp
using Index = std::uint16_t;
```

一Drawで参照できる頂点範囲は0から65535です。Memoryと帯域を抑えられます。

## 6. 32-bit Index

```cpp
using Index = std::uint32_t;
```

大きなMeshを扱えますが、Index Data量は16-bitの2倍です。

## 7. Format対応

```text
uint16_t -> DXGI_FORMAT_R16_UINT
uint32_t -> DXGI_FORMAT_R32_UINT
```

Signed Formatや8-bit FormatをIndex Bufferへ指定しません。

## 8. Format選択

最大Index値が65535以下なら16-bitを検討します。頂点数だけでなく実際の最大IndexとMesh分割方針を確認します。

## 9. Index Buffer Descriptor

```cpp
D3D11_BUFFER_DESC desc{};
desc.ByteWidth = static_cast<UINT>(sizeof(Index) * indices.size());
desc.Usage = D3D11_USAGE_IMMUTABLE;
desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
```

## 10. 初期Data

```cpp
D3D11_SUBRESOURCE_DATA initial{};
initial.pSysMem = indices.data();

ThrowIfFailed(device->CreateBuffer(
    &desc, &initial, indexBuffer.GetAddressOf()));
```

空配列とByte数Overflowを事前に拒否します。

## 11. Usage

Static MeshはImmutableが基本です。頻繁にTopologyが変化するProcedural MeshではDefault更新やDynamicを検討します。

## 12. IASetIndexBuffer

```cpp
context->IASetIndexBuffer(
    indexBuffer.Get(),
    DXGI_FORMAT_R16_UINT,
    0);
```

Buffer、Index Format、先頭Byte Offsetを渡します。

## 13. Byte Offset

第3引数はIndex個数でなくByte単位です。一つのBufferへ複数Index領域を詰める場合に使います。

## 14. Primitive Topology

Input Assemblerが順番に読んだ頂点を、どのPrimitiveとして解釈するか決めるStateです。

## 15. Point List

```cpp
context->IASetPrimitiveTopology(
    D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
```

各頂点が独立した点になります。

## 16. Line List

2頂点ごとに独立した線分を作ります。Debug Line、Grid、Collision可視化に適します。

## 17. Line Strip

最初の2頂点で線を作り、その後は直前頂点と新頂点を接続します。Polylineを少ないIndexで表現できます。

## 18. Triangle List

3頂点ごとに独立した三角形を作ります。一般的な3D Meshの基本Topologyです。

## 19. Triangle Strip

最初の3頂点で三角形を作り、その後は1頂点追加ごとに三角形を作ります。順序と表裏が交互に変わる規則へ注意します。

## 20. Patch List

Hull/Domain ShaderによるTessellationで使います。Control Point数ごとのPatch Topologyを選びます。

## 21. Undefined Topology

`UNDEFINED`のままDrawしません。PassまたはPipeline State設定時に必ず明示します。

## 22. DrawIndexed

```cpp
context->DrawIndexed(indexCount, startIndexLocation, baseVertexLocation);
```

Index数、開始Index、Vertex番号へ加えるBase値を指定します。

## 23. indexCount

Triangle Listなら通常3の倍数です。Buffer全体Sizeではなく、このDrawで読むIndex個数です。

## 24. startIndexLocation

Binding時Byte Offsetを基準に、何個目のIndexから読むか指定します。単位はIndexです。

## 25. baseVertexLocation

読み取ったIndex値へ加算されるSigned値です。共有Vertex Buffer内のMesh開始位置を指定できます。

## 26. Addressの考え方

```text
index address = index buffer byte offset
              + (startIndex + local index) * index element size

vertex number = decoded index + baseVertex
vertex address = vertex buffer offset + vertex number * stride
```

## 27. Negative Base Vertex

API上はSigned値です。加算結果が有効頂点範囲外にならないことを保証します。

## 28. DrawIndexedInstanced

```cpp
context->DrawIndexedInstanced(
    indexCountPerInstance,
    instanceCount,
    startIndex,
    baseVertex,
    startInstance);
```

Index再利用とInstance再利用を組み合わせます。

## 29. Winding Order

Screen上の頂点順がClockwiseかCounter-clockwiseかでFront Faceを判定します。Rasterizer Stateの`FrontCounterClockwise`と一致させます。

## 30. Cullとの関係

Index順を逆にすると三角形の表裏が反転し、Back-face Cullingで消える場合があります。表示されないときはWindingを確認します。

## 31. 座標系変換

Model ImportでHandednessやAxisを変換するとWindingが反転することがあります。PositionだけでなくIndex順、Normal、Tangentも一貫して変換します。

## 32. Degenerate Triangle

同じ頂点を含むなど面積0の三角形です。通常表示されませんが、Strip接続や不正Meshで発生します。

## 33. Primitive Restart

Strip中の特殊Index値で一つのStripを終了し、次を開始する仕組みです。16-bitと32-bitでCut値が異なります。

## 34. Cut値

```text
R16_UINT -> 0xFFFF
R32_UINT -> 0xFFFFFFFF
```

Cut値を通常頂点Indexとして使えなくなる点を考慮します。

## 35. StripかListか

現代のMeshではTriangle Listが単純でToolとの相性も良好です。StripのIndex削減効果だけで選ばず、CacheとMesh処理全体を測定します。

## 36. Post-transform Vertex Cache

GPUは最近処理した頂点結果を再利用できます。近いIndex位置で同じ頂点を再参照する順序に最適化するとVertex Shader実行を減らせます。

## 37. Index順最適化

三角形の見た目を変えず、Vertex Cache localityを高めるようTriangle順を並べ替えます。Asset Build工程で行います。

## 38. Overdraw最適化

手前を先に描く順序はPixel処理を減らす可能性がありますが、Vertex Cache順と競合します。Scene、Depth Prepass、GPU特性を含め測定します。

## 39. Vertex Fetch最適化

最適化後のIndex初出順にVertex Dataを並べ替え、Memory localityを改善します。Indexも新Vertex番号へRemapします。

## 40. Mesh分割

Material、LOD、16-bit上限、Culling単位、Skinning Palette、Streaming単位を基準にSubmeshへ分けます。

## 41. Submesh情報

```cpp
struct Submesh
{
    UINT indexCount = 0;
    UINT startIndex = 0;
    INT baseVertex = 0;
    UINT materialIndex = 0;
};
```

`DrawIndexed`引数とMaterialをまとめます。

## 42. Bounds

SubmeshごとにBounding Box/Sphereを持つと、見えない部分のDrawを省けます。Index範囲から参照する頂点BoundsをBuild時に計算します。

## 43. LOD

距離や画面占有率に応じて異なるIndex/Vertex Rangeを選びます。切替ちらつきをHysteresisやCross-fadeで抑えます。

## 44. Mesh Validation

すべてのIndexがVertex Count未満か検証します。不正IndexはGPU Memory範囲外参照につながる重大なAsset Errorです。

## 45. Triangle List検証

Index Countが3の倍数か、Degenerate率、重複Triangle、Winding一貫性をBuild Toolで調べます。

## 46. 16-bit変換検証

32-bit値を単純Castせず、最大値が`uint16_t`範囲に入ることを確認してから変換します。

## 47. Dynamic Index Buffer

地形編集やProcedural Geometryで使えますが、毎FrameTopologyを変える必要性、最大容量、Discard/No-overwrite範囲を設計します。

## 48. Debug描画

Line List Indexを生成してSkeleton、Collider、Navigation、Attack判定を可視化できます。本番描画と別Buffer/Passに分けます。

## 49. Debug Name

```cpp
SetDebugName(*indexBuffer.Get(), "Enemy LOD0 IndexBuffer R16");
```

Mesh、LOD、Formatを識別できる名前にします。

## 50. よくある失敗：Format不一致

Memoryは`uint32_t`なのに`R16_UINT`としてBindingし、Byte列が別のIndex列として解釈されます。

## 51. よくある失敗：Offset単位の混同

IA Binding OffsetはByte、Start Indexは要素、Base Vertexは頂点番号です。変数名へ単位を含めます。

## 52. よくある失敗：Index範囲外

Asset破損やSubmesh計算誤りで存在しない頂点を参照します。Load時に全Indexを検証します。

## 53. よくある失敗：Topology設定忘れ

前PassのLine Listが残り、三角形が線として解釈されます。Graphics Pipeline設定をPass単位で明示します。

## 54. よくある失敗：裏面が消える

WindingとRasterizer Front Face規約が逆です。Cullを無効にして症状を確認し、根本のIndex順を修正します。

## 55. 作成テスト

- 空Indexを拒否する。
- ByteWidthとFormat Sizeが一致する。
- 最大IndexがVertex Count未満である。
- R16への縮小変換でOverflowしない。
- Debug Layer Warningがない。

## 56. 描画テスト

- Triangle Listの四角形を描く。
- Winding反転でCull結果が変わる。
- Start IndexとBase Vertexを個別に試す。
- 複数Submeshを一つのBufferから描く。
- Instanced Indexed Drawを確認する。

## 57. 完成確認表

- [ ] Index Bufferによる頂点再利用を説明できる。
- [ ] R16/R32を安全に選べる。
- [ ] Binding OffsetとDraw引数の単位を説明できる。
- [ ] Point、Line、Triangle Topologyを使い分けられる。
- [ ] WindingとCullingの関係を説明できる。
- [ ] Primitive Restartを理解している。
- [ ] Submeshを`DrawIndexed`引数へ変換できる。
- [ ] 全IndexをLoad時に検証できる。
- [ ] Vertex CacheとOverdraw最適化を区別できる。
- [ ] LOD、Culling、Material単位でMeshを分割できる。

## 58. この章の要点

- Index BufferはVertex番号を保存し、頂点Dataを再利用します。
- 16-bitは省Memory、32-bitは広いIndex範囲を持ちます。
- Topologyは頂点列をPrimitiveへ組み立てるContext Stateです。
- `DrawIndexed`はIndex Count、Start Index、Base Vertexを使います。
- WindingはFront FaceとCulling結果を決めます。
- Asset Load時にIndex範囲とTopology条件を検証します。
- Index順はVertex Cache、Overdraw、Vertex Fetchへ影響します。
- SubmeshはMaterial、LOD、Culling、Skinning単位を支えます。

## 59. 公式資料

- [ID3D11DeviceContext::IASetIndexBuffer](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11devicecontext-iasetindexbuffer)
- [ID3D11DeviceContext::IASetPrimitiveTopology](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11devicecontext-iasetprimitivetopology)
- [D3D11_PRIMITIVE_TOPOLOGY](https://learn.microsoft.com/en-us/windows/win32/api/d3dcommon/ne-d3dcommon-d3d_primitive_topology)
- [ID3D11DeviceContext::DrawIndexed](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11devicecontext-drawindexed)
- [ID3D11DeviceContext::DrawIndexedInstanced](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11devicecontext-drawindexedinstanced)
- [Primitive topologies](https://learn.microsoft.com/en-us/windows/win32/direct3d11/d3d10-graphics-programming-guide-primitive-topologies)

次章では、CPUからShaderへ行列・色・時間などを渡すConstant Bufferと、16 Byte境界、更新方式、Slot設計を扱います。
