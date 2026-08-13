# Character Controller・Character Motor・移動設計

> 対象: Unity 6系のGameObject向け組み込み`CharacterController`を中心に扱います。Entities向け`com.unity.charactercontroller` Packageは別の仕組みです。

## 1. CharacterControllerとは

Unityの組み込み`CharacterController`は、直立したCapsule形状を使い、壁や床を貫通しないCharacter移動を作るComponentです。通常のDynamic `Rigidbody`とは異なり、force、質量、momentumに運動を任せません。

```text
Input
  ↓ Player Intent
Character State（Move / Attack / Dodge / Stun）
  ↓ 許可された移動要求
Character Motor
  ├─ camera-relative方向
  ├─ acceleration
  ├─ gravity / jump
  ├─ external force
  └─ ground / slope / step
        ↓
CharacterController.Move
        ↓
CollisionFlags / Ground Probe
        ↓
Animation・Camera・Combatへ結果を通知
```

「Character Controller」はComponent名、「Character Motor」はGameplay側で移動速度や接地を計算する自作責務、と区別します。

## 2. Rigidbody方式との選択

### CharacterController向き

- 素早く停止・方向転換する。
- 常に直立し、倒れない。
- 質量やmomentumより操作感を優先する。
- 坂・段差を歩行用規則で処理したい。

### Dynamic Rigidbody向き

- 物理objectとの相互作用を重視する。
- forceやmomentumを自然に使う。
- 倒れる、転がる、押される動作が必要。

### Custom Kinematic Motor向き

- 高度なsweep/slide/step規則が必要。
- rollback、決定的simulation、独自Physics Sceneが必要。
- 多様な重力方向や形状変更が必要。

万能な正解はありません。高速3DアクションではCharacterControllerまたはCustom Kinematic Motorが扱いやすい一方、knockbackやmoving platformを自分で設計する必要があります。

## 3. Componentの主な設定

- Center: Capsuleのlocal中心。
- Radius: Capsule半径。
- Height: Capsule全高。
- Slope Limit: 歩行可能な最大傾斜角。
- Step Offset: 自動で乗り越えられる段差高。
- Skin Width: 接触の余裕。小さすぎると引っ掛かりやjitterを招く。
- Min Move Distance: 小さい移動を無視する閾値。多くの場合は過度に上げない。
- Detect Collisions: collision検出。
- Enable Overlap Recovery: static geometryへ重なった場合の押し出し回復。

値はCharacterの見た目だけでなく、Worldの単位、階段高、door幅、移動速度に合わせます。Prefab Variantごとに無秩序に変えるとLevel Designの通過保証が崩れます。

## 4. Moveは重力を自動適用しない

`CharacterController.Move`へ「今回移動したい差分」を渡します。重力は自分で積分します。戻り値`CollisionFlags`は、その移動でSides/Above/Belowへ接触したかを表します。

```csharp
using UnityEngine;

[RequireComponent(typeof(CharacterController))]
public sealed class BasicCharacterMotor : MonoBehaviour
{
    [SerializeField] private float moveSpeed = 6.0f;
    [SerializeField] private float gravity = -25.0f;

    private CharacterController controller;
    private Vector2 moveInput;
    private float verticalSpeed;

    private void Awake()
    {
        controller = GetComponent<CharacterController>();
    }

    public void SetMoveInput(Vector2 input)
    {
        moveInput = Vector2.ClampMagnitude(input, 1.0f);
    }

    private void Update()
    {
        if (controller.isGrounded && verticalSpeed < 0.0f)
        {
            // 床へ軽く押し付け、浮き上がりを抑える。
            verticalSpeed = -2.0f;
        }
        else
        {
            verticalSpeed += gravity * Time.deltaTime;
        }

        Vector3 horizontal = new(moveInput.x, 0.0f, moveInput.y) * moveSpeed;
        Vector3 velocity = horizontal + Vector3.up * verticalSpeed;

        CollisionFlags flags = controller.Move(velocity * Time.deltaTime);

        if ((flags & CollisionFlags.Above) != 0 && verticalSpeed > 0.0f)
        {
            // 頭を天井へぶつけた。上昇速度を残さない。
            verticalSpeed = 0.0f;
        }
    }
}
```

これは原理用の最小例です。本番ではcamera-relative方向、加減速、ground probe、坂、外力、state policyを分離します。

## 5. MoveとSimpleMove

`Move`:

- 移動差分を渡す。
- 重力を自分で計算する。
- jump、knockback、任意速度を制御しやすい。

`SimpleMove`:

- 速度を渡し、簡易的な重力処理を含む。
- 一frameに一回の利用を想定する等、API契約を確認する。
- 高度なCharacter Motorでは`Move`の方が責務を明示しやすい。

同じframeに複数Systemが`Move`を呼ぶと、接触結果や移動合成が読みづらくなります。Motor一箇所で全channelを合成し、原則一度の最終移動へまとめます。

## 6. InputをWorld方向へ変換する

Third-person actionではstickの上をCamera前方へ対応させます。Cameraのpitchを含めると地面方向が歪むため、World Upへ投影します。

```csharp
public static Vector3 CameraRelativeDirection(
    Vector2 input,
    Transform cameraTransform,
    Vector3 up)
{
    Vector3 forward = Vector3.ProjectOnPlane(cameraTransform.forward, up);
    Vector3 right = Vector3.ProjectOnPlane(cameraTransform.right, up);

    if (forward.sqrMagnitude > 0.0001f)
    {
        forward.Normalize();
    }

    if (right.sqrMagnitude > 0.0001f)
    {
        right.Normalize();
    }

    return Vector3.ClampMagnitude(
        forward * input.y + right * input.x,
        1.0f);
}
```

Cameraが真上を向く等でprojectionがゼロに近づく場合のfallbackを決めます。lock-on中はTarget基準の前後左右へ切り替える設計もあります。

## 7. 加速と減速

入力から即座に最大速度へ変えるか、加速度を持たせるかで操作感が変わります。

```csharp
private Vector3 planarVelocity;

private void UpdatePlanarVelocity(Vector3 desiredDirection, float deltaTime)
{
    Vector3 targetVelocity = desiredDirection * maxSpeed;
    float rate = desiredDirection.sqrMagnitude > 0.0f
        ? acceleration
        : deceleration;

    planarVelocity = Vector3.MoveTowards(
        planarVelocity,
        targetVelocity,
        rate * deltaTime);
}
```

`Lerp(current, target, fixedT)`を毎frame使うとFPSで収束速度が変わります。`MoveTowards`へ単位`m/s² × s`を使うか、frame-rate independentな指数減衰を使います。

地上、空中、lock-on、attack中で加速度や最高速を変えます。ただしAnimator State名をMotorが直接読まず、Gameplay stateからMovement Policyを渡します。

## 8. 向きの制御

移動方向へ向く方式:

```csharp
private void RotateTowards(Vector3 direction, float deltaTime)
{
    if (direction.sqrMagnitude < 0.0001f)
    {
        return;
    }

    Quaternion target = Quaternion.LookRotation(direction, Vector3.up);
    transform.rotation = Quaternion.RotateTowards(
        transform.rotation,
        target,
        turnDegreesPerSecond * deltaTime);
}
```

Lock-on中はTarget方向、自由移動中は移動方向、attack中はrotation lockまたはlimited tracking、とstateごとの方針を決めます。Animation Root Motionも回転を動かすなら所有権を競合させません。

## 9. 接地判定を一つのboolで済ませない

`controller.isGrounded`は直近の`Move`中に接地したかという性質を持ちます。任意の時点で未来まで保証する床判定ではありません。

本番のGround State:

```csharp
public readonly struct GroundState
{
    public GroundState(
        bool isStable,
        Vector3 point,
        Vector3 normal,
        Collider collider,
        float distance)
    {
        IsStable = isStable;
        Point = point;
        Normal = normal;
        Collider = collider;
        Distance = distance;
    }

    public bool IsStable { get; }
    public Vector3 Point { get; }
    public Vector3 Normal { get; }
    public Collider Collider { get; }
    public float Distance { get; }
}
```

Capsule/Sphere Castで足元をprobeし、normal、slope、ground body、distanceを記録します。`isGrounded`、`CollisionFlags.Below`、probe結果の役割を決め、矛盾時の優先規則をtestします。

## 10. Ground Probe

Ray一本は段差の端や隙間で不安定です。Character下端に少し小さいSphere/Capsuleをcastします。

```csharp
private bool ProbeGround(out RaycastHit hit)
{
    Vector3 origin = transform.position + Vector3.up * groundProbeStart;

    return Physics.SphereCast(
        origin,
        groundProbeRadius,
        Vector3.down,
        out hit,
        groundProbeDistance,
        groundLayers,
        QueryTriggerInteraction.Ignore);
}
```

注意:

- 自分自身のColliderをLayerで除外する。
- Triggerを地面として扱うか明示する。
- radiusをController半径より少し小さくする。
- Cast開始時に地面へ重なっている場合の挙動をtestする。
- Debug Gizmoは実際の引数と同じdataを使う。

## 11. 坂

床normalとUpの角度を求めます。

```csharp
float slopeAngle = Vector3.Angle(hit.normal, Vector3.up);
bool walkable = slopeAngle <= controller.slopeLimit;
```

歩行可能な坂では、移動方向をsurface planeへ投影します。

```csharp
Vector3 slopeDirection = Vector3.ProjectOnPlane(desiredDirection, groundNormal);
slopeDirection = slopeDirection.normalized;
```

ただし投影後normalizeすると、急坂でも水平速度と同じsurface速度を維持します。World水平速度、surface速度、上り下り補正のどれを守るかを仕様にします。

歩行不可な急坂では:

- 壁として停止。
- 重力を坂面へ投影して滑る。
- Character専用のslide stateへ移る。

Slope Limitだけへ任せず、Ground ProbeとGameplay stateで安定判定を持つと調整しやすくなります。

## 12. 段差とStep Offset

`stepOffset`以下の障害を自動で登れます。高すぎると壁や台へ不自然に吸い上がります。

Level Designとの契約:

```text
Character radius
Character height
Step offset
Walkable slope
Door minimum width
Ceiling minimum height
Ledge minimum depth
```

stairsで揺れる場合は、Colliderを階段そのものではなく滑らかなrampで覆い、見た目MeshとGameplay collisionを分ける方法もあります。

## 13. Skin Width

Skin WidthはCapsule接触に余裕を持たせ、引っ掛かりやjitterを減らす重要値です。小さすぎる値を「精密」と考えないでください。大きすぎると見た目上の隙間やprobe調整へ影響します。

Character寸法を変更したら、Radius、Height、Center、Skin Width、Step Offset、Ground Probeを一組で再検証します。

## 14. 重力

単純なEuler積分:

```csharp
verticalSpeed += gravity * deltaTime;
verticalDisplacement = verticalSpeed * deltaTime;
```

落下速度へterminal velocityを設けるとtunnelingや数値暴走を抑えられます。

```csharp
verticalSpeed = Mathf.Max(
    verticalSpeed + gravity * deltaTime,
    -maxFallSpeed);
```

床上で`verticalSpeed = 0`にすると微小な段差でGroundから離れる場合があるため、負のstick-to-ground speedを使う設計があります。ただしMoving Platform上や坂で押し込みすぎないよう調整します。

## 15. Jump速度

目標jump高`h`と重力`g (< 0)`から初速を求められます。

```text
v² = -2gh
v = sqrt(-2gh)
```

```csharp
private void StartJump(float desiredHeight)
{
    verticalSpeed = Mathf.Sqrt(-2.0f * gravity * desiredHeight);
}
```

式は空気抵抗なし・一定重力の理想モデルです。Character Controllerでは天井、可変jump、apex補正等が加わります。

## 16. Coyote TimeとJump Buffer

- Coyote Time: 崖を離れた直後も短時間jumpを許す。
- Jump Buffer: 着地直前に押されたjump要求を短時間保持する。

```csharp
private double lastGroundedAt = double.NegativeInfinity;
private double jumpRequestedAt = double.NegativeInfinity;

public void RequestJump()
{
    jumpRequestedAt = Time.timeAsDouble;
}

private bool CanConsumeJump(double now)
{
    bool recentlyGrounded = now - lastGroundedAt <= coyoteSeconds;
    bool recentlyRequested = now - jumpRequestedAt <= jumpBufferSeconds;
    return recentlyGrounded && recentlyRequested;
}
```

消費した要求はsequenceまたは`-Infinity`で明示的に無効化します。Pause、Slow Motionの影響を受ける時計かも決めます。

## 17. 可変Jump

buttonを早く離したら上昇速度を切り、短いjumpにします。

```csharp
public void ReleaseJump()
{
    if (verticalSpeed > 0.0f)
    {
        verticalSpeed *= jumpCutMultiplier;
    }
}
```

Input Systemの`started/performed/canceled`と結びつけます。入力callbackで直接Transformを動かさず、Motorへcommandを渡します。

## 18. Air Control

空中で地上と同じ即時方向転換を許すかはGame Designです。

```csharp
float accelerationRate = ground.IsStable
    ? groundAcceleration
    : airAcceleration;
```

空中の現在velocityへ入力方向を加え、最高horizontal speedを制限します。jump時の慣性を残すか、action gameらしく強く操舵できるかをtestします。

## 19. CeilingとWall

`CollisionFlags.Above`で上昇速度を0へします。側面接触時は移動全体を止めず、wall normalへ向かう成分だけを除いてslideさせるのが自然です。組み込みControllerが行う解決に加え、独自velocity stateも結果へ合わせます。

```text
remainingVelocity =
    velocity - normal * min(dot(velocity, normal), 0)
```

壁へ向かい続けるvelocityを毎frame保持すると、離れた瞬間に不自然な加速が戻るため、解決後velocityを次frameのstateへ反映します。

## 20. Dodge

Dodgeは通常移動と異なるMovement Policyです。

```text
Dodge state
├─ directionを開始時にlockするか
├─ duration / curve
├─ turn許可
├─ collision response
├─ invincibility window
├─ attack cancel window
└─ interruption policy
```

```csharp
public Vector3 EvaluateDodgeVelocity(float normalizedTime)
{
    float speed = dodgeSpeedCurve.Evaluate(normalizedTime);
    return dodgeDirection * speed;
}
```

無敵と移動を同じboolにしません。Dodge移動が壁で止まっても無敵windowを続けるか、仕様として決めます。

## 21. 外力・Knockback

CharacterControllerはRigidbodyのように自動でforceへ反応しないため、Motorが外力channelを持ちます。

```csharp
private Vector3 externalVelocity;

public void AddImpulse(Vector3 impulseVelocity)
{
    externalVelocity += impulseVelocity;
}

private void DecayExternalVelocity(float deltaTime)
{
    externalVelocity = Vector3.MoveTowards(
        externalVelocity,
        Vector3.zero,
        externalDeceleration * deltaTime);
}
```

最終速度:

```text
locomotion
+ vertical gravity/jump
+ dodge/root motion
+ knockback/external velocity
+ platform motion
= motor request
```

各channelの加算・上書き・制限・優先順位を表にします。Stun中はlocomotionを0にしてもknockbackは残す、といった規則をState Machineが渡します。

## 22. Root Motionとの統合

AnimatorのRoot MotionもCharacterを動かす場合:

1. Animationがdelta position/rotationを提案。
2. Motorがcollision、slope、state ruleを適用。
3. CharacterController.Moveで確定。
4. 実移動量をAnimator/Gameplayへ返す。

AnimatorがTransformへ直接適用し、Motorも同時に`Move`すると二重移動になります。`OnAnimatorMove`でdeltaを取得しMotorへ渡す等、所有権を一つにします。

## 23. Moving Platform

CharacterControllerはDynamic Rigidbodyのようにplatform velocityを自動継承するとは限りません。Ground Probeでplatformを特定し、その位置・回転deltaをCharacter移動へ加えます。

```text
platform pose at previous tick
platform pose at current tick
  ↓ delta position / delta rotation
Character pointをplatform local spaceで追跡
  ↓
platform motion + player motion
```

注意:

- 親子化だけで解決するとscale/rotation/解除時velocityで問題が出る。
- Platform更新とCharacter更新の順序を固定する。
- Platformからjumpしたとき速度を継承するか決める。
- teleportしたPlatformへ追従させるか分ける。
- 回転platform上の接線速度を考慮する。

## 24. Rigidbodyを押す

`OnControllerColliderHit`で接触情報を得て、Dynamic Rigidbodyへforce/velocityを与えられます。しかしCharacterController自身が物理的質量を持つわけではないため、押す強さをGameplay規則で決めます。

```csharp
private void OnControllerColliderHit(ControllerColliderHit hit)
{
    Rigidbody body = hit.rigidbody;
    if (body == null || body.isKinematic)
    {
        return;
    }

    // 下向き接触で足元objectを横へ飛ばさない。
    if (hit.moveDirection.y < -0.3f)
    {
        return;
    }

    Vector3 push = Vector3.ProjectOnPlane(hit.moveDirection, Vector3.up);
    body.AddForce(push * pushForce, ForceMode.Acceleration);
}
```

callbackで無制限にforceを加えず、Layer、mass上限、pushable marker、速度上限を検査します。

## 25. Teleport

Spawn、Respawn、Character交代でteleportする場合:

1. 目的地CapsuleがWorldへ重ならないか確認。
2. Controllerを一時無効化する必要性を利用版で確認。
3. Transform位置を設定。
4. vertical/external velocityをresetまたは継承。
5. Ground Probeを再構築。
6. Camera damping履歴をwarp。
7. async/旧Character参照をinvalidate。

Overlap Recoveryはstatic geometryとの重なりを押し出せますが、正しいSpawn地点検証の代わりではありません。dynamic objectは別扱いです。

## 26. Capsule寸法変更

Crouch、巨大化等で`height`/`center`を変えるとき:

- 足位置を固定するようcenterを計算する。
- 立ち上がる頭上spaceをCapsule queryで確認する。
- skin width、step offset、probe originを更新する。
- 値変更によるnative controller再構築やcontact変化をProfilerで確認する。

```csharp
private bool CanStand(Vector3 bottom, Vector3 top, float radius)
{
    return !Physics.CheckCapsule(
        bottom,
        top,
        radius,
        obstacleLayers,
        QueryTriggerInteraction.Ignore);
}
```

## 27. Character交代

入力ownerはPlayer/Party層に置き、CharacterごとにInput Actionを購読させません。

```text
Player Input
 → Command Buffer
 → Party Controller
 → Current Character State
 → Current Character Motor
```

交代時に移す状態:

- world position/rotationを継承するか。
- vertical/external velocityを継承するか。
- ground/platform情報。
- dodge/attackのcancel。
- Camera target。
- collision有効化順序。
- 旧Characterのasync request generation。

新旧CharacterのColliderを同じ場所で同時に有効にすると押し出しが起こります。退場側disable、新側配置、overlap検証、active切替の順序を一つのtransactionにします。

## 28. Motorを状態機械から分離する

```csharp
public readonly struct MotorCommand
{
    public MotorCommand(
        Vector3 desiredDirection,
        bool jump,
        Vector3 externalVelocity,
        float maxSpeed)
    {
        DesiredDirection = desiredDirection;
        Jump = jump;
        ExternalVelocity = externalVelocity;
        MaxSpeed = maxSpeed;
    }

    public Vector3 DesiredDirection { get; }
    public bool Jump { get; }
    public Vector3 ExternalVelocity { get; }
    public float MaxSpeed { get; }
}
```

Combat State Machineは「このtickに許す意図」を作り、Motorは空間移動を解決します。MotorがAttack comboやSkill IDを知る必要はありません。

## 29. UpdateかFixedUpdateか

組み込みCharacterControllerはDynamic Rigidbodyではありません。`Move`をUpdateで呼ぶ構成も一般的です。ただし:

- Input sampleのphase。
- Animator update mode。
- Moving Platform/RigidbodyのFixedUpdate。
- Combat simulation tick。
- Camera LateUpdate。

これらを混ぜるとjitterします。CharacterControllerだから機械的にUpdate、Physicsだから機械的にFixedUpdateとせず、Project全体の時系列を設計します。

```text
Update: Inputをsample、Gameplay tick、Character Move
Fixed: Dynamic Rigidbody simulation
Late : Camera
```

Moving Rigidbodyとの相互作用が重要なら、補間とpose受渡しを明示します。

## 30. Animationへ渡す値

AnimatorへInput値をそのまま渡さず、Motorの実結果を使います。

- planar speed。
- local forward/right velocity。
- vertical speed。
- grounded/stable。
- slope angle。
- acceleration。
- turn rate。
- collision blocked。

壁へ向かってstickを倒しても実速度は0です。Input magnitudeだけでRun animationを出すと足滑りになります。

## 31. Debug表示

- CharacterController Capsule。
- Ground Probeの始点、終点、radius。
- ground point/normalとslope angle。
- desired、planar、external、final velocity。
- CollisionFlags。
- current movement state。
- coyote/jump buffer残時間。
- Platform IDとvelocity。

```csharp
private void OnDrawGizmosSelected()
{
    Gizmos.color = grounded ? Color.green : Color.red;
    Gizmos.DrawRay(groundPoint, groundNormal);

    Gizmos.color = Color.cyan;
    Gizmos.DrawRay(transform.position, planarVelocity);

    Gizmos.color = Color.magenta;
    Gizmos.DrawRay(transform.position, externalVelocity);
}
```

## 32. よくある不具合

- `Move`が自動で重力を掛けると思う。
- `Move`へ速度を渡し、`deltaTime`を掛け忘れる。
- 一frameに複数Systemが`Move`する。
- `isGrounded`だけで全接地仕様を決める。
- Input callbackから直接Transformを動かす。
- Camera forwardのY成分を残し、下向きに走る。
- `Lerp`固定係数でFPS依存の加速を作る。
- 壁衝突後も壁向きvelocityを保持する。
- Jump Bufferを消費せず着地ごとに再jumpする。
- Step Offsetを高くして壁へ登る。
- Ground Ray一本で段差の端から落ちる。
- Hit Stop中もunscaledでMotorが動く。
- Root MotionとMotorで二重移動する。
- Moving Platformへ親子化し、解除時に飛ぶ。
- Character交代時に二体のCapsuleが重なる。
- Controller寸法変更後にGround Probeを更新しない。

## 33. Test Matrix

| 観点 | Test |
|---|---|
| Frame rate | 30/60/120、stall |
| Terrain | 平面、坂上り/下り、急坂、階段、縁 |
| Space | 狭いdoor、低い天井、角、壁沿い |
| Jump | 通常、coyote、buffer、天井、可変高 |
| Movement | 即反転、斜め、camera真上、lock-on |
| State | attack、dodge、stun、knockback、死亡 |
| Platform | 並進、回転、上下、teleport、消滅 |
| Time | pause、slow motion、hit stop |
| Spawn | 空中、坂、重なり、Scene load直後 |
| Switch | 地上、空中、攻撃中、platform上 |

## 34. 設計チェックリスト

- CharacterController/Rigidbody/Custom Motorを選んだ理由があるか。
- 移動を最終確定するownerは一つか。
- Capsule寸法とLevel Design基準が共有されているか。
- camera-relative方向の退化caseがあるか。
- scaled/unscaled/fixedの時計を説明できるか。
- 接地情報がnormal、point、body、distanceを持つか。
- 坂、段差、天井、壁の仕様があるか。
- jump buffer/coyoteを一度だけ消費するか。
- dodge、root motion、knockbackの優先順位があるか。
- Moving Platformの更新順と速度継承が明確か。
- Teleport/交代時にvelocityとCamera履歴を処理するか。
- AnimatorはInputではなく実移動結果を読んでいるか。
- 判定をGizmoとoverlayで可視化できるか。

## 公式資料

- [Unity Manual: Introduction to character control](https://docs.unity3d.com/6000.0/Documentation/Manual/CharacterControllers.html)
- [Unity Manual: Character Controller component](https://docs.unity3d.com/6000.0/Documentation/Manual/class-CharacterController.html)
- [Unity API: CharacterController.Move](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/CharacterController.Move.html)
- [Unity API: CharacterController.isGrounded](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/CharacterController-isGrounded.html)
- [Unity API: CharacterController.stepOffset](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/CharacterController-stepOffset.html)
- [Unity API: CharacterController.enableOverlapRecovery](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/CharacterController-enableOverlapRecovery.html)
- [Unity API: MonoBehaviour.OnControllerColliderHit](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/MonoBehaviour.OnControllerColliderHit.html)
- [Unity Manual: Entities Character Controller package](https://docs.unity3d.com/6000.0/Documentation/Manual/com.unity.charactercontroller.html)

