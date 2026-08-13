# 38 Niagara VFXと戦闘演出

## 1. Niagaraの構造

```text
Niagara System
├─ Emitter A
│  └─ Module Stack
├─ Emitter B
│  └─ Module Stack
└─ User Parameters
```

- System：複数Emitterをまとめた完成Effect。
- Emitter：Particleの生成・更新単位。
- Module：Spawn、Velocity、Color等を順番に処理するBlock。
- Parameter：System／Emitter／Particle間やGameから渡す値。

Module Stackは上から下へDataを更新します。順序変更で結果が変わるため、各Moduleが読む値と書く値を確認します。

## 2. 戦闘Systemから演出を分離する

```text
Hit確定
  ↓ Presentation Event / Gameplay Cue
Effect Resolver
  ↓ Attack Tag・Surface・強度からAsset選択
Niagara System Spawn
```

Damage計算が特定Niagara Assetを直接知る構造にせず、`GameplayCue.Combat.Hit.Heavy`のような意味から演出Dataを選びます。

## 3. Spawn例

```cpp
UNiagaraComponent* Effect = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
    GetWorld(),
    HitEffect,
    Hit.ImpactPoint,
    Hit.ImpactNormal.Rotation(),
    FVector::OneVector,
    true,  // 自動破棄。
    true); // 自動Activation。

if (Effect)
{
    Effect->SetVariableFloat(TEXT("User.Intensity"), Intensity);
    Effect->SetVariableLinearColor(TEXT("User.HitColor"), HitColor);
}
```

Parameter名の文字列不一致をData Validationや定数で防ぎます。Actor／Componentへ追従するEffectはAttached Spawnを使い、SocketとDetach方針を指定します。

## 4. CPUとGPU Simulation

- CPU：Gameplay Dataとの連携や一部Collisionに柔軟だが大量ParticleでCPU負荷。
- GPU：大量Particleに向くがData読戻しや機能制約、遅延を理解する。

Hit判定をGPU Particle Collision結果だけへ依存させません。Gameplay CollisionはC++／Physics Query、Niagaraは見た目を担当します。

## 5. Effectの寿命

Loop EffectはOwner死亡、交代、Level遷移時に必ず停止します。Auto Destroyだけに頼れるのは終了するSystemです。

```cpp
if (ActiveAuraComponent)
{
    ActiveAuraComponent->Deactivate();
    ActiveAuraComponent = nullptr;
}
```

非同期ロード完了後にOwnerが消えている場合も再検証します。

## 6. PoolingとScalability

- 同種Effectを短時間に大量生成するならComponent Poolを検討。
- Effect Type、Significance、Distance Cullingで重要度を制御。
- 画面外・遠距離・低設定でSpawn RateやEmitter数を落とす。
- Overdraw、透明Particle、Light、Ribbon、Collisionを計測。
- Shader Compile数とMaterial種類も抑える。

## 7. ZZZ系演出の読みやすさ

- Hit位置を短いFlashと形状で伝える。
- 属性Colorだけでなく形・音でも区別する。
- Enemy攻撃予告は背景Effectより優先表示。
- 連続Hitで画面中央を完全に覆わない。
- Hit Stop中にParticleを止めるか動かすかをEffect種別で決める。

## 8. Debug

Niagara Debugger、Effect Type、Bounds表示、GPU／CPU時間、Particle数、Overdrawを確認します。Boundsが小さすぎると画面内でも消え、大きすぎると不要描画が残ります。

## 参考

- [Niagara Overview](https://dev.epicgames.com/documentation/en-us/unreal-engine/overview-of-niagara-effects-for-unreal-engine)
- [Creating Visual Effects](https://dev.epicgames.com/documentation/en-us/unreal-engine/creating-visual-effects-in-niagara-for-unreal-engine)
