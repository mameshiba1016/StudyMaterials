# DirectX 11：Model・Mesh・Animation

この章では、Model FileをRuntime用Mesh、Material、Skeleton、Animationへ変換し、Direct3D 11で描画する設計を学びます。Import、座標系正規化、Submesh、Skinning、Clip Sampling、Blend、Root Motion、Event、Bounds、LOD、非同期Loadまでを扱います。

## 1. Model Assetの構成

```text
scene nodes
meshes / primitives
materials / textures
skeleton / skins
animation clips
metadata
```

一つのFileが一つのVertex Bufferだけとは限りません。

## 2. Source形式とRuntime形式

FBX、glTF等のAuthoring/Exchange形式をGame実行中に毎回複雑解析するより、Build工程で検証済みRuntime形式へ変換します。

## 3. Import Pipeline

```text
read source
-> validate
-> normalize coordinates and units
-> build vertices/indices
-> generate/validate normals and tangents
-> build materials/skeleton/clips
-> optimize
-> serialize runtime asset
```

## 4. 座標系正規化

Left/Right handedness、Up/Forward軸、Meter単位、Winding、UV V方向をProject規約へ変換します。

## 5. Node

Nodeは名前、Local Transform、Parent、Children、Mesh参照等を持つ階層要素です。Meshを持たないTransform Nodeもあります。

## 6. Node Index

RuntimeではPointerでなく連続IndexとParent Indexを使うとSerialize、Cache、Job処理が容易です。Cycleを禁止します。

## 7. LocalとGlobal Transform

Node LocalはParent基準、GlobalはRootからの合成です。Parentを先に評価して子へ伝播します。

## 8. Mesh

Geometry Dataの集合です。Vertex Stream、Index Buffer、Submesh、Bounds、Skinning情報を持ちます。

## 9. Submesh

同じMaterial、Topology、Vertex/Index Rangeで描ける単位です。

```cpp
struct Submesh
{
    UINT indexCount;
    UINT startIndex;
    INT baseVertex;
    UINT materialIndex;
};
```

## 10. Primitive変換

SourceがTriangle Strip/Fan等でも、Runtime規約のTriangle Listへ変換できます。WindingとRestart Indexを正しく処理します。

## 11. 頂点統合

Positionだけで統合せず、Normal、Tangent、UV、Color、Bone Index/Weightの組が同じCornerだけ共有します。

## 12. Index検証

全IndexがVertex Count未満、Triangle ListならCountが3の倍数、16-bit化するなら最大Indexが範囲内であることを確認します。

## 13. NormalとTangent

Source Dataを検証し、不足時はSmoothing/UVを基に生成します。Mirror UVにはTangent Handednessが必要です。

## 14. Material Mapping

SubmeshからMaterial Index、MaterialからTexture/Factor/Blend/Cull設定へ解決します。欠損参照はFallbackへ置換します。

## 15. Bounds

Mesh/Node/SubmeshごとにAABBやSphereを作り、Frustum Culling、LOD、Camera Focusへ使います。

## 16. Animated Bounds

Bind Pose BoundsだけではAnimation中の手足や武器が外へ出ます。Clip全体から保守的BoundsをBakeするか、Bone BoundsからRuntime更新します。

## 17. Skeleton

Skinningへ使うBone/Joint階層です。Node階層全体とSkin Joint配列は同じとは限りません。

## 18. Joint Data

```cpp
struct Joint
{
    std::string name;
    int parentIndex;
    XMFLOAT4X4 inverseBind;
    Transform bindLocal;
};
```

## 19. Bind Pose

MeshがSkeletonへBindingされた基準姿勢です。現在Poseとの差から頂点を変形します。

## 20. Inverse Bind Matrix

Bind時のJoint空間へ頂点を戻す変換です。現在Joint Globalと組み合わせてSkin Matrixを作ります。

## 21. Skin Matrix規約

```text
skin matrix = inverse bind * current joint global
```

これはRow Vector規約の代表例です。Source形式とEngineのMatrix方向に合わせ、既知Poseで検証します。

## 22. Bone IndexとWeight

各頂点は影響Joint IndexとWeightを持ちます。Index範囲、負Weight、NaNを拒否します。

## 23. Weight正規化

採用Influenceの合計を1へ正規化します。合計0ならFallback JointへWeight 1を設定します。

## 24. Influence数制限

上位4または8 Weightだけ残す等、Vertex FormatとShader Costに合わせます。捨てた後に再正規化します。

## 25. GPU Skinning

Vertex ShaderがBone PaletteからMatrixを読み、Weight付きでPosition/Normal/Tangentを合成します。

## 26. Position Skinning

```hlsl
float4 skinnedPosition = 0;
for (uint i = 0; i < 4; ++i)
{
    skinnedPosition += weight[i] *
        mul(float4(position, 1), bones[index[i]]);
}
```

## 27. Normal Skinning

w=0としてBone変換し、合成後Normalizeします。Boneに非一様Scaleを許す場合はNormal Matrix規約が必要です。

## 28. Bone Palette Upload

少数BoneはConstant Buffer、多数・複数CharacterはStructured Buffer/Texture等を検討します。Constant Buffer 64 KiB上限を考慮します。

## 29. Skeleton Palette Remap

Submeshが使うJointだけを局所Paletteへ詰めるとUpload/Shader範囲を減らせます。Vertex Bone IndexをRemapします。

## 30. CPU Skinning

Collision、特殊Deformation、低Feature Pathで使えますが、毎Frame全頂点更新Costが大きくなります。必要頂点/Boneだけ評価します。

## 31. Animation Clip

名前、Duration、Sample Rate、Node/Joint Channel、Event等を持つ時間Dataです。

## 32. ChannelとTrack

JointごとにTranslation、Rotation、Scale Keyを持ちます。欠けたTrackはBind/Default値を使います。

## 33. Keyframe Sampling

現在時刻を挟む二Keyを見つけ、Translation/ScaleはLerp、RotationはQuaternion Slerp/Nlerpで補間します。

## 34. Clip Time

Loop ClipはDurationでWrapし、One-shotは終端へClampします。Duration 0や負Playback Rateを安全に扱います。

## 35. Key検索

時間が前進する通常再生では前回Indexから進め、Seek/逆再生ではBinary Search等を使います。

## 36. Pose

全JointのLocal Transform配列です。Clip Sampling、Blend、IKをLocal Poseへ適用後、Global Poseを階層評価します。

## 37. Pose Blend

二PoseのTranslation/ScaleをLerp、Rotationを短経路補間します。Weight 0/1の端値を正確に扱います。

## 38. Cross-fade

IdleからRun等の遷移でSource/Target Poseを時間とともにBlendします。遷移中のClip時刻も進めます。

## 39. Animation State Machine

State、Transition条件、Blend時間、Exit Time、PriorityをDataとして管理します。Gameplay StateとAnimation見た目を疎結合にします。

## 40. Blend Tree

速度や方向ParameterからIdle/Walk/Run/Strafe Clipを連続Blendします。Parameter範囲外とClip位相を処理します。

## 41. Phase同期

Walk/Runの足接地位相を合わせてBlendし、足滑りを減らします。

## 42. Additive Animation

基準Poseとの差分を既存Poseへ加えます。Aim Offset、Recoil、Hit Reaction、Breathingに使います。

## 43. Bone Mask

上半身だけAttack、下半身はRunなど、JointごとのBlend Weightを使います。Hierarchy境界を滑らかにします。

## 44. Root Motion

Root Boneの移動/回転DeltaをGameplay Characterへ適用します。Visual Rootから抽出後、Pose側から消費する規約を定めます。

## 45. In-place Animation

Clipは原点付近で足だけ動き、Gameplay VelocityがCharacterを移動します。Root Motion方式と混在規約を明示します。

## 46. Root MotionとCollision

Animation希望移動をCharacter Controllerへ渡し、Collision後の実移動との差をVisual補正します。Animationが壁を貫通して位置を確定しません。

## 47. Animation Event

Footstep、Attack判定開始、Effect、音等の時刻Markerです。低FPSで時刻を飛び越えても区間内Eventを発火します。

## 48. Loop境界Event

前時刻からDuration、0から現在時刻の二区間を検査します。同一Eventの二重発火を防ぎます。

## 49. Attack判定

Animation EventだけにGame Ruleを完全依存させず、AuthoritativeなCombat TimelineとVisual Clip同期を設計します。

## 50. Socket

Hand、Weapon、Head等のJoint/NodeへEffectやWeaponをAttachします。Joint GlobalとSocket Local Offsetを合成します。

## 51. IK

Footを地面へ合わせる、手をWeaponへ合わせる等、Sample/Blend後のPoseをConstraintで補正します。

## 52. 評価順序

```text
sample clips
-> state/blend tree
-> additive and masks
-> root motion extraction
-> IK/constraints
-> global pose
-> skin palette
```

## 53. Animation LOD

遠距離Characterは更新頻度、Bone数、IK、Facial Animationを減らせます。Camera近傍へ戻る際のPose飛びを防ぎます。

## 54. Mesh LOD

LODごとにGeometry、Material、Bone Influence/Paletteを変えます。Skeleton互換性とCross-fadeを管理します。

## 55. GPU Resource

Runtime MeshはVertex/Index Buffer、Submesh、Material参照、Boundsを所有します。CPU Source Dataを保持するか再Load可能にします。

## 56. 非同期Load

WorkerでFile/Decode/Validation、Render側でGPU Resource作成、完了後にAtomicなAsset公開を行います。

## 57. Fallback

Load中/失敗時はError Mesh、Fallback Material、Bind Poseを使い、Null Resourceで描画をCrashさせません。

## 58. Asset Version

Runtime形式へMagic、Version、Endianness、Section Size、Hashを持たせ、互換でないDataを明確に拒否します。

## 59. よくある失敗：NodeとJointを同一視

JointでないNodeや複数Mesh Nodeが失われます。Scene Node、Skin Joint、Meshを別Tableにします。

## 60. よくある失敗：Inverse Bind順が逆

Bind PoseですでにMeshが崩れます。Bind Poseを評価したSkin MatrixがIdentity付近になるTestを作ります。

## 61. よくある失敗：Weight合計が1でない

頂点が縮む/膨らむ/原点へ寄ります。Import時に検証・正規化します。

## 62. よくある失敗：Animation Event取り逃し

現在時刻とEvent時刻の完全一致だけを見ます。前回から現在までの区間を検査します。

## 63. Import Test

- 座標軸、単位、Winding、UV方向を確認する。
- Index/Material/Texture参照を検証する。
- Bind PoseでSourceと一致する。
- Mirror UV Tangentを確認する。
- 壊れたSection/巨大Countを拒否する。

## 64. Skinning Test

- Joint一つWeight 1で剛体変換になる。
- 複数Weightの既知結果をCPU/GPU比較する。
- Normal/TangentがUnit付近になる。
- 最大Joint Indexを検証する。
- Bind Pose Skin MatrixがIdentity付近になる。

## 65. Animation Test

- Clip開始/終了/Loop境界をSampleする。
- Translation/Scale/Rotation補間を確認する。
- Cross-fade端値がSource/Targetと一致する。
- Root MotionとCollisionを確認する。
- Eventを低FPS/逆再生/Loopで検証する。

## 66. 完成確認表

- [ ] Source形式とRuntime形式を分離できる。
- [ ] Node、Mesh、Submesh、Materialを区別できる。
- [ ] 座標系と単位をImport時に正規化できる。
- [ ] Skeleton、Bind Pose、Inverse Bindを説明できる。
- [ ] Weightを制限・正規化できる。
- [ ] GPU Skinning Paletteを作成できる。
- [ ] ClipをKeyframe間でSampleできる。
- [ ] Cross-fade、Additive、Bone Maskを扱える。
- [ ] Root Motion、Event、IKの評価順を説明できる。
- [ ] Bounds、LOD、非同期Load、Fallbackを設計できる。

## 67. この章の要点

- Model AssetはNode、Mesh、Material、Skeleton、Animationの集合です。
- Sourceを検証・正規化したRuntime形式へ変換します。
- Skin MatrixはInverse Bindと現在Joint Globalを規約どおり合成します。
- Bone Weightは範囲検査、Influence制限、再正規化が必要です。
- PoseはLocal Joint Transform配列としてSample・Blend後に階層評価します。
- Root Motion、Event、IKは明確な評価順とGameplay境界を持ちます。
- Animated Bounds、LOD、非同期Resource公開が多数Characterの安定性を支えます。
- Bind Poseと既知AnimationをCPU/GPU自動Testします。

## 68. 公式資料

- [ID3D11DeviceContext::DrawIndexed](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11devicecontext-drawindexed)
- [ID3D11DeviceContext::VSSetConstantBuffers](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11devicecontext-vssetconstantbuffers)
- [HLSL data types](https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/dx-graphics-hlsl-data-types)
- [XMMatrixDecompose](https://learn.microsoft.com/en-us/windows/win32/api/directxmath/nf-directxmath-xmmatrixdecompose)
- [XMQuaternionSlerp](https://learn.microsoft.com/en-us/windows/win32/api/directxmath/nf-directxmath-xmquaternionslerp)

次章では、Light視点のDepthを作成し、Camera描画で比較するShadow Mappingを扱います。
