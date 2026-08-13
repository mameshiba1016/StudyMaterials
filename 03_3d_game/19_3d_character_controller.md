# 3D Character Controller

高速アクションのPlayerは、完全なDynamic Rigid Bodyより専用Kinematic Controllerで制御することが多くあります。入力応答、Slope、Step、Moving Platform、Root Motionを明示的に扱えるためです。

## 入力からWorld方向へ

CameraのForward/Rightを地面Planeへ射影します。

```cpp
Vector3 cameraForwardPlanar{NormalizeOrZero(ProjectOnPlane(camera.forward, worldUp))};
Vector3 cameraRightPlanar{NormalizeOrZero(Cross(worldUp, cameraForwardPlanar))};

Vector3 desiredDirection{
    NormalizeOrZero(cameraRightPlanar * input.x + cameraForwardPlanar * input.y)
};
```

斜め入力を長さ1へ制限します。Cameraが真上を向く等、射影がゼロになる場合は前回基底を使います。

## 速度制御

```cpp
Vector3 targetVelocity{desiredDirection * maximumSpeed};
planarVelocity = MoveToward(planarVelocity, targetVelocity, acceleration * fixedDelta);
verticalVelocity += gravity * fixedDelta;
```

地上加速、減速、空中制御、攻撃中倍率、Lock-on Strafeを分けます。

## Capsule Cast移動

目標変位へCapsuleをCastし、最初のHitまで進み、残り変位をSurfaceへSlideさせます。

```text
remaining = desiredDisplacement
repeat maxIterations:
  hit = CapsuleCast(position, remaining)
  hitなし → remaining全て移動して終了
  hitあり → skin手前まで移動
           remainingをhit.normalのPlaneへ射影
```

Iteration上限、Skin Width、非常に短い変位を設定し、角で無限反復しないようにします。

## Slope

```cpp
const float upDot{Dot(hit.normal, worldUp)};
const bool isWalkable{upDot >= std::cos(maxSlopeRadians)};
```

歩行可能Slopeでは移動を面へ投影し、急Slopeは壁としてSlideまたは滑落します。接地判定と壁判定を同じNormal閾値で統一します。

## Step Offset

低い段差へは、上へ試行→前へ試行→下へ接地の三段階で登ります。頭上空間、段差高さ、着地Slopeを検査します。単純にYを加算すると天井・壁を貫通します。

## Ground Probe

足元へ短いSphere/Capsule Castを行い、Ground Entity、Point、Normal、Distance、Velocityを取得します。瞬間的に地面を失ってもCoyote Timeを使えます。

## Moving Platform

前tickのPlatform Transformから接地点の変位・回転を計算し、Characterへ先に適用します。Platformを安定IDで保持し、破棄・Teleport時に解除します。

## Root Motion

Animationから抽出したDeltaを入力移動と合成し、最終的にControllerのCastへ通します。

```text
animationDelta + gameplayDelta + platformDelta
→ collision-constrained movement
```

実際に進めなかった距離をAnimationへどう反映するかを決めます。

## Dodge

Dodge開始時に方向を確定し、速度CurveまたはRoot Motionで移動します。開始直後の方向変更可否、段差落下、壁衝突、無敵WindowはAction Stateで管理します。

## Teleport

位置だけ変更せず、Ground Cache、速度、Platform、接触、Root Motion残量、Camera履歴をResetします。

## Debug

Capsule、Cast、Ground Probe、Normal、Walkable判定、Step試行、速度、Root Motion Deltaを表示します。
