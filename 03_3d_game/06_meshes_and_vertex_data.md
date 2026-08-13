# Mesh・Vertex・Index・Tangent Space

MeshはVertex DataとTriangle Topologyから形状を表します。GPUは通常TriangleをRasterizeします。

## Vertex属性

```cpp
struct Vertex
{
    Vector3 position{};
    Vector3 normal{};
    Vector4 tangent{}; // xyz方向、wにBitangent符号等。
    Vector2 uv{};
    Color color{};
};
```

Skinning MeshではBone IndexとWeightを追加します。Shader Input LayoutとCPU structのFormat・Offsetを一致させます。Paddingも確認します。

## Vertex BufferとIndex Buffer

```text
Vertex Buffer：属性配列
Index Buffer ：三角形が参照するVertex番号
```

共有頂点をIndexで再利用しMemoryとVertex Shader実行を減らします。

```text
indices = [0, 1, 2, 2, 1, 3]
```

16-bit Indexは最大参照数が小さい代わりBandwidthを節約、32-bitは大Meshに対応します。

## WindingとBack-face Culling

画面上でClockwiseまたはCounter-clockwiseをFrontと定義します。座標系変換で一軸反転するとWindingも反転します。Cull ModeとAsset Importを合わせます。

## Normal

Surfaceの向きを表しLightingに使います。Triangle Face Normalは辺の外積から求めます。Vertex Normalを隣接Faceで平均すると滑らかに見えますが、Hard Edgeでは頂点を分割して異なるNormalを持たせます。

## UV SeamとVertex分割

同じPositionでも、UV、Normal、Materialが異なればGPU上は別Vertexが必要です。そのためDCC Toolの「頂点数」とRuntime Vertex数が異なります。

## Tangent Space

Normal MapのTexture方向をWorldへ変換するBasisです。

```text
Tangent / Bitangent / Normal
```

Tangent `w`へBitangent handednessを保存し、Shaderで`B = sign * cross(N,T)`と再構築できます。Asset ToolとEngineでMikkTSpace等の生成規約を一致させないとSeamが出ます。

## InterleavedとSeparate

- AoS/Interleaved：一Vertexの属性をまとめる。一般描画で局所性がよい。
- SoA/Separate Stream：Positionだけ必要なPass等でBandwidthを減らせる。

Pipelineと更新頻度で選びます。

## Static・Dynamic・Streaming Buffer

- Static：一度Uploadしほぼ変更しない。
- Dynamic：CPUから頻繁に更新。
- Streaming/Ring：Frameごとの一時Geometry。

GPU使用中BufferをCPUが上書きしないよう、複数Frame分、Ring Allocation、Fenceを使います。

## Submesh

一Mesh内でMaterialが異なるIndex範囲をSubmeshとして描きます。Material数が多いとDraw Callが増えます。AtlasやMaterial統合とAsset制作上の柔軟性を比較します。

## Bounds

Local AABB/SphereをImport時に計算し、World TransformでCulling用Boundsへ変換します。回転AABBは8 Corner変換またはAbsolute MatrixでExtentを求めます。Skinned MeshはAnimationでBoundsが変わるため、Clip Bounds、Bone Bounds、CPU/GPU更新を使います。

## LOD

距離・画面占有率に応じてTriangle数の少ないMeshへ切り替えます。切替のPoppingにはHysteresis、Cross Fade、Ditherを使います。LODごとにMaterialやBone数も削減できます。

## Import

Source AssetからRuntime形式へ変換します。

- Axis/Handedness/Unit。
- Triangulation。
- Normal/Tangent生成。
- UV Channel。
- Material Slot。
- Skin Weight上限。
- Animation/Skeleton参照。
- Bounds。

RuntimeでFBX等を直接解析するより、Build時に検証済みBinaryへCookする方式が一般的です。
