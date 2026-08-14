# DirectX 12 第7章：Root Signature・Resource Binding

この章では、Command ListとShaderのResource Binding契約を定義するRoot Signatureを学びます。Descriptor Table、Root Descriptor、Root Constants、Static Sampler、Register/Space、Visibility、Version 1.1、Cost、更新頻度別Layoutを扱います。

## 1. Root Signatureとは

Shaderが利用するCBV/SRV/UAV/Sampler/Constantを、Command Listからどう渡すか定義するBinding Interfaceです。

## 2. ABIとして考える

```text
CPU Command側              HLSL側
Root Parameter 0  <------> b0, space0
Root Parameter 1  <------> t0..t7, space1
Root Parameter 2  <------> 32-bit constants b1
Static Sampler    <------> s0
```

Layout不一致は正しいDataを別用途として読む原因になります。

## 3. PSOとの関係

Root SignatureはBinding契約、PSOはShaderとPipeline Stateの組合せです。互換なRoot SignatureをPSOへ関連付けます。

## 4. 三種類のRoot Parameter

```text
Descriptor Table : Heap内Descriptor Rangeを指す
Root Descriptor  : Buffer GPU Virtual Addressを直接渡す
Root Constants   : 32-bit値をCommandへ直接埋め込む
```

## 5. Static Sampler

Sampler設定をRoot Signatureへ固定で埋め込みます。通常のRoot Parameterとは別に宣言します。

## 6. Root Signature Budget

Root Signatureは最大64 DWORDのCost Budgetを持ちます。大きくすれば常に便利という設計ではありません。

## 7. Costの基本

```text
Descriptor Table : 1 DWORD
Root Descriptor  : 2 DWORD
Root Constants   : 32-bit値一個につき1 DWORD
```

Static SamplerはRoot Argument DWORDとは別に扱われますが個数制約があります。

## 8. DWORDとは

ここでは32-bit単位です。64 DWORDは256 byte相当のRoot Argument Budgetです。

## 9. Root Argument更新Cost

Drawごとに頻繁に変えるRoot ParameterはCommand Stream SizeやHardware Costへ影響します。更新頻度とParameter順を考えます。

## 10. Descriptor Table

Shader-visible Descriptor Heap内の連続範囲をGPU Handle一つで参照します。

## 11. Tableの利点

多数のTexture/Bufferを1 DWORDのRoot CostでBindingできます。Descriptor HeapのAllocationとLifetime管理が必要です。

## 12. Descriptor Range

一つのTable内でCBV/SRV/UAV/SamplerのRegister範囲を記述します。SamplerはSampler Heap用の別Tableにします。

## 13. Range Type

```text
D3D12_DESCRIPTOR_RANGE_TYPE_CBV
D3D12_DESCRIPTOR_RANGE_TYPE_SRV
D3D12_DESCRIPTOR_RANGE_TYPE_UAV
D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER
```

## 14. Version 1.0 Range

```cpp
D3D12_DESCRIPTOR_RANGE range{};
range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
range.NumDescriptors = 8;
range.BaseShaderRegister = 0;
range.RegisterSpace = 1;
range.OffsetInDescriptorsFromTableStart =
    D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
```

## 15. Base Shader Register

`BaseShaderRegister = 0`のSRV RangeはHLSLの`t0`から始まります。

## 16. Num Descriptors

Rangeに含むDescriptor数です。HLSL Array/Binding規約と一致させます。

## 17. Register Space

同じRegister番号を論理Groupへ分けられます。

```hlsl
Texture2D materialTextures[8] : register(t0, space1);
StructuredBuffer<Light> lights : register(t0, space2);
```

## 18. Table Offset

Table先頭からRange開始位置を指定します。`APPEND`は前Range直後へ配置します。

## 19. Rangeの重なり

同じVisibilityでRegister/Spaceが競合するRangeを作りません。Serializer/Debug Layer Errorを確認します。

## 20. Table内の複数Range

```text
Table Start
 +0 CBV b0
 +1 SRV t0
 +2 SRV t1
 +3 UAV u0
```

Range Offsetと実Descriptor配置を一致させます。

## 21. Sampler Tableを分ける理由

Samplerは別Heap Typeに置かれ、CBV/SRV/UAV Heapと同一Table/Heapへ混在できません。

## 22. SetGraphicsRootDescriptorTable

```cpp
commandList->SetGraphicsRootDescriptorTable(
    rootParameterIndex,
    tableGpuHandle);
```

Graphics Root SignatureのParameter Indexを指定します。

## 23. SetComputeRootDescriptorTable

```cpp
commandList->SetComputeRootDescriptorTable(
    rootParameterIndex,
    tableGpuHandle);
```

Compute Binding StateはGraphicsと別です。

## 24. Heapを先にBind

Descriptor Table設定前に対象Shader-visible Heapを`SetDescriptorHeaps`でBindします。

## 25. Heap切替後

Descriptor Heapを変更すると、そのHeapを参照するRoot Tableを新しいGPU Handleで再設定します。

## 26. TableのLifetime

GPUがCommandを実行し終えるまでHeapとDescriptor内容を変更・解放しません。

## 27. Root Descriptor

BufferのGPU Virtual AddressをRoot Argumentとして直接Bindingします。

## 28. Root CBV

```cpp
D3D12_ROOT_DESCRIPTOR cbv{};
cbv.ShaderRegister = 0;
cbv.RegisterSpace = 0;
```

Root Signature 1.0の構造例です。

## 29. SetGraphicsRootConstantBufferView

```cpp
commandList->SetGraphicsRootConstantBufferView(
    rootParameterIndex,
    constantGpuAddress);
```

Descriptor Heap Slotを使いません。

## 30. Root SRV

```cpp
commandList->SetGraphicsRootShaderResourceView(
    rootParameterIndex,
    bufferGpuAddress);
```

Root SRVはBuffer Resource用途です。

## 31. Root UAV

```cpp
commandList->SetComputeRootUnorderedAccessView(
    rootParameterIndex,
    bufferGpuAddress);
```

UAV Access順序とResource Stateは別途管理します。

## 32. Root Descriptorの制約

TextureをRoot SRV/UAVとして直接Bindingする用途ではありません。BufferのGPU Virtual Addressを渡す仕組みです。

## 33. Root DescriptorのCost

一つ2 DWORDです。少数の頻繁に変わるBufferには便利ですが、大量ResourceはTableへ置きます。

## 34. Root CBVとCBV Table

```text
Root CBV  : Heap Copy不要、2 DWORD、Addressを直接更新
Table CBV : 1 Tableで多数Binding、Descriptor Allocationが必要
```

## 35. Root Constants

少量の32-bit値をCommand Listから直接設定します。

## 36. Constants宣言

```cpp
D3D12_ROOT_CONSTANTS constants{};
constants.ShaderRegister = 1;
constants.RegisterSpace = 0;
constants.Num32BitValues = 4;
```

HLSLでは対応するConstant Buffer Registerとして見えます。

## 37. SetGraphicsRoot32BitConstants

```cpp
struct DrawConstants
{
    uint32_t objectIndex;
    uint32_t materialIndex;
    uint32_t flags;
    float fade;
};

DrawConstants data{ objectIndex, materialIndex, flags, fade };

commandList->SetGraphicsRoot32BitConstants(
    rootParameterIndex,
    4,
    &data,
    0);
```

## 38. 一部Constant更新

Destination Offsetを使いParameter内の一部を変更できます。未設定部分を暗黙に正しいと仮定しません。

## 39. Root ConstantsのCost

値一個につき1 DWORDです。Matrixや大きなArrayはRoot CBV/Descriptor Tableへ置きます。

## 40. 型の扱い

APIは32-bit値列として渡します。HLSL側の`uint`、`float`、LayoutとBit表現を一致させます。

## 41. boolを直接置かない

C++ `bool`のSize/Layoutへ依存せず`uint32_t` Flagを使います。

## 42. Static Sampler

```cpp
D3D12_STATIC_SAMPLER_DESC sampler{};
sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
sampler.ShaderRegister = 0;
sampler.RegisterSpace = 0;
sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
sampler.MinLOD = 0.0f;
sampler.MaxLOD = D3D12_FLOAT32_MAX;
```

## 43. Static Samplerの利点

Sampler HeapのAllocation/Bindingが不要で、共通Samplerを固定できます。

## 44. Static Samplerの欠点

RuntimeでFilter/Address Modeを変えられません。Materialごとに可変ならSampler Descriptor Tableを使います。

## 45. Comparison Sampler

Shadow Map等ではComparison Filter/Functionを正しく設定します。通常SamplerとRegister契約を分けます。

## 46. Border Color

Static Samplerで使えるBorder Colorは定義済みEnumです。任意Colorが必要な場合の制約を確認します。

## 47. Shader Visibility

Parameterを参照できるShader Stageを限定します。

```text
ALL
VERTEX
HULL
DOMAIN
GEOMETRY
PIXEL
AMPLIFICATION
MESH
```

SDK/Root Signature Versionの対応を確認します。

## 48. Visibilityの目的

Binding競合回避やHardware最適化Hintになります。実際に使うStageへ限定できます。

## 49. VisibilityはSecurity境界ではない

Resource Accessの正しさとState/LifetimeはApplicationが管理します。

## 50. Register重複とVisibility

Vertex限定b0とPixel限定b0を別Parameterで定義できる構成があります。Visibilityが重なる場合は競合します。

## 51. Deny Flags

使わないShader StageのRoot AccessをDenyするRoot Signature Flagがあります。

## 52. Root Signature Flags

```cpp
D3D12_ROOT_SIGNATURE_FLAGS flags =
    D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
    D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
    D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
    D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;
```

## 53. Input Assembler Flag

Input Layoutを使うGraphics Pipelineでは`ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT`を指定します。

## 54. Local Root Signature

Raytracing等でLocal Root Signatureの概念があります。通常Graphics Global Root Signatureと用途を混同しません。

## 55. Root Signature Version

Deviceの`D3D12_FEATURE_ROOT_SIGNATURE`で1.1対応を確認し、非対応なら1.0へFallbackします。

## 56. Version 1.1

Descriptor Range/Root DescriptorへData/Descriptorの可変性Flagを追加し、Driver最適化へ情報を与えます。

## 57. Range Flags

```text
DESCRIPTORS_VOLATILE
DATA_VOLATILE
DATA_STATIC_WHILE_SET_AT_EXECUTE
DATA_STATIC
DESCRIPTORS_STATIC_KEEPING_BUFFER_BOUNDS_CHECKS
```

使用可能条件と意味を公式仕様で確認します。

## 58. DESCRIPTORS_VOLATILE

Descriptor内容がCommand List実行中の規則に従い変化し得ることを示します。寿命ルールを緩く解釈しません。

## 59. DATA_VOLATILE

Descriptorが指すDataが変化し得るHintです。Resource State/同期が不要になるわけではありません。

## 60. DATA_STATIC_WHILE_SET_AT_EXECUTE

Root Tableが設定されて実行される期間のData不変性を表します。更新Policyと一致させます。

## 61. DATA_STATIC

より強い不変性を宣言します。実際に変更するDataへ誤指定すると未定義動作の原因になります。

## 62. 1.1から1.0への変換

Versioned Root Signature Serializerを使い、Support Versionに合わせたDescriptionを渡します。

## 63. D3D12_VERSIONED_ROOT_SIGNATURE_DESC

```cpp
D3D12_VERSIONED_ROOT_SIGNATURE_DESC versioned{};
versioned.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
versioned.Desc_1_1.NumParameters = parameterCount;
versioned.Desc_1_1.pParameters = parameters;
versioned.Desc_1_1.NumStaticSamplers = samplerCount;
versioned.Desc_1_1.pStaticSamplers = samplers;
versioned.Desc_1_1.Flags = flags;
```

参照配列はSerialize呼出しまで生存させます。

## 64. Root Parameter 1.1

`D3D12_ROOT_PARAMETER1`へParameter Type、Visibility、Table/Constants/Descriptor Unionを設定します。

## 65. Descriptor Range 1.1

`D3D12_DESCRIPTOR_RANGE1`へRange Flagsを含めます。

## 66. Parameter配列のPointer

Vector再Allocationで内部Pointerが無効にならないよう、Rangeを完成させてからParameterへPointerを設定する等のBuilder設計が必要です。

## 67. Serialize

```cpp
ComPtr<ID3DBlob> serialized;
ComPtr<ID3DBlob> errorBlob;

HRESULT hr = D3D12SerializeVersionedRootSignature(
    &versioned,
    &serialized,
    &errorBlob);
```

## 68. Error Blob

Serialize失敗時はError Blobの文字列をLogへ出します。HRESULTだけではRegister競合等の原因が分かりにくいです。

## 69. CreateRootSignature

```cpp
ComPtr<ID3D12RootSignature> rootSignature;
ThrowIfFailed(
    device->CreateRootSignature(
        0,
        serialized->GetBufferPointer(),
        serialized->GetBufferSize(),
        IID_PPV_ARGS(&rootSignature)),
    "ID3D12Device::CreateRootSignature");

rootSignature->SetName(L"Main Graphics Root Signature");
```

## 70. Node Mask

Single Adapter基本実装では0を使います。Multi-nodeは別途設計します。

## 71. SetGraphicsRootSignature

```cpp
commandList->SetGraphicsRootSignature(rootSignature.Get());
```

Graphics Root Parameterを設定する前にBindします。

## 72. SetComputeRootSignature

```cpp
commandList->SetComputeRootSignature(computeRootSignature.Get());
```

Graphics/Compute Root Signature Stateは別です。

## 73. Root Signature変更

異なるRoot SignatureをBindした後、必要なRoot Argumentを再設定します。以前のParameter値へ依存しません。

## 74. Parameter Index

APIはRegister番号ではなくRoot Parameter配列Indexを受け取ります。Enum/生成MetadataでMagic Numberを避けます。

## 75. 型付きIndex

```cpp
enum class MainRootParameter : UINT
{
    FrameCBV = 0,
    ViewCBV,
    MaterialTable,
    ObjectConstants,
    Count
};
```

## 76. HLSL Binding例

```hlsl
cbuffer FrameConstants : register(b0, space0)
{
    float time;
    float3 padding;
};

cbuffer ViewConstants : register(b1, space0)
{
    float4x4 viewProjection;
};

Texture2D materialTextures[8] : register(t0, space1);
SamplerState linearSampler : register(s0, space0);
```

## 77. Update Frequency

```text
Frame    : 時間、Lighting共通値
View     : Camera、Projection
Pass     : Shadow/Lighting/Post Process設定
Material : Texture、Material Parameter
Object   : Transform、Bone/Instance Index
Draw     : 少量Flag/Index
```

## 78. 更新頻度別Parameter

頻度の低いParameterを先に設定し、Drawごとの変更を少数にします。

## 79. Example Layout

```text
0 Root CBV       b0 space0 : Frame
1 Root CBV       b1 space0 : View
2 DescriptorTable t0..t7 space1 : Material Textures
3 Root Constants b2 space0 : Object/Material Index
4 DescriptorTable t0..tN space2 : Global Buffers
Static Sampler s0.. : common samplers
```

## 80. Layoutに唯一の正解はない

Hardware、Binding Tier、Draw数、Material数、Bindless設計で変わります。PIXとCPU/GPU計測で判断します。

## 81. Object MatrixをRoot Constantsに置く問題

4x4 Matrixは16 DWORDを使い、BudgetとCommand Streamを圧迫します。Upload CBV/Structured Buffer Indexを検討します。

## 82. Object Index方式

Root ConstantsでObject Indexだけ渡し、Structured BufferからTransform/Materialを読むとDrawごとのRoot Costを抑えられます。

## 83. Material Table方式

Materialごとに連続Texture Descriptor Rangeを作り、Draw時にTable Handleを切り替えます。

## 84. Bindless方式

大規模HeapをShaderからIndex参照し、Material DataへTexture Indexを保持します。対応Tier/Shader Model/Flagsを確認します。

## 85. Directly Indexed Heap Flags

新しいBinding ModelではCBV/SRV/UAV HeapやSampler Heapの直接Indexing用Root Signature Flagがあります。対応Shader ModelとHardwareを確認します。

## 86. BindlessとLifetime

IndexがGPU Dataへ長期保存されるため、Descriptor Slotの安定性、世代、Heap Migration、Device Lost再Mappingが重要です。

## 87. Root Signature共有

多数PSOで共通Root Signatureを使うとBinding LayoutとCacheを単純化できます。Featureが不要Parameterを大量に持つCostとのTrade-offがあります。

## 88. Pass専用Root Signature

Post Process/Compute等へ小さな専用Layoutを使えます。PSO切替とParameter再Bindを考慮します。

## 89. Shader内Root Signature

HLSLへRoot Signature文字列を埋め込み、Shader Blobから取得する方法があります。C++生成とのSource of Truthを一本化します。

## 90. Root Signature Macro

HLSL Macroで記述できますが、複雑な文字列の保守、Register契約、Variantを管理します。

## 91. ReflectionとValidation

Shader Reflectionから使用Register/Spaceを取得し、Root Signatureが全BindingをCoverageするかBuild時に検証できます。

## 92. Generated Binding Code

Shader MetadataからC++ Parameter Enum、Table Offset、Material Layoutを生成すると手書き不一致を減らせます。

## 93. Version Hash

Root Signature LayoutへHash/Versionを付け、PSO Cache、Material Data、Shader Binaryの互換性を検証します。

## 94. Hot Reload

Binding Layoutが同じShader差替えと、Root Signature変更を伴う差替えを分けます。旧PSO/Root SignatureはFence後に解放します。

## 95. Device Lost

Serialized Description/MetadataからRoot Signatureを再生成します。生ObjectだけをAsset情報として保持しません。

## 96. Null Binding

未使用/任意ResourceのTable Slotへ型の合うNull/Default Descriptorを設定します。未初期化Slotを参照しません。

## 97. Resource Stateは別責務

Root TableへSRVをBindingしてもResourceが自動でShader Resource Stateになるわけではありません。Barrierを記録します。

## 98. Resource Lifetimeは別責務

Root Parameter設定はResourceをGPU完了まで所有しません。Frame/Registry/Fenceで保持します。

## 99. Descriptor Heap Lifetime

Tableが指すHeapとSlot内容をGPU完了まで維持します。Root Signatureだけ残しても不十分です。

## 100. Debug Layer

Register競合、Heap未Binding、無効Table、Root Signature/PSO不整合等を検出できます。GPU-based Validationも利用します。

## 101. PIX

Drawを選び、Root Signature、各Root Argument、Descriptor Table内容、HLSL Register、Resourceを対応させます。

## 102. Root Cost表示

BuilderがDWORD Costを計算し、64超過をSerialize前にErrorにします。Parameter別CostもLogします。

## 103. Unit Test

Register重複、Visibility、Cost、Table Offset、Version Fallback、Parameter Enum、HLSL Metadata互換をTestします。

## 104. Integration Test

Frame/View/Object Constantと複数Texture/SamplerをBindingし、Parameterを変えたDrawが期待色/Transformになるか確認します。

## 105. よくある失敗：RegisterとIndex混同

`SetGraphicsRoot...`へb0の0を渡せばよいと思います。API引数はRoot Parameter配列Indexです。

## 106. よくある失敗：64 DWORD超過

Matrixや大量Root Descriptorを直接置きます。Table/Buffer Indexへ移します。

## 107. よくある失敗：TextureをRoot SRV

Texture ResourceのAddressをRoot SRVとして渡そうとします。Texture SRV Descriptor Tableを使います。

## 108. よくある失敗：Heap前にTable設定

対象Shader-visible HeapがBindされていません。Heap→Root Signature→Tableの順序を整理します。

## 109. よくある失敗：Root Signature変更後再Bindなし

以前のRoot Argumentが有効だと仮定します。新Layoutの全必要Parameterを設定します。

## 110. よくある失敗：Static宣言と実Data変更

Root Signature 1.1で強いStatic Flagを付けたDataを実行中に変更します。更新PolicyとFlagを一致させます。

## 111. よくある失敗：Sampler Heap混在

Sampler RangeをCBV/SRV/UAV Heap Tableへ置きます。Sampler用Table/Heapを分けます。

## 112. 実装Checklist

- [ ] Root SignatureとPSOの役割を区別できる。
- [ ] Table/Descriptor/ConstantsのCostを計算できる。
- [ ] Register/Space/VisibilityをHLSLと一致させる。
- [ ] Sampler TableをResource Tableと分ける。
- [ ] Root DescriptorをBuffer用途に限定する。
- [ ] Root Constantsを少量32-bit Dataに使う。
- [ ] 対応VersionをQueryし1.1/1.0を選ぶ。
- [ ] Serialize Error BlobをLogする。
- [ ] Root Signature変更後にArgumentを再設定する。
- [ ] 更新頻度別にParameterを設計する。
- [ ] Descriptor/Resource/Heap LifetimeをFenceで保証する。
- [ ] Reflection/MetadataでBinding互換性を検証する。

## 113. 理解確認問題

1. Root SignatureをBinding ABIと呼ぶ理由を説明してください。
2. Descriptor Table、Root Descriptor、Root Constantsの違いを説明してください。
3. 各ParameterのDWORD Costを説明してください。
4. Register Spaceを使う目的を説明してください。
5. TextureをRoot SRVへ直接Bindingできない理由を説明してください。
6. Static SamplerとSampler TableのTrade-offを説明してください。
7. Root Signature 1.1のData Flagを誤る危険を説明してください。
8. 更新頻度別LayoutがCommand Costを減らす仕組みを説明してください。

## 114. 章末要点

- Root SignatureはCommand ListとHLSLのResource Binding契約です。
- Descriptor Tableは多数Resource、Root DescriptorはBuffer Address、Root Constantsは少量値に使います。
- 64 DWORD Budgetと更新頻度を考慮してParameterを設計します。
- Register、Space、Visibility、Table OffsetをShaderと一致させます。
- Version 1.1 Flagは実際のDescriptor/Data更新Policyと一致させます。
- Serializer Error、Reflection、Generated Metadataで契約違反を検出します。
- Root Bindingとは別にResource State、Descriptor/Resource Lifetimeを管理します。

## 115. 公式資料

- [Root signatures overview](https://learn.microsoft.com/en-us/windows/win32/direct3d12/root-signatures-overview)
- [Root signature limits](https://learn.microsoft.com/en-us/windows/win32/direct3d12/root-signature-limits)
- [Creating a root signature](https://learn.microsoft.com/en-us/windows/win32/direct3d12/creating-a-root-signature)
- [Descriptor tables](https://learn.microsoft.com/en-us/windows/win32/direct3d12/descriptor-tables)
- [Using descriptors directly in the root signature](https://learn.microsoft.com/en-us/windows/win32/direct3d12/using-descriptors-directly-in-the-root-signature)
- [Using constants directly in the root signature](https://learn.microsoft.com/en-us/windows/win32/direct3d12/using-constants-directly-in-the-root-signature)
- [Root signature version 1.1](https://learn.microsoft.com/en-us/windows/win32/direct3d12/root-signature-version-1-1)
- [D3D12SerializeVersionedRootSignature](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-d3d12serializeversionedrootsignature)
- [ID3D12Device::CreateRootSignature](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12device-createrootsignature)
- [ID3D12GraphicsCommandList::SetGraphicsRootDescriptorTable](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12graphicscommandlist-setgraphicsrootdescriptortable)

次章では、DXC/HLSL Compile、Input Layout、Graphics Pipeline State Object、PSO Cache、Triangle描画を扱います。
