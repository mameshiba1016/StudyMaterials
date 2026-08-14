# 第12章 Debug Log・Gizmo・Error処理

Debug機能の目的は、問題が起きた後に推測するのではなく「何が、いつ、どの状態で起きたか」を再現可能にすることです。本章ではLog、Assert、Error、画面Console、Gizmo、計測を一つの観測基盤として設計します。

## 1. 観測可能性

- Log: 離散的な出来事と文脈。
- Metrics: 件数、時間、Memoryなどの数値推移。
- Trace: Frameや処理の時系列。
- Gizmo: World内状態の視覚化。
- Crash情報: 異常終了時のCall stackと環境。

一種類だけでは原因を十分に絞れません。

## 2. Severity

```cpp
enum class LogLevel
{
    Trace,
    Debug,
    Info,
    Warning,
    Error,
    Fatal
};
```

- Trace: 非常に細かい流れ。
- Debug: 開発用状態。
- Info: 起動、Scene遷移、主要成功。
- Warning: 継続可能だが想定外。
- Error: 一機能が失敗。
- Fatal: 正常継続不能。

## 3. Category

```cpp
enum class LogCategory
{
    Application,
    Scene,
    Resource,
    Render,
    Audio,
    Collision,
    Combat,
    Input,
    Save
};
```

Category別に表示・保存・抑制できます。

## 4. Log Record

```cpp
#include <chrono>
#include <string>
#include <thread>
#include <vector>

struct LogField final
{
    std::string key{};
    std::string value{};
};

struct LogRecord final
{
    std::chrono::system_clock::time_point timestamp{};
    std::uint64_t frame = 0;
    std::thread::id threadId{};
    LogLevel level = LogLevel::Info;
    LogCategory category = LogCategory::Application;
    std::string message{};
    std::vector<LogField> fields{};
};
```

Messageへ全情報を埋め込まず、検索可能なFieldとして保持します。

## 5. Context Field

```text
event=ResourceLoadFailed
assetId=player_texture
scene=Battle
path=assets/player.png
generation=3
error=LoadGraphFailed
```

「失敗しました」だけでは原因を追えません。

## 6. Source Location

```cpp
#include <source_location>

struct LogSource final
{
    const char* file = "";
    const char* function = "";
    std::uint_least32_t line = 0;
};

[[nodiscard]] LogSource Here(
    std::source_location loc = std::source_location::current()) noexcept
{
    return {loc.file_name(), loc.function_name(), loc.line()};
}
```

Macroだけに頼らずC++20の`source_location`で呼出位置を取得できます。

## 7. Logger Interface

```cpp
class ILogger
{
public:
    virtual ~ILogger() = default;
    virtual void Write(LogRecord record, LogSource source) = 0;
    virtual void Flush() = 0;
};
```

Game CodeをFile出力APIへ直接依存させません。

## 8. Sink

- Debug output Sink。
- File Sink。
- In-game Console Sink。
- Ring Buffer Sink。
- Test Sink。

Loggerは一つのRecordを有効な複数Sinkへ配ります。

## 9. DXライブラリのLog

DXライブラリ自身のLog出力設定と独自Loggerは役割が異なります。初期化失敗や内部情報には公式Logも確認し、Game固有Contextは独自Loggerへ残します。

## 10. File Log

毎回Fileを開閉せず、起動時に開いてBufferし、必要な時点でFlushします。ただしCrash直前の重要Errorは失われないようFlush Policyを設けます。

## 11. Rotation

Log Fileが無制限に増えないようSizeまたは日付で世代管理します。保持数と合計容量を制限し、削除対象をLog Directory内へ限定します。

## 12. Encoding

UTF-8へ統一すると日本語と外部Toolの相性がよくなります。BOM有無、改行Code、Console表示の文字Codeを明示します。

## 13. Timestamp

人間向けのWall Clockと、経過時間向けのMonotonic Clockを分けます。OS時刻変更で負の経過時間が出ないよう、性能計測には`steady_clock`を使います。

## 14. Thread Safety

複数Threadから同時に書く場合、Mutex、Thread-local Buffer、Lock-free Queueなどが必要です。最初は正しいMutex版を作り、計測後に最適化します。

## 15. 非同期Logger

ProducerがRecordをQueueし、専用ThreadがFileへ書けばMain Thread停止を減らせます。一方、終了時Join、Queue満杯、Fatal時Flush、Record順序を扱う必要があります。

## 16. Queue満杯Policy

- Trace/Debugを捨てる。
- Warning以上は同期書き込み。
- 古い低Priority Recordを捨てる。
- 一時的にProducerを待たせる。

捨てた件数自体をMetricsへ残します。

## 17. Formatting Cost

無効Levelでも高Costな文字列生成を先に行うと無駄です。

```cpp
if (logger.IsEnabled(LogLevel::Debug, LogCategory::Collision))
{
    logger.Write(BuildExpensiveCollisionRecord(), Here());
}
```

遅延Formattingや`std::format`用引数保持も候補です。外部文字列をFormat文字列として扱いません。

## 18. 個人情報と秘密

Password、Token、認証Header、Saveの個人情報、完全なUser DirectoryをLogしません。外部送信や共有前にRedactionします。

## 19. 重複抑制

毎Frame同じWarningを出すと重要Logが埋もれ、I/Oも増えます。

```cpp
struct RateLimitState final
{
    std::uint64_t lastFrame = 0;
    std::uint32_t suppressed = 0;
};
```

「最初の一回」「N秒に一回」「状態変化時だけ」を使い分け、抑制数を次回出力へ含めます。

## 20. Log Once

Asset欠落など同一Keyで一度だけ出す場合、Event種別とAsset IDをKeyにします。再試行成功後は状態を解除し、再発を記録できるようにします。

## 21. Ring Buffer

直近N件をMemoryへ保持すると、画面ConsoleとCrash時の直前履歴へ使えます。

```cpp
template<class T, std::size_t Capacity>
class RingBuffer final
{
public:
    void Push(T value)
    {
        data_[write_] = std::move(value);
        write_ = (write_ + 1) % Capacity;
        count_ = std::min(count_ + 1, Capacity);
    }

private:
    std::array<T, Capacity> data_{};
    std::size_t write_ = 0;
    std::size_t count_ = 0;
};
```

## 22. In-game Console

Level、Category、文字列、Frame範囲でFilterし、Pause中でも閲覧できるOverlayにします。ConsoleがGame入力を消費している間、Character入力へ伝播させません。

## 23. Console Command

```cpp
struct ConsoleCommand final
{
    std::string name{};
    std::string help{};
    std::function<void(std::span<const std::string_view>)> execute{};
};
```

引数を検証し、Release Buildで危険Commandを無効化します。任意Shell Command実行機能を安易に組み込みません。

## 24. ErrorとLogの違い

Logは記録、Errorは処理結果です。`logger.Error()`を呼んだだけで呼出側が失敗を知れるわけではありません。戻り値、`expected`、例外などで伝播します。

## 25. 回復可能Error

- 任意Texture欠落: Placeholderで継続。
- Save読込失敗: BackupまたはDefaultを提案。
- Audio Deviceなし: 無音で継続。
- Optional Network機能失敗: Offlineで継続。

Fallbackを行った事実はWarning/Errorへ残します。

## 26. Fatal Error

DX初期化失敗、必須Data破損、内部不変条件破壊など正常継続不能な場合です。User向け説明、技術Log、可能なら安全な終了処理を分けます。

## 27. Error Context

低Levelで情報を失わず、上位へ文脈を追加します。

```text
LoadGraph failed
→ Texture player_texture load failed
→ BattleScene initialization failed
→ Scene transition Title→Battle aborted
```

同じErrorを各層で無意味に重複Logせず、最終処理地点でContext chainを一度記録します。

## 28. Error型

```cpp
struct Error final
{
    std::string code{};
    std::string message{};
    std::vector<LogField> context{};
};

template<class T>
using Result = std::expected<T, Error>; // C++23。環境に応じ代替を用意。
```

Error CodeはProgram判断、Messageは人間の説明に使います。

## 29. Exceptionの境界

Memory確保、標準Library、Parsingで例外が出る可能性があります。Main Loop最外周、Job境界、Plugin境界などで捕捉し、例外をGame Frame間へ放置しません。

## 30. Destructorで投げない

Stack unwinding中にDestructorが例外を投げると`std::terminate`になり得ます。破棄失敗は記録し、Destructor外の明示`Close`で結果を返す設計を検討します。

## 31. Assert

```cpp
#include <cassert>

assert(handle != -1 && "Graph handle must be valid");
```

AssertはProgrammerの契約違反を早期発見するものです。外部File欠落など通常起こり得るError処理の代わりにしません。

## 32. Debug AssertとRelease

ReleaseでAssert式が評価されない場合があります。副作用をAssert内へ書きません。

```cpp
const int result = InitializeSystem();
assert(result == 0);
// assert(InitializeSystem() == 0); は避ける。
```

## 33. Ensure

「契約違反を記録するが継続可能」なEnsureを用意できます。戻り値で分岐し、同一箇所の連続Spamを抑制します。

## 34. Breakpoint

Debugger接続中だけErrorでBreakする機能は原因地点を捕まえやすくします。User環境で無条件にBreak命令を実行しません。

## 35. Crash情報

例外Code、Call Stack、Thread、Build ID、Scene、Frame、直近Log、設定概要を保存します。Memory破壊時は複雑な通常Logger自体が安全に動かない可能性があります。

## 36. Build ID

Git Commit、Build日時、Compiler、Architecture、Debug/Releaseを起動時Logへ残します。「どのBinaryか」を特定できなければ再現が難しくなります。

## 37. Reproduction Context

乱数Seed、Stage ID、Character ID、入力記録、固定Delta設定を残すと再現性が上がります。ただし大容量DataとPrivacyを考慮します。

## 38. Debug Draw API

```cpp
class DebugDraw final
{
public:
    void LineWorld(Vec2 from, Vec2 to, Rgb8 color,
                   float lifetimeSeconds = 0.0F);
    void BoxWorld(Aabb box, Rgb8 color,
                  float lifetimeSeconds = 0.0F);
    void CircleWorld(Circle circle, Rgb8 color,
                     float lifetimeSeconds = 0.0F);
    void TextScreen(Vec2 position, std::string text, Rgb8 color);
};
```

World用とScreen用を関数名で分けます。

## 39. Gizmo Command

```cpp
enum class GizmoDepthMode { Always, WorldDepth };

struct GizmoLine final
{
    Vec2 from{};
    Vec2 to{};
    Rgb8 color{};
    float remainingSeconds = 0.0F;
    GizmoDepthMode depth = GizmoDepthMode::Always;
};
```

2DではLayer順、3DではDepth Test有無として発展します。

## 40. Lifetime

0秒を1Frame、正値を指定秒、負値を永続など契約化します。Game TimeとReal Timeのどちらで減らすかをPause要件で選びます。

## 41. Category別Gizmo

Collision、AI、Camera、Combat、NavigationをBit flagで切り替えます。全表示で画面が読めなくなるのを防ぎます。

## 42. Collision Gizmo

- Hurtbox: 緑。
- Hitbox: 赤。
- Pushbox: 青。
- Trigger: 黄。
- Contact: 白い点。
- Normal: 接触点から線。

色だけに依存せず線種やLabelも使い、色覚差へ配慮します。

## 43. Camera Gizmo

Camera中心、Dead Zone、Look-ahead、Clamp領域、Targetを表示します。Camera揺れはSimulation CameraとRender Cameraを別色にすると原因を追えます。

## 44. Combat Timeline

現在State、経過時間、入力Buffer、Cancel Window、無敵、Active Hitboxを横軸時間で描画すると、戦闘の「1Frameずれ」を見つけやすくなります。

## 45. AI Gizmo

現在State、Target、視界、検知距離、選択Action、Utility Score、Pathを表示します。最終結果だけでなく候補Scoreを見せます。

## 46. GizmoとGame State

Gizmoは状態を読むだけにし、描画ON/OFFでSimulation結果を変えません。Gizmo用乱数がGameplay乱数を消費してはいけません。

## 47. Command上限

無限にGizmoを登録するとMemoryと描画Costが増えます。最大Command数を設け、超過時は低Priorityを捨てて件数をWarningします。

## 48. Profiler Scope

```cpp
class ProfileScope final
{
public:
    ProfileScope(const char* name)
        : name_(name), begin_(std::chrono::steady_clock::now()) {}

    ~ProfileScope()
    {
        const auto end = std::chrono::steady_clock::now();
        RecordProfileSample(name_, end - begin_);
    }

private:
    const char* name_;
    std::chrono::steady_clock::time_point begin_;
};
```

RAIIで全return経路の時間を記録します。

## 49. CPU Frame内訳

Input、Fixed Update、Collision、AI、Render Submit、Screen Flip、Audio UpdateへMarkerを置きます。平均だけでなく最大、Percentile、Spike Frameを見ます。

## 50. Frame Hitch記録

閾値を超えたFrameで、各Scope時間、Load、Allocation、Gizmo数、Collider候補数をSnapshot保存します。Hitch後に通常値へ戻っても原因を失いません。

## 51. Metrics

```cpp
struct FrameMetrics final
{
    double cpuMilliseconds = 0.0;
    std::uint32_t drawCommands = 0;
    std::uint32_t collisionPairs = 0;
    std::uint32_t activeVoices = 0;
    std::size_t resourceBytes = 0;
};
```

時系列Graphで傾向を見ます。

## 52. Debug機能のBuild制御

高CostなGizmo生成やConsoleは開発Buildへ限定できます。一方、最低限のError LogやCrash情報はReleaseにも必要です。全部を一つの`#ifdef`で消さず機能別に制御します。

## 53. Debug Codeの副作用

無効BuildでもMacro引数が評価される設計に注意します。Debug機能ON/OFFでObject寿命、乱数、Thread timingが大きく変わらないようにします。

## 54. User向けError表示

技術詳細をそのまま表示せず、「Data読込に失敗しました。再試行または終了してください」のように行動を示します。詳細CodeとLog保存先は併記できます。

## 55. Recovery Flow

```text
Error検出
→ Context付与
→ Log
→ Resource/Sceneを有効状態へRollback
→ Retry / Fallback / Error Scene / Quit
```

半端な状態で継続しません。

## 56. Test Logger

```cpp
class RecordingLogger final : public ILogger
{
public:
    void Write(LogRecord record, LogSource) override
    {
        records.push_back(std::move(record));
    }
    void Flush() override {}

    std::vector<LogRecord> records{};
};
```

失敗時に正しいLevel、Category、Asset IDが一度だけ記録されたかTestできます。

## 57. Fault Injection

Load失敗、Save失敗、Audioなし、Queue満杯、Disk容量不足、Gizmo上限、Worker timeoutを意図的に発生させます。Error経路が正常系より危険になっていないか確認します。

## 58. よくある不具合

- Logが巨大: 毎FrameWarningとRotation不足。
- 原因不明: ID、Scene、Frame、Path等のContext不足。
- ReleaseだけCrash: Assert内に副作用を書いた。
- 終了時欠落: 非同期LoggerをFlush/Joinしていない。
- Debug表示で挙動変化: Simulation状態を書き換えた。
- Error後に二次Crash: 部分初期化状態で継続した。
- 秘密漏洩: Tokenや個人PathをLogした。

## 59. 設計チェックリスト

- [ ] LevelとCategoryを定義した。
- [ ] Timestamp、Frame、Thread、Sourceを記録する。
- [ ] Error Contextを構造化Fieldにした。
- [ ] 無効LogのFormatting Costを避けた。
- [ ] 同一WarningをRate Limitした。
- [ ] Log Fileに容量上限とRotationがある。
- [ ] 秘密情報をRedactする。
- [ ] Errorを戻り値で伝播する。
- [ ] Assertを通常Error処理に使わない。
- [ ] GizmoはGame Stateを書き換えない。
- [ ] Profilerは`steady_clock`を使う。
- [ ] 終了時にLoggerをFlush・Joinする。

## 60. 理解確認問題

1. LogとError伝播の違いは何か。
2. Wall Clockを処理時間計測へ使わない理由は何か。
3. 無効Debug LogにもCostが生じる例は何か。
4. 同じWarningを毎Frame出す問題は何か。
5. Assert内へ副作用を書いてはいけない理由は何か。
6. Errorを各Layerで重複Logしない方法は何か。
7. Gizmo LifetimeにGame TimeとReal Timeの選択が必要な理由は何か。
8. Crash時にBuild IDが必要な理由は何か。
9. 非同期Logger終了時に必要な処理は何か。
10. Fault Injectionで何を検証できるか。

## 61. 実践課題

1. Level・Category・Field付きLoggerを作る。
2. File、Ring Buffer、Recording Sinkを作る。
3. Rate LimitとLog Onceを実装する。
4. Filter可能な画面Consoleを作る。
5. `Result<T>`へError Contextを追加する。
6. Collider、Camera、Combat Timeline Gizmoを作る。
7. RAII Profile ScopeとFrame Graphを作る。
8. Hitch時Snapshotを保存する。
9. Fault InjectionでError Sceneまで確認する。
10. Release Logから秘密情報が除去されるTestを書く。

## 62. 公式資料

- [DXライブラリ 関数リファレンス](https://dxlib.xsrv.jp/dxfunc.html)
- [DXライブラリ その他関数](https://dxlib.xsrv.jp/function/dxfunc_other.html)

DXライブラリ自身のLog出力、Error戻り値、非同期状態、Platform別Debug出力は利用中バージョンの公式資料を確認してください。

## 63. 次章への接続

2D機能と観測基盤がそろいました。次章から3Dへ進み、Vector、内積・外積、座標系、Matrix、Quaternionを、Gizmoと数値Testで確認しながら学びます。
