# 空間分割・可視性・LOD

Scene内の全Objectを毎Frame、全Systemで調べると規模に比例して負荷が増えます。空間構造で「影響し得る候補」だけを抽出します。

## 構造の選択

- Uniform Grid：均一分布、動的Object、近傍検索。
- Spatial Hash：広い疎なWorld。
- Quadtree/Octree：密度差のある空間。
- BVH：Mesh Triangle、Ray Query、Static/Dynamic Object。
- KD-tree：点・分割検索。
- Portal/Room：室内の可視性。

名前で選ばず、Object分布、更新頻度、Query種類、Memoryを測ります。

## Dynamic AABB Tree

各LeafがObjectのAABBを持ち、親が子Boundsを包含します。移動Objectには少し大きいFat AABBを与え、小移動の再挿入を減らします。定期的にTree品質を測り再構築します。

## Frustum Culling

Camera Frustum PlaneとObject Sphere/AABBを比較します。BroadにSphere、必要ならAABBを使います。Skinned Mesh Boundsが小さすぎると手足が消えます。

## Occlusion Culling

Frustum内でも壁の裏は描画不要です。

- Hardware Occlusion Query。
- Hierarchical Z Buffer。
- Software Rasterizer。
- Precomputed Visibility。

Query結果待ちでCPU/GPUを同期せず、前Frame結果やGPU-driven処理を使います。急に現れるObjectへ保守的Boundsを使います。

## LOD

World距離よりScreen占有率を基準にするとFOV・解像度へ対応しやすくなります。

- Mesh LOD。
- Material/Shader LOD。
- Animation/Bone LOD。
- Particle LOD。
- AI更新頻度。

切替にはHysteresisを設け、境界で往復しないようにします。

## Instancing

同じMesh/Materialを異なるTransformでまとめて描きます。Instance BufferへTransform、Color、Object ID等を格納します。透明順序、個別Material差、Culling粒度が制約です。

## GPU-driven Rendering

Compute ShaderでCulling・LODを行いIndirect Draw Commandを生成します。CPU Draw Callを減らせますが、Debug、Resource Lifetime、Platform対応、Overdrawを管理します。

## Streaming Cell

大規模WorldをCellへ分け、Camera・Gameplay位置周辺だけロードします。非同期完了時の世代確認、隣Cell依存、Physics/Nav/Dataのロード順、Memory Budgetが必要です。

## Gameplay Query

Rendering可視性とGameplay Activeを同一にしません。画面外の敵、Projectile、Timerを停止すると規則が変わります。Full/Reduced/Dormant等のSimulation LODを別に設計します。

## Debug

Tree、Cell、Frustum、Occlusion結果、LOD、Draw数、Query候補数、再挿入数を表示します。
