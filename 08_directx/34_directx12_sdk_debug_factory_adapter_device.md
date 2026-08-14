# DirectX 12 第2章：Windows SDK・Debug Layer・Factory・Adapter・Device

この章では、DirectX 12初期化の入口を実装順に学びます。Visual Studio/Windows SDK、Header/Library、Debug Layer、DXGI Factory、Adapter列挙、GPU Preference、WARP、Device生成、Feature照会、診断情報までを扱います。

## 1. 初期化の完成条件

```text
Build環境確認
 -> Debug Layer設定
 -> DXGI Factory生成
 -> Adapter列挙・選択
 -> D3D12 Device生成
 -> Feature Support照会
 -> Info Queue設定
```

この章ではまだWindowへ描画しません。

## 2. 必要な開発環境

- Windows 10/11
- Visual StudioのDesktop C++ Workload
- Windows SDK
- 対応GPU Driver
- C++17以降を推奨

具体的Version要件は使用機能と配布対象OSに合わせます。

## 3. Windows SDK Version

Project PropertiesのWindows SDK Versionを、導入済みSDKへ合わせます。存在しないVersionを固定すると別PCでBuildできません。

## 4. Platform Toolset

Visual StudioのC++ Toolsetを選びます。Debug/Release、x64、Runtime Library、Language StandardをProject全体で統一します。

## 5. x64を基本にする

現代のGame開発では通常x64を使います。Win32/x86設定とLibrary/Output Pathを混在させません。

## 6. 必要Header

```cpp
#include <windows.h>
#include <wrl/client.h>
#include <dxgi1_6.h>
#include <d3d12.h>
#include <dxgidebug.h>
#include <string>
#include <vector>
#include <stdexcept>
```

使用するInterface Versionに応じたDXGI Headerを選びます。

## 7. ComPtr

```cpp
using Microsoft::WRL::ComPtr;
```

COM参照CountをRAIIで管理します。`Get`、`GetAddressOf`、`ReleaseAndGetAddressOf`、`As`の違いを理解します。

## 8. Link Library

```cpp
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxguid.lib")
```

Build System側で指定する方法でも構いません。Shader Compilerは使用方式に応じて別途設定します。

## 9. Unicode

Win32 APIとAdapter名はWide Stringを多く使います。ProjectをUnicode Character Setにし、安易な文字化け変換を避けます。

## 10. DebugとRelease

Debug LayerやGPU-based Validationは開発用です。Release BuildでもHRESULT、Device Removed診断、最低限のLogは残します。

## 11. HRESULT Helper

```cpp
class GraphicsException : public std::runtime_error
{
public:
    GraphicsException(HRESULT hr, std::string operation)
        : std::runtime_error(std::move(operation)), hr_(hr) {}

    HRESULT Error() const noexcept { return hr_; }

private:
    HRESULT hr_;
};
```

## 12. ThrowIfFailed

```cpp
inline void ThrowIfFailed(HRESULT hr, std::string_view operation)
{
    if (FAILED(hr))
    {
        LogHRESULT(hr, operation);
        throw GraphicsException(hr, std::string(operation));
    }
}
```

失敗したAPI名を必ず残します。

## 13. HRESULTの文字列化

`FormatMessageW`等でSystem Messageを取得し、16進値も併記します。DXGI固有ErrorはSymbolic Nameも分類します。

## 14. 初期化Config

```cpp
struct GraphicsConfig
{
    bool enableDebugLayer = false;
    bool enableGpuValidation = false;
    bool preferHighPerformance = true;
    bool allowWarpFallback = true;
    D3D_FEATURE_LEVEL minimumFeatureLevel = D3D_FEATURE_LEVEL_11_0;
};
```

Build Macroだけでなく実行設定として選べると診断しやすくなります。

## 15. Debug LayerはDevice作成前

Debug Layerを有効化するなら`D3D12CreateDevice`より前に行います。Device作成後に有効化しようとしません。

## 16. D3D12GetDebugInterface

```cpp
ComPtr<ID3D12Debug> debug;
ThrowIfFailed(
    D3D12GetDebugInterface(IID_PPV_ARGS(&debug)),
    "D3D12GetDebugInterface");

debug->EnableDebugLayer();
```

## 17. Debug Component未導入

Debug Interface取得が失敗する場合はWindows Graphics Toolsの導入状況を確認します。製品起動を必ず失敗させるか、Debugなしで続行するかPolicyを決めます。

## 18. GPU-based Validation

```cpp
ComPtr<ID3D12Debug1> debug1;
if (SUCCEEDED(debug.As(&debug1)))
{
    debug1->SetEnableGPUBasedValidation(TRUE);
}
```

DescriptorやResource StateのShader側Accessを追加検証できます。

## 19. GPU ValidationのCost

GPU-based ValidationはShaderへ検査処理を追加し、実行・Memory Costが大きくなる場合があります。通常Performance計測では無効にします。

## 20. Synchronized Command Queue Validation

対応Debug InterfaceでQueue Validationを設定できます。GPU-based ValidationとのCostと目的を理解して使います。

## 21. Auto Name

対応Debug InterfaceではObject自動命名を有効化できます。明示的な用途名を付ける設計も継続します。

## 22. Debug Layer設定順

```text
D3D12GetDebugInterface
 -> Debug/GPU Validation設定
 -> CreateDXGIFactory2
 -> D3D12CreateDevice
```

順序を一つの初期化関数に固定します。

## 23. DXGI Factory

FactoryはAdapter、Output、Swap Chain等のDXGI Objectを生成・列挙する入口です。

## 24. CreateDXGIFactory2

```cpp
UINT factoryFlags = 0;

#if defined(_DEBUG)
if (config.enableDebugLayer)
    factoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
#endif

ComPtr<IDXGIFactory6> factory;
ThrowIfFailed(
    CreateDXGIFactory2(factoryFlags, IID_PPV_ARGS(&factory)),
    "CreateDXGIFactory2");
```

## 25. Factory Interface

必要な`IDXGIFactory6`等を要求します。古いOS Supportが必要ならInterface取得失敗時のFallback列挙を用意します。

## 26. Factory Debug Flag

DXGI側のDebug Messageを有効にします。D3D12 Debug Layerとは別の層です。

## 27. Adapterとは

Graphics HardwareまたはSoftware Rendererを表します。PCにはIntegrated GPU、Discrete GPU、Remote Adapter等が存在し得ます。

## 28. Adapterを決め打ちしない

Adapter index 0が必ず最速、Discrete、目的GPUとは限りません。列挙し、DescriptionとFeature Supportを調べます。

## 29. EnumAdapterByGpuPreference

```cpp
const DXGI_GPU_PREFERENCE preference =
    config.preferHighPerformance
        ? DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE
        : DXGI_GPU_PREFERENCE_UNSPECIFIED;

for (UINT index = 0; ; ++index)
{
    ComPtr<IDXGIAdapter1> adapter;
    if (factory->EnumAdapterByGpuPreference(
            index, preference, IID_PPV_ARGS(&adapter)) == DXGI_ERROR_NOT_FOUND)
        break;

    // DescriptionとSupportを評価する。
}
```

## 30. GPU Preferenceは保証ではない

列挙順のHintであり、要件を満たすかは個別に検証します。ユーザーのOS Graphics Preferenceも考慮されます。

## 31. Adapter Description

```cpp
DXGI_ADAPTER_DESC1 desc{};
ThrowIfFailed(adapter->GetDesc1(&desc), "IDXGIAdapter1::GetDesc1");
```

Name、Vendor ID、Device ID、Memory、FlagをLogへ出します。

## 32. DXGI_ADAPTER_DESC3

`IDXGIAdapter4::GetDesc3`を使える環境ではGraphics/Compute Preemption、Hardware/Software Flag等の追加情報を取得できます。

## 33. Software Adapter除外

```cpp
if ((desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) != 0)
    continue;
```

通常のHardware選択では除外し、WARPは専用Pathで選びます。

## 34. DedicatedVideoMemory

専用Video Memory量だけで性能を順位付けしません。Integrated GPUではShared Memory構成が異なります。

## 35. Vendor ID

診断やWorkaround識別に使えますが、Vendor名だけで機能Supportを仮定しません。Feature Queryを使います。

## 36. LUID

Adapter LUIDはProcess/Subsystem間でAdapterを識別する用途に使えます。Device Lost後の再列挙では旧Adapterが存在するか確認します。

## 37. Adapter Candidate

```cpp
struct AdapterCandidate
{
    ComPtr<IDXGIAdapter1> adapter;
    DXGI_ADAPTER_DESC1 desc{};
    D3D_FEATURE_LEVEL featureLevel{};
    bool supportsRequiredFeatures = false;
};
```

列挙、評価、選択を別段階にします。

## 38. D3D12CreateDeviceによるSupport確認

Candidate Adapterで最低Feature LevelのDevice作成が成功するかを確認します。選択後に最終Deviceを保持します。

## 39. D3D12CreateDevice

```cpp
ComPtr<ID3D12Device> device;
ThrowIfFailed(
    D3D12CreateDevice(
        adapter.Get(),
        config.minimumFeatureLevel,
        IID_PPV_ARGS(&device)),
    "D3D12CreateDevice");
```

## 40. Minimum Feature Level

第二引数はApplicationが要求する最低Levelです。作成後に実際の最大Supportを個別照会します。

## 41. nullptr Adapter

Adapterへ`nullptr`を渡すとDefault Adapterが使われます。学習用には簡単ですが、複数GPU選択と診断を行うなら明示列挙します。

## 42. Adapter選択Policy

1. User指定Adapterが有効なら優先する。
2. GPU Preference順にHardwareを列挙する。
3. 必須Featureを検証する。
4. Score/Policyで選ぶ。
5. 必要ならWARPへFallbackする。

## 43. User選択の保存

Adapter indexはDriver更新や接続変更で変わります。LUIDやVendor/Device ID等を保存し、見つからなければ再選択します。

## 44. Laptopの複数GPU

Integrated/Discrete切替、Power Policy、External GPUを想定します。Applicationだけで完全制御できると仮定しません。

## 45. Display OutputとAdapter

Windowが置かれたMonitorとRendering Adapterが異なる場合があります。Present/Copy CostやHDR Supportの調査でOutput情報を使います。

## 46. WARP

Windows Advanced Rasterization PlatformはCPU上のSoftware Rasterizerです。Hardwareが使えない環境、Test、機能検証のFallbackに利用できます。

## 47. EnumWarpAdapter

```cpp
ComPtr<IDXGIAdapter> warpAdapter;
ThrowIfFailed(
    factory->EnumWarpAdapter(IID_PPV_ARGS(&warpAdapter)),
    "IDXGIFactory4::EnumWarpAdapter");

ComPtr<ID3D12Device> warpDevice;
ThrowIfFailed(
    D3D12CreateDevice(
        warpAdapter.Get(),
        config.minimumFeatureLevel,
        IID_PPV_ARGS(&warpDevice)),
    "D3D12CreateDevice(WARP)");
```

## 48. WARPの用途

- CI/Virtual環境の一部
- Hardware Driver問題の切り分け
- 描画正しさの比較
- Device作成Fallback

Performance代表値には使いません。

## 49. Remote Adapter

Remote Desktop等ではAdapter構成が変わる可能性があります。起動時の列挙結果とDevice Lost後の再列挙をLogします。

## 50. Device作成後の命名

```cpp
device->SetName(L"Main D3D12 Device");
```

以後作るQueue、Allocator、Heap、Resourceにも用途名を付けます。

## 51. SetName Helper

```cpp
template<class T>
void SetObjectName(T* object, std::wstring_view name)
{
    if (object && !name.empty())
        ThrowIfFailed(object->SetName(name.data()), "ID3D12Object::SetName");
}
```

`wstring_view`がNull終端されない場合を考慮し、実装では安全な`std::wstring`へ変換します。

## 52. ID3D12InfoQueue

```cpp
ComPtr<ID3D12InfoQueue> infoQueue;
if (SUCCEEDED(device.As(&infoQueue)))
{
    infoQueue->SetBreakOnSeverity(
        D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
    infoQueue->SetBreakOnSeverity(
        D3D12_MESSAGE_SEVERITY_ERROR, TRUE);
}
```

## 53. Warning Break

開発初期はWarningでもBreakし、原因を理解します。既知MessageをFilterする場合はIDと理由を管理します。

## 54. Info Queue Storage

保存Message数が無制限に増えないよう、Frame/Testごとに読み出してClearする設計もあります。

## 55. DXGI Info Queue

`DXGIGetDebugInterface1`から`IDXGIInfoQueue`を取得し、DXGI Messageを調べられます。D3D12 Info Queueと分けます。

## 56. Live Object Report

終了時にDXGI Debug Interfaceから生存Objectを報告できます。正しい解放順の後で参照Leakを確認します。

## 57. Feature Queryの目的

Device作成成功は全D3D12機能Supportを意味しません。Shader Model、Root Signature、Binding Tier、Raytracing等を個別に照会します。

## 58. CheckFeatureSupport

```cpp
template<class T>
bool CheckFeature(
    ID3D12Device* device,
    D3D12_FEATURE feature,
    T& data)
{
    return SUCCEEDED(device->CheckFeatureSupport(
        feature, &data, sizeof(T)));
}
```

構造体の初期値を正しく設定します。

## 59. Feature Levels Query

```cpp
D3D_FEATURE_LEVEL requested[] =
{
    D3D_FEATURE_LEVEL_12_2,
    D3D_FEATURE_LEVEL_12_1,
    D3D_FEATURE_LEVEL_12_0,
    D3D_FEATURE_LEVEL_11_1,
    D3D_FEATURE_LEVEL_11_0
};

D3D12_FEATURE_DATA_FEATURE_LEVELS levels{};
levels.NumFeatureLevels = static_cast<UINT>(std::size(requested));
levels.pFeatureLevelsRequested = requested;

ThrowIfFailed(device->CheckFeatureSupport(
    D3D12_FEATURE_FEATURE_LEVELS,
    &levels,
    sizeof(levels)), "Check FEATURE_LEVELS");
```

`levels.MaxSupportedFeatureLevel`を記録します。

## 60. Shader Model Query

```cpp
D3D12_FEATURE_DATA_SHADER_MODEL shaderModel{
    D3D_SHADER_MODEL_6_7
};

if (FAILED(device->CheckFeatureSupport(
        D3D12_FEATURE_SHADER_MODEL,
        &shaderModel,
        sizeof(shaderModel))))
{
    shaderModel.HighestShaderModel = D3D_SHADER_MODEL_5_1;
}
```

要求した最高Versionから段階的にFallbackする設計も使えます。

## 61. Shader ModelとFeature Level

別概念です。Feature Levelだけから使用可能Shader Modelを決めつけず、Compiler/OS/DriverとQuery結果を確認します。

## 62. Root Signature Version

```cpp
D3D12_FEATURE_DATA_ROOT_SIGNATURE rootSignature{
    D3D_ROOT_SIGNATURE_VERSION_1_1
};

if (FAILED(device->CheckFeatureSupport(
        D3D12_FEATURE_ROOT_SIGNATURE,
        &rootSignature,
        sizeof(rootSignature))))
{
    rootSignature.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
}
```

## 63. D3D12 Options

`D3D12_FEATURE_D3D12_OPTIONS`からResource Binding Tier、Tiled Resources Tier等の基本能力を取得します。

## 64. Options系列

`OPTIONS1`、`OPTIONS5`等、多数の追加Feature構造体があります。SDK更新で増えるため、利用機能に必要なものだけQueryします。

## 65. Resource Binding Tier

Descriptor数やBinding Modelに関係します。Bindless/大規模Descriptor設計では必ず確認します。

## 66. Resource Heap Tier

Heapへ配置できるResource種類の制約に関係します。Placed Resource Allocator設計で重要です。

## 67. Format Support

```cpp
D3D12_FEATURE_DATA_FORMAT_SUPPORT formatSupport{};
formatSupport.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;

ThrowIfFailed(device->CheckFeatureSupport(
    D3D12_FEATURE_FORMAT_SUPPORT,
    &formatSupport,
    sizeof(formatSupport)), "Check FORMAT_SUPPORT");
```

必要なRender Target、Blend、Typed UAV等のFlagを確認します。

## 68. Multisample Quality Levels

FormatとSample Countごとに`D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS`を照会します。MSAA対応を一括りにしません。

## 69. Architecture Query

UMA、Cache Coherent UMA、Tile Based Renderer等の情報を取得できます。Memory設計やPerformance Hintとして使います。

## 70. GPU Virtual Address Support

Process/Resource当たりのGPU VA Bit数等をQueryできます。大規模Allocatorや発展機能で参照します。

## 71. Protected Resource Session

保護Content等の特殊要件がある場合にFeatureを確認します。通常の学習Rendererでは必須ではありません。

## 72. Raytracing Tier

`D3D12_FEATURE_D3D12_OPTIONS5`等で確認します。D3D12 Deviceが作れたからDXR対応とは限りません。

## 73. Mesh Shader Tier

対応Optionsで確認します。従来Vertex Pipelineの後に学ぶ発展機能であり、初期化の必須要件にしません。

## 74. Variable Rate Shading

VRS TierとTile Size等をQueryします。高速ActionのPerformance調整候補ですが画質検証が必要です。

## 75. Sampler Feedback

Streaming最適化に使える発展機能です。対応Tierを確認し、Fallback Pathを用意します。

## 76. Feature Requirements

```cpp
struct RequiredFeatures
{
    D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_0;
    D3D_SHADER_MODEL shaderModel = D3D_SHADER_MODEL_6_0;
    D3D_ROOT_SIGNATURE_VERSION rootSignature = D3D_ROOT_SIGNATURE_VERSION_1_0;
};
```

必須と任意を分けます。

## 77. Capability Structure

```cpp
struct DeviceCapabilities
{
    D3D_FEATURE_LEVEL featureLevel{};
    D3D_SHADER_MODEL shaderModel{};
    D3D_ROOT_SIGNATURE_VERSION rootSignatureVersion{};
    D3D12_RESOURCE_BINDING_TIER bindingTier{};
    D3D12_RESOURCE_HEAP_TIER heapTier{};
    bool supportsRaytracing = false;
    bool supportsMeshShader = false;
};
```

Rendererの各所で再Queryせず、Device生成時にまとめます。

## 78. RequiredとOptional

必須Feature不足なら明確なErrorで起動を止めます。任意Feature不足ならFallback Renderer/Shader Pathを選びます。

## 79. Feature Log

起動時にAdapter情報、Feature Level、Shader Model、Root Signature Version、Tier、Format Supportを一つのReportへ保存します。

## 80. Adapter Score

High-performance Preference、必須Support、Memory情報、User指定等からScore化できます。ただし不透明なScoreより選択理由をLogへ残します。

## 81. Device Factory関数

```cpp
struct DeviceCreationResult
{
    ComPtr<IDXGIFactory6> factory;
    ComPtr<IDXGIAdapter1> adapter;
    ComPtr<ID3D12Device> device;
    DeviceCapabilities capabilities;
};

DeviceCreationResult CreateGraphicsDevice(const GraphicsConfig& config);
```

成功結果を一つにまとめます。

## 82. 初期化を段階化する

Debug、Factory、Adapter、Device、Capabilities、Info Queueを小さな関数へ分け、各段階のErrorをTest可能にします。

## 83. Partial Failure

途中失敗時も`ComPtr`が既に作ったObjectを安全に解放します。半端なGlobal Singletonへ公開しません。

## 84. Publish after Success

完全な`DeviceCreationResult`が完成してからRenderer MemberへMoveします。他Threadが初期化途中のDeviceを見ないようにします。

## 85. Threading

Device/Factory初期化は一つのThreadで完了させると単純です。Resource Loading WorkerはDevice公開後に開始します。

## 86. Agility SDK

DirectX 12 Agility SDKを使うと対応環境でD3D12 Runtime機能をApplicationと共に配布できます。NuGet/Package設定、SDK Version、Path、署名、OS要件は公式資料に従います。

## 87. System Runtimeとの違い

Agility SDKを使わない場合はOS提供のD3D12 Runtimeを使います。どちらを使うかBuild/Deployment方針として明示します。

## 88. SDK Version Export

Agility SDKでは`D3D12SDKVersion`と`D3D12SDKPath`等の設定が関係します。Packageの公式手順をそのVersionに合わせて使用します。

## 89. D3D12Core配置

Agility SDK Binaryの配置Pathを間違えると起動時にRuntimeをLoadできません。Build OutputとInstallerを両方検証します。

## 90. Shader Compiler

Shader Model 6系ではDXCを使用します。D3D12 Device初期化とは別Subsystemですが、要求Shader Modelと配布Binaryを一致させます。

## 91. Debug LayerとAgility

Runtime/SDK/Debug LayerのVersion整合を確認します。Device作成失敗時はOS Build、Driver、SDK設定をLogへ出します。

## 92. Device Removed初期診断

Device作成直後でもDriver/Adapter問題が起こり得ます。以後のAPI失敗では`GetDeviceRemovedReason`とDREDを使います。

## 93. DRED設定時期

DREDの設定はDevice作成前に行う項目があります。Debug初期化Phaseへ含め、Crash後にBreadcrumb/Page Fault情報を保存します。

## 94. Stable Power State

GPU計測向けの機能がありますが、開発者Modeや権限、用途の制約があります。通常Player動作で有効にしません。

## 95. PIXとの接続

Adapter/Device/Queue/Resource名とEvent Markerを付けることでCaptureが読みやすくなります。PIX要件は公式の最新情報を確認します。

## 96. 起動Log例

```text
DXGI Factory Debug: on
Adapter: Example GPU
VendorId: 0x1234 DeviceId: 0x5678
DedicatedVideoMemory: ... MiB
FeatureLevel: 12_1
ShaderModel: 6_7
RootSignature: 1_1
ResourceBindingTier: 3
WARP: false
GPU Validation: off
```

## 97. Error Message設計

「Device作成失敗」だけでなく、Adapter名、要求Feature Level、HRESULT、OS/Driver/SDK情報、Fallback試行結果を表示します。

## 98. Adapter選択Test

- Hardware Adapterが1個。
- Integrated/Discreteの複数GPU。
- Software Adapterを除外。
- User指定Adapterが消失。
- Hardware失敗からWARP Fallback。
- Remote/Virtual環境。

## 99. Feature Query Test

Query失敗、低いShader Model、Root Signature 1.0、Format非対応をMock/Policy Testし、Fallbackが選ばれるか確認します。

## 100. Debug Configuration Test

Debug Layer off/on、GPU Validation off/on、Graphics Toolsなし、Release BuildでDevice生成結果とLogを確認します。

## 101. よくある失敗：Device作成後にDebug Layer

Debug Layer有効化の順序が遅く、期待するValidationが付かないかDevice問題になります。最初に設定します。

## 102. よくある失敗：Adapter 0固定

望まないIntegrated/Software Adapterを選ぶ可能性があります。Preference列挙とFeature検証を行います。

## 103. よくある失敗：VRAM量だけで選択

Memory ArchitectureやUser Preferenceを無視します。Feature、Preference、Policy、実測を組み合わせます。

## 104. よくある失敗：Feature Levelだけ確認

Shader Model、Root Signature、Binding Tier、Format等が不足します。使用機能を個別Queryします。

## 105. よくある失敗：必須と任意を混同

任意のRaytracing非対応だけで起動不能にする等の問題が起きます。Fallback可能性を要件表へ書きます。

## 106. よくある失敗：Debug Warningを大量Filter

意味を理解せず消すとResource StateやLifetime問題を隠します。ID単位で理由を残します。

## 107. よくある失敗：初期化途中で公開

WorkerがCapabilities未確定Deviceを使います。成功結果を構築後に一括Publishします。

## 108. 実装Checklist

- [ ] Windows SDK/Toolset/x64設定を説明できる。
- [ ] Debug LayerをDevice作成前に有効化する。
- [ ] FactoryへDXGI Debug Flagを設定する。
- [ ] GPU PreferenceでAdapterを列挙する。
- [ ] Software Adapterを通常選択から除外する。
- [ ] Adapter情報と選択理由をLogする。
- [ ] WARP Fallbackを選択可能にする。
- [ ] Device作成HRESULTを処理する。
- [ ] Info Queue ErrorでBreakできる。
- [ ] Feature Level/Shader Model/Root Signatureを照会する。
- [ ] 必須/任意FeatureとFallbackを定義する。
- [ ] 初期化完了後にDeviceを公開する。

## 109. 理解確認問題

1. Debug LayerをDevice作成前に有効化する理由を説明してください。
2. DXGI FactoryとD3D12 Deviceの役割を説明してください。
3. Adapter index 0を固定すべきでない理由を説明してください。
4. GPU PreferenceとFeature Support確認の違いを説明してください。
5. WARPを使う目的を三つ挙げてください。
6. Feature LevelとShader Modelの違いを説明してください。
7. Required/Optional Featureを分ける理由を説明してください。
8. Agility SDKとSystem Runtimeの違いを説明してください。

## 110. 章末要点

- SDK、Header、Library、x64設定を再現可能にします。
- Debug Layer/GPU Validation/DREDはDevice生成前に設定します。
- DXGI FactoryでAdapterを列挙し、DescriptionとFeatureを評価します。
- GPU PreferenceはHintであり、必須機能を個別検証します。
- Hardwareが使えない場合のWARP Policyを用意します。
- Device作成後にInfo QueueとCapability Reportを設定します。
- Feature LevelだけでなくShader Model、Root Signature、Tier、Formatを照会します。
- 完全な初期化結果が完成してから他Subsystemへ公開します。

## 111. 公式資料

- [Direct3D 12 programming environment setup](https://learn.microsoft.com/en-us/windows/win32/direct3d12/directx-12-programming-environment-set-up)
- [Enabling the debug layer](https://learn.microsoft.com/en-us/windows/win32/direct3d12/using-d3d12-debug-layer-gpu-based-validation)
- [D3D12GetDebugInterface](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-d3d12getdebuginterface)
- [CreateDXGIFactory2](https://learn.microsoft.com/en-us/windows/win32/api/dxgi1_3/nf-dxgi1_3-createdxgifactory2)
- [IDXGIFactory6::EnumAdapterByGpuPreference](https://learn.microsoft.com/en-us/windows/win32/api/dxgi1_6/nf-dxgi1_6-idxgifactory6-enumadapterbygpupreference)
- [IDXGIFactory4::EnumWarpAdapter](https://learn.microsoft.com/en-us/windows/win32/api/dxgi1_4/nf-dxgi1_4-idxgifactory4-enumwarpadapter)
- [D3D12CreateDevice](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-d3d12createdevice)
- [ID3D12Device::CheckFeatureSupport](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12device-checkfeaturesupport)
- [DirectX 12 Agility SDK](https://devblogs.microsoft.com/directx/directx12agility/)
- [Agility SDK getting started](https://devblogs.microsoft.com/directx/gettingstarted-dx12agility/)

次章では、Direct Queue、Command Allocator、Graphics Command Listの生成・Reset・Close・Executeと、正しい所有権を実装します。
