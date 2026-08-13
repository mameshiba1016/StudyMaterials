# Animation Clip・Pose・Blend・Root Motion

Animation ClipはBoneごとのTranslation、Rotation、Scale Curveと時間範囲を持ちます。Runtimeは指定時刻でPoseをSampleし、複数PoseをBlendして最終Skeletonを作ります。

## Keyframe Sampling

時刻の前後Keyを探し補間します。

- Translation/Scale：Lerp。
- Rotation：Nlerp/Slerp。

Key時刻がSortedか、重複、Clip範囲外、Loop境界をImport時に検証します。Binary Search、前回Index Cache、Uniform Samplingで高速化します。

## Local Pose

```cpp
struct LocalPose
{
    std::span<Vector3> translations;
    std::span<Quaternion> rotations;
    std::span<Vector3> scales;
};
```

Bone数が一致し、Skeletonとの対応Indexが必要です。Animation Scratch BufferをFrame Allocatorから確保します。

## Pose Blend

```text
T = lerp(Ta, Tb, weight)
R = nlerp(Ra, Rb, weight)
S = lerp(Sa, Sb, weight)
```

Weightを0～1へ管理し、Quaternion最短経路を選びます。

## Cross Fade

現在Clipから次Clipへ一定時間でWeightを変えます。遷移中にさらに遷移要求が来た場合、現在のBlend結果をSnapshotにするか、Source/Targetを組み替えるかを設計します。

## Additive Animation

Base Poseとの差分を別Poseへ加えます。呼吸、Recoil、Aim Offsetに使います。

```text
translationResult = base + additiveTranslation * weight
rotationResult = weightedAdditiveRotation * baseRotation
```

Local/Model Space Additive、Reference Pose、Quaternion順序を統一します。

## Bone Mask

上半身だけAttack、下半身はRunのようにBoneごとのWeightを使います。階層へWeightを伝播し、肩境界を滑らかにします。

## Root Motion

Root BoneのAnimation移動量をCharacter Worldへ適用します。

```text
deltaRoot = inverse(previousRoot) * currentRoot
```

Loop境界、Blend中、Teleport、Network、Collisionによる未達移動を処理します。Animation RootをPoseから除去しないと二重移動になります。

## In-place

Animationはその場で動き、Gameplay VelocityがCharacterを動かします。操作制御とNetworkに向きますが、足滑りが出ます。Stride Warping、Play Rate調整、Motion Matching等で改善できます。

## Animation Event

Footstep、Hit開始、VFX等をClip時刻へ置きます。前回時刻から今回時刻までに跨いだ全Eventを処理し、Loop、逆再生、大deltaを扱います。

Gameplayの重要判定をEventだけへ依存すると、BlendやLODで失われる危険があります。Combat tickと同期し、Eventは通知・演出中心にします。

## Sync Group

Walk/Runを足接地Phaseで同期し、Blend時の足滑りを減らします。正規化時刻だけでなくMarker（LeftFootDown等）を使います。

## Compression

AnimationはKey削減、量子化、Quaternion圧縮を行います。Error ThresholdをBone重要度で変えます。FingerとRootでは許容誤差が違います。Decode CostとMemoryを測ります。

## LOD

遠距離CharacterはAnimation更新頻度、Bone数、IK、Morphを減らします。更新を間引いたPoseは描画時補間します。Camera外停止後の復帰時刻を同期します。

## Debug

Skeleton、Local/Component軸、Root軌跡、Blend Weight、Clip時刻、Event、Sync Marker、Bone Maskを表示します。一時停止と一Frame送りを用意します。
