# DirectX 11：DXGI Factory・Adapter・Output

この章では、DXGIを使ってGraphics AdapterとDisplay Outputを調べます。複数GPU環境では「最初に見つかったGPU」を無条件に使うだけでは、統合GPU、独立GPU、外付けGPU、Software Adapterを誤って選ぶ可能性があります。

## 1. DXGIの責務

DXGIはDirectX Graphics Infrastructureの略です。

- Adapterの列挙。
- Outputの列挙。
- Swap Chainの作成。
- Presentと表示Mode。
- Video Memory Budget情報。
- Hardware構成変更の検出。

## 2. Object関係

```text
IDXGIFactory
 -> IDXGIAdapter
    -> IDXGIOutput
 -> IDXGISwapChain
```

Factoryは列挙と生成の入口、AdapterはGPU能力、OutputはMonitor接続を表します。

## 3. DXGI Version付きInterface

```text
IDXGIFactory
IDXGIFactory1
...
IDXGIFactory6
IDXGIFactory7
```

数字が大きいほど新しい機能が追加されています。対象OSで利用可能か`QueryInterface`相当の`ComPtr::As`で確認します。

## 4. Header

```cpp
#include <dxgi1_6.h>
#include <wrl/client.h>

#pragma comment(lib, "dxgi.lib")
```

新しいHeaderをCompileできることと、実行OSがInterfaceを提供することは別です。

## 5. CreateDXGIFactory2

```cpp
using Microsoft::WRL::ComPtr;

UINT flags = 0;
#if defined(_DEBUG)
flags |= DXGI_CREATE_FACTORY_DEBUG;
#endif

ComPtr<IDXGIFactory6> factory;
HRESULT hr = CreateDXGIFactory2(
    flags,
    IID_PPV_ARGS(factory.GetAddressOf()));
```

## 6. IID_PPV_ARGS

`IID_PPV_ARGS`は出力Pointer型から対応IIDと`void**`を作るHelper Macroです。Interface型とIIDの不一致を減らします。

## 7. Factory作成失敗

HRESULT、Flag、OS Version、SDK Runtimeを記録し、Adapter列挙へ進みません。Debug Factoryだけ失敗した場合も原因を報告します。

## 8. Factoryの寿命

AdapterやSwap Chainを作る基盤としてRendererが所有します。Hardware構成が変わった場合は新しいFactoryを作り直す場合があります。

## 9. Factory IsCurrent

```cpp
if (!factory->IsCurrent())
{
    // AdapterやOutputの情報が古い可能性があります。
    // 安全な境界でFactoryと列挙情報を作り直します。
}
```

Display構成やAdapter情報の変更に対応します。

## 10. Adapterとは

AdapterはGraphics HardwareまたはSoftware Graphics能力の抽象です。

```text
iGPU  CPUと同一Packageに統合されたGPU
dGPU  独立したGPU
xGPU  外付けGPU
WARP  CPUで動くSoftware Rasterizer
```

## 11. 高性能順の列挙

```cpp
ComPtr<IDXGIAdapter4> adapter;

HRESULT hr = factory->EnumAdapterByGpuPreference(
    0,
    DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
    IID_PPV_ARGS(adapter.GetAddressOf()));
```

Index 0は指定Preferenceで最も優先されるAdapterです。

## 12. GPU Preference

```text
DXGI_GPU_PREFERENCE_UNSPECIFIED
DXGI_GPU_PREFERENCE_MINIMUM_POWER
DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE
```

Preferenceは性能保証ではなく列挙順の希望です。

## 13. Compatibility Fallback

`IDXGIFactory6`を取得できない環境では`IDXGIFactory1::EnumAdapters1`へFallbackできます。対象OS要件を決め、不要な古環境対応を増やしすぎません。

## 14. 列挙Loop

```cpp
std::vector<ComPtr<IDXGIAdapter4>> adapters;

for (UINT index = 0;; ++index)
{
    ComPtr<IDXGIAdapter4> adapter;
    const HRESULT hr = factory->EnumAdapterByGpuPreference(
        index,
        DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
        IID_PPV_ARGS(adapter.GetAddressOf()));

    if (hr == DXGI_ERROR_NOT_FOUND)
        break;
    if (FAILED(hr))
        throw HResultException(hr, "EnumAdapterByGpuPreference");

    adapters.push_back(std::move(adapter));
}
```

`DXGI_ERROR_NOT_FOUND`は列挙終了であり、必ずしも異常ではありません。

## 15. Adapter Description

```cpp
DXGI_ADAPTER_DESC3 description{};
HRESULT hr = adapter->GetDesc3(&description);
```

Name、Vendor ID、Device ID、Memory情報、LUID、Flagなどを取得します。

## 16. Description

`Description`は表示とLog用のGPU名です。機能判定を文字列比較で行いません。

## 17. VendorIdとDeviceId

Hardware識別や診断に使えますが、Vendor名だけで機能を決めずDirect3DのFeature Support Queryを使います。

## 18. Adapter LUID

LUIDはAdapterを識別する値です。保存Fileへ永続的なHardware IDとして盲信せず、ProcessやSystem構成変更を考慮します。

## 19. DedicatedVideoMemory

専用Video Memoryの容量を表す値です。これがそのままApplicationが自由に使える現在Budgetではありません。

## 20. SharedSystemMemory

GPUが共有可能なSystem Memoryの上限情報です。専用VRAMと同じ速度・性質ではありません。

## 21. Memory容量の表示

```cpp
double BytesToGiB(std::uint64_t bytes)
{
    constexpr double bytesPerGiB = 1024.0 * 1024.0 * 1024.0;
    return static_cast<double>(bytes) / bytesPerGiB;
}
```

GBとGiBの表記を混同しません。

## 22. Software Adapter Flag

```cpp
const bool software =
    (description.Flags & DXGI_ADAPTER_FLAG3_SOFTWARE) != 0;
```

通常のHardware Device候補からSoftware Adapterを除外できます。

## 23. Remote Adapter

Remote Sessionや仮想環境ではAdapterの性質が通常PCと異なります。FlagとDevice作成結果をLogします。

## 24. Adapter候補Data

```cpp
struct AdapterInfo final
{
    ComPtr<IDXGIAdapter4> adapter{};
    std::wstring name{};
    LUID luid{};
    std::uint64_t dedicatedVideoMemory{};
    std::uint64_t sharedSystemMemory{};
    UINT vendorId{};
    UINT deviceId{};
    bool software{};
};
```

COM Pointerと診断用Snapshotを分けても構いません。

## 25. Device作成可能性のProbe

列挙されたAdapterが必要なFeature LevelでD3D11 Deviceを作れるか試します。

```cpp
HRESULT hr = D3D11CreateDevice(
    adapter.Get(),
    D3D_DRIVER_TYPE_UNKNOWN,
    nullptr,
    creationFlags,
    featureLevels,
    static_cast<UINT>(std::size(featureLevels)),
    D3D11_SDK_VERSION,
    device.GetAddressOf(),
    &selectedFeatureLevel,
    context.GetAddressOf());
```

## 26. Adapter指定時のDriver Type

`pAdapter`へ非Null Adapterを渡す場合、Driver Typeは`D3D_DRIVER_TYPE_UNKNOWN`を使います。Hardwareを同時指定すると`E_INVALIDARG`になる契約があります。

## 27. 最初のAdapterだけで諦めない

Preference先頭Adapterで必要Feature LevelのDeviceを作れない場合、次候補を試します。各失敗HRESULTを記録します。

## 28. WARP Fallback

Hardware候補がすべて失敗した場合、仕様として許可するなら`D3D_DRIVER_TYPE_WARP`でSoftware Deviceを作ります。

WARPは「Software Adapterを通常列挙から選ぶ」方法と同一扱いにせず、明示的Fallbackとして管理します。

## 29. Adapter選択Policy

```cpp
struct AdapterSelectionPolicy final
{
    DXGI_GPU_PREFERENCE preference{
        DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE};
    bool allowSoftwareFallback{true};
    D3D_FEATURE_LEVEL minimumFeatureLevel{
        D3D_FEATURE_LEVEL_11_0};
};
```

## 30. User選択

SettingsでAdapterを選ばせる場合、NameだけでなくLUIDやVendor・Device IDを保存候補にします。次回起動で存在しなければ自動選択へ戻します。

## 31. OSのGPU Preference

Windows側のGraphics SettingsやDriver PolicyもGPU選択へ影響し得ます。Application内Preferenceが絶対命令とは限りません。

## 32. Hybrid GPU

LaptopではiGPUがDisplayを担当し、dGPUでRenderした結果を転送する構成があります。Outputが直接列挙されないAdapterでも描画に使える場合があります。

## 33. Outputとは

OutputはAdapterに接続されたDisplay出力を表します。

```text
Adapter 0
 -> Output 0: Internal display
 -> Output 1: External monitor
```

## 34. Output列挙

```cpp
std::vector<ComPtr<IDXGIOutput>> outputs;

for (UINT index = 0;; ++index)
{
    ComPtr<IDXGIOutput> output;
    const HRESULT hr = adapter->EnumOutputs(
        index, output.GetAddressOf());

    if (hr == DXGI_ERROR_NOT_FOUND)
        break;
    if (FAILED(hr))
        throw HResultException(hr, "EnumOutputs");

    outputs.push_back(std::move(output));
}
```

## 35. Output Description

```cpp
DXGI_OUTPUT_DESC description{};
HRESULT hr = output->GetDesc(&description);
```

Device Name、Desktop座標、Monitor Handle、Desktop接続状態、回転情報を得ます。

## 36. DesktopCoordinates

複数MonitorのVirtual Desktop上でのRectangleです。負の座標になるMonitorもあります。

## 37. AttachedToDesktop

OutputがDesktopへ接続されているかを示します。列挙されたから表示対象として利用可能と決めつけません。

## 38. Rotation

Portrait Monitorなどで出力回転があります。Windowed ApplicationはOS Compositionの影響も受けます。

## 39. Monitor Handle

`HMONITOR`を使い、Windowが主に属するMonitorやMonitor情報をWin32 APIから取得できます。

## 40. Windowに最も近いMonitor

```cpp
HMONITOR monitor = MonitorFromWindow(
    window, MONITOR_DEFAULTTONEAREST);
```

Window移動後に表示先が変わる可能性があります。

## 41. Output6

```cpp
ComPtr<IDXGIOutput6> output6;
if (SUCCEEDED(output.As(&output6)))
{
    DXGI_OUTPUT_DESC1 description{};
    output6->GetDesc1(&description);
}
```

より詳しいColor Spaceや輝度特性などを取得できます。

## 42. Color Space

SDR、HDR、Advanced ColorではOutputのColor特性が異なります。Back Buffer FormatとColor Space設定を一致させる必要があります。

## 43. Output情報は変化する

Monitor設定、HDR設定、接続状態は実行中に変化します。FactoryがCurrentでない場合は再作成し、Output情報を再取得します。

## 44. DPI Awarenessとの関係

Output座標や見えるScreen SizeはDPI Awarenessの影響を受けるAPIがあります。Win32 WindowのDPI設定とDXGI Output情報を合わせて扱います。

## 45. Display Mode

```cpp
UINT modeCount = 0;
HRESULT hr = output->GetDisplayModeList(
    DXGI_FORMAT_R8G8B8A8_UNORM,
    0,
    &modeCount,
    nullptr);
```

最初に件数を取得し、配列を確保して再度呼びます。

## 46. 二段階Query

```cpp
std::vector<DXGI_MODE_DESC> modes(modeCount);
hr = output->GetDisplayModeList(
    DXGI_FORMAT_R8G8B8A8_UNORM,
    0,
    &modeCount,
    modes.data());
modes.resize(modeCount);
```

二回の間に構成が変わる可能性があるAPIでは再試行方針を考えます。

## 47. Refresh Rate

```cpp
double RefreshHz(const DXGI_RATIONAL& rate)
{
    if (rate.Denominator == 0)
        return 0.0;
    return static_cast<double>(rate.Numerator) /
           static_cast<double>(rate.Denominator);
}
```

59.94Hzなどがあるため、整数Hzへ早く丸めません。

## 48. WindowedとExclusive Fullscreen

WindowedやBorderlessではDesktop Compositionとの関係が中心です。Exclusive FullscreenのMode列挙・切替は必要性を検討し、最初から複雑化しません。

## 49. Video Memory Budget

Adapter3 InterfaceからProcessの現在使用量とOSが割り当てたBudgetを取得できます。

```cpp
ComPtr<IDXGIAdapter3> adapter3;
if (SUCCEEDED(adapter.As(&adapter3)))
{
    DXGI_QUERY_VIDEO_MEMORY_INFO info{};
    HRESULT hr = adapter3->QueryVideoMemoryInfo(
        0,
        DXGI_MEMORY_SEGMENT_GROUP_LOCAL,
        &info);
}
```

## 50. Budget

`Budget`はOSがProcessへ現在割り当てる目標範囲です。専用VRAM総容量とは異なり、他ApplicationやSystem状況で変動します。

## 51. CurrentUsage

Processの現在Video Memory使用量です。Resource作成・破棄直後の内部処理やDriver挙動も考慮し、傾向をProfilerで見ます。

## 52. AvailableForReservation

予約に利用可能な量です。予約は物理Memoryを即座に独占する意味ではなく、OS Memory ManagerへのHintとして扱われます。

## 53. LocalとNonLocal

```text
DXGI_MEMORY_SEGMENT_GROUP_LOCAL
DXGI_MEMORY_SEGMENT_GROUP_NON_LOCAL
```

Discrete GPUとIntegrated GPUでMemory構成の意味が異なります。

## 54. Budget超過

使用量がBudgetを超えると、Page Outや一時停止によってStutterが起こる可能性があります。単にResource作成成功だけを基準にしません。

## 55. Memory Telemetry

```cpp
struct VideoMemoryStats final
{
    std::uint64_t budget{};
    std::uint64_t currentUsage{};
    std::uint64_t availableForReservation{};
    std::uint64_t currentReservation{};

    double UsageRatio() const
    {
        if (budget == 0)
            return 0.0;
        return static_cast<double>(currentUsage) /
               static_cast<double>(budget);
    }
};
```

## 56. Memory Pressure対応

- 未使用TextureをCacheから退避する。
- 高解像度Mipを後回しにする。
- Render Target解像度を見直す。
- Streaming Budgetを下げる。
- WarningとCapture情報を記録する。

戦闘中に無計画な同期解放を大量実行しません。

## 57. Budget変化通知

Adapter3にはVideo Memory Budget変更通知の仕組みがあります。Polling頻度を下げ、OSからの変更をEventで検出する設計を後のResource管理で扱います。

## 58. FactoryとAdapterのCache

列挙結果を毎Frame作りません。初期化時と構成変更時だけ更新し、Frame中は選択済みAdapterとSnapshotを使います。

## 59. DeviceからAdapterを逆引きする

既にD3D11 Deviceがある場合、`IDXGIDevice`、Adapter、FactoryとParentを辿れます。

```cpp
ComPtr<IDXGIDevice> dxgiDevice;
device.As(&dxgiDevice);

ComPtr<IDXGIAdapter> adapter;
dxgiDevice->GetAdapter(adapter.GetAddressOf());
```

## 60. 同じFactory系統を使う

Deviceから取得したAdapterのParent Factoryと、別途新規作成したFactoryのObjectを混在させる古いDXGI規則に注意します。初期化経路を一つに統一します。

## 61. Device Removed後

Adapterが取り外された、Driverが更新された場合、古いFactoryとAdapter情報を使い続けず再列挙し、Deviceと全依存Resourceを再作成します。

## 62. Hot Plug

外付けGPUやMonitorの接続変更中にOutput、Color、Adapter構成が変わります。Window MessageとFactory鮮度を組み合わせ、Frame境界で再構築します。

## 63. Adapter Log

起動時に次をLogします。

- Adapter Name。
- Vendor ID、Device ID。
- LUID。
- Dedicated、Shared Memory。
- Software Flag。
- 選択Feature Level。
- Device作成失敗HRESULT。

## 64. Output Log

- Device Name。
- Desktop Rectangle。
- Attached状態。
- Rotation。
- Color Space。
- Bits Per Color。
- 輝度情報。
- 現在DPIとWindow位置。

## 65. Debug UI

Adapter一覧、選択中GPU、Output一覧、Memory Budget、Current Usage、Budget比率を画面に表示できるようにします。

## 66. Adapter選択関数

```cpp
struct AdapterSelectionResult final
{
    ComPtr<IDXGIAdapter4> adapter{};
    D3D_FEATURE_LEVEL featureLevel{};
    HRESULT lastFailure{S_OK};
    bool usingWarp{};
};
```

成功Objectだけでなく選択理由とFallback状態を返します。

## 67. 選択Algorithm

```text
Create debug-capable factory
 -> enumerate by configured GPU preference
 -> skip software adapters for hardware path
 -> read and log description
 -> probe required D3D feature level
 -> choose first valid adapter
 -> if none and allowed, create WARP device
 -> retain adapter identity and diagnostics
```

## 68. よくある失敗：VRAM最大だけで選ぶ

Memory容量だけではGPU性能、Driver対応、消費電力、外付け状態を判断できません。OSのPreference順とDevice作成検査を使います。

## 69. よくある失敗：Software Adapterを選ぶ

通常Hardware候補にSoftware Flagを含め、性能が極端に低くなります。WARPは明示的Fallbackにします。

## 70. よくある失敗：pAdapterとHARDWARE

明示Adapterを`D3D11CreateDevice`へ渡しながら`D3D_DRIVER_TYPE_HARDWARE`を指定し、`E_INVALIDARG`になります。`UNKNOWN`を使います。

## 71. よくある失敗：DedicatedVideoMemoryをBudget扱い

総容量だけを見てResourceを作り、OS Budgetを超えてStutterします。`QueryVideoMemoryInfo`を監視します。

## 72. よくある失敗：OutputがないGPUを無効扱い

Hybrid構成ではDisplayへ直接接続されない高性能GPUが描画可能な場合があります。Output数だけでDevice候補を除外しません。

## 73. よくある失敗：Factoryを永久に使う

構成変更後も古いOutput色情報を使います。`IsCurrent`と再列挙経路を持ちます。

## 74. Adapter列挙テスト

- 0件終了を安全に処理する。
- 複数AdapterをPreference順に記録する。
- Software FlagをHardware候補から除外する。
- 一つ目のDevice作成失敗後に次を試す。
- WARP許可・不許可を切り替える。
- 古いFactoryの再作成を行う。

## 75. Outputテスト

- Output 0件を安全に扱う。
- 複数Monitorと負座標を記録する。
- Desktop非接続Outputを識別する。
- Window移動でMonitorを再判定する。
- DPIとColor情報を再取得する。
- Display ModeのDenominator 0を防ぐ。

## 76. Memory Budgetテスト

- Query非対応時に機能を無効化する。
- Budget 0で0除算しない。
- LocalとNonLocalを別に表示する。
- UsageがBudgetを超えたとき警告する。
- Scene終了後にUsageが低下するか観察する。

## 77. 完成確認表

- [ ] Debug Flag付きFactoryを作れる。
- [ ] GPU Preference順にAdapterを列挙できる。
- [ ] Adapter DescriptionとLUIDをLogできる。
- [ ] Software Adapterを区別できる。
- [ ] 明示AdapterではDriver Type UNKNOWNを使う。
- [ ] 必要Feature LevelでDeviceをProbeできる。
- [ ] Hardware失敗時のWARP方針がある。
- [ ] OutputとMonitor情報を列挙できる。
- [ ] Memory総容量とProcess Budgetを区別できる。
- [ ] Factoryが古くなったとき再構築できる。

## 78. この章の要点

- FactoryはAdapter、Output、Swap Chainの入口です。
- AdapterはGPU能力、OutputはDisplay接続を表します。
- GPU Preferenceは列挙順の希望であり性能保証ではありません。
- Adapter FlagとDevice作成Probeで実際の候補を選びます。
- Hybrid GPUではOutputがないAdapterも描画候補になり得ます。
- Dedicated Memory総量とOSがProcessへ与えるBudgetは別です。
- Factoryの鮮度を確認し、構成変更後は再列挙します。
- 選択理由、Hardware情報、Memory BudgetをLogとDebug UIへ残します。

## 79. 公式資料

- [DXGI overview](https://learn.microsoft.com/en-us/windows/win32/direct3ddxgi/d3d10-graphics-programming-guide-dxgi)
- [EnumAdapterByGpuPreference](https://learn.microsoft.com/en-us/windows/win32/api/dxgi1_6/nf-dxgi1_6-idxgifactory6-enumadapterbygpupreference)
- [DXGI adapter description](https://learn.microsoft.com/en-us/windows/win32/api/dxgi/ns-dxgi-dxgi_adapter_desc)
- [QueryVideoMemoryInfo](https://learn.microsoft.com/en-us/windows/win32/api/dxgi1_4/nf-dxgi1_4-idxgiadapter3-queryvideomemoryinfo)
- [DXGI query video memory info](https://learn.microsoft.com/en-us/windows/win32/api/dxgi1_4/ns-dxgi1_4-dxgi_query_video_memory_info)
- [IDXGIOutput6 GetDesc1](https://learn.microsoft.com/en-us/windows/win32/api/dxgi1_6/nf-dxgi1_6-idxgioutput6-getdesc1)

次章では、選択したAdapterからD3D11 Device、Feature Level、Immediate Contextを作り、Capability QueryとThreading境界を詳しく扱います。
