# 第16章 MV1 Animation・Blend

Animation Systemは画像を順番に見せる処理ではありません。Clip時間、Pose、遷移、Layer、Event、Root Motionを管理し、Combat StateとModel表示を同期する仕組みです。MV1ではAnimationをModelへAttachし、時間とBlend率を明示的に設定します。

## 1. 用語

- Clip: 一つのAnimation Data。
- Pose: 特定時刻の全Bone姿勢。
- Animator: Clip再生状態を管理するInstance。
- Transition: Clip間の遷移。
- Layer: 複数Poseの合成単位。
- Root Motion: Root Boneの移動をGameplayへ適用する方式。

## 2. Animation一覧

```cpp
const int count = MV1GetAnimNum(modelHandle);
if (count == -1) return;

for (int i = 0; i < count; ++i)
{
    const char* name = MV1GetAnimName(modelHandle, i);
    const float total = MV1GetAnimTotalTime(modelHandle, i);
    // name==nullptr、total==-1.0FはErrorとして扱う。
}
```

## 3. 名前からIndex

```cpp
const int runIndex = MV1GetAnimIndex(modelHandle, "Run");
if (runIndex == -1)
{
    // 必須Clip欠落。Asset Validation Error。
}
```

毎Frame検索せずPrototype生成時にCacheします。

## 4. Animation Metadata

```cpp
struct AnimationClipInfo final
{
    std::string name{};
    int mv1Index = -1;
    float totalTime = 0.0F; // MV1の時間単位。
    bool loop = false;
    float playbackRate = 1.0F;
};
```

MV1の時間値を秒と決めつけず、Asset/Tool側との換算を一か所へ集めます。

## 5. Attach

```cpp
const int attach = MV1AttachAnim(modelHandle, animIndex, -1, FALSE);
if (attach == -1)
{
    // Model、AnimIndex、Source Model、NameCheckをLog。
}
```

戻り値はAttach番号です。Animation Indexとは別物です。

## 6. 外部ModelのAnimation

`AnimSrcMHandle`へ別Modelを渡すと、そのModelが持つAnimationをAttachできます。Frame名不一致を許可するか`NameCheck`で指定します。Skeleton互換性をAsset Build時に検証します。

## 7. Attach番号の型

```cpp
struct AnimAttachment final
{
    int modelHandle = -1;
    int attachIndex = -1;

    [[nodiscard]] bool IsValid() const noexcept
    { return modelHandle != -1 && attachIndex != -1; }
};
```

Attach番号はModelとの組です。別Modelへ流用しません。

## 8. Detach

```cpp
if (attachment.IsValid())
{
    MV1DetachAnim(attachment.modelHandle, attachment.attachIndex);
    attachment = {};
}
```

公式は不要AnimationをDetachする利用を想定しています。Model削除時は自動Detachされますが、通常運用では不要Slotを早く解放します。

## 9. Attach RAII

```cpp
class UniqueAnimAttachment final
{
public:
    ~UniqueAnimAttachment() { Reset(); }
    UniqueAnimAttachment(const UniqueAnimAttachment&) = delete;
    UniqueAnimAttachment& operator=(const UniqueAnimAttachment&) = delete;

    void Reset() noexcept
    {
        if (model_ != -1 && attach_ != -1)
            MV1DetachAnim(model_, attach_);
        model_ = attach_ = -1;
    }

private:
    int model_ = -1;
    int attach_ = -1;
};
```

Modelより先にAttachmentを破棄します。Model削除後にDetachしない所有順が必要です。

## 10. 時間は自動で進まない

DXライブラリ公式仕様ではAnimationを自動再生しません。Game側が時間を進め、毎更新設定します。

```cpp
playTime += deltaSeconds * mv1UnitsPerSecond * playbackRate;
MV1SetAttachAnimTime(modelHandle, attachIndex, playTime);
```

## 11. 総時間

```cpp
const float total = MV1GetAttachAnimTotalTime(modelHandle, attachIndex);
if (total <= 0.0F)
{
    // 不正ClipによるLoop計算不能。
}
```

## 12. Loop

```cpp
[[nodiscard]] float WrapAnimationTime(float time, float total)
{
    if (total <= 0.0F) return 0.0F;
    float wrapped = std::fmod(time, total);
    if (wrapped < 0.0F) wrapped += total;
    return wrapped;
}
```

大Deltaでも余りを保ちます。単に0へ戻すと超過分が失われます。

## 13. Non-loop

```cpp
playTime = std::clamp(playTime, 0.0F, totalTime);
finished = playTime >= totalTime;
```

最終Poseを保持するか、Idleへ遷移するかをStateで決めます。

## 14. Normalized Time

```cpp
[[nodiscard]] float NormalizedTime(float time, float total)
{
    return total > 0.0F ? time / total : 0.0F;
}
```

0～1で扱えば異なるClip長でもEventやTransition条件を表現できます。Loop回数は別に保持します。

## 15. Playback Rate

Animation速度を変えても、攻撃判定や移動速度をどう同期するか決めます。表示だけ倍速にしてCombat時間が固定だとHit Frameがずれます。

## 16. Time Source

- Game Time: Pause/Hit Stopで止まる。
- Unscaled Time: UIや一部演出。
- Fixed Time: Gameplay同期を重視。

Animation用途ごとに選びます。

## 17. Blend率

```cpp
MV1SetAttachAnimBlendRate(modelHandle, idleAttach, 1.0F - t);
MV1SetAttachAnimBlendRate(modelHandle, runAttach, t);
```

公式のBlend率範囲は0～1です。`t`をClampします。

## 18. Crossfade

旧Clipと新Clipを同時Attachし、一定時間でWeightを入れ替えます。

```cpp
struct Crossfade final
{
    int fromAttach = -1;
    int toAttach = -1;
    float elapsed = 0.0F;
    float duration = 0.15F;
};
```

## 19. Crossfade更新

```cpp
void UpdateCrossfade(Crossfade& fade, float delta, int model)
{
    fade.elapsed += delta;
    const float t = fade.duration > 0.0F
        ? std::clamp(fade.elapsed / fade.duration, 0.0F, 1.0F)
        : 1.0F;

    MV1SetAttachAnimBlendRate(model, fade.fromAttach, 1.0F - t);
    MV1SetAttachAnimBlendRate(model, fade.toAttach, t);
}
```

完了後は旧AttachをDetachします。

## 20. 遷移中の再遷移

Crossfade途中に別Stateへ移る場合があります。現在見えているPoseをどう引き継ぐかが課題です。

- 現在の主要Clipから新Clipへ遷移。
- 複数Weightを維持し新Clipを追加。
- Snapshot Poseを経由。

最初は「最大Weight Clipをfrom」にする等、明確なPolicyを選びます。

## 21. 全Clipを常時Attachしない

公式資料は全AnimationをAttachし続けるとMemory・処理負荷が増える旨を示しています。現在・遷移先・必要LayerだけをAttachします。

## 22. Animator Slot

```cpp
struct AnimationSlot final
{
    int attachIndex = -1;
    int clipIndex = -1;
    float time = 0.0F;
    float totalTime = 0.0F;
    float weight = 0.0F;
    float speed = 1.0F;
    bool loop = false;
};
```

## 23. State Machine

```cpp
enum class LocomotionState
{
    Idle,
    Walk,
    Run,
    Start,
    Stop,
    Turn
};
```

StateはClip名そのものではありません。一Stateが速度に応じ複数ClipをBlendできます。

## 24. Transition Rule

```cpp
struct TransitionRule final
{
    LocomotionState from{};
    LocomotionState to{};
    float blendSeconds = 0.15F;
    bool canInterrupt = true;
};
```

条件、優先度、Exit Time、InterruptをData化します。

## 25. Locomotion Blend

速度0でIdle、低速でWalk、高速でRunをBlendします。Animation速度と実移動速度の足滑りを合わせます。

```cpp
const float runWeight = std::clamp(
    (speed - walkSpeed) / (runSpeed - walkSpeed), 0.0F, 1.0F);
```

## 26. Phase同期

WalkとRunの左足接地TimingをNormalized Timeで合わせるとCrossfade中の足滑りが減ります。Clip MarkerでLeftFoot/RightFoot位相を合わせる方法もあります。

## 27. Additive Animation

基準Poseとの差分をBase Poseへ加える方式です。Recoil、呼吸、被Damage揺れに向きます。MV1 Blendがどの合成方式かを確認し、必要ならFrame User Matrix等で補正します。

## 28. Layer

```text
Base Layer: Locomotion全身
Upper Body Layer: 攻撃・Aim
Additive Layer: Recoil・Damage
Procedural Layer: Look-at・IK
```

下から上へ適用順を固定します。

## 29. Bone Mask

上半身攻撃ならSpine以下へWeight、脚へ0を適用します。MV1 APIで必要なBone単位制御が不足する場合、Frame Matrix制御またはAsset側Animation分割を検討します。

## 30. Maskの境界

Spineだけ急にWeight1、Pelvisが0だと境界が不自然です。階層に沿ってWeightを徐々に変えるMaskを使えます。

## 31. Animation Event

```cpp
enum class AnimEventType
{
    Footstep,
    AttackStart,
    HitboxOn,
    HitboxOff,
    SpawnEffect,
    Voice
};

struct AnimEvent final
{
    float normalizedTime = 0.0F;
    AnimEventType type{};
    std::string payload{};
};
```

## 32. Event通過判定

前時間から現在時間の間に通過したEventを発行します。大Deltaで複数Eventを飛び越えても全て拾います。Loop境界では`previous→total`と`0→current`へ分けます。

## 33. Eventは描画から発行しない

Modelが画面外でDrawされなくても足音やHitbox Ruleが必要な場合があります。Animator Updateで発行し、Drawは結果Poseを表示するだけにします。

## 34. Eventの一意性

同じAnimation時間を複数回評価してEventを重複発行しないよう、Loop回数・Event Index・Attack Instance IDを記録します。

## 35. Combat Timelineとの関係

Animation EventだけをDamage Ruleの唯一の正にすると、Clip差替えでGameplayが変わります。

- Combat Dataを正、Animationを追従。
- 共通Timelineから両者を駆動。
- Event契約を検証して同期。

どれを選ぶか明示します。

## 36. Cancel Window

Animatorが「見た目が終わった」だけでStateをCancel可能にしません。Combat StateのCancel WindowとAnimation Transitionを連携します。

## 37. Hit Stop

Hit Stop中は攻撃Animation時間を止めるか、非常に遅くします。VFX、Camera、UIは別Time Sourceを選べます。同じEventを再発行しません。

## 38. Root Motion

Root Boneの移動量をCharacterのWorld移動へ使います。Animation前時刻と現在時刻のRoot位置差を得てDeltaを計算します。

## 39. Root Delta

```cpp
struct RootMotionDelta final
{
    Vec3 translation{};
    float yawRadians = 0.0F;
};
```

絶対Root位置を毎Frame加算せず、差分だけを適用します。

## 40. Root Motion Loop

Loop境界では終端Rootと開始Rootの差を正しく扱います。単純に時間を0へ戻すと大きな逆移動が出る場合があります。

## 41. In-place Clip

Rootがその場に留まるClipはGameplay速度でCharacterを動かします。足滑りを減らすためAnimation速度を移動速度へ合わせます。

## 42. Root MotionとCollision

Animationが要求したDeltaをそのまま適用せずCharacter Controllerへ渡し、壁・床・Slopeで解決した実DeltaをModelへ反映します。

## 43. Warping

Attack終端をTarget位置へ合わせるためRoot Motionを拡大・回転補正します。許容範囲を超えるWarpは不自然なので、攻撃選択段階で距離を制限します。

## 44. Frame Local Position

`MV1GetAttachAnimFrameLocalPosition`でAttach Animationの指定Frame Local位置を取得できます。Root Motion抽出や軌跡解析へ利用できます。Local/Worldの違いを明記します。

## 45. Bone Socket更新

Pose時間を設定した後にFrame World Matrixを取得し、Weapon、Hitbox、VFXを同期します。順序が逆なら1Frame遅れます。

## 46. Physicsとの順序

公式資料ではPosition、Rotation、Attach、Animation時間設定後にリアルタイム物理計算を行う必要がある場合があります。

```text
Transform設定
→ Animation Attach/Time/Blend
→ Procedural補正
→ MV1 Physics
→ Bone/Socket取得
→ Draw
```

利用機能の公式契約を確認します。

## 47. Animation Source Model寿命

別ModelからAnimationをAttachする場合、Source Model基礎Dataが必要な期間を確認し、Attach先より先に破棄しません。Prototype Dependencyとして所有します。

## 48. Clip ID

Game Codeへ文字列を散らさず、論理IDからAsset固有Indexへ解決します。

```cpp
enum class AnimationId { Idle, Walk, Run, Attack1, Dodge, Hit, Down };
```

## 49. Animator Parameter

```cpp
struct AnimatorParameters final
{
    float movementSpeed = 0.0F;
    bool grounded = true;
    bool attacking = false;
    bool hit = false;
};
```

AnimatorはGameplay状態のSnapshotを読み、Gameplay Object自体を所有しません。

## 50. Trigger Parameter

`hit=true`を放置すると毎Frame遷移するため、Triggerは一回消費型にします。QueueやSequence番号で取りこぼしと重複を防ぎます。

## 51. Animator更新順

```text
Gameplay State確定
→ Animator Parameters作成
→ Transition評価
→ Clip時間更新
→ Event抽出
→ Blend率設定
→ Root Motion抽出
→ Character Controller解決
→ Model Transform/Pose再適用
```

循環依存を避けます。

## 52. FixedとVariable Update

Gameplay Event/Root MotionはFixed Update、描画Poseは補間時間で評価する方式があります。同じAttachへ異なる時間を何度も設定する場合、Eventは一度だけSimulation側で発行します。

## 53. Pose補間

固定更新の前Poseと現在Poseを描画時に補間できれば滑らかですが、MV1のAttach時間設定へRender用時刻を渡すとGameplay Bone取得と混ざる恐れがあります。Simulation ModelとRender評価時点を明確にします。

## 54. Animation LOD

遠いModelは更新頻度を下げる、Bone数を減らす、停止する方法があります。ただし画面外でもCombat用Bone Colliderが必要ならGameplay Poseは更新します。

## 55. Culling時Animation

- 常に更新: 正確だがCost大。
- 時間だけ進め、再表示時にPose評価。
- 完全停止: 再表示時に時間差が出る。

Object用途ごとにPolicyを設定します。

## 56. Animation Cache

同Skeleton・同Clip・同時刻のPose共有は理論上可能ですが、InstanceごとのBlend、Root Motion、補正が違います。複雑化前にProfilerで評価Costを測ります。

## 57. Debug UI

State、Clip名、Attach番号、時間/総時間、Normalized Time、Weight、Speed、Loop回数、遷移残時間、直近Eventを表示します。

## 58. Timeline Gizmo

横軸にClip時間を描き、Event、Hitbox Active、Cancel Window、現在位置、Crossfade Weightを重ねます。1Frameずれを可視化できます。

## 59. Skeleton Gizmo

Bone親子を線で描き、選択BoneのLocal/World位置、Layer Weight、User Matrix Overrideを表示します。

## 60. Asset Validation

- 必須Clip名が存在。
- 総時間が正。
- Skeleton Frame名が互換。
- Loop Clipの開始・終端Poseが自然。
- Root Motion軸が規約通り。
- Event時刻が0～1内で整列。
- 必須Foot Markerが存在。

## 61. Hot Reload

Clip Index、総時間、Event Dataを再構築します。現在Normalized Timeを新ClipへMappingし、存在しないClipならFallback Stateへ遷移します。古いAttachは安全にDetachします。

## 62. よくある不具合

- Animationが止まる: 時間を自動で進むと思っている。
- Memory増加: 古いAttachをDetachしていない。
- 別Modelが動く: Attach番号をModel間で流用。
- Crossfade後も重い: Weight0の旧ClipがAttach中。
- Event二重発行: Loop/再評価の通過管理不足。
- Weaponが1Frame遅い: Pose設定前にBoneを取得。
- 壁を抜ける: Root MotionをCollisionなしで適用。
- 物理が戻る: Animation/Transformと物理計算順が違う。

## 63. Metrics

- Active Animator数。
- Attach総数、最大同時Attach数。
- Transition数。
- Animation更新時間。
- Event発行数。
- Bone/Socket取得数。
- LOD別更新頻度。
- Root Motion Correction量。

## 64. Test

- 大DeltaでLoop余りを保持。
- Non-loopが終端で止まる。
- Crossfade Weight合計が期待値。
- 完了時に旧Attachを一度だけDetach。
- Loop境界Eventを一度ずつ発行。
- Hit Stop中にEventが再発しない。
- Root DeltaのLoop境界。
- Hot Reload後のClip Fallback。

## 65. 設計チェックリスト

- [ ] Anim IndexとAttach番号を区別した。
- [ ] Attach番号をModelと組で保持する。
- [ ] Animation時間を明示的に進める。
- [ ] Loop超過分を保持する。
- [ ] Crossfade完了後に旧ClipをDetachする。
- [ ] 全Clipを常時Attachしない。
- [ ] EventをUpdateで通過判定する。
- [ ] Combat Timelineと表示Timelineの責任を決めた。
- [ ] Root MotionをCollisionへ通す。
- [ ] Pose設定後にSocketを取得する。
- [ ] Physics更新順を公式契約へ合わせた。

## 66. 理解確認問題

1. Animation IndexとAttach番号の違いは何か。
2. MV1 Animationが自動再生されないとはどういう意味か。
3. Loop時に時間を単純に0へ戻す欠点は何か。
4. Crossfade中に二ClipをAttachする理由は何か。
5. Weight0のClipをDetachすべき理由は何か。
6. 大DeltaでEvent通過をどう処理するか。
7. Animation EventだけをCombatの正にする危険は何か。
8. Root Motionを直接World位置へ足してはいけない理由は何か。
9. Bone Socket取得順が重要な理由は何か。
10. Animation LODとGameplay Boneの関係は何か。

## 67. 実践課題

1. Clip一覧と必須Clip Validationを作る。
2. Attach/Detachを管理するMove-only Slotを作る。
3. Loop/Non-loop Playerを実装する。
4. Idle→Walk→Run Crossfadeを作る。
5. 遷移中の再遷移Policyを実装する。
6. Footstep/Hitbox Event通過判定を作る。
7. Root MotionをCharacter Controllerへ渡す。
8. Upper Body攻撃Layerを検討・実装する。
9. TimelineとSkeleton Gizmoを作る。
10. Hot ReloadでNormalized Timeを移行する。

## 68. 公式資料

- [DXライブラリ MV1 Animation関数](https://dxlib.xsrv.jp/function/dxfunc_3d_model_1.html)
- [DXライブラリ 3D関数一覧](https://dxlib.xsrv.jp/function/dxfunc_3d.html)
- [DXライブラリ Animationサンプル](https://dxlib.xsrv.jp/program/dxprogram_Animation.html)
- [Animationによる座標移動](https://dxlib.xsrv.jp/program/dxprogram_AnimationMoveControl.html)

Animation時間単位、Attach失敗値、別Model Sourceの寿命、Blend方式、Frame Local Position、物理更新順を利用中バージョンの公式資料で確認してください。

## 69. 次章への接続

次章では3D Collision・Physics設計を扱い、Sphere/AABB/OBB/Capsule、Model Triangle Query、Character Controller、Ground/Slope、Continuous Collision、物理更新順をAnimationと統合します。
