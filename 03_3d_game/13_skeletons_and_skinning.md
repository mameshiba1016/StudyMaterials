# Skeleton・Bone・Skinning

Skeletonは親子Boneの階層、Skinned Meshは各Vertexを複数Boneの影響で変形します。

## Skeleton

```cpp
struct Bone
{
    std::string name{};
    int parentIndex{-1};
    Transform bindLocal{};
    Matrix4 inverseBindMatrix{};
};
```

親Indexが子より前という順序をImport時に保証すると、前からWorld Poseを計算できます。循環、重複名、無効Parentを検証します。

## Bind Pose

Mesh Vertexが作られた基準姿勢です。Current Bone Worldへ直接Vertexを掛けるのではなく、Inverse BindでBone Local基準へ戻してからCurrent Poseへ移します。

```text
skinMatrix = currentBoneWorld * inverseBindMatrix
```

Matrix Conventionで順序は変わります。

## Linear Blend Skinning

```text
skinnedPosition = Σ weight[i] * (skinMatrix[i] * bindPosition)
```

Weight合計を1へ正規化し、無効Bone IndexをImport時に拒否します。GPUの最大Influence数に合わせ、上位Weightを残して再正規化します。

## Normal/Tangent Skinning

Positionの`w=1`に対し方向は`w=0`です。Boneへ非一様Scaleがある場合Normal Matrixが必要です。通常Skeletonで非一様Scaleを制限することがあります。Skin後にNormal/Tangentを正規化します。

## Matrix Palette

各CharacterのSkin Matrix配列をConstant/Structured BufferへUploadします。Bone数×Character数のBandwidthがかかります。

- Visible Characterだけ更新。
- Pose共有。
- Bone LOD。
- Texture/Buffer Palette。
- GPU Animation。

を検討します。

## CPUとGPU Skinning

- CPU：結果VertexをCPUで生成。Collision/特殊処理へ使いやすいがBandwidthが大きい。
- GPU Vertex Shader：一般的。CPUはPaletteだけUpload。
- Compute Skinning：一度変形し複数Passで再利用、Memory/同期が必要。

Shadow、Depth、Main Passで同じMeshを描く場合、Compute結果再利用が有利なことがあります。

## Dual Quaternion Skinning

Linear Blendの肘・ねじりVolume損失を改善しますが、Scale対応やCostが異なります。Corrective ShapeやTwist Boneも使われます。

## Socket

WeaponやEffectをBoneへAttachします。

```text
socketWorld = characterWorld * boneComponentPose * socketOffset
```

どの空間のPoseかを明確にします。描画補間PoseとGameplay固定Poseのずれも考慮します。

## Skeleton Retargeting

異なる体格・Bone長へAnimationを移します。Bone Mapping、Reference Pose、Translation Scale、Root、Twist Boneを処理します。名前一致だけでは十分でありません。

## Bounds

Skinned MeshのStatic Bind BoundsではAnimation中の手足が外へ出て誤Cullingされます。

- 全ClipからBoundsをBake。
- Bone Sphereを合成。
- CPU/GPUで動的更新。
- 安全Margin。

用途とCostを選びます。

## Gameplay Collision

Rendered Vertex全体で毎Frame精密衝突せず、BoneへCapsule/Sphere/Boxを付けます。Hit Box用BoneとPhysics Assetを調整し、Debug表示します。
