# DirectX 11：Texture・WIC・Shader Resource View

この章では、PNGやJPEGなどの画像FileをWindows Imaging Component（WIC）でDecodeし、Direct3D 11のTexture2Dへ格納し、Shader Resource View（SRV）を通してPixel Shaderから読み取るまでを学びます。File Format、Pixel Format、Row Pitch、Color Space、Alpha、Resource/View、非同期読込、Cache、Fallback、Binding競合を分離して理解します。

## 1. 最初に全体の流れを見る

```text
image file bytes
-> WIC decoder
-> decoded frame
-> pixel format conversion
-> CPU pixel buffer + metadata
-> ID3D11Texture2D
-> ID3D11ShaderResourceView
-> PSSetShaderResources
-> Texture2D.Sample in HLSL
```

File DecodeとGPU Resource作成は別工程です。

## 2. Textureとは

Textureは1次元、2次元、3次元、配列、Cubeなどの形を持つGPU Resourceです。本章では一般的な画像用`ID3D11Texture2D`を中心にします。

## 3. 画像FileとGPU Textureは同じではない

PNGやJPEGは保存・転送用に圧縮されたFile形式です。GPUはそのFile Byte列を通常そのままSampleせず、Decode後のPixel DataまたはGPU向け圧縮形式をTexture Resourceへ配置します。

## 4. WICとは

Windows Imaging Componentは画像Codec、Metadata、Pixel Format変換、ScaleなどをCOM Interfaceとして提供するWindows機能です。

## 5. WICが扱いやすい形式

環境とCodecに依存しますが、PNG、JPEG、BMP、TIFF、GIFなどの一般画像をDecodeできます。DDSなどGPU Texture向けContainerは別Loaderを使う設計が一般的です。

## 6. WICとDirect3Dの責任分担

```text
WIC       : file decode, frame selection, pixel conversion
Direct3D  : GPU memory, texture resource, view, shader binding
```

WIC ObjectをShaderへ直接Bindingすることはできません。

## 7. COM初期化

WICはCOMを使用します。ThreadがCOMを使う前に、そのThreadでApartment Modelを決めて初期化します。

```cpp
HRESULT hr = CoInitializeEx(
    nullptr,
    COINIT_MULTITHREADED);
```

## 8. RPC_E_CHANGED_MODE

同じThreadですでに別Apartment ModelでCOMが初期化されていると、Model変更はできません。Library内で無条件にModelを決めず、Application全体のThread方針を統一します。

## 9. CoUninitialize

`CoInitializeEx`が成功した呼出しに対応して同じThreadで`CoUninitialize`します。Resource Loaderの寿命よりThread COM寿命を短くしません。

## 10. WIC Factory

```cpp
Microsoft::WRL::ComPtr<IWICImagingFactory> wicFactory;

ThrowIfFailed(CoCreateInstance(
    CLSID_WICImagingFactory,
    nullptr,
    CLSCTX_INPROC_SERVER,
    IID_PPV_ARGS(wicFactory.GetAddressOf())));
```

利用可能SDKとOSを対象に、必要なら新しいFactory Interfaceも検討します。

## 11. Factoryを毎Texture作らない

WIC FactoryはLoader Subsystemで再利用します。画像一枚ごとにCOM Object全体を初期化し直す設計は避けます。

## 12. Decoder作成

```cpp
Microsoft::WRL::ComPtr<IWICBitmapDecoder> decoder;

ThrowIfFailed(wicFactory->CreateDecoderFromFilename(
    path.c_str(),
    nullptr,
    GENERIC_READ,
    WICDecodeMetadataCacheOnDemand,
    decoder.GetAddressOf()));
```

## 13. File Path

APIはWide文字列Pathを受け取ります。相対PathをCurrent Working Directoryへ依存させず、Asset Rootから正規化したPathを使います。

## 14. Pathの正規化

```text
resolve asset root
-> normalize separators
-> remove redundant components
-> enforce allowed directory
-> canonical cache key
```

同じFileが異なる表記で二重Loadされるのを防ぎます。

## 15. Decoder Cache Option

`WICDecodeMetadataCacheOnDemand`は必要時にMetadataを読み、`WICDecodeMetadataCacheOnLoad`は作成時に読み込みます。File Handle寿命とLoad Patternを考慮します。

## 16. Frame数

```cpp
UINT frameCount = 0;
ThrowIfFailed(decoder->GetFrameCount(&frameCount));
```

PNG/JPEGは通常1 Frameですが、GIFやTIFFなど複数Frameを持つ形式があります。

## 17. Frame取得

```cpp
Microsoft::WRL::ComPtr<IWICBitmapFrameDecode> frame;
ThrowIfFailed(decoder->GetFrame(0, frame.GetAddressOf()));
```

本章ではFrame 0を静止画として使用します。

## 18. 画像寸法

```cpp
UINT width = 0;
UINT height = 0;
ThrowIfFailed(frame->GetSize(&width, &height));
```

0 Size、上限超過、乗算OverflowをPixel Buffer確保前に検証します。

## 19. Dimension上限

Device Feature Levelが保証するTexture2D最大寸法を超える画像は作成できません。Asset Build時とRuntime Load時の両方で制限します。

## 20. Pixel Format取得

```cpp
WICPixelFormatGUID sourceFormat{};
ThrowIfFailed(frame->GetPixelFormat(&sourceFormat));
```

File拡張子からDecode後のChannel配置を決めつけません。

## 21. WIC Pixel FormatとDXGI Format

WICとDXGIは別のFormat体系です。対応表を持つか、WIC側で共通Pixel Formatへ変換してからDXGI Formatを決めます。

## 22. 共通出力Format

初学者向けの安定した経路は、WICで32-bit RGBAまたはBGRAへ揃え、対応するDXGI FormatでTextureを作る方法です。

## 23. RGBAとBGRA

Channel順を取り違えると赤と青が入れ替わります。

```text
RGBA bytes -> DXGI_FORMAT_R8G8B8A8_UNORM
BGRA bytes -> DXGI_FORMAT_B8G8R8A8_UNORM
```

実際のWIC GUIDとMemory配置を対応させます。

## 24. Format Converter

```cpp
Microsoft::WRL::ComPtr<IWICFormatConverter> converter;
ThrowIfFailed(wicFactory->CreateFormatConverter(
    converter.GetAddressOf()));
```

## 25. Converter初期化

```cpp
ThrowIfFailed(converter->Initialize(
    frame.Get(),
    GUID_WICPixelFormat32bppRGBA,
    WICBitmapDitherTypeNone,
    nullptr,
    0.0,
    WICBitmapPaletteTypeCustom));
```

Sourceがすでに同Formatでも、共通経路に揃える設計は理解しやすくなります。

## 26. CanConvert

```cpp
BOOL canConvert = FALSE;
ThrowIfFailed(converter->CanConvert(
    sourceFormat,
    GUID_WICPixelFormat32bppRGBA,
    &canConvert));
```

変換不能を`Initialize`失敗だけで知るより、明確なError Messageを作れます。

## 27. Bits Per Pixel

32bpp RGBAは一Pixel 4 Byteです。

```cpp
constexpr std::uint64_t bytesPerPixel = 4;
```

Formatを変える場合は固定値を流用しません。

## 28. Row Pitch

Row Pitchは画像一行が占めるByte数です。

```cpp
const std::uint64_t rowPitch64 =
    static_cast<std::uint64_t>(width) * bytesPerPixel;
```

## 29. Image Size

```cpp
const std::uint64_t imageSize64 =
    rowPitch64 * static_cast<std::uint64_t>(height);
```

`UINT`へ変換する前にOverflowと上限を検証します。

## 30. なぜ64-bitで計算するか

`width * 4 * height`を32-bitで先に計算すると、変換前にOverflowする可能性があります。広い型へ変換してから乗算します。

## 31. CPU Pixel Buffer

```cpp
std::vector<std::byte> pixels(
    static_cast<std::size_t>(imageSize64));
```

Allocation失敗もLoad Errorとして扱います。

## 32. CopyPixels

```cpp
ThrowIfFailed(converter->CopyPixels(
    nullptr,
    static_cast<UINT>(rowPitch64),
    static_cast<UINT>(imageSize64),
    reinterpret_cast<BYTE*>(pixels.data())));
```

第1引数`nullptr`は画像全体をCopyする指定です。

## 33. WICRect

画像の一部分だけDecode/Copyする場合は`WICRect`を渡せます。Atlas切出し目的ならAsset Build時処理とRuntime処理を比較します。

## 34. Row PitchはWidthと同義ではない

PitchはByte単位、WidthはPixel単位です。Channel数、Bit深度、Paddingによって値が変わります。

## 35. Decode結果型

```cpp
struct DecodedImage
{
    std::vector<std::byte> pixels;
    UINT width = 0;
    UINT height = 0;
    UINT rowPitch = 0;
    DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
    bool isSrgb = false;
};
```

WIC ObjectをRendererへ渡さず、中立的なDataへ変換します。

## 36. Texture2D Descriptor

```cpp
D3D11_TEXTURE2D_DESC desc{};
desc.Width = image.width;
desc.Height = image.height;
desc.MipLevels = 1;
desc.ArraySize = 1;
desc.Format = image.format;
desc.SampleDesc = {1, 0};
desc.Usage = D3D11_USAGE_IMMUTABLE;
desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
desc.CPUAccessFlags = 0;
desc.MiscFlags = 0;
```

## 37. Sample Count

通常のSample用Textureは非MSAAなのでCount 1、Quality 0です。MSAA Render Targetとは用途が異なります。

## 38. Immutable Texture

内容が変わらない画像TextureはImmutableにできます。作成時に全Subresourceの初期Dataが必要です。

## 39. Default Texture

後からGPU Copyや`UpdateSubresource`を行うTextureはDefaultが適します。StreamingやRender Target兼用では別のDescriptorが必要です。

## 40. Dynamic Texture

CPUが頻繁に書く特殊用途で使いますが、一般的なGame画像を毎Frame Dynamic更新する設計は避けます。

## 41. Subresource Data

```cpp
D3D11_SUBRESOURCE_DATA initial{};
initial.pSysMem = image.pixels.data();
initial.SysMemPitch = image.rowPitch;
initial.SysMemSlicePitch = 0;
```

2D TextureではRow Pitchが重要です。

## 42. CreateTexture2D

```cpp
Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
ThrowIfFailed(device->CreateTexture2D(
    &desc,
    &initial,
    texture.GetAddressOf()));
```

成功後、Immutable TextureならCPU Pixel Bufferを解放できます。

## 43. ResourceとView

`ID3D11Texture2D`はMemoryと形状、`ID3D11ShaderResourceView`はShaderからどう読むかを表します。SRVはPixel DataのCopyではありません。

## 44. Shader Resource View

SRVはTextureやBufferをShader InputとしてBindingするViewです。対象Resourceは作成時に`D3D11_BIND_SHADER_RESOURCE`を持つ必要があります。

## 45. CreateShaderResourceView

```cpp
Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
ThrowIfFailed(device->CreateShaderResourceView(
    texture.Get(),
    nullptr,
    srv.GetAddressOf()));
```

単一Mip、単一Textureでは既定Viewを使えます。

## 46. 明示SRV Descriptor

```cpp
D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
srvDesc.Format = desc.Format;
srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
srvDesc.Texture2D.MostDetailedMip = 0;
srvDesc.Texture2D.MipLevels = 1;
```

## 47. MostDetailedMip

Viewから見える最も高精細なMip Levelです。0ならResourceのMip 0から読みます。

## 48. MipLevels

SRVから参照可能なMip数です。Resource全Mipを公開する指定と、特定範囲だけ公開する指定があります。

## 49. SRVがResource寿命を保持する

SRV作成後にTextureの直接`ComPtr`を解放しても、SRVがResource参照を保持します。ただしDebugやStreaming Metadataのため本体を所有する設計もあります。

## 50. sRGBとは

Base Colorなど多くの表示用画像はsRGB Transfer Functionで保存されています。Lighting計算は通常Linear空間で行うため、Sample時にLinearへDecodeします。

## 51. sRGB Format

```text
R8G8B8A8_UNORM      : 数値をそのままUNORMとして読む
R8G8B8A8_UNORM_SRGB : RGBをsRGBからLinearへDecodeして読む
```

Alpha Channelは通常sRGB変換対象ではありません。

## 52. 何をsRGBにするか

Base Color、Emissive Color、UI Colorなど色を表すTextureはsRGB候補です。Normal、Roughness、Metallic、Mask、DepthなどData TextureはLinearとして読みます。

## 53. File拡張子でColor Spaceを決めない

PNGだからsRGB、TGAだからLinearとは限りません。Asset Import設定またはMaterial SemanticをSource of Truthにします。

## 54. Typeless ResourceとsRGB View

同じResourceへLinear ViewとsRGB Viewを作り分ける場合、互換なTypeless Resource Formatを使う設計があります。Format互換性を確認します。

## 55. 二重Gamma変換

Shaderで手動`pow`を行い、さらにsRGB SRV Decodeを使うと二重変換になります。入力Decodeと出力Encodeの責任を一か所にします。

## 56. Alphaの種類

- Straight Alpha：RGBがAlphaで事前乗算されていない。
- Premultiplied Alpha：RGBがAlphaで乗算済み。
- Opaque：Alphaを使わない。

Blend StateとTexture内容を一致させます。

## 57. WIC変換とPremultiplied Alpha

WIC Pixel FormatにはPremultiplied表現もあります。名前が似ているFormatを無条件に選ばず、最終Blend規約へ合わせます。

## 58. JPEGのAlpha

一般的なJPEGはAlpha Channelを持ちません。RGBAへ変換したときのAlphaは不透明として扱うのが基本です。

## 59. PNGの透明Pixel RGB

Alpha 0でもRGB値が残る場合があります。Linear FilterやMip生成で周辺へ色が滲むため、Asset加工やPremultiply規約を検討します。

## 60. PSへSRVをBindingする

```cpp
ID3D11ShaderResourceView* views[] = {srv.Get()};
context->PSSetShaderResources(0, 1, views);
```

HLSLの`register(t0)`とSlot 0を一致させます。

## 61. HLSL Texture宣言

```hlsl
Texture2D baseColorTexture : register(t0);
SamplerState baseColorSampler : register(s0);
```

TextureとSamplerは別Object、別Slot Tableです。

## 62. Sample

```hlsl
float4 baseColor =
    baseColorTexture.Sample(baseColorSampler, input.uv);
```

SamplerがFilter、Address Mode、LOD選択規則を提供します。

## 63. Load

```hlsl
float4 value = texture.Load(int3(pixelX, pixelY, mipLevel));
```

整数Texel座標で読み、通常のFilterを使いません。用途と境界を明確にします。

## 64. SampleLevel

明示Mip Levelを指定します。Pixel Shader以外や特殊PassでLODを制御する際に使います。

## 65. SRV SlotはStageごとに別

PS Slot 0へBindingしてもVS Slot 0からは見えません。Vertex ShaderでTextureを読むなら`VSSetShaderResources`が必要です。

## 66. SRVを解除する

```cpp
ID3D11ShaderResourceView* nullViews[] = {nullptr};
context->PSSetShaderResources(0, 1, nullViews);
```

同じResourceをRTVやUAVとして書く前に、Input Slotから外します。

## 67. Read/Write競合

同一SubresourceをSRVとして読みながらRTV/UAVとして書くことはできません。Runtimeの自動Null化とWarningへ依存せず、Pass境界で明示解除します。

## 68. Texture Asset型

```cpp
struct TextureAsset
{
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
    UINT width = 0;
    UINT height = 0;
    UINT mipLevels = 1;
    DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
    bool isSrgb = false;
};
```

描画と診断に必要なMetadataを保持します。

## 69. Loaderの二段階化

```text
worker thread: file read + WIC decode + validation
render/upload side: Direct3D resource creation + publish
```

DiskとDecodeの待ちをRender Loopから外します。

## 70. Device MethodのThread利用

Device Resource作成はThread利用できる設計ですが、Loader Queue、Device喪失、Shutdown、公開順序を統一します。Immediate Context操作をWorkerへ無秩序に広げません。

## 71. 非同期Loadの状態

```text
Unloaded -> Queued -> Decoding -> Uploading -> Ready
                              \-> Failed
```

描画側はReadyでないAssetを直接Dereferenceしません。

## 72. Fallback Texture

白、黒、Flat Normal、Checker Errorなど用途別の小Textureを常備します。Load中や失敗時も有効SRVをBindingできます。

## 73. 用途別Fallback

```text
base color -> white or checker
normal     -> (0.5, 0.5, 1.0)
roughness  -> chosen neutral scalar
metallic   -> 0
emissive   -> black
```

一つのMagenta Textureを全Data用途へ流用しません。

## 74. Texture Cache

正規化Path、Color Space、Import Option、Mip方針などをKeyにします。同じFileでもsRGB/Linear解釈が違えば別Viewまたは別Asset契約です。

## 75. CacheとWeak Ownership

Cacheが永久に強参照すると未使用Textureが解放されません。参照Count、Weak Entry、LRU、BudgetなどEngine規模に合う方式を選びます。

## 76. Memory見積り

非圧縮RGBA8のMip 0は概算`width * height * 4` Byteです。Mip Chain、Array、Alignment、Driver管理分も加わります。

## 77. Power-of-two

Direct3D 11では多くの一般Textureで2の累乗寸法だけに限定されません。ただしBlock Compression、Mip、古いFeature Level、Tool制約は確認します。

## 78. GPU圧縮Texture

BC1、BC3、BC5、BC7などはMemoryと帯域を減らします。WIC Decode後のRGBA Uploadだけでなく、Asset Build時にDDS等へ変換するPipelineを検討します。

## 79. Normal Map圧縮

Normal Mapは色画像と性質が違います。BC5などを使い、sRGB Decodeを無効にし、Shaderで必要ならZを再構築します。

## 80. Texture Streaming

最初は低Mipだけを読み、必要に応じ高Mipを追加する方式です。Resource作成方針、Mip residency、I/O Priority、破棄Budgetが必要です。

## 81. Device Lost

Device喪失時、TextureとSRVも古いDeviceに属するため再作成します。Source Asset、Decoded Cache、再Upload Queueのどこから復元するか決めます。

## 82. Debug Name

```cpp
SetDebugName(*texture.Get(), "BaseColor Texture: player_body");
SetDebugName(*srv.Get(), "BaseColor SRV: player_body sRGB");
```

用途、Asset名、Color Spaceを識別できる名前にします。

## 83. Load Errorに含める情報

```text
canonical path
file size
decoder/frame result
source and destination pixel formats
width/height
row pitch
DXGI format
HRESULT and message
```

「Texture読込失敗」だけでは原因を直せません。

## 84. よくある失敗：PNG Byte列をTextureへ直接渡す

圧縮File ByteをRGBA Pixelとして扱います。DecoderでPixelへ展開してからTextureを作ります。

## 85. よくある失敗：Row PitchをPixel数で渡す

Width 1024をPitch 1024として渡し、実際はRGBA8なので4096 Byte必要です。Pitchの単位はByteです。

## 86. よくある失敗：RGBA/BGRAの取り違え

赤と青が入れ替わります。WIC出力GUID、CPU Memory順、DXGI Formatを一組で定義します。

## 87. よくある失敗：全TextureをsRGBにする

NormalやRoughnessへsRGB Decodeが適用され、値が壊れます。Material SemanticでColor/Dataを分類します。

## 88. よくある失敗：TextureだけBindingする

SamplerをBindingせず、期待したFilterやAddress Modeになりません。SRVとSamplerは別Stateです。

## 89. よくある失敗：SRV Slotが古いまま

MaterialにTextureがないとき前MaterialのSRVが残ります。FallbackまたはNullを全必要Slotへ明示Bindingします。

## 90. よくある失敗：DecodeをRender Threadで同期実行

戦闘中の初回表示でDisk I/OとDecodeがFrameを停止させます。事前Load、非同期Decode、Placeholderを使います。

## 91. Decode Test

- PNG、JPEG、透明PNGを読む。
- 異なるWIC Source Formatを共通RGBAへ変換する。
- 0 Size、巨大Size、壊れたFileを拒否する。
- Row Pitch/Image Size Overflowを検出する。
- Unicode Pathを扱う。

## 92. GPU作成Test

- Descriptorの寸法とFormatがDecode結果と一致する。
- Immutable Textureへ完全な初期Dataを渡す。
- SRV DescriptorがResourceと互換である。
- sRGB/Linear Viewを期待どおり選ぶ。
- Debug Layer Warningがない。

## 93. 描画Test

- UV 0から1の四角形へ表示する。
- RGBA各Channelを個別可視化する。
- Alpha 0/0.5/1を確認する。
- TextureなしMaterialでFallbackを表示する。
- SRV解除後に同ResourceをRTVへBindingできる。

## 94. 非同期Test

- Load中にFallbackを表示する。
- Decode失敗で描画Threadが停止しない。
- 同じAssetへの同時要求を統合する。
- Shutdown時にJobとCOM寿命を安全に終了する。
- Device再作成時にTextureを復元する。

## 95. 完成確認表

- [ ] File FormatとPixel Formatを区別できる。
- [ ] COM/WIC Factoryを正しいThread寿命で管理できる。
- [ ] Decoder、Frame、Converterの役割を説明できる。
- [ ] Width、Row Pitch、Image Sizeを安全に計算できる。
- [ ] Texture2DとSRVの違いを説明できる。
- [ ] sRGB TextureとData Textureを分類できる。
- [ ] SRVとSamplerを別々にBindingできる。
- [ ] SRV/RTV/UAV競合を明示解除できる。
- [ ] DecodeとGPU Uploadを非同期Pipelineへ分けられる。
- [ ] Cache、Fallback、Device Lost復元を設計できる。

## 96. この章の要点

- WICは画像FileをPixelへDecodeし、Direct3DはPixelをGPU Textureへ格納します。
- WIC Pixel Format、CPU Memory Channel順、DXGI Formatを一致させます。
- Row PitchとImage SizeはByte単位でOverflow検証します。
- TextureはResource、SRVはShaderから読むためのViewです。
- Base ColorはsRGB候補、NormalやMaskはLinear Dataです。
- SRVとSamplerは別Objectで、StageごとにBindingします。
- 同じSubresourceをShaderで読みながらRTV/UAVで書けません。
- 実用LoaderではDecode、Upload、Cache、Fallback、Device復元を一つのLifecycleとして設計します。

## 97. 公式資料

- [Windows Imaging Component overview](https://learn.microsoft.com/en-us/windows/win32/wic/-wic-about-windows-imaging-codec)
- [IWICImagingFactory::CreateDecoderFromFilename](https://learn.microsoft.com/en-us/windows/win32/api/wincodec/nf-wincodec-iwicimagingfactory-createdecoderfromfilename)
- [IWICFormatConverter](https://learn.microsoft.com/en-us/windows/win32/api/wincodec/nn-wincodec-iwicformatconverter)
- [IWICBitmapSource::CopyPixels](https://learn.microsoft.com/en-us/windows/win32/api/wincodec/nf-wincodec-iwicbitmapsource-copypixels)
- [ID3D11Device::CreateTexture2D](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11device-createtexture2d)
- [ID3D11Device::CreateShaderResourceView](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11device-createshaderresourceview)
- [D3D11_SHADER_RESOURCE_VIEW_DESC](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/ns-d3d11-d3d11_shader_resource_view_desc)
- [ID3D11DeviceContext::PSSetShaderResources](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11devicecontext-pssetshaderresources)
- [DXGI formats](https://learn.microsoft.com/en-us/windows/win32/direct3ddxgi/dxgi-format)
- [Texture resources](https://learn.microsoft.com/en-us/windows/win32/direct3d11/overviews-direct3d-11-resources-textures)

次章では、Textureをどのように補間し、範囲外UVをどう扱い、どのMip Levelを選ぶかを決めるSampler State、UV、Mip Mapを扱います。
