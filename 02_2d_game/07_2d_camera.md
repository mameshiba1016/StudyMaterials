# 2Dカメラ

カメラは世界のどの範囲を画面へ写すかを定義します。プレイヤー座標を直接画面中央に描くだけでは、追従、揺れ、ズーム、画面端、複数解像度を扱えません。

## WorldからScreenへの基本変換

回転なし、`cameraPosition`をビュー中心、`viewportSize`を内部解像度とする例です。

```cpp
Vector2 WorldToScreen(Vector2 worldPosition, const Camera2D& camera)
{
    const Vector2 relative{worldPosition - camera.position};
    const Vector2 centered{relative * camera.zoom};
    return centered + camera.viewportSize * 0.5F;
}
```

逆変換はマウス選択、照準、エディタで使います。

```cpp
Vector2 ScreenToWorld(Vector2 screenPosition, const Camera2D& camera)
{
    const Vector2 centered{screenPosition - camera.viewportSize * 0.5F};
    return centered / camera.zoom + camera.position;
}
```

カメラ回転がある場合は逆回転も必要です。一般には行列と逆行列で一貫して扱います。

## 単純追従と問題

```cpp
camera.position = player.position;
```

入力の小さな変化やアニメーション由来の揺れまで画面へ伝わり、酔いやすくなります。カメラの対象には物理中心、足元、専用CameraTargetを使います。

## Smooth Follow

```cpp
const float factor{1.0F - std::exp(-followSharpness * deltaSeconds)};
camera.position = Lerp(camera.position, targetPosition, factor);
```

指数減衰でFPS差を抑えます。単純Lerpは目標へ漸近し続けるため、十分近い場合にスナップする、最大速度を設ける、ばねモデルを使う設計があります。

## Dead Zone

プレイヤーが画面中央の矩形範囲内にいる間はカメラを動かさず、境界を越えた分だけ追従します。細かな往復を抑え、進行方向を見せやすくします。

```text
┌──────── Viewport ────────┐
│                          │
│      ┌─ Dead Zone ─┐     │
│      │   Player    │     │
│      └─────────────┘     │
└──────────────────────────┘
```

## Look Ahead

移動方向、照準、速度に応じてカメラ目標を前方へずらします。方向反転時に即座に反対へ飛ぶと不快なので、Look Ahead自体を平滑化します。空中と地上、戦闘と探索でパラメータを変えることもあります。

## ステージ境界

カメラ表示矩形がステージ外へ出ないよう中心をクランプします。

```cpp
const Vector2 halfView{camera.viewportSize * (0.5F / camera.zoom)};

camera.position.x = std::clamp(
    camera.position.x,
    stageBounds.left + halfView.x,
    stageBounds.right - halfView.x
);
```

ステージがViewportより小さい場合は最小値が最大値を越えるため、中央配置など別処理が必要です。

## Camera Shake

基本追従位置と揺れオフセットを分離します。

```cpp
Vector2 finalCameraPosition{
    followPosition + traumaShakeOffset + scriptedOffset
};
```

毎フレーム完全ランダムにすると高周波で不快になりやすいため、ノイズ、減衰振動、方向性、周波数を使います。強さを「trauma」として加算し二乗して減衰させる方式もあります。UIまで揺らすかをレイヤーで分け、設定で軽減・無効化可能にします。

## 複数ターゲット

協力ゲームやボス戦では、対象群を囲むBoundsから中心と必要Zoomを計算します。Zoomの最小・最大、画面端余白、対象が離れすぎた時のルールが必要です。

## カリング

カメラ外のスプライトを描画コマンドへ入れないことで負荷を下げます。大きさ、回転、エフェクト余白を含むWorld BoundsとCamera Boundsを比較します。更新自体を止めるかは別問題で、画面外の敵AIを止めるとゲームルールが変わります。

## UIカメラ

HUDはWorld Cameraの移動・Zoom・Shakeを通常受けません。World PassとUI Passを分けます。ワールド上のHPバーはWorld座標からScreen座標へ変換してUIとして描く方法があります。
