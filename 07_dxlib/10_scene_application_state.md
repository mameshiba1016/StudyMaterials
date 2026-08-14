# 第10章 Scene・Application State

SceneはTitle、Menu、Battleなど、ひとまとまりの状態と処理を所有する単位です。本章ではScene遷移を安全な時点で行い、Resource、Collision、Audioを正しい順番で生成・破棄するApplication構造を作ります。

## 1. Applicationの責任

```text
DXライブラリ初期化
→ 共通System生成
→ Main Loop
→ Scene更新・描画
→ 共通System破棄
→ DxLib_End
```

ApplicationはProcess全体の寿命を所有し、Scene固有の戦闘Ruleまでは持ちません。

## 2. Sceneの例

- Boot: 設定と最小Assetを読む。
- Title: 開始・設定・終了を選ぶ。
- Loading: 次Sceneを準備する。
- Battle: World、Character、Combatを動かす。
- Result: 結果を表示する。
- Pause: 下のBattleへ重ねるOverlay。

Scene数より、責任と遷移条件が明確かが重要です。

## 3. Scene Interface

```cpp
class IScene
{
public:
    virtual ~IScene() = default;

    virtual bool Initialize() = 0;
    virtual void OnEnter() {}
    virtual void HandleInput(const InputSnapshot& input) = 0;
    virtual void FixedUpdate(float fixedDelta) = 0;
    virtual void Update(float delta) = 0;
    virtual void Draw(const RenderContext& context) const = 0;
    virtual void OnExit() {}
};
```

Virtual Destructorがなければ基底Pointer経由の破棄が不完全になります。

## 4. ConstructorとInitialize

Constructorは不変条件を作り、失敗し得るAsset Loadは`Initialize`で行う設計例です。失敗を`bool`だけでなくError情報として返す設計も有効です。

## 5. Scene ID

```cpp
enum class SceneId
{
    Title,
    Loading,
    Battle,
    Result
};
```

Scene間でConcrete classを直接生成し合わず、Scene IDと遷移DataをManagerへ要求します。

## 6. 遷移要求

```cpp
struct SceneTransition final
{
    enum class Kind { None, Replace, Push, Pop, Quit };
    Kind kind = Kind::None;
    SceneId destination = SceneId::Title;
};
```

Sceneが自身を更新中に即破棄しないため、要求だけを記録します。

## 7. 安全な遷移時点

```text
Input
→ Fixed Update群
→ Variable Update
→ 遷移要求を適用
→ Draw
```

またはFrame末尾に適用します。どこで切り替わるかを一つに固定します。

## 8. Scene Manager

```cpp
#include <memory>
#include <vector>

class SceneManager final
{
public:
    void Request(SceneTransition transition)
    {
        // 同Frameに複数要求が来る場合の優先Ruleが必要。
        pending_ = transition;
    }

    void ApplyPending();
    void Update(float delta);
    void Draw(const RenderContext& context) const;

private:
    std::vector<std::unique_ptr<IScene>> stack_{};
    SceneTransition pending_{};
};
```

## 9. Replace

現在Sceneを終了・破棄し、新Sceneへ交換します。

```text
旧OnExit
→ 旧Scene破棄
→ 新Scene生成
→ 新Initialize
→ 新OnEnter
```

新Scene初期化が失敗した場合のFallbackを先に決めます。

## 10. Transactional Replace

旧Sceneを先に消さず、新Sceneを一時生成・初期化し、成功後に交換します。

```cpp
auto candidate = factory.Create(destination);
if (!candidate || !candidate->Initialize())
{
    // 現Sceneを維持し、Error Sceneまたは通知へ。
    return;
}

stack_.back()->OnExit();
stack_.back() = std::move(candidate);
stack_.back()->OnEnter();
```

## 11. Stack

Pauseや確認Dialogは`Push`し、閉じると`Pop`します。下Sceneを破棄せず復帰できます。

```text
Battle
└─ Pause Overlay
   └─ Settings Overlay
```

## 12. Update伝播

最上段だけ更新するか、下Sceneも更新するかをScene属性にします。Pause中はBattle更新を止め、背景Animationだけ進めるなどのPolicyがあります。

```cpp
struct ScenePolicy final
{
    bool blocksInputBelow = true;
    bool blocksUpdateBelow = true;
    bool blocksDrawBelow = false;
};
```

## 13. Draw伝播

不透明Sceneは下を描く必要がありません。透明Overlayは下から順に描きます。Stack上から下へ調べ、最初の不透明Sceneから上を描画します。

## 14. Input伝播

Pause Buttonを押したFrameにBattleとPauseの両方が同じ入力を処理すると即解除することがあります。入力を消費するか、Scene適用時点をFrame末尾にします。

## 15. Scene Factory

```cpp
class SceneFactory final
{
public:
    SceneFactory(GameServices& services) : services_(services) {}

    [[nodiscard]] std::unique_ptr<IScene> Create(SceneId id);

private:
    GameServices& services_;
};
```

生成責任を一か所へ集め、Scene同士のInclude依存を減らします。

## 16. Services

```cpp
struct GameServices final
{
    InputManager& input;
    TextureCache& textures;
    AudioSystem& audio;
    Renderer& renderer;
    Logger& logger;
};
```

Sceneが必要なSystemを明示的に借用します。所有者はApplicationです。

## 17. Service Locatorの注意

どこからでもGlobal取得できる仕組みは便利ですが、依存が隠れ、Testと破棄順が難しくなります。Constructor Injectionなどで依存を見える形にします。

## 18. 所有権の木

```text
Application
├─ Window/DX lifetime
├─ Renderer
├─ Resource Cache
├─ Audio System
├─ Input Manager
└─ Scene Manager
   └─ Scene
      ├─ Collision World
      ├─ Entities
      └─ Scene-local Resources
```

親が子より長生きし、破棄は逆順です。

## 19. 生成順と破棄順

RendererやAudioがDX APIを使うなら`DxLib_Init`後に生成し、`DxLib_End`前に破棄します。SceneがCache資源を借りるならSceneをCacheより先に破棄します。

## 20. Application RAII

```cpp
int RunApplication()
{
    if (DxLib_Init() < 0) return -1;

    int result = 0;
    {
        Application app;
        if (!app.Initialize()) result = -1;
        else result = app.Run();
    } // 全SystemとHandleをここで破棄。

    DxLib_End();
    return result;
}
```

## 21. Main Loop

```cpp
int Application::Run()
{
    while (!quitRequested_)
    {
        if (ProcessMessage() < 0) break;

        const FrameTime time = clock_.Tick();
        const InputSnapshot input = input_.Capture();
        scenes_.HandleInput(input);

        accumulator_ += time.delta;
        while (accumulator_ >= fixedDelta_)
        {
            scenes_.FixedUpdate(fixedDelta_);
            accumulator_ -= fixedDelta_;
        }

        scenes_.Update(time.delta);
        scenes_.ApplyPending();
        renderer_.BeginFrame();
        scenes_.Draw(renderer_.Context());
        renderer_.EndFrame();
    }
    return 0;
}
```

## 22. Quit要求

Sceneから`exit()`を直接呼ばず、ApplicationへQuit要求を送ります。確認Dialog、Save完了待ち、正常なResource解放を通せます。

## 23. Window Close

OSの閉じる操作もQuit要求へ統合します。ただし強制終了やOS Shutdownでは十分な時間がない場合があるため、重要Dataは平常時にも安全に保存します。

## 24. Loading Scene

Loadingは進捗表示と次Sceneの準備を担当します。同期Loadだけなら画面が更新されないため、分割Load・非同期Load・事前Loadを使います。

## 25. Loading状態

```cpp
enum class LoadState { Preparing, Loading, Finalizing, Ready, Failed };

struct LoadProgress final
{
    std::size_t completed = 0;
    std::size_t total = 0;
    [[nodiscard]] float Ratio() const noexcept
    {
        return total == 0 ? 1.0F
                          : static_cast<float>(completed) / total;
    }
};
```

File Sizeと処理時間は比例しないため、進捗率は概算であることがあります。

## 26. 非同期結果の寿命

Loading中に戻る操作が行われても、Workerが破棄済みSceneへ結果を書かないようCancel Token、共有Job状態、Main Thread適用Queueを使います。

## 27. Thread境界

File読込・DecodeをWorkerで行えても、DXライブラリHandle生成を任意Threadで呼べるとは限りません。公式Thread制約に従い、GPU資源生成はMain Threadへ戻します。

## 28. Scene-localとGlobal Resource

- Global: Font、共通UI、共通SE。
- Scene-local: Stage、敵Model、専用BGM。
- Shared Cache: 複数Sceneで参照され、最後の利用者後に解放。

全Resourceを永続化すると遷移は速い一方、Memoryが増えます。

## 29. Scene Data

遷移時に渡す値を型にします。

```cpp
struct BattleRequest final
{
    int stageId = 0;
    int difficulty = 0;
};

struct BattleResult final
{
    bool cleared = false;
    int score = 0;
};
```

Global変数経由で渡さず、FactoryやTransition Payloadを使います。

## 30. Payload

`std::variant`で許可された遷移Dataだけを表現できます。

```cpp
using ScenePayload = std::variant<std::monostate,
                                  BattleRequest,
                                  BattleResult>;
```

送信先とPayload型の不一致を遷移適用時に検証します。

## 31. Scene内部State

Battle Scene内にもIntro、Playing、Victory、Defeatがあります。全てを別Sceneにせず、Resource寿命や描画世界が同じなら内部State Machineにする方が自然です。

## 32. SceneかStateか

- Resource集合と画面構成が大きく変わる: Scene候補。
- 同じ世界でCharacter挙動だけ変わる: 内部State候補。
- 下画面を残して一時的に重ねる: Overlay候補。

## 33. Pauseは時間Policy

Pause SceneをPushするだけでなく、Game Time、Audio、Particle、UI Animationのどれを止めるか定義します。実時間とGame時間を分けます。

## 34. Transition Effect

Fade Out中に旧Sceneを描き、暗転時に交換し、Fade Inで新Sceneを見せます。

```text
Idle → FadingOut → Swap → FadingIn → Idle
```

Fade ControllerがScene交換時点を所有すると一貫します。

## 35. Screenshot Transition

旧SceneをRender TargetへCaptureし、その画像をFadeさせながら新SceneをLoadできます。Capture Handleの寿命とWindow Resizeを管理します。

## 36. Error Scene

必須Asset失敗、Save破損、初期化失敗を表示し、Titleへ戻る・再試行・終了を選べるSceneを用意します。Error文字列に秘密情報や個人Pathを出さないようにします。

## 37. Restart

Battle再開始はProcess全体を再起動せず、旧Battle Sceneを完全破棄して新しく生成します。前回のEntity、Collision、Audio、乱数状態が漏れていないか確認します。

## 38. Resetと再生成

巨大な`Reset()`で全Fieldを初期化し直すより、Scene Objectを破棄・再生成する方が初期状態を保証しやすい場合があります。高Cost ResourceはCacheで共有します。

## 39. Event Busの寿命

Scene ObjectがGlobal Event Busへ購読したまま破棄されるとDangling callbackになります。Subscription TokenをRAIIで解除するか、Scene Scopeごと破棄します。

## 40. 遷移競合

同Frameに「死亡でResult」「Pause」「Window Close」が来る場合の優先度を決めます。

```text
Quit > Fatal Error > Replace > Push/Pop > None
```

最初を採用、最後を採用、優先度で採用のどれかを明文化します。

## 41. 再入防止

遷移適用中に`OnExit`や`OnEnter`から別遷移を即適用すると再帰的にStackが壊れます。新要求は次の安全点までQueueします。

## 42. Frame中のScene参照

Sceneへの生Pointerを長期間保存せず、Manager所有の`unique_ptr`をその場で借用します。遷移後に古いPointerを使わない契約が必要です。

## 43. Saveとの連携

Scene遷移前にSaveする場合、途中失敗で遷移を止めるか警告して続けるかを決めます。File保存は一時Fileへ書き、成功後に交換する方式が安全です。

## 44. Test用Fake Scene

```cpp
struct SceneLog final
{
    std::vector<std::string> calls{};
};

// Fake Sceneで Initialize→OnEnter→Update→OnExit→Destructor の順を記録する。
```

描画せずScene Managerの寿命と遷移をTestできます。

## 45. Test項目

- Replace成功・失敗。
- 空StackでPop。
- OverlayのUpdate/Draw伝播。
- 同Frame複数要求の優先度。
- Initialize失敗時に旧Scene維持。
- Quit時の破棄順。
- Loading Cancel後の遅い完了通知。
- Restart後に旧Event購読が残らない。

## 46. Debug表示

Scene Stack、Pending Transition、各SceneのUpdate/Draw有無、Scene経過時間、Load進捗、Scene-local Resource数を表示します。

## 47. よくある不具合

- 遷移直後Crash: 更新中に自身を即破棄した。
- Pauseを即解除: 同じ押下を両Sceneが処理した。
- Memory増加: 旧Scene参照やEvent購読が残る。
- 黒画面: 新Scene初期化失敗後に旧Sceneを先に消した。
- 終了時Crash: DX終了後にHandle Destructorが走った。
- Loading後Crash: Cancel済みSceneへWorkerが書いた。

## 48. 設計チェックリスト

- [ ] ApplicationとSceneの所有責任が分かれている。
- [ ] 遷移は要求として安全点で適用する。
- [ ] 新Scene成功後に旧Sceneを交換する。
- [ ] OverlayのInput/Update/Draw伝播を定義した。
- [ ] Sceneより共通Systemが長生きする。
- [ ] 全Handleを`DxLib_End`前に破棄する。
- [ ] Loading CancelとThread境界を扱う。
- [ ] 同Frameの遷移競合Policyがある。
- [ ] Event購読をScene破棄時に解除する。
- [ ] Fake Sceneで遷移順序をTestする。

## 49. 実践課題

1. `IScene`、Factory、Managerを作る。
2. Title→Loading→Battle→ResultをReplaceする。
3. PauseとSettingsをStackで重ねる。
4. Transactional Replace失敗を再現する。
5. Fade Out→交換→Fade Inを実装する。
6. Cancel可能なLoading状態を作る。
7. Scene Stackと破棄順をDebug表示する。
8. Fake Sceneで全遷移Testを書く。

## 50. 次章への接続

SceneとApplicationの寿命が整理できると、Resource Cacheをどこが所有し、Scene終了時に何を解放するかが明確になります。次章ではResource Cache・RAIIを画像・音声・Modelへ一般化し、型安全なHandle、参照、Load失敗、Hot Reloadを統合します。
