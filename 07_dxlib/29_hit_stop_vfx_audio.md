# DXライブラリ：Hit Stop・VFX・Audio演出

この章では、確定済みの命中結果から「攻撃が当たった感触」を作ります。ヒットストップ、スローモーション、画面揺れ、発光、火花、斬撃、効果音を、戦闘結果を壊さない独立した演出システムとして設計します。

## 1. 手応えを構成する要素

- Hit Stop：短時間だけ動きを止め、衝撃を認識させる。
- Time Scale：強い攻撃や決着をゆっくり見せる。
- VFX：位置、方向、属性、強度を視覚化する。
- Audio：接触材質と攻撃強度を耳で伝える。
- Camera Shake：画面全体へ衝撃を伝える。
- Flash：攻撃者や被攻撃者を一瞬発光させる。
- UI：Damage Numberや弱点表示で結果を説明する。

これらは同じイベントから始まりますが、寿命と更新方法は別々です。

## 2. 戦闘結果と演出を分離する

```text
Damage Commit
 -> HitPresentationEvent
    -> HitStopSystem
    -> EffectSystem
    -> AudioSystem
    -> CameraShakeSystem
    -> DamageNumberSystem
```

音声ファイルの読み込み失敗やVFX上限到達が、HPダメージや怯み結果を変えてはいけません。

## 3. 演出イベント

```cpp
#include <DxLib.h>
#include <cstdint>

enum class HitStrength
{
    Light,
    Medium,
    Heavy,
    Finisher
};

struct HitPresentationEvent final
{
    std::uint32_t sequence{};       // 同一フレーム内でも安定した順に処理する番号。
    std::uint32_t attackerId{};
    std::uint32_t defenderId{};
    VECTOR position{};
    VECTOR normal{};
    VECTOR attackDirection{};
    int damage{};
    HitStrength strength{HitStrength::Light};
    std::uint16_t surfaceId{};
    std::uint16_t effectPresetId{};
    std::uint16_t soundPresetId{};
    bool critical{};
    bool weakPoint{};
    bool killed{};
};
```

生ポインタを保存せず、イベント処理中も有効な値とIDを渡します。

## 4. 演出Preset

```cpp
struct HitPresentationPreset final
{
    std::uint16_t hitStopTicks{};
    float attackerStopScale{0.0f};
    float defenderStopScale{0.0f};
    float cameraAmplitude{};
    float cameraFrequency{20.0f};
    float cameraDuration{};
    float flashDuration{};
    float effectScale{1.0f};
    int soundVolume{255};
};
```

軽・中・重攻撃の値を処理へ直書きせず、調整可能なPresetにまとめます。

## 5. ゲーム時間と実時間

時間は最低でも二種類に分けます。

- Game Time：Time ScaleやHit Stopの影響を受ける。
- Real Time：メニュー、デバッグ表示、演出管理など停止中にも進めたい処理。

```cpp
struct FrameTime final
{
    float realDeltaSeconds{};
    float gameDeltaSeconds{};
    std::uint64_t fixedTick{};
};
```

すべてへ同じDelta Timeを渡すと、停止解除用Timerまで停止して永久停止になります。

## 6. Hit Stopとは何か

Hit Stopは数フレームの停止です。入力受付、UI、Particleなどを完全停止するとは限りません。何を止めるかをDomain単位で決めます。

```cpp
enum class TimeDomain
{
    World,
    Attacker,
    Defender,
    Camera,
    Particle,
    UserInterface
};
```

## 7. Hit Stop要求

```cpp
struct HitStopRequest final
{
    std::uint32_t sourceSequence{};
    std::uint32_t attackerId{};
    std::uint32_t defenderId{};
    std::uint16_t durationTicks{};
    float attackerScale{};
    float defenderScale{};
    int priority{};
};
```

秒でなく固定Tickにすると、30fpsと144fpsで停止のゲーム更新回数が変わりません。

## 8. 単純なHit Stop状態

```cpp
#include <algorithm>

struct HitStopState final
{
    std::uint16_t remainingTicks{};
    float timeScale{1.0f};
    int priority{};

    bool Active() const
    {
        return remainingTicks > 0;
    }

    void Tick()
    {
        if (remainingTicks > 0)
            --remainingTicks;
        if (remainingTicks == 0)
        {
            timeScale = 1.0f;
            priority = 0;
        }
    }
};
```

解除判定は停止の影響を受けないFixed Tick管理側で実行します。

## 9. 複数要求の合成

```cpp
void ApplyHitStop(HitStopState& state, const HitStopRequest& request)
{
    if (request.priority < state.priority && state.Active())
        return;

    state.priority = request.priority;
    state.remainingTicks = std::max(
        state.remainingTicks, request.durationTicks);
    state.timeScale = std::clamp(request.defenderScale, 0.0f, 1.0f);
}
```

常に加算すると多段攻撃で長時間止まり続けます。最大値、上書き、延長上限などの規則を決めます。

## 10. 攻撃者と対象を別に止める

攻撃者は2Tick、対象は4Tickのように分けると、攻撃者が先に動き出して追撃準備へ入れます。Entity単位のTime ScaleをAnimation、Movement、Combatへ同じように適用します。

## 11. 入力Bufferは止めるか

Hit Stop中も入力を受け付け、固定TickまたはReal TimeでBuffer寿命を管理する方式が一般的です。停止中の入力を捨てると操作感が悪化します。ただし受付Windowそのものを進めるかは技仕様です。

## 12. Animationの停止

```cpp
float ScaledAnimationDelta(const FrameTime& time,
                           const HitStopState& stop)
{
    return time.gameDeltaSeconds * stop.timeScale;
}
```

モデルアニメーションだけを止め、状態遷移Timerが進むと見た目と当たり判定がずれます。関連するゲーム更新を同じDomainへ所属させます。

## 13. Particleは止めるか

命中火花まで止めると一枚絵のような強い衝撃になり、動かし続けると派手さを維持できます。PresetごとにParticle Time Domainを選べるようにします。

## 14. Slow Motion要求

```cpp
struct TimeScaleRequest final
{
    float targetScale{1.0f};
    float blendInSeconds{};
    float holdSeconds{};
    float blendOutSeconds{};
    int priority{};
    std::uint32_t ownerId{};
};
```

Slow MotionはHit Stopより長いため、瞬時切替でなく補間を持たせると自然です。

## 15. Time Scale補間

```cpp
float SmoothStep01(float t)
{
    t = std::clamp(t, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

float BlendScale(float from, float to, float normalizedTime)
{
    const float t = SmoothStep01(normalizedTime);
    return from + (to - from) * t;
}
```

補間TimerはReal Timeで進めないと、Scale 0へ近づくほど解除も遅くなります。

## 16. Pauseとの違い

Pauseはユーザー操作によるゲーム停止、Hit Stopは戦闘演出、Slow Motionは時間倍率変更です。同じBoolへまとめず、理由別のTokenまたは優先度付き要求として合成します。

## 17. Effect HandleのRAII

DXライブラリの画像Handleは読み込みと破棄の寿命を管理します。

```cpp
class GraphHandle final
{
public:
    explicit GraphHandle(const TCHAR* path)
        : value_(LoadGraph(path)) {}

    ~GraphHandle()
    {
        if (value_ >= 0)
            DeleteGraph(value_);
    }

    GraphHandle(const GraphHandle&) = delete;
    GraphHandle& operator=(const GraphHandle&) = delete;

    int Get() const { return value_; }
    bool Valid() const { return value_ >= 0; }

private:
    int value_{-1};
};
```

実際のResource Cacheでは共有所有権やAsset IDを使い、同じ画像の重複Loadを防ぎます。

## 18. Effect Instance

```cpp
struct EffectInstance final
{
    std::uint32_t generation{};
    int graphHandle{-1};
    VECTOR position{};
    VECTOR velocity{};
    VECTOR normal{VGet(0, 1, 0)};
    float age{};
    float lifetime{0.2f};
    float scale{1.0f};
    float rotation{};
    float angularVelocity{};
    unsigned int color{0xffffffffu};
    bool active{};
};
```

Assetと再生中Instanceを分けます。同じ画像から多数のInstanceを生成できます。

## 19. Object Pool

```cpp
#include <vector>

class EffectPool final
{
public:
    explicit EffectPool(std::size_t capacity)
        : instances_(capacity) {}

    EffectInstance* Acquire()
    {
        for (EffectInstance& instance : instances_)
        {
            if (!instance.active)
            {
                instance = EffectInstance{};
                instance.active = true;
                ++instance.generation;
                return &instance;
            }
        }
        return nullptr;
    }

private:
    std::vector<EffectInstance> instances_{};
};
```

命中ごとの動的確保を避けます。満杯時に古い弱演出を再利用するか、新規演出を捨てるかを決めます。

## 20. Generationの注意

上の単純例はInstanceを初期化してから世代を増やすため、再利用時の世代保持が不十分です。本番ではPool Slot側に世代番号を持ち、古い参照が再利用後の別Instanceを操作できないようにします。

## 21. 安全なEffect ID

```cpp
struct EffectId final
{
    std::uint16_t index{};
    std::uint16_t generation{};
};
```

Indexだけでは再利用後の別Effectを誤って停止します。IndexとGenerationを照合します。

## 22. VFX生成要求

```cpp
struct SpawnEffectRequest final
{
    std::uint16_t presetId{};
    VECTOR position{};
    VECTOR normal{};
    VECTOR direction{};
    float scale{1.0f};
    std::uint32_t seed{};
    int priority{};
};
```

Seedを含めると、火花の角度や速度をリプレイで再現できます。

## 23. 法線から基底を作る

```cpp
struct Basis final
{
    VECTOR right{};
    VECTOR up{};
    VECTOR forward{};
};

Basis BuildBasisFromNormal(VECTOR normal)
{
    VECTOR forward = VDot(normal, normal) > 0.000001f
        ? VNorm(normal) : VGet(0, 1, 0);
    const VECTOR helper = std::abs(forward.y) < 0.99f
        ? VGet(0, 1, 0) : VGet(1, 0, 0);
    const VECTOR right = VNorm(VCross(helper, forward));
    const VECTOR up = VCross(forward, right);
    return {right, up, forward};
}
```

法線と平行な補助軸を選ぶと外積がゼロになるため、向きに応じて補助軸を切り替えます。

## 24. 火花速度

```cpp
VECTOR BuildSparkVelocity(const Basis& basis,
                          float rightAmount,
                          float upAmount,
                          float normalAmount)
{
    return VAdd(VAdd(VScale(basis.right, rightAmount),
                     VScale(basis.up, upAmount)),
                VScale(basis.forward, normalAmount));
}
```

世界軸へ固定せず、接触面の基底で速度を作ると壁や床でも自然に飛びます。

## 25. Effect更新

```cpp
void UpdateEffect(EffectInstance& effect, float deltaSeconds,
                  VECTOR gravity)
{
    if (!effect.active)
        return;

    effect.age += std::max(deltaSeconds, 0.0f);
    if (effect.age >= effect.lifetime)
    {
        effect.active = false;
        return;
    }

    effect.velocity = VAdd(effect.velocity,
        VScale(gravity, deltaSeconds));
    effect.position = VAdd(effect.position,
        VScale(effect.velocity, deltaSeconds));
    effect.rotation += effect.angularVelocity * deltaSeconds;
}
```

更新と描画を分け、描画されないFrameでも寿命が正しく進むようにします。

## 26. 寿命の正規化

```cpp
float NormalizedAge(const EffectInstance& effect)
{
    if (effect.lifetime <= 0.0f)
        return 1.0f;
    return std::clamp(effect.age / effect.lifetime, 0.0f, 1.0f);
}
```

この0～1をAlpha、Scale、色のCurve入力に使います。

## 27. Alpha Curve

```cpp
float FadeInOut(float t, float fadeInEnd, float fadeOutBegin)
{
    t = std::clamp(t, 0.0f, 1.0f);
    if (t < fadeInEnd)
        return fadeInEnd > 0.0f ? t / fadeInEnd : 1.0f;
    if (t > fadeOutBegin)
        return fadeOutBegin < 1.0f
            ? (1.0f - t) / (1.0f - fadeOutBegin) : 0.0f;
    return 1.0f;
}
```

分母が0にならないよう境界値を処理します。

## 28. Billboard描画

```cpp
void DrawEffectBillboard(const EffectInstance& effect)
{
    if (!effect.active || effect.graphHandle < 0)
        return;

    const float alpha = FadeInOut(NormalizedAge(effect), 0.1f, 0.7f);
    SetDrawBlendMode(DX_BLENDMODE_ALPHA,
                     static_cast<int>(alpha * 255.0f));
    DrawBillboard3D(effect.position, 0.5f, 0.5f,
                    effect.scale, effect.rotation,
                    effect.graphHandle, TRUE);
    SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
}
```

描画状態を変更したら必ず戻します。戻し忘れは後続描画を半透明にします。

## 29. Additive Blend

発光や火花には加算合成が向きますが、明るい背景では白飛びします。Alpha BlendとAdditiveをPresetで選択し、常に加算へ固定しません。

## 30. 深度と半透明

半透明Effectは描画順で見え方が変わります。大規模ならCameraからの距離で奥から手前へ並べます。ただし大量Particleの完全Sortは高コストなので、Emitter単位のSortや近似も検討します。

## 31. Screen Space Effect

画面端の色収差、Vignette、FlashはRender TargetへSceneを描いた後のPost Processで適用します。World VFXとScreen Effectを同じInstanceへ詰め込まず、描画段階を分けます。

## 32. 被弾Flash

```cpp
struct FlashState final
{
    float remaining{};
    float duration{};
    unsigned int color{0xffffffffu};

    float Weight() const
    {
        if (duration <= 0.0f)
            return 0.0f;
        return std::clamp(remaining / duration, 0.0f, 1.0f);
    }
};
```

Model全体のMaterialを恒久的に書き換えず、描画時だけFlash Parameterを適用して元へ戻します。

## 33. Audio AssetとVoice

- Audio Asset：読み込まれた音声データ。
- Voice：現在再生中の一回分。

同じ斬撃音を複数回再生するには、複製Handleまたは同時再生に対応した管理が必要です。

## 34. Sound HandleのRAII

```cpp
class SoundHandle final
{
public:
    explicit SoundHandle(const TCHAR* path)
        : value_(LoadSoundMem(path)) {}

    ~SoundHandle()
    {
        if (value_ >= 0)
            DeleteSoundMem(value_);
    }

    SoundHandle(const SoundHandle&) = delete;
    SoundHandle& operator=(const SoundHandle&) = delete;
    int Get() const { return value_; }

private:
    int value_{-1};
};
```

再生中にHandleを破棄しないよう、Audio ManagerがAsset寿命を所有します。

## 35. 音声再生要求

```cpp
struct PlaySoundRequest final
{
    std::uint16_t presetId{};
    VECTOR position{};
    float volume{1.0f};
    float pitch{1.0f};
    float minimumDistance{1.0f};
    float maximumDistance{30.0f};
    std::uint32_t seed{};
    int priority{};
};
```

DXライブラリのAPIへ渡す最終単位へ変換する前は、Volumeを0～1として扱うと合成しやすくなります。

## 36. 距離減衰

```cpp
float DistanceAttenuation(float distance,
                          float minimumDistance,
                          float maximumDistance)
{
    const float minD = std::max(minimumDistance, 0.0f);
    const float maxD = std::max(maximumDistance, minD + 0.001f);
    const float t = std::clamp((distance - minD) / (maxD - minD),
                               0.0f, 1.0f);
    return 1.0f - t;
}
```

実際には線形、二乗、対数的減衰を聞き比べて選びます。

## 37. 3D音の左右Pan

```cpp
float StereoPan(VECTOR listenerRight, VECTOR toSound)
{
    if (VDot(toSound, toSound) <= 0.000001f)
        return 0.0f;
    return std::clamp(VDot(VNorm(listenerRight), VNorm(toSound)),
                      -1.0f, 1.0f);
}
```

ListenerのRight Vectorとの内積で左右位置を求めます。

## 38. 音量をDXライブラリへ変換

```cpp
int ToDxVolume(float normalizedVolume)
{
    return static_cast<int>(
        std::lround(std::clamp(normalizedVolume, 0.0f, 1.0f) * 255.0f));
}
```

入力を範囲制限し、負数や255超過を渡しません。

## 39. Pitch Variation

同じ音を連打すると機械的に聞こえます。Seed付き乱数で±数%のPitch Variationを加えます。大きく変えすぎると別の材質に聞こえるため、Presetで範囲を制限します。

## 40. 同時発音数

```cpp
struct VoiceLimit final
{
    std::uint16_t groupId{};
    std::uint16_t maximumVoices{4};
    float retriggerInterval{0.02f};
};
```

多段攻撃ですべての音を鳴らすと飽和します。Groupごとの上限、再発音間隔、Priorityを使います。

## 41. Voice Stealing

上限時には、最も古い音、最も小さい音、最も遠い音、最もPriorityの低い音から停止します。重要なFinisher音が雑魚の足音に奪われない規則が必要です。

## 42. Audio Bus

```cpp
enum class AudioBus
{
    Master,
    Music,
    SoundEffect,
    Voice,
    UserInterface
};

struct BusVolume final
{
    float master{1.0f};
    float user{1.0f};
    float ducking{1.0f};

    float Effective() const
    {
        return std::clamp(master * user * ducking, 0.0f, 1.0f);
    }
};
```

設定画面の値と一時的なDuckingを別に持ちます。

## 43. Ducking

必殺技Voice中にBGMや通常SEを少し下げる処理です。瞬時に音量を変えずAttack・Hold・Releaseを持つEnvelopeで補間します。

## 44. Surface別の音

```cpp
enum class SurfaceType
{
    Flesh,
    Metal,
    Stone,
    Wood,
    Shield,
    Energy
};
```

攻撃種別とSurfaceの組み合わせからSound Presetを選びます。組み合わせが未登録なら安全なDefaultへ戻します。

## 45. Camera Shake要求

```cpp
struct CameraShakeRequest final
{
    std::uint32_t sequence{};
    float amplitude{};
    float frequency{};
    float duration{};
    VECTOR epicenter{};
    float maximumDistance{};
    std::uint32_t seed{};
    int priority{};
};
```

Shakeはカメラ本体の追従計算後、最終姿勢へ小さなOffsetとして加えます。

## 46. 減衰曲線

```cpp
float ShakeEnvelope(float elapsed, float duration)
{
    if (duration <= 0.0f)
        return 0.0f;
    const float t = std::clamp(elapsed / duration, 0.0f, 1.0f);
    const float remaining = 1.0f - t;
    return remaining * remaining;
}
```

二乗減衰は最初に強く、終わりへ滑らかに小さくなります。

## 47. Shakeの波形

毎Frame完全な乱数を使うと細かく震えすぎます。Seed付きNoise、複数Sin波、補間Noiseなど連続した波形を使います。

```cpp
float SineShake(float time, float frequency, float phase)
{
    constexpr float twoPi = 6.28318530718f;
    return std::sin(time * frequency * twoPi + phase);
}
```

## 48. 複数Shakeの合成

単純加算後に最大振幅へClampします。Finisherなど高Priority要求が来たら弱いShakeを減衰させる方式もあります。無制限加算で画面が飛ばないようにします。

## 49. 位置Shakeと回転Shake

位置Offsetは酔いやすく、回転Offsetは衝撃を強く感じます。軸ごとの倍率をPreset化し、上下・Rollを控えめにします。

## 50. Accessibility

ユーザー設定としてCamera Shake強度、Flash強度、画面点滅、色収差、Damage Numberを調整可能にします。0%設定でもゲームルールは変えません。

## 51. Damage Number要求

```cpp
struct DamageNumberRequest final
{
    VECTOR worldPosition{};
    int amount{};
    bool critical{};
    bool weakPoint{};
    std::uint32_t targetId{};
};
```

World座標を毎Frame Screen座標へ投影し、Camera移動に追従させるか、生成時Screen座標へ固定するかを選びます。

## 52. 数字の集約

同じ対象へ短時間に多数の小ダメージが入る場合、合計表示、一定数だけ表示、重要Hitだけ表示の方式があります。内部ダメージは一切省略せず、UI表示だけを整理します。

## 53. イベントの安定した処理順

同一Frameのイベントを`sequence`、攻撃者ID、対象IDで安定Sortします。VFX生成上限やVoice Stealingの結果が実行ごとに変わることを防ぎます。

## 54. Presentation Budget

```cpp
struct PresentationBudget final
{
    int maximumEffectsPerFrame{64};
    int maximumSoundsPerFrame{16};
    int maximumDamageNumbers{32};
};
```

上限到達時はPriority、距離、画面内判定で選別します。戦闘結果は捨てず、演出要求だけを省略します。

## 55. Culling

- Cameraから遠すぎるWorld VFXを生成しない。
- 画面外ではLightやDamage Numberを省略する。
- 音は視界外でも距離内なら残す場合がある。
- BossやPlayerの重要演出は高Priorityにする。

## 56. Debug表示

```cpp
struct PresentationDebugStats final
{
    int hitStopRequests{};
    int activeEffects{};
    int droppedEffects{};
    int activeVoices{};
    int stolenVoices{};
    int activeShakes{};
};
```

要求数だけでなく、上限で捨てた数も表示します。

## 57. VFX Debug

命中位置へSphere、法線へ線、攻撃方向へ別色の線を描きます。Effectが変な方向を向く問題が、Collision情報か描画変換かを切り分けられます。

## 58. Audio Debug

再生したPreset ID、Surface、距離減衰、最終Volume、Pitch、Voice Stealing理由をリングバッファへ記録します。

## 59. Hit Stop Debug

EntityごとのTime Scale、残りTick、所有要求、Priorityを表示します。永久停止時は、解除Timerが停止Domainで更新されていないか確認します。

## 60. よくある失敗：全世界を停止する

UI、停止解除Timer、入力取得まで止めると復帰不能になります。Domainごとに停止対象を明示します。

## 61. よくある失敗：音を毎Frame再生する

状態がActiveである間ではなく、状態へ入った瞬間または確定イベント一件につき一度だけ再生します。

## 62. よくある失敗：描画Stateを戻さない

Blend、Depth Write、Shader、Render Targetを変更後に戻さないと、後続のUIやModelが壊れます。RendererのPass境界で既知状態を設定します。

## 63. よくある失敗：Effectが戦闘Objectを所有する

Effectは発生元が死亡しても寿命まで再生できる値型データを持ちます。必要な追従対象はEntity IDとGenerationで安全に解決します。

## 64. よくある失敗：Time Scaleを掛け重ねる

`delta *= hitStop; delta *= slowMotion; delta *= pause`を無計画に行うと極端な値になります。理由別要求を一箇所で合成し、最終Scaleを一度だけ配布します。

## 65. テスト：Hit Stop

- 0Tick要求では停止しない。
- 1Tick要求が正確に1回の固定更新を止める。
- 弱い要求が強い要求を上書きしない。
- 多段攻撃で上限を超えて延長されない。
- Pause中でも解除状態が破損しない。
- 入力Bufferが仕様どおり保持される。

## 66. テスト：VFX

- Pool満杯でもCrashしない。
- lifetime 0でも0除算しない。
- ゼロ法線でも有効な基底ができる。
- Effect終了後にSlotが再利用される。
- 古いGenerationのIDで新Instanceを停止できない。
- Blend Stateが描画後に復元される。

## 67. テスト：Audio

- 無効Handleを再生しない。
- 距離が最小以下なら最大音量になる。
- 最大距離以上なら無音になる。
- Voice上限時に低Priorityから奪う。
- Master 0で音量0になる。
- 同一Hitで二重再生しない。

## 68. テスト：Camera

- Duration 0でNaNにならない。
- 距離外のShakeを省略する。
- 複数Shake合成が最大値を超えない。
- Shake終了後にOffsetが完全に0へ戻る。
- ユーザー設定0%で揺れない。

## 69. 更新順序

```text
1. Combat events are committed
2. Presentation requests are generated
3. Budgets and priorities are resolved
4. Hit stop / time scale state is updated by real time
5. Gameplay and animation use scaled time
6. Effects and audio are updated
7. Base camera pose is calculated
8. Camera shake is added
9. World, transparent effects, post process, UI are drawn
```

## 70. 実装順序

1. 確定イベントからPresetを選ぶ。
2. Entity単位のHit Stopを作る。
3. VFX PoolとBillboard一種類を作る。
4. Audio Managerへ再生要求を渡す。
5. Surface別Soundと同時発音制限を加える。
6. Camera Shakeを最終Camera姿勢へ合成する。
7. Damage NumberとFlashを加える。
8. Budget、Culling、Debug表示を加える。
9. Accessibility設定を接続する。

## 71. 完成確認表

- [ ] Hit Stop中も解除Timerと必要な入力が動く。
- [ ] 多段攻撃で永久停止しない。
- [ ] VFX上限到達でも戦闘結果が変わらない。
- [ ] 接触法線に沿って火花が飛ぶ。
- [ ] SoundがSurfaceと強度で変わる。
- [ ] 同時発音数が上限内に収まる。
- [ ] Camera Shakeが終了後に0へ戻る。
- [ ] 描画Stateが各Pass後に復元される。
- [ ] Seed固定時に演出Variationを再現できる。
- [ ] ShakeとFlashをユーザー設定で軽減できる。

## 72. この章の要点

- 戦闘結果を先に確定し、演出はイベントから派生させます。
- Game TimeとReal Timeを分け、停止解除処理を止めません。
- Hit Stopは対象とDomainを明示し、複数要求の合成規則を持ちます。
- VFXはAssetとInstanceを分け、PoolとGenerationで安全に再利用します。
- Audioは距離、Surface、Priority、Voice上限、Bus音量を管理します。
- Camera Shakeは追従後の最終Offsetとして加え、振幅を制限します。
- Budget超過時に省略するのは演出だけで、戦闘結果は保持します。
- Debug統計と固定Seedにより「感触」の不具合も再現可能にします。

次章では、敵が移動・索敵・攻撃位置選択を行うためのEnemy AIとNavigationをDXライブラリ上で統合します。
