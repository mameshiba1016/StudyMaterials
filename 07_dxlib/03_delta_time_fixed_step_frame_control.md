# Delta Time・Fixed Step・Frame制御

> 対象: C++20とWindows版DXライブラリ。時間計測は`std::chrono::steady_clock`を基本とし、DXライブラリ固有counterとの対応も扱います。

## 1. Frame依存movementの問題

```cpp
positionX += 3.0f; // 一frameに3 pixels移動。
```

このcodeでは:

```text
60fps  → 180 pixels/second
120fps → 360 pixels/second
30fps  →  90 pixels/second
```

となり、monitor refreshや処理負荷でgame speedが変わります。

## 2. 速度の単位

速度を「一frame当たり」でなく「一秒当たり」で定義します。

```cpp
constexpr float speedPixelsPerSecond = 180.0f;

positionX += speedPixelsPerSecond * deltaSeconds;
```

```text
pixels/second × seconds/frame = pixels/frame
```

単位をfield名へ含めると誤用を減らせます。

## 3. Delta Time

Delta Timeは前回frameから今回frameまでの経過時間です。

```text
previous frame timestamp = 10.000 s
current frame timestamp  = 10.016 s
delta                    =  0.016 s
```

毎frame一定とは限りません。OS scheduling、VSync、load、debugger、background化で変化します。

## 4. Clockの条件

game経過時間計測には:

- 単調増加する。
- 十分なresolution。
- wall clock変更の影響を受けない。
- 値のdifferenceをdurationへ変換できる。

clockが適します。calendar時刻はgame deltaに使いません。

## 5. `steady_clock`

```cpp
#include <chrono>

using Clock = std::chrono::steady_clock;

const Clock::time_point now = Clock::now();
```

`steady_clock`は単調clockとして定義され、system時刻の手動変更等で逆行しない用途に向きます。

## 6. `high_resolution_clock`の注意

名前が高精度でも、implementationによって別clockのaliasであり、steadyとは限りません。game deltaには`steady_clock`を明示します。

「resolutionが細かい」ことと「正確」「単調」「実際の測定noiseが小さい」ことは別です。

## 7. 最小GameClock

```cpp
#include <chrono>

class GameClock final
{
public:
    using Clock = std::chrono::steady_clock;

    GameClock()
        : previous_(Clock::now())
    {
    }

    [[nodiscard]] double tickSeconds()
    {
        const auto current = Clock::now();
        const std::chrono::duration<double> elapsed = current - previous_;
        previous_ = current;
        return elapsed.count(); // double秒へ変換して返す。
    }

private:
    Clock::time_point previous_;
};
```

最初のtickにはconstructor後からの初期化時間も含まれるため、load終了後にresetする設計も必要です。

## 8. Clock reset

```cpp
void reset() noexcept
{
    previous_ = Clock::now();
}
```

次の後にresetします。

- 長い初期load。
- debugger breakpoint。
- pause解除。
- window focus復帰。
- blocking dialog。

数秒分を一frameでsimulationしないためです。

## 9. DXライブラリの高精度counter

公式の例では`GetNowHiPerformanceCount`がmicrosecond単位のcounterとしてDelta Time/FPS計測に使われています。

```cpp
LONGLONG previousMicroseconds = GetNowHiPerformanceCount();

// loop内
const LONGLONG currentMicroseconds = GetNowHiPerformanceCount();
const double deltaSeconds =
    static_cast<double>(currentMicroseconds - previousMicroseconds) / 1'000'000.0;
previousMicroseconds = currentMicroseconds;
```

DXライブラリ依存を減らすなら`steady_clock`、既存DX codeとの統合ならこのcounterも選択肢です。

## 10. `GetNowCount`との差

公式説明例では`GetNowCount`はmillisecond、`GetNowHiPerformanceCount`はmicrosecond単位です。

```text
1 millisecond = 1/1,000 second
1 microsecond = 1/1,000,000 second
```

単位を間違えて1000倍のspeedにしないよう、変数名へ`Microseconds`を含めます。

## 11. 秒へ統一する

gameplay APIは秒に統一します。

```cpp
void Player::update(double deltaSeconds);
void Animation::update(double deltaSeconds);
void Cooldown::update(double deltaSeconds);
```

millisecond、microsecond、frame数を混在させず、変換をclock境界で一度だけ行います。

## 12. `float`か`double`か

- 時間累積・clock変換: `double`が安全。
- GPU/position math: library APIに合わせ`float`が多い。

```cpp
double totalSeconds_ = 0.0;
float animationDelta = static_cast<float>(deltaSeconds);
```

長時間累積をfloatへ保存すると小さいdeltaを表現しにくくなります。

## 13. Variable timestep

描画frameごとに実deltaで一回updateします。

```cpp
while (ProcessMessage() == 0)
{
    const double deltaSeconds = clock.tickSeconds();

    input.update();
    world.update(deltaSeconds);

    ClearDrawScreen();
    world.render();
    ScreenFlip();
}
```

simpleなcamera、UI、visual effectには扱いやすい方式です。

## 14. Variable timestepの弱点

- collisionで大delta時にすり抜け。
- spring/constraintの安定性変化。
- inputとsimulation結果の再現性低下。
- 同じ操作でもframe patternで結果が変わる。
- network/replay tickとの対応が難しい。

Delta Timeを掛ければ全問題が消えるわけではありません。

## 15. Delta clamp

windowを移動した、breakpointで止めた、重いloadがあった場合に巨大deltaが来ます。

```cpp
constexpr double maxFrameDeltaSeconds = 0.25;

const double rawDeltaSeconds = clock.tickSeconds();
const double frameDeltaSeconds = std::clamp(
    rawDeltaSeconds,
    0.0,
    maxFrameDeltaSeconds);
```

clampで時間を捨てるため、simulationがwall timeへ完全追従しないtrade-offがあります。巨大一stepで壊すより安全なgameplay用途です。

## 16. Clamp値

0.25秒は例です。

- action window。
- physics stability。
- pause/focus処理。
- network timeout。
- replay requirement。

で決めます。network/auth timeoutのclockまでclampしてはいけません。

## 17. Clockを分ける

```text
Real Time       OS経過時間、network timeout、Profiler
Game Time       pause/time scaleの影響を受ける
Simulation Time fixed tickで進むworld
Presentation    hit stop/camera/UI個別時間
```

一つの`deltaTime`を全systemへ渡すとpauseやhit stopの範囲を制御できません。

## 18. Time scale

```cpp
const double scaledDeltaSeconds = realDeltaSeconds * timeScale;
```

- `timeScale = 1`: 通常。
- `timeScale = 0`: Game Time停止。
- `timeScale = 0.5`: half speed。

negativeや極端なscaleを許すか、audio pitch、particle、input buffer期限をどう扱うか決めます。

## 19. Pause

pause時にも:

- OS message。
- input（pause解除）。
- UI animation。
- audio menu。
- network受信/timeout。

は進める必要があります。World updateだけscaled delta 0、UIはreal deltaという分離が有効です。

## 20. Hit Stop

全gameをglobal time scale 0にする以外に、attacker/victim/action presentationだけを止めるlocal clockがあります。

```cpp
struct TimeContext
{
    double realDeltaSeconds;
    double gameDeltaSeconds;
    double combatDeltaSeconds;
};
```

どのsystemがどの時間を読むかを明示します。

## 21. Fixed timestep

simulationを一定幅で進めます。

```text
fixed delta = 1/60 second = 0.016666...

Sim Tick 100 → 101 → 102 → 103
Render frameは別frequency
```

毎描画frame一回ではなく、accumulatorに溜まった実時間分だけ0回以上実行します。

## 22. Accumulator

```cpp
constexpr double fixedDeltaSeconds = 1.0 / 60.0;
double accumulatorSeconds = 0.0;

while (ProcessMessage() == 0)
{
    const double frameDelta = std::min(clock.tickSeconds(), 0.25);
    accumulatorSeconds += frameDelta;

    while (accumulatorSeconds >= fixedDeltaSeconds)
    {
        world.fixedUpdate(fixedDeltaSeconds);
        accumulatorSeconds -= fixedDeltaSeconds;
    }

    world.render();
}
```

30fps描画なら一frameに約2 tick、120fps描画ならtickがないframeもあります。

## 23. Fixed tickとInput

inputは描画loopでsamplingし、edgeをbufferしてfixed tickで消費します。

```text
Render frameでAttack押下を検知
→ Input Command Bufferへtick/時刻付き保存
→ 次Fixed Tickが消費
```

fixed tickがないrender frameで押下を捨てないようにします。

## 24. Spiral of Death

処理が遅れてaccumulatorが増え、追いつくためfixed updateを大量実行し、さらに遅れる循環です。

```text
slow frame
→ 8 simulation steps必要
→ step処理でさらにslow
→ 次frameは12 steps必要
```

最大step数とdelta clampで防ぎます。

## 25. 最大step数

```cpp
constexpr int maxSimulationStepsPerFrame = 8;
int stepCount = 0;

while (accumulatorSeconds >= fixedDeltaSeconds &&
       stepCount < maxSimulationStepsPerFrame)
{
    world.fixedUpdate(fixedDeltaSeconds);
    accumulatorSeconds -= fixedDeltaSeconds;
    ++stepCount;
}

if (stepCount == maxSimulationStepsPerFrame)
{
    // 方針例: backlogを捨ててsimulation崩壊を防ぐ。
    // network/deterministic simulationでは単純破棄できない場合がある。
    accumulatorSeconds = std::fmod(accumulatorSeconds, fixedDeltaSeconds);
}
```

backlog破棄をlog/metricへ記録します。

## 26. Fixed timestepの選択

```text
30Hz  = 33.33ms: 軽いが高速actionには粗い
60Hz  = 16.67ms: 一般的な基準
120Hz =  8.33ms: 精密だがCPU cost約2倍方向
```

collision速度、input response、AI/physics cost、network tickを実測して選びます。

## 27. Simulation stateの二世代

描画補間用にprevious/current stateを保存します。

```cpp
void World::fixedUpdate(double fixedDeltaSeconds)
{
    previousState_ = currentState_; // step前状態を保存。
    simulate(currentState_, fixedDeltaSeconds);
}
```

大world全体をdeep copyせず、interpolateが必要なTransform等だけ二世代保持します。

## 28. Interpolation alpha

```cpp
const double alpha = accumulatorSeconds / fixedDeltaSeconds;
```

`alpha`は0～1未満で、次fixed tickまでどれほど時間が進んだかを表します。

```cpp
const float renderX = std::lerp(
    previousState.positionX,
    currentState.positionX,
    static_cast<float>(alpha));
```

## 29. なぜ補間するか

simulation 60Hz、display 144Hzでは同じsimulation stateを複数frame描くため、そのままでは細かくstutterします。previous/current間を表示して滑らかにします。

補間は見た目を少し過去に置きます。simulationの正しさとpresentation smoothingを分離します。

## 30. Rotation補間

angleを単純lerpすると359度→1度で遠回りする場合があります。

- 2D angleのshortest arc補間。
- 3D quaternionのslerp/nlerp。
- teleport時は補間せずsnap。

を使います。

## 31. Teleport

Scene移動、respawn、warpを補間すると壁を横切ります。

```cpp
void TransformHistory::teleport(const Transform& value)
{
    previous = value;
    current = value;
}
```

連続motionとdiscontinuous changeを区別します。

## 32. Renderはstateを変更しない

```cpp
void Player::render(double alpha) const;
```

render中にposition、cooldown、random、eventを変更するとrender frequencyでgameplayが変わります。logical state更新はsimulation側だけにします。

## 33. 完全loop例

```cpp
constexpr double fixedDelta = 1.0 / 60.0;
constexpr double maxFrameDelta = 0.25;
constexpr int maxSteps = 8;

GameClock clock;
double accumulator = 0.0;

while (ProcessMessage() == 0)
{
    const double rawDelta = clock.tickSeconds();
    const double frameDelta = std::clamp(rawDelta, 0.0, maxFrameDelta);
    accumulator += frameDelta;

    // 描画frameごとにdevice状態を読み、edge入力をcommand bufferへ保存する。
    input.sample();

    int steps = 0;
    while (accumulator >= fixedDelta && steps < maxSteps)
    {
        // fixed tick番号を進め、保存済みcommandをこのtickへ適用する。
        world.fixedUpdate(fixedDelta, input);
        accumulator -= fixedDelta;
        ++steps;
    }

    if (steps == maxSteps && accumulator >= fixedDelta)
    {
        // 過負荷をmetricへ残す。方針に従い古いbacklogを捨てる。
        accumulator = std::fmod(accumulator, fixedDelta);
    }

    const double alpha = accumulator / fixedDelta;

    ClearDrawScreen();
    world.renderInterpolated(alpha);

    if (ScreenFlip() == -1)
    {
        break;
    }
}
```

## 34. Tick number

```cpp
using Tick = std::uint64_t;

Tick simulationTick = 0;

while (accumulator >= fixedDelta)
{
    ++simulationTick;
    world.fixedUpdate(simulationTick, fixedDelta);
    accumulator -= fixedDelta;
}
```

replay、network command、debug logを同じinteger時間軸で扱えます。

## 35. Tickから時間

```cpp
const double simulationSeconds =
    static_cast<double>(simulationTick) * fixedDelta;
```

timerをfloat秒で減算し続ける代わりに、終了tickを保存する方法もあります。

```cpp
const Tick cooldownEndTick = currentTick + cooldownTicks;
const bool ready = currentTick >= cooldownEndTick;
```

## 36. 秒からtickへの丸め

```cpp
Tick secondsToTicksCeil(double seconds, double fixedDelta)
{
    return static_cast<Tick>(std::ceil(seconds / fixedDelta));
}
```

ceil/floor/roundでgameplay境界が変わります。「0.1秒無敵を最低保証」ならceil等、仕様で選びます。

## 37. Timer class

```cpp
class Countdown final
{
public:
    explicit Countdown(double durationSeconds)
        : remainingSeconds_(std::max(0.0, durationSeconds))
    {
    }

    void update(double deltaSeconds) noexcept
    {
        remainingSeconds_ = std::max(0.0, remainingSeconds_ - deltaSeconds);
    }

    [[nodiscard]] bool finished() const noexcept
    {
        return remainingSeconds_ <= 0.0;
    }

private:
    double remainingSeconds_;
};
```

同じtimerを複数systemがupdateしないようownerを一つにします。

## 38. Durationとtimestamp

- Duration countdown: Game Timeでpauseしたいcooldown。
- Absolute real timestamp: network timeout、file cache age。
- Tick deadline: deterministic action window。

用途に合う時間表現を選びます。

## 39. FPSとは

FPSは一定区間に表示したframe数です。

```cpp
class FpsCounter final
{
public:
    void onFrame(double realDeltaSeconds)
    {
        elapsed_ += realDeltaSeconds;
        ++frames_;

        if (elapsed_ >= 1.0)
        {
            fps_ = static_cast<double>(frames_) / elapsed_;
            elapsed_ = 0.0;
            frames_ = 0;
        }
    }

    [[nodiscard]] double fps() const noexcept { return fps_; }

private:
    double elapsed_ = 0.0;
    std::uint64_t frames_ = 0;
    double fps_ = 0.0;
};
```

## 40. Instant FPSのnoise

```cpp
const double instantFps = 1.0 / deltaSeconds;
```

一frame値は大きく揺れ、delta 0付近で危険です。移動平均、exponential smoothing、一定window集計を使います。

## 41. Frame timeを見る

60fps表示だけではhitchが見えません。

```text
Frame A 8ms
Frame B 25ms
平均上は約60fps付近でも不均一
```

milliseconds graph、95/99 percentile、worst frameを記録します。

## 42. VSync

DXライブラリの`SetWaitVSyncFlag`は`ScreenFlip`時にvertical syncを待つかを設定し、公式referenceではdefault TRUE、DirectX9版以降は`DxLib_Init`前のみ有効とされています。

```cpp
// 必ずDxLib_Init前。
SetWaitVSyncFlag(TRUE);
```

display refreshへpresentを合わせtearingを抑えます。

## 43. Refresh rate依存を除く

VSyncで60Hz/120Hz/144Hzにframe数が変わっても、simulation speedはdelta/fixed tickで一定にします。

```text
誤り: ScreenFlip一回につき3 pixels
正解: 180 pixels/second × delta
```

VSyncはgame clockではなくpresent pacingの一要素です。

## 44. VSyncの待ち時間

Profilerで`ScreenFlip`が長く見える場合、GPU/present/VSync待ちを含む可能性があります。

```cpp
const auto before = Clock::now();
ScreenFlip();
const auto after = Clock::now();
const auto flipTime = after - before;
```

待っていることとCPUで重い計算をしていることを区別します。

## 45. VSyncを無効化する場合

```cpp
// DxLib_Initより前にのみ効果がある仕様を確認する。
SetWaitVSyncFlag(FALSE);
```

利点:

- uncapped性能測定。
- custom frame limiter。

欠点:

- tearing。
- CPU/GPU使用率・発熱上昇。
- frame pacing設計が必要。

## 46. `WaitTimer`

DXライブラリにはmillisecond指定の待機関数があります。

```cpp
WaitTimer(16);
```

ただし処理時間を無視して毎frame16ms待つと:

```text
work 5ms + wait 16ms = 21ms ≒ 47.6fps
```

になります。frame deadlineまでの残り時間を待ちます。

## 47. Sleep精度

OS sleepは指定時刻ぴったりのwakeを保証せず、少なくともそのduration待つ方向のscheduler requestです。

- timer resolution。
- OS load。
- power policy。
- thread scheduling。

でoversleepします。busy waitだけならCPUを浪費します。

## 48. `sleep_until`

相対sleepを毎frame積むより、絶対deadlineを進めます。

```cpp
using Clock = std::chrono::steady_clock;
constexpr auto targetFrame = std::chrono::duration<double>(1.0 / 60.0);

auto nextDeadline = Clock::now();

while (running)
{
    nextDeadline += std::chrono::duration_cast<Clock::duration>(targetFrame);

    processFrame();

    std::this_thread::sleep_until(nextDeadline);
}
```

処理がdeadline超過したときのskip/resync方針が必要です。

## 49. Hybrid wait

精度を求める場合:

```text
deadlineまで2ms以上 → sleep
残り短時間         → yield/short spin
deadline到達       → 次frame
```

とする方法がありますが、busy spinはCPU/電力を消費します。まずVSyncまたは通常sleepで要件を満たすか測ります。

## 50. Frame limiter例

```cpp
class FrameLimiter final
{
public:
    explicit FrameLimiter(double framesPerSecond)
        : frameDuration_(std::chrono::duration<double>(1.0 / framesPerSecond)),
          deadline_(Clock::now())
    {
    }

    void wait()
    {
        deadline_ += std::chrono::duration_cast<Clock::duration>(frameDuration_);
        const auto now = Clock::now();

        // 大幅に遅れた場合、過去deadlineを追い続けず現在から再同期する。
        if (now > deadline_ + std::chrono::milliseconds(250))
        {
            deadline_ = now;
            return;
        }

        std::this_thread::sleep_until(deadline_);
    }

private:
    using Clock = std::chrono::steady_clock;
    std::chrono::duration<double> frameDuration_;
    Clock::time_point deadline_;
};
```

constructorで0以下fpsを拒否するvalidationも必要です。

## 51. LimiterとVSync二重待ち

VSyncで既にrefresh待ちした後、さらに16.67ms limiter waitするとfpsが半分近くになる場合があります。

```text
VSync wait ≈ 16.7ms
custom wait ≈ 16.7ms
total      ≈ 33.4ms → 約30fps
```

present modeとlimiterの責任を一つに決めます。

## 52. Render frequencyとSimulation frequency

```text
Simulation: 60Hz fixed
Rendering:   display/VSyncに応じ60/120/144Hz
Input sample: render frameまたは別poll頻度
Network:     20/30/60Hz
AI decision: 5/10/20Hz
```

全systemを同じfrequencyで動かす必要はありません。

## 53. Slow motionとFixed Step

real deltaをscaleしてaccumulatorへ入れる方式:

```cpp
accumulator += realDelta * gameTimeScale;
```

ではtimeScale 0.5でsimulation tick頻度が半分になり、render補間で滑らかに見せます。固定tick幅自体を変える方式はphysics挙動を変えるため注意します。

## 54. Fast forward

timeScaleを上げると一render frameのfixed step数が増えます。max stepへ達しやすくなるため:

- renderを間引く。
- simulationだけ高速loop。
- effect/audioを簡略化。
- maximum scaleを制限。

します。

## 55. Determinism

fixed timestepだけで完全決定論にはなりません。

- floating point/platform差。
- container iteration順。
- random seed。
- thread scheduling。
- input quantization。
- physics implementation。

が結果を変えます。Replay/network要件に応じstate correctionやinteger/fixed-point等を検討します。

## 56. Randomと時間

```cpp
// 悪い例: frameごとに乱数を一回使うためfpsで試行回数が変わる。
if (randomChance()) spawnParticle();
```

rate per secondをdeltaからprobabilityへ変換するか、fixed tick/event scheduleを使います。

Poisson過程なら単純`rate * delta`は小delta近似であり、厳密には`1 - exp(-rate * delta)`等を用います。

## 57. Animation frame

sprite animationも「render frame一回でindex++」にしません。

```cpp
animationTimeSeconds += deltaSeconds;

const int frameIndex = static_cast<int>(
    animationTimeSeconds / secondsPerAnimationFrame) % frameCount;
```

eventを飛び越えた場合、旧時刻と新時刻の間にあるeventを全処理する必要があります。

## 58. Cooldown boundary

```cpp
remaining = std::max(0.0, remaining - delta);

if (previousRemaining > 0.0 && remaining <= 0.0)
{
    onCooldownFinished(); // crossingを一度だけ検出。
}
```

毎frame`remaining <= 0`でeventを出すと無限に発火します。

## 59. Focus loss

windowが非activeの間:

- simulationを止めるか。
- audioをpauseするか。
- networkは継続するか。
- CPU usageを下げるか。
- 復帰時clock resetするか。

を仕様化します。single-playerとonlineで異なります。

## 60. Debugger breakpoint

breakpoint後のraw deltaが30秒でも、game worldを30秒simulateしません。

- development時delta clamp。
- debugger検知ではなく一般的な巨大delta処理。
- clock reset shortcut。
- fixed backlog metric。

を用意します。

## 61. Frame timing log

```cpp
struct FrameTiming
{
    double rawDeltaMs;
    double clampedDeltaMs;
    double updateMs;
    double renderMs;
    double presentMs;
    int simulationSteps;
};
```

平均だけでなくring bufferに最近数百frameを保存し、hitch時にdumpします。

## 62. CPU計測scope

```cpp
class ScopedTimer final
{
public:
    ScopedTimer(const char* name, Logger& logger)
        : name_(name), logger_(logger), start_(Clock::now()) {}

    ~ScopedTimer()
    {
        const std::chrono::duration<double, std::milli> elapsed =
            Clock::now() - start_;
        logger_.timing(name_, elapsed.count());
    }

private:
    using Clock = std::chrono::steady_clock;
    const char* name_;
    Logger& logger_;
    Clock::time_point start_;
};
```

毎framefileへ同期logすると計測対象を重くするため、buffer/Profilerを使います。

## 63. Unit test

Clockを直接読むclassはtestしにくいため、delta/tickを外から渡します。

```cpp
TEST(Countdown, FinishesAtBoundary)
{
    Countdown timer(0.1);
    timer.update(0.04);
    timer.update(0.06);
    EXPECT_TRUE(timer.finished());
}
```

fake clock interfaceでpause/reset/large jumpもtestできます。

## 64. Frame pattern test

同じ1秒を異なるdelta列で進めます。

```text
60 × 1/60
30 × 1/30
120 × 1/120
0.4 + 0.3 + 0.3
```

variable movementの最終位置、fixed simulation tick数、clamp時の仕様を比較します。

## 65. Floating error

`accumulator >= fixedDelta`の長期誤差を理解し、doubleを使います。より厳密なschedulerではclock durationのinteger tick/countでaccumulatorを持つ方法もあります。

```cpp
using Nanoseconds = std::chrono::nanoseconds;
Nanoseconds accumulator{0};
```

ただしgameplay計算との変換境界を明確にします。

## 66. よくある失敗

### 一frame固定移動

refresh rateでspeedが変わる。per-second速度×deltaへする。

### Deltaをmillisecondのまま掛ける

1000倍速になる。秒へ統一する。

### 巨大deltaをそのまま使う

すり抜け・timer全終了。clamp/resetする。

### Fixed updateを必ず一frame一回

render rateとsimulation rateが再び結合する。accumulatorを使う。

### backlogを無制限に処理

spiral of death。max stepsと過負荷policyを持つ。

### VSyncとcustom limiterを二重使用

約半分のfpsになる。present pacing ownerを一つにする。

### Renderでstate変更

display refreshでgameplayが変わる。simulationだけがstateを更新する。

## 67. 完成checklist

- [ ] monotonic clockを使っている。
- [ ] clock unitを秒へ一度だけ変換している。
- [ ] load/pause/focus復帰でclockをresetできる。
- [ ] raw deltaとclamped deltaを区別している。
- [ ] network timeout等はclamped Game Timeを使わない。
- [ ] fixed accumulatorに最大step数がある。
- [ ] input edgeをfixed tickまでbufferする。
- [ ] previous/current stateを描画補間できる。
- [ ] teleport時にhistoryをresetする。
- [ ] renderがlogical stateを変更しない。
- [ ] VSync設定を`DxLib_Init`前に行う。
- [ ] VSyncとlimiterを二重に待たない。
- [ ] frame time percentile/hitchを観測できる。
- [ ] 異なるdelta patternでtestした。

## 68. 確認問題

1. 一frame3 pixels移動が120Hzで二倍速になる理由は何か。
2. `steady_clock`がgame deltaへ適する理由は何か。
3. millisecondをsecondsへ直す係数はいくつか。
4. delta clampで何を守り、何を失うか。
5. Variable timestepの弱点は何か。
6. Fixed accumulatorが一frameに0回または複数回updateする理由は何か。
7. Spiral of Deathとは何か。
8. interpolation alphaはどう計算するか。
9. VSyncをgame speedの基準にしてはいけない理由は何か。
10. VSyncと16ms sleepを二重に使うと何が起こるか。

## 69. 次章への接続

次章ではinputを時間軸へ接続します。

```text
Device current state
→ previous/current差分
→ Pressed / Held / Released
→ Command
→ timestamp/tick付きBuffer
→ gameplay stateが消費
```

keyboard、mouse、gamepadの違いをInput Managerへ閉じ込めます。

## 70. 公式資料

- [DXライブラリ公式・その他関数リファレンス](https://dxlib.xsrv.jp/function/dxfunc_other.html)
- [DXライブラリ公式・サンプルプログラム](https://dxlib.xsrv.jp/dxprogram.html)
- [cppreference `std::chrono::steady_clock`](https://en.cppreference.com/w/cpp/chrono/steady_clock)
- [cppreference `std::this_thread::sleep_until`](https://en.cppreference.com/w/cpp/thread/sleep_until)

timer resolution、VSync、sleep精度はOS、driver、display、DXライブラリversionで変わるため、target環境でframe timeを計測してください。
