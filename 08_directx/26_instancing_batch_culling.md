# DirectX 11：Instancing・Batch・Culling

この章では、同じMeshを少ないDraw Callで多数描くInstancing、同じPipeline/MaterialをまとめるBatch、見えないObjectを描かないCullingを学びます。CPU Submission、Instance Buffer、LOD、Spatial Partition、Occlusion、並列Visible Listまでを扱います。

## 1. 最適化の三方向

```text
instancing : one drawで同一meshを複数配置
batching   : state変更とdraw submissionをまとめる
culling    : 見えないdrawを作らない
```

目的は似ていますが仕組みは別です。

## 2. Draw Call Cost

DrawごとにState確認、Resource Binding、Driver Command生成等のCPU Costがあります。Triangle数が少なくてもDraw数が多いとCPU Bottleneckになります。

## 3. GPU Costも残る

InstancingでDraw Callを減らしてもVertex/Pixel処理はInstance分必要です。見えないInstanceはCullingで減らします。

## 4. Instance Data

```cpp
struct InstanceData
{
    XMFLOAT4 worldRow0;
    XMFLOAT4 worldRow1;
    XMFLOAT4 worldRow2;
    XMFLOAT4 color;
};
```

必要なTransform表現とPer-instance属性だけを格納します。

## 5. 4×3 Transform

Affine Transformなら4×4すべてでなく3/4 Row等へ圧縮できます。HLSL側の復元とMatrix規約を一致させます。

## 6. Instance Vertex Buffer

Dynamic Vertex BufferへVisible Instance Dataを毎Frame書き、Input Slot 1等へBindingします。

## 7. Input Element

Instance Matrix各Rowを`INSTANCE_TRANSFORM0..2`等へ割り当て、`D3D11_INPUT_PER_INSTANCE_DATA`、Step Rate 1を指定します。

## 8. Per-vertex Stream

MeshのPosition/Normal/UVはSlot 0、全Instanceで共有します。

## 9. Per-instance Stream

World Transform、Color、Object ID等はSlot 1からInstanceごとに一つ進みます。

## 10. Binding

```cpp
ID3D11Buffer* buffers[] = {meshVb.Get(), instanceVb.Get()};
UINT strides[] = {sizeof(Vertex), sizeof(InstanceData)};
UINT offsets[] = {0, instanceByteOffset};
context->IASetVertexBuffers(0, 2, buffers, strides, offsets);
```

## 11. DrawIndexedInstanced

```cpp
context->DrawIndexedInstanced(
    submesh.indexCount,
    visibleInstanceCount,
    submesh.startIndex,
    submesh.baseVertex,
    startInstanceLocation);
```

## 12. SV_InstanceID

ShaderでInstance番号を取得できます。Structured Buffer等からDataをIndex参照する方式でも使います。

## 13. startInstanceLocation

大きなInstance Buffer内の開始Instanceを指定します。Buffer Byte Offsetとの単位を混同しません。

## 14. Dynamic Upload

Frame最初は`WRITE_DISCARD`、同Buffer未使用領域への追加は安全な範囲で`WRITE_NO_OVERWRITE`を使うRing方式を検討します。

## 15. Buffer容量

最大Instance数を決め、超過時は別Batchへ分割またはBuffer拡張します。Release中の無制限再確保を避けます。

## 16. Instance Data Alignment

Vertex Input FormatとC++ Field Offset/Strideを照合します。Constant Bufferの16 Byte規則とは別ですが、SIMD/帯域に適した配置を使います。

## 17. Instance Color

同一MaterialでもTint、Damage Flash、Dissolve量等をPer-instanceで変えられます。

## 18. Object ID

Picking、Outline、Visibility History用IDをInstance Dataへ入れられます。Floatへ無理に変換せず整数Input Formatを使います。

## 19. Texture差の問題

Instanceごとに別SRVを直接Bindingできない通常経路では、Texture Array/Atlas、Material別Batch、Index参照方式を使います。

## 20. Texture Array

同寸法・Format・Mip構成のTextureをSliceへまとめ、Instance Material Indexで選びます。

## 21. Atlas

異なる画像を一枚へ詰め、InstanceごとにUV Scale/Offsetを渡します。PaddingとMip Bleedingを処理します。

## 22. Instancingに向く物

草、岩、木、弾、Particle Mesh、群衆小物、同型Enemy、環境Props等です。

## 23. 向かない物

Mesh、Material、Shader、Skeletonが大きく異なる少数Objectは無理に一Batchへしません。

## 24. Skinned Instancing

Instanceごとに異なるBone Paletteが必要で複雑です。Animation Texture/Structured Buffer、同Pose共有、Crowd専用方式を検討します。

## 25. Batch Key

```text
render pass
pipeline/shader variant
mesh/submesh
material/resources
blend/depth/rasterizer
topology
```

同じKeyのDrawをまとめます。

## 26. Opaque Sorting

Pipeline/Material/Meshでまとめつつ、Front-to-backを組み合わせてState変更とOverdrawを減らします。

## 27. Transparent Sorting

透明物は基本的に奥から手前の順序が優先です。自由なMaterial Batch化は結果を壊します。

## 28. Static Batching

動かない複数MeshをWorld変換済みの大Bufferへ結合します。Draw削減と引き換えにCulling粒度・Memory・Build複雑性が変わります。

## 29. Dynamic Batching

CPUで毎Frame頂点を結合する方式はCopy/Transform Costが高くなります。Instancingとの比較を測定します。

## 30. Multi-material Mesh

SubmeshごとにMaterialが違えば通常別Drawです。Material数を減らすAsset設計もDraw数へ効きます。

## 31. Cullingの段階

```text
enabled/layer
distance
frustum
occlusion
LOD selection
batch building
```

安い判定から行います。

## 32. Bounding Sphere

回転しても形が変わらず判定が速いBoundsです。細長いObjectでは空間を多く含みます。

## 33. AABB

World軸に沿うBoxです。Local AABBをWorldへ変換する際、8 Cornerまたは中心＋絶対Matrix等で保守的Boundsを作ります。

## 34. OBB

Object方向へ沿うBoxで精度が上がりますが判定Costも増えます。

## 35. Frustum Culling

Camera FrustumとBoundsが完全に外なら除外します。Intersects/Contains/Disjointを使い分けます。

## 36. Conservative Culling

見える物を誤って消さないことが優先です。Boundsを少し大きくする、数値Epsilonを持つ等を行います。

## 37. Animated Bounds

Bind Pose BoundsだけではAnimation中の手足/武器が外れます。Clip BakeまたはBone Boundsで更新します。

## 38. Effect Bounds

Trail、Aura、Hit Effect等が親Mesh Bounds外へ出る場合、別Renderable/Boundsとして扱います。

## 39. Distance Culling

重要度と距離で小Objectを省きます。表示/非表示境界のちらつきをHysteresisやFadeで抑えます。

## 40. Screen-size Culling

距離だけでなくProjected Sizeで判断するとFOVやObject寸法へ適応します。

## 41. LOD選択

Projected Screen Size等でGeometry LODを選びます。境界へHysteresisを設けます。

## 42. LOD Cross-fade

Dither、Alpha、Morph等で切替を目立たなくします。二LOD同時描画期間のCostがあります。

## 43. Shadow LOD

Camera Color Passより粗いLODや更新頻度をShadowへ使えます。影形状のPopを確認します。

## 44. Occlusion Culling

Frustum内でも他Geometryに完全に隠れたObjectを除外します。

## 45. Occlusion Query

GPU QueryでProxyの可視Sample数を調べられます。結果取得LatencyとCPU Stallを避け、前Frame結果を使います。

## 46. Queryの問題

ObjectごとQueryを発行するとDraw/Query Costが増えます。大きなOccludeeやGroup単位に限定します。

## 47. Hierarchical Z

Depth PyramidとBoundsを比較するGPU Occlusion方式です。Compute Shader/UAVとIndirect Drawへ発展します。

## 48. Portal/Room Culling

室内SceneではRoomとPortalのConnectivityから見える領域を限定できます。

## 49. Spatial Partition

Grid、Quadtree、Octree、BVH等でCamera付近/Frustum候補だけを列挙します。

## 50. Uniform Grid

均一分布や動的Objectに扱いやすい方式です。巨大Objectが多数Cellへ重複する問題を処理します。

## 51. BVH

Bounds階層で大きな領域を一度に除外できます。Static Scene、Ray Query、Cullingに有効です。

## 52. Scene Graphとの違い

Transform親子階層はOwnership/座標用、Spatial Structureは位置検索/Culling用です。同じTreeに無理に統合しません。

## 53. Visible List

Culling結果を直接Drawせず、Renderable ID、LOD、Depth、Batch Keyを持つListへ出します。

## 54. 並列Culling

Object範囲をJobへ分割しThread-local Visible Listを作り、最後に結合/Sortします。共有Vectorへの細粒度Lockを避けます。

## 55. Determinism

並列結合順で透明物や同Key順が変わらないよう、Stable IDと明示Sort Keyを使います。

## 56. Render Proxy

Gameplay Objectそのものではなく、Render Thread向けSnapshotにTransform、Bounds、Mesh、Material、Flagsを持たせます。

## 57. Previous Transform

CullingされてもMotion Vector履歴を正しく保つため、前Frame Transform更新Policyを決めます。

## 58. Camera別Visibility

Main、Shadow、Reflection、MinimapでFrustum/Layer/Caster条件が違います。Visible ListをViewごとに作ります。

## 59. Shadow Culling

Camera外でもCamera内へShadowを落とすCasterが必要です。Main Camera Frustum結果を流用しません。

## 60. CPU/GPU Bottleneck計測

Draw数、Instance数、Visible/Total数、Triangles、State変更、Upload Byte、CPU Build時間、GPU時間を記録します。

## 61. Debug View

Bounds、Frustum、Spatial Cell、LOD色、Culled理由、Batch ID、Instance Countを可視化します。

## 62. よくある失敗：全Objectを一Batch

Material/Texture差、透明順、Cull粒度を壊します。Batch互換条件をKeyへ含めます。

## 63. よくある失敗：Instance Buffer Overflow

最大容量を超えてMemory破壊または描画欠落します。分割、拡張、統計、明確な上限処理を持ちます。

## 64. よくある失敗：Bounds更新忘れ

移動/Animation後も旧Boundsで判定し、画面端で消えます。Transform/Pose Versionに連動して更新します。

## 65. よくある失敗：Occlusion結果を同期Wait

同Frame結果をCPUで待ちPipeline Stallします。遅延結果と保守的Visibilityを使います。

## 66. Instancing Test

- 1/最大/超過Instanceを描く。
- Transform/Color/Object IDを確認する。
- startInstanceとBuffer Offsetを確認する。
- Dynamic RingのWrapを検証する。
- 単Drawとの画像比較を行う。

## 67. Culling Test

- Frustum各Plane境界へBoundsを置く。
- Camera内/外/交差を確認する。
- Animation/Effect Boundsを確認する。
- LOD Hysteresisを往復する。
- Main/Shadow View結果を分離する。

## 68. Performance Test

- Instancing前後のCPU/GPU時間を比較する。
- Draw/State変更数を数える。
- Visible率別にCulling Costを測る。
- Spatial Structureなし/ありを比較する。
- 透明SortとOpaque Batchを別計測する。

## 69. 完成確認表

- [ ] Instancing、Batching、Cullingを区別できる。
- [ ] Per-instance Input Layoutを作成できる。
- [ ] DrawIndexedInstancedの全引数を説明できる。
- [ ] Batch互換条件をKey化できる。
- [ ] Opaque/Transparent Sortを分けられる。
- [ ] Sphere/AABB/OBBを使い分けられる。
- [ ] Frustum、Distance、LODを保守的に判定できる。
- [ ] Occlusion QueryのLatencyを理解している。
- [ ] View別Visible Listを並列生成できる。
- [ ] Draw数・Upload・可視率を計測できる。

## 70. この章の要点

- Instancingは同一Mesh/Stateを一Drawで複数配置します。
- Batch KeyへPass、Pipeline、Mesh、Material、Stateを含めます。
- OpaqueはState/Front-to-back、透明はBack-to-frontを優先します。
- Cullingは安いLayer/DistanceからFrustum、Occlusionへ進めます。
- Boundsは移動・Animation・Effectを保守的に含めます。
- LODはScreen SizeとHysteresisで安定させます。
- Spatial Structureと並列Visible Listで多数Objectを処理します。
- 最適化はDraw数、CPU Submission、GPU時間を実測して判断します。

## 71. 公式資料

- [ID3D11DeviceContext::DrawIndexedInstanced](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11devicecontext-drawindexedinstanced)
- [D3D11_INPUT_ELEMENT_DESC](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/ns-d3d11-d3d11_input_element_desc)
- [ID3D11DeviceContext::IASetVertexBuffers](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11devicecontext-iasetvertexbuffers)
- [DirectXCollision](https://learn.microsoft.com/en-us/windows/win32/dxmath/directx-collision-detection)
- [D3D11 queries](https://learn.microsoft.com/en-us/windows/win32/direct3d11/overviews-direct3d-11-render-multi-thread-queries)

次章では、Compute Shader、Thread Group、Structured Buffer、UAV、Barrier相当のPass境界、GPU並列処理を扱います。
