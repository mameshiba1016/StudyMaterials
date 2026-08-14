# DirectX 12 第18章：Device Removed・DRED・PIX

この章では、GPU処理の失敗を検出・保存・解析・復旧する方法を学びます。HRESULT、Device Removed、Debug Layer、Info Queue、GPU-based Validation、DRED Breadcrumb/Page Fault、PIX Capture、Crash Report、再現Testまで扱います。

## 1. 到達目標

- Device Removedを通常のAPI失敗と区別して扱う。
- DRED情報から最後のGPU処理とPage Fault候補を追う。
- Debug Layer/GBV/PIXを目的別に使い分ける。
- Crash時に必要なRuntime状態を失う前に保存する。
- Device再作成の範囲と限界を説明する。

## 2. Device Removedとは

GPU Deviceが以後の処理に利用できない状態です。Driver Reset、GPU Hang、無効GPU Access、Hardware/Driver問題等で発生し得ます。

## 3. 発見地点と原因地点

失敗が`Present`や`Signal`で表面化しても、原因は以前に提出したCommandの場合があります。最後のHRESULTだけで原因箇所を断定しません。

## 4. 関連HRESULT

```text
DXGI_ERROR_DEVICE_REMOVED
DXGI_ERROR_DEVICE_RESET
DXGI_ERROR_DEVICE_HUNG
DXGI_ERROR_DRIVER_INTERNAL_ERROR
DXGI_ERROR_INVALID_CALL
```

値ごとに分類し、元のAPI名と共に記録します。

## 5. HRESULTを必ず検査する

Device/Resource/PSO/Command/Fence/Present作成・実行APIの失敗を無視しません。失敗後のNull Object利用で二次Crashを起こさないようにします。

## 6. GetDeviceRemovedReason

```cpp
const HRESULT reason = device->GetDeviceRemovedReason();
```

検出直後に取得して記録します。返値だけでDREDやContext情報を置き換えません。

## 7. 最初の失敗を保存する

複数Threadが異常を検出しても、最初のHRESULT、Frame、Pass、Queue、Thread、時刻をAtomicに一度だけ確定します。

## 8. Error State Machine

```text
Running -> FailureDetected -> SubmissionStopped
 -> DiagnosticsCaptured -> ResourcesReleased
 -> DeviceRecreated / FatalExit
```

複数Threadが勝手にDevice再作成を始めないよう所有者を一つにします。

## 9. Submission停止

異常検出後は新しいCommandをQueueへ提出せず、Worker/Streamingへ停止を通知します。壊れたDeviceを使い続けません。

## 10. 無限Fence Waitを避ける

Deviceが失われた後はFenceが目標値へ到達しない場合があります。Wait LoopでRemoved ReasonとTimeout/Shutdown状態を確認します。

## 11. Debug Layerを早期有効化

```cpp
Microsoft::WRL::ComPtr<ID3D12Debug> debug;
if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug))))
    debug->EnableDebugLayer();
```

Device作成前に有効化します。

## 12. Debug Buildだけに限定する理由

検証Costがあるため通常配布Buildでは方針を分けます。ただしRelease相当設定でのみ出るBugを捕捉する診断Buildも用意します。

## 13. Info Queue

`ID3D12InfoQueue`でMessageの保存、Filter、Break条件を設定できます。

## 14. Break on Severity

Corruption/ErrorでDebugger Breakし、Warningは内容を選別します。既知Warningを全部隠さず理由付きFilterへします。

## 15. Message Filter

OS/SDK差による既知MessageをIDで管理し、期限/根拠を残します。Severity全体を無効にしません。

## 16. Info Queue Log

Frame末尾や失敗時にMessage ID、Severity、Descriptionを自前Logへ保存するとRemote環境でも調査できます。

## 17. GPU-based Validation

Shader実行時Descriptor/Resource State等の問題を追加検証します。非常に重くなり得るため、小Sceneや自動Testで使います。

## 18. GBVの有効化時期

Debug Interfaceを通じDevice作成前に設定します。起動OptionでON/OFF可能にし、Capture設定へ記録します。

## 19. Synchronized Queue Validation

Queue同期問題を検出しやすくする追加検証があります。SDK Interface/利用可能性を確認します。

## 20. Debug Layerで見つかる例

Resource State不一致、Descriptor不正、Allocator早期Reset、PSO/RT Format不一致、Command List規則違反等です。

## 21. Debug Layerで見つからない例

見た目の数式Bug、論理的に誤ったBuffer Offset、正しい範囲内の古いData、Gameplay仕様違反等は別検証が必要です。

## 22. DREDとは

Device Removed Extended DataはGPU作業履歴やPage Fault関連情報を提供し、Device Removed原因調査を助けます。

## 23. DRED設定時期

DRED Settings InterfaceからAuto Breadcrumb、Page Fault等をDevice作成前に設定します。

## 24. DRED Settings例

```cpp
Microsoft::WRL::ComPtr<ID3D12DeviceRemovedExtendedDataSettings1> settings;
if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&settings))))
{
    settings->SetAutoBreadcrumbsEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
    settings->SetPageFaultEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
    settings->SetBreadcrumbContextEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
}
```

利用Interface VersionはWindows SDK/OSで確認します。

## 25. Auto Breadcrumb

GPUがどのCommand List/Operation付近まで進んだかを示す履歴です。最後の完了Breadcrumbと未完了Commandを調べます。

## 26. Breadcrumb Node

Command List名、Queue名、Breadcrumb Count、Last Value、Command History、Context等をLinked Listから収集します。

## 27. Breadcrumbを読む

最後に完了したOperationの次が疑わしい候補ですが、非同期実行やEarlier Memory Corruptionも考慮し断定しません。

## 28. Breadcrumb Context

Pass/Draw/Dispatchの意味が分かるMarkerを記録しておくと、数値Operationだけより原因へ近づけます。

## 29. Command List名

`Frame/Queue/Pass/Worker/Chunk`を含む名前を設定します。再利用時に最新用途へ更新します。

## 30. PIX Event Marker

Pass、Resource生成、Upload、Draw Bucket等へMarkerを入れ、Breadcrumb/PIX/CPU Logで同じ名称を使います。

## 31. Page Fault Data

GPU Virtual Address Faultに関連する既存/最近解放Allocation候補を取得できます。

## 32. Existing Allocation

Fault Address付近に現在存在するResource/Heap名、Type等を調べます。範囲計算とAlias状態も照合します。

## 33. Recently Freed Allocation

解放済みResourceがFault候補に出れば、GPU完了前解放、古いDescriptor/GPU Address、Indirect Data等を疑います。

## 34. Page Faultの典型原因

Buffer範囲外、Use-after-free、Descriptor早期再利用、不正GPU Address、Indirect Argument破損、Alias/Lifetime誤り等です。

## 35. DRED取得

Deviceから対応するDRED InterfaceをQueryし、Auto Breadcrumb/Page Fault Outputを取得します。失敗時も基本情報を保存します。

## 36. DRED Version

OS/SDKにより利用可能Interface/Fieldが異なります。新Versionを試し、利用不可なら旧VersionへFallbackします。

## 37. Pointer Lifetime

DRED Output内PointerをDevice破棄後に参照しません。必要情報を自前所有Memoryへ直ちにCopy/Serializeします。

## 38. Crash Report内容

```text
HRESULT / removed reason
frame / scene / camera
adapter / driver / OS / SDK
queue fence values
DRED breadcrumbs / page fault
recent resource/descriptor events
memory budget/usage
render settings / shader versions
```

## 39. Adapter情報

Vendor/Device ID、LUID、Dedicated Memory、Driver Version、Feature Level等を記録し、Hardware別傾向を集計します。

## 40. Build情報

Commit、Build ID、Compiler、Shader Hash、Asset Version、Config、Feature Toggleを含め、同じBinary/Dataを復元可能にします。

## 41. Privacy

User Path、名前、Network Token、個人Data等をCrash Reportへ無制限に含めません。収集/送信方針を明示します。

## 42. Crash-safe Logging

普段から固定容量Ring BufferへEventを保持し、異常時に重いAllocationを大量実行しなくても保存できるようにします。

## 43. Resource Registry

Resource ID、名前、Desc、Heap/Offset、GPU Address、Descriptor、作成/破棄Frame、Last-use Fenceを追跡します。

## 44. Descriptor Registry

Heap、Index、Generation、View Type、Resource ID、更新Frameを記録し、Fault Descriptorを逆引きします。

## 45. GPU Address Registry

Buffer Address RangeからResource/Allocationへ検索できるInterval Mapを開発Buildで持つと診断しやすくなります。

## 46. Command History

FrameごとのPass、List、Draw/Dispatch Count、Barrier、Signal/Waitを要約保存します。全Dataを無制限に残しません。

## 47. PIXとは

Windows向けGPU Capture/Timing/Shader/Resource解析Toolです。FrameのCommandとPipeline Stateを詳細確認できます。

## 48. PIX Capture準備

Debug名、Event Marker、安定した再現Scene、固定設定、Symbol/Shader Debug情報を用意します。

## 49. GPU Capture

問題FrameをCaptureし、Event List、Pipeline、Resource、Pixel History、Shader Debug、Warningsを順に調べます。

## 50. Timing Capture

CPU/GPU Timeline、Queue、Thread、Fence/Wait、Pass時間を調べます。GPU Captureとは目的とOverheadが異なります。

## 51. Pixel History

特定Pixelへ到達したDrawとDepth/Stencil/Blend等の結果を追います。見た目の欠落/上書きに有効です。

## 52. Pipeline View

PSO、Root Signature、Descriptor、VB/IB、RT/DS、Viewport/Scissorを確認します。

## 53. Resource History

Resourceの作成/使用/書込み/遷移を追い、最初に壊れたPassを探します。

## 54. Shader Debug

特定Thread/Pixelの入力と式を調べます。最適化やDebug情報、再現条件による制約があります。

## 55. Timingの注意

CaptureによるPerturbationがあります。複数回、通常実行Counter、別GPUで確認します。

## 56. RenderDoc等との使い分け

Toolごとに対応API/機能/Platformが異なります。D3D12 Queue/Timing/DREDは公式Tool/Runtime情報を中心にします。

## 57. Device再作成

全D3D12 Device Child、Queue、Fence、Heap、Resource、Descriptor、PSO等は失われたDeviceに属します。部分差替えでは済みません。

## 58. 再作成の流れ

```text
stop submissions/workers
capture diagnostics
release device-dependent objects
re-enumerate adapter
create device/queues/swap chain/resources
re-upload persistent assets
resume or return to safe scene
```

## 59. CPU Asset Cache

再Upload可能なSource/Runtime CPU Dataを保持するか再読込みできる設計が必要です。GPU ResourceだけしかないDataは復元困難です。

## 60. Gameplay State

Renderer復旧中にSimulationをPause/安全画面へ移す等の方針を決めます。中途半端な描画Object参照を残しません。

## 61. Adapter変更

元Adapterが使えない場合のFallback方針を決めます。Feature/Format/Memory能力が変わるため全設定を再評価します。

## 62. Recoveryの限界

Driver/Hardware状態によって再作成も失敗します。一定回数後は診断を保存して安全終了します。

## 63. TDR

Windowsが長時間応答しないGPU処理をResetする仕組みです。巨大Dispatch、無限Loop、極端なShader Work等を避けます。

## 64. Dispatch分割

大量WorkをChunk化し、各Dispatchの最悪実行時間を抑えます。ただしBarrier/Dispatch Overheadと比較します。

## 65. Shader無限Loop

Loop上限、入力Validation、Debug Counterを用意します。GPU上の停止条件を信用できない外部Dataへ依存させません。

## 66. Aftermath等

Vendor固有Crash SDKを追加利用する場合がありますが、DRED/PIX/自前LogとID体系を統合します。

## 67. Reproduction Package

Scene、Camera、Input、Seed、Render設定、Asset Hash、Frame番号を保存し、問題Frameまで自動Replayできる形を目指します。

## 68. Deterministic Capture

Random Seed、時間Step、Streaming順、Job結果順を固定し、同じCommand Sequenceへ近づけます。

## 69. Minimal Repro

Pass/Feature/Assetを一つずつ無効化し、Crashを維持する最小条件へ縮小します。一度に多数変更しません。

## 70. Feature Bisect

Shadow、Async Compute、GPU Culling、Raytracing等のToggleを自動組合せし、原因範囲を絞ります。

## 71. Validation Matrix

Debug Layer、GBV、DRED、PIX、Release最適化を組み合わせます。Tool有効時だけ消えるTiming Bugも記録します。

## 72. Driver Matrix

複数Vendor、Driver Version、GPU世代で再現率を集計します。最新Driverだけでなく最低対応範囲も確認します。

## 73. Long-run Test

Scene遷移、Resize、Alt+Tab、Streaming、戦闘を長時間繰返し、Lifetime/Fragmentation/Fence Wrap相当を検証します。

## 74. Fault Injection

Allocation失敗、Device Removed相当のError経路、Upload中断、Worker停止等を疑似発生させ、Cleanup/ReportをTestします。

## 75. Debug Layer Test Gate

代表Sceneを実行し、未許可Warning/Errorが1件でも出たら失敗させます。許可ListはIDと理由をVersion管理します。

## 76. DRED Parser Test

収集/Serialize Codeを模擬Linked DataでTestし、Null、長いList、文字列欠損でもCrashしないようにします。

## 77. Crash Handlerの注意

Processが不安定な状況でLock、Heap Allocation、他Thread待機を多用すると二次Deadlockします。最小限の安全な処理へします。

## 78. 二次Crash

診断処理自体の失敗を別Codeで記録します。元のRemoved Reasonを上書きしません。

## 79. 高速戦闘SceneでのMarker

Character/Effect単位を無限に細分化せず、Pass、Boss、Effect Batch、Indirect Buffer、Skinning Chunk等の意味ある粒度へ付けます。

## 80. SpikeとHangの区別

単に重いFrame、CPU Deadlock、GPU Wait Cycle、TDR/Device RemovedをTimeline/Fence/Removed Reasonで分けます。

## 81. よくある失敗：Presentだけ調べる

Breadcrumb、直前Command、Resource Lifetime、Shader/Dataを遡り、原因地点と検出地点を分けます。

## 82. よくある失敗：DRED名が空

Object名/Event Markerを普段から設定し、Crash後に追加しようとしません。

## 83. よくある失敗：再作成後またCrash

古いDevice Child/Descriptor/GPU Address、停止していないWorker、未再Upload Assetを確認します。

## 84. よくある失敗：Fence待ちで固まる

Removed Reason、未Signal値、Queue Cycle、Event設定失敗、Shutdown Flagを確認します。

## 85. よくある失敗：Debugでは再現しない

Timing、Optimization、GBV Cost、未初期化Data、Raceを疑い、Release診断BuildとRing Logを使います。

## 86. 実装Checklist

- [ ] 全重要HRESULTをAPI/Frame/Pass付きで検査する。
- [ ] Device作成前に必要なDebug/DRED設定を行う。
- [ ] 最初の失敗を一度だけ確定しSubmissionを停止する。
- [ ] Fence Wait中もDevice Removedを検出する。
- [ ] Object名とPIX Markerを一貫して付ける。
- [ ] DRED PointerをDevice破棄前にSerializeする。
- [ ] Resource/Descriptor/GPU Address履歴を保持する。
- [ ] Recovery時に全Device Childを再作成する。
- [ ] Fault Injectionと長時間Testを行う。

## 87. 理解確認問題

1. Device Removedの検出地点と原因地点が違う理由を説明してください。
2. Debug LayerとGBVを比較してください。
3. Auto Breadcrumbから何が分かるか説明してください。
4. Page Fault Dataで疑うLifetime Bugを挙げてください。
5. Crash Reportに必要な情報を挙げてください。
6. PIX GPU CaptureとTiming Captureを使い分けてください。
7. Fence Waitが永遠に戻らない場合の処理を説明してください。
8. Device再作成で作り直す対象を説明してください。
9. TDRを避けるCompute設計を提案してください。
10. Releaseでのみ起こるBugの再現方法を提案してください。

## 88. 要点

- HRESULTの失敗地点はGPU上の原因地点とは限りません。
- Debug Layer/GBVは事前検出、DREDはRemoved後診断、PIXはFrame解析に使います。
- Breadcrumb、Page Fault、Resource履歴、Markerを同じID体系で結びます。
- 異常時は新規Submissionを止め、診断をDevice破棄前に保存します。
- 再作成では全Device依存ObjectとGPU Assetを再構築します。
- Fence無限待ち、二次Crash、再作成Loopへ安全策を持たせます。
- 再現Package、Fault Injection、複数GPU/Driver Testで診断力を高めます。

## 89. 公式資料

- [DRED](https://learn.microsoft.com/en-us/windows/win32/direct3d12/use-dred)
- [ID3D12Device::GetDeviceRemovedReason](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12device-getdeviceremovedreason)
- [ID3D12InfoQueue](https://learn.microsoft.com/en-us/windows/win32/api/d3d12sdklayers/nn-d3d12sdklayers-id3d12infoqueue)
- [GPU-based Validation](https://learn.microsoft.com/en-us/windows/win32/direct3d12/using-d3d12-debug-layer-gpu-based-validation)
- [PIX on Windows](https://devblogs.microsoft.com/pix/)

## 90. 次章への接続

次章ではFrame Graph統合を扱います。これまでのPass、Resource、Barrier、Queue、Transient Memory、Markerを宣言Graphから自動構築し、DRED/PIXで追跡可能にします。
