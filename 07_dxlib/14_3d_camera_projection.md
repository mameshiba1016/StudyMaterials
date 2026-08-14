# 第14章 3D Camera・Projection

Cameraは「視点を置くObject」ではなく、Worldをどの座標系へ変換し、どの範囲をどの投影法で画面へ写すかを決めるSystemです。本章では数学、DXライブラリ状態、アクションCameraの振る舞いを分離します。

## 1. Camera Data

```cpp
struct CameraPose final
{
    Vec3 position{};
    Vec3 target{0, 0, 1};
    Vec3 up{0, 1, 0};
};

struct CameraLens final
{
    float verticalFovRadians = ToRadians(60.0F);
    float nearClip = 0.1F;
    float farClip = 1000.0F;
    float aspectRatio = 16.0F / 9.0F;
};
```

PoseとLensを分けると、位置を変えずFOVだけ演出できます。

## 2. View行列

View MatrixはWorldをCamera座標へ変換します。Camera World Transformの逆です。Cameraを右へ動かすとWorld全体がCamera空間で左へ動きます。

## 3. Camera基底

```cpp
struct CameraBasis final
{
    Vec3 right{};
    Vec3 up{};
    Vec3 forward{};
};

CameraBasis BuildCameraBasis(const CameraPose& pose)
{
    const Vec3 forward = NormalizeOr(pose.target - pose.position, {0,0,1});
    const Vec3 right = NormalizeOr(Cross(pose.up, forward), {1,0,0});
    const Vec3 up = NormalizeOr(Cross(forward, right), {0,1,0});
    return {right, up, forward};
}
```

座標系により外積順が変わるため、右・上・前をGizmo表示して確認します。

## 4. 特異姿勢

PositionとTargetが同じ、またはForwardとUpが平行だと姿勢を一意に作れません。入力値を検証し、直前の有効姿勢やFallback Upを使います。

## 5. DXライブラリへ適用

```cpp
int ApplyCamera(const CameraPose& pose, const CameraLens& lens)
{
    if (lens.nearClip <= 0.0F || lens.farClip <= lens.nearClip)
        return -1;

    if (SetCameraNearFar(lens.nearClip, lens.farClip) == -1)
        return -1;

    if (SetupCamera_Perspective(lens.verticalFovRadians) == -1)
        return -1;

    return SetCameraPositionAndTargetAndUpVec(
        ToDx(pose.position), ToDx(pose.target), ToDx(pose.up));
}
```

## 6. UpVecY版

```cpp
SetCameraPositionAndTarget_UpVecY(ToDx(position), ToDx(target));
```

Worldの+Yを基本Upとして姿勢を計算する簡易版です。Rollや球面上のCameraでは明示Up版を使います。

## 7. Angle版

`SetCameraPositionAndAngle`は位置、垂直・水平・捻り角をRadianで指定します。Euler順と符号を実測し、独自Quaternionとの二重管理を避けます。

## 8. View Matrix直接指定

`SetCameraViewMatrix`は独自行列を設定する場合に使います。Position/Target APIとの同時利用でどちらが最後に有効か曖昧にせず、Camera Backendの入口を一つにします。

## 9. Perspective Projection

遠いObjectほど小さく見える投影です。`SetupCamera_Perspective`のFOVはRadianです。初期値は公式説明で60度です。

## 10. Vertical FOV

FOVが縦角か横角かで画面比率変更時の見え方が変わります。DX APIの契約を確認し、Game Data名へ`verticalFov`などを含めます。

## 11. FOVの効果

- 狭いFOV: 望遠。圧縮された見え方、揺れが大きく感じやすい。
- 広いFOV: 広角。速度感が出るが端が歪む。

Camera距離変更とFOV変更は同じ画面Sizeに見えてもPerspectiveが異なります。

## 12. Aspect Ratio

```cpp
[[nodiscard]] float AspectRatio(int width, int height)
{
    return height > 0 ? static_cast<float>(width) / height : 1.0F;
}
```

Window Resize、Render Target、Split ScreenごとにViewport比率が変わります。

## 13. Orthographic Projection

距離でSizeが変わらない投影です。Map、Editor、2.5D、Shadow用途に向きます。PerspectiveとOrthoの設定APIは排他的なのでPassごとに明示します。

## 14. Near Clip

Nearより手前は描画されません。Nearを極端に小さくするとDepth Buffer精度が悪化します。公式資料も、不都合のない範囲でNearを大きくするよう注意しています。

## 15. Far Clip

必要最奥より少し遠くへ置きます。巨大値へすれば万能ではなくDepth精度とCulling効率へ影響します。遠景を別表現にする方法もあります。

## 16. Depth精度

Perspective Depthは線形に分布せずCamera近傍へ精度が集中します。Near/Far比が極端だとZ-fightingが増えます。

## 17. Z-fighting

近い二面のDepth値を区別できずちらつく現象です。

- Nearを大きくする。
- Farを必要範囲へ縮める。
- 面を完全に重ねない。
- World Scaleを適正化する。
- 必要ならDepth Biasを使う。

## 18. 状態のReset

公式仕様では`SetDrawScreen`、`SetGraphMode`、`ChangeWindowMode`でNear/Far設定がResetされます。Render Target切替後、各3D Pass開始時にCameraとLensを再適用します。

## 19. Camera Pass

```cpp
void BeginWorldPass(int target,
                    const CameraPose& pose,
                    const CameraLens& lens,
                    int width, int height)
{
    SetDrawScreen(target);
    SetDrawArea(0, 0, width, height);
    ApplyCamera(pose, lens); // 描画先切替の後で再設定。
}
```

## 20. Frustum

Near、Far、Left、Right、Top、Bottomの6平面で見える錐台を表します。Perspectiveでは先が広がり、Orthoでは箱型です。

## 21. Frustum Culling

ObjectのBounding Sphere/AABBが全てFrustum外なら描画候補から除外します。完全なMesh判定より安価なBoundsを使います。

## 22. Plane Test

Sphere中心からPlaneへのSigned Distanceが`-radius`未満なら、そのPlaneの完全外側です。6平面のどれかで外ならCullingできます。

## 23. CullingとSimulation

画面外だからAIやCollisionを無条件に止めません。Render VisibilityとSimulation Activationは別Policyです。

## 24. WorldからScreenへ

```cpp
struct ScreenProjection final
{
    bool visibleDepth = false;
    Vec2 screen{};
    float depth01 = 0.0F;
};

[[nodiscard]] ScreenProjection ProjectWorld(Vec3 world)
{
    const VECTOR result = ConvWorldPosToScreenPos(ToDx(world));
    return {result.z > 0.0F && result.z < 1.0F,
            {result.x, result.y}, result.z};
}
```

公式仕様ではZが0以下または1以上の場合、X/YはScreen座標と無関係な値です。先にZを検証します。

## 25. 頭上UI

Characterの頭Bone/OffsetをWorld座標化し、Projectします。Depth範囲内でも画面矩形外なら表示を省略または端へClampします。

## 26. Camera後方

後方Pointを画面端Indicatorへ使う場合、単純Clampすると左右が逆転することがあります。Camera空間のForward符号を確認し、後方専用処理を行います。

## 27. ScreenからWorldへ

Screen X/Yだけでは奥行きが決まりません。Near側とFar側の二点へ逆変換し、Rayを作ります。

```text
nearWorld = Unproject(mouseX, mouseY, depth=0)
farWorld  = Unproject(mouseX, mouseY, depth=1)
ray.direction = normalize(farWorld - nearWorld)
```

使用中APIの関数名・Z契約を公式資料で確認します。

## 28. Mouse Picking

Mouse RayをCollision WorldやModel Triangleへ投げます。最も近いHitを選び、UIがMouseを消費中ならWorld Pickingを止めます。

## 29. Camera Rig

```cpp
struct ThirdPersonRig final
{
    Vec3 focusPoint{};
    float yaw = 0.0F;
    float pitch = ToRadians(15.0F);
    float distance = 6.0F;
    Vec3 shoulderOffset{0.5F, 0.5F, 0.0F};
};
```

Rig Dataから最終Poseを計算し、Gameplay CharacterへCamera内部値を持たせません。

## 30. Orbit Camera

Yaw/PitchからFocus周りのOffsetを作ります。Pitchを上下限へClampし、PoleでUpが反転するのを防ぎます。

## 31. Input感度

Mouse DeltaはFrame間の移動量なので、さらにDelta Timeを掛けるべきかInput API契約を確認します。Gamepad Stickは速度入力なのでDelta Timeを掛けるのが一般的です。

## 32. Pitch Clamp

```cpp
rig.pitch = std::clamp(rig.pitch,
                       ToRadians(-30.0F),
                       ToRadians(70.0F));
```

上下限を越えてCameraが反転しないようにします。

## 33. Focus Point

足元ではなく胸・頭の間など、Characterの見せたい位置をFocusにします。Animation Boneを直接使うと上下動が激しい場合、安定したGameplay TransformとOffsetを使います。

## 34. Dead Zone

Targetが一定範囲内ならCameraを動かさず、越えた分だけ追従します。細かいCharacter揺れをCameraへ伝えません。

## 35. Look-ahead

移動方向、Aim方向、Lock Target方向へFocusを先行させます。急反転時は滑らかに戻し、入力なし時の減衰を設計します。

## 36. Camera Smoothing

```cpp
Vec3 SmoothPosition(Vec3 current, Vec3 desired,
                    float sharpness, float delta)
{
    const float t = 1.0F - std::exp(-sharpness * delta);
    return Lerp(current, desired, t);
}
```

Simulation PoseとRender Poseを分離し、固定更新の段差も補間します。

## 37. Spring Camera

位置・速度を持つ減衰Springは追従遅れと自然な収束を作れます。Underdampedでは揺れ、Critical dampingではOvershootなしに速く収束します。大Deltaへの安定性をTestします。

## 38. Camera Collision

Focusから理想Camera位置へSphere/Capsule Castし、壁Hitより少し手前へCameraを移します。RayだけだとCamera Near Planeの角が壁へ入ります。

## 39. Collision復帰

壁へ近づく時は素早く縮め、壁から離れる時はゆっくり理想距離へ戻すと、壁の出入りでCameraが激しく前後しにくくなります。

## 40. ObstructionとOcclusion

- Obstruction: Camera自体が壁へ入る。
- Occlusion: Cameraは安全だがTargetが物で隠れる。

後者にはCamera移動、障害物Fade、Outline、別Angleなどを使います。

## 41. Shoulder Camera

左右肩Offsetを切り替える際、壁側へ移動して衝突しないかCastします。Aim ReticleのRayとWeapon発射Rayの差も補正します。

## 42. Target Lock

PlayerとTargetの中間や重み付き点をFocusにし、両者が画面内へ収まる距離・FOVを計算します。単純にTargetだけを見るとPlayerが画面外へ出ます。

## 43. 複数対象Frame

対象群をCamera基底へ投影し、Screen上BoundsへMarginを加えます。距離またはFOVを調整します。急な対象追加・削除は補間します。

## 44. Lock中Yaw

Camera ForwardをPlayer→Target方向へ近づけつつ、入力で周回できる許容角を設けられます。完全固定かSoft Lockかを仕様化します。

## 45. Camera Mode

```cpp
enum class CameraMode
{
    Free,
    Follow,
    LockOn,
    Cinematic,
    Photo
};
```

巨大な条件分岐よりModeごとのControllerと共通Rigへ分けます。

## 46. Mode遷移

現在Poseから新Modeの理想PoseへBlendします。Mode切替時にYaw/Pitch内部値を現在姿勢から同期しないと瞬間移動します。

## 47. Camera Shake

最終Poseへ小さい位置・回転Offsetを加えます。Base Camera状態を直接書き換えず、Effect Layerとして合成します。

```cpp
struct CameraImpulse final
{
    float amplitude = 0.0F;
    float frequency = 0.0F;
    float duration = 0.0F;
    float elapsed = 0.0F;
};
```

## 48. Shake方向

World軸、Camera Local軸、Impact方向を使い分けます。激しいRollや高周波Shakeは酔い・視認性低下につながるためOptionで強度を調整可能にします。

## 49. Deterministic Noise

毎Frame独立乱数より連続Noiseの方が滑らかです。Camera演出用乱数列をGameplayから分離します。同一Seedと時間で再現可能にします。

## 50. FOV Kick

Dash時にFOVを広げ、終了後戻すと速度感が出ます。Camera距離と同時に変えると構図が大きく変わるためCurveと上限を設計します。

## 51. Hit Stop中Camera

Camera Shake、入力回転、追従をGame Timeで止めるかReal Timeで続けるかをEffectごとに選びます。Hit Stop中も小さなImpulseを動かすとImpactを強調できます。

## 52. Cinematic Camera

Keyframe Pose、Focus Target、FOV、Ease CurveをTimelineで再生します。Gameplay Cameraへ戻す際は現在Poseを引き継いでBlendします。

## 53. Camera Cut

意図的な瞬間切替です。補間すべき遷移と明示Cutを区別します。TeleportやScene開始で長い補間が逆に不自然な場合があります。

## 54. Split Screen

CameraごとにViewport、Aspect、Render Target、Layerを持ちます。`SetDrawScreen`でCamera設定がResetされるため、各Passで必ず再適用します。

## 55. Mini-map Camera

上空Orthographic Cameraを別Targetへ描きます。Main Camera設定を流用せず、Projection、Near/Far、表示Layerを独立させます。

## 56. Audio Listener

3D SoundのListener位置・前方・上方向をCameraに合わせるかPlayerに合わせるかを決めます。公式資料ではCamera Poseに対応する3D Sound Listener APIがあります。

## 57. CameraとAim

ReticleからWorldへRayを飛ばしてAim Pointを得て、Weapon位置からAim Pointへ弾を飛ばします。Camera Rayが壁をHitし、Weaponからその点への線が別壁へ当たる近距離Caseを処理します。

## 58. Camera LagとGameplay

照準・Target選択を遅延したRender Cameraから計算するか、即時Simulation Cameraから計算するかで操作感が変わります。見た目と判定の基準を明示します。

## 59. Safe Area

HUDやTarget Indicatorは画面端ぴったりでなくSafe Margin内へClampします。Ultra-wide、Window、字幕領域を考慮します。

## 60. Debug Gizmo

- Camera PositionとBasis。
- Focus/Target。
- 理想位置とCollision補正位置。
- Near/Far平面とFrustum。
- Dead Zone、Look-ahead。
- Screen Projection点。
- Camera CastとHit Normal。

## 61. Debug UI

Position、Yaw/Pitch、FOV、Near/Far、Mode、追従誤差、Collision距離、Shake数、Projection Zを表示します。

## 62. Frustum Corner

Inverse View-ProjectionでNDCの8CornerをWorldへ戻すとFrustum Gizmoを作れます。行列規約とNDC Z範囲をDX契約に合わせます。

## 63. 数値安全性

Pose、FOV、Near/Far、Aspectへ`isfinite`検査を行います。FOVを0やπへ近づけず、Aspectを0にしません。失敗時は直前の有効Cameraを維持します。

## 64. Test

- Camera前方Pointが画面中央付近へProjectされる。
- Near/Far外ではProjection Zが範囲外。
- Position==TargetでFallbackする。
- Pitch Clampを越えない。
- Delta違いでも追従が近い結果になる。
- Camera Collisionで理想距離以下になる。
- Mode切替開始Poseが現在Poseと一致する。

## 65. よくある不具合

- 3Dが突然消える: Target切替後Near/Farを再設定していない。
- Z-fighting: Nearが小さすぎ、Farが大きすぎ。
- 頭上UIが暴れる: Projection Z範囲外のX/Yを使った。
- Cameraが反転: ForwardとUpが平行、Pitch未制限。
- 壁へ入る: Point RayだけでCamera体積を考慮していない。
- LockでPlayerが消える: TargetだけをFocusにした。
- Mode切替で跳ぶ: 内部Yaw/Pitchを現在Poseへ同期していない。

## 66. 性能

複数CameraはWorld描画を複数回行います。CameraごとのVisible Object数、Culling時間、Draw数、Target Memoryを計測します。Camera Collision Queryの回数と形状も記録します。

## 67. 設計チェックリスト

- [ ] PoseとLensを分けた。
- [ ] Position==Targetと平行Upを処理した。
- [ ] FOV単位と縦横の契約を明記した。
- [ ] Nearを可能な範囲で大きくした。
- [ ] 描画先切替後にCameraを再設定する。
- [ ] Projection Zを検証してからX/Yを使う。
- [ ] Mouse RayをNear/Far二点から作る。
- [ ] Base PoseとShakeを分けた。
- [ ] Camera Collisionに体積Castを使った。
- [ ] Mode遷移で現在Poseを引き継ぐ。
- [ ] FrustumとRigをGizmo表示する。

## 68. 理解確認問題

1. View MatrixがCamera World Transformの逆である理由は何か。
2. Nearを極端に小さくすると何が起きるか。
3. FOV変更とCamera距離変更の見え方の違いは何か。
4. `SetDrawScreen`後に何を再設定すべきか。
5. Projection結果のZが範囲外ならX/Yを使えない理由は何か。
6. Screen X/YだけでWorld位置が一意に決まらない理由は何か。
7. Camera CollisionでSphere CastがRayより安全な理由は何か。
8. Target Lockで中間点を見る利点は何か。
9. ShakeをBase Poseへ直接加算し続けてはいけない理由は何か。
10. Split ScreenでAspectをCameraごとに持つ理由は何か。

## 69. 実践課題

1. Pose/LensをDX Cameraへ適用するBackendを作る。
2. Orbit CameraとPitch Clampを作る。
3. World→Screen頭上UIと範囲外判定を作る。
4. Mouse Picking Rayを作る。
5. Frustum Sphere Cullingを実装する。
6. Sphere CastによるCamera Collisionを作る。
7. Follow/LockOn間をBlendする。
8. Impulse型ShakeとFOV Kickを作る。
9. Mini-map用Orthographic Cameraを作る。
10. Rig、Frustum、Projection ZをDebug表示する。

## 70. 公式資料

- [DXライブラリ Camera関数](https://dxlib.xsrv.jp/function/dxfunc_3d_camera.html)
- [DXライブラリ 3D関数一覧](https://dxlib.xsrv.jp/function/dxfunc_3d.html)
- [DXライブラリ 3D算術演算](https://dxlib.xsrv.jp/function/dxfunc_3d_math.html)
- [DXライブラリ Sound関数](https://dxlib.xsrv.jp/function/dxfunc_sound.html)

FOVの定義、Projection Z、逆変換、Camera設定のReset条件、View/Projection Matrix規約は利用中バージョンの公式資料と既知値Testを正としてください。

## 71. 次章への接続

次章ではMV1 Model・Materialを扱い、Model Handleの寿命、Transform、Frame階層、Mesh、Material、半透明描画、LOD、CullingをCamera Pipelineへ接続します。
