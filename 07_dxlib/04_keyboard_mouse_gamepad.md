# Keyboard・Mouse・Gamepad・Action Mapping

> 対象: Windows版DXライブラリ。Device状態を直接gameplayへ渡さず、`Physical Input → Logical Action → Command Buffer`へ変換します。

## 1. Input pipeline

```text
OS / Device
→ DXライブラリ入力API
→ Raw Device State
→ Dead Zone / Normalize / Edge検出
→ Binding / Logical Action
→ Command / Buffer
→ Gameplay Stateが消費
```

Player classが`KEY_INPUT_Z`や`PAD_INPUT_1`を直接読むと、rebind、AI、replay、network、testへ拡張しにくくなります。

## 2. PollingとEvent

- Polling: 現在押されているかをframeごとに読む。
- Event/Log: 押した・離した履歴を順に読む。

keyboard movementのheld状態はpolling、低fpsでも短いmouse clickを逃したくない場合は入力logが適します。DXライブラリの`GetMouseInputLog2`は押下/解放情報とその時点の座標を蓄積して取得できます。

## 3. `ProcessMessage`との関係

Windows messageと入力内部状態を更新するため、main loopで定期的に`ProcessMessage`を呼びます。重いload中に戻らないと入力・window responseが遅れます。

```cpp
while (ProcessMessage() == 0)
{
    input.update();
    // simulation / render
}
```

入力APIをworker threadへ無秩序に移さず、main loopの決まった地点でsnapshotを作ります。

## 4. Current・Previous state

```text
Previous Current Result
0        0       Up
0        1       Pressed
1        1       Held
1        0       Released
```

現在状態一つだけでは押した瞬間と押し続けを区別できません。

## 5. ButtonState

```cpp
struct ButtonState
{
    bool previous = false;
    bool current = false;

    [[nodiscard]] bool pressed() const noexcept
    {
        return current && !previous;
    }

    [[nodiscard]] bool held() const noexcept
    {
        return current;
    }

    [[nodiscard]] bool released() const noexcept
    {
        return !current && previous;
    }
};
```

`pressed`は一回、`held`は毎frame、`released`は離した一回だけtrueです。

## 6. Update順序

```cpp
void updateButton(ButtonState& state, bool newPhysicalState) noexcept
{
    // 古いcurrentをpreviousへ退避してから、新しいdevice値をcurrentへ入れる。
    state.previous = state.current;
    state.current = newPhysicalState;
}
```

順序を逆にするとprevious/currentが同じ値になりedgeが消えます。

## 7. Keyboard単一key

```cpp
const bool escapeHeld = CheckHitKey(KEY_INPUT_ESCAPE) != 0;
```

公式referenceでは押下中1、非押下0です。少数keyを素早く試すには簡単ですが、多数keyなら一括取得を使います。

## 8. Keyboard全key取得

```cpp
std::array<char, 256> rawKeys{};

if (GetHitKeyStateAll(rawKeys.data()) == -1)
{
    // 入力snapshot取得失敗。以前の状態を維持するか全releaseにするか仕様化する。
}
```

公式referenceでは`char[256]`を渡し、indexへ`KEY_INPUT_*`を使います。

## 9. Keyboard snapshot

```cpp
class Keyboard final
{
public:
    static constexpr std::size_t KeyCount = 256;

    [[nodiscard]] bool update() noexcept
    {
        previous_ = current_;

        std::array<char, KeyCount> raw{};
        if (GetHitKeyStateAll(raw.data()) == -1)
        {
            return false;
        }

        for (std::size_t i = 0; i < KeyCount; ++i)
        {
            current_[i] = raw[i] != 0;
        }
        return true;
    }

    [[nodiscard]] bool pressed(int keyCode) const noexcept
    {
        return valid(keyCode) && current_[keyCode] && !previous_[keyCode];
    }

    [[nodiscard]] bool held(int keyCode) const noexcept
    {
        return valid(keyCode) && current_[keyCode];
    }

    [[nodiscard]] bool released(int keyCode) const noexcept
    {
        return valid(keyCode) && !current_[keyCode] && previous_[keyCode];
    }

private:
    static bool valid(int code) noexcept
    {
        return code >= 0 && code < static_cast<int>(KeyCount);
    }

    std::array<bool, KeyCount> previous_{};
    std::array<bool, KeyCount> current_{};
};
```

## 10. 初frame

起動時にkeyが既に押されていると最初のupdateでPressedになります。それを許すか、current/previousを同じsnapshotで初期化してedgeを抑えるか決めます。

focus復帰時も同様です。Alt-Tab中のreleaseを取り逃す可能性があるため、全state resetまたはresampleします。

## 11. Key rollover

keyboard hardwareや接続方式により同時押し可能組合せが異なります。特定3key combinationが取れない場合、codeだけで直せないことがあります。

重要actionを同時押し前提にし過ぎず、複数bindingと実keyboard testを行います。

## 12. Text inputとkey input

`KEY_INPUT_A`はphysical key状態であり、userが入力した文字`a/A/あ`とは別です。

```text
Gameplay command: key state
Name/chat field: IME/text input system
```

Shift、keyboard layout、IME、composition、clipboardを扱うtext inputへkey pollingを流用しません。

## 13. Mouse position

```cpp
int mouseX = 0;
int mouseY = 0;

if (GetMousePoint(&mouseX, &mouseY) == -1)
{
    // 取得失敗。
}
```

座標がclient area、logical resolution、scalingのどの空間かを確認し、UI/world ray変換で混同しません。

## 14. Mouse button mask

```cpp
const int buttons = GetMouseInput();
const bool left = (buttons & MOUSE_INPUT_LEFT) != 0;
const bool right = (buttons & MOUSE_INPUT_RIGHT) != 0;
```

bit maskは`== MOUSE_INPUT_LEFT`で全体比較せず、bitwise ANDで対象bitだけ調べます。

## 15. Mouse snapshot

```cpp
struct MouseState
{
    int x = 0;
    int y = 0;
    int deltaX = 0;
    int deltaY = 0;
    int buttons = 0;
    int previousButtons = 0;
    int wheelDelta = 0;
};

bool updateMouse(MouseState& state)
{
    const int oldX = state.x;
    const int oldY = state.y;

    state.previousButtons = state.buttons;

    if (GetMousePoint(&state.x, &state.y) == -1)
        return false;

    state.deltaX = state.x - oldX;
    state.deltaY = state.y - oldY;
    state.buttons = GetMouseInput();
    state.wheelDelta = GetMouseWheelRotVol();
    return true;
}
```

first updateのdeltaを0にする初期化flagも必要です。

## 16. Mouse edge

```cpp
bool mousePressed(const MouseState& state, int mask) noexcept
{
    return (state.buttons & mask) != 0 &&
           (state.previousButtons & mask) == 0;
}

bool mouseReleased(const MouseState& state, int mask) noexcept
{
    return (state.buttons & mask) == 0 &&
           (state.previousButtons & mask) != 0;
}
```

mouse buttonをmask単位で扱います。

## 17. Mouse click log

Polling間隔より短いpress→releaseは両snapshotでupになり見逃します。DXライブラリの`GetMouseInputLog2`は履歴からbutton、座標、press/release種別を取得できます。

```text
Frame N sample: Up
その間にDown→Up
Frame N+1 sample: Up
```

UIの確実なclick履歴が必要ならlog APIを検討します。

## 18. Mouse wheel

`GetMouseWheelRotVol`は前回呼び出し以降の回転量を返す契約です。複数systemが呼ぶと最初のsystemが値を消費する可能性があるため、Input Managerが一回だけ読みsnapshotへ保存します。

## 19. Mouse coordinate spaces

```text
OS screen coordinates
Window client coordinates
DX logical screen coordinates
UI canvas coordinates
World coordinates
```

の変換を明示します。letterbox/render scaleがある場合、黒帯外をrejectしlogical coordinateへscaleします。

## 20. Relative mouse look

絶対position差でcameraを回す場合:

- window端で止まる。
- focus復帰で大delta。
- cursor warpingが擬似inputを生む。
- DPI/scaling差。

があります。中央へ`SetMousePoint`する方式はwarp後deltaを除外し、OS raw inputが必要なら別adapterを検討します。

## 21. Mouse cursor表示

```cpp
SetMouseDispFlag(TRUE);  // menu
SetMouseDispFlag(FALSE); // camera look
```

Scene側が直接切り替えず、Input/Window mode ownerがgameplay/UI mode transitionで管理します。終了/focus loss時の復元も考えます。

## 22. Gamepad接続数

```cpp
const int connectedCount = GetJoypadNum();
```

接続数と`PAD1`が同じphysical deviceを永続的に表すとは限りません。抜き差し、再列挙、複数controllerでplayer assignmentを設計します。

## 23. Gamepad digital state

```cpp
const int pad = GetJoypadInputState(DX_INPUT_PAD1);

const bool up = (pad & PAD_INPUT_UP) != 0;
const bool attack = (pad & PAD_INPUT_1) != 0;
```

戻り値はbutton bit maskです。`DX_INPUT_KEY_PAD1`はkeyboardとpad 1を統合した入力も返せますが、device別表示/rebindが必要なら個別取得します。

## 24. KeyboardとPadの統合shortcut

`DX_INPUT_KEY_PAD1`は簡単なgameには便利です。しかし:

- どのdeviceが最後に操作したか分からない。
- key/pad別binding表示が難しい。
- multiplayer local assignmentが曖昧。
- rebind dataを分離しにくい。

ため、規模が増えたらlogical action layerで統合します。

## 25. Gamepad analog input

```cpp
int rawX = 0;
int rawY = 0;

if (GetJoypadAnalogInput(&rawX, &rawY, DX_INPUT_PAD1) == -1)
{
    // 非接続または取得失敗。
}
```

取得range/axis方向を実deviceで確認し、後段で-1～1へnormalizeします。

## 26. Analog normalize

raw最大値を`maxMagnitude`とした例:

```cpp
float normalizeAxis(int raw, int maxMagnitude) noexcept
{
    if (maxMagnitude <= 0)
        return 0.0f;

    return std::clamp(
        static_cast<float>(raw) / static_cast<float>(maxMagnitude),
        -1.0f,
        1.0f);
}
```

実際のrangeがpositive/negative非対称なら各側を別scaleにします。

## 27. Y axis方向

device raw Yの正方向とgame world/screen Yの正方向が違う場合があります。

```cpp
const float moveY = -normalizeAxis(rawY, maxRaw);
```

invert camera Yはuser settingであり、device normalizeと別層にします。

## 28. Dead zone

stick中心は0に完全一致せずdriftすることがあります。

```text
raw magnitude < dead zone → zero
dead zone外              → 0..1へ再map
```

単に小値を0にするだけだとdead zone境界で値がjumpします。

## 29. Radial dead zone

```cpp
struct Vec2
{
    float x = 0.0f;
    float y = 0.0f;
};

Vec2 applyRadialDeadZone(Vec2 value, float deadZone)
{
    const float magnitude = std::sqrt(value.x * value.x + value.y * value.y);

    if (magnitude <= deadZone || magnitude <= 0.0f)
        return {};

    const float clampedMagnitude = std::min(magnitude, 1.0f);
    const float remapped = (clampedMagnitude - deadZone) / (1.0f - deadZone);
    const float scale = remapped / magnitude;

    return {value.x * scale, value.y * scale};
}
```

`deadZone`は0以上1未満へvalidationします。

## 30. Axial dead zoneとの違い

- Axial: X/Y別に小値を0。cardinal方向を出しやすいが形が十字的。
- Radial: vector magnitudeで判定。全方向の感触が均一。

movement、menu navigation、trigger等で使い分けます。

## 31. Square-to-circle問題

X/Yを個別normalizeするとdiagonal magnitudeが最大`√2`になります。

```cpp
const float magnitude = std::sqrt(x*x + y*y);
if (magnitude > 1.0f)
{
    x /= magnitude;
    y /= magnitude;
}
```

diagonal movementが約1.414倍にならないようclampします。

## 32. Response curve

dead zone後の値へcurveを適用できます。

```cpp
float applyExpo(float value, float exponent)
{
    return std::copysign(std::pow(std::abs(value), exponent), value);
}
```

exponent > 1で中心を細かく、<1で早く反応します。cameraとmovementで別設定にします。

## 33. DirectInput raw state

`GetJoypadDirectInputState`は`DINPUT_JOYSTATE`へaxis、POV、buttons等の生情報を取得します。

用途:

- key configuration。
- device別axis/button調査。
- simple maskで足りないcontroller。

deviceごとにaxis mapping/rangeが異なる可能性を前提にします。

## 34. XInput state

`GetJoypadXInputState`はXInput対応controllerのraw stateを取得し、左右trigger等を独立して扱えます。公式referenceではXInput対象でないpadでは-1になります。

XInput失敗をgame全体errorにせず、DirectInput/簡易API等へfallbackするdevice backend設計を検討します。

## 35. Trigger

triggerはbuttonではなく0～最大のanalog値の場合があります。

- dead threshold。
- normalized 0..1。
- pressed/held/released threshold crossing。
- release thresholdを少し低くするhysteresis。

を持たせ、noiseで連打しないようにします。

## 36. Hysteresis

```cpp
bool updateAnalogButton(bool previousHeld, float value) noexcept
{
    constexpr float pressThreshold = 0.55f;
    constexpr float releaseThreshold = 0.45f;

    return previousHeld
        ? value > releaseThreshold
        : value >= pressThreshold;
}
```

press/release境界を分けるとthreshold付近のchatteringを防げます。

## 37. Vibration

DXライブラリには開始/停止振動APIがあります。presentation requestとして扱います。

- deviceが対応するか。
- player assignment。
- intensity/duration。
- 複数requestのpriority。
- pause/focus loss/切断時停止。
- accessibility setting。

damage domainが直接pad番号を指定しません。

## 38. Device disconnect

pad取得失敗時:

- current buttonsをrelease扱いにする。
- pause/UI通知。
- assignmentを保持して再接続待ち。
- keyboard fallback。
- vibration停止。

を決めます。以前のheld stateを残すと移動し続けます。

## 39. Reconnect edge

再接続時にbuttonが押されたままだと全てPressedになることがあります。first snapshotではprevious=currentにしてedgeを抑えるか、確認buttonだけ特別処理します。

## 40. Device IDとSlot

`PAD1`というslotとphysical device identityは別です。local multiplayerでは:

```text
Physical Device
→ Runtime Device Record
→ Player Slot Assignment
→ Logical Action State
```

を管理します。接続順だけを永久IDとしません。

## 41. Logical Action

```cpp
enum class Action
{
    MoveLeft,
    MoveRight,
    Jump,
    LightAttack,
    HeavyAttack,
    Dodge,
    Guard,
    Pause,
    Count
};
```

gameplayはphysical keyでなくActionを読みます。

## 42. Axis Action

digital buttonとcontinuous axisを分けます。

```cpp
enum class Axis
{
    MoveX,
    MoveY,
    LookX,
    LookY,
    Count
};
```

keyboard A/Dは-1/+1、stick Xはanalog値として同じMoveXへ合成できます。

## 43. Binding

```cpp
enum class DeviceKind { Keyboard, Mouse, Gamepad };

struct ButtonBinding
{
    DeviceKind device;
    int code;
};

struct AxisBinding
{
    DeviceKind device;
    int code;
    float scale = 1.0f;
};
```

実装ではplayer/device ID、modifier、chord、dead zone、thresholdも含めます。

## 44. Action state合成

一Actionへ複数bindingを割り当てます。

```text
Jump = Keyboard Space OR Gamepad Button 1
MoveX = D(+1) + A(-1) + LeftStickX
```

buttonはOR、axisは最大絶対値または加算後clamp等、合成規則を定義します。

## 45. Last used device

UI promptを`[Space]`/`[A]`へ切り替えるため、meaningful inputを最後に出したdeviceを記録します。

- stick driftを入力扱いしない。
- mouse微小movementへthreshold。
- device切替cooldown/hysteresis。
- keyboardとmouseを同groupにするか。

を設計します。

## 46. Input context

同じbuttonでもcontextでActionが変わります。

```text
Gameplay: Space = Dodge
Menu:     Space = Confirm
Dialog:   Space = Advance
Rebind:   Space = Captured physical input
```

active context stackとpriorityを持ち、複数contextが同じpressを二重消費しないようにします。

## 47. Input consumption

```cpp
struct ActionEvent
{
    Action action;
    std::uint64_t sequence;
    bool consumed = false;
};
```

UIがPause pressを消費したらGameplayへ渡さない等、routing ownerを一つにします。単なるglobal action stateではconsumer競合が起きます。

## 48. Commandへの変換

```cpp
enum class CombatCommand
{
    LightAttack,
    HeavyAttack,
    Dodge,
    GuardPressed,
    GuardReleased
};

struct TimedCommand
{
    CombatCommand command;
    std::uint64_t sequence;
    double realTimestampSeconds;
    std::uint64_t targetSimulationTick;
};
```

device情報をcombat domainへ持ち込みません。

## 49. Input buffer

```cpp
class CommandBuffer final
{
public:
    void push(TimedCommand command)
    {
        commands_.push_back(command);
    }

    template<class Predicate>
    std::optional<TimedCommand> consumeFirst(Predicate canConsume)
    {
        const auto iterator = std::find_if(
            commands_.begin(), commands_.end(), canConsume);

        if (iterator == commands_.end())
            return std::nullopt;

        TimedCommand result = *iterator;
        commands_.erase(iterator);
        return result;
    }

private:
    std::deque<TimedCommand> commands_;
};
```

期限、最大数、priority、duplicate policyを追加します。

## 50. Fixed tickへの受け渡し

render frameでsampleしてPressedを一度commandへ変換し、次fixed tickが消費します。

```text
144Hz Render Input Sample
→ command queue
60Hz Fixed Simulation
→ sequence順にconsume
```

Pressedを各fixed stepで再計算すると同じphysical pressを複数回使う危険があります。

## 51. Held inputとFixed tick

movement axis/guard held等は最新snapshotを各tickで読めます。一回eventと持続値を分けます。

```cpp
struct PlayerInputFrame
{
    Vec2 move;
    Vec2 look;
    bool guardHeld;
    std::vector<TimedCommand> edgeCommands;
};
```

## 52. Repeat

menu長押しrepeatはOS key repeatをそのままgameplayへ使わず、自前timerで統一できます。

```text
Pressedで一回
initial delay 0.4s
以後0.08sごと
Releasedで停止
```

real/unscaled timeを使い、frame rate依存を避けます。

## 53. Double tap

```cpp
if (pressed && now - lastPressedTime <= doubleTapWindow)
{
    emit(DoubleTap);
}
lastPressedTime = now;
```

同じkey、direction、release必要条件、pause中のclock、triple tap時の重複を仕様化します。

## 54. Chord

Shift+Button等の同時入力では評価順と猶予を決めます。

- modifier held時だけ。
- 数十ms以内ならchord。
- simple actionを遅延してchord待ちするか。
- rebind UIでreserved chordをどう扱うか。

高速actionでは意図しない入力遅延を避けます。

## 55. Rebinding

```text
Capture開始
→ 現在held keyがreleaseされるまで待つ
→ 次のmeaningful inputを取得
→ reserved/conflict検証
→ 仮binding表示
→ confirm
→ UTF-8 configへsave
```

Escape cancel、duplicate許可、controller未接続、default復元を扱います。

## 56. Binding表示

internal codeからuser向け名称へ変換します。

```text
KEY_INPUT_SPACE → "Space"
PAD_INPUT_1     → device layoutに応じたglyph/name
```

physical position表示とlogical label（A/B、Cross/Circle等）をplatform/deviceごとに確認します。特定platformの商標・表示規則にも従います。

## 57. Save format

```json
{
  "version": 1,
  "actions": {
    "LightAttack": [
      { "device": "Keyboard", "code": "Z" },
      { "device": "Gamepad", "code": "FaceLeft" }
    ]
  }
}
```

DX macroの整数値を永続IDに直保存せず、stableな自前IDからruntime codeへmapします。

## 58. Input Manager責任

```cpp
class InputManager final
{
public:
    bool update();

    [[nodiscard]] const Keyboard& keyboard() const noexcept;
    [[nodiscard]] const MouseState& mouse() const noexcept;
    [[nodiscard]] const GamepadState& gamepad(PlayerId player) const;
    [[nodiscard]] const ActionSnapshot& actions(PlayerId player) const;

private:
    Keyboard keyboard_;
    MouseState mouse_;
    std::vector<GamepadState> gamepads_;
    BindingDatabase bindings_;
};
```

DX入力APIを呼ぶ場所を集中させます。

## 59. Backend interface

```cpp
class IInputBackend
{
public:
    virtual ~IInputBackend() = default;
    virtual bool poll(RawInputSnapshot& output) = 0;
};
```

- DxLibInputBackend。
- ReplayInputBackend。
- TestInputBackend。
- NetworkInputBackend。

を同じlogical mappingへ接続できます。

## 60. Replay

raw device stateでなくlogical command/tickを記録するとdevice差を除けます。

```text
Tick 100 Move(0.5, 1.0)
Tick 102 LightAttack Pressed
Tick 118 Dodge Pressed
```

version、binding後値、random seed、game data hashも記録します。

## 61. Test

DX hardware APIを使わずsnapshotを注入します。

```cpp
TEST(ButtonState, DetectsPressHoldRelease)
{
    ButtonState state{};

    updateButton(state, true);
    EXPECT_TRUE(state.pressed());

    updateButton(state, true);
    EXPECT_TRUE(state.held());
    EXPECT_FALSE(state.pressed());

    updateButton(state, false);
    EXPECT_TRUE(state.released());
}
```

dead zone境界、disconnect、reconnect、multiple bindingもtestします。

## 62. Input latency

```text
Device sample
→ game update
→ render submission
→ GPU render
→ VSync/present
→ display scanout
```

入力をframeの早い位置でsampleし、余計なframe queueを避けます。ただし固定tick command orderingと一致させます。

## 63. Low FPS

低fpsでは:

- pollingで短いclickを逃す。
- mouse deltaが大きくなる。
- fixed tickへ複数step入力をどう配るか。
- command buffer期限をreal/tickどちらで測るか。

が問題になります。20fps、hitch、background復帰をtestします。

## 64. Debug表示

表示するもの:

- raw key/button bit。
- previous/current/pressed/released。
- raw/normalized/dead-zone後stick。
- connected device/assigned player。
- active context。
- logical action値。
- command queue/sequence/age。
- last used device。

見えない入力を可視化すると誤bindingとdriftを診断できます。

## 65. よくある失敗

### `CheckHitKey`をPlayerから直接呼ぶ

deviceとgameplayが結合する。Input ManagerでActionへ変換する。

### currentだけでPressed判定

押し続け中毎frame発火する。previous/currentのedgeを見る。

### Input updateを複数回呼ぶ

同frame中にpreviousが上書きされedgeが消える。一frame一snapshot。

### Stick X/Yを個別clamp

diagonalが速くなる。vector magnitudeを1へclampする。

### Dead zone内を0にするだけ

境界で値がjumpする。残りrangeを0～1へremapする。

### Disconnect後stateを残す

移動/guardし続ける。release/reset policyを持つ。

### Wheel APIを複数systemが読む

累積値を先に消費する。managerが一回だけ読む。

## 66. 完成checklist

- [ ] `ProcessMessage`後の決まった地点で一回pollする。
- [ ] keyboard全keyをsnapshot化する。
- [ ] previous/currentからPressed/Held/Releasedを作る。
- [ ] mouse buttonをbit maskで判定する。
- [ ] click logが必要な低fps用途を理解する。
- [ ] wheelを一回だけ取得する。
- [ ] stickをnormalizeしradial dead zoneを適用する。
- [ ] diagonal magnitudeをclampする。
- [ ] analog triggerへhysteresisを使う。
- [ ] disconnect/reconnectを扱う。
- [ ] physical bindingとlogical Actionを分離する。
- [ ] contextとconsumptionを設計する。
- [ ] edge commandをfixed tickへbufferする。
- [ ] rebindをstable IDでsaveする。
- [ ] fake backendでtestできる。

## 67. 確認問題

1. Current stateだけではPressedを判定できない理由は何か。
2. `GetHitKeyStateAll`へ渡すbufferの要件は何か。
3. key inputとtext/IME inputの違いは何か。
4. Mouse pollingで短いclickを逃す理由は何か。
5. Wheel値をInput Managerだけが読むべき理由は何か。
6. Radial dead zoneのremapが必要な理由は何か。
7. diagonal stick入力をそのまま使うと何倍になり得るか。
8. `DX_INPUT_KEY_PAD1`が大規模設計で不便になる理由は何か。
9. Physical InputとLogical Actionを分ける利点は何か。
10. Pressed commandをfixed updateごとに再生成してはいけない理由は何か。

## 68. 次章への接続

次章では入力結果を2D画面へ描きます。

```text
Logical coordinate
→ color
→ point/line/box/circle
→ text
→ clipping
→ camera/view transform
→ logical resolution scaling
```

Input Debug画面も図形・文字描画systemとして構築します。

## 69. 公式資料

- [DXライブラリ公式・入力関係関数](https://dxlib.xsrv.jp/function/dxfunc_input.html)
- [DXライブラリ公式・関数一覧](https://dxlib.xsrv.jp/dxfunc.html)
- [DXライブラリ公式・キーコンフィグ](https://dxlib.xsrv.jp/program/dxprogram_KeyConfig.html)

controllerごとのaxis/button配置、DirectInput/XInput対応、振動機能はdeviceとDXライブラリversionで異なるため、raw debug表示を作って実機確認してください。
