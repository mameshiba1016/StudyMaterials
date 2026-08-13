# Graph探索・Dijkstra・A*

PathfindingはNodeとEdgeからなるGraph上でStartからGoalへの経路を求めます。

```cpp
struct Edge
{
    NodeId destination{};
    float cost{};
};
```

Costは負でない前提にします。距離だけでなく危険、Slope、Door、能力条件を含められます。

## BFS

全Edge Costが同じGraphで最短Edge数を求めます。Queueを使います。

## Dijkstra

非負Costの最小経路を求めます。Startからの確定Costが小さいNodeをPriority Queueで展開します。

## A*

```text
f(n) = g(n) + h(n)
```

- `g`：Startから現在Nodeまでの実Cost。
- `h`：Goalまでの推定Cost。

`h=0`ならDijkstraです。

## Heuristic

最適経路保証には通常、実際の残りCostを過大評価しないAdmissible Heuristicを使います。Gridでは移動規則に合わせます。

- 4方向：Manhattan。
- 8方向：Octile。
- 自由空間：Euclidean。

Diagonal移動を許すのにManhattanをそのまま使う等、Costと一致しない設計に注意します。

## Open/Closed

Openは未展開候補、Closedは展開済みです。より安い経路が見つかった時にCostとParentを更新します。Priority QueueにDecrease-keyがなければ新Entryを追加し、取り出し時に古いEntryを無視できます。

## 経路復元

Goalから`cameFrom`をStartまで辿って逆順にします。到達不能、Start=Goal、壊れたParent循環を処理します。

## Grid

CellをNodeとしてSolidを除外します。Corner Cuttingを許すか、Agent半径、異なる地形Costを定義します。Point Agentの経路は太いCharacterが通れないため、ClearanceまたはNavMeshを使います。

## Path Smoothing

Grid経路の各角をそのまま通ると不自然です。離れたWaypoint間にLine of Sightがあれば中間を削除します。Collision Radiusを含むShape Castを使います。

## 動的障害物

小さな移動AgentはLocal Avoidance、大きな通路閉鎖はRepathします。毎tick全AgentがRepathしないようCooldownとFrame分散を使います。

## Hierarchical Pathfinding

大MapをRegionへ分け、上位Graphで粗経路、局所Graphで詳細経路を求めます。Streaming Cell境界にも利用できます。

## 非同期化

Path Jobへ不変Nav SnapshotとStart/Goalを渡します。完了時にRequesterの世代、Goal変更、Nav Versionを検査し、古い結果を捨てます。

## Debug/計測

Open/Closed、Cost、Heuristic、Parent、最終Path、展開Node数、最大Queue、時間、Repath理由を表示します。
