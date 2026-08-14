# DirectX 12 第13章：Model・Material・Animation

この章では、3D Model AssetをDirectX 12で描画可能なRuntime Dataへ変換し、Material、Skeleton、Animationと統合する方法を学びます。Import/Cook、Mesh Buffer、PBR、Skinning、Animation Blend、Root Motion、LOD、Culling、非同期Upload、Character描画の性能設計まで扱います。

## 1. 到達目標

- Model FileとRuntime Resourceを区別する。
- Vertex/Index/Submesh/Materialの関係を説明する。
- Skeleton PoseからSkinning Matrixを作る。
- Animationの補間、Blend、State遷移を実装できる。
- D3D12のBuffer、Descriptor、Fenceへ安全に接続する。
- 多人数の高速戦闘Sceneを計測して最適化する。

## 2. Model Assetの中身

```text
Scene/Node hierarchy
Mesh/Primitive/Submesh
Vertex attributes/Indices
Material/Texture
Skin/Skeleton/Joint weights
Animation clips/Events
Bounds/LOD/Metadata
```

一つのModelが一つのMeshやDraw Callとは限りません。

## 3. Source形式とRuntime形式

FBX、glTF等は交換/編集に便利ですが、Runtimeに最適とは限りません。Build工程で検証、座標変換、圧縮、並べ替えを行い、Version付きRuntime形式へCookします。

## 4. Import Pipeline

```text
Source read
 -> validation
 -> coordinate/unit conversion
 -> triangulation
 -> vertex/index conversion
 -> material/texture resolution
 -> skeleton/animation conversion
 -> optimization/compression
 -> runtime package
```

どの段階で値が変わったか追跡できるLogを残します。

## 5. glTFの位置付け

glTF 2.0はMesh、PBR Material、Skin、Animation等を表現できます。JSON/GLB構造を理解しつつ、RuntimeではEngine専用形式へ変換する設計も有効です。

## 6. FBXの位置付け

DCC Toolとの交換で広く使われますが、SDK、Version、Exporter設定差を管理します。Source Fileを直接GPUへ送れるわけではありません。

## 7. Asset Version

Runtime HeaderへMagic、Schema Version、Endianness、Section Offset/Size、Dependency ID、Content Hashを持たせ、不一致を明示的に拒否します。

## 8. Coordinate System正規化

Up Axis、Forward Axis、左手/右手、Winding、UV原点をImport時に統一します。PositionだけでなくNormal、Tangent、Animation、Inverse Bindも同じ変換規約に従わせます。

## 9. Unit Scale

cm/m等を統一し、Physics、Camera、Animation移動量、Audio距離と一致させます。Node Scaleへ場当たり的に残さない方が安全です。

## 10. Transform Hierarchy

```text
local transform  : parentから見た変換
global transform : model rootから見た変換
world transform  : scene内の変換
```

通常`global = local * parentGlobal`か逆順かは行列規約によるため、Project全体で固定します。

## 11. TRS

Translation、Rotation、Scaleを保持し、QuaternionでRotationを補間します。非一様Scaleと回転の合成ではShearや分解精度に注意します。

## 12. Triangulation

GPUへTriangleとして渡すためPolygonを三角形化します。DCC/Importer間で結果が変わるとTangentやAnimation後の見た目も変わり得ます。

## 13. Vertex Attribute

```text
POSITION
NORMAL
TANGENT
TEXCOORD
COLOR
JOINTS/BLENDINDICES
WEIGHTS/BLENDWEIGHT
```

Material/Shader Variantが必要とするAttributeだけをRuntime Layoutへ含めます。

## 14. InterleavedとPlanar

全Attributeを一Vertexへまとめる方式と、Position等を別Streamにする方式があります。Passごとの読取り量、Cache、更新頻度から決めます。

## 15. Vertex構造体例

```cpp
struct SkinnedVertex
{
    DirectX::XMFLOAT3 position;
    DirectX::XMFLOAT3 normal;
    DirectX::XMFLOAT4 tangent;
    DirectX::XMFLOAT2 uv;
    std::array<std::uint16_t, 4> joints;
    std::array<std::uint16_t, 4> weights;
};
```

CPU Layout、Input Layout、HLSL型、Alignment、正規化Formatを一致させます。

## 16. Packed Format

Normal/Tangent/Weight等は適切なUNORM/SNORMやHalfへ圧縮できます。Memory/Bandwidth削減と変形/Lighting誤差を比較します。

## 17. Vertex Weight正規化

有効Jointだけを残し、Weight合計を1へ正規化します。量子化後の合計ずれを補正し、全Weight 0を検出します。

## 18. Influence数

1 Vertexあたり4または8 Joint等へ上限を設けます。小さいWeightを落とした後に再正規化し、品質差をAsset Testします。

## 19. Index Buffer

Vertex数に応じ16-bit/32-bitを選びます。16-bitは容量/Bandwidthを減らせますがIndex範囲を超えられません。

## 20. Submesh

Index範囲、Base Vertex、Material ID、Bounds等を保持するDraw単位です。一つのMesh内にMaterialごとのSubmeshを持てます。

## 21. Primitive Topology

通常ModelはTriangle Listです。PSOのTopology Typeと`IASetPrimitiveTopology`を一致させます。

## 22. Vertex Buffer View

```cpp
D3D12_VERTEX_BUFFER_VIEW view{};
view.BufferLocation = vertexBuffer->GetGPUVirtualAddress();
view.SizeInBytes = vertexBytes;
view.StrideInBytes = sizeof(SkinnedVertex);
```

Resource Lifetime中だけGPU Addressが有効です。

## 23. Index Buffer View

```cpp
D3D12_INDEX_BUFFER_VIEW view{};
view.BufferLocation = indexBuffer->GetGPUVirtualAddress();
view.SizeInBytes = indexBytes;
view.Format = DXGI_FORMAT_R32_UINT;
```

Formatと実Data Widthを一致させます。

## 24. Default HeapへUpload

静的MeshはDefault Heap Resourceへ置き、Upload BufferからCopyします。Copy完了後にVertex/Index用途へTransitionします。

## 25. Upload Lifetime

Upload Resourceを参照するCopy CommandのFence完了前に破棄/再利用しません。Asset LoaderがOwnershipを追跡します。

## 26. Resource State

```text
copy destination -> COPY_DEST
vertex data      -> VERTEX_AND_CONSTANT_BUFFER
index data       -> INDEX_BUFFER
```

Queue間同期とState Trackingを前章までの規則へ接続します。

## 27. Mesh Resource構造

```cpp
struct MeshGpuResource
{
    Microsoft::WRL::ComPtr<ID3D12Resource> vertices;
    Microsoft::WRL::ComPtr<ID3D12Resource> indices;
    D3D12_VERTEX_BUFFER_VIEW vertexView{};
    D3D12_INDEX_BUFFER_VIEW indexView{};
    std::vector<Submesh> submeshes;
};
```

CPU MetadataとGPU Resourceの寿命を明確に分けます。

## 28. DrawIndexedInstanced

```cpp
commandList->IASetVertexBuffers(0, 1, &mesh.vertexView);
commandList->IASetIndexBuffer(&mesh.indexView);
commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
commandList->DrawIndexedInstanced(
    submesh.indexCount,
    instanceCount,
    submesh.firstIndex,
    submesh.baseVertex,
    firstInstance);
```

各引数がRuntime Dataのどこを指すか説明できるようにします。

## 29. Bounds

AABB、Sphere、必要ならOBBを保持します。Import時Bounds、Animation後Bounds、World変換後Boundsを混同しません。

## 30. Materialの役割

Shader/PSO分類、Texture参照、数値Parameter、Alpha/Cull/Depth規約をひとまとめにし、Surfaceの描画方法を定義します。

## 31. PBR Metallic-Roughness

```text
base color
metallic
roughness
normal
occlusion
emissive
alpha mode/cutoff
```

値のColor SpaceとTexture Channel Packingを仕様化します。

## 32. SRGBとLinear

Base Color/Emissive等の色TextureはSRGB View、Normal/Metallic/Roughness/Occlusion等のData TextureはLinearとして扱うのが基本です。

## 33. Normal Map

Tangent Space規約、Y方向、TangentのHandednessをImporterとShaderで揃えます。非一様ScaleではNormal変換も正しく行います。

## 34. Material定数

```cpp
struct alignas(256) MaterialConstants
{
    DirectX::XMFLOAT4 baseColorFactor;
    DirectX::XMFLOAT3 emissiveFactor;
    float metallicFactor;
    float roughnessFactor;
    float alphaCutoff;
    std::uint32_t flags;
};
```

CBV Offset/Sizeは256-byte Alignmentを守ります。C++/HLSL Packingも検証します。

## 35. Texture Descriptor

Materialが使うSRVをDescriptor Heapへ配置します。欠損Texture用にWhite、Black、Flat Normal等のFallback Descriptorを用意します。

## 36. Descriptor Index方式

TextureごとのIndexをMaterial Dataへ持ち、大きいDescriptor Tableから選ぶ設計があります。Hardware/Shader Model要件とLifetimeを管理します。

## 37. Material Instance

共有Material定義に対し、Character Color等の一部Parameterだけを上書きします。Texture/PSO全複製を避けます。

## 38. Alpha Mode

```text
OPAQUE : Blend off, depth write on
MASK   : cutoff判定, depth write on
BLEND  : blend on, depth write通常off
```

前章のPSO Variantと描画QueueへMappingします。

## 39. Double-sided Material

Cull Noneだけでなく、裏面Normal/Lightingをどう扱うか決めます。全Materialを両面にすると無駄なPixel処理が増えます。

## 40. Material Sort Key

OpaqueはPSO/Materialを優先してBatchし、TransparentはDepth順を優先します。Texture Descriptor方式に応じ切替Costが変わります。

## 41. Skeleton

Jointの親子関係、Bind Pose、Inverse Bind Matrix、名前/IDを保持します。Node Hierarchy全体とSkin Joint集合は同一とは限りません。

## 42. Joint Index

String検索を毎Frame行わず、Import時に安定した整数Indexへ変換します。Invalid ParentとCycleを検証します。

## 43. Bind Pose

MeshがSkeletonへ結び付けられた基準Poseです。Inverse Bind Matrixは基準PoseのVertexをJoint Spaceへ移すために使います。

## 44. Local Pose

各JointのParent基準TRSです。Animation ClipからSamplingした値をここへ書きます。

## 45. Global Pose

Parent順にLocal変換を連結します。Joint Arrayを親が先に来る順へ並べれば線形Loopで評価できます。

## 46. Skinning Matrix

概念的には次の組合せです。

```text
skinMatrix[j] = inverseBind[j] * animatedGlobal[j]
```

行列の掛け順、Transpose、Model Root変換はProjectのConventionに合わせます。

## 47. Linear Blend Skinning

```hlsl
float4 p = 0;
[unroll]
for (uint i = 0; i < 4; ++i)
{
    p += mul(float4(input.position, 1), joints[input.joints[i]])
       * input.weights[i];
}
```

Normal/Tangentは方向として変換し、必要に応じ再正規化します。

## 48. LBSの限界

捻りで体積が潰れるCandy-wrapper Artifact等があります。Art側のWeight調整、補助Bone、Dual Quaternion等を要件に応じ検討します。

## 49. Joint Matrixの転送

小規模ならConstant Buffer、大規模/多数CharacterならStructured Buffer等を検討します。Root Signature、Alignment、Frame更新量を比較します。

## 50. Frame Resource

GPUが前FrameのJoint Dataを読んでいる間にCPUが上書きしないよう、Frame数分の領域またはRing Allocationを使います。

## 51. GPU Skinning出力

Vertex Shaderで直接変形する方式と、ComputeでSkinned Vertex Bufferを生成して複数Passから再利用する方式があります。

## 52. Vertex Shader Skinning

実装が単純で追加Buffer不要ですが、Depth/Shadow/Main等の各Passで同じ変形を再計算する場合があります。

## 53. Compute Skinning

一度計算した結果を複数Passで再利用できますが、出力Memory、UAV Barrier、Queue同期、Dispatch Costが増えます。

## 54. Skinning方式の選択

Character数、Vertex数、Pass数、Animation更新率、Memory Bandwidthを実測して選びます。常にComputeが速いとは限りません。

## 55. Morph Target

Position/Normal/Tangent差分へWeightを掛けて表情や変形を作ります。Skinningとの適用順序をAsset PipelineとShaderで統一します。

## 56. Animation Clip

Duration、Sample Rate、Joint Track、Morph Track、Event、Root Motion等を持つ時間範囲です。

## 57. Track

JointごとのTranslation/Rotation/Scale Key列です。Trackが欠損するChannelはReference Pose等の既定値を使います。

## 58. Time更新

```cpp
time += deltaTime * playbackRate;
if (looping)
    time = std::fmod(time, duration);
else
    time = std::min(time, duration);
```

負の時間、duration 0、大きなDelta、逆再生を仕様化します。

## 59. Key検索

前回Index Cache、Binary Search、均一Samplingなら直接Index計算を使えます。Clip圧縮形式に合う方法を選びます。

## 60. Translation/Scale補間

通常はLinear Interpolationを使います。Cubic Curveを使う形式ではTangent Dataと補間規則を保持します。

## 61. Quaternion補間

短い回転経路を選ぶよう符号を扱い、Nlerp/Slerpを用途で選びます。補間後は正規化します。

## 62. Pose Blend

```text
T = lerp(Ta, Tb, weight)
R = shortest-path nlerp/slerp(Ra, Rb, weight)
S = lerp(Sa, Sb, weight)
```

行列同士の単純な要素補間は回転/Scaleを壊し得ます。

## 63. Cross Fade

遷移元Poseと遷移先Poseを時間WeightでBlendします。遷移中に次の遷移が起きる場合のSource Pose保持を設計します。

## 64. Animation State Machine

Idle、Move、Attack、Dodge、Hit等のStateと条件、優先度、遷移時間を管理します。Gameplay Stateと見た目Stateの責務を分けます。

## 65. Blend Tree

Speed/Direction等のParameterで複数ClipをBlendします。2D BlendではSample配置とWeightの連続性を検証します。

## 66. Additive Animation

Reference Poseとの差分をBase Poseへ加えます。上半身Recoil、呼吸、Hit Reaction等に使えます。Local/Model Space規約を固定します。

## 67. Bone Mask

JointごとのWeightで上半身/下半身等をLayer分けします。Hierarchy境界で不自然な断絶が出ないよう遷移Weightを調整します。

## 68. Root Motion

Root Jointの移動/回転を抽出し、Gameplay Character Transformへ適用します。Animation側移動とController側移動を二重適用しません。

## 69. Root Motionと衝突

希望移動をCollision/Navigationへ渡し、実際に許可された移動量をCharacterへ反映します。見た目とPhysicsの誤差補正方針を決めます。

## 70. In-place Animation

Animation内の移動を除き、Gameplay速度でModelを動かします。操作応答を作りやすい一方、足滑り対策が必要です。

## 71. Animation Event

Hit判定開始、Footstep、Effect、Cancelable Window等の時刻を持てます。Frame落ちでEvent時刻を飛び越えても区間判定で一度だけ発火します。

## 72. Gameplay判定との関係

CriticalなDamage判定を表示専用Eventだけに依存させると同期/再現性が崩れる場合があります。権威あるSimulation Dataとの責務を定めます。

## 73. Fixed TickとRender補間

Gameplay/Physicsを固定Step、描画Poseを補間する設計があります。Hit判定Poseと表示Poseの時間差を把握します。

## 74. Animation Compression

Key削減、Quantization、Constant Track除去、Segment圧縮等でMemory/Bandwidthを減らします。Joint/Clip別の誤差基準を設けます。

## 75. Quaternion圧縮

最大成分省略等の方式があります。復元後の正規化、符号、角度誤差をTestします。

## 76. Clip Streaming

全Clipを常駐させず必要Chunkを読込む設計があります。次に必要なActionを予測し、未到着時のFallbackを用意します。

## 77. Animation LOD

遠距離Characterは更新頻度、Joint数、Morph、IKを減らせます。Camera距離だけでなく画面Sizeと重要度を使います。

## 78. Skeleton LOD

省略Jointを親へRemapし、Mesh WeightとSocket/Effect Attachmentへの影響をCook時に処理します。

## 79. Mesh LOD

画面Sizeに応じVertex/Triangle数の異なるMeshへ切替えます。Silhouette、UV、Skin Weight、Material Slotの互換を検証します。

## 80. LOD Hysteresis

境界付近で毎Frame切替わるPoppingを防ぐため、入る閾値と戻る閾値を分けます。

## 81. Frustum Culling

World BoundsをCamera Frustumと判定し、完全に外ならDrawを除外します。Animation Boundsが小さ過ぎると手足が消えます。

## 82. Occlusion Culling

Depth Pyramid/Query等で隠れたObjectを除外できます。判定Cost、Latency、急なCamera移動時の保守性を考慮します。

## 83. Per-submesh Culling

細かいほどDraw削減の可能性は増えますが、Bounds/判定/Command管理Costも増えます。粒度を実測します。

## 84. Instancing

同じMesh/Materialを複数Transformで描く場合、Instance DataをBufferへまとめDraw Callを減らせます。

## 85. Skinned Instancing

CharacterごとのJoint Palette OffsetをInstance Dataへ持たせる設計があります。異なるSkeleton/LOD/Material VariantのBatch条件を定義します。

## 86. Draw Command生成

CPUでVisible Submeshを分類してCommandを記録する方式と、Compute CullingからIndirect Argumentを生成する方式があります。

## 87. ExecuteIndirectへの接続

GPU Driven RenderingではVisible Result、Draw Argument、Count Buffer、Command Signatureを管理します。詳細は次章で扱います。

## 88. 非同期Asset Load

```text
I/O -> decode/cook read -> CPU validation
 -> upload allocation -> copy queue
 -> fence completion -> render registry publish
```

途中状態をRender Threadへ公開しません。

## 89. Placeholder

Model未到着時はBox/既定Mesh、Material未到着時はFallback Textureを使い、Null Descriptorや未初期化GPU Addressを避けます。

## 90. Asset Handle

Pointerを長期保持する代わりにGeneration付きHandle等を使い、Unload/Reload後の古い参照を検出します。

## 91. Hot Reload

新ResourceのUpload/Fence完了後に参照を切替え、旧Resourceは使用中Frame完了後に遅延破棄します。

## 92. Dependency

ModelからMaterial、MaterialからTexture、SkeletonからClip等の依存Graphを追跡し、Reload/Unload順を決めます。

## 93. CPU/GPU Ownership

CPU Asset、Upload Job、GPU Resource、Descriptor、Render Instanceの所有者を分離します。`shared_ptr`だけでFence安全性は保証されません。

## 94. Character Render Instance

```cpp
struct CharacterRenderInstance
{
    MeshHandle mesh;
    MaterialSetHandle materials;
    D3D12_GPU_VIRTUAL_ADDRESS jointPalette;
    DirectX::XMFLOAT4X4 world;
    std::uint32_t objectId;
    std::uint32_t flags;
};
```

Gameplay ObjectそのものをRender Commandへ直接渡さずSnapshot化します。

## 95. Previous Transform/Pose

Motion VectorやTAA用に前FrameのWorld/Poseを保持します。Teleport/Spawn時は不正な長いVelocityが出ないようResetします。

## 96. Motion Vector

現在/前回Clip PositionからScreen Space移動量を出します。Skinned Vertexでは前回Skinning結果または前回Joint Paletteが必要です。

## 97. Shadow Pass

Main MaterialのColor計算は不要でも、Position/Skinning/Alpha Maskは必要です。Depth-only PSOとTexture Bindingを最小化します。

## 98. Outline Pass

Skinned Position/Normal、Stencil、Cull、Depthを前章のOutline PSOへ接続します。表情Morphや衣装Scaleも反映します。

## 99. 多人数戦闘のBudget

Visible Character数、Skinned Vertex数、Joint評価数、Shadow Caster数、Draw数、Material数、透明Pixel数をFrame単位で記録します。

## 100. Update Rate分離

近いPlayer/敵は毎Frame、遠い群衆は低頻度でAnimation更新し、間を補間できます。Hit対象等の重要度で例外を作ります。

## 101. Pose Cache

同じClip/時刻/ParameterのPoseを共有できる場合がありますが、個別Blend、IK、Root Motion、Eventを誤共有しないようKeyを厳密にします。

## 102. Animation Job

Clip Sampling、Blend、Global Pose、Skin Matrix作成をJob化できます。Skeleton単位の依存とOutput Buffer領域を事前に割り当てます。

## 103. Render Threadへの受渡し

Frame SnapshotまたはDouble BufferでPose/Instance Dataを渡し、Gameplay Threadが書込み中のDataを読みません。

## 104. Memory Layout

AoS/SoA、Joint-major/Character-majorを処理単位に合わせます。SIMD、Cache Line、Upload Copy回数をProfilerで確認します。

## 105. Debug表示

Skeleton Line、Joint Axis、Bind Pose、Bounds、LOD、Weight Heatmap、Normal/Tangent、Material IDを切替表示できるようにします。

## 106. PIX確認

Mesh Buffer、Input Layout、Root Parameter、Material SRV、Joint Buffer、PSO、Draw引数、Resource StateをDraw単位で確認します。

## 107. Asset Validation

NaN/Inf、Index範囲外、Degenerate Triangle、Weight合計、Joint範囲、Hierarchy Cycle、Texture欠損、Clip時刻順をCook時に拒否します。

## 108. Unit Test

TRS合成、Hierarchy、Quaternion補間、Key検索、Loop境界、Pose Blend、Root Motion抽出、Bounds変換、Runtime File ParserをTestします。

## 109. Golden Pose Test

既知Clipを既知時刻で評価し、Joint Transformを許容誤差付きで比較します。Importer/Compression変更による差を検出します。

## 110. Render Test

Static、Skinned、Morph、複数Material、Alpha Mask、LOD、Shadow、Outlineを固定Cameraで画像比較します。

## 111. Stress Test

大量Character、Clip切替、Asset Streaming、Hot Reload、LOD境界、Copy Queue混雑、Device Lostを組み合わせます。

## 112. よくある失敗：Modelが裏返る

座標系、Winding、Cull、負Scale、行列掛け順を確認します。Cull Noneで隠すだけでは根本解決になりません。

## 113. よくある失敗：皮膚が爆発する

Joint Index範囲、Weight Format/正規化、Inverse Bind、Matrix Convention、Buffer Offset、Frame Lifetimeを確認します。

## 114. よくある失敗：Animationが震える

Quaternion符号、Key時刻、圧縮精度、Delta Time、親子評価順、Data Raceを確認します。

## 115. よくある失敗：Textureが変

SRGB/Linear、Channel Packing、Normal Y、Sampler、Descriptor Index、Fallback差替え時期を確認します。

## 116. よくある失敗：遠距離で消える

Animation Bounds、World変換、Frustum Plane、LOD Bounds、Occlusion Latencyを確認します。

## 117. よくある失敗：Frameごとに化ける

Upload Ring上書き、Joint Palette Offset、Fence、Descriptor再利用、Snapshot同期を確認します。

## 118. 実装Checklist

- [ ] 座標系、単位、行列、Windingを固定する。
- [ ] Sourceを検証済みRuntime形式へCookする。
- [ ] Mesh/Submesh/Material/Boundsを分離する。
- [ ] Default Heap UploadとFence Lifetimeを守る。
- [ ] PBR TextureのSRGB/Linear規約を守る。
- [ ] Joint/Weight/Inverse Bindを検証する。
- [ ] Pose補間、Blend、Root Motion、Event境界をTestする。
- [ ] Materialを正しいPSO/Queueへ分類する。
- [ ] LOD/Culling/更新頻度をBudget化する。
- [ ] PIXと画像Testで描画を検証する。

## 119. 理解確認問題

1. Source形式をRuntime形式へCookする理由を説明してください。
2. Mesh、Submesh、Materialの関係を説明してください。
3. Default HeapへMeshをUploadする流れを説明してください。
4. Bind Pose、Inverse Bind、Animated Global Poseの関係を説明してください。
5. Vertex Shader SkinningとCompute Skinningを比較してください。
6. Quaternion Blendで符号を考慮する理由を説明してください。
7. Root MotionとCollisionを統合する方法を説明してください。
8. Alpha ModeをPSO/描画Queueへ割り当ててください。
9. Animation LODで削減できる処理を挙げてください。
10. 多人数戦闘Sceneで記録すべきBudgetを挙げてください。

## 120. 要点

- ModelはNode、Mesh、Material、Skeleton、Animationから成る複合Assetです。
- Source Dataを検証、正規化、圧縮したRuntime形式へ変換します。
- D3D12ではMesh Upload、Descriptor、Resource State、Fence Lifetimeを明示管理します。
- SkinningはPose、Hierarchy、Inverse Bind、Weightの規約一致が重要です。
- AnimationはClip再生だけでなくBlend、Layer、Root Motion、Eventを含むSystemです。
- Material分類をDepth/Blend/Rasterizer PSOと描画順へ接続します。
- LOD、Culling、Job、Skinning方式は多人数高速戦闘の計測結果から選びます。

## 121. 参考資料

- [glTF 2.0 Specification](https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html)
- [D3D12_VERTEX_BUFFER_VIEW](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/ns-d3d12-d3d12_vertex_buffer_view)
- [D3D12_INDEX_BUFFER_VIEW](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/ns-d3d12-d3d12_index_buffer_view)
- [ID3D12GraphicsCommandList::DrawIndexedInstanced](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12graphicscommandlist-drawindexedinstanced)
- [DirectXMath Programming Guide](https://learn.microsoft.com/en-us/windows/win32/dxmath/pg-xnamath-programming-guide)

## 122. 次章への接続

次章ではCompute・UAV・Indirectを扱います。本章のCompute Skinning、Culling、Instance Data、Draw Command生成をCompute Pipelineと`ExecuteIndirect`へ接続します。
