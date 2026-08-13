# 衝突検出・衝突応答・空間分割

衝突システムは「重なったか」だけでなく、候補抽出、詳細判定、接触情報、押し戻し、イベント通知を順序立てて処理します。

## Broad PhaseとNarrow Phase

要素数`N`の全組合せを比較すると、おおよそ`N²`回になります。

```text
Broad Phase：衝突し得る候補Pairを安価に抽出
Narrow Phase：候補へ正確な図形判定
Resolution ：位置・速度を修正、イベント生成
```

Broad PhaseにはUniform Grid、Spatial Hash、Sweep and Prune、Quadtree、BVHなどがあります。

## Uniform Grid

世界を同じ大きさのセルへ分け、Colliderが重なるセルへ登録します。

```cpp
struct CellCoordinate
{
    int x{};
    int y{};
};

CellCoordinate WorldToCell(Vector2 position, float cellSize)
{
    return {
        static_cast<int>(std::floor(position.x / cellSize)),
        static_cast<int>(std::floor(position.y / cellSize))
    };
}
```

負座標では単純な整数キャストが0方向へ丸めるため、`floor`が必要です。大きなColliderは複数セルへ登録し、同じPairの重複判定をPair IDで除去します。

## Collision LayerとMask

```cpp
enum class CollisionLayer : std::uint32_t
{
    Player       = 1u << 0,
    Enemy        = 1u << 1,
    PlayerAttack = 1u << 2,
    EnemyAttack  = 1u << 3,
    World        = 1u << 4
};
```

各Colliderが所属Layerと、衝突したいLayerのMaskを持ちます。

```cpp
bool ShouldTest(const Collider& a, const Collider& b)
{
    return (a.mask & b.layer) != 0u &&
           (b.mask & a.layer) != 0u;
}
```

ビット操作用の型安全な演算を用意し、列挙値を無秩序に整数化しません。

## 接触情報

```cpp
struct Contact
{
    EntityId first{};
    EntityId second{};
    Vector2 normal{};    // firstをsecondから離す方向など、向きを契約化。
    float penetration{}; // 重なり深さ。
    Vector2 point{};
};
```

法線の向きを統一しないと、一方を押し戻す符号が逆になります。

## AABBの最小押し戻し

X軸とY軸の重なり量を求め、小さい方の軸へ分離します。

```cpp
const float overlapX{
    std::min(a.maximum.x, b.maximum.x) -
    std::max(a.minimum.x, b.minimum.x)
};

const float overlapY{
    std::min(a.maximum.y, b.maximum.y) -
    std::max(a.minimum.y, b.minimum.y)
};
```

同じ深さ、角への侵入、複数壁同時接触では結果が順序依存になります。キャラクター制御ではX移動→X解決→Y移動→Y解決の軸分離が扱いやすい場合がありますが、高速斜面や一般物理には限界があります。

## 離散判定とトンネリング

高速な弾が一フレームで薄い壁の反対側へ移ると、開始・終了位置のどちらでも重ならず見逃します。

対策：

- 固定tickを細かくする。
- 速度に応じてSubstepする。
- 移動前から移動後までShape Castする。
- 連続衝突判定でTime of Impactを求める。
- 弾をRay/Segmentとして扱う。

tickを細かくするだけでは速度上限次第で再発し、CPUコストも増えます。

## Triggerイベント

`Enter`、`Stay`、`Exit`を作るには、前tickと今tickの接触Pair集合を比較します。

```text
current - previous → Enter
current ∩ previous → Stay
previous - current → Exit
```

Entityが破棄された場合にExitを通知するか、Scene遷移時に全Pairをクリアするかを決めます。Pairの順序を正規化し、`(A,B)`と`(B,A)`を同一視します。

## 一フレーム一命中

Hit BoxとHurt Boxが数tick重なると、毎tickダメージが入る可能性があります。攻撃インスタンスIDと命中済み対象集合を持ちます。

```cpp
if (!attack.HasAlreadyHit(targetId))
{
    ApplyDamage(targetId, attack.damage);
    attack.MarkHit(targetId);
}
```

多段攻撃ではHit Group、再命中間隔、最大命中数をデータ化します。

## イベント中にWorldを変更しない

Collision callback内で相手を即削除すると、残りPairが無効になります。Damage/Destroyイベントをキューへ積み、検出フェーズ後に処理します。順序を決定的にしたい場合、イベントをID等で安定ソートします。

## 浮動小数点と接触許容差

完全に面へ置いたつもりでも微小侵入・離隔が起きます。Skin Width、Contact Offset、epsilonを使いますが、大きすぎると空中接地、小さすぎると震えになります。ワールド単位に対して定めます。

## デバッグ統計

- Collider総数。
- Broad Phase Pair数。
- Narrow Phase回数。
- 実接触数。
- セル占有数と最大密度。
- Raycast回数。
- 各フェーズ時間。

表示すると、空間分割が本当に比較数を減らしているか判断できます。
