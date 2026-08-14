# DirectX 11：GPU Debug・PIX・Profiler

この章では、描画の誤りと性能問題を証拠から特定する方法を学びます。Debug Layer、Info Queue、Object Name、GPU Query、PIX Capture、CPU/GPU Timeline、Bottleneck分析、継続的な計測までを扱います。

## 1. DebugとProfileの違い

```text
Debug   : 結果が間違う原因を探す
Profile : 時間・Memory・回数のBottleneckを測る
```

正しくない描画を高速化しても意味がありません。まずDebug LayerのError/Warningをなくし、その後Profileします。

## 2. 観測できる層

```text
Application Logic
CPU Render Preparation
Direct3D API Submission
Driver / Runtime
GPU Pipeline
Display / Present
```

どの層の問題かを切り分けます。

## 3. 再現条件を固定する

Camera、Scene、Resolution、VSync、Render Scale、Quality、Random Seed、Frame番号を固定します。条件が変わる計測値同士を比較しません。

## 4. Debug Buildだけでは足りない

C++ DebuggerはCPU MemoryやControl Flowには強い一方、GPU Resource Binding、Shader入力、Draw Event、GPU時間はGraphics DebuggerやQueryで確認します。

## 5. D3D11 Debug Layer

Device作成Flagへ`D3D11_CREATE_DEVICE_DEBUG`を追加します。

```cpp
UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;

#if defined(_DEBUG)
flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

HRESULT hr = D3D11CreateDevice(
    adapter,
    driverType,
    nullptr,
    flags,
    featureLevels,
    featureLevelCount,
    D3D11_SDK_VERSION,
    &device,
    &createdFeatureLevel,
    &context);
```

## 6. Debug Layerの役割

- 無効なAPI引数
- Resource Binding Hazard
- View/Formatの不一致
- Map/Usageの誤り
- 未設定State
- Resource Leakの手掛かり
- 非効率な呼出しの一部

GPU上の全Logic Errorを自動発見するものではありません。

## 7. Debug Layerが利用できない場合

Debug Componentが未導入ならDebug Flag付きDevice作成が失敗する場合があります。開発環境へGraphics Toolsを導入し、製品版ではDebug Flagを外します。

## 8. HRESULTを残す

```cpp
void ThrowIfFailed(HRESULT hr, std::string_view operation)
{
    if (FAILED(hr))
    {
        LogHRESULT(hr, operation);
        throw GraphicsException(hr, operation);
    }
}
```

失敗したOperation、File、Line、Frame、Resource名も記録します。

## 9. ID3D11InfoQueue

```cpp
ComPtr<ID3D11InfoQueue> infoQueue;
if (SUCCEEDED(device.As(&infoQueue)))
{
    infoQueue->SetBreakOnSeverity(
        D3D11_MESSAGE_SEVERITY_CORRUPTION, TRUE);
    infoQueue->SetBreakOnSeverity(
        D3D11_MESSAGE_SEVERITY_ERROR, TRUE);
}
```

重大Message発生時にDebugger Breakできます。

## 10. WarningでBreakするか

初期開発ではWarning Breakも有効ですが、既知のNoiseで作業不能にしません。原因を確認した上で必要最小限のFilterを使います。

## 11. Message Category

Info Queue MessageにはCategory、Severity、ID、Descriptionがあります。Description文字列だけでなくIDをLogへ保存すると集計しやすくなります。

## 12. Storage Filter

特定SeverityやMessage IDの保存をFilterできます。ただし理解せず全Warningを隠すと本物の不具合を見逃します。

## 13. Info Queueの読み出し

```cpp
const UINT64 count = infoQueue->GetNumStoredMessagesAllowedByRetrievalFilter();

for (UINT64 i = 0; i < count; ++i)
{
    SIZE_T size = 0;
    infoQueue->GetMessage(i, nullptr, &size);

    std::vector<std::byte> storage(size);
    auto* message = reinterpret_cast<D3D11_MESSAGE*>(storage.data());
    infoQueue->GetMessage(i, message, &size);
    LogD3D11Message(*message);
}

infoQueue->ClearStoredMessages();
```

Frameごとの新規MessageをTestで検出できます。

## 14. Debug Messageを0件へ

「画面が動いているから無視」で済ませません。Warningが本当に無害なら理由とFilter IDを記録します。

## 15. Object Name

```cpp
void SetDebugName(ID3D11DeviceChild* object, std::string_view name)
{
    if (!object || name.empty()) return;

    object->SetPrivateData(
        WKPDID_D3DDebugObjectName,
        static_cast<UINT>(name.size()),
        name.data());
}
```

CaptureとLive Object Reportで`Texture2D #57`ではなく用途名を表示できます。

## 16. 命名規則

```text
RT.SceneColor.1920x1080
DS.MainDepth.1920x1080
Tex.Character.Body.Albedo
CB.Camera.Frame2
Shader.PS.PBR_Opaque
```

種類、用途、Owner、Size/Variantを含めます。

## 17. User-defined Annotation

```cpp
ComPtr<ID3DUserDefinedAnnotation> annotation;
immediateContext.As(&annotation);

annotation->BeginEvent(L"Main Opaque Pass");
DrawOpaque();
annotation->EndEvent();
```

Graphics Capture上でEvent階層を作れます。

## 18. Marker

```cpp
annotation->SetMarker(L"Boss Phase Changed");
```

重要な瞬間をTimelineへ残します。毎Objectへ過剰な長文Markerを付けるとCaptureが読みにくくなります。

## 19. Event階層

```text
Frame 1842
 ├─ Shadow
 │   ├─ Cascade 0
 │   └─ Cascade 1
 ├─ Main
 │   ├─ Depth Prepass
 │   ├─ Opaque
 │   └─ Transparent
 ├─ Effects
 ├─ Post Process
 └─ UI
```

Renderer Architectureと同じ階層にします。

## 20. Live Object Report

Debug InterfaceからDevice終了時に生存Objectを報告できます。意図したCacheまでLeakと決めつけず、Shutdown順序を揃えてから調べます。

```cpp
ComPtr<ID3D11Debug> debug;
if (SUCCEEDED(device.As(&debug)))
{
    debug->ReportLiveDeviceObjects(D3D11_RLDO_DETAIL);
}
```

## 21. Context State確認

描画されない場合は、Render Target、Depth、Viewport、Scissor、Input Layout、Vertex/Index Buffer、Shader、Constant、Texture、Sampler、Blend/Rasterizer/Depth Stateを順に確認します。

## 22. Draw Call単位の検査

Graphics Captureで問題のDrawを選び、次を確認します。

- 入力頂点とIndex
- Primitive Topology
- VS入力/出力
- Constant Buffer値
- Texture/Sampler
- Rasterizer結果
- Depth/Stencil Test
- Blend結果
- Render Target出力

## 23. Pixel History

対応Toolが提供するPixel Historyでは、特定Pixelへ影響したDraw、Depth失敗、Blend、上書き順を追跡できます。透明・Shadow・UIの重なり調査に有効です。

## 24. Shader Debug

特定Vertex/Pixel/ThreadのShaderを追跡できる場合があります。最適化、Symbol、動的分岐、Tool Supportにより制限があるため、入力値と中間Target可視化も併用します。

## 25. Render Target可視化

Scene Color、Normal、Roughness、Depth、Motion Vector、Shadow Map等を画面へ直接表示できるDebug Viewを作ります。

## 26. False Color

```text
赤   : Overdraw大
緑   : 正常範囲
青   : 低LOD
紫   : NaN/Inf
黄   : Bounds/Culling境界
```

数値を色へ変換するとScene全体の異常分布を発見しやすくなります。

## 27. NaN検出

```hlsl
bool invalid = any(isnan(value)) || any(isinf(value));
if (invalid)
    return float4(1, 0, 1, 1);
```

本番Shaderへ常時残すのではなくDebug Variantとして使います。

## 28. Wireframe

Topology、LOD、過剰Tessellation、Model破綻を確認できます。見た目だけでTriangle Costを断定せずCounterと併用します。

## 29. Bounds表示

Sphere/AABB/OBB、Frustum、Light Volume、Trigger、Hitboxを表示し、CullingやCollisionと描画位置の不一致を調べます。

## 30. PIXとは

PIX on WindowsはDirectX ApplicationのGPU Capture、Timing Capture、Resource/Shader/Pipeline調査に使うMicrosoftのPerformance Debuggerです。対応OS、GPU、Driver、APIの最新要件は公式資料で確認します。

## 31. PIX Capture前の準備

- 再現Sceneを固定する。
- Debug NameとEventを付ける。
- Symbolを用意する。
- ResolutionとGraphics設定を記録する。
- Overlayや外部Capture Toolの競合を避ける。
- 代表Frameを選ぶ。

## 32. GPU Capture

一つまたは少数FrameのCommand、Resource、Pipeline State、Shader、Event時間を詳細に調べます。見た目の不具合や一Frame内のGPU Cost分析に向きます。

## 33. Timing Capture

複数FrameにわたるCPU Thread、GPU Queue、Present、Event等の時間関係を調べます。Frame Spike、CPU/GPU待ち、Workload推移の分析に向きます。

## 34. Captureは実行を変える

Capture/Debug Layer/Marker自体にOverheadがあります。絶対値だけを信用せず、通常実行のTelemetryやRelease相当Buildでも確認します。

## 35. Capture Frameの選択

平均Frameだけでなく、攻撃Hit、多数Effect生成、Camera Cut、敵出現、Streaming、UI遷移等のSpike FrameもCaptureします。

## 36. Event Listの読み方

上から描画順を追い、PassごとのDraw/Dispatch数、State変更、Render Target遷移、Clear、Copyを確認します。想定外の重複Passを探します。

## 37. Pipeline State

問題Drawを選び、各StageのShader、Resource Slot、Input Layout、Stateを確認します。Application Codeの想定ではなく実際に記録された値を根拠にします。

## 38. Resource Inspector

Mip、Array Slice、Format、Size、内容、Historyを確認します。SRGB/Linear、Typeless View、Mip Level、Cube Faceの取り違えを調べます。

## 39. Mesh Viewer

入力頂点、VS後座標、Normal、UV、Index順を確認します。行列転置、Stride、Input Semantic、Bone Weightの問題を切り分けます。

## 40. Shader Table

Shaderごとの実行CostやInvocation傾向から高負荷Shaderを見つけます。ただしPixel数、Overdraw、Texture待ち等も含めて解釈します。

## 41. Bottleneckの四分類

```text
CPU bound       : GPUへ仕事を供給するCPUが遅い
GPU bound       : GPU処理時間がFrameを決める
Present limited : VSync/Frame Cap/Display待ち
Mixed/Streaming : IO、Page Fault、Upload、同期等が変動
```

## 42. CPU Boundの兆候

GPU Timelineに空白があり、Render ThreadまたはJobが長い状態です。Draw Submission、Culling、Sort、Animation、Lock、Allocationを調べます。

## 43. GPU Boundの兆候

GPUが連続して稼働し、CPUが先行できている一方、GPU Frame時間がBudgetを超えています。Pass別GPU時間とCounterを見ます。

## 44. Present Limited

60 Hz VSyncならFrameが約16.67 ms単位に見えることがあります。VSync、Frame Limiter、Waitable Object、Tearing設定を記録して解釈します。

## 45. Frame Budget

```text
30 FPS  : 約33.33 ms
60 FPS  : 約16.67 ms
120 FPS : 約 8.33 ms
144 FPS : 約 6.94 ms
```

CPUとGPUを同じ16.67 msへ単純に足すのではなく、PipelineとしてOverlapすることを理解します。

## 46. 平均値だけを見ない

Average FPSは短いStutterを隠します。Frame TimeのMedian、P95、P99、Maximum、分布を記録します。

## 47. GPU Timestamp Query

GPU上のPass時間をDirect3D 11 Queryで測れます。Timestamp単体だけでなくTimestamp Disjoint Queryと組み合わせます。

## 48. Query作成

```cpp
D3D11_QUERY_DESC timestampDesc{};
timestampDesc.Query = D3D11_QUERY_TIMESTAMP;

D3D11_QUERY_DESC disjointDesc{};
disjointDesc.Query = D3D11_QUERY_TIMESTAMP_DISJOINT;

device->CreateQuery(&timestampDesc, &startQuery);
device->CreateQuery(&timestampDesc, &endQuery);
device->CreateQuery(&disjointDesc, &disjointQuery);
```

HRESULTを省略せず確認します。

## 49. Timestamp記録

```cpp
context->Begin(disjointQuery.Get());
context->End(startQuery.Get());

DrawMeasuredPass();

context->End(endQuery.Get());
context->End(disjointQuery.Get());
```

Timestampは`End`でGPU Command Streamへ記録します。

## 50. Query結果取得

```cpp
D3D11_QUERY_DATA_TIMESTAMP_DISJOINT disjoint{};
UINT64 start = 0;
UINT64 end = 0;

// 数Frame後にGetDataし、S_OKになった結果だけ使う。
```

直後に結果を待つとCPU/GPUを同期させてしまいます。

## 51. GPU時間計算

```cpp
const double seconds =
    static_cast<double>(end - start) /
    static_cast<double>(disjoint.Frequency);

const double milliseconds = seconds * 1000.0;
```

`disjoint.Disjoint == TRUE`なら周波数が途中で不連続なため、その測定を無効とします。

## 52. Query Ring

FrameごとにQuery SetをRingで持ち、数Frame前の結果を非Blockingで取得します。

```text
Frame N   : Query Set 0へ記録
Frame N+1 : Query Set 1へ記録
Frame N+2 : Set 0の結果を試しに取得
```

## 53. DONOTFLUSH

`GetData`へ`D3D11_ASYNC_GETDATA_DONOTFLUSH`を指定すると、結果取得のための暗黙Flushを避けられます。未完了なら待たず次Frameへ回します。

## 54. Pipeline Statistics Query

`D3D11_QUERY_PIPELINE_STATISTICS`でInput Assembler頂点数、Primitive数、VS/PS/GS/HS/DS/CS Invocation等を取得できます。

## 55. Pipeline Statisticsの読み方

Pixel Shader Invocationが画面Pixel数より極端に多いならOverdrawや大量のShadow/Transparent描画を疑えます。ただしMSAA、Early-Z、複数Pass等を考慮します。

## 56. Occlusion Query

通過Sample数を測れますが、同Frameで結果を待つとStallします。可視性判定だけでなくDebug Counterとしても使えます。

## 57. CPU Profiler Scope

```cpp
class CpuProfileScope
{
public:
    explicit CpuProfileScope(ProfileId id);
    ~CpuProfileScope();
};

#define PROFILE_CPU_SCOPE(id) CpuProfileScope scope_##__LINE__(id)
```

RAIIでEarly ReturnやException時もScopeを閉じます。

## 58. CPU Clock

`std::chrono::steady_clock`等の単調Clockを使います。計測呼出し自体のCostとThread間Clock特性も理解します。

## 59. CPU Marker階層

Frame、Simulation、Animation、Culling、Sort、Record、Submit、Presentに分けます。細かすぎるScopeはProfiler Data量を増やします。

## 60. Thread Timeline

Main、Render、Worker、IO Threadの実行・待機を並べ、Lock待ち、Job不足、Load Imbalance、優先度逆転を探します。

## 61. Draw Call Counter

Frame/PassごとにDraw、Dispatch、Triangle、Instance、Material変更、Shader変更、Texture Binding回数を集計します。

## 62. Resource Counter

Texture/Buffer数、推定Byte、Transient Peak、Upload量、Readback量、Map回数を記録します。VRAM値だけではLifetime Peakを説明できません。

## 63. Shader Variant Counter

使用Variant数、Compile数、Pipeline組合せ、Frame内切替数を記録します。Variant ExplosionはBuild、Memory、Runtime Cacheへ影響します。

## 64. Overdrawの調査

OpaqueはFront-to-back、Depth Prepass、Culling、LODを検討します。TransparentはBlend要件のためOverdraw削減が特に重要です。

## 65. Fill-rate Test

Render Resolutionを下げてGPU時間が大きく改善するならPixel/Fill/Texture Bandwidth側を疑います。変わらなければVertex、Compute、固定Cost等を調べます。

## 66. Vertex負荷Test

Model LODやShadow Casterを減らし、GPU時間の変化を測ります。Triangle数だけでなくVertex ShaderのSkinning/属性数も考慮します。

## 67. Shader負荷Test

問題PassのPixel Shaderを単純色出力へ一時置換し、時間差を測ります。Resource BindingやGeometry条件を変えずShader Costを隔離します。

## 68. Bandwidth負荷

大きなRender Target、過剰なG-Buffer、High Precision Format、多数のFull-screen Pass、UAV Read/WriteはBandwidthを消費します。

## 69. State Change負荷

Material/Pipeline順にSortし、Binding回数を比較します。Driver側で重いStateと単なるPointer設定を同一視せず計測します。

## 70. Upload Stall

Dynamic Buffer Map、UpdateSubresource、Texture UploadがGPU使用中Resourceと競合すると待機や内部Copyが生じます。Frame Resource RingとUpload量を確認します。

## 71. Readback Stall

Query、Screenshot、GPU Culling結果等を直後にMap/GetDataするとPipelineが止まります。数Frame遅延またはGPU内消費へ変更します。

## 72. Shader Compilation Stutter

Gameplay中の初回Shader CompileやVariant生成は大きなSpikeになります。事前Compile、Cache、Warm-up、非同期Pipelineを検討します。

## 73. Resource Creation Stutter

Texture/Buffer/Viewを戦闘中に大量生成せず、Pool、Preload、Async Streamingを使います。Create時間を専用Counterへ記録します。

## 74. Lock Contention

Renderer Cache、Allocator、Job Queue、Asset ManagerのMutex待ちをThread Timelineで確認します。Lock回数だけでなく待機時間を測ります。

## 75. Fast Action Sceneの計測地点

- Character AnimationとSkinning
- Enemy群のCulling/LOD
- Shadow Caster描画
- Effect Particle Update/Draw
- Trail、Decal、Distortion
- Post ProcessとUpscale
- UI/HUD
- Asset Streaming

## 76. Attack Impact Spike

Hit FrameにParticle Spawn、Light、Decal、Camera Effect、Audio Eventが集中します。平均戦闘Frameとは別Scenarioとして計測します。

## 77. Camera Cut Spike

Cameraが急移動するとOcclusion履歴、LOD、Streaming、Shadow更新が一度に変わります。Camera PathをReplayして再現します。

## 78. Performance HUD

開発中の画面にFPSだけでなくCPU Frame、GPU Frame、P95、Draw、Triangle、Visible数、Effect数、Upload量を表示します。

## 79. Telemetry Ring Buffer

直近数秒のFrame MetricをMemoryへ保持し、Spike検出時に前後FrameをFileへ保存します。問題発生後でも直前の状況を確認できます。

## 80. Capture Trigger

```text
GPU frame > 20 msが3回
CPU frame > 20 ms
Device Removed
Shader compile発生
Manual hotkey
```

自動診断Data保存のTriggerを設計します。

## 81. 比較実験

一度に一要因だけ変えます。Before/Afterで同じSceneを複数回測り、Warm-up Frameを除外します。

## 82. Statistical Noise

OS Scheduling、Background Process、Clock変動、Temperature、Driver Cache等で結果が揺れます。複数Sampleと分布を使います。

## 83. Release相当Build

最終性能は最適化Build、現実的なLogging、Debug Layer無効で測ります。ただしDebug Buildの検査も別に継続します。

## 84. GPU Warm-up

Shader/Resource Cache、GPU Clock、Streamingが安定するまでWarm-upし、起動直後と定常状態を別Metricとして扱います。

## 85. 実機構成

High-end一台だけでなく、対象となる複数GPU/CPU/VRAM/Resolution/Driverで測ります。BottleneckはHardwareごとに変わります。

## 86. Regression Budget

Pass、Scenario、PlatformごとにBudgetを定め、継続計測で悪化を検出します。小さな悪化の積み重ねをRelease直前まで放置しません。

## 87. Screenshot差分

最適化前後で画像差分を取り、描画品質が意図せず変わっていないか確認します。Temporal Effectは固定Time/Seedで比較します。

## 88. よくある失敗：FPSだけを見る

FPSは逆数で変化し、Spike原因やCPU/GPU分類を示しません。Frame TimeとPass内訳を見ます。

## 89. よくある失敗：CPU TimerでGPUを測る

Draw API呼出し時間はCommand発行時間でありGPU実行時間ではありません。GPU TimestampまたはCaptureを使います。

## 90. よくある失敗：Query結果を待つ

`while (GetData(...) != S_OK) {}`はCPU/GPU Stallを作ります。Query Ringで後から非Blocking取得します。

## 91. よくある失敗：Debug Warningを隠す

大量WarningをFilterで消す前に根本原因を直します。無害と判断したMessage IDだけ理由付きで管理します。

## 92. よくある失敗：Capture一枚で断定

一つのFrame、一回の測定、一台のPCだけでは変動や別Bottleneckを見逃します。ScenarioとSampleを増やします。

## 93. よくある失敗：複数最適化を同時投入

何が効いたか、何が画質を壊したか分からなくなります。一変更ごとにCapture、Metric、画像を比較します。

## 94. Debug手順

1. 再現条件と問題Frameを固定する。
2. HRESULTとDebug Layer Messageを0件へ近づける。
3. Object NameとEvent Markerを確認する。
4. Graphics Captureで問題Drawを特定する。
5. Input、Shader、Resource、State、Outputを順に見る。
6. 中間Target/Bounds/NaNを可視化する。
7. 最小変更で修正し画像差分を確認する。

## 95. Performance手順

1. 目標FPSとFrame Budgetを定める。
2. 固定Scenarioを複数回測る。
3. CPU/GPU/Present Limitedを分類する。
4. Pass別TimestampとCPU Scopeを取る。
5. CounterとCaptureから原因仮説を作る。
6. 一要因だけ変更する。
7. Release相当Buildで再計測する。
8. 別HardwareとSpike Scenarioを確認する。
9. Regression Metricへ追加する。

## 96. Debug Checklist

- [ ] Debug Layerを開発Buildで有効にできる。
- [ ] Info Queue ErrorでBreakできる。
- [ ] HRESULTへOperation/File/Lineを記録する。
- [ ] 主要ResourceにDebug Nameがある。
- [ ] Frame/Pass/ChunkのEvent階層がある。
- [ ] 中間Render Targetを可視化できる。
- [ ] Bounds、LOD、Overdraw、NaN表示がある。
- [ ] Shutdown時にLive Objectを確認できる。

## 97. Profiler Checklist

- [ ] CPUとGPU Frame Timeを別々に測る。
- [ ] GPU TimestampをDisjoint Queryと使う。
- [ ] Query結果を数Frame遅延して取得する。
- [ ] P95/P99/Maximumを記録する。
- [ ] Draw、Triangle、Dispatch、Upload Counterがある。
- [ ] CPU Thread Timelineを確認できる。
- [ ] 固定Replay/Camera Pathがある。
- [ ] Before/Afterの画像とMetricを保存する。
- [ ] 複数HardwareでRegressionを確認する。

## 98. 理解確認問題

1. Debug LayerとGraphics Captureの役割の違いを説明してください。
2. Object NameとEvent Markerが診断を速める理由を説明してください。
3. CPU Draw Call時間がGPU時間ではない理由を説明してください。
4. Timestamp Disjoint Queryが必要な理由を説明してください。
5. Query RingがStallを防ぐ仕組みを説明してください。
6. Resolution低下Testから何を推測できるか説明してください。
7. Average FPSだけではStutterを検出できない理由を説明してください。
8. 一度に一変更だけ行う理由を説明してください。

## 99. 章末要点

- 正しさをDebug Layer、Info Queue、Captureで確認してから性能を測ります。
- Resource名とPass Eventを付け、CaptureをRenderer構造と対応させます。
- CPU/GPU/Present LimitedをTimelineとTimestampから分類します。
- GPU QueryはRing化し、同Frameで結果を待ちません。
- Pass時間だけでなくInvocation、Draw、Upload、Memory等のCounterを合わせて読みます。
- 固定Scenario、分布、Release相当Build、複数Hardwareで比較します。
- 一変更ずつ測定し、画像差分とRegression Dataを残します。

## 100. 公式資料

- [Direct3D 11 Debug Layer](https://learn.microsoft.com/en-us/windows/win32/direct3d11/using-the-debug-layer-to-test-apps)
- [ID3D11InfoQueue](https://learn.microsoft.com/en-us/windows/win32/api/d3d11sdklayers/nn-d3d11sdklayers-id3d11infoqueue)
- [ID3D11Debug::ReportLiveDeviceObjects](https://learn.microsoft.com/en-us/windows/win32/api/d3d11sdklayers/nf-d3d11sdklayers-id3d11debug-reportlivedeviceobjects)
- [ID3DUserDefinedAnnotation](https://learn.microsoft.com/en-us/windows/win32/api/d3d11_1/nn-d3d11_1-id3duserdefinedannotation)
- [ID3D11Device::CreateQuery](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11device-createquery)
- [D3D11_QUERY](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/ne-d3d11-d3d11_query)
- [ID3D11DeviceContext::GetData](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11devicecontext-getdata)
- [PIX on Windows documentation](https://devblogs.microsoft.com/pix/documentation/)
- [PIX GPU Captures](https://devblogs.microsoft.com/pix/gpu-captures/)
- [PIX Timing Captures](https://devblogs.microsoft.com/pix/timing-captures/)

次章では、これまでの機能を保守可能なRendererへ統合するRenderer ArchitectureとFrame Graphを扱います。
