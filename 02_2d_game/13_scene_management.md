# シーン管理・遷移・ロード

シーンはタイトル、ゲーム、リザルトなど大きな実行状態を分けます。Sceneの切替中に現在Sceneを即破棄すると、呼出スタック上の`this`が無効になる危険があります。

## Sceneインターフェース

```cpp
class IScene
{
public:
    virtual ~IScene() = default;

    virtual void OnEnter() = 0;
    virtual void HandleInput(const InputFrame& input) = 0;
    virtual void Update(double deltaSeconds) = 0;
    virtual void Render(Renderer& renderer) const = 0;
    virtual void OnExit() = 0;
};
```

SceneManagerが`std::unique_ptr<IScene>`を所有できます。

## 遷移要求を遅延適用

```cpp
void SceneManager::RequestReplace(SceneId next)
{
    pendingTransition_ = ReplaceRequest{next};
}

void SceneManager::ApplyPendingTransition()
{
    if (!pendingTransition_)
    {
        return;
    }

    current_->OnExit();
    current_ = factory_.Create(pendingTransition_->sceneId);
    current_->OnEnter();
    pendingTransition_.reset();
}
```

Update終了後など安全な地点で反映します。複数要求が同tickに出た際の優先順位も決めます。

## Scene Stack

Pauseや確認Dialogは現在Sceneを破棄せず上へ積めます。

```text
PauseMenu       ← 入力・UI描画
GameplayScene   ← 更新停止、背面描画のみ
```

各Sceneに`blocksUpdateBelow`、`blocksRenderBelow`等を持たせる方法があります。Overlayが下の入力を消費する規則も必要です。

## Transition状態

暗転はSceneそのものより、遷移Controllerが管理できます。

```text
FadeOut current
    ↓ 完全に暗い安全点
Unload current / Load next
    ↓
FadeIn next
```

ロードに時間がかかる場合、Loading Sceneを表示し、ワーカーでCPU側データを読みます。GPUリソース生成はRendererの許可スレッドへ戻します。

## Asset寿命

- Global：フォント、共通UI、システム音。
- Scene：ステージ背景、敵画像、BGM。
- Entity：一時VFX等。

Scene Asset Bundleを持ち、Scene終了時に参照を解放します。Asset Cacheが保持する場合は予算とEviction規則が必要です。

## 非同期ロードの競合

Stage A読込中にユーザーがTitleへ戻った後、Aの完了Callbackが現在SceneへEntityを追加してはいけません。

```cpp
struct LoadTicket
{
    SceneGeneration generation{};
    std::stop_token cancellation{};
};
```

完了反映時に世代を比較し、古い結果を捨てます。キャンセル要求は即停止を保証しないため、結果検査が必要です。

## Persistent State

Sceneを跨ぐ設定、セーブ進行、プレイヤー編成をScene自身へ置くと破棄で失われます。Application/GameSession/Profileなど寿命の異なる所有者へ分けます。

```text
Application lifetime：設定、Renderer、Audio
Session lifetime    ：現在のRun、スコア、編成
Scene lifetime      ：ステージEntity、背景
Frame lifetime      ：描画コマンド、一時イベント
```

## Factory

Scene IDから生成します。巨大な`switch`でも小規模なら明瞭です。文字列Reflectionを無理に自作せず、依存オブジェクトをコンストラクタへ渡します。

## 失敗処理

次Sceneの必須Assetロードが失敗した場合、半分破棄された状態にしません。新Sceneを準備し成功後に交換する、Error Sceneへ移る、再試行を提示するなどTransactionalに扱います。

## テスト

- 全Sceneを起動できる。
- 往復遷移でメモリが増え続けない。
- 遷移連打・戻る操作。
- ロード失敗・キャンセル。
- Pause StackのUpdate/Render順。
- フォーカス喪失中の遷移。
