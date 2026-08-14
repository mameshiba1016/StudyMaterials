# DXライブラリ：Character Controller

この章では、入力された方向へキャラクターを動かし、地面・壁・坂・段差と自然に接触させ、Animationや戦闘Actionとも矛盾しないCharacter Controllerを設計します。単純な `position += velocity * deltaTime` から始め、実戦で必要になる安定化処理まで段階的に組み立てます。

> Character Controllerは剛体Physicsそのものではありません。操作性を優先するKinematic Controllerとして設計し、必要な箇所だけ物理的挙動を採用します。

## 1. Character Controllerの責任

- 入力意図をWorld方向へ変換する。
- 加速・減速・最高速度を決める。
- 重力、Jump、落下速度を扱う。
- 地面、坂、壁、天井、段差との衝突を解決する。
- Root Motionや攻撃移動を統合する。
- 最終位置・速度・接地状態をGameplayへ返す。
- Animationへ速度や接地状態を提供する。

Input、Camera、Animation、Combat、Collisionを一Classへ詰め込まず、Controllerは移動解決へ集中します。

## 2. Controller Pipeline

```text
Input sampling
 -> movement intent
 -> camera-relative direction
 -> locomotion/combat request composition
 -> acceleration and gravity
 -> collision sweep
 -> depenetration and slide
 -> ground probe and snap
 -> final movement result
 -> animation parameters
```

順序を固定し、Frame途中で別SystemがPositionを直接上書きしないようにします。

## 3. StateとConfigを分離する

```cpp
struct CharacterControllerConfig final
{
    float radius{0.4f};
    float height{1.8f};
    float maximumGroundSpeed{6.0f};
    float groundAcceleration{35.0f};
    float groundDeceleration{45.0f};
    float airAcceleration{10.0f};
    float gravity{-25.0f};
    float maximumFallSpeed{-40.0f};
    float jumpSpeed{9.0f};
    float maximumSlopeDegrees{50.0f};
    float stepHeight{0.35f};
    float groundSnapDistance{0.2f};
};

struct CharacterControllerState final
{
    VECTOR position{};
    VECTOR velocity{};
    VECTOR groundNormal{VGet(0.0f, 1.0f, 0.0f)};
    bool grounded{};
    bool wasGrounded{};
    float timeSinceGrounded{};
};
```

Configは調整値、Stateは実行中の値です。ConfigをData化し、範囲検査して読み込みます。

## 4. Capsule形状

人型Characterには縦長Capsuleが扱いやすい形状です。

- Cylinder部で胴体の幅を表す。
- 上下の半球が段差や角へ引っ掛かりにくい。
- 回転しても幅が変わらない。
- 足元と頭上の位置を求めやすい。

`height >= radius * 2` を保証します。Modelの見た目とCollisionは一致させすぎず、操作しやすい少し細めのCapsuleを使うことがあります。

## 5. Capsule Segment

```cpp
struct Capsule final
{
    VECTOR bottomSphereCenter{};
    VECTOR topSphereCenter{};
    float radius{};
};

Capsule BuildCapsule(VECTOR feetPosition, const CharacterControllerConfig& config)
{
    const float upperCenterHeight = std::max(config.radius,
        config.height - config.radius);

    return {
        VAdd(feetPosition, VGet(0.0f, config.radius, 0.0f)),
        VAdd(feetPosition, VGet(0.0f, upperCenterHeight, 0.0f)),
        config.radius
    };
}
```

`position` が足元、Capsule中心、Model RootのどれかをProject全体で統一します。

## 6. Input Vector

```cpp
VECTOR BuildLocalInput(float horizontal, float vertical)
{
    VECTOR input = VGet(horizontal, 0.0f, vertical);
    const float lengthSquared = VDot(input, input);

    // 斜め入力が√2倍にならないよう、長さ1を超えた場合だけ正規化する。
    if (lengthSquared > 1.0f)
        input = VNorm(input);

    return input;
}
```

Analog Stickの小さい入力は強度として残します。常に正規化すると歩き分けが失われます。

## 7. Dead Zone

```cpp
float ApplyRadialDeadZone(float magnitude, float deadZone)
{
    if (magnitude <= deadZone) return 0.0f;
    return std::clamp((magnitude - deadZone) / (1.0f - deadZone), 0.0f, 1.0f);
}
```

X/Y別のAxial Dead Zoneは斜め方向を歪めます。移動StickにはRadial Dead Zoneが自然です。DeviceごとのDrift差に対応できる設定も検討します。

## 8. Camera-relative移動

Cameraの前・右をXZ平面へ投影し、入力をWorld方向へ変換します。

```cpp
VECTOR BuildCameraRelativeDirection(
    VECTOR cameraForward,
    VECTOR worldUp,
    VECTOR localInput)
{
    VECTOR planarForward = VSub(
        cameraForward,
        VScale(worldUp, VDot(cameraForward, worldUp)));

    if (VDot(planarForward, planarForward) < 0.000001f)
        planarForward = VGet(0.0f, 0.0f, 1.0f); // 真上・真下CameraへのFallback。
    else
        planarForward = VNorm(planarForward);

    VECTOR right = VNorm(VCross(worldUp, planarForward));
    VECTOR direction = VAdd(
        VScale(right, localInput.x),
        VScale(planarForward, localInput.z));

    const float lengthSquared = VDot(direction, direction);
    return lengthSquared > 1.0f ? VNorm(direction) : direction;
}
```

Cross Productの順序でRightの符号が変わります。使用座標系でTestします。

## 9. Target Lock中の基準

- Free Move：Camera forward/right基準。
- Target Lock：Targetへの方向とそのRight基準。
- Attack中：攻撃定義が入力・Target・現在向きの混合を決める。

移動方向とCharacter向きを分け、Strafe中はTargetを向いたまま横移動できるようにします。

## 10. Desired Velocity

```cpp
VECTOR ComputeDesiredVelocity(VECTOR worldDirection,
                              float inputMagnitude,
                              float maximumSpeed)
{
    if (VDot(worldDirection, worldDirection) < 0.000001f)
        return VGet(0.0f, 0.0f, 0.0f);

    return VScale(VNorm(worldDirection),
                  maximumSpeed * std::clamp(inputMagnitude, 0.0f, 1.0f));
}
```

Walk、Run、Aim、Guard、Attack等のStateでmaximumSpeed倍率を変えます。

## 11. 加速と減速

```cpp
float MoveTowards(float current, float target, float maximumDelta)
{
    if (current < target) return std::min(current + maximumDelta, target);
    return std::max(current - maximumDelta, target);
}

VECTOR MoveTowardsPlanar(VECTOR current, VECTOR target, float maximumDelta)
{
    VECTOR difference = VSub(target, current);
    difference.y = 0.0f;
    const float distance = VSize(difference);

    if (distance <= maximumDelta || distance < 0.000001f)
        return VGet(target.x, current.y, target.z);

    const VECTOR step = VScale(difference, maximumDelta / distance);
    return VAdd(current, step);
}
```

入力ありはAcceleration、入力なしは強めのDecelerationを使うとAction Gameらしく止まりやすくなります。

## 12. 指数Damping

Frame Rateに依存しにくい補間です。

```cpp
float DampAlpha(float sharpness, float deltaSeconds)
{
    return 1.0f - std::exp(-sharpness * deltaSeconds);
}
```

ただし一定Accelerationの運動と指数収束はFeelが異なります。用途に応じて使い分けます。

## 13. 重力

```cpp
void ApplyGravity(CharacterControllerState& state,
                  const CharacterControllerConfig& config,
                  float deltaSeconds)
{
    if (state.grounded && state.velocity.y <= 0.0f)
    {
        // 地面へ密着する小さい下向き速度。0固定より段差追従が安定する場合がある。
        state.velocity.y = -2.0f;
        return;
    }

    state.velocity.y += config.gravity * deltaSeconds;
    state.velocity.y = std::max(state.velocity.y, config.maximumFallSpeed);
}
```

Gravity、Jump Speed、最大落下速度は互いにJump高さ・滞空時間へ影響します。

## 14. Jumpの式

上向きを正、Gravityの大きさを `g > 0`、Jump最高点の高さを `h` とすると：

```text
jumpSpeed = sqrt(2 * g * h)
timeToApex = jumpSpeed / g
```

DesignerがJump高さと最高点到達時間を指定し、Gravityと初速を逆算する方式も使えます。

## 15. Coyote Time

崖から少し離れた直後でもJumpを許可します。

```cpp
bool CanUseGroundJump(const CharacterControllerState& state,
                      float coyoteDuration)
{
    return state.grounded || state.timeSinceGrounded <= coyoteDuration;
}
```

数Frameの入力・判定誤差を吸収し、意図したJumpが出やすくなります。

## 16. Jump Buffer

着地直前に押したJumpを短時間保持します。

```cpp
struct BufferedAction final
{
    float remainingSeconds{};

    void Press(float duration) { remainingSeconds = duration; }
    void Update(float dt) { remainingSeconds = std::max(0.0f, remainingSeconds - dt); }
    bool Consume()
    {
        if (remainingSeconds <= 0.0f) return false;
        remainingSeconds = 0.0f;
        return true;
    }
};
```

着地後にControllerがGroundedを確定してからBufferを消費します。

## 17. Variable Jump Height

Jump Buttonを早く離した場合に上昇速度を減らします。

```cpp
if (jumpReleased && state.velocity.y > 0.0f)
{
    state.velocity.y *= 0.45f;
}
```

Button Hold中に重力を弱める方法もあります。入力遅延とAnimationの見え方をTestします。

## 18. Fixed Step

Collision解決はFrameごとのDeltaが大きく変わると結果が不安定になります。Fixed StepでControllerを更新し、描画時に補間します。

```text
accumulator += frameDelta
while accumulator >= fixedDelta:
    simulate character(fixedDelta)
    accumulator -= fixedDelta
render interpolation = accumulator / fixedDelta
```

一Frameの最大Step数を制限し、長時間停止後のSpiral of Deathを防ぎます。

## 19. Tunneling

現在位置だけで重なり判定すると、高速移動時に薄い壁を飛び越えます。開始形状から終了形状までSweepし、最初の衝突時刻を求めます。

DXライブラリのCapsule Queryで候補Polygonを得て、移動を小分けまたは二分探索する方法を組み合わせられます。

## 20. Stage Collisionの準備

```cpp
if (MV1SetupCollInfo(stageModelHandle, -1, 32, 8, 32) == -1)
{
    throw std::runtime_error("Stage collision setup failed");
}
```

分割数はStage形状とQuery分布で調整します。細かすぎても構築Memoryと管理Costが増えます。

静的Stageなら一度構築後に更新不要です。動くCollision ModelはPosition・Animation変更後に `MV1RefreshCollInfo` が必要です。

## 21. Collision結果のRAII

Sphere/Capsule Queryが返す `MV1_COLL_RESULT_POLY_DIM` は動的領域を持つため、使用後に必ず終了処理します。

```cpp
class UniqueCollisionResult final
{
public:
    explicit UniqueCollisionResult(MV1_COLL_RESULT_POLY_DIM result) noexcept
        : result_(result), active_(true) {}

    ~UniqueCollisionResult()
    {
        if (active_) MV1CollResultPolyDimTerminate(result_);
    }

    UniqueCollisionResult(const UniqueCollisionResult&) = delete;
    UniqueCollisionResult& operator=(const UniqueCollisionResult&) = delete;

    [[nodiscard]] int HitCount() const noexcept { return result_.HitNum; }
    [[nodiscard]] MV1_COLL_RESULT_POLY Get(int index) const
    {
        return MV1CollCheck_GetResultPoly(result_, index);
    }

private:
    MV1_COLL_RESULT_POLY_DIM result_{};
    bool active_{};
};
```

## 22. Collision Worldを抽象化する

```cpp
struct CharacterHit final
{
    VECTOR position{};
    VECTOR normal{};
    float distance{};
    std::uint32_t colliderId{};
};

class ICharacterCollisionWorld
{
public:
    virtual ~ICharacterCollisionWorld() = default;
    virtual std::vector<CharacterHit> OverlapCapsule(const Capsule& capsule) = 0;
    virtual std::optional<CharacterHit> SweepCapsule(
        const Capsule& capsule, VECTOR displacement) = 0;
    virtual std::optional<CharacterHit> Raycast(
        VECTOR start, VECTOR end) = 0;
};
```

ControllerをDXライブラリの結果構造体から切り離すと、数学Testや将来のPhysics Backend変更が容易になります。

## 23. Move-and-Slide

```text
remaining displacement = desired displacement
repeat up to maximum iterations:
  sweep along remaining
  no hit -> move all and finish
  hit -> move to contact minus skin width
  project remaining displacement onto hit plane
```

Planeへの投影は次です。

```cpp
VECTOR ProjectOnPlane(VECTOR vector, VECTOR unitNormal)
{
    return VSub(vector, VScale(unitNormal, VDot(vector, unitNormal)));
}
```

反復上限を設け、複雑な角で無限Loopしないようにします。

## 24. Skin Width

衝突面ぴったりへ置くと浮動小数誤差で次Frameに重なります。接触点より数mm手前で止めるSkin Widthを使います。

Skinが大きすぎると壁から浮き、小さすぎると振動します。Character Scaleに合わせます。

## 25. Depenetration

Platform移動、Teleport、精度誤差で開始時点から重なる場合があります。

1. OverlapしたPolygonを集める。
2. 各面の法線と侵入量から押出し候補を作る。
3. 最小限の補正を反復する。
4. 最大補正距離・反復数を超えたら安全地点へ戻す。

複数面の押出しを単純加算すると振動するため、優先面や制約解法を検討します。

## 26. Ground Probe

足元から短いLine/Sphere/Capsule Castを下へ行います。

```cpp
struct GroundHit final
{
    VECTOR position{};
    VECTOR normal{};
    float distance{};
    bool walkable{};
};
```

中心Lineだけでは段差角でGroundを失いやすいため、複数ProbeまたはSphere/Capsuleを使います。

## 27. Walkable Slope

World UpとGround Normalの内積で傾斜を判定します。

```cpp
bool IsWalkableSlope(VECTOR normal, float maximumSlopeDegrees)
{
    const float cosineLimit = std::cos(maximumSlopeDegrees * DX_PI_F / 180.0f);
    return VDot(VNorm(normal), VGet(0.0f, 1.0f, 0.0f)) >= cosineLimit;
}
```

Angleを毎回 `acos` で求めず、Cosine同士を比較できます。

## 28. 坂上の移動

Desired VelocityをGround Planeへ投影します。

```cpp
VECTOR ConstrainToGround(VECTOR desired, VECTOR groundNormal)
{
    VECTOR alongSlope = ProjectOnPlane(desired, VNorm(groundNormal));
    const float originalSpeed = VSize(desired);

    if (VDot(alongSlope, alongSlope) < 0.000001f)
        return VGet(0.0f, 0.0f, 0.0f);

    return VScale(VNorm(alongSlope), originalSpeed);
}
```

速度を維持すると上り坂でも水平速度と同じSurface速度になります。上りで減速させたいなら別のDesign Curveを掛けます。

## 29. 急坂

Walkableでない面はGroundとして扱わず、壁に近いSurfaceとしてSlideします。

- 上向き速度を与えて登らせない。
- 重力を面へ投影して滑らせる。
- Jump開始面として使わない。
- Ground Snap対象から外す。

角度境界にHysteresisを設けるとGroundedの往復を減らせます。

## 30. Ground Snap

下り坂や小さい段差で毎Frame空中判定になるのを防ぎます。

```text
前Frame Grounded
現在は上昇中でない
Snap距離以内にWalkable Ground
 -> Ground位置へ下げ、Vertical Velocityを接地値へする
```

Jump直後にSnapすると地面へ引き戻されるため、正のVertical VelocityやJump抑制Timer中は無効にします。

## 31. Step Up

```text
水平Sweepが低い壁へHit
 -> Step Height分上へ移動可能か確認
 -> 上位置から水平Sweep
 -> 先で下向きProbe
 -> Walkable GroundならStep成功
```

頭上空間、最終Ground角度、段差奥行きも確認します。高い壁をStep処理で登らせてはいけません。

## 32. Step Down

低い下り段差へGround Snapし、落下Animationのちらつきを防ぎます。許容高さを超える場合は空中へ移行します。

## 33. Corner処理

二つの壁へ挟まれた場合、最初の壁Planeへ投影した移動が次の壁へ当たります。二つの法線のCross Productで交線方向を求めるCrease移動が使えます。

ほぼ平行・反対の法線では数値が不安定になるため、長さThresholdと停止Fallbackを持ちます。

## 34. Ceiling

上昇中に頭上へHitしたら、接触まで移動し、Vertical Velocityの上向き成分を0へします。CeilingをGroundとして扱わず、Ground Snapもしません。

Capsule上端とVisual HeadのOffsetをDebug表示します。

## 35. Moving Platform

```text
Platform previous transform -> current transform
 -> Character contact pointをPlatform localへ保持
 -> current transformでWorldへ戻す
 -> platform deltaをCharacterへ先に適用
 -> Character自身の移動を解決
```

平行移動だけでなく回転も考慮します。Platform Velocityを離れた瞬間に継承するかはDesign次第です。

## 36. 動くCollision Model

DXライブラリでCollision Modelを移動・Animationした後は `MV1RefreshCollInfo` を呼ばなければ、Queryは最後に更新した形状のままです。

```cpp
MV1SetPosition(platformModelHandle, newPosition);

if (MV1RefreshCollInfo(platformModelHandle, -1) == -1)
{
    // Collision更新失敗。見た目と当たり判定がずれるため記録する。
}
```

更新は高Costなので、静的Stageへ毎Frame呼ばないようにします。

## 37. One-way Platform

下から通り抜け、上から着地できる床です。

- Characterが上から下へ移動中。
- 前Frameの足位置が床面以上。
- 現在の移動で床面を横切る。
- Drop-through入力中でない。

この条件でだけCollision候補に含めます。

## 38. Ledge

足元Probeの一部だけがGroundへ触れる場合、見た目と操作感を選びます。

- Capsule中心が支えられるまで立てる。
- 足の一定割合が外れたら落ちる。
- Input方向へ小さなEdge抵抗をかける。
- Combat中は落下防止Assistを有効にする。

見えない壁を多用すると操作の予測可能性を損ないます。

## 39. Ledge GrabとVault

通常移動とは別のTraversal検出です。

```text
胸高さの前方Sweep: 壁あり
頭上Sweep: 空間あり
上面Down Probe: Walkable面あり
高さ・奥行きが許容範囲
 -> Vault/Ledge候補
```

開始前にAnimation軌道全体の空間を検査し、途中で壁へ埋まらないようにします。

## 40. Character Rotation

移動方向へ向ける場合：

```cpp
float WrapAngle(float radians)
{
    // 2πをDX_PI_Fから作り、特定の補助定数があることへ依存しない。
    return std::remainder(radians, DX_PI_F * 2.0f);
}

float RotateTowards(float current, float target,
                    float maximumRadiansPerSecond, float deltaSeconds)
{
    const float difference = WrapAngle(target - current);
    const float maximumStep = maximumRadiansPerSecond * deltaSeconds;
    return current + std::clamp(difference, -maximumStep, maximumStep);
}
```

入力が小さい時は現在向きを保持し、Stick Driftで回転しないようにします。

## 41. Visual RotationとCollision Rotation

縦CapsuleはYaw回転しても形状が同じです。Gameplay向き、Model向き、Camera向きを分けると、素早い入力応答と滑らかなVisual回転を両立できます。

Hitbox生成時にどの向きを参照するか明示します。

## 42. Root Motion

Animation Rootの移動量をControllerへRequestとして渡します。

```cpp
struct MotionRequest final
{
    VECTOR locomotionDisplacement{};
    VECTOR rootMotionDisplacement{};
    VECTOR combatCorrection{};
};

VECTOR ComposeMotion(const MotionRequest& request,
                     float rootMotionWeight)
{
    return VAdd(
        VScale(request.locomotionDisplacement, 1.0f - rootMotionWeight),
        VAdd(VScale(request.rootMotionDisplacement, rootMotionWeight),
             request.combatCorrection));
}
```

ModelをAnimation側で直接移動させず、ControllerがCollision解決した結果へ従わせます。

## 43. Root Motionの時間区間

Animationの前回時刻から今回時刻までのRoot Transform差分を取ります。Loop境界、Blend中の複数Clip、Playback Speed、Hit Stopでの時間停止を考慮します。

Root Motionを抽出した後にAnimation Rootを原点へ戻し、二重移動を防ぎます。

## 44. Motion Warping

攻撃やFinisherの到達点をTargetへ合わせるため、Animation Root軌道を有限範囲で補正します。

- Warp開始・終了WindowをData化する。
- 平行移動と回転を別々に制限する。
- 一Frameの最大補正量を設ける。
- Collision結果を無視しない。
- Targetが消えた場合のFallbackを持つ。

Teleportのような大補正を隠す機能ではありません。

## 45. Movement Authority

複数Systemからの移動Requestに優先度を付けます。

```cpp
enum class MovementAuthority
{
    Locomotion,
    Dodge,
    AttackRootMotion,
    Knockback,
    ScriptedSequence,
    Teleport
};
```

「最後にPositionを書いたSystemが勝つ」設計を避け、Controllerが一つの最終Displacementへ合成します。

## 46. Dodge移動

Dodge開始時に方向を確定し、途中の入力追従量をDataで調整します。

- 完全固定方向：予測可能。
- 毎Frame入力追従：操作性は高いが曲がりすぎる。
- 前半固定・後半補正：両者の中間。

無敵時間、Collision、移動Curveは同じTimeline上で同期させます。

## 47. Attack移動

- Root Motion主体。
- 入力方向へのLunge。
- Targetへの有限吸着。
- Hit時だけ停止・減速。
- 空振り時は予定距離を進む。

Targetを壁越しに追い越さず、ControllerのSweep結果を必ず適用します。

## 48. Knockback

```cpp
struct Impulse final
{
    VECTOR velocityChange{};
    float decayPerSecond{};
    bool affectedByGroundFriction{};
};
```

通常移動Velocityへ加算するか別Channelで保持し、入力Accelerationと混ぜる順序を決めます。壁衝突時に反射、停止、壁Hit Reactionのどれを使うかは攻撃Data次第です。

## 49. Hit Stop

Hit Stop中にControllerのSimulation時間を止めるか、重力・Platformだけ進めるかを決めます。

- Gameplay Time停止。
- Camera/UIはUnscaled Timeで継続可能。
- Moving Platformとの相対位置を維持する。
- Hit Stop終了Frameに大きなDeltaを渡さない。

蓄積したRoot Motionを一度に適用してはいけません。

## 50. Teleport

通常Sweepを行わず位置を変更しますが、安全検査は必要です。

```text
Destination Capsule overlap?
 -> no: commit
 -> yes: nearby valid position search
 -> none: reject or return to last safe position
```

Teleport後はVelocity、Grounded、Platform参照、Camera smoothing、Trailを適切にResetします。

## 51. Last Safe Position

一定条件を満たす接地点を定期保存します。

- Walkable Ground上。
- Hazard外。
- 十分なCapsule空間。
- Moving Platform上ならPlatform IDとLocal座標も保存。

World外落下や深い侵入から復旧できます。毎Frame上書きせず、安全確認後に更新します。

## 52. World Boundary

極端に大きな座標、NaN、Infinityを検出します。

```cpp
bool IsFinite(VECTOR value)
{
    return std::isfinite(value.x) &&
           std::isfinite(value.y) &&
           std::isfinite(value.z);
}
```

異常時はLast Safe Positionへ戻し、直前のStateとCollision情報をLogへ残します。

## 53. Update順序

```text
1. Moving platform transform update
2. Platform collision refresh
3. Character input and action state
4. Animation root motion extraction
5. Character controller simulation
6. Final transform commit
7. Camera follow
8. Animation pose/model transform update
9. Combat hitbox generation
10. Render snapshot
```

要件に応じて調整しますが、依存方向を明文化します。

## 54. Modelへの反映

```cpp
void ApplyCharacterTransformToModel(
    int modelHandle,
    const CharacterControllerState& state,
    float visualYaw)
{
    MV1SetPosition(modelHandle, state.position);
    MV1SetRotationXYZ(modelHandle, VGet(0.0f, visualYaw, 0.0f));
}
```

Model RootとFeet基準が違う場合は一定Offsetを一箇所で適用します。

## 55. Animation Parameters

```cpp
struct LocomotionAnimationInput final
{
    float forwardSpeed{};
    float rightSpeed{};
    float verticalSpeed{};
    float normalizedSpeed{};
    float slopeAngleDegrees{};
    bool grounded{};
    bool justLanded{};
};
```

World VelocityをVisual facingのLocal軸へ投影し、Blend Treeへ渡します。Desiredではなく解決後Velocityを使うと壁に押し付けた時の足滑りを減らせます。

## 56. Foot Sliding

- Animation速度と実移動速度が違う。
- Root Motionを二重適用した。
- 壁で停止してもRun Animationが続く。
- Blend中のRoot差分が不連続。

Stride Warping、Playback Speed調整、解決後速度の使用、足IKを組み合わせます。

## 57. Ground Normalの平滑化

三角形境界でNormalが急変するとModelが揺れます。複数ProbeのNormalを重み付き平均し、時間補間します。ただしCollision判定用のWalkable判定には生Normalを使い、Visual傾きだけ平滑化する方法が安全です。

## 58. Characterの傾き

二足Characterは通常Yawだけ回し、坂に完全追従してPitch/Rollさせません。足IKで接地を表現します。Vehicleや四足ではGround Normalへ徐々に合わせます。

## 59. Pushable Object

Kinematic CharacterがDynamic Objectへ接触した時の力を設計します。

- Character速度の法線成分からImpulseを作る。
- Mass比と最大Impulseを制限する。
- 上に乗ったObjectを横へ弾き飛ばさない。
- Gameplayに不要なら押せないLayerへ分ける。

Character自身は反作用で操作不能にならないよう制限します。

## 60. Character同士の衝突

多数のEnemyを完全なCapsule同士で押し合うとJitterと詰まりが起きます。

- 主役と重要Enemyだけ強い衝突。
- Enemy同士はSoft Separation。
- Attack中は一部通り抜けを許す。
- NavigationのLocal Avoidanceと二重補正しない。
- 最大押出し量と優先順位を持つ。

## 61. Soft Separation

```cpp
VECTOR ComputeSeparation(VECTOR self, VECTOR other,
                         float desiredDistance, float strength)
{
    VECTOR difference = VSub(self, other);
    difference.y = 0.0f;
    const float distance = VSize(difference);

    if (distance <= 0.0001f || distance >= desiredDistance)
        return VGet(0.0f, 0.0f, 0.0f);

    const float weight = 1.0f - distance / desiredDistance;
    return VScale(difference, strength * weight / distance);
}
```

全PairのO(N²)を避け、Spatial Gridで近傍だけ調べます。

## 62. Networkを見据えたState

Local開発でも、Input、Simulation State、Visual Stateを分けるとReplayや将来の同期に強くなります。

```cpp
struct ControllerInputCommand final
{
    std::uint32_t simulationTick{};
    std::int16_t moveX{};
    std::int16_t moveY{};
    bool jumpPressed{};
};
```

PointerやFrame Deltaを保存せず、Tickと量子化入力を使います。

## 63. Determinism

Triangle列挙順、浮動小数、反復順で結果が変わり得ます。

- HitをDistance、Collider ID、Polygon IDで安定Sortする。
- 最大反復数を固定する。
- ThresholdをData化する。
- Randomを使わない。
- Simulation Tickを固定する。

Replay HashでPosition・Velocity・Groundedを比較します。

## 64. Debug Draw

- 現在Capsule。
- Sweep開始・終了Capsule。
- DesiredとActual Displacement。
- Velocity、Ground Normal、Wall Normal。
- Ground ProbeとSnap距離。
- Step Up三段階の形状。
- Walkable/Unwalkable面の色分け。
- Platform Delta。
- Last Safe Position。

Controllerの不具合は形状とVectorを画面で見ると早く特定できます。

## 65. Telemetry

```cpp
struct ControllerDebugStats final
{
    int sweepCount{};
    int overlapCount{};
    int testedPolygonCount{};
    int slideIterations{};
    int depenetrationIterations{};
    bool usedStepUp{};
    bool usedGroundSnap{};
};
```

Frame Timeと一緒に記録し、特定地形でQueryが急増する箇所を探します。

## 66. よくある不具合：壁へ張り付く

- Gravityや入力を壁法線へ正しく投影していない。
- Frictionを空中の壁へ適用している。
- Skin Widthが小さく毎Frame侵入する。
- Slide後の残り移動量を更新していない。
- 壁をGroundとして判定している。

## 67. よくある不具合：坂で跳ねる

- Ground Snapがない。
- Desired Velocityを地面Planeへ投影していない。
- Triangle境界でGround Normalが変動する。
- Vertical VelocityをGrounded時に正値のまま残した。
- Fixed Stepを使わずDeltaが大きい。

## 68. よくある不具合：段差へ登れない

- Step HeightよりCapsule Skinを含む障害が高い。
- 上方Clearanceを検査していない/厳しすぎる。
- Step後の水平Sweep開始位置が壁へ重なる。
- Landing面がWalkableでない。
- Model見た目の足位置とController Feetがずれている。

## 69. よくある不具合：床を抜ける

- 移動距離が大きいのにOverlapだけで判定した。
- Collision Infoを構築していない。
- 動くStageの `MV1RefreshCollInfo` を忘れた。
- Query結果を早く解放・解放し忘れた。
- Capsule端点やRadiusが不正。
- Groundを背面から判定している。

## 70. よくある不具合：入力感が重い

- Accelerationが低すぎる。
- Animation向きの補間をGameplay向きにも使った。
- Camera smoothing後の古い軸で移動した。
- Input Dead Zoneが大きすぎる。
- Fixed Step入力を一回しかSampleせず短いPressを落とした。
- 壁補正が接線速度まで消している。

入力から実速度までの各段階をGraph表示します。

## 71. Performance

- Stage Collision分割数を計測で決める。
- Query回数と候補Polygon数を表示する。
- 同じCapsuleへの重複Queryを統合する。
- 遠距離NPCの更新頻度を下げる。
- 静的CollisionへRefreshを呼ばない。
- 動的PlatformをLayer・Boundsで絞る。
- 結果VectorのCapacityを再利用する。

主役Controllerは入力応答優先、遠距離NPCはBudget優先にします。

## 72. Unit Test用の単純World

実ModelだけでTestすると原因が複雑です。数学的Primitiveで作ったMock Collision Worldを使います。

- 無限平面。
- 垂直壁。
- 角。
- 指定角度の坂。
- 指定高さの段差。
- 低い天井。
- 移動・回転Platform。

期待位置、Grounded、Velocity、Query回数を検証します。

## 73. Frame Rate Test

同じ入力Scriptを30、60、120、144fps相当で実行し、1秒後の位置とJump Apexを比較します。Fixed Stepなら一致しやすく、Variable Stepなら許容誤差を定義します。

## 74. Edge Case Test

- Delta 0、非常に大きいDelta。
- Zero input、最大斜め入力。
- Radius 0、Height不足のConfig拒否。
- 坂角度がLimitの直前・直後。
- Step Heightの直前・直後。
- 壁と床の同時接触。
- CeilingとFloorに挟まれる。
- PlatformからJumpする瞬間。
- Teleport先が埋まっている。
- NaN/Infinityが入ったState。

## 75. Character Controller Service

```cpp
struct CharacterMoveInput final
{
    VECTOR desiredWorldDirection{};
    float inputMagnitude{};
    bool jumpPressed{};
    bool jumpReleased{};
    VECTOR externalDisplacement{};
    float deltaSeconds{};
};

struct CharacterMoveResult final
{
    VECTOR previousPosition{};
    VECTOR position{};
    VECTOR velocity{};
    VECTOR groundNormal{};
    bool grounded{};
    bool landedThisStep{};
    bool hitCeiling{};
    bool hitWall{};
};

class CharacterController final
{
public:
    CharacterMoveResult Move(const CharacterMoveInput& input,
                             const CharacterControllerConfig& config,
                             CharacterControllerState& state,
                             ICharacterCollisionWorld& world);
};
```

入力と結果を明示すると、Animation、Camera、CombatがController内部へ侵入せず連携できます。

## 76. 実装チェックリスト

- [ ] Controller Positionの基準点を統一した。
- [ ] Capsule HeightとRadiusを検証した。
- [ ] 斜め入力とAnalog強度を正しく扱った。
- [ ] Camera真上・真下へのFallbackがある。
- [ ] 加速・減速・空中操作を分離した。
- [ ] Gravity、Jump、Coyote、Bufferを時間基準で処理した。
- [ ] Sweepと反復上限で高速移動を解決した。
- [ ] Skin WidthとDepenetrationがある。
- [ ] Walkable角度を内積で判定した。
- [ ] Ground SnapをJump中に無効化した。
- [ ] Step Upで上方・前方・下方を検査した。
- [ ] Moving Platformの平行移動と回転を扱った。
- [ ] 動くCollision Modelを明示更新した。
- [ ] Query結果を必ず終了処理した。
- [ ] Root MotionをController経由で適用した。
- [ ] 複数の移動権限を優先度で合成した。
- [ ] ModelとAnimationへ解決後速度を渡した。
- [ ] Capsule、Probe、Normal、補正量をDebug描画できる。
- [ ] Fixed ReplayとEdge Case Testがある。

## 77. 練習課題

1. Camera基準のAnalog移動を作る。
2. 加速・減速・最高速度をGraph表示する。
3. Jump高さとApex時間からGravityと初速を逆算する。
4. Coyote TimeとJump Bufferを実装する。
5. DX Stage ModelへCapsule Queryを行いRAIIで結果を解放する。
6. Move-and-Slideを最大4反復で実装する。
7. Ground ProbeとSlope Limitを作る。
8. Ground Snapの有無を坂道で比較する。
9. Step Up/Downを指定高さのBoxでTestする。
10. Moving Platformの回転追従を作る。
11. Root Motion攻撃を壁で正しく停止させる。
12. TargetへのMotion Warpingを上限付きで作る。
13. Damage Knockbackと通常入力を合成する。
14. 30/60/120fpsの固定入力結果を比較する。
15. Controller Debug Overlayを作る。

## 78. 理解確認

1. Character ControllerをDynamic Rigidbodyと分ける理由は何ですか。
2. Camera ForwardをXZ平面へ投影する理由は何ですか。
3. SweepがOverlapだけより高速移動へ強い理由は何ですか。
4. Skin Widthは何を防ぎますか。
5. Ground SnapをJump直後に無効化する理由は何ですか。
6. Slope判定でCosineを比較できる理由は何ですか。
7. Step Upに上・前・下の三検査が必要な理由は何ですか。
8. 動くModelで `MV1RefreshCollInfo` が必要な理由は何ですか。
9. Root MotionをModelへ直接適用してはいけない理由は何ですか。
10. 解決後VelocityをAnimationへ渡す利点は何ですか。

## 79. この章の到達点

- Camera/Target基準の入力を安定したWorld移動へ変換できる。
- 加速、重力、Jump AssistをFrame Rate非依存で実装できる。
- Capsule Sweep、Slide、Depenetration、Ground Probeを設計できる。
- 坂、段差、角、天井、Moving Platformを処理できる。
- DXライブラリのCollision情報と結果寿命を安全に管理できる。
- Locomotion、Root Motion、Dodge、Attack、Knockbackを一つの移動権限へ統合できる。
- Animation、Camera、Combatへ安定した結果を渡せる。
- Debug描画、Telemetry、Mock World、Replayで不具合を再現・検証できる。

## 80. 公式・関連資料

- [DXライブラリ：3D Model Collision関係](https://dxlib.xsrv.jp/function/dxfunc_3d_model_3.html)
- [DXライブラリ：3D関係関数一覧](https://dxlib.xsrv.jp/function/dxfunc_3d.html)
- [DXライブラリ：3D Action基本Sample](https://dxlib.xsrv.jp/program/dxprogram_3DAction.html)
- [DXライブラリ：3D Actionと追加Collision Model](https://dxlib.xsrv.jp/program/dxprogram_3DAction_CollObj.html)

Collision構築・更新条件、Query結果の後始末、引数の座標空間は、利用中の公式Headerとリファレンスで再確認してください。
