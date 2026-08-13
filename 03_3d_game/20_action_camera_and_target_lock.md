# 3D Action Camera・Target Lock

Action CameraはPlayer、Target、障害物、入力を同時に扱います。Cameraの見やすさは戦闘Systemの一部です。

## Rig分離

```text
Follow Target：Player上の基準点
Pivot        ：Yaw/Pitch回転中心
Boom         ：Camera距離
Camera       ：FOV、Shake、Collision補正
```

Player Modelの揺れるBoneを直接Followせず、平滑化したGameplay Targetを使います。

## Free Camera

Mouse/Stick入力をYaw/Pitchへ積算します。PitchをClampし、Stick dead zoneと応答Curveを適用します。Frame依存の固定Lerpではなく指数減衰で追従します。

## Camera Collision

Pivotから理想Camera位置へSphere Castし、障害物手前へCameraを寄せます。

```text
idealDistance → SphereCast → collisionDistance
```

近づく時は速く、戻る時は遅くすると壁通過を防ぎつつ急なZoom Outを抑えられます。Player自身と透明化対象をFilterします。

## Target候補

```cpp
struct TargetCandidate
{
    EntityId id{};
    float screenDistance{};
    float worldDistance{};
    float viewDot{};
    bool hasLineOfSight{};
};
```

生存、Targetable、距離、Camera前方、Screen内、遮蔽を検査します。

## Score

```text
score = screenCenterWeight * screenDistance
      + distanceWeight * normalizedDistance
      + angleWeight * anglePenalty
      + occlusionPenalty
```

単純な最近距離だけでは画面外の敵を選びます。各項をDebug表示します。

## Lock状態

Lock中はPlayer-to-Target方向を基準にCameraと移動を制御します。

- 入力前後：Targetへ接近・離脱。
- 入力左右：Strafe。
- Character Facing：Target方向へ追従。
- Camera：PlayerとTargetをFraming。

攻撃中のFacing追尾量はAttack Definitionで制御します。

## Target切替

Stick Flick方向をScreen Spaceで評価し、現在Targetの左右・上下にある候補から選びます。入力を一度ニュートラルへ戻すHysteresisを設け、連続切替を防ぎます。

## Lock解除

- Target死亡・破棄。
- 最大距離超過。
- 長時間遮蔽。
- User入力。
- Cutscene/Scene遷移。

破棄済み生ポインタを保持せず、世代付きHandleで毎tick有効性を確認します。

## 複数対象Framing

PlayerとTargetの中点をCamera注視点にし、Screen上の余白へ収まる距離/FOVを求めます。ただしTargetが急移動してCameraが激しく振られないよう速度制限します。

## Aim Assistとの違い

Target Lock、Camera Assist、Attack Homing、Projectile Aimは別機能です。それぞれ対象選択と強度を持ち、勝手に同じTargetへ結合しません。

## Camera Shake

Base Rig結果へPosition/Rotation Noiseを加えます。Hit方向、Attack強度、距離に応じ、UIとGameplay RayにはShakeなしMatrixを使います。揺れ軽減設定を用意します。

## Occlusion

CameraとPlayer間の壁だけでなく、Targetを隠す柱も扱います。Camera Collision、Object Fade、Camera Side切替を使います。透明化Materialの復帰と複数Cameraを管理します。

## Debug

候補、Score、Line of Sight、理想/補正Camera位置、Sphere Cast、Screen Framing、現在Lock Handleを表示します。
