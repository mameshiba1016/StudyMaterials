# Animator・Animation Event・Avatar・Root Motion

> 対象: Unity 6系のMecanim/Animator。Animation Rigging等のPackageは別章で補足します。

## 1. AnimationはGameplayの見た目である

高速アクションでは、AnimationをGameplayの唯一の真実にすると、攻撃受付、cancel、hit判定、Slow Motion、Character交代がAnimation遷移へ強く依存します。

```text
Input Command
  ↓
Combat State Machine（攻撃可否・tick・cancel・damage）
  ├─ Movement Policy → Character Motor
  └─ Presentation State → Animation Driver
                            ↓
                          Animator
                            ↓
                   Pose / Root Motion / Event
```

Animatorは「現在どう見せるか」を表現します。HP減少、damage確定、combo分岐等の重要なGameplay規則は純粋なCombat Stateが所有します。

## 2. 用語

- Animation Clip: bone、Transform、BlendShape等の時間変化。
- Animator Component: GameObject上でControllerを評価してposeを出す。
- Animator Controller: State Machine、Parameter、Layer、Blend Treeを保存するAsset。
- State: ClipまたはBlend Tree等を再生する状態。
- Transition: State間の遷移条件・blend。
- Parameter: float、int、bool、trigger。
- Avatar: modelの骨格をAnimation Systemへ対応付けるAsset。
- Avatar Mask: Layer等で適用するbone/body partを制限。
- Root Motion: Animationから抽出されるroot位置・回転変化。

## 3. 推奨Hierarchy

```text
CharacterRoot
├─ CharacterController / Motor / Combat / Entity ID
├─ HurtboxRoot
└─ VisualRoot
   └─ Model
      ├─ Animator
      └─ Skeleton / SkinnedMesh
```

Gameplay rootとmodel rootを分けると、model交換、Avatar差、scale補正、visual rotationをGameplay collisionから隔離できます。Animator参照を毎frame検索せず、Prefab組立時に注入・cacheします。

## 4. HumanoidとGeneric

### Humanoid

人型boneをAvatarへmappingし、Muscle表現とRetargetingを利用できます。異なる体格のHumanoidへ同じAnimationを再利用しやすい一方、Avatar検証と人型制約があります。

### Generic

任意のbone hierarchyを扱えます。Monster、武器、機械、独自skeletonに向きます。Root Node設定とhierarchyの一致が重要です。

### Legacy

古いAnimation System向けです。新規の複雑なCharacterでは通常Mecanimを検討します。

Import時にAnimation Typeを後から変えると、Avatar、Clip、Root Motion、referenceへ影響します。model import設定をVersion Controlでreviewします。

## 5. AvatarとRetargeting

Humanoid RetargetingはAvatarによって骨格対応を得ます。

確認項目:

- Avatar DefinitionがCreate From This ModelかCopy From Other Avatarか。
- Configure Avatarでbone mappingがvalidか。
- T-poseが正しいか。
- twist bone、finger、jaw、eye等のoptional bone。
- modelのscaleと前方向。
- ClipとmodelでAvatar基準が一致するか。

Retargetingしても、手足長、武器位置、接地、攻撃rangeが完全一致するわけではありません。Gameplay Hitboxはbone poseを参考にしつつ、Character別dataで補正します。

## 6. Animator ControllerはAnimation用State Machine

```text
Base Layer
├─ Locomotion Blend Tree
├─ Jump
├─ Fall
├─ Land
├─ Dodge
├─ Attack Sub-State Machine
│  ├─ Attack01
│  ├─ Attack02
│  └─ Finisher
├─ Hit Reaction
└─ Death
```

全Character、全Weapon、全Skillを一つの巨大graphへ入れるとtransitionが爆発します。

- 共通locomotionは共通Controller。
- 差替えClipはAnimator Override Controller等を検討。
- 大きく構造が違うCharacterは別Controller。
- Gameplay combo graphはAnimator transitionと分ける。

## 7. Parameter

```csharp
using UnityEngine;

public sealed class CharacterAnimationDriver : MonoBehaviour
{
    private static readonly int SpeedId = Animator.StringToHash("Speed");
    private static readonly int GroundedId = Animator.StringToHash("Grounded");
    private static readonly int AttackId = Animator.StringToHash("Attack");

    [SerializeField] private Animator animator;

    public void SetLocomotion(float speed, bool grounded)
    {
        animator.SetFloat(SpeedId, speed);
        animator.SetBool(GroundedId, grounded);
    }

    public void PlayAttack()
    {
        animator.SetTrigger(AttackId);
    }
}
```

`StringToHash`で名前検索を毎回行う箇所とtypoを減らせます。しかしParameter rename時のcompile errorにはならないため、Editor validatorでControllerにParameterが存在するか検査します。

## 8. Parameter型の使い分け

- float: speed、方向、blend weight。
- int: stance、weapon style等。ただしmagic numberを避ける。
- bool: grounded、aiming等の継続状態。
- trigger: 一度の遷移要求。

Triggerはevent queueではありません。遷移条件や同frameのResetで消費されないことがあります。重要commandはCombat Stateが保持し、Animator Triggerはpresentation要求として使います。

## 9. StateとClipを同一視しない

同じClipを複数Stateで使えます。Stateにはspeed、Mirror、Foot IK、Write Defaults、Behaviour、Transition等の文脈があります。

「現在Attack Clipか」をClip名で判断するのではなく、Gameplay StateまたはAnimator State tag/hashを目的に応じて使います。それでもdamage可否をAnimator Stateだけで決めません。

## 10. Transition

Transitionは次を持ちます。

- Conditions。
- Has Exit Time / Exit Time。
- Transition Duration。
- Fixed Durationかnormalized durationか。
- Transition Offset。
- Interruption Source / Ordered Interruption。

Conditionが複数ある場合は基本的に全条件成立が必要です。Exit TimeとConditionを併用すると、Exit Time後に条件が満たされる必要があります。

## 11. normalizedTime

`AnimatorStateInfo.normalizedTime`は概念上、整数部がloop回数、小数部が現在loop内の進捗を表します。ただしtransition中はcurrent/next stateが同時に評価されます。

```csharp
AnimatorStateInfo current = animator.GetCurrentAnimatorStateInfo(0);

float loopProgress = current.normalizedTime % 1.0f;
int completedLoops = Mathf.FloorToInt(current.normalizedTime);
```

非loop Clipでもblend、speed、offset、crossfadeにより「0.7なら確実に攻撃判定開始」とするのは危険です。Combat tick/dataを真実にし、normalized timeは同期・presentation補正へ使います。

## 12. Transition割込み

Transition中にはcurrent stateとnext stateが存在し、さらに割込みでinterrupted stateが関わる場合があります。StateMachineBehaviour callbackが単純なEnter→Update→Exit一列だけになると決めつけません。

確認:

- Attack→DodgeでどのTransitionが割り込めるか。
- Hit ReactionがどのStateから入るか。
- Any State Transitionが自己遷移するか。
- 同frameに複数Triggerが立った場合。
- Transition中にCharacterが死亡した場合。

## 13. Any State

Any StateはどこからでもHit/Death等へ遷移でき便利ですが、多用すると遷移元と優先順位が見えなくなります。

- Death等、本当にglobalな遷移に限定。
- Can Transition To Selfを確認。
- 条件を一意にする。
- Sub-State Machineとの責務を整理。
- 割込み設定をtest。

## 14. Blend Tree

Blend Treeは似たmotionをParameterで連続blendします。Transitionは異なるState間、Blend Treeは同種motion間の連続混合です。

### 1D

`Speed`でIdle→Walk→Runをblendします。

### 2D Simple Directional

前後左右等、同じ方向へmotionが重複しない方向blend。

### 2D Freeform

複数速度・方向sampleを配置するlocomotion等。種類ごとの前提を確認します。

Clip同士のfoot contact timingが揃わないと足滑りや二重足接地になります。normalized time上の位相を揃えます。

## 15. Blend値は実速度から作る

```csharp
public void UpdateLocomotion(
    Vector3 actualWorldVelocity,
    Transform characterTransform,
    float deltaTime)
{
    Vector3 local = characterTransform.InverseTransformDirection(actualWorldVelocity);

    animator.SetFloat("MoveX", local.x, 0.1f, deltaTime);
    animator.SetFloat("MoveY", local.z, 0.1f, deltaTime);
    animator.SetFloat("Speed", new Vector2(local.x, local.z).magnitude);
}
```

stickではなくMotorの実移動を使えば、壁に塞がれたときの足滑りを減らせます。Parameter dampingの時間軸がPause/Slow Motionでどうなるか利用版とupdate modeを確認します。

## 16. Animator Layer

Layerで複数State Machineのposeを合成できます。

```text
Base Layer       : 全身locomotion
UpperBody Layer  : 上半身aim/reload
Additive Layer   : recoil/breath
Face Layer       : facial pose
```

- Override: 下層poseをweightで置換。
- Additive: 基準poseとの差分を加算。
- Avatar Mask: 適用bone/body partを制限。
- IK Pass: Layer単位のIK callback設定。
- Synced Layer: State構造を同期しmotion等を差替え。

Layer数、IK、mask、blendはevaluation costを増やします。見えないNPCへPlayerと同じ構成を無条件に使いません。

## 17. Avatar Mask

Upper Body Layerで腕・上体だけを有効にし、脚はBase locomotionを残せます。

注意:

- Humanoid body maskとTransform hierarchy mask。
- Weapon boneや追加boneがmask対象か。
- rootを含めて意図せずRoot Motionへ影響しないか。
- Character model差でhierarchy pathが変わらないか。
- Import Maskを使う場合のAsset設定。

## 18. Additive Animation

Additive Clipはreference poseとの差分を加算します。Recoil、呼吸、damage揺れに向きます。

Reference Pose/Frameが間違うと全身がずれます。元Clipが本当にadditive用か、import設定、loop pose、mask、weightを確認します。

## 19. Animation Event

Clip timeline上からmethodを呼ぶ仕組みです。足音、手元VFX、軽いpresentation cueには便利です。

```csharp
using UnityEngine;

public sealed class AnimationEventReceiver : MonoBehaviour
{
    [SerializeField] private FootstepPlayer footstepPlayer;

    // Clip側Eventのfunction名と一致させる。
    public void OnFootstep(int footIndex)
    {
        footstepPlayer.Play(footIndex);
    }
}
```

## 20. Animation Eventの弱点

- method名が文字列でrenameに弱い。
- model再importやClip差替えでEventが変化・消失し得る。
- blend中に複数ClipからEventが発火し得る。
- frame飛び、speed、loop、transitionで期待回数をtestする必要。
- Receiverが無いとwarning/errorの原因になる。
- RetargetingしたClipでGameplay timingがCharacterごとに合わない。

Damage確定、item消費、save等の重要処理をAnimation Eventだけに任せません。

## 21. EventをPresentation Cueへ変換する

```text
Animation Event
  ↓ AnimationEventReceiver
Typed Presentation Cue
  ├─ Footstep Audio
  ├─ Weapon Trail
  └─ Dust VFX
```

Event methodが直接Scene全体を検索しないよう、Receiverに依存を注入します。同じEventが二重発火しても破綻しない処理にします。

## 22. Hitboxの有効期間

選択肢:

1. Combat tick dataがHitbox query期間を決める。
2. Animation Eventはvisual trailだけを開始・停止。
3. Motion変更へ追従させたい場合も、EventをCombat Stateの補助signalに留める。

```text
Combat Attack Instance
├─ startup ticks
├─ active ticks      ← hit判定の真実
├─ recovery ticks
└─ cancel windows

Animation Clip
├─ anticipation
├─ swing
└─ follow-through    ← 見た目を同期
```

## 23. StateMachineBehaviour

Animator State/State Machineへ付け、`OnStateEnter`、`OnStateUpdate`、`OnStateExit`、`OnStateMove`、`OnStateIK`等を受け取れます。

```csharp
using UnityEngine;

public sealed class AnimationStateSignalBehaviour : StateMachineBehaviour
{
    [SerializeField] private int stateSignalId;

    public override void OnStateEnter(
        Animator animator,
        AnimatorStateInfo stateInfo,
        int layerIndex)
    {
        if (animator.TryGetComponent<AnimationSignalReceiver>(out var receiver))
        {
            receiver.OnAnimationStateEntered(stateSignalId, layerIndex);
        }
    }
}
```

StateMachineBehaviourは`ScriptableObject`系で、instance/shared behaviourの扱いに注意が必要です。Character固有runtime stateをfieldへ保存せず、Animator側Receiverへ渡します。

## 24. StateMachineBehaviour callbackの複雑さ

Transitionと割込み中はcurrent、interrupted、next stateが同時にactiveになり得ます。Enter/Exitだけで入力lockを管理すると、割込みで解除漏れや早期解除が起きます。

対策:

- Gameplay lockはCombat Stateが所有。
- Animation callbackはtoken/state instance ID付きsignal。
- ExitだけでなくCharacter disable/交代でもcleanup。
- Layer indexとstate hashを記録。
- CrossFade、Rebind、Controller交換をtest。

## 25. Root Motion

Root MotionはClip内のbody/root移動をGameObject移動へ反映する仕組みです。

### In-place

Clipはその場で動き、Motorが速度を決めます。

長所:

- Gameplay速度をdataで統一しやすい。
- collision、network/replayを管理しやすい。

### Root Motion driven

Clipの移動量を使います。

長所:

- attack踏込み、turn、特殊移動がAnimationに自然に一致。

短所:

- Character/Clip差で移動距離が変わる。
- collision解決と実移動量の統合が必要。
- gameplay timingがClipへ密結合しやすい。

Hybridとしてlocomotionはin-place、attack/dodgeだけRoot Motionも有効です。

## 26. Root TransformのImport設定

Clip ImportにはRoot Transform Rotation、Position Y、Position XZとBake Into Pose等があります。

確認:

- OriginalまたはBody Orientation基準。
- Based Upon設定。
- Offset。
- Loop Pose。
- Feet基準のY。
- Root Motionへ残す軸とPoseへ焼く軸。

設定を誤るとClip開始時のrotation jump、足浮き、毎loop driftが起きます。

## 27. OnAnimatorMove

`OnAnimatorMove`を実装するとRoot Motionをscriptで処理できます。

```csharp
using UnityEngine;

public sealed class RootMotionBridge : MonoBehaviour
{
    [SerializeField] private Animator animator;
    [SerializeField] private CharacterMotor motor;

    private void OnAnimatorMove()
    {
        // 今回Animator評価で得た移動差分。
        Vector3 deltaPosition = animator.deltaPosition;
        Quaternion deltaRotation = animator.deltaRotation;

        // Transformへ直接二重適用せず、Motorがcollision込みで確定する。
        motor.SubmitRootMotion(deltaPosition, deltaRotation);
    }
}
```

`applyRootMotion`、`OnAnimatorMove`、`ApplyBuiltinRootMotion`の関係を利用版で確認します。一つのframeで誰がrootを動かすか一意にします。

## 28. Root MotionとCharacterController

```text
Animator.deltaPosition
  ↓
State policyでscale/axis制限
  ↓
external/vertical/platform motionと合成
  ↓
CharacterController.Move
  ↓
実 displacement
  ↓
Animator speed/warping/debugへfeedback
```

壁に阻まれた場合、Animationが進むのにCharacterが進まずfoot slidingします。

対応:

- Clip speed/strideを補正。
- collision時にmotionを選び直す。
- Motion Warping相当の仕組みを導入。
- Root Motionをdesired displacementと考え、実結果との差を記録。

## 29. Root Motionと重力

ClipのYを使うか、Motor gravityを使うかをStateごとに決めます。

- Locomotion: YをBake Into Pose、Motor gravity。
- Jump Clip: Gameplay ballistic trajectory、Animationはpose。
- Vault/Finisher: root Yを使う特殊state。

Animatorの`gravityWeight`等、Root Transform Y設定との関係も公式資料で確認します。AnimationとGravityがYを二重に動かさないようにします。

## 30. CrossFade

scriptからState hashへCrossFadeできます。

```csharp
private static readonly int DodgeStateId =
    Animator.StringToHash("Base Layer.Dodge");

public void PlayDodge(float normalizedTransitionDuration)
{
    animator.CrossFade(
        DodgeStateId,
        normalizedTransitionDuration,
        0,
        0.0f);
}
```

State path/hashはController構造変更に弱いので定数化・validatorを用意します。Transition graphとCrossFade直呼びを混在させる場合、どちらがState変更を所有するか規約化します。

## 31. Animator Override Controller

同じState Machine構造でClipだけをCharacter/Weaponごとに差替えられます。

注意:

- 全Clip slotの対応漏れ。
- Clip長の差によるExit Time、Event、Root Motion距離。
- 毎回override tableを再構築するallocation。
- 同じAssetをruntime変更して複数Characterへ波及。
- Controller差替え時のState保持/Rebind挙動。

Attack dataとoverride Clipを同じCharacter Definitionから組み立て、対応をvalidatorで確認します。

## 32. Animator Update Mode

Animatorの更新時間軸を選べます。表示名・挙動は利用版を確認します。

- Normal系: 通常frame/timeScaleに連動。
- Animate Physics系: Physics timingとの同期用途。
- Unscaled Time系: timeScale非依存。

Pause中もUI Characterを動かしたい場合と、Gameplay Characterを止めたい場合を分けます。Motor、Root Motion、Animatorが別時間軸だとdriftします。

## 33. Culling Mode

画面外Animatorの更新を省略して性能を上げられますが、Animation Event、Root Motion、bone追従Hitbox、Skinned Mesh boundsへ影響します。

Gameplay判定がbone poseに依存する敵を完全cullすると、画面外で判定が止まる危険があります。

- Gameplay simulationをAnimationから分離。
- visibilityに応じてpresentationだけ簡略化。
- off-screen testを行う。
- boundsが小さすぎて誤cullされないか確認。

## 34. Animator RebindとController交換

model/Controller交換、Character pool再利用で`Rebind`や`Update(0)`相当が必要になる場合があります。

reset対象:

- Parameter。
- Trigger。
- Layer weight。
- Current state/normalized time。
- IK target。
- Root Motion蓄積。
- StateMachineBehaviourの外部token。

Poolから取り出したCharacterが前回のHit/Death poseを保持しないよう、明示的なpresentation reset procedureを作ります。

## 35. IK

Animator IKでhand/foot/look targetへposeを補正できます。`OnAnimatorIK`はIK Passを有効にしたLayer等の条件があります。

```csharp
private void OnAnimatorIK(int layerIndex)
{
    if (lookTarget == null)
    {
        animator.SetLookAtWeight(0.0f);
        return;
    }

    animator.SetLookAtWeight(lookWeight);
    animator.SetLookAtPosition(lookTarget.position);
}
```

IKはAnimation評価後のpresentation補正です。Lock-on判定そのものを頭boneの向きだけで決めません。weightを急変させず、target消滅時に安全に0へ戻します。

## 36. Character交代

交代時のAnimation手順:

1. 旧CharacterのCombat Stateを退場へ。
2. Animation cueを送る。
3. Root Motionの所有を旧側から解除。
4. 新Characterのposition/rotationをMotorへ設定。
5. Animatorを必要状態へ初期化。
6. Controller/Override/Avatar整合を確認。
7. Camera/UI targetを一回だけ交換。
8. 旧async/Event signalをgenerationで無効化。

旧CharacterのAnimation Eventが遅れて新Characterへdamage処理を送らないよう、Attack Instance IDとowner generationを検証します。

## 37. Animation Debug

- Animator WindowのLive Link。
- Parameter値。
- current/next state hash。
- normalized time。
- transition中か。
- Layer weight。
- Root Motion deltaと実移動delta。
- Event受信回数。
- Character/Attack Instance ID。

```csharp
private void LogAnimatorState(int layer)
{
    AnimatorStateInfo current = animator.GetCurrentAnimatorStateInfo(layer);
    AnimatorStateInfo next = animator.GetNextAnimatorStateInfo(layer);

    Debug.Log(
        $"Layer={layer}, Current={current.fullPathHash}, " +
        $"Next={next.fullPathHash}, Transition={animator.IsInTransition(layer)}");
}
```

毎framelogは性能と可読性を壊すため、Development Build限定のring buffer/overlayへ記録します。

## 38. 性能

- Animator数、bone数、Layer数、StateMachineBehaviour。
- Humanoid Retargeting。
- IK、Animation Rigging。
- Blend Tree child数。
- Skinned Mesh Renderer。
- off-screen culling。
- Parameter設定回数。
- Controller/Overrideのruntime再構築。

値が変わったときだけParameterを設定する最適化は可能ですが、まずProfilerで`Animator.Update`、skin、render costを分けて測ります。

LOD:

```text
Near : full Animator + IK + face + cloth
Mid  : reduced IK/layers
Far  : simplified clips / lower update
Off  : presentation停止、Gameplay simulation継続
```

## 39. よくある不具合

- AnimatorをGameplay Stateの唯一の真実にする。
- Animation Eventで直接damageを確定する。
- Triggerをcommand queueだと思う。
- Any Stateを増やし遷移優先が読めない。
- Transition割込み中のStateを一つだけと思う。
- Blend Tree Clipのfoot timingが揃っていない。
- Input値でRunを出し、壁で足滑りする。
- Avatar MaskからWeapon boneが漏れる。
- Root MotionとMotorで二重移動する。
- Root YとGravityを二重適用する。
- Clip差替え後もExit Time/Eventが合うと思う。
- StateMachineBehaviour fieldへCharacter固有runtime stateを置く。
- Cullingで画面外のGameplay Eventまで止まる。
- Pool再利用時にTrigger/State/Layer weightが残る。
- Character交代後に旧Animation Eventが届く。

## 40. Test Matrix

| 観点 | Test |
|---|---|
| Transition | 通常、割込み、自己遷移、Any State |
| Clip | 短い/長い、loop、speed変更、reverse対象 |
| Layer | weight 0/1/中間、mask、additive |
| Time | 30/60/120 FPS、pause、slow、hit stop |
| Root Motion | 壁、坂、段差、platform、teleport |
| Event | blend中、loop境界、frame stall、二重発火 |
| Character | Avatar差、体格差、Override差 |
| State | attack、dodge、hit、death、switch |
| Visibility | near、far、off-screen |
| Pool | disable/enable、Rebind、Scene reload |

## 41. 設計チェックリスト

- Gameplay StateとAnimation Stateを分離したか。
- Parameterの所有者がAnimation Driver一つか。
- Parameter/State hashを定数化・検証したか。
- Transition割込み仕様をtestしたか。
- Blend Tree Clipの位相が揃っているか。
- Layer/Mask/Additiveの責務が説明できるか。
- Animation Eventをpresentation cueへ限定したか。
- Root Motionを適用するownerは一つか。
- Root Motionとcollision後の実移動差を扱うか。
- GravityとRoot Yを二重適用していないか。
- Controller/Override差替え時にresetするか。
- CullingしてもGameplay simulationが止まらないか。
- Character交代後の古いsignalをrejectできるか。
- ProfilerでAnimator/skin/IKを個別に測ったか。

## 公式資料

- [Unity Manual: Animation](https://docs.unity3d.com/6000.0/Documentation/Manual/AnimationSection.html)
- [Unity Manual: Animator Controller](https://docs.unity3d.com/6000.0/Documentation/Manual/Animator.html)
- [Unity Manual: Animation transitions](https://docs.unity3d.com/6000.0/Documentation/Manual/class-Transition.html)
- [Unity Manual: Blend Trees](https://docs.unity3d.com/6000.0/Documentation/Manual/class-BlendTree.html)
- [Unity Manual: Humanoid retargeting](https://docs.unity3d.com/6000.0/Documentation/Manual/Retargeting.html)
- [Unity Manual: Root Motion](https://docs.unity3d.com/6000.0/Documentation/Manual/RootMotion.html)
- [Unity API: MonoBehaviour.OnAnimatorMove](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/MonoBehaviour.OnAnimatorMove.html)
- [Unity API: StateMachineBehaviour](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/StateMachineBehaviour.html)
- [Unity API: Animator.ApplyBuiltinRootMotion](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/Animator.ApplyBuiltinRootMotion.html)

