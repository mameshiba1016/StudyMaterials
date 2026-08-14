# DirectX 11：HRESULT・COM・ComPtr・Debug Layer

この章では、DirectX Objectの寿命と失敗を安全に扱います。DirectXの不具合は、描画式より先に「失敗したHRESULTを無視した」「COM参照を漏らした」「出力引数を誤用した」「Debug Layerを見ていない」ことから起こりがちです。

## 1. COMの役割

COMはComponent Object Modelの略です。Direct3DとDXGIの多くのObjectはCOM Interfaceを通して利用します。

```text
Application owns interface pointers
 -> methods call runtime / driver implementation
 -> QueryInterface discovers another supported interface
 -> AddRef / Release control lifetime
```

## 2. Interface Pointer

```cpp
ID3D11Device* device = nullptr;
```

これは実装Classへ直接触るPointerではなく、`ID3D11Device` InterfaceのMethod Tableを通じてObjectを操作するPointerです。

## 3. IUnknown

すべてのCOM Interfaceは直接または間接に`IUnknown`を基礎とします。

```text
QueryInterface  別Interfaceを問い合わせる
AddRef          参照Countを増やす
Release         参照Countを減らす
```

## 4. QueryInterface

Objectが指定Interfaceを対応しているかRuntimeに問い合わせます。

```cpp
ID3D11Debug* debug = nullptr;
HRESULT hr = device->QueryInterface(
    __uuidof(ID3D11Debug),
    reinterpret_cast<void**>(&debug));
```

成功時は取得Pointerの参照Countが増えます。不要になったらReleaseが必要です。

## 5. Interface ID

COM InterfaceはIIDという一意の識別子を持ちます。C++型名だけでなくIIDによってInterfaceを問い合わせます。

## 6. QueryInterfaceの失敗

ObjectがInterfaceを対応しない場合は一般に`E_NOINTERFACE`を返します。OS、Runtime、Device作成Flag、Interface Versionによって利用可否が異なる場合があります。

## 7. COM ObjectのIdentity

公式COM規則では、同一Objectへ`IID_IUnknown`を問い合わせると同じ物理Pointer値が得られ、Object Identityを比較できます。別Interface Pointerの値自体が同じとは限りません。

## 8. AddRef

Interface Pointerの新しい所有Copyを作るとき、参照Countを増やします。

```cpp
ID3D11Device* copied = device;
if (copied != nullptr)
    copied->AddRef();
```

手動所有は間違えやすいため、通常はComPtrへ任せます。

## 9. Release

```cpp
if (copied != nullptr)
{
    copied->Release();
    copied = nullptr;
}
```

最後の参照が解放されるとObjectは破棄され得ます。Release後にPointerを使いません。

## 10. Reference Count値をLogicに使わない

`AddRef`や`Release`の戻り値は診断目的であり、Objectが共有されているかをGame Logicで判断する用途にしません。Runtime内部参照が存在する場合もあります。

## 11. 所有参照と借用参照

- 所有参照：寿命を延ばす責任を持ち、最後にReleaseする。
- 借用参照：所有者より短い範囲だけ使い、Releaseしない。

関数の契約でどちらかを明確にします。

## 12. COM作成関数の出力

多くの作成関数は成功時に参照Count済みのPointerを出力します。

```cpp
ID3D11Buffer* buffer = nullptr;
HRESULT hr = device->CreateBuffer(&description, nullptr, &buffer);
```

成功した`buffer`は呼び出し側がReleaseします。

## 13. 手動管理の問題

```cpp
ID3D11Buffer* buffer = nullptr;
if (FAILED(CreateBuffer(&buffer)))
    return false;

if (FAILED(CreateOtherThing()))
    return false; // bufferのReleaseを忘れる危険。
```

早期Return、例外、複数失敗経路で漏れます。

## 14. RAII

RAIIはResource Acquisition Is Initializationの略です。Object寿命をC++ Scopeへ結び付けます。

```cpp
{
    Microsoft::WRL::ComPtr<ID3D11Buffer> buffer;
    // Scope終了で保持参照をReleaseします。
}
```

## 15. ComPtr

```cpp
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

ComPtr<ID3D11Device> device;
ComPtr<ID3D11DeviceContext> context;
ComPtr<IDXGISwapChain> swapChain;
```

## 16. Default状態

Default構築されたComPtrはNullです。

```cpp
ComPtr<ID3D11Buffer> buffer;
if (!buffer)
{
    // まだObjectを保持していません。
}
```

## 17. Get

```cpp
ID3D11Device* borrowed = device.Get();
```

`Get()`は保持中Pointerを借用します。戻りPointerへReleaseを呼びません。

## 18. operator->

```cpp
device->CreateBuffer(...);
```

ComPtrは保持InterfaceのMethodをPointerのように呼べます。Null確認なしで使えば通常のNull参照と同じ危険があります。

## 19. GetAddressOf

```cpp
HRESULT hr = D3D11CreateDevice(
    nullptr,
    D3D_DRIVER_TYPE_HARDWARE,
    nullptr,
    0,
    nullptr,
    0,
    D3D11_SDK_VERSION,
    device.GetAddressOf(),
    nullptr,
    context.GetAddressOf());
```

空のComPtrへ作成APIの出力を受け取る用途です。

## 20. 既存ObjectとGetAddressOf

既にObjectを保持するComPtrへ同じStorage Addressを渡して上書きすると、古い参照を漏らす危険があります。出力前に空である契約を守ります。

## 21. ReleaseAndGetAddressOf

```cpp
HRESULT hr = RecreateResource(
    resource.ReleaseAndGetAddressOf());
```

古い参照をReleaseしてから出力Addressを取得します。再作成失敗時には旧Resourceが失われるため、強い例外安全性が必要なら一時ComPtrへ作って成功後にSwapします。

## 22. 安全な再作成

```cpp
ComPtr<ID3D11Texture2D> newTexture;
HRESULT hr = device->CreateTexture2D(
    &description, nullptr, newTexture.GetAddressOf());

if (FAILED(hr))
    return false;

texture.Swap(newTexture); // 成功後に置換します。
```

## 23. Reset

```cpp
buffer.Reset();
```

保持参照をReleaseしてNullへ戻します。

## 24. Attach

```cpp
ComPtr<IUnknown> owner;
owner.Attach(rawPointer);
```

既に呼び出し側へ所有参照として渡された生Pointerを、追加のAddRefなしでComPtrへ移します。契約を誤ると二重Releaseまたは早期破棄になります。

## 25. Detach

```cpp
IUnknown* transferred = owner.Detach();
```

ComPtrからPointerを外し、Release責任を呼び出し側へ移します。受け手が必ず所有を引き継ぐ場合だけ使います。

## 26. Copy

```cpp
ComPtr<ID3D11Device> another = device;
```

ComPtr Copyは参照を増やし、両方が独立して保持します。

## 27. Move

```cpp
ComPtr<ID3D11Device> moved = std::move(device);
```

所有を移し、元のComPtrは通常Nullになります。AddRefを増やさず所有権を転送します。

## 28. As

```cpp
ComPtr<ID3D11Debug> debug;
HRESULT hr = device.As(&debug);
```

`ComPtr::As`はQueryInterfaceを型安全に扱いやすくします。対応していなければHRESULTが失敗します。

## 29. As成功時の寿命

`debug`は同じCOM Objectの別Interfaceを所有します。`device`をResetしても`debug`参照が残る間、基盤Objectが生存する可能性があります。

## 30. 参照Cycle

二つのCOM Objectが互いを所有参照すると参照Countが0にならず漏れます。Direct3D Objectの通常使用だけで自作Cycleを作らないよう、CallbackやOwner関係を確認します。

## 31. Device Childの寿命

Buffer、Texture、ShaderなどDevice ChildがDeviceへの内部参照を持つ場合があります。Device ComPtrをResetしてもChildが残ればDeviceがLiveとして報告され得ます。

## 32. 解放順

```text
Unbind context state
 -> release views and resources
 -> release swap chain
 -> release context
 -> report live objects with debug interface
 -> release debug interface and device
```

正確な所有関係に合わせます。

## 33. ContextがResourceを保持する

PipelineへBindingされたResourceはContextから参照されます。終了診断前に`ClearState`と必要なFlushを行うことで、Application側参照とContext参照を切り分けます。

```cpp
context->ClearState();
context->Flush();
```

## 34. HRESULT

`HRESULT`はWindows APIで成功または失敗と追加情報を表す32bit値です。

```cpp
HRESULT hr = S_OK;

if (SUCCEEDED(hr)) { /* 成功 */ }
if (FAILED(hr))    { /* 失敗 */ }
```

## 35. S_OKだけではない成功

成功Codeは`S_OK`だけとは限りません。したがって`hr == S_OK`ではなく`SUCCEEDED(hr)`を使います。

## 36. S_FALSE

`S_FALSE`は名前にFALSEを含みますが成功範囲のHRESULTです。意味はAPIごとの契約を確認します。

## 37. 代表的なError

```text
E_INVALIDARG                  引数が不正
E_OUTOFMEMORY                 Memory確保失敗
E_NOINTERFACE                 Interface非対応
DXGI_ERROR_DEVICE_REMOVED     Deviceが失われた
DXGI_ERROR_DEVICE_RESET       DeviceがResetされた
DXGI_ERROR_SDK_COMPONENT_MISSING Debug Component不足
```

## 38. HRESULTの内部構造

HRESULTにはSeverity、Facility、CodeがBit Fieldとして含まれます。数値を自力で分解する前にMacroと公式Error一覧を使います。

## 39. ErrorをHex表示する

```cpp
void LogHResult(const char* operation, HRESULT hr)
{
    std::fprintf(stderr, "%s failed: 0x%08lX\n",
                 operation, static_cast<unsigned long>(hr));
}
```

API名と数値を必ず残します。

## 40. FormatMessage

SystemがMessage Textを提供するHRESULTなら`FormatMessageW`で説明を得られます。ただしすべてのDXGI Errorに期待する文章があるとは限りません。

## 41. Error Context

```cpp
struct GraphicsError final
{
    HRESULT code{};
    const char* operation{};
    const char* file{};
    int line{};
};
```

失敗値だけでなく、どのResource、Format、Size、Feature LevelだったかをLogします。

## 42. Return型

```cpp
struct GraphicsResult final
{
    HRESULT hresult{S_OK};
    const char* operation{};

    explicit operator bool() const
    {
        return SUCCEEDED(hresult);
    }
};
```

Exceptionを使わないProjectでもErrorを失いません。

## 43. Exception Helper

```cpp
class HResultException final : public std::runtime_error
{
public:
    HResultException(HRESULT code, const char* operation)
        : std::runtime_error(operation), code_(code) {}

    HRESULT Code() const noexcept { return code_; }

private:
    HRESULT code_{};
};
```

Project全体でException方針を統一します。Destructorから投げません。

## 44. Check Helper

```cpp
void ThrowIfFailed(HRESULT hr, const char* operation)
{
    if (FAILED(hr))
        throw HResultException(hr, operation);
}
```

呼び出し名を失わないようにします。

## 45. Source Location

C++20の`std::source_location`でFileとLineを自動記録できます。Helperの内部位置ではなく呼び出し位置を保存します。

## 46. Debug Layer

Direct3D 11 Debug LayerはAPI使用、Pipeline State、Resource Bindingなどの誤りや警告をDebug Outputへ報告します。

## 47. Debug Device Flag

```cpp
UINT creationFlags = 0;
#if defined(_DEBUG)
creationFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
```

Release Buildでの性能と配布環境を考え、条件付きにします。

## 48. Debug Layer不足

必要Componentがない環境ではDebug Flag付きDevice作成が`DXGI_ERROR_SDK_COMPONENT_MISSING`で失敗し得ます。GPU非対応と誤診せず、Install状態を報告します。

## 49. Debug Messageの読み方

MessageにはSeverity、Category、ID、説明があります。最初に出たErrorから直します。後続Errorは一つ目の不正Stateから連鎖している場合があります。

## 50. Info Queue

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

## 51. WarningでBreakするか

学習中はWarningも調査しますが、意図した一部のWarningで毎回停止すると作業不能になります。無視する前に原因を理解し、Message ID単位で局所Filterします。

## 52. Message Filter

大量の既知MessageをFilterできますが、広いCategoryを無条件で消さず、理由と対象IDを記録します。

## 53. ID3D11Debug

```cpp
ComPtr<ID3D11Debug> debug;
HRESULT hr = device.As(&debug);
```

Debug Layerが有効なDeviceからDebug Interfaceを取得します。

## 54. Live Object Report

```cpp
debug->ReportLiveDeviceObjects(
    D3D11_RLDO_DETAIL | D3D11_RLDO_IGNORE_INTERNAL);
```

終了時に残っているDevice Objectを報告し、Resource漏れを調べます。

## 55. Reportの時点

ApplicationのResource ComPtrがまだ生きている時点でReportすれば、当然Liveとして出ます。所有Objectを明示的にResetし、Context StateをClearした後に報告します。

## 56. Debug Interface自身

`ID3D11Debug`もDeviceを参照するため、報告内容と解放順を理解します。Report後にDebug InterfaceとDeviceを解放します。

## 57. Debug Name

```cpp
void SetDebugName(ID3D11DeviceChild* object, const char* name)
{
#if defined(_DEBUG)
    if (object == nullptr || name == nullptr)
        return;

    object->SetPrivateData(
        WKPDID_D3DDebugObjectName,
        static_cast<UINT>(std::strlen(name)),
        name);
#endif
}
```

Graphics DebuggerとLive Object Reportで識別しやすくなります。

## 58. DXGI Debug

DXGI側にもDebug InterfaceとLive Object報告があります。D3D11 ObjectだけでなくFactory、Adapter、Swap Chainの漏れも対象にします。

## 59. Device Removed Reason

```cpp
HRESULT reason = device->GetDeviceRemovedReason();
```

PresentやResource操作がDevice Removedを返したとき、追加理由を記録します。

## 60. `DXGI_ERROR_DEVICE_HUNG`

不正なGPU Command、長時間ShaderなどでDriverがGPU処理を停止した場合に関係します。直前のDebug Message、Frame Capture、Shader変更を確認します。

## 61. `DXGI_ERROR_DEVICE_RESET`

不正CommandまたはSystem側Resetなどが考えられます。Deviceと全Device依存Resourceの再作成経路が必要です。

## 62. `DXGI_ERROR_DRIVER_INTERNAL_ERROR`

Driver内部Errorです。Applicationの不正API使用が引き金の可能性もあるため、Debug Layerと再現手順を確認します。

## 63. Object名の規則

```text
SceneColor.Texture
SceneColor.RTV
SceneDepth.Texture
SceneDepth.DSV
Character.VertexBuffer
Lighting.PixelShader
```

ResourceとViewを名前で区別します。

## 64. COM初期化との関係

一部のWindows Componentは`CoInitializeEx`を必要とします。D3D11 Device作成そのものと、WICなど別COM APIのApartment初期化を混同しません。

## 65. CoInitializeEx

```cpp
HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
```

成功した初期化呼び出しに対応して`CoUninitialize`します。`RPC_E_CHANGED_MODE`など既存Apartmentとの競合を処理します。

## 66. ThreadとCOM Apartment

COMを使うThreadごとに必要な初期化とApartment Modelを決めます。別ThreadへInterfaceを渡す規則は利用ComponentのThreading Modelを確認します。

## 67. Direct3D Thread Safety

Device作成MethodとDevice Context CommandにはThread安全性の性質が異なります。ComPtrが参照Countを安全にしても、Object Methodの同時呼び出しまで自動で安全になるわけではありません。

## 68. Weak Pointerの代用をしない

`QueryInterface`後すぐReleaseしてPointerだけ保持するのは弱参照ではありません。Objectが破棄されればDangling Pointerになります。

## 69. 生PointerをMemberへ保存する場合

借用Pointerなら所有者が必ず長生きする構造を型とCommentで明示します。可能ならID、Reference、ComPtrのいずれかで意図を表します。

## 70. よくある失敗：GetしたPointerをRelease

```cpp
ID3D11Device* raw = device.Get();
raw->Release(); // ComPtrの所有参照を壊すので誤り。
```

借用PointerへReleaseしません。

## 71. よくある失敗：GetAddressOfへ上書き

既に保持中のComPtrへ作成結果を上書きし、旧Objectを漏らします。一時ComPtrまたは適切なReleaseAndGetAddressOfを使います。

## 72. よくある失敗：Attachの誤用

借用PointerをAttachすると、ComPtrがReleaseして他所有者の参照を壊します。Attachは所有参照の移譲契約がある場合だけ使います。

## 73. よくある失敗：HRESULTをboolへ変換

HRESULTの0が成功であるため、`if (hr)`では意味が逆転する場合があります。必ず`SUCCEEDED`または`FAILED`を使います。

## 74. よくある失敗：Debug Messageを放置

画面が表示されていてもResource Hazardや無効StateのWarningが性能低下・将来Crashの原因になります。意図を説明できないWarningを残しません。

## 75. 参照Countテスト

- ComPtr Copy後に片方をResetしてもObjectを使える。
- Move後に元ComPtrがNullになる。
- QueryInterface成功後に元InterfaceをResetしても取得Interfaceを使える。
- AttachとDetachでRelease責任が一度だけ移る。
- 全Resource解放後にLive Objectが残らない。

## 76. HRESULTテスト

- `S_OK`と`S_FALSE`を成功として扱う。
- `E_INVALIDARG`を失敗として扱う。
- API名、HRESULT、File、LineをLogする。
- 初期化失敗後に次段階へ進まない。
- Device Removed Reasonを別に記録する。

## 77. Debug Layerテスト

- Debug BuildでLayerが有効になる。
- 不正Bindingを意図的に作りMessageを確認する。
- Error SeverityでDebugger Breakする。
- ResourceへDebug Nameが表示される。
- 終了時にLive Object Reportを出す。
- Debug Component不足を明確に報告する。

## 78. Shutdown確認表

- [ ] Game Loopを停止した。
- [ ] GPUへ新しいCommandを出さない。
- [ ] Context StateをClearした。
- [ ] Scene ResourceとViewを解放した。
- [ ] Swap Chainを解放した。
- [ ] Contextを解放した。
- [ ] Live Objectを報告した。
- [ ] Debug InterfaceとDeviceを解放した。
- [ ] 必要なCOM Apartmentを終了した。

## 79. この章の要点

- COM Interfaceは`QueryInterface`、`AddRef`、`Release`の契約を持ちます。
- 所有参照と借用参照を区別し、ComPtrでRAII管理します。
- `Get`、`GetAddressOf`、`ReleaseAndGetAddressOf`、`Attach`、`Detach`の所有権を理解します。
- HRESULTは`SUCCEEDED`と`FAILED`で評価し、API名と設定値をLogします。
- Debug Layer、Info Queue、Debug Nameを開発開始時から有効にします。
- Context Bindingを解除し、Live Object Reportで参照漏れを確認します。
- ComPtrはMethodのThread Safetyまで保証しません。

## 80. 公式資料

- [COM technical overview](https://learn.microsoft.com/en-us/windows/win32/com/com-technical-overview)
- [Rules for implementing QueryInterface](https://learn.microsoft.com/en-us/windows/win32/com/rules-for-implementing-queryinterface)
- [Programming DirectX with COM](https://learn.microsoft.com/en-us/windows/win32/prog-dx-with-com)
- [IUnknown AddRef](https://learn.microsoft.com/en-us/windows/win32/api/unknwn/nf-unknwn-iunknown-addref)
- [WRL ComPtr class](https://learn.microsoft.com/en-us/cpp/cppcx/wrl/comptr-class)
- [ID3D11Debug](https://learn.microsoft.com/en-us/windows/win32/api/d3d11sdklayers/nn-d3d11sdklayers-id3d11debug)
- [Using the Direct3D 11 debug layer](https://learn.microsoft.com/en-us/windows/win32/direct3d11/using-the-debug-layer-to-test-apps)

次章では、DXGI FactoryからAdapterとOutputを列挙し、複数GPU環境、VRAM情報、GPU選択、Software Adapterを扱います。
