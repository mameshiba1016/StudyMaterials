# タイルマップ・ステージデータ

タイルマップは、ステージを一定サイズのセルへ分割し、各セルへタイルIDや属性を保存します。描画レイヤーと衝突・ゲームプレイデータを分離します。

## 一次元配列で保存

```cpp
class TileLayer
{
public:
    TileId Get(int x, int y) const
    {
        if (x < 0 || y < 0 || x >= width_ || y >= height_)
        {
            return TileId::Invalid;
        }

        return tiles_[static_cast<std::size_t>(y * width_ + x)];
    }

private:
    int width_{};
    int height_{};
    std::vector<TileId> tiles_{};
};
```

`width * height`の整数オーバーフロー、ファイルの巨大値、配列サイズ不一致をロード時に検証します。

## WorldとTile座標

```cpp
int WorldToTile(float worldCoordinate, float tileSize)
{
    return static_cast<int>(std::floor(worldCoordinate / tileSize));
}

float TileToWorld(int tileCoordinate, float tileSize)
{
    return tileCoordinate * tileSize;
}
```

負座標で`static_cast<int>`だけを使うと0方向へ丸められ、セルがずれます。

## Tile Set

Tile IDから次のメタデータを参照します。

- Atlas内Source Rect。
- Solid、One-way、Ladder、Damage等の属性。
- アニメーション。
- 摩擦、足音Surface。
- 自動接続ルール。

数値IDをコードへ直接散らさず、ロード時に定義とMapの参照整合性を検査します。

## レイヤー

```text
Background Decoration
Back Terrain
Collision / Gameplay（非表示でもよい）
Front Terrain
Foreground Decoration
Object Spawn
Trigger
```

見た目タイルを変更してColliderが消えるような結合を避けます。一方、小規模作品では同じタイル属性からColliderを生成してもよく、編集ワークフローに合わせます。

## 可視範囲だけ描く

CameraのWorld BoundsをTile座標へ変換し、少し余白を加えて反復します。

```cpp
const int firstX{WorldToTile(cameraLeft, tileSize) - 1};
const int lastX {WorldToTile(cameraRight, tileSize) + 1};
```

Map全体を毎フレーム描画しません。境界をMap範囲へクランプします。

## 衝突候補

キャラクターAABBの移動後範囲が重なるTile座標だけを調べます。全Tileとの比較は不要です。高速移動なら開始から終了までのSwept Boundsを使います。

## 複数Solid Tileの統合

各Solid Tileを個別Colliderにすると接触数が増え、タイル境界で引っ掛かる場合があります。隣接Solidを大きな矩形へ結合するGreedy Meshingや輪郭抽出をロード時に行う方法があります。

## Auto Tiling

上下左右・斜め近傍の接続状態をビット化し、表示タイルを選びます。

```text
Up=1, Right=2, Down=4, Left=8
```

論理地形IDと表示Variantを分けると編集しやすくなります。

## Object Layer

敵やアイテムをTile IDだけで表すと、パラメータや回転、安定IDを持ちにくくなります。Object Layerへ型、位置、サイズ、プロパティ、GUIDを保存し、Factoryで生成します。未知の型や不足プロパティはロードエラーへします。

## Chunk

大規模Mapは固定サイズChunkへ分割し、Camera周辺だけロード・描画・衝突登録します。

- 非同期読込中の代替表示。
- 隣Chunk境界。
- 変更済みChunkの保存。
- EntityがChunk間を移動する所有権。
- ロード完了時にSceneが変わっていないか。

を設計します。

## 外部Editor

Tiled等を使う場合、JSON/TMXを直接ゲーム毎フレームで解釈するのではなく、開発用Import段階で検証・変換したランタイム形式を作る方法があります。Editorのバージョン、カスタムプロパティ、座標系、Layer順を固定します。
