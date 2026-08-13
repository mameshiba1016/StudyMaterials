# ゲームループ

ゲームループは、終了までイベント、更新、描画を繰り返す中心制御です。OSイベントを処理しないと、ウィンドウは応答不能になります。

## 基本形

```cpp
int main()
{
    Game game{};

    if (!game.Initialize())
    {
        return 1;
    }

    while (game.IsRunning())
    {
        // ウィンドウ終了、入力、リサイズ等をOSキューから取得する。
        game.ProcessEvents();

        // 前フレームからの経過時間を測る。
        const double deltaSeconds{game.MeasureFrameDeltaSeconds()};

        // 入力と時間を使ってゲーム状態を次へ進める。
        game.Update(deltaSeconds);

        // 現在状態をバックバッファへ描く。
        game.Render();

        // 完成したバックバッファを画面へ提示する。
        game.Present();
    }

    game.Shutdown();
    return 0;
}
```

実際のAPIでは初期化失敗をRAIIで管理し、明示`Shutdown`が不要な設計もあります。

## ダブルバッファリング

表示中の画面へ直接少しずつ描くと、途中状態が見えてちらつきます。通常は非表示のバックバッファへ一フレームを描き、完成後にフロントへ提示します。

```text
Front Buffer：現在モニターが表示
Back Buffer ：次フレームを描画中
Present     ：完成した画像を表示側へ渡す
```

Presentの待機方式はVSync、トリプルバッファ、可変リフレッシュレート、ドライバー設定により変わります。

## イベント駆動とポーリング

- OSイベント：キー押下、文字入力、ウィンドウ終了などキューから取得。
- デバイス状態：現在キーが押されているか毎フレーム取得。
- ゲームアクション：Jump、Attack、Pauseなど意味へ変換。

文字入力とゲーム操作キーを同じ処理にしません。IME、キーリピート、キーボード配列が関係するためです。

## 更新順序は仕様

例：

```text
Input → Player → Enemy AI → Movement → Collision → Damage → Death → Camera
```

敵が死んだフレームにも攻撃するか、カメラが衝突解決前後どちらの位置を追うかが変わります。順序を偶然の`vector`並びに任せず、フェーズとして定義します。

## 固定更新と描画の分離

```cpp
while (running)
{
    const double frameDelta{MeasureAndClampDelta()};
    accumulator += frameDelta;

    ProcessEvents();
    input.BeginFrame();

    int stepCount{0};
    while (accumulator >= fixedStep && stepCount < maxStepsPerFrame)
    {
        UpdateFixed(fixedStep);
        accumulator -= fixedStep;
        ++stepCount;
    }

    const double alpha{accumulator / fixedStep};
    Render(alpha);
    Present();
}
```

入力のエッジ（押した瞬間）を複数固定更新で二重消費しない設計が必要です。入力をtickへ割当てる、最初のtickだけ消費する、タイムスタンプ付きイベントを使うなどがあります。

## 何も更新しない状態

最小化、非フォーカス、ポーズ時に通常更新を続けるかを決めます。

- 最小化中は描画停止・低頻度更新でCPU/GPUを節約。
- オンラインゲームはゲーム世界を止められない場合がある。
- フォーカス喪失時に押下状態をクリアしないと、キーが押しっぱなしになることがある。
- 復帰フレームの巨大deltaを破棄・クランプする。

## ループを止める処理

終了要求を受けた瞬間に深い場所からプロセスを強制終了すると、セーブ・ログ・リソース解放が行われません。`RequestQuit`で状態を設定し、安全なループ境界で終了します。致命的クラッシュ時には通常終了が不可能な場合もあるため、クラッシュダンプを別系統で用意します。

## CPUを使い切らない

VSyncもフレーム制限もないループは、可能な限り高速に回りCPU/GPUを占有します。目標FPS、VSync、スリープを構成可能にし、計測しながら待機します。`sleep_for(16ms)`だけでは処理時間を加算してしまい、正確な60 FPSになりません。

## 非同期処理との境界

ワーカースレッドでアセットを読んでも、RendererやWorldへの反映はスレッド安全なキューを経由し、フレーム境界で行います。Sceneが遷移済みなら完了結果を破棄できるよう、世代番号やキャンセルトークンを持たせます。
