# 39 サウンド、ミキシング、戦闘音響

## 1. 音の責任を分類する

- 2D UI音：画面上の操作Feedback。
- 3D World音：位置と距離を持つ攻撃、足音、環境音。
- Loop音：Aura、Charge、機械等。明示停止が必要。
- Music：戦闘Phaseに応じたLayer／Transition。
- Voice：優先度、字幕、割込み規則が必要。

## 2. 再生例

```cpp
UGameplayStatics::PlaySoundAtLocation(
    this,
    ImpactSound,
    HitLocation,
    VolumeMultiplier,
    PitchMultiplier);
```

制御不要のOne-shotに向きます。後から停止・Parameter更新する音は`UAudioComponent`を保持します。

```cpp
UAudioComponent* Loop = UGameplayStatics::SpawnSoundAttached(
    ChargeSound,
    GetMesh(),
    TEXT("weapon_r"));

if (Loop)
{
    Loop->FadeIn(0.05f);
}
```

## 3. Attenuation

距離減衰、Spatialization、Occlusion、Listener Focusを設定します。戦闘上重要な背後攻撃音は、物理的な正しさだけで聞こえなくならないよう設計します。

## 4. Concurrency

同じ足音やHit音が大量に鳴る場合、Concurrencyで最大Voice数、古い音の停止、距離／Priorityによる選択を設定します。

制限なしでは音割れとVoice奪取が起きます。制限が強すぎると重要なParry音が雑魚足音に負けます。Category別Priorityを決めます。

## 5. Sound ClassとSubmix

```text
Master
├─ Music
├─ SFX
│  ├─ PlayerCombat
│  └─ EnemyCombat
├─ Voice
└─ UI
```

Volume設定をSound Classへ適用し、SubmixでEQ、Compression、Reverb等を処理します。Hit Stop時にSFX Pitchだけ変える、必殺技で環境音をDuckする、といった演出をMix Snapshot的に管理します。

## 6. MetaSounds

MetaSoundsは音声DSP GraphをData駆動で構築し、入力ParameterからVariationを作れます。攻撃速度、Combo段数、SurfaceをParameterとして渡し、同一Sampleの単調な連続再生を避けられます。

ただしGameplay判定を音Graphへ置かず、結果を受け取る演出層として使います。

## 7. Surface別足音・Hit音

TraceのPhysical Material／Surface TypeからCueを選びます。

```cpp
const EPhysicalSurface Surface = UPhysicalMaterial::DetermineSurfaceType(
    Hit.PhysMaterial.Get());
const TObjectPtr<USoundBase> Sound = AudioTable->FindImpactSound(Surface, AttackType);
```

Asset未設定時のFallbackを用意します。

## 8. 戦闘の優先情報

音でPlayerへ伝えるべきもの：

- 攻撃の命中強度。
- Parry／Perfect Dodge成功。
- 画面外からの危険予告。
- Resource満タン／不足。
- Character交代可能／不能。

同じ周波数帯へ全音を重ねず、音域、長さ、Stereo位置、Duckで分離します。

## 9. 寿命と非同期

Loop Audio ComponentはOwnerの`EndPlay`、Montage中断、Ability CancelでFade Out／Stopします。非同期読込後に古いAction Sequenceへ音を付けないようIDを再検証します。

## 10. テスト

- Speaker、Headphone、低音量。
- 多数敵の同時攻撃。
- Pause／Hit Stop／Time Dilation。
- Cameraから遠い攻撃。
- Character交代中のLoop停止。
- 音量0でも字幕・視覚Indicatorが機能する。
