# Navigation Mesh

NavMeshは歩行可能な3D表面を凸Polygonの集合として表します。Gridより少ないNodeで自由な空間を表現できます。

## Build

```text
Scene Geometry
→ Walkable Slope判定
→ Agent半径分の障害物膨張
→ Voxel/Contour生成
→ Convex Polygon化
→ 隣接Link生成
```

Agent Radius、Height、Step Height、Max Slopeごとに通行可能領域が変わります。

## Polygon Graph

各PolygonをNode、共有Edgeを接続としてA*探索します。Start/Goal World位置を最寄りNav PolygonへProjectします。単純なPolygon中心経路は不自然なのでPortal列を作ります。

## Funnel Algorithm

通過Portalの左右端から、Nav Corridor内の最短に近い折れ線を作ります。座標系の左右判定、重複Portal、非常に狭いPortalを処理します。

## Off-mesh Link

Jump、Drop、Ladder、Door等を特別なEdgeとして表します。

```cpp
struct OffMeshLink
{
    Vector3 start{};
    Vector3 end{};
    TraversalType type{};
    bool bidirectional{};
    float cost{};
};
```

Path追従中にLinkへ到達したら、通常移動から専用Actionへ移ります。Animation、Root Motion、着地点安全性を検査します。

## Dynamic Obstacle

- Carving：NavMeshから一時領域を切る。更新Costあり。
- Local Avoidance：小さい移動障害物を回避。
- Cost/Area変更：危険地帯・閉鎖Door。
- Tile再Build：大きな地形変更。

すべての移動ObjectでCarveしません。

## Path Corridor

完全なWaypoint列だけでなくPolygon Corridorを保持し、Agent移動に合わせStartを更新します。Goal移動時は部分修正またはRepathします。

## Nav Area

Ground、Mud、Water、Danger等へCostを設定します。Cost 0や負値を許さず、能力による通行可否をFilterへ持たせます。

## Streaming

NavMeshをTileへ分け、World Streamingと同期します。Path Job完了時にNav Versionを確認し、Unload済みTileを参照しません。

## Debug

Polygon、Link、Area、Agent Radius、Corridor、Funnel Portal、Project結果、Repath理由を表示します。
