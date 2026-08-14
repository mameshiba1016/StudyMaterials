# DXライブラリ：Architecture・Test・完成確認表

この章はDXライブラリ編の総仕上げです。35章で扱った機能を、変更しやすく、調査しやすく、再現可能な一つのGame Architectureへ整理します。完成とは「一度動いた」ことではなく、責務、寿命、失敗経路、検証方法を説明できる状態です。

## 1. Architectureの目的

- 変更の影響範囲を狭くする。
- 所有者と寿命を明確にする。
- 更新順序を再現可能にする。
- DXライブラリ依存を境界へ集める。
- 戦闘計算を画面なしでもTestできるようにする。
- 不具合発生Tickと原因を追跡できるようにする。

ArchitectureはClassを増やすこと自体が目的ではありません。

## 2. 推奨Layer

```text
Application
 -> Scene
 -> Gameplay / Domain
 -> Platform Abstraction
 -> DX Library
```

GameplayからDXライブラリを直接呼ぶ箇所を限定すると、TestとDirectXへの移行が容易になります。

## 3. 依存方向

上位Policyは下位の具体的APIを直接知らず、必要なInterfaceまたは値型へ依存します。RendererがCombatを操作したり、AudioがHPを変更したりしません。

## 4. Module例

```text
Application
Scene
Time
Input
World
Character
Combat
AI
Camera
Rendering
Audio
Effects
UI
Resources
Debug
Tests
```

Module名はProject規模に合わせます。最初から細かく分けすぎません。

## 5. Folder例

```text
src/
  application/
  platform/dxlib/
  core/
  gameplay/
    character/
    combat/
    ai/
  presentation/
    camera/
    rendering/
    audio/
    effects/
    ui/
  scenes/
tests/
assets/
config/
```

Folderは依存関係を表し、単にFile種類で分けません。

## 6. Header境界

公開Headerには利用者が必要な宣言だけを置きます。DXライブラリHandleや巨大な内部Classを不用意に公開すると、変更時に全体が再Compileされます。

## 7. Forward Declaration

PointerまたはReferenceだけを宣言に使う場合はForward Declarationで依存を減らせます。ただし値Memberは完全型が必要です。

## 8. Platform Adapter

```cpp
struct IAudioOutput
{
    virtual ~IAudioOutput() = default;
    virtual void Play(std::uint16_t soundId,
                      float volume,
                      float pan) = 0;
};
```

Gameplayは`PlaySoundMem`を直接呼ばず、Audio要求を出します。

## 9. Interfaceを作る基準

すべてを抽象化しません。外部API境界、Testで置換したい時間・乱数・入出力、複数実装が本当にある責務に絞ります。

## 10. Composition Root

```cpp
class GameApplication final
{
public:
    bool Initialize();
    int Run();
    void Shutdown();

private:
    // 実際には依存順に所有Objectを並べます。
    std::unique_ptr<class ResourceManager> resources_{};
    std::unique_ptr<class SceneManager> scenes_{};
};
```

Object生成と依存接続をApplication初期化へ集めます。

## 11. 所有権表

| 対象 | 主な所有者 | 参照方法 |
|---|---|---|
| Scene | SceneManager | Scene ID |
| Entity Runtime | World | Entity ID + Generation |
| Texture・Model・Sound | Resource Manager | Asset ID / RAII Handle |
| Effect Instance | Effect Pool | Effect ID + Generation |
| Event | Event Queue | 値 |
| Attack Definition | Immutable Database | Definition ID |

所有者不明の生ポインタを長期間保存しません。

## 12. RAII

```cpp
class ModelHandle final
{
public:
    explicit ModelHandle(const TCHAR* path)
        : value_(MV1LoadModel(path)) {}

    ~ModelHandle()
    {
        if (value_ >= 0)
            MV1DeleteModel(value_);
    }

    ModelHandle(const ModelHandle&) = delete;
    ModelHandle& operator=(const ModelHandle&) = delete;
    int Get() const { return value_; }

private:
    int value_{-1};
};
```

初期化成功と破棄を一つの型へまとめます。

## 13. 初期化のTransaction

途中でAsset読み込みに失敗した場合、それまでに作ったResourceをRAIIで戻します。半分だけ初期化されたSceneをRunしません。

## 14. Shutdown順

参照する側を先に、参照される側を後に破棄します。SceneとEntityを消してからResource Managerを破棄し、最後にDXライブラリを終了します。

## 15. Service Locatorの注意

どこからでもGlobal Serviceへ触れる設計は依存を隠します。限定的なContextまたはConstructor引数で必要なServiceを明示します。

## 16. DataとRuntime

```cpp
struct AttackDefinition final
{
    int damage{};
    std::uint32_t activeBeginTick{};
    std::uint32_t activeEndTick{};
};

struct AttackRuntime final
{
    std::uint32_t elapsedTicks{};
    std::uint32_t generation{};
};
```

共有可能な不変Definitionと、個体ごとの可変Runtimeを分けます。

## 17. IDとGeneration

Entity、Effect、Control、Attack、Phase、Path RequestにGenerationを使い、Pool再利用後の古い処理を拒否します。

## 18. Deferred Command

```cpp
enum class WorldCommandType
{
    Spawn,
    Destroy,
    ChangeScene,
    AddComponent,
    RemoveComponent
};

struct WorldCommand final
{
    WorldCommandType type{};
    std::uint32_t targetIndex{};
    std::uint32_t targetGeneration{};
};
```

配列走査中に構造を変更せず、更新境界で適用します。

## 19. EventとCommand

- Command：実行してほしい要求。
- Event：すでに確定した事実。

`DealDamageCommand`は拒否され得ますが、`DamageAppliedEvent`は確定済みです。

## 20. Event Queueの寿命

Eventが同一Tick、次Tick、Frame末のどこまで有効かを型またはQueue名で表します。購読中に同じBufferを書き換えません。

## 21. 固定更新Pipeline

```text
Commands
 -> AI / decisions
 -> state machines
 -> animation pose
 -> movement
 -> collision candidates
 -> defense / damage commit
 -> gameplay events
 -> deferred structural changes
```

順序を一つのOrchestratorへ記述します。

## 22. Presentation Pipeline

```text
Gameplay events
 -> VFX / audio / UI requests
 -> budgets and culling
 -> camera interpolation and shake
 -> render passes
```

Presentationの失敗がGameplayをRollbackしません。

## 23. Time Domain

Fixed Game Time、Scaled Entity Time、Real Time、Render Timeを混ぜません。Timer型またはContextで利用する時間を明示します。

## 24. Random Stream

```cpp
enum class RandomStreamId
{
    Combat,
    EnemyDecision,
    Loot,
    Presentation
};
```

演出の火花乱数を増やしてもCombat会心結果が変わらないようStreamを分けます。

## 25. Error方針

- 必須Asset失敗：Scene開始を中止する。
- 任意VFX失敗：Fallbackまたは省略する。
- Data不正：読み込み時に詳細を報告する。
- Runtime不変条件違反：DebugではAssert、Releaseでは安全に隔離する。

失敗を無条件で無視しません。

## 26. Result型

```cpp
enum class LoadError
{
    FileNotFound,
    InvalidFormat,
    UnsupportedVersion,
    DxLibraryFailure
};

struct LoadResult final
{
    int handle{-1};
    std::optional<LoadError> error{};
    explicit operator bool() const { return handle >= 0 && !error; }
};
```

失敗理由を呼び出し側へ返します。

## 27. Log Level

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

Category、Tick、Entity ID、Thread、Source位置を含めると検索しやすくなります。

## 28. Assertと入力検証

Assertは開発者が守る不変条件、入力検証は外部Dataやユーザー入力へ使います。ReleaseでAssertが消えても安全性が必要な条件は通常処理でも検証します。

## 29. Test Pyramid

```text
多数：純粋関数のUnit Test
中数：複数SystemのIntegration Test
少数：実際のDXライブラリを使うRuntime Test
```

すべてを画面操作だけで確認しません。

## 30. Pure Function Test

Damage式、範囲判定、Camera補間、Target Score、Cooldown、Window通過など、副作用なしの関数を最優先でTestします。

```cpp
void TestHealthRatio()
{
    assert(HealthRatio(50, 100) == 0.5f);
    assert(HealthRatio(1, 0) == 0.0f);
}
```

浮動小数点は必要に応じて許容誤差で比較します。

## 31. Fake Time

```cpp
class FakeClock final
{
public:
    void AdvanceTicks(std::uint64_t count) { tick_ += count; }
    std::uint64_t Tick() const { return tick_; }

private:
    std::uint64_t tick_{};
};
```

実時間Sleepを使わずCooldownやBufferを高速に検証します。

## 32. Fixed Random

乱数器を注入し、会心、AI選択、Pattern選択を再現します。Test失敗時にはSeedとStream状態を出力します。

## 33. Fake Renderer

描画命令を記録するFakeへ置換し、「HP Barが一度要求された」「無効Handleを描画しない」を画面なしで検証できます。

## 34. Integration Test World

最小Stage、Player一体、Enemy一体をMemory上に作り、Commandを数Tick流してHP、Action、Eventを確認します。

## 35. Golden Replay

固定Command列を再生し、重要TickのState Hashを期待値と比較します。意図的な仕様変更時だけ期待値を更新します。

## 36. State Hash

Entityを安定順に並べ、ID、量子化位置、HP、Action、Random StateをHash化します。Pointer値や未初期化Paddingを含めません。

## 37. Property Testの考え方

多数の値を生成し、「HPは0～最大」「正規化VectorにNaNがない」「Budgetが負にならない」など常に守る性質を検証します。

## 38. Boundary Test

- 0件、1件、最大件数、最大+1件。
- Tick境界の直前、同値、直後。
- HP0、HP1、最大HP。
- 距離0、範囲境界、非常に遠い。
- Delta 0、通常、非常に大きい。
- 無効ID、古いGeneration、有効ID。

## 39. Failure Injection

Model、Texture、Soundの読み込み失敗、Pool満杯、Path失敗、Event上限を意図的に発生させ、CleanupとFallbackを確認します。

## 40. Soak Test

自動戦闘を長時間実行し、Memory、Handle、Pool使用数、Event Queue容量、Generation、平均・最大Frame時間を監視します。

## 41. Performance Budget

```cpp
struct FrameBudget final
{
    double simulationMilliseconds{4.0};
    double renderingMilliseconds{10.0};
    double presentationMilliseconds{1.0};
};
```

数値は対象環境と目標fpsから決め、推測で固定しません。

## 42. Profiler Counter

- Active Entity数。
- AI知覚Ray数。
- Path Query数。
- Hit BoxとCollision候補数。
- 確定Hit数。
- Draw CallとTriangle数。
- Active VoiceとEffect数。
- Resource LoadとCache Hit率。

時間と件数を同時に記録します。

## 43. Memory Tracking

Scene開始前後、戦闘開始後、再戦後、Scene終了後のAllocationとDX Handle数を比較します。増加し続ける値を見逃しません。

## 44. Compile設定

Warning Levelを高くし、可能ならWarningをErrorとして扱います。Debug、Development、ReleaseでOptimizationと診断機能の違いを明文化します。

## 45. Sanitizerと静的解析

利用可能なCompiler機能でAddress Sanitizer、Undefined Behavior検査、静的解析を使います。DXライブラリ境界の誤検出は理由を記録して局所抑制します。

## 46. Coding Rule

- 所有権を型で表す。
- 単位を名前へ含める。
- 無効値より`optional`を使う。
- Enumを整数Flag代わりに乱用しない。
- `const`で変更しない意図を表す。
- 巨大関数を責務単位に分ける。

## 47. Data Validation

Attack Windowが総Tick内か、Phase閾値が順序通りか、Asset IDが存在するか、Animation Markerが重複していないかを起動時に検証します。

## 48. Version付きSave

```cpp
struct SaveHeader final
{
    std::uint32_t magic{};
    std::uint32_t version{};
    std::uint32_t payloadSize{};
    std::uint32_t checksum{};
};
```

構造体をそのままBinary書き込みせず、Fieldごとに形式を定義します。

## 49. Debug Menu

- Scene選択。
- Enemy生成。
- 無敵、HP、Resource変更。
- AI停止とState指定。
- Boss Phase・部位操作。
- Hit Box、Path、Camera Collision表示。
- Time ScaleとFixed Step制御。
- ProfilerとEvent Log表示。

通常経路を通すDebug Commandを使います。

## 50. Crash前の情報

直近のLog、Replay Command、State Hash、Scene、Tick、Seed、Resource一覧を保存できると再現率が上がります。個人情報や巨大Dataを不用意に記録しません。

## 51. DXライブラリ依存一覧

- WindowとMessage Loop。
- Input Polling。
- Model・Animation Handle。
- Texture・Render Target。
- Sound Handle。
- Collision Query。
- Shaderと描画State。

これらをAdapter境界として把握するとDirectX編で置換対象が見えます。

## 52. DirectXへ持ち越せる知識

- Game Loopと固定更新。
- Resource CacheとRAII。
- Scene・Entity・Event設計。
- Camera・行列・座標系。
- CollisionとCombat Pipeline。
- Shader ParameterとRender Pass。
- Profiler、Test、Debug Draw。

APIは変わっても設計原理は残ります。

## 53. DirectXで自作する部分

Device、Swap Chain、Command、Buffer、Texture Upload、Pipeline State、Descriptor、Synchronizationなど、DXライブラリが隠していたGPU・Windows処理を段階的に学びます。

## 54. 完成確認：Application

- [ ] 初期化失敗時に途中Resourceを解放する。
- [ ] Message Loopと終了処理が一箇所にある。
- [ ] Fixed Stepと最大追従回数を設定する。
- [ ] Scene切替中に古いEventを拒否する。
- [ ] Shutdown順が所有関係と一致する。

## 55. 完成確認：Resource

- [ ] HandleをRAIIで解放する。
- [ ] 同じAssetを重複Loadしない。
- [ ] 必須と任意Assetの失敗方針がある。
- [ ] Scene終了後に不要Resourceを解放できる。
- [ ] 文字コードとPath規則が統一されている。

## 56. 完成確認：Input・Time

- [ ] 現在、前回、押した瞬間、離した瞬間を区別する。
- [ ] Action Mappingで物理Buttonを隠す。
- [ ] Input Bufferを固定Tickで管理する。
- [ ] Game TimeとReal Timeを分ける。
- [ ] Hit Stop中にも解除処理が進む。

## 57. 完成確認：Character・Camera

- [ ] Character Controllerが壁、坂、段差、接地を処理する。
- [ ] Root MotionをCollision経由で適用する。
- [ ] Lock-on対象をIDとGenerationで管理する。
- [ ] Camera Collisionで壁内へ入らない。
- [ ] Shakeが基本Camera姿勢を破壊しない。

## 58. 完成確認：Combat

- [ ] Action、Window、CancelをData化する。
- [ ] Pose更新後にHit Boxを同期する。
- [ ] 重複HitとMulti Hitを区別する。
- [ ] Dodge、Guard、Parryの優先順が固定されている。
- [ ] Damage、Reaction、Deathを一度だけCommitする。
- [ ] GameplayとPresentation Eventを分ける。

## 59. 完成確認：AI・複数敵

- [ ] 知覚、記憶、判断、移動を分離する。
- [ ] AIが座標やHPを直接変更しない。
- [ ] Path失敗とStuckから回復できる。
- [ ] Combat Directorが攻撃Tokenを管理する。
- [ ] Slot、Role、Pressureで戦闘密度を制御する。
- [ ] 画面外攻撃に統一規則がある。

## 60. 完成確認：Boss・交代

- [ ] Phase移行を一度だけ確定する。
- [ ] 部位Damageと本体Damageを二重計算しない。
- [ ] Pattern中断時に全所有権を返す。
- [ ] Character交代がTransactionになっている。
- [ ] 安全な登場位置を検査する。
- [ ] Support退場後もProjectileが安全に残る。

## 61. 完成確認：Presentation

- [ ] VFX、Audio、UI、Cameraが確定Eventから動く。
- [ ] Pool満杯でもGameplay結果が変わらない。
- [ ] Blend、Depth、Shader状態をPassごとに復元する。
- [ ] VoiceとEffectの同時数を制限する。
- [ ] Shake、Flashの軽減設定がある。

## 62. 完成確認：品質

- [ ] Debug Buildで重大Warningがない。
- [ ] Invalid Handleと古いGenerationを拒否する。
- [ ] Unit、Integration、Runtime Testがある。
- [ ] 固定ReplayでState Hashが一致する。
- [ ] 長時間実行でMemoryとHandleが増え続けない。
- [ ] 目標環境でCPU・GPU Budget内に収まる。

## 63. 学習確認の質問

- なぜ描画補間座標をHit判定へ使わないのか。
- なぜCollision Callback内でHPを減らさないのか。
- EventとCommandはどう違うか。
- IDだけでなくGenerationが必要なのはなぜか。
- Hit Stop解除TimerはどのTime Domainで動くか。
- Resource所有者と参照者は誰か。
- DirectXへ移ると何を自分で管理する必要があるか。

答えをCodeと実行結果で説明できれば、知識がつながっています。

## 64. 実装例へ進む順序

1. Window、Game Loop、固定更新。
2. Input、2D描画、Resource RAII。
3. 3D Model、Camera、Character Controller。
4. 一つの攻撃と一つの敵。
5. Combo、回避、Guard、Damage、VFX。
6. AI、複数敵、Boss。
7. Character交代とSupport。
8. Replay、Profiler、総合Test。

一度に全機能を作らず、動く縦切りを積み上げます。

## 65. Definition of Done

一機能について、次を満たしたときに完了とします。

- 正常系が動く。
- 境界値と失敗系が定義されている。
- 所有者とCleanup経路が明確である。
- Debug表示またはLogで内部状態を確認できる。
- 自動Testまたは再現手順がある。
- Profilerで負荷を確認している。
- 関連資料とData形式が更新されている。

## 66. この章の要点

- Architectureは変更、調査、再現を容易にするためにあります。
- DXライブラリ依存をPlatformとPresentation境界へ集めます。
- DefinitionとRuntime、CommandとEventを分離します。
- RAII、ID＋Generation、Deferred Commandで寿命を安全にします。
- Pure Function Test、Integration Test、Replayを組み合わせます。
- 失敗注入、長時間Test、Profilerで動作以外の品質も確認します。
- 完成確認表を満たしてから、対応する実行例とDirectX内部実装へ進みます。

これでDXライブラリ編の基礎ノートは完了です。次はDirectX 11編で、DXライブラリが内部で担当していたWindow、Device、Swap Chain、Resource、Shader、描画命令を自分で構築します。
