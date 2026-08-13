# キャラクター移動・重力・2D物理

アクションゲームのキャラクターは、現実物理を完全再現するより、入力へ素早く反応し再現可能な制御を優先することが多くあります。

## 基本積分

```cpp
velocity += acceleration * deltaSeconds;
position += velocity * deltaSeconds;
```

これはSemi-Implicit Eulerの基本形です。位置を先に更新するExplicit Eulerとは安定性が異なります。固定deltaを使うと調整と再現が容易になります。

## 横移動

目標速度へ一定加速度で近づけます。

```cpp
float MoveToward(float current, float target, float maxDelta)
{
    if (current < target)
    {
        return std::min(current + maxDelta, target);
    }
    return std::max(current - maxDelta, target);
}

const float targetSpeed{moveInputX * maximumSpeed};
const float acceleration{
    std::abs(moveInputX) > 0.0F ? groundAcceleration : groundDeceleration
};

velocity.x = MoveToward(
    velocity.x,
    targetSpeed,
    acceleration * deltaSeconds
);
```

加速、減速、空中制御、方向反転を別パラメータにすると手触りを調整できます。

## 重力と落下速度上限

```cpp
velocity.y += gravityPixelsPerSecondSquared * deltaSeconds;
velocity.y = std::min(velocity.y, maximumFallSpeed);
```

Y下向き正の座標系の例です。符号をプロジェクト全体で統一します。

## ジャンプ

目標の高さ`h`と重力加速度`g`から初速度の大きさを求められます。

```text
jumpSpeed = sqrt(2 * gravity * jumpHeight)
```

Y下向き正なら上向き速度は負です。

```cpp
velocity.y = -std::sqrt(2.0F * gravity * jumpHeight);
```

高さと頂点到達時間から重力と速度を逆算すると、デザイナーが理解しやすい調整値になります。

## 可変ジャンプ

ボタンを早く離した時、上昇速度を弱めます。

```cpp
if (jumpWasReleased && velocity.y < 0.0F)
{
    velocity.y *= jumpCutMultiplier; // 0～1。
}
```

上昇・頂点付近・下降で重力倍率を変えると、上が軽く下が速いアクション向け軌道を作れます。

## 接地判定

Y速度が0だから接地とは限りません。足元Shape Cast、衝突接触法線、短いProbeなどで地面を検出します。

```cpp
isGrounded = contact.normal.y < -minimumGroundNormalY;
```

座標系と法線向きによって符号は変わります。壁接触を地面扱いしないよう、法線の傾斜を検査します。

## Coyote Timeと入力バッファ

- Coyote Time：足場を離れて短時間はジャンプ可能。
- Jump Buffer：着地直前の入力を短時間保存。

両方を組み合わせると、見た目とプレイヤー感覚のずれを吸収します。使用後にタイマーを消費し、空中ジャンプ回数と混同しません。

## 軸分離によるタイル衝突

```text
1. X位置を進める
2. 重なったSolid TileからX方向へ押し戻し、velocity.xを0
3. Y位置を進める
4. Y方向へ押し戻し、接地・天井状態とvelocity.yを更新
```

簡潔で矩形タイルに向きますが、斜面、動く床、高速移動には追加処理が必要です。

## 動く床

床の速度または前tickからの変位をキャラクターへ継承します。単に親子Transformへすると、ジャンプ時の解除、回転床、衝突解決との二重移動が問題になります。

```text
floorDeltaを適用 → player自身を移動 → 衝突解決
```

床がキャラクターを壁へ押し込む場合の優先順位も必要です。

## One-way Platform

上から降りた時だけ着地する床です。

- 移動前の足元が床上面以下でないこと。
- 下降中であること。
- 今tickで上面を跨いだこと。
- 下入力による一時無視状態でないこと。

現在位置の重なりだけで判定すると、下から上昇した際に引っ掛かります。

## KinematicとDynamic

- Kinematic Controller：コードが目標変位を決め、衝突で修正。操作感を作りやすい。
- Dynamic Body：力・Impulseを物理ソルバーへ与える。相互作用は自然だが制御が難しい。

プレイヤーをKinematic、箱や破片をDynamicにする混合設計があります。外部物理エンジンを使っても、Layer、形状、質量、摩擦、固定tick、所有権の理解は必要です。

## 摩擦と減衰

毎フレーム`velocity *= 0.9F`はFPS依存です。固定tickに限定するか、指数減衰を使います。

```cpp
velocity *= std::exp(-dragPerSecond * deltaSeconds);
```

地上の操作減速と物理的Dragは意味を分けます。

## 状態リセット

Scene開始、Respawn、Teleport時には位置だけでなく速度、接触キャッシュ、床参照、入力バッファ、残りジャンプ、攻撃状態を整合した初期状態へ戻します。
