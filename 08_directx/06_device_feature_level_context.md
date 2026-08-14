# DirectX 11：Device・Feature Level・Device Context

この章では、選択したAdapterからDirect3D 11 DeviceとDevice Contextを作成します。DeviceはResourceを作り能力を問い合わせ、ContextはPipeline Stateを設定してCommandを発行します。Feature LevelはGPUの速さではなく、保証される機能集合です。

## 1. 三つの中心値

```cpp
Microsoft::WRL::ComPtr<ID3D11Device> device;
Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
D3D_FEATURE_LEVEL featureLevel{};
```

- Device：Resource FactoryとCapability Query。
- Context：State設定とGPU Command発行。
- Feature Level：作成されたDeviceで保証される機能段階。

## 2. Device作成の入力と出力

```text
Input
  Adapter or driver type
  creation flags
  requested feature levels
  SDK version
Output
  device
  selected feature level
  immediate context
```

## 3. D3D11CreateDevice

```cpp
HRESULT D3D11CreateDevice(
    IDXGIAdapter* adapter,
    D3D_DRIVER_TYPE driverType,
    HMODULE software,
    UINT flags,
    const D3D_FEATURE_LEVEL* featureLevels,
    UINT featureLevelCount,
    UINT sdkVersion,
    ID3D11Device** device,
    D3D_FEATURE_LEVEL* selectedFeatureLevel,
    ID3D11DeviceContext** immediateContext);
```

宣言の意味を理解するための表記です。実際はSDK Headerの宣言を使います。

## 4. 明示Adapter経路

```cpp
HRESULT hr = D3D11CreateDevice(
    adapter.Get(),
    D3D_DRIVER_TYPE_UNKNOWN,
    nullptr,
    creationFlags,
    requestedLevels.data(),
    static_cast<UINT>(requestedLevels.size()),
    D3D11_SDK_VERSION,
    device.GetAddressOf(),
    &featureLevel,
    context.GetAddressOf());
```

Adapterを渡す場合、Driver Typeは`UNKNOWN`です。

## 5. Default Adapter経路

```cpp
HRESULT hr = D3D11CreateDevice(
    nullptr,
    D3D_DRIVER_TYPE_HARDWARE,
    nullptr,
    creationFlags,
    requestedLevels.data(),
    static_cast<UINT>(requestedLevels.size()),
    D3D11_SDK_VERSION,
    device.GetAddressOf(),
    &featureLevel,
    context.GetAddressOf());
```

選択PolicyをOS既定へ任せる簡潔な方法です。

## 6. WARP経路

```cpp
HRESULT hr = D3D11CreateDevice(
    nullptr,
    D3D_DRIVER_TYPE_WARP,
    nullptr,
    creationFlags,
    requestedLevels.data(),
    static_cast<UINT>(requestedLevels.size()),
    D3D11_SDK_VERSION,
    device.GetAddressOf(),
    &featureLevel,
    context.GetAddressOf());
```

Hardware失敗時に仕様で許可するSoftware Fallbackです。

## 7. Reference Device

Reference Rasterizerは正確さ検証向けで非常に低速です。通常GameのFallbackとして使いません。

## 8. Creation Flags

```cpp
UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;

#if defined(_DEBUG)
flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
```

必要FlagだけをBit ORします。

## 9. BGRA Support

`D3D11_CREATE_DEVICE_BGRA_SUPPORT`はDirect2DなどBGRA形式との相互運用で必要になります。将来UI統合を考える場合に有効化します。

## 10. SINGLETHREADED Flag

`D3D11_CREATE_DEVICE_SINGLETHREADED`はThreading保証を制限して最適化するFlagです。Resource作成を複数Threadから行う可能性があるなら使いません。

## 11. PREVENT_INTERNAL_THREADING_OPTIMIZATIONS

Runtime内部Thread最適化を抑える特殊Flagです。通常用途で性能改善を期待して有効化しません。

## 12. Feature Level要求配列

```cpp
constexpr std::array requestedLevels{
    D3D_FEATURE_LEVEL_11_1,
    D3D_FEATURE_LEVEL_11_0,
    D3D_FEATURE_LEVEL_10_1,
    D3D_FEATURE_LEVEL_10_0
};
```

上位から下位の順に並べます。

## 13. 選ばれるLevel

Device作成は要求配列のうちAdapterが対応する最上位を返します。Applicationが対応できない低いLevelを配列に入れません。

## 14. Feature Levelは性能ではない

同じ11_0でもGPU性能、Memory帯域、Driver品質は大きく異なります。Feature Levelは機能保証でありfps保証ではありません。

## 15. API Versionとの違い

Direct3D 11 APIを使いながらFeature Level 10_0 Deviceを作ることができます。API入口とGPU機能段階は別です。

## 16. Shader Modelとの関係

Feature Levelは利用可能なShader StageとShader Profileへ影響します。高いProfileでCompileしたShaderは低いFeature Levelで使えません。

## 17. 11_1要求のFallback

Direct3D 11.1 Runtimeがない環境で要求配列に11_1を含めると`E_INVALIDARG`になり得ます。その場合は11_1を除いた配列で再試行します。

```cpp
HRESULT TryCreateWithFallback(...)
{
    HRESULT hr = TryCreate(levelsWith11_1);
    if (hr == E_INVALIDARG)
        hr = TryCreate(levelsWithout11_1);
    return hr;
}
```

## 18. E_INVALIDARGを無条件Fallbackしない

別の引数不正でも同じErrorが返り得ます。どの呼び出し条件でFallback対象にするか限定し、両方の結果をLogします。

## 19. Feature Level表示

```cpp
const char* FeatureLevelName(D3D_FEATURE_LEVEL level)
{
    switch (level)
    {
    case D3D_FEATURE_LEVEL_11_1: return "11_1";
    case D3D_FEATURE_LEVEL_11_0: return "11_0";
    case D3D_FEATURE_LEVEL_10_1: return "10_1";
    case D3D_FEATURE_LEVEL_10_0: return "10_0";
    default: return "unknown";
    }
}
```

## 20. 最低要件

```cpp
bool MeetsMinimum(D3D_FEATURE_LEVEL actual,
                  D3D_FEATURE_LEVEL minimum)
{
    return static_cast<unsigned>(actual) >=
           static_cast<unsigned>(minimum);
}
```

列挙値の順序に依存する場合はSDK定義を確認し、明示Switchでも表現できます。

## 21. Deviceの責務

- Buffer、Texture、Viewの作成。
- Shader Objectの作成。
- Blend、Rasterizer、Depth Stateの作成。
- FormatとFeatureの対応確認。
- Device Removed Reason取得。

## 22. Deviceは描画しない

`device->Draw`というMethodはありません。DrawはContextへ発行します。

## 23. Device Child

Buffer、Texture、Shader、Stateなど多くは`ID3D11DeviceChild`を基礎とします。作成元DeviceやPrivate Dataへアクセスできます。

## 24. GetDevice

```cpp
ComPtr<ID3D11Device> owningDevice;
buffer->GetDevice(owningDevice.GetAddressOf());
```

取得参照をComPtrで管理します。毎Frame無意味に逆引きしません。

## 25. Device Contextの責務

- Pipeline State Binding。
- DrawとDispatch。
- Resource Copy。
- Dynamic ResourceのMapとUnmap。
- QueryのBegin、End、GetData。
- Command List実行。

## 26. Immediate Context

Deviceごとに一つのImmediate Contextがあり、Driverへ直接Commandを送る順序を形成します。

## 27. GetImmediateContext

```cpp
ComPtr<ID3D11DeviceContext> anotherReference;
device->GetImmediateContext(anotherReference.GetAddressOf());
```

作成時に取得したContextと同じImmediate Contextへの別参照です。

## 28. State Machine

Contextは現在のShader、Buffer、Texture、Sampler、Render Target、Viewport、Blendなどを保持するState Machineです。

```cpp
context->VSSetShader(vertexShader.Get(), nullptr, 0);
context->PSSetShader(pixelShader.Get(), nullptr, 0);
context->Draw(vertexCount, 0);
```

## 29. Draw時点のState

`Draw`はその時点でContextへBindingされている全Pipeline Stateを使います。前のDrawのStateが残るため、Pass境界で必要Stateを明示します。

## 30. Context Stateが参照を持つ

BindingしたResourceやStateはContextから参照されます。Application側ComPtrをResetしてもContextが保持している間はLiveです。

## 31. ClearState

```cpp
context->ClearState();
```

全Pipeline Stateを既定状態へ戻し、Binding参照を解放します。毎Draw呼ぶものではなく、終了や大きなReset境界で使います。

## 32. Flush

```cpp
context->Flush();
```

保留CommandをDriverへ送ります。GPU完了を保証する一般的Waitではありません。毎Frame乱用すると性能を損ないます。

## 33. Deferred Context

Deferred ContextはWorker ThreadでCommandを記録し、Command Listを作ります。

```cpp
ComPtr<ID3D11DeviceContext> deferred;
HRESULT hr = device->CreateDeferredContext(
    0, deferred.GetAddressOf());
```

## 34. Command List

```cpp
ComPtr<ID3D11CommandList> commandList;
HRESULT hr = deferred->FinishCommandList(
    FALSE, commandList.GetAddressOf());

context->ExecuteCommandList(commandList.Get(), FALSE);
```

## 35. Deferred ContextはStateを継承しない

Immediate Contextの現在Stateを自動で引き継ぎません。Command記録に必要なStateをDeferred Contextへ設定します。

## 36. Deferred Contextの制約

すべての操作がImmediate Contextと同じように使えるわけではありません。Map、Query、Command Listの制約をAPIごとに確認します。

## 37. 並列化は無料ではない

Command List構築、State設定重複、実行Overhead、Driver実装により、小規模Sceneでは遅くなる場合があります。Profilerで判断します。

## 38. ContextのThread Safety

ImmediateでもDeferredでも、一つのContextを同時に複数Threadから呼びません。Contextごとに利用Threadを一つにします。

## 39. DeviceのThread Safety

公式資料ではDeviceのResource作成MethodはFree-threadedです。ただし自作Resource CacheやAllocatorのThread Safetyは別途必要です。

## 40. Device ChildのThread Safety

Device Child由来InterfaceにもThreading上の性質がありますが、GPU使用中ResourceのData RaceやContext順序まで自動で解決するわけではありません。

## 41. ID3D11Multithread

```cpp
ComPtr<ID3D11Multithread> multithread;
if (SUCCEEDED(context.As(&multithread)))
{
    multithread->SetMultithreadProtected(TRUE);
}
```

Immediate Contextの複数Thread利用を内部Lockで保護できます。

## 42. Multithread保護のCost

保護を有効にすると各Immediate Context呼び出しへ同期Overheadが加わります。可能ならRender Thread一つにCommand発行を集約します。

## 43. EnterとLeave

`ID3D11Multithread::Enter`と`Leave`でCritical Sectionを明示できます。例外や早期ReturnでもLeaveするRAII Guardが必要です。

## 44. Render Thread方針

```text
Worker threads
 -> load CPU data
 -> build render packets
Render thread
 -> create/submit needed GPU commands
 -> present
```

最初はこの単純な所有権が安全です。

## 45. CheckFeatureSupport

Feature Levelだけで全Optional機能を推測せず、Deviceへ問い合わせます。

```cpp
D3D11_FEATURE_DATA_THREADING threading{};
HRESULT hr = device->CheckFeatureSupport(
    D3D11_FEATURE_THREADING,
    &threading,
    sizeof(threading));
```

## 46. 構造体Size

`CheckFeatureSupport`へFeatureと対応しない構造体やSizeを渡すと`E_INVALIDARG`になります。`sizeof(variable)`を使います。

## 47. Threading Capability

`D3D11_FEATURE_DATA_THREADING`からConcurrent Resource CreationやCommand List対応情報を確認できます。Driverごとの結果をLogします。

## 48. CheckFormatSupport

```cpp
UINT support = 0;
HRESULT hr = device->CheckFormatSupport(
    DXGI_FORMAT_R8G8B8A8_UNORM,
    &support);

const bool texture2D =
    (support & D3D11_FORMAT_SUPPORT_TEXTURE2D) != 0;
const bool renderTarget =
    (support & D3D11_FORMAT_SUPPORT_RENDER_TARGET) != 0;
```

## 49. Format対応は用途別

FormatがTextureとして使えてもRender Target、Blend、Depth、Typed UAVなど全用途に対応するとは限りません。必要Flagを個別に確認します。

## 50. Multisample Quality

```cpp
UINT qualityLevels = 0;
HRESULT hr = device->CheckMultisampleQualityLevels(
    DXGI_FORMAT_R8G8B8A8_UNORM,
    4,
    &qualityLevels);

const bool supports4x = SUCCEEDED(hr) && qualityLevels > 0;
```

Sample CountとFormatの組み合わせで問い合わせます。

## 51. Options Query

`D3D11_FEATURE_D3D11_OPTIONS`系列からOptional機能を確認できます。Interface VersionだけでCapabilityを決めません。

## 52. Capability Snapshot

```cpp
struct D3D11Capabilities final
{
    D3D_FEATURE_LEVEL featureLevel{};
    bool concurrentResourceCreation{};
    bool commandLists{};
    bool rgba8RenderTarget{};
    bool depth24Stencil8{};
    bool multisample4x{};
    UINT multisample4xQualityLevels{};
};
```

起動時に一度収集し、Renderer各所で毎回Queryしません。

## 53. CapabilityからPathを選ぶ

```cpp
enum class RenderPath
{
    Standard,
    Reduced,
    Unsupported
};
```

必要機能がなければFallback Assetや低品質Pathを使うか、明確に起動を中止します。

## 54. Device Interface Version

```cpp
ComPtr<ID3D11Device5> device5;
if (SUCCEEDED(device.As(&device5)))
{
    // 新しいDevice Interface機能を利用可能。
}
```

基礎Deviceを失わず追加Interfaceを取得します。

## 55. Context Interface Version

```cpp
ComPtr<ID3D11DeviceContext4> context4;
const bool available = SUCCEEDED(context.As(&context4));
```

新Interface取得成功と、個々のHardware機能対応を必要に応じて分けて確認します。

## 56. Device Removed Event

新しいDevice InterfaceではDevice RemovalのEvent通知を登録できる場合があります。PresentのHRESULT確認と組み合わせます。

## 57. Device Removed時の範囲

Device、Context、Swap Chain、全Device Child Resourceは再作成対象です。Texture一つだけ作り直して復旧できません。

## 58. Renderer初期化結果

```cpp
struct DeviceCreateResult final
{
    ComPtr<ID3D11Device> device{};
    ComPtr<ID3D11DeviceContext> immediateContext{};
    D3D_FEATURE_LEVEL featureLevel{};
    D3D11Capabilities capabilities{};
    HRESULT hresult{S_OK};
    bool warp{};
};
```

成功Objectと診断情報をまとめます。

## 59. 強い初期化保証

Memberへ直接順番に書かず、一時ResultへDevice、Context、Capabilityを作り、すべて成功してからRendererへMoveします。

## 60. Debug Name

Device Context自身や作成Resourceへ用途名を付けます。Device作成直後にDebug LayerとInfo Queueも構成します。

## 61. 起動Log

- Adapter NameとLUID。
- Driver Type。
- WARP利用有無。
- Creation Flags。
- 選択Feature Level。
- Device / Context Interface Version。
- Threading Capability。
- 必須Format対応。
- MSAA Quality Level。

## 62. よくある失敗：NULL Feature Levels

11_1を期待しながら配列をNullにします。公式仕様ではNull既定配列に11_1が含まれない場合があるため、要求Levelを明示します。

## 63. よくある失敗：低Feature Levelを受け入れる

Device作成成功だけを見て、後からShader作成が失敗します。作成直後に最低要件を検証します。

## 64. よくある失敗：Feature Levelを性能Tier扱い

同じLevelの低性能GPUで高品質設定を選びます。性能TierはBenchmark、Adapter情報、設定、Profilerから別に判断します。

## 65. よくある失敗：Contextを複数Thread共有

ComPtr Copyできるため安全だと思い同時呼び出しします。Interface参照寿命とMethod Thread Safetyは別です。

## 66. よくある失敗：FlushでGPU完了を待つ

FlushはCommand送信を促しますが一般的なGPU完了Waitではありません。Queryや同期設計を用途に応じて使います。

## 67. よくある失敗：Optional機能を推測

Feature LevelやGPU名から決めつけます。`CheckFeatureSupport`、`CheckFormatSupport`、`CheckMultisampleQualityLevels`を使います。

## 68. Device作成テスト

- 明示AdapterでDriver Type UNKNOWNを使う。
- Default Hardware経路を作れる。
- 11_1のE_INVALIDARG Fallbackを試す。
- 最低Feature Level未満を拒否する。
- Hardware失敗時だけWARPを試す。
- Debug Layer不足を明確に報告する。

## 69. Capabilityテスト

- Featureと構造体Sizeを一致させる。
- 必須Formatの全用途Flagを確認する。
- MSAA Quality 0を非対応と扱う。
- Optional Interface非対応でCrashしない。
- Capability SnapshotをDebug表示する。

## 70. Threadingテスト

- Immediate ContextをRender Threadだけから呼ぶ。
- Deferred Contextを所有Workerだけから呼ぶ。
- Command Listの実行順を固定する。
- Multithread保護ON/OFFのCostを測る。
- Scene終了時にWorker Command生成を止める。

## 71. Shutdownテスト

- Contextへ新Commandを出さない。
- Deferred Command Listを解放する。
- `ClearState`でBinding参照を外す。
- Scene Resourceを解放する。
- Contextを解放する。
- Live Object Report後にDeviceを解放する。

## 72. 完成確認表

- [ ] Device、Context、Feature Levelの役割を説明できる。
- [ ] 明示AdapterとDefault Adapterの引数を区別できる。
- [ ] Feature Level要求配列を上位順に定義する。
- [ ] 11_1 RuntimeのFallbackを安全に行う。
- [ ] 最低Feature Levelを作成直後に検証する。
- [ ] Optional機能をAPIでQueryする。
- [ ] Immediate Contextを一Threadへ所有させる。
- [ ] Deferred ContextのStateと制約を理解する。
- [ ] Capability Snapshotを作りLogできる。
- [ ] Device Removed時の全再作成範囲を理解する。

## 73. この章の要点

- DeviceはResource作成と能力確認、ContextはState設定とCommand発行を担当します。
- Feature Levelは保証機能であり性能ではありません。
- 要求配列から対応する最上位Levelが選ばれます。
- 11_1を要求する場合はRuntime非対応のFallbackを考慮します。
- Optional機能とFormat用途はDevice Queryで確認します。
- Immediate Contextは基本的にRender Thread一つから使います。
- Multithread保護は安全性と引き換えに呼び出しCostを増やします。
- CapabilityをSnapshot化し、Renderer Path選択と診断へ使います。

## 74. 公式資料

- [D3D11CreateDevice](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-d3d11createdevice)
- [Introduction to a Direct3D 11 device](https://learn.microsoft.com/en-us/windows/win32/direct3d11/overviews-direct3d-11-devices-intro)
- [Direct3D feature levels](https://learn.microsoft.com/en-us/windows/win32/direct3d11/overviews-direct3d-11-devices-downlevel-intro)
- [D3D feature level enumeration](https://learn.microsoft.com/en-us/windows/win32/api/d3dcommon/ne-d3dcommon-d3d_feature_level)
- [CheckFeatureSupport](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11device-checkfeaturesupport)
- [Checking hardware feature support](https://learn.microsoft.com/en-us/windows/win32/direct3ddxgi/checking-hardware-feature-support)
- [ID3D11Multithread](https://learn.microsoft.com/en-us/windows/win32/api/d3d11_4/nn-d3d11_4-id3d11multithread)

次章では、Win32 WindowとDeviceをDXGI Swap Chainへ接続し、Flip Model、Buffer Count、Present、VSync、Tearing、Frame Latencyを扱います。
