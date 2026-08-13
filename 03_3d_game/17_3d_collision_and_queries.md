# 3D衝突形状・Query・Continuous Collision

3D衝突はBroad Phaseで候補を減らし、Narrow Phaseで形状を検査し、ContactをSolverへ渡します。

## 基本形状

- Sphere：回転に影響されず高速。
- AABB：軸平行Box。Broad Phaseに有用。
- OBB/Box：回転可能。
- Capsule：Character・四肢に適する。
- Convex Hull：任意凸形状。
- Triangle Mesh：Static地形向け。
- Height Field：Terrain向け。

動的な凹Mesh同士は高コスト・不安定なため、Convex分解やPrimitive複合を使います。

## Bounds

Local AABBをWorldへ変換したWorld AABBをBroad Phaseへ登録します。Rotation時は8 Cornerを変換するか、Rotation Matrixの絶対値でExtentを求めます。

## Query

```cpp
struct QueryFilter
{
    CollisionMask layers{};
    EntityId ignoredEntity{};
    bool includeTriggers{};
};
```

- Raycast：線で最初/全Hit。
- Shape Cast：Sphere/Capsule/Boxを移動。
- Overlap：現在重なる形状。
- Closest Point：最近点。

結果にDistance、Point、Normal、Collider、Material、Face Indexを持たせます。

## RayとSphere

二次方程式を解き、Ray Parameterが`>=0`か検査します。Ray方向が正規化済みならParameterが距離です。Sphere内部開始時に入口/出口のどちらを返すかを仕様化します。

## RayとTriangle

Möller–Trumbore等で交差とBarycentric座標を求めます。Back-faceをHitするか、平行Tolerance、Segment最大距離を設定します。

## SAT

Box等の凸多面体で分離軸を調べます。OBB同士では両Box軸と軸同士の外積が候補です。ほぼ平行軸の数値誤差へToleranceが必要です。

## GJK

Support Mapping可能な凸形状同士の交差・距離をMinkowski Difference上で調べます。交差後のPenetration Depth/NormalにはEPA等を使います。実装は数値的Edge Caseが多く、検証済みPhysics Libraryを利用することが一般的です。

## Capsule

線分と半径で表せます。Capsule同士は二線分の最近点距離を半径和と比較します。Character Controllerでは段差・斜面に安定しやすい形状です。

## Triangle Mesh

全Triangleを比較せず、BVH等で候補Triangleを抽出します。薄い片面Triangleの裏側、Edge/Vertex接触、Scaleを処理します。動的変形MeshはAcceleration Structure更新が高コストです。

## Contact Manifold

一点だけでなく複数接触点を保持し、Boxが面上で安定するようにします。前StepのContactをCacheしWarm Startへ使えます。

## Continuous Collision Detection

高速Objectが薄い壁を通過するTunnelingを防ぎます。

- Swept ShapeでTime of Impactを求める。
- Conservative Advancement。
- Speculative Contact。
- Substep。

全ObjectへCCDを使うと高コストなので、弾・高速Body等へ限定します。

## Filter

Layer/Mask、Trigger、同一Owner、Team、Joint接続Bodyの無視をBroad Phase前後で適用します。Filter Callback内でWorldを変更しません。

## Debug

Shape、AABB、BVH Node、Ray、Hit Point/Normal、Contact Manifold、Pair数、Query時間を表示します。
