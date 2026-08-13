# 23 State Machine、Blend Space、Layer

## 1. LocomotionはState Machineに向く

Animation State MachineはIdle／Move／Jump／Fall／Landなど、継続状態と遷移を視覚化します。

```text
Grounded Locomotion ── bIsFalling ──> In Air
       ↑                               │
       └── Landed / Grounded ──────────┘
```

攻撃Combo全体をState Machineへ詰め込むより、LocomotionをState Machine、単発・分岐ActionをMontageとして重ねる方が管理しやすいことが多いです。

## 2. Transition Rule

悪い例：Animation State内からCharacterの体力を変更する。

良い例：CharacterMovementの`bIsFalling`やGameplayから翻訳された値を読み、Pose遷移だけを決める。

Transitionは毎Frame評価され得ます。重いWorld QueryやCastを置かず、Snapshot変数から純粋に判定します。

## 3. Blend時間

遷移Blendが長すぎると入力応答が遅く見え、短すぎるとPoseが跳びます。

- Idle→Run：Accelerationに合わせた短いBlend。
- Fall→Land：着地速度別AnimationとBlend。
- Stun→Locomotion：Gameplay硬直終了とPose復帰を同期。
- Direction反転：Inertialization等も検討。

Blend Profileで骨ごとにBlend速度を変えると、下半身は素早く、上半身は滑らかに追従させられます。

## 4. Blend Space

Blend Spaceは1つまたは2つの値から複数Animation SampleをBlendします。

```text
Axis X = Local Forward Speed (-Max ～ +Max)
Axis Y = Local Right Speed   (-Max ～ +Max)
Samples = 前後左右移動・斜め移動・Idle
```

```cpp
const FVector LocalVelocity = ActorTransform.InverseTransformVectorNoScale(Velocity);
BlendForward = LocalVelocity.X;
BlendRight = LocalVelocity.Y;
```

Sampleの足周期、Root位置、速度を揃えないとBlend中に足滑りが起きます。補間設定で隠す前にAnimation Assetの整合性を確認します。

## 5. Direction角度方式

SpeedとDirection（-180～180度）を軸にする方式もあります。Lock On StrafeではLocal X/Y速度の方が前後左右を直接表現しやすい場合があります。

0付近で方向が急変する低速時は、速度閾値、方向保持、入力方向との併用でAnimationの振動を抑えます。

## 6. Additive Animation

Additiveは基準Poseとの差分を既存Poseへ重ねます。

- Aim Offset。
- 呼吸や揺れ。
- 軽い被弾反応。
- 上半身の傾き。

Local Space AdditiveとMesh Space Additiveを用途に合わせます。Aim Offsetは通常Mesh Space Additive Sampleを使う構成です。

## 7. Layered Blend Per Bone

```text
Base Pose: 下半身を含むLocomotion
Blend Pose: 上半身Attack Montage
Branch Filter: spine_01から上
```

これにより走りながら上半身攻撃を再生できます。ただしRoot Motionを含む全身攻撃を上半身だけへ切ると破綻します。攻撃をFull BodyとUpper Bodyへ分類します。

## 8. Sync GroupとMarker

複数Locomotion AnimationをBlendする際、左足・右足の接地PhaseをSync Markerで揃えます。速度値だけ滑らかでも足Phaseがずれると、Blend中に足が交差・滑走します。

## 9. Inertialization

Inertializationは遷移元Poseを一定時間再評価し続けず、動きの慣性から遷移を滑らかにします。多数の分岐を軽量化できる場合がありますが、急停止すべき攻撃やHit Reactionで残像のような動きが出ないか確認します。

## 10. State爆発を避ける

悪い構成：`IdleSword`、`IdleGun`、`RunSword`、`RunGun`、各被弾、全攻撃を単一State Machineに並べる。

分離案：

```text
Locomotion State Machine
  ↓
Weapon Linked Layer
  ↓
Montage Slot（Action）
  ↓
Additive Hit / Aim
  ↓
IK
```

直交する要素をLayerに分け、Stateは本当に排他的な状態だけに使います。

## 11. 高速アクションの確認項目

- Lock On開始／解除時に足方向が飛ばない。
- 180度反転時のTurn Animationと入力応答。
- 攻撃中の下半身移動を許可する範囲。
- 空中攻撃終了後にFall Poseへ戻る。
- Montage中断後にSlot Weightが残らない。
- CharacterごとのSkeleton差をRetarget後に確認。

## 参考

- [State Machines](https://dev.epicgames.com/documentation/en-us/unreal-engine/state-machines-in-unreal-engine)
- [Blend Spaces](https://dev.epicgames.com/documentation/en-us/unreal-engine/blend-spaces-in-unreal-engine)
