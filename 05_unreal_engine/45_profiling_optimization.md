# 45 Unreal Insightsと最適化

## 1. 計測して最大要因から直す

```text
目標Frame Time設定 → Capture → Bottleneck分類
→ 原因を絞る → 変更 → 同条件で再計測
```

60fpsのFrame Budgetは約16.67ms、120fpsは約8.33msです。Game Thread、Render Thread、GPUの最長経路がFrameを制限します。

## 2. 基本表示

- `stat unit`：Game／Draw／GPU等。
- `stat game`：Game Thread Group。
- `stat gpu`／GPU Profile：描画Cost。
- `stat memory`：Memory概要。
- Unreal Insights：CPU Timing、Task、Load、Memory、Network等のTrace。

Editorの追加負荷を避け、Development packaged buildとTarget hardwareでも測ります。

## 3. 独自Trace Scope

```cpp
TRACE_CPUPROFILER_EVENT_SCOPE(Combat_TargetSelection);
```

意味のあるScope名を付け、候補収集、Score、Trace、Sortを分けます。巨大な`CombatTick`だけでは原因を絞れません。

## 4. CPU

- 不要TickをEvent／Timer化。
- Cast／Actor検索／Allocationを毎Frame行わない。
- AI、EQS、Targetingを時間分散。
- Animation Update Rate／LOD。
- 非同期LoadでGame Thread Hitchを避ける。
- Lock競合とTask粒度をInsightsで確認。

## 5. GPU

- Draw Call、Triangle、Material Slot。
- Shadow caster数とLight範囲。
- Translucency／Niagara Overdraw。
- Screen Resolution、Lumen、Post Process。
- Skeletal Mesh Bone／Skinning Cost。
- Nanite／Virtual Shadow Mapの対象と設定。

CPU最適化をしてもGPU Boundならfpsは変わりません。

## 6. MemoryとStreaming

Size Map、Asset Audit、MemReport、Render Resource ViewerでTexture、Mesh、Animation、Audioを確認します。Soft Reference、Bundle、Texture／Mesh LOD、Streaming Poolを設計します。

## 7. Hitch

平均fpsだけでなくFrame Time分布を見る必要があります。同期Asset Load、Shader／PSO準備、GC、大量Spawn、初回Niagara、SaveがHitch原因になり得ます。

## 8. Scalability

低／中／高設定でEffect数、Shadow、View Distance、Post Process、Resolutionを変更します。Gameplay判定や敵予告は品質設定で消さないでください。

## 9. 回帰防止

同じ戦闘Scenarioの自動Capture、Frame Budget、Memory上限、Load時間を継続計測します。最適化前後のCaptureとBuild番号を保存します。

## 参考

- [Unreal Insights](https://dev.epicgames.com/documentation/unreal-engine/unreal-insights-in-unreal-engine)
- [Real-Time Rendering Optimization](https://dev.epicgames.com/documentation/en-us/unreal-engine/optimizing-and-debugging-projects-for-realtime-rendering-in-unreal-engine)
