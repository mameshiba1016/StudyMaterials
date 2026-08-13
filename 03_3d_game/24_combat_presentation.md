# 戦闘演出・VFX・Audio・Cameraの統合

戦闘の手触りはDamage値ではなく、入力結果を視覚・音・時間で明確に返すことで作られます。Gameplayは意味Eventを発行し、Presentation Systemが具体表現を選びます。

## Combat Presentation Event

```cpp
struct CombatPresentationEvent
{
    CombatEventType type{};
    Vector3 position{};
    Vector3 direction{};
    float intensity{};
    EntityHandle attacker{};
    EntityHandle target{};
};
```

`HeavyHit`、`ParrySuccess`等の意味を持たせ、Texture名やSound PathをCombat Codeへ埋め込みません。

## 反応先

- VFX：Spark、Trail、Distortion。
- Audio：SE、Pitch、Bus Ducking。
- Camera：Shake、FOV、Framing。
- Time：Hit Stop、Slow Motion。
- Character：Flash、Hit Animation、Afterimage。
- UI：Damage、Gauge、Prompt。
- Haptics：振動Pattern。

## 強度階層

Light/Heavy/Critical/Parry/Finisherで強度を分けます。すべて最大演出では重要Hitが埋もれます。Intensityから各SystemのCurveへ変換します。

## Hit Stop

Gameplay、Attacker、Target、VFX、Camera、UIのTime Domainを分けます。要求が重なった時の最大・上書き・上限を定めます。Online/Audio Clockを止めません。

## Trail

Weapon Socketの過去位置をSamplingしRibbon Meshを生成します。固定tickと描画Frame間を補間し、Teleport・Animation切替で履歴をResetします。長さ、頂点上限、Fadeを管理します。

## Impact位置

Collision Contact Point、Hit Shape中心、Target Bone位置のどれを使うかをAttack Typeで選びます。画面上見えない内部位置ならCamera側へ少し補正する演出もありますが、判定の真実は変更しません。

## Screen Effect

Flash、Vignette、Chromatic、Radial Blur等はPost Process Parameterへ短いEnvelopeを加えます。複数要求の合成と上限、解像度、アクセシビリティ設定を持ちます。

## Audio variation

同じSEの連続感を減らすため複数Sample、Pitch/Volume範囲を使います。ただしParry等の識別音は変化させすぎません。同時Voice上限とPriorityを設定します。

## Haptics

左右Motor、Trigger等へAttack方向・強度・長さをMappingします。Device切断、複数Player、振動無効設定を処理し、停止Commandを必ず送ります。

## Presentation遅延

意味EventをFrame末尾まで遅らせる場合でも、入力から音・Hit Stopまでの遅延を小さくします。Network確定待ちとLocal予測演出のRollback方針は別途必要です。

## Pool

短命VFX/Decal/Audio VoiceをPoolします。世代Handle、最大数、古いEffectのSteal、Scene遷移Resetを管理します。

## Debug

Event Timeline、発生元、Intensity、選択Preset、Active Effect数、Voice数、Hit Stop Layer、Camera Shakeを記録します。演出を個別無効化して原因を分離します。
