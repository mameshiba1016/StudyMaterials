# Cinemachine・Camera・Lock-on・Camera Collision

> 対象: Unity 6 + Cinemachine 3.1系を中心にする。CinemachineはPackageであり、2.xと3.xではComponent名・namespace・構成が大きく異なる。導入版をPackage Managerと`Packages/manifest.json`で確認すること。

## 1. CameraはGameplayを見せるSystem

CameraはCharacterの子へ置くだけのTransformではありません。

```text
Gameplay State
├─ Current Character
├─ Lock-on Target
├─ Combat Intensity
├─ Camera Volume / Zone
└─ Cutscene State
        ↓
Camera Director（どのShotを使うか）
        ↓
Cinemachine Camera（構図候補）
        ↓
Cinemachine Brain（選択・Blend・出力）
        ↓
Unity Camera（実際にrender）
```

Cameraが敵選択、damage、Character交代の真実を所有しません。Gameplay stateを読み、構図へ変換します。

## 2. Unity CameraとCinemachine Camera

- Unity `Camera`: 実際のView/Projectionを作りrenderする。
- `CinemachineBrain`: Unity Camera上で有効なCinemachine Cameraを選び、blend結果を適用する。
- Cinemachine Camera: Follow、Look At、Lens、Position Control、Rotation Control、Noise等を持つshot設計。自分ではrenderしない。

通常はMain Cameraを一つ置き、Brainを付け、Scene内に複数のCinemachine Cameraを用意します。

```text
MainCamera
├─ Camera
├─ AudioListener
└─ CinemachineBrain

CameraRigs
├─ ExplorationCamera
├─ LockOnCamera
├─ FinisherCamera
└─ CutsceneCamera
```

## 3. Cinemachine 2.xと3.x

Cinemachine 3は新しい構成です。古い記事の`CinemachineVirtualCamera`、FreeLook rig、namespace、Extension名をそのまま貼り付けないでください。

移行時:

- Package versionを固定・記録。
- Upgrade Guideを読む。
- Prefab/Sceneをbackupしたbranchで変換。
- Input provider、Collider、Impulse、Timeline bindingを確認。
- Camera blendとlensを動画比較。
- Script APIのnamespace/type変更をcompileで検出。

この教材では概念を安定軸にし、具体的なInspector名は使用中の3.1 Manualを優先します。

## 4. FollowとLook At

- Follow Target: Camera位置を計算する基準。
- Look At/Tracking Target: Camera回転・構図の基準。

同じCharacter rootを両方へ渡すだけでなく、専用Target Transformを用意します。

```text
CharacterRoot
├─ Motor / Combat
└─ CameraTargets
   ├─ FollowTarget（腰付近）
   ├─ AimTarget（胸～頭）
   └─ LockOnTarget（敵との中点へ動くこともある）
```

Animation boneを直接Followすると歩行揺れがCameraへ入ります。Gameplay rootや平滑化したtargetを使い、必要な揺れだけNoise/Impulseで足します。

## 5. Camera Target Binder

Package API差を局所化するため、GameplayからCinemachine Componentを直接各所で触りません。

```csharp
using UnityEngine;

public interface ICameraTargetSink
{
    void BindFollow(Transform target);
    void BindLookAt(Transform target);
    void ClearTargets();
}

public sealed class CameraTargetCoordinator : MonoBehaviour
{
    private ICameraTargetSink sink;
    private uint bindingGeneration;

    public void Initialize(ICameraTargetSink targetSink)
    {
        sink = targetSink;
    }

    public void BindCharacter(
        Transform follow,
        Transform lookAt)
    {
        ++bindingGeneration;
        sink.BindFollow(follow);
        sink.BindLookAt(lookAt);
    }

    public uint CurrentGeneration => bindingGeneration;
}
```

実際のCinemachine 3 Componentへの代入はadapter一つに閉じ込めます。Package更新時の修正範囲が狭くなります。

## 6. Camera Updateの時系列

Characterが動く前にCameraを更新すると一frame遅れ、FixedUpdate/Update/LateUpdateが混在するとjitterします。

```text
Input sample
 → Character simulation / Motor
 → Animator / Root Motion
 → Camera target pose
 → Cinemachine evaluation
 → BrainがUnity Cameraへ適用
 → Render
```

BrainのUpdate Method、Blend Update Method、Smart Update、Fixed/Late/Manual系の選択肢はPackage版で確認します。

- Character ControllerをUpdateで動かすならCameraはその後。
- RigidbodyをFixedUpdate + Interpolationで動かすなら補間後poseを追う。
- Manual simulationならBrainも明示的な順序へ。

## 7. Damping

Dampingは単なる遅延ではなく、target変化へCameraが追いつく時間的応答です。位置、回転、軸ごとに設定できます。

大きすぎる場合:

- Characterが画面端へ逃げる。
- 素早いDodge/交代へ遅れる。
- Lock-on Targetを見失う。

小さすぎる場合:

- Animationや段差の微振動を拾う。
- 急な向き変更で酔う。

Camera目的ごとに値を変え、全shotへ同じDampingを使いません。

## 8. Camera lagを数式で理解する

frame-rate independentな指数追従の概念:

```csharp
public static Vector3 ExpSmooth(
    Vector3 current,
    Vector3 target,
    float sharpness,
    float deltaTime)
{
    float t = 1.0f - Mathf.Exp(-sharpness * deltaTime);
    return Vector3.LerpUnclamped(current, target, t);
}
```

Cinemachine内部設定を自作式で二重平滑化しないでください。Target側とCamera側の両方へDampingを掛けると遅延が重なります。どの層が何のnoiseを除去するか決めます。

## 9. Lens

- Field of View: Perspective Cameraの画角。
- Orthographic Size: Orthographic Cameraの表示範囲。
- Near/Far Clip Plane。
- Dutch angle。
- Physical Camera関連設定。

FOVを広げると速度感と周辺視野が増しますが、歪みと対象の小ささが増します。FOV kickは速度に応じて急変させずcurveとDampingを使います。

Near Planeを小さくしすぎるとdepth precisionへ影響し、Far Planeを大きくしすぎても同様です。rendering章でdepth bufferと併せて扱います。

## 10. Exploration Camera

自由移動Cameraの責務:

- Playerの入力でYaw/Pitch。
- CharacterをFollow。
- Camera方向から移動方向を作る。
- Pitch範囲を制限。
- Recentering。
- Camera Collision。
- Characterの背後へ自動追従するか。

入力のscale:

```csharp
public readonly struct CameraLookInput
{
    public CameraLookInput(Vector2 value, bool isPointerDelta)
    {
        Value = value;
        IsPointerDelta = isPointerDelta;
    }

    public Vector2 Value { get; }
    public bool IsPointerDelta { get; }
}
```

Mouse deltaとGamepad stickは意味が異なります。Mouseはframe中の移動量、stickは速度意図として扱うことが多く、両方へ同じ`deltaTime`を無条件に掛けません。

## 11. Input Systemとの接続

Cinemachine 3のInput Axis Controller等、Package提供Componentを使う方法と、自作adapterからAxisへ値を渡す方法があります。

設計事項:

- Gameplay/UI/Photo Mode Action Map。
- Mouse/Gamepad感度を別に保存。
- X/Y反転。
- acceleration/deadzone。
- Pause中にunscaledで動くか。
- Device変更時のglyphだけでなく感度profile。
- Rebinding。

CharacterごとにCamera Inputを購読せず、Player Camera層が所有します。

## 12. Recentering

入力が一定時間無いとCharacter背後へ戻す機能です。

```text
Wait time
 → recenter開始
 → curve/速度で目標Yawへ
 → Player入力で即cancel
```

Lock-on、壁際、Aim中、空中、Finisher中ではRecentering policyを変えます。Cameraが勝手に戻りPlayer入力と奪い合わないよう、入力検出とstate切替を同じownerに置きます。

## 13. Lock-on SystemとCameraを分離する

```text
Targeting System
├─ candidate query
├─ screen position
├─ distance/angle/visibility score
├─ team/alive/targetable
└─ Current Target
        ↓
Camera Director
        ↓ Lock-on shotを選ぶ
Lock-on Camera
```

CameraがPhysics.OverlapSphereで敵を選ぶのではなく、Targeting Systemが選択し、CameraはCurrent Targetを構図へ使います。

## 14. Lock-on候補score

```csharp
public readonly struct TargetScore
{
    public TargetScore(
        float screenDistance,
        float worldDistance,
        float viewAngle,
        bool visible)
    {
        ScreenDistance = screenDistance;
        WorldDistance = worldDistance;
        ViewAngle = viewAngle;
        Visible = visible;
    }

    public float ScreenDistance { get; }
    public float WorldDistance { get; }
    public float ViewAngle { get; }
    public bool Visible { get; }
}
```

最終scoreの例:

```text
screen center distance × weight
+ world distance × weight
+ view angle × weight
+ occlusion penalty
+ current target hysteresis bonus
```

現在Targetへbonusを与えないと、ほぼ同scoreの敵間で毎frame切り替わります。

## 15. WorldToViewportPoint

Camera画面上の位置で候補を評価できます。

```csharp
Vector3 viewport = camera.WorldToViewportPoint(targetPosition);

bool inFront = viewport.z > 0.0f;
Vector2 fromCenter = new Vector2(
    viewport.x - 0.5f,
    viewport.y - 0.5f);
```

behind-cameraの点は投影値だけで判断せず`z`を確認します。Safe Area、split screen、render viewportがある場合、基準中心を調整します。

## 16. Lock-on構図

PlayerだけをFollowしTargetをLook Atすると、Playerが画面外へ寄りすぎることがあります。PlayerとEnemyの中点を動的Targetにします。

```csharp
public void UpdateLockOnTarget(
    Vector3 player,
    Vector3 enemy,
    float enemyWeight)
{
    Vector3 midpoint = Vector3.Lerp(player, enemy, enemyWeight);
    lockOnAimTarget.position = midpoint + Vector3.up * heightOffset;
}
```

距離が広がるとCamera distance/FOVを調整し、両者をframe内へ収めます。ただし急拡大で酔わないようhysteresisとrate limitを持たせます。

## 17. Target Group

Cinemachine Target Group相当の機能で複数Targetの位置と重みを構図に使えます。具体Component名と設定は3.1のManualを確認します。

注意:

- targetが破棄されたときにfake nullを除去。
- weight/radiusの意味。
- 多数敵をGroupへ入れてCameraが遠ざかりすぎない。
- PlayerとCurrent Targetだけを基本にする。
- Character交代時に旧Player targetを置換。

## 18. Target切替

右stick flick等で現在Targetから画面方向に次候補を選びます。

```text
current target viewport
 → input方向とのdot
 → 希望方向側だけfilter
 → screen distance + visibilityでscore
 → hysteresis/cooldown
 → switch
```

入力を押し続けて毎frame切り替わらないようedge、deadzone、cooldownを使います。

## 19. Lock-on解除条件

- Target死亡/破棄。
- targetable false。
- 最大距離を一定時間超過。
- Camera後方へ長時間移動。
- 遮蔽が一定時間継続。
- Scene/Character交代。
- Playerによる明示解除。

一frame遮蔽しただけで解除するとちらつきます。Grace timeと理由別policyを持ちます。

## 20. Camera Collision

Cameraが壁へ入るとWorldが遮蔽されます。Targetからdesired Camera位置へSphere Castし、障害物手前へ寄せます。

```csharp
public static Vector3 ResolveCameraCollision(
    Vector3 pivot,
    Vector3 desiredPosition,
    float radius,
    LayerMask obstacleMask)
{
    Vector3 delta = desiredPosition - pivot;
    float distance = delta.magnitude;

    if (distance <= 0.0001f)
    {
        return pivot;
    }

    Vector3 direction = delta / distance;

    if (Physics.SphereCast(
        pivot,
        radius,
        direction,
        out RaycastHit hit,
        distance,
        obstacleMask,
        QueryTriggerInteraction.Ignore))
    {
        return pivot + direction * Mathf.Max(0.0f, hit.distance - 0.05f);
    }

    return desiredPosition;
}
```

CinemachineのCollider/Deoccluder系機能が同じ責務を担う場合、自作collisionと二重に適用しません。3.xでの名称・Extension構成を確認します。

## 21. Camera Collisionの難所

- pivotが壁内にある。
- Camera sphereが開始時点でoverlap。
- 細い柱の横で左右に跳ぶ。
- doorway通過で急にzoom。
- Target自身のColliderへ当たる。
- Trigger/VFX Colliderへ当たる。
- obstacleがCameraとTarget間を高速で横切る。
- 元距離へ戻るとき急に飛ぶ。

侵入は速く、復帰は少し遅くする非対称Dampingがよく使われます。ただし閉所では距離が呼吸しないようhysteresisを入れます。

## 22. OcclusionとCollision

- Collision: Camera自身がgeometryへ入らない。
- Occlusion: Character/Targetがgeometryで隠れない。

同じではありません。Cameraが壁外でも柱がPlayerを隠す場合があります。

対応:

- Camera位置を移す。
- Shoulder sideを切り替える。
- FOV/距離を調整。
- 障害物をfade/dither。
- Target outlineを表示。
- Lock-on解除。

CameraだけでなくRendering/Targetingと連携します。

## 23. Shoulder Camera

左右肩越しCameraでは、壁際で肩側を切り替えると視界を確保できます。

```text
Right Shoulder Shot
Left Shoulder Shot
  ↓ obstacle/Player inputで選択
Blendまたはoffset補間
```

Aim reticleのscreen rayとweapon muzzle rayがずれるため:

1. Camera中心からaim pointをRaycast。
2. Muzzleからaim pointへ射線確認。
3. 手前障害物へ当たる場合はmuzzle hitを採用。

Cameraが見えても銃口が壁内なら撃てない、という視差を処理します。

## 24. PriorityとShot選択

Cinemachine CameraのPriorityやenabled state等からBrainがlive Cameraを選びます。数字を各scriptが直接書き換えると競合します。

```csharp
public enum CameraMode
{
    Exploration,
    LockOn,
    Aim,
    Finisher,
    Cutscene,
    PhotoMode
}

public sealed class CameraDirector : MonoBehaviour
{
    public CameraMode CurrentMode { get; private set; }

    public void SetMode(CameraMode mode)
    {
        if (mode == CurrentMode)
        {
            return;
        }

        CurrentMode = mode;
        ApplyShotSelection(mode);
    }
}
```

Camera Director一つがpriority/activationを管理します。Cutscene、Death、Pauseが同時要求した場合の優先順位を状態遷移表にします。

## 25. Camera Mode優先順位

例:

```text
Cutscene
 > Finisher
 > Death
 > Aim
 > LockOn
 > Exploration
```

単なる整数最大値だけでなく、request tokenを発行します。Finisher終了時にExplorationへ直戻しすると、途中から有効なDeath Camera要求を上書きする危険があります。

```csharp
public readonly struct CameraRequestToken
{
    public CameraRequestToken(uint id) => Id = id;
    public uint Id { get; }
}
```

Ownerが自身のtokenだけをreleaseし、Directorが残存requestから次modeを再計算します。

## 26. Blend

Camera切替にはCutまたはBlendを使います。

- Cut: 0秒。瞬間移動、Teleport、視点不連続を隠したい場合。
- Ease In/Out等: Character交代やLock-on。
- Custom curve: Shotごとの演出。
- Custom Blend Asset: Camera A→Bの組合せ別設定。

Blend中はincoming/outgoing両Cameraがliveとして評価され得ます。両方のTarget、Extension、Noiseが有効である前提でtestします。

## 27. Camera warp

CharacterがTeleport/Respawnすると、Damping履歴によってCameraがWorldを横断することがあります。PackageのTarget Object Warped相当API、force position、previous state invalidation等を利用版で確認します。

手順:

1. Camera Directorへwarp開始。
2. Characterを安全な地点へ移動。
3. Follow/LookAt targetを移動。
4. Cinemachineのprevious stateをwarp/reset。
5. 必要ならCut。
6. Camera Collisionを再評価。

## 28. Character交代

交代時にCinemachine Camera自体をCharacterごとに作り直すより、Player寿命のCamera RigへTargetを差し替える方が管理しやすい場合があります。

```csharp
public void OnCurrentCharacterChanged(CharacterView next)
{
    if (next == null)
    {
        cameraTargets.ClearTargets();
        return;
    }

    cameraTargets.BindCharacter(
        next.CameraFollowTarget,
        next.CameraAimTarget);
}
```

確認:

- 旧Targetを参照したblend。
- 新旧Characterの距離が大きい場合はCutかwarp。
- Lock-on Targetを維持するか。
- Camera yaw/pitchを維持するか。
- Character別offset/FOV profile。
- 旧Character破棄後のfake null。

## 29. Camera Profile

CharacterやState別値をScriptableObjectへまとめられます。

```csharp
using UnityEngine;

[CreateAssetMenu(menuName = "Game/Camera Profile")]
public sealed class CameraProfile : ScriptableObject
{
    [field: SerializeField] public float FieldOfView { get; private set; } = 60.0f;
    [field: SerializeField] public Vector3 FollowOffset { get; private set; }
    [field: SerializeField] public float PositionDamping { get; private set; } = 0.2f;
    [field: SerializeField] public float RotationDamping { get; private set; } = 0.1f;
}
```

Assetは静的設定です。runtimeの現在yawやshake残量を書き込みません。

## 30. Camera ShakeとImpulse

Camera Shakeは位置/回転noiseを加える演出です。Cinemachine Impulseはsourceがsignalを出しlistener側Cameraが受ける構成を提供します。

用途:

- attack impact。
- landing。
- explosion。
- parry。
- environment。

Damage callback内からCamera Transformを直接揺らさず、typed Camera Impulse Eventを送ります。

## 31. Shake設計

```csharp
public readonly struct CameraImpulseRequest
{
    public CameraImpulseRequest(
        Vector3 worldPosition,
        float amplitude,
        float frequency,
        float duration)
    {
        WorldPosition = worldPosition;
        Amplitude = amplitude;
        Frequency = frequency;
        Duration = duration;
    }

    public Vector3 WorldPosition { get; }
    public float Amplitude { get; }
    public float Frequency { get; }
    public float Duration { get; }
}
```

考慮:

- 距離減衰。
- 方向性。
- 複数Impulse合成。
- 最大振幅clamp。
- Slow Motion/Unscaled時間。
- Player設定のShake強度。
- Camera酔い対策で完全OFF。

## 32. Perlin NoiseとImpulse

- Continuous Noise: 手持ち、走行、環境の継続揺れ。
- Impulse: hit、爆発等の一時的signal。

同じ高周波noiseを重ねると視認性が落ちます。Character Animationの揺れ、Follow Targetの揺れ、Noise、Impulseを分離します。

## 33. CameraとHit Stop

Hit StopでGameplay timeScaleを0にしてもCameraを動かすか決めます。

- Cameraも止める: impact frameを静止。
- Cameraだけunscaledで寄る/揺れる: 演出強調。
- Aim入力だけ許可: Player comfort。

BrainのIgnore Time Scale相当設定やImpulse time envelopeを利用版で確認します。Camera、Input、UI、VFXの時計を表にします。

## 34. Camera Zone

狭い通路、Boss arena、室内では通常Camera profileが合いません。Trigger/VolumeでCamera制約を変更します。

```text
Camera Zone
├─ allowed yaw range
├─ min/max distance
├─ FOV
├─ shoulder side
├─ collision radius
└─ fixed shot candidate
```

Enter/Exit callbackだけに頼るとteleportやdisableで解除漏れが起きます。現在positionからactive zoneを再resolveできる仕組みを持ちます。

## 35. Boss Camera

Boss全体を常にframeへ入れるとPlayerが小さくなり操作しづらくなります。

- 通常はPlayer中心。
- 攻撃予兆時のみBoss方向へbias。
- Boss距離に応じFOV/距離を制限内で調整。
- 巨大BossはTarget pointを頭ではなく攻撃部位/中点へ。
- Off-screen attack indicatorとCameraを役割分担。

すべてをCameraだけで見せずUI、Audio、VFXのtelegraphも使います。

## 36. Finisher Camera

演出Cameraへ切り替える場合:

1. Gameplay stateがFinisherを確定。
2. Camera request tokenを取得。
3. Cinemachine CameraへActor targetsをbind。
4. obstacle/arenaを確認してshot選択。
5. Cut/Blend。
6. Sequence終了・cancel・死亡でtoken release。
7. Gameplay Cameraへ戻しtarget/historyを復旧。

Animation Event一つだけでreturnすると、割込み時にCameraが戻らない事故が起きます。Flow ownerのfinally/cancel pathでreleaseします。

## 37. Timeline

TimelineはCinemachine shotをtrackで制御できます。Timeline override中は通常のpriority logicより強く制御される場合があります。

確認:

- Timeline開始前Camera。
- skip時のCamera復旧。
- binding先Characterの差替え。
- Scene unload。
- Pause/timeScale。
- duplicate Brain/Camera。
- Post Processing/Audioとの同期。

## 38. CameraとRendering

CameraはTransformだけでなくrender設定も持ちます。

- Culling Mask。
- Clear Flags/Background。
- HDR/MSAA。
- Render Type/Stacking（URP）。
- Post Processing。
- Depth。
- Viewport Rect。
- Target Texture。

Gameplay Camera、UI Camera、Mini-map Camera、Reflection Cameraを分ける場合、AudioListener重複、Post Processing二重、Layer漏れに注意します。

## 39. Camera-safe Gameplay

Cameraが壁で近づいても:

- Player攻撃rangeは変えない。
- lock-on判定のWorld距離は変えない。
- Mouse rayのorigin/aimはCamera位置変化を考慮。
- Character透明化/透過をpresentationで処理。
- Camera clipで敵が見えなくてもAI simulationを止めない。

Camera状態をGameplay物理の基準へ無秩序に使いません。

## 40. Camera酔い対策

- Camera rotation accelerationを急にしすぎない。
- 高周波shakeを抑える。
- FOV変化を小さく・設定可能にする。
- Head bobをOFF可能にする。
- Motion Blurを設定可能にする。
- Horizon/Dutchを安定させる。
- Lock-on切替時の急回転をclamp。
- Camera距離と感度を設定可能にする。
- Shake scaleを0まで下げられるようにする。

「迫力」をPlayer comfortより優先しません。

## 41. Debug

- 現在live Camera。
- Brain blendのfrom/to、経過率。
- Follow/LookAt target instance ID。
- Camera Modeとrequest token一覧。
- desired/resolved Camera位置。
- collision hit point/normal。
- Lock-on candidate score。
- viewport座標。
- Camera Zone。
- active Impulse。

```csharp
private void OnDrawGizmos()
{
    Gizmos.color = Color.yellow;
    Gizmos.DrawLine(cameraPivot, desiredCameraPosition);

    Gizmos.color = Color.red;
    Gizmos.DrawWireSphere(resolvedCameraPosition, cameraCollisionRadius);
}
```

PackageのGame View debug textやScene View guidesも使います。Release Buildでは無効化します。

## 42. 性能

- Standby CameraのUpdate設定。
- Cinemachine Camera/Extension数。
- Target Groupのtarget数。
- 毎frameのPhysics query。
- CameraごとのPost Processing。
- Reflection/Mini-map追加Camera。
- UI Camera stacking。
- Impulse listener/source数。

Camera collisionを毎frame複数Sphere Castする場合、Layerを絞り、必要数を測ります。最適化で障害物抜けを起こさないようscenario testを残します。

## 43. よくある不具合

- Cinemachine 2.xの記事を3.xへそのまま使う。
- Main Cameraが複数ありBrain/AudioListenerが競合する。
- Character boneを直接Followして揺れる。
- Target側とCamera側でDampingを二重にする。
- Mouse deltaへ`deltaTime`を二重適用する。
- Camera自身がlock-on Targetを選ぶ。
- behind-camera候補をviewport値だけで採用する。
- Target切替を毎frame行い高速で巡回する。
- LayerMaskからPlayerを除外せずCamera collisionする。
- Camera collisionの侵入と復帰を同速度にして跳ねる。
- Priorityを複数scriptが直接上書きする。
- Finisher終了時に別の高優先requestを破壊する。
- Teleport後にDamping履歴でCameraが飛ぶ。
- Character交代後に旧Targetを参照する。
- Camera Shakeを合成せず振幅が暴走する。
- Pause中のCamera時計が仕様と違う。

## 44. Test Matrix

| 観点 | Test |
|---|---|
| Input | Mouse、Gamepad、invert、最大/最小感度 |
| Space | 広場、狭い通路、door、柱、低天井、角 |
| Target | 近い、遠い、背後、上下、複数、死亡 |
| Character | 通常、Dodge、jump、knockback、交代 |
| Camera | Exploration、Lock-on、Aim、Finisher |
| Blend | Cut、短い、長い、途中割込み |
| Time | pause、slow、hit stop、unscaled |
| Warp | teleport、respawn、Scene load |
| Visibility | occlusion、off-screen、transparent obstacle |
| Comfort | shake 0/最大、FOV、motion blur |

## 45. 設計チェックリスト

- Cinemachine Package版を固定・記録したか。
- Unity Camera、Brain、Cinemachine Cameraの責務を説明できるか。
- Camera DirectorがShot選択を一元管理するか。
- Follow/LookAt専用Targetを使うか。
- Character更新後にCameraを評価するか。
- MouseとGamepad入力を別の単位で扱うか。
- Lock-on選択をTargeting Systemへ分離したか。
- Camera CollisionとOcclusionを区別したか。
- request tokenでCamera要求を安全にreleaseできるか。
- Teleport/Character交代で履歴とTargetを更新するか。
- Shake/FOV/Motion BlurをPlayerが調整できるか。
- Package更新時にshot比較testがあるか。

## 公式資料

- [Unity Manual: Cinemachine package version information](https://docs.unity3d.com/6000.0/Documentation/Manual/com.unity.cinemachine.html)
- [Cinemachine 3.1 Manual](https://docs.unity3d.com/Packages/com.unity.cinemachine@3.1/manual/index.html)
- [Cinemachine 3.1: Cinemachine Camera](https://docs.unity3d.com/Packages/com.unity.cinemachine@3.1/manual/CinemachineCamera.html)
- [Cinemachine 3.1: Cinemachine Brain](https://docs.unity3d.com/Packages/com.unity.cinemachine@3.1/manual/CinemachineBrain.html)
- [Cinemachine 3.1: Third Person Cameras](https://docs.unity3d.com/Packages/com.unity.cinemachine@3.1/manual/ThirdPersonCameras.html)
- [Cinemachine 3.1: Input Axis Controller](https://docs.unity3d.com/Packages/com.unity.cinemachine@3.1/manual/CinemachineInputAxisController.html)
- [Cinemachine 3.1: Impulse](https://docs.unity3d.com/Packages/com.unity.cinemachine@3.1/manual/CinemachineImpulse.html)
- [Cinemachine 3.1: Upgrade from 2.x](https://docs.unity3d.com/Packages/com.unity.cinemachine@3.1/manual/CinemachineUpgradeFrom2.html)

