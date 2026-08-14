# DXライブラリ：Action Camera・Target Lock

この章では、3D Action GameのCameraとTarget Lockを設計します。Cameraは単にPlayerの後ろへ置くObjectではありません。状況を読み取り、Player・Target・危険・地形を画面内へ収め、操作方向の基準となり、攻撃の手応えを演出するSystemです。

> Cameraの動きは酔い、視認性、入力感へ直結します。強いShake、急なFOV変化、自動回転は量を設定可能にし、軽減・無効化できるようにします。

## 1. Action Cameraの責任

- Playerを追従する。
- Free Look入力を受ける。
- Target Lock時にPlayerとTargetを構図へ入れる。
- 壁・地形へめり込まない。
- 攻撃や移動方向の基準軸を提供する。
- Stateに応じてDistance、Height、FOVを変える。
- Shake、Recoil、Zoom等の演出を合成する。
- UIへWorld-to-Screen情報を提供する。

Camera Logic、Collision、Effect、DX API反映を分離します。

## 2. Camera Pipeline

```text
Gameplay state and input
 -> choose rig/mode
 -> calculate pivot and target composition
 -> calculate desired yaw/pitch/distance/FOV
 -> smooth base camera
 -> solve camera collision
 -> add bounded procedural effects
 -> validate final transform
 -> apply to DX library
 -> publish camera snapshot
```

ShakeをCollision前に入れるか後に入れるかで壁貫通の挙動が変わります。大きいEffectはCollision前、微小な画面Shakeは後段でも構いません。

## 3. Camera StateとConfig

```cpp
struct ActionCameraConfig final
{
    float distance{6.5f};
    float targetHeight{1.35f};
    float shoulderOffset{0.45f};
    float minimumPitch{-0.65f};
    float maximumPitch{1.05f};
    float yawSpeed{2.8f};
    float pitchSpeed{2.2f};
    float positionSharpness{18.0f};
    float targetSharpness{22.0f};
    float collisionRadius{0.2f};
    float collisionMargin{0.05f};
    float fieldOfViewRadians{60.0f * DX_PI_F / 180.0f};
};

struct ActionCameraState final
{
    VECTOR position{};
    VECTOR target{};
    float yaw{};
    float pitch{0.25f};
    float distance{};
    float fieldOfViewRadians{};
    bool obstructed{};
};
```

Configは調整値、Stateは現在値です。ModeごとにConfig Presetを持ちます。

## 4. Camera Rig

```text
Follow target position
 -> Pivot offset
 -> Yaw/Pitch orbit
 -> Shoulder offset
 -> Desired eye
 -> Collision corrected eye
 -> Look target
```

Player Positionそのものを注視すると足元が中央になります。胸・頭付近のPivotを使います。

## 5. YawとPitch

YawはWorld Up周り、PitchはCamera Right周りの回転です。Pitchを制限し、Cameraが真上・真下を通過して反転するのを防ぎます。

```cpp
float WrapRadians(float angle)
{
    return std::remainder(angle, DX_PI_F * 2.0f);
}

void UpdateOrbitAngles(ActionCameraState& state,
                       const ActionCameraConfig& config,
                       float lookX, float lookY,
                       float deltaSeconds)
{
    state.yaw = WrapRadians(
        state.yaw + lookX * config.yawSpeed * deltaSeconds);

    state.pitch = std::clamp(
        state.pitch + lookY * config.pitchSpeed * deltaSeconds,
        config.minimumPitch,
        config.maximumPitch);
}
```

Mouse DeltaはすでにFrame間変位なので、入力層で秒基準へ変換済みか確認し、Delta Timeを二重に掛けません。

## 6. 感度と反転

```cpp
struct CameraInputSettings final
{
    float horizontalSensitivity{1.0f};
    float verticalSensitivity{1.0f};
    bool invertHorizontal{};
    bool invertVertical{};
};
```

Mouse、Gamepad Stick、Gyroは入力特性が違うため、感度とCurveを別々に持ちます。

## 7. Look Stick Curve

```cpp
float ApplyResponseCurve(float value, float exponent)
{
    const float sign = value < 0.0f ? -1.0f : 1.0f;
    return sign * std::pow(std::abs(value), exponent);
}
```

Exponentが1より大きいと中心付近が細かく、端で速くなります。Dead Zoneを適用後に再正規化し、Curveを掛けます。

## 8. Orbit Vector

```cpp
VECTOR BuildOrbitOffset(float yaw, float pitch, float distance)
{
    const float horizontal = std::cos(pitch) * distance;

    return VGet(
        std::sin(yaw) * horizontal,
        std::sin(pitch) * distance,
        std::cos(yaw) * horizontal);
}
```

使用座標系とCameraが見る方向により符号を調整します。AxisをDebug表示してTestします。

## 9. Desired Camera Transform

```cpp
struct CameraTransform final
{
    VECTOR position{};
    VECTOR target{};
    VECTOR up{VGet(0.0f, 1.0f, 0.0f)};
    float fieldOfViewRadians{};
};

CameraTransform BuildFreeCamera(
    VECTOR playerFeet,
    const ActionCameraState& state,
    const ActionCameraConfig& config)
{
    const VECTOR target = VAdd(
        playerFeet, VGet(0.0f, config.targetHeight, 0.0f));
    const VECTOR orbit = BuildOrbitOffset(state.yaw, state.pitch, config.distance);

    return {
        VSub(target, orbit),
        target,
        VGet(0.0f, 1.0f, 0.0f),
        config.fieldOfViewRadians
    };
}
```

## 10. Shoulder Offset

AimやLock CameraではCameraを左右へずらすとPlayerとTargetを同時に見やすくなります。

```cpp
CameraTransform ApplyShoulderOffset(CameraTransform transform, float amount)
{
    const VECTOR forward = VNorm(VSub(transform.target, transform.position));
    const VECTOR right = VNorm(VCross(VGet(0.0f, 1.0f, 0.0f), forward));
    transform.position = VAdd(transform.position, VScale(right, amount));
    transform.target = VAdd(transform.target, VScale(right, amount * 0.35f));
    return transform; // Positionだけでなく補正済みTargetとFOVも保持して返す。
}
```

左右切替を用意し、壁際ではOffsetを減らします。

## 11. Smooth Damp

単純Lerpへ固定係数を使うとFrame Rate依存になります。指数Dampingなら秒基準にできます。

```cpp
float ExponentialAlpha(float sharpness, float deltaSeconds)
{
    return 1.0f - std::exp(-sharpness * deltaSeconds);
}

VECTOR DampVector(VECTOR current, VECTOR target,
                  float sharpness, float deltaSeconds)
{
    const float alpha = ExponentialAlpha(sharpness, deltaSeconds);
    return VAdd(current, VScale(VSub(target, current), alpha));
}
```

Position、Target、Distance、FOVへ別のSharpnessを使えます。

## 12. Spring Camera

速度を持つCritically Damped Springは、急変へ自然に追従します。

```cpp
struct SpringVector final
{
    VECTOR value{};
    VECTOR velocity{};
};
```

Overshootを許すか、最大速度を制限するかを用途別に決めます。Target Lock中心点はOvershootが少ない方が安定します。

## 13. Update Time

CameraはCharacter確定後に更新します。Pause中でもCameraを動かすならUnscaled Delta、Hit Stopへ同期するならGameplay Deltaを使います。

一Frame遅れを防ぐ基本順：

```text
Character simulation
 -> Animation/target points finalization
 -> Camera base update
 -> Camera collision
 -> Camera effects
 -> Render
```

## 14. Fixed UpdateとRender補間

CharacterがFixed Step、CameraがRender Frameなら、Characterの補間済みTransformを追従します。未補間のFixed Positionを追うとCameraが小刻みに揺れます。

Gameplay Aim RayがCameraに依存する場合、Simulation CameraとVisual Cameraの役割を分けます。

## 15. Camera Collisionの目的

- Cameraが壁の裏へ出ない。
- 壁の内側を映さない。
- PlayerとCamera間の遮蔽物を減らす。
- 狭い場所でも急激に跳ねない。

RayだけではCamera Near Planeの角が壁を貫通するため、Sphere/Capsule相当の太さを使います。

## 16. PivotからEyeまでのCapsule Query

DXライブラリ公式Sampleでは、TargetからEyeまでCapsule判定し、衝突時に安全な距離を探索しています。

```cpp
bool IsCameraPathBlocked(int stageModelHandle,
                         VECTOR pivot,
                         VECTOR eye,
                         float radius)
{
    MV1_COLL_RESULT_POLY_DIM result = MV1CollCheck_Capsule(
        stageModelHandle, -1, pivot, eye, radius);

    const bool blocked = result.HitNum > 0;
    MV1CollResultPolyDimTerminate(result); // 動的結果を必ず解放する。
    return blocked;
}
```

複数Stage Collision Modelがある場合は、最初のHitで打ち切れる構造にします。

## 17. Binary Searchで安全距離を探す

```cpp
float FindSafeCameraDistance(
    VECTOR pivot,
    VECTOR desiredEye,
    float radius,
    int stageModelHandle,
    int iterations)
{
    const VECTOR direction = VNorm(VSub(desiredEye, pivot));
    const float desiredDistance = VSize(VSub(desiredEye, pivot));
    float safeDistance = 0.0f;
    float blockedDistance = desiredDistance;

    for (int index = 0; index < iterations; ++index)
    {
        const float testDistance = (safeDistance + blockedDistance) * 0.5f;
        const VECTOR testEye = VAdd(pivot, VScale(direction, testDistance));

        if (IsCameraPathBlocked(stageModelHandle, pivot, testEye, radius))
            blockedDistance = testDistance;
        else
            safeDistance = testDistance;
    }

    return safeDistance;
}
```

理想Eye自体が安全な場合は探索せず、そのまま使います。

## 18. Collision Margin

安全距離から小さいMarginを引き、壁表面ぴったりでちらつくのを防ぎます。

```cpp
safeDistance = std::max(0.0f, safeDistance - collisionMargin);
```

Camera RadiusとNear Clipの両方を考慮します。

## 19. Pull-inとReturnの速度

壁が入った時は素早く近づけ、壁が消えた時はゆっくり元距離へ戻すと、壁越し表示とCameraの飛びを抑えられます。

```cpp
const float sharpness = collisionBlocked
    ? config.collisionPullInSharpness
    : config.collisionReturnSharpness;
```

ただしPull-inが遅いと壁を一瞬映します。衝突方向だけ即時補正し、復帰だけSmoothにする方法もあります。

## 20. Pivotも壁へ入れない

Player胸位置が壁の反対側、狭い天井内、Animationで地形へ入る場合、PivotからのQueryだけでは不安定です。Player Capsule付近から安全なPivotを作り、必要ならPivot自体をCollision面から押し出します。

## 21. Occluder Transparency

Cameraを近づけるだけではPlayerが見えない場合、Playerを遮るObjectを一時的にDither/Fadeできます。

- CameraとPlayer間にあるObjectだけ。
- Gameplay Collisionは維持する。
- Fade In/OutへHysteresisを持つ。
- Stage全体ではなくMaterial/Object単位で扱う。

DXライブラリでObject単位制御が難しい場合は、Camera Collisionを優先します。

## 22. Near/Far Clip

```cpp
SetCameraNearFar(0.1f, 1000.0f);
```

値はWorld Scaleへ合わせます。Nearを必要以上に小さくするとDepth精度が悪化します。Farは必要な最奥より少し先にします。

公式仕様上、`SetDrawScreen`、`SetGraphMode`、`ChangeWindowMode` 等で設定がResetされる場合があるため、画面再構築後に再適用します。

## 23. FOV

- 狭いFOV：Targetが大きく見えるが周辺状況を失う。
- 広いFOV：複数敵を見やすいが歪みが増える。
- Sprint時：少し広げて速度感を出す。
- Aim時：狭める。
- Finisher：演出用に制御する。

DegreeとRadianを型・関数名で区別します。

## 24. FOV Kick

```cpp
float ComposeFieldOfView(float baseFov,
                         float sprintOffset,
                         float attackOffset,
                         float accessibilityScale)
{
    const float effect = (sprintOffset + attackOffset) * accessibilityScale;
    return std::clamp(baseFov + effect,
                      35.0f * DX_PI_F / 180.0f,
                      100.0f * DX_PI_F / 180.0f);
}
```

複数Effectの加算上限を設け、急変をSmoothにします。

## 25. Target Lockの状態

```cpp
enum class TargetLockState
{
    Unlocked,
    Acquiring,
    Locked,
    Switching,
    LostGracePeriod
};

struct TargetLockData final
{
    std::optional<std::uint32_t> targetId;
    TargetLockState state{TargetLockState::Unlocked};
    float lostVisibilitySeconds{};
    float switchCooldownSeconds{};
};
```

Target Pointerを直接保持せず、Generation付きEntity IDやHandleを使います。

## 26. Lock Candidate条件

- 生存しTarget可能である。
- Playerから最大距離以内。
- Camera前方Cone内。
- Screen境界の許容範囲内。
- Team/Layerが対象条件に合う。
- 完全な遮蔽が一定時間以上続いていない。
- Cutsceneや特殊Stateで除外されていない。

Hard Filter後にScoreを計算します。

## 27. Candidate Score

```cpp
struct TargetCandidateMetrics final
{
    float normalizedScreenDistance{};
    float normalizedWorldDistance{};
    float viewAlignment{};
    float threat{};
    float visibility{};
    float previousTargetBonus{};
};

float ScoreCandidate(const TargetCandidateMetrics& value)
{
    return
        (1.0f - value.normalizedScreenDistance) * 4.0f +
        (1.0f - value.normalizedWorldDistance)  * 1.5f +
        value.viewAlignment                      * 2.0f +
        value.threat                             * 1.0f +
        value.visibility                         * 2.0f +
        value.previousTargetBonus;
}
```

各項目を0～1へ正規化し、Debug UIで内訳を表示します。

## 28. Screen-space距離

画面中央またはAim ReticleからCandidateの投影位置までを測ります。

```cpp
std::optional<VECTOR> ProjectVisiblePoint(VECTOR worldPosition)
{
    const VECTOR screen = ConvWorldPosToScreenPos(worldPosition);

    // 公式仕様：Near/Far範囲外ではX/Yを画面座標として使えない。
    if (screen.z <= 0.0f || screen.z >= 1.0f)
        return std::nullopt;

    return screen;
}
```

画面外も少し許容して選べるようMarginを設けます。

## 29. View Alignment

```cpp
float ComputeViewAlignment(VECTOR cameraForward,
                           VECTOR cameraPosition,
                           VECTOR targetPosition)
{
    const VECTOR toTarget = VNorm(VSub(targetPosition, cameraPosition));
    return std::clamp(VDot(VNorm(cameraForward), toTarget), -1.0f, 1.0f);
}
```

Dotが1に近いほどCamera正面です。ThresholdはAngleのCosineとして事前計算します。

## 30. Visibility

CameraまたはPlayerからTargetのAim PointへLine Queryします。

- 完全可視：高Score。
- 一部遮蔽：低Score。
- 短時間遮蔽：Lock維持。
- 長時間遮蔽：解除候補。

自分自身、Target自身、透明Object、攻撃だけ通すObjectをFilterします。

## 31. LockのHysteresis

現在TargetにはBonusを与え、別Targetが少しだけ高Scoreでも切り替えません。取得Coneより維持Coneを広くし、距離も維持側を長くします。

```text
Acquire angle <= 45°
Maintain angle <= 70°
Acquire distance <= 18m
Maintain distance <= 24m
```

## 32. Grace Period

柱の裏へ一瞬隠れただけで解除すると操作が不安定です。

```cpp
if (!visible)
    lostVisibilitySeconds += deltaSeconds;
else
    lostVisibilitySeconds = 0.0f;

if (lostVisibilitySeconds > maximumOcclusionGrace)
    Unlock();
```

Target死亡、削除、距離超過は即時解除する場合があります。

## 33. Target Point

Entity Originが足元だとCameraが低く向きます。胸、Lock Socket、弱点等のTarget Pointを使います。

```cpp
struct TargetableComponent final
{
    std::uint32_t ownerId{};
    int priority{};
    VECTOR fallbackOffset{VGet(0.0f, 1.2f, 0.0f)};
    std::optional<int> modelFrameIndex;
};
```

Animation後のFrame Positionを参照し、Renderと同じ姿勢を使います。

## 34. Lock Cameraの構図

PlayerとTargetの中点を注視点にするだけでは、距離差や身長差で構図が崩れます。

```cpp
VECTOR ComputeLockFocus(VECTOR playerPoint,
                        VECTOR targetPoint,
                        float targetWeight)
{
    return VAdd(playerPoint,
        VScale(VSub(targetPoint, playerPoint),
               std::clamp(targetWeight, 0.0f, 1.0f)));
}
```

通常はPlayer寄り、Targetが大きいBossでは弱点とPlayerが入るようWeightとHeightを変えます。

## 35. 二者を収めるDistance

PlayerとTargetの離隔が大きいほどCamera DistanceまたはFOVを増やします。

```cpp
float ComputeLockDistance(float separation,
                          float minimumDistance,
                          float distancePerMeter,
                          float maximumDistance)
{
    return std::clamp(
        minimumDistance + separation * distancePerMeter,
        minimumDistance,
        maximumDistance);
}
```

距離を伸ばしすぎるとCharacterが小さくなるため、Lock解除距離と連携させます。

## 36. Screen Boundsによる構図検証

Player PointとTarget Pointを投影し、Safe Frame内か確認します。外れた場合にDistance、Yaw、Target Weight、FOVを段階調整します。

一回で完全解を求めず、有限回反復または設計Curveで安定させます。

## 37. Target Switching

右入力なら現在TargetのScreen位置より右側、左なら左側のCandidateを優先します。

```cpp
float ComputeDirectionalSwitchScore(
    VECTOR currentScreen,
    VECTOR candidateScreen,
    float requestedDirection)
{
    const float horizontalDelta = candidateScreen.x - currentScreen.x;
    if (horizontalDelta * requestedDirection <= 0.0f)
        return -std::numeric_limits<float>::infinity();

    const float verticalPenalty = std::abs(candidateScreen.y - currentScreen.y);
    return std::abs(horizontalDelta) * -0.5f - verticalPenalty * 0.25f;
}
```

最も近い横方向Candidateを選ぶよう、Score符号と正規化をTestします。

## 38. Switch Inputの誤作動防止

- Stick FlickのThreshold。
- Neutralへ戻るまで再入力を受けない。
- 短いCooldown。
- Camera LookとのAction分離。
- Target不在時はFree Lookへ戻す。

Stickを倒し続けただけで連続切替しないようにします。

## 39. Large BossとLock Point

Boss全体へ一つだけLock Pointを置くとCamera角度が極端になります。

- 胸・頭・部位ごとにPointを持つ。
- 近距離では近い部位、遠距離では中心を使う。
- 破壊済み部位を除外する。
- Phaseで有効Pointを切り替える。
- Camera用PointとAttack用Pointを分けられる。

## 40. Multiple Enemy Combat

Hard Lockは一体へ固定しますが、Cameraは周囲の脅威も考慮できます。

- 画面外から攻撃するEnemy方向へ余白を作る。
- 高Threat Enemyの重心を弱く構図へ混ぜる。
- ただし主Targetを画面中央から追い出さない。
- Camera自動回転量に上限を設ける。

Combat DirectorからThreat情報を受けます。

## 41. Soft Lock / Aim Assist

Hard Lockせず、攻撃方向だけ近いCandidateへ補正します。

```text
input direction
 -> candidate within assist cone
 -> angle and distance score
 -> limited angular correction
 -> collision-valid attack direction
```

Cameraを勝手に大きく回さず、Playerが意図した方向を優先します。

## 42. LockとCharacter Rotation

Lock中はCharacterがTargetを向くStrafeが基本です。ただし回避、攻撃、Knockback等は別のMovement Authorityが向きを決めます。

Camera SystemはTarget情報を提供し、Character Transformを直接書き換えません。

## 43. Lockと入力方向

Lock中の移動基準：

```text
forward = normalize(targetXZ - playerXZ)
right = cross(worldUp, forward)
move = right * inputX + forward * inputY
```

TargetとPlayerがほぼ同位置ならCamera基準へFallbackします。

## 44. Lock解除条件

- Userが解除Actionを押した。
- Target死亡・削除。
- 最大維持距離を一定時間超過。
- 長時間遮蔽。
- TargetがTargetableでなくなった。
- Cutscene、Traversal等のModeへ入った。
- Camera構図を維持できない特殊状態。

解除理由をEventへ含め、UIとCombatが適切に反応できるようにします。

## 45. Camera Mode

```cpp
enum class CameraMode
{
    FreeFollow,
    TargetLock,
    Aim,
    Sprint,
    Traversal,
    Finisher,
    Cutscene,
    PhotoMode
};
```

巨大なswitchへ全処理を詰めず、ModeごとのRigが共通Camera Requestを生成します。

## 46. Camera Request

```cpp
struct CameraRequest final
{
    VECTOR pivot{};
    VECTOR focus{};
    float yaw{};
    float pitch{};
    float distance{};
    float fieldOfViewRadians{};
    float blendDurationSeconds{0.25f};
    int priority{};
};
```

Camera Directorが優先度とBlendを管理します。Cutsceneが終わったらFree/Lockの以前Stateへ戻せます。

## 47. Mode Blend

PositionとTargetを別々に線形補間すると軌道が不自然になる場合があります。Pivot、Yaw/Pitch、Distance、FOVのRig Parameterを補間してTransformを再構築すると安定します。

Angleは最短方向へ補間します。

```cpp
float LerpAngle(float from, float to, float alpha)
{
    const float difference = std::remainder(to - from, DX_PI_F * 2.0f);
    return from + difference * alpha;
}
```

## 48. Cutsceneとの受け渡し

Cutscene開始時にGameplay Camera Stateを保存し、終了時に現在Player位置から新しいGameplay Rigを再計算します。古いPositionへ単純に戻すとTeleportします。

入力Lock、HUD、Target Lockを誰が保持・復元するかをScene Stateで明文化します。

## 49. Camera Shakeの構造

```cpp
struct CameraShakeInstance final
{
    float durationSeconds{};
    float ageSeconds{};
    float positionAmplitude{};
    float rotationAmplitudeRadians{};
    float frequency{};
    std::uint32_t seed{};
    float distanceFalloff{};
};
```

Randomを毎Frame独立に引くと白色Noiseで不快になりやすいため、連続Noiseや減衰Sin波を使います。

## 50. Trauma方式

複数Impactを0～1のTraumaへ加算し、Shake強度を二乗等で非線形化します。

```cpp
trauma = std::clamp(trauma + addedTrauma, 0.0f, 1.0f);
trauma = std::max(0.0f, trauma - recoveryPerSecond * deltaSeconds);
const float shakeStrength = trauma * trauma;
```

小さいHitは控えめ、大きいHitは強く感じられます。

## 51. Shakeの軸

- Position Shake：World/Camera Localのどちらか。
- Rotation Shake：Yaw/Pitch/Roll。
- FOV Pulse：前後の衝撃感。
- Target Offset：注視点の揺れ。

全部を同時に大きくしません。Gameplay照準へVisual Shakeを反映するかも明示します。

## 52. Distance Falloff

Explosion位置からCameraまでの距離でShakeを弱めます。

```cpp
float SmoothFalloff(float distance, float inner, float outer)
{
    if (distance <= inner) return 1.0f;
    if (distance >= outer) return 0.0f;
    const float t = (distance - inner) / (outer - inner);
    return 1.0f - t * t * (3.0f - 2.0f * t);
}
```

`outer > inner` をValidationします。

## 53. Accessibility

- Shake強度0～100%。
- Camera自動回転強度。
- FOV Effect強度。
- Motion Blur軽減。
- Head Bob無効化。
- Camera Collisionの急なZoom軽減。
- Horizontal/Vertical反転。
- SensitivityをDevice別設定。

視認性に関わる設定は即時Previewと安全なDefaultを持ちます。

## 54. RecoilとHit Camera

攻撃者のAction EventからCamera Impulseを発行します。

```cpp
struct CameraImpulse final
{
    VECTOR localPositionOffset{};
    VECTOR localRotationOffset{};
    float attackSeconds{};
    float recoverySeconds{};
    int priority{};
};
```

Hit Stop中にImpulse時間を止めるか、Unscaled Timeで進めるかを演出要件で決めます。

## 55. Screen Direction Attack Indicator

World位置を投影し、画面外ならSafe Area端へClampします。Camera後方は単純な投影X/Yを使わず、Camera Local方向から左右を判断します。

`ConvWorldPosToScreenPos` のZが0～1外ならX/Yが有効な画面座標ではないという公式契約を守ります。

## 56. Mouse Picking

`ConvScreenPosToWorldPos` へ同じX/YとZ=0、Z=1を渡し、Near/Far上の二点からRayを作れます。

```cpp
const VECTOR nearPoint = ConvScreenPosToWorldPos(VGet(mouseX, mouseY, 0.0f));
const VECTOR farPoint  = ConvScreenPosToWorldPos(VGet(mouseX, mouseY, 1.0f));
const VECTOR rayDirection = VNorm(VSub(farPoint, nearPoint));
```

Screen ZはWorld距離ではなく0～1です。

## 57. Camera Snapshot

```cpp
struct CameraSnapshot final
{
    VECTOR position{};
    VECTOR target{};
    VECTOR forward{};
    VECTOR right{};
    VECTOR up{};
    float fieldOfViewRadians{};
    float nearClip{};
    float farClip{};
    std::uint64_t frameNumber{};
};
```

Character移動、UI、VFX、Audioが同じFrameのSnapshotを読みます。各SystemがDXライブラリのGlobal Cameraを別時点で取得しないようにします。

## 58. DXライブラリへ反映

```cpp
bool ApplyCamera(const CameraTransform& camera,
                 float nearClip,
                 float farClip)
{
    if (nearClip <= 0.0f || farClip <= nearClip)
        return false;

    if (SetCameraNearFar(nearClip, farClip) == -1)
        return false;

    if (SetCameraPositionAndTargetAndUpVec(
            camera.position, camera.target, camera.up) == -1)
        return false;

    // 使用DXライブラリ版のFOV設定APIへcamera.fieldOfViewRadiansを適用する。
    return true;
}
```

PositionとTargetが同一点、UpがView方向と平行、NaN/Infinityを事前検査します。

## 59. Cameraの直交Basis

```cpp
struct CameraBasis final
{
    VECTOR forward{};
    VECTOR right{};
    VECTOR up{};
};

std::optional<CameraBasis> BuildCameraBasis(VECTOR position,
                                            VECTOR target,
                                            VECTOR worldUp)
{
    VECTOR forward = VSub(target, position);
    if (VDot(forward, forward) < 0.000001f)
        return std::nullopt;

    forward = VNorm(forward);
    VECTOR right = VCross(worldUp, forward);
    if (VDot(right, right) < 0.000001f)
        return std::nullopt;

    right = VNorm(right);
    const VECTOR up = VNorm(VCross(forward, right));
    return CameraBasis{forward, right, up};
}
```

## 60. NaN防止

- Zero Vectorを正規化しない。
- `acos` 前にDotを-1～1へClampする。
- 除算前に分母を確認する。
- FOV、Near、Far、Distanceの範囲を検査する。
- 最終Transformの全成分へ `isfinite` を行う。

異常時は前Frameの安全CameraへFallbackし、入力とModeをLogへ残します。

## 61. Camera Teleport

Scene開始、Respawn、Fast TravelではSmooth追従せずCamera Stateを理想位置へ即時Resetします。

```cpp
void SnapCamera(ActionCameraState& state,
                const CameraTransform& desired)
{
    state.position = desired.position;
    state.target = desired.target;
    state.fieldOfViewRadians = desired.fieldOfViewRadians;
    // Spring velocity、shake、collision historyも用途に応じてResetする。
}
```

## 62. Camera Zone

Level側にCamera Hint Volumeを置けます。

- Distance制限。
- Yaw/Pitch制限。
- Fixed Camera方向。
- FOV Override。
- Collision Radius変更。
- Auto Rotate抑制。

Volume境界はBlendし、重複時のPriorityを決めます。Player操作を奪いすぎないようにします。

## 63. Narrow Space

狭い通路では通常Distanceを維持できません。

- Collisionで近づける。
- Shoulder Offsetを0へ寄せる。
- FOVを少し広げる。
- Player Modelを部分Fadeする。
- Camera ZoneでPitchを制限する。

複数補正を急に切り替えず、状態にHysteresisを持たせます。

## 64. Lock Cameraと壁

PlayerとTargetの中点をPivotにすると、中点が壁の向こうへ入る場合があります。Player側の安全Pivotから始め、Target方向のFocusだけを混ぜます。

Targetが見えない場合にCameraを壁越しへ移動させず、Grace Period後に解除します。

## 65. CameraをGameplay判定に使う注意

Visual CameraはShake、Collision、演出で毎Frame変わります。Attack方向を完全にVisual Cameraへ依存するとShake中に照準がずれます。

- Stable Aim Camera：Shake前のBasis。
- Render Camera：全Effect適用後。
- UI Camera：Render CameraまたはStable Cameraを用途別使用。

## 66. Target Selectionの更新頻度

全EnemyへのLine of Sightを毎Frame行うと高Costです。

```text
Broad phase each few frames:
  distance + view cone + targetable
Narrow phase:
  screen score + visibility for top candidates
Current target validation:
  every frame or priority frequency
```

候補ListをSpatial Queryから取得し、最大評価数を設けます。

## 67. Stable Sorting

Score同点や微小差でTargetが変わらないよう、最終KeyへStable Entity IDを使います。

```text
sort by score descending
then current target first
then stable entity ID ascending
```

浮動小数Scoreへ完全一致だけを期待せず、Switch Thresholdを使います。

## 68. Camera Collision Performance

- 理想Eyeが安全ならBinary Searchしない。
- Static Stage Collision情報を再利用する。
- 複数ModelをBoundsでBroad Phaseする。
- Binary Search反復数を固定する。
- Query数とHit Polygon数をProfilerへ出す。
- Camera Radiusを必要以上に大きくしない。

## 69. Debug Draw

- Pivot、Desired Eye、Corrected Eye。
- Pivot-to-Eye Capsule。
- Camera Forward/Right/Up。
- Near/Far Frustum。
- Free/Lock Focus Point。
- Acquire/Maintain Cone。
- CandidateへのLineとScore。
- Visibility Ray。
- Safe Screen Frame。
- Camera Zone Bounds。
- Shake前後Transform。

## 70. Target Debug Table

```text
ID | total | screen | distance | align | threat | visible | current bonus | reject reason
```

選ばれなかった理由を数値で見られるようにします。Target不可理由を最初の一件だけでなくBit Flagで保持すると便利です。

## 71. Camera Telemetry

```cpp
struct CameraDebugStats final
{
    int targetCandidateCount{};
    int visibilityQueryCount{};
    int collisionQueryCount{};
    int collisionSearchIterations{};
    float desiredDistance{};
    float actualDistance{};
    float frameFieldOfView{};
    bool lockActive{};
};
```

狭所や多数敵でCostが跳ねる状況を記録します。

## 72. よくある不具合：Cameraが震える

- Characterの未補間Fixed Transformを追っている。
- Camera CollisionのHit/No Hitが境界で往復する。
- PositionとTargetを異なる時間順で更新した。
- Moving Platform更新より先にCameraを更新した。
- Ground NormalをCamera Upへ直接使った。
- Shake Seedが毎Frame変わる。

Base、Collision後、Effect後の各Transformを別色で表示します。

## 73. よくある不具合：壁が一瞬映る

- Collision Pull-inをSmoothにしすぎた。
- RayだけでNear Plane幅を考慮していない。
- Query Radiusが小さすぎる。
- Desired EyeだけOverlapし、PivotからのSweepをしていない。
- Camera EffectをCollision後に大きく加えた。

侵入方向は即時補正し、復帰だけSmoothにします。

## 74. よくある不具合：Lock対象が飛ぶ

- Current Target Bonusがない。
- AcquireとMaintain条件が同じ。
- Visibility一Frame欠落で即解除する。
- Scoreの正規化範囲が不適切。
- Entity Pointerが削除後も残っている。
- 同点時の順序が非決定的。

## 75. よくある不具合：Target切替が直感と違う

- World左右で判定し、Screen左右を使っていない。
- Camera後方Candidateを含めた。
- 現在Targetからではなく画面中央から距離を測った。
- Vertical差を無視した。
- Stick Holdで連続切替した。
- UI Marker位置と評価Pointが違う。

## 76. よくある不具合：酔う

- 強いPosition/Rotation Shake。
- FOVの急変。
- Character入力とCamera自動回転が競合する。
- Head Bobが大きい。
- Frame Timeが不安定。
- Camera Collisionで距離が高速往復する。
- HorizonがRollし続ける。

効果別強度設定と完全無効化を用意します。

## 77. Unit Test

- Angle Wrapと最短補間。
- Pitch Clamp。
- Dead ZoneとResponse Curve。
- Candidate FilterとScore。
- Current Target BonusとSwitch Threshold。
- Screen左右の切替候補選択。
- Grace Period。
- DistanceからLock Camera距離へのCurve。
- Collision Binary Searchの収束。
- NaN/Zero Vector Fallback。

DX APIはAdapter越しにMockし、数学・選択Logicを単体Testします。

## 78. Scenario Test

- 一体のEnemyを正面・端・後方へ置く。
- 二体を左右・奥行き・上下へ置く。
- Targetが柱の裏へ一瞬/長時間隠れる。
- Target死亡・削除・Phase移行。
- 狭い通路、低い天井、Camera背後の壁。
- Moving Platform上。
- 30/60/120fpsで追従比較。
- 強いHit StopとShake。
- Bossの巨大部位Target。
- Accessibility強度0と100%。

## 79. Camera Director設計

```cpp
class CameraDirector final
{
public:
    CameraTransform Update(
        const CameraContext& context,
        const CameraInput& input,
        float deltaSeconds);

    void PushRequest(const CameraRequest& request);
    void AddImpulse(const CameraImpulse& impulse);
    void SnapToCurrentMode(const CameraContext& context);

private:
    ActionCameraState state_{};
    TargetLockData targetLock_{};
    std::vector<CameraShakeInstance> shakes_;
};
```

Camera ContextはPlayer、Target候補、Collision World、Settingsの読み取りSnapshotです。

## 80. Target Lock Service設計

```cpp
struct TargetSelectionResult final
{
    std::optional<std::uint32_t> targetId;
    float score{};
    std::string_view reason;
};

class TargetLockService final
{
public:
    TargetSelectionResult Acquire(const TargetQueryContext& context) const;
    TargetSelectionResult Switch(const TargetQueryContext& context,
                                 float screenDirection) const;
    bool ValidateCurrent(const TargetQueryContext& context,
                         std::uint32_t targetId,
                         float deltaSeconds);
};
```

Camera DirectorとCombatは選択結果を共有しますが、選択ServiceはCamera Transformを直接変更しません。

## 81. 実装チェックリスト

- [ ] Camera Update順をCharacter・Animation後へ固定した。
- [ ] MouseとStickの入力単位を分けた。
- [ ] Yaw Wrap、Pitch Clamp、Zero Vector Fallbackがある。
- [ ] Frame Rate非依存のSmoothを使った。
- [ ] 補間済みCharacter Transformを追従した。
- [ ] PivotからEyeまで太さのあるCollision Queryを行った。
- [ ] Collision結果を必ず終了処理した。
- [ ] Pull-inとReturnの速度を分けた。
- [ ] Near/FarをWorld Scaleへ合わせ、Reset後に再適用した。
- [ ] World-to-ScreenのZ範囲を確認した。
- [ ] Target候補をFilter後に正規化Scoreで評価した。
- [ ] Acquire/Maintain条件とGrace Periodを分けた。
- [ ] Target IDの寿命を安全に管理した。
- [ ] Screen方向によるTarget切替を実装した。
- [ ] PlayerとTargetをSafe Frame内へ収めた。
- [ ] Stable Aim CameraとRender Cameraを分けた。
- [ ] Shake、FOV、自動回転の軽減設定がある。
- [ ] Camera/Target Debug表示とTelemetryがある。

## 82. 練習課題

1. Yaw/Pitch Orbit Cameraを作る。
2. MouseとGamepad別の感度・反転・Curveを作る。
3. Frame Rate非依存Dampingを実装する。
4. Pivot-to-Eye Capsule Collisionを作る。
5. Binary Searchで安全Camera距離を求める。
6. 壁へのPull-in即時、Return緩和を比較する。
7. Candidate Scoreの内訳をDebug表示する。
8. Acquire/Maintain ConeとGrace Periodを作る。
9. 左右FlickでScreen方向Target切替を作る。
10. Player/Target離隔に応じてDistanceとFOVを調整する。
11. Seed付き連続NoiseのCamera Shakeを作る。
12. Stable AimとVisual Shakeを分離する。
13. Safe Frame、Camera Capsule、候補線をDebug描画する。
14. 30/60/120fpsと狭所Scenarioを自動再生する。

## 83. 理解確認

1. CameraをCharacterの子Transformにするだけでは不足する理由は何ですか。
2. Fixed Step Characterを補間せず追うと何が起きますか。
3. Camera CollisionにRayよりCapsuleが向く理由は何ですか。
4. Collision復帰だけを緩やかにする理由は何ですか。
5. Near Clipを極端に小さくすべきでない理由は何ですか。
6. World-to-Screen結果のZ確認が必要な理由は何ですか。
7. AcquireとMaintain条件を分ける理由は何ですか。
8. Target切替をWorld方向でなくScreen方向で評価する理由は何ですか。
9. Stable Aim CameraとRender Cameraを分ける理由は何ですか。
10. Camera EffectにAccessibility設定が必要な理由は何ですか。

## 84. この章の到達点

- Free Follow、Orbit、Shoulder Cameraを安定して構築できる。
- Frame Rate非依存の追従とMode Blendを実装できる。
- DXライブラリのCamera設定と座標変換契約を正しく扱える。
- Camera Collision、狭所補正、復帰Dampingを設計できる。
- Target候補のFilter、Score、維持、切替、解除を実装できる。
- PlayerとTargetを画面構図へ収め、Bossや複数敵へ対応できる。
- Shake、FOV、Recoilを安全に合成し軽減設定を提供できる。
- Debug表示、Telemetry、Unit/Scenario Testで不具合を再現できる。

## 85. 公式・関連資料

- [DXライブラリ：Camera関係関数](https://dxlib.xsrv.jp/function/dxfunc_3d_camera.html)
- [DXライブラリ：3D関係関数一覧](https://dxlib.xsrv.jp/function/dxfunc_3d.html)
- [DXライブラリ：3D Action基本](https://dxlib.xsrv.jp/program/dxprogram_3DAction.html)
- [DXライブラリ：3D Actionと追加Collision Model](https://dxlib.xsrv.jp/program/dxprogram_3DAction_CollObj.html)

Near/FarのReset条件、World/Screen変換のZ範囲、Collision結果の寿命は、利用中の公式Headerとリファレンスで再確認してください。
