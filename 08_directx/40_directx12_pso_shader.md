# DirectX 12 第8章：Pipeline State Object・Shader

この章では、HLSLをDXCでCompileし、Graphics/Compute Pipeline State Objectを生成します。Shader Model、Entry Point、Input Layout、固定機能State、RTV/DSV Format、PSO Variant、Cache、Pipeline Library、Hot Reload、Triangle描画を扱います。

## 1. PSOとは

Pipeline State Objectは、Shaderと主要な固定機能Stateを事前にまとめた不変Objectです。

## 2. D3D11との違い

```text
D3D11 : VS、PS、Blend、Rasterizer、Depth Stateを個別Bind
D3D12 : 主要な組合せをGraphics PSOとして事前生成
```

## 3. なぜ事前にまとめるのか

DriverがPipeline互換性を検証・Compileしやすくなり、Draw時の隠れたCostを減らせます。その代わりVariant管理が必要です。

## 4. Graphics PSOの主要要素

- Root Signature
- VS/PS/DS/HS/GS Bytecode
- Input Layout
- Blend State
- Rasterizer State
- Depth/Stencil State
- Primitive Topology Type
- RTV/DSV Format
- Sample Count/Quality

## 5. Compute PSO

Root Signature、Compute Shader、Cache等を持ちます。Graphics用のBlend/RTV/Input Layoutはありません。

## 6. Shader Compile Pipeline

```text
.hlsl Source
 -> Include/Define/Compile Arguments
 -> DXC Compile
 -> Error/Warning Log
 -> DXIL Bytecode
 -> Reflection/Hash/Metadata
 -> PSO作成
```

## 7. DXCとは

DirectX Shader CompilerはLLVM/Clang基盤のHLSL Compilerで、Shader Model 6系のDXILを生成します。

## 8. 必要Header/Library

```cpp
#include <dxcapi.h>
#include <wrl/client.h>
```

DXC Binary/Libraryの導入・配布方式をBuild Systemで明示します。

## 9. DXC Object

```cpp
ComPtr<IDxcUtils> utils;
ComPtr<IDxcCompiler3> compiler;

ThrowIfFailed(
    DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&utils)),
    "Create DXC Utils");

ThrowIfFailed(
    DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&compiler)),
    "Create DXC Compiler");
```

## 10. Include Handler

```cpp
ComPtr<IDxcIncludeHandler> includeHandler;
ThrowIfFailed(
    utils->CreateDefaultIncludeHandler(&includeHandler),
    "Create DXC Include Handler");
```

ProductionではVirtual File System/Dependency Tracking対応Handlerも検討します。

## 11. Source読込み

```cpp
ComPtr<IDxcBlobEncoding> sourceBlob;
ThrowIfFailed(
    utils->LoadFile(shaderPath.c_str(), nullptr, &sourceBlob),
    "Load HLSL File");
```

EncodingとPathをLogへ残します。

## 12. DxcBuffer

```cpp
DxcBuffer source{};
source.Ptr = sourceBlob->GetBufferPointer();
source.Size = sourceBlob->GetBufferSize();
source.Encoding = DXC_CP_UTF8;
```

実Source Encodingと一致させます。

## 13. Entry Point

VSなら`VSMain`、PSなら`PSMain`等、Compile開始関数名を指定します。

## 14. Target Profile

```text
vs_6_0 : Vertex Shader Model 6.0
ps_6_0 : Pixel Shader Model 6.0
cs_6_0 : Compute Shader Model 6.0
```

DeviceのHighest Shader Modelと配布要件を確認します。

## 15. Compile Arguments

```cpp
std::vector<LPCWSTR> arguments =
{
    shaderPath.c_str(),
    L"-E", entryPoint.c_str(),
    L"-T", targetProfile.c_str(),
    L"-HV", L"2021",
    L"-Zpr"
};
```

行列Packing方針`-Zpr/-Zpc`をC++側Transpose規約と統一します。

## 16. Debug Arguments

```cpp
#if defined(_DEBUG)
arguments.push_back(L"-Zi");
arguments.push_back(L"-Qembed_debug");
arguments.push_back(L"-Od");
#else
arguments.push_back(L"-O3");
#endif
```

性能計測は最適化Shaderで行います。

## 17. Warnings as Errors

`-WX`でWarningをErrorにできます。段階導入し、既知Warningを放置しません。

## 18. Defines

`-D`引数でVariant Macroを渡せます。Define名/値をPSO KeyとBuild Logへ含めます。

## 19. Include Path

`-I`でInclude Directoryを渡します。相対Pathの基準とAsset Package Pathを固定します。

## 20. Compile呼出し

```cpp
ComPtr<IDxcResult> result;
ThrowIfFailed(
    compiler->Compile(
        &source,
        arguments.data(),
        static_cast<UINT32>(arguments.size()),
        includeHandler.Get(),
        IID_PPV_ARGS(&result)),
    "IDxcCompiler3::Compile");
```

## 21. Compiler呼出し成功とShader成功

`Compile` API自体が成功しても、Shader Compile Statusが失敗している場合があります。両方を確認します。

## 22. Error Output

```cpp
ComPtr<IDxcBlobUtf8> errors;
result->GetOutput(
    DXC_OUT_ERRORS,
    IID_PPV_ARGS(&errors),
    nullptr);

if (errors && errors->GetStringLength() > 0)
    LogShaderMessage(errors->GetStringPointer());
```

Warningもここへ出ます。

## 23. Compile Status

```cpp
HRESULT status = S_OK;
ThrowIfFailed(result->GetStatus(&status), "DXC GetStatus");
ThrowIfFailed(status, "HLSL Compilation");
```

## 24. DXIL Object

```cpp
ComPtr<IDxcBlob> object;
ThrowIfFailed(
    result->GetOutput(
        DXC_OUT_OBJECT,
        IID_PPV_ARGS(&object),
        nullptr),
    "Get DXIL Object");
```

## 25. PDB Output

Debug Buildでは`DXC_OUT_PDB`と提案Nameを取得・保存できます。PIX/Shader Debugとの対応を管理します。

## 26. Shader Hash

Source、Include、Define、Arguments、Compiler VersionをHashへ含め、Cache Keyを作ります。

## 27. Dependency Tracking

Include File更新でもCacheを無効化する必要があります。Custom Include HandlerでDependencyを記録できます。

## 28. Compiler Version

DXC更新でBytecode/最適化結果が変わり得ます。Build ReportとCache Versionへ含めます。

## 29. Shader Reflection

Input/Output Signature、Resource Binding、Constant Buffer Layout、Thread Group Size等を取得し、Engine Metadataと検証します。

## 30. Reflectionの用途

- Root Signature互換性検証
- Input Layout検証
- Material Parameter生成
- Descriptor Table生成
- Debug UI

## 31. HLSL Vertex Shader

```hlsl
struct VSInput
{
    float3 position : POSITION;
    float4 color    : COLOR0;
};

struct VSOutput
{
    float4 position : SV_Position;
    float4 color    : COLOR0;
};

VSOutput VSMain(VSInput input)
{
    VSOutput output;
    output.position = float4(input.position, 1.0f);
    output.color = input.color;
    return output;
}
```

## 32. HLSL Pixel Shader

```hlsl
float4 PSMain(VSOutput input) : SV_Target0
{
    return input.color;
}
```

`SV_Target0`はRTV Slot 0へ対応します。

## 33. Semantic

POSITION、NORMAL、TEXCOORD、COLOR等のSemantic Name/IndexをInput Layoutと一致させます。

## 34. SV_Position

Vertex Shader出力のClip-space位置を表すSystem-value Semanticです。

## 35. Interpolation

VS出力からPS入力へRasterizerが補間します。整数ID等には`nointerpolation`が必要な場合があります。

## 36. CPU Vertex構造体

```cpp
struct Vertex
{
    DirectX::XMFLOAT3 position;
    DirectX::XMFLOAT4 color;
};

static_assert(sizeof(Vertex) == 28);
```

PaddingとStrideを確認します。

## 37. Input Element

```cpp
D3D12_INPUT_ELEMENT_DESC inputElements[] =
{
    {
        "POSITION", 0,
        DXGI_FORMAT_R32G32B32_FLOAT,
        0, 0,
        D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
        0
    },
    {
        "COLOR", 0,
        DXGI_FORMAT_R32G32B32A32_FLOAT,
        0, 12,
        D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
        0
    }
};
```

## 38. Aligned Byte Offset

Field Offsetを明示するか`D3D12_APPEND_ALIGNED_ELEMENT`を使います。C++ Layoutを`offsetof`で検証します。

## 39. Input Slot

複数Vertex Buffer Streamを使う場合にSlotを分けます。`IASetVertexBuffers`のSlotと一致させます。

## 40. Per-instance Data

`D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA`とInstance Data Step Rateを設定します。

## 41. Shader Inputとの照合

FormatのComponent数/型、Semantic、IndexがVS入力と互換である必要があります。

## 42. Graphics PSO Desc

```cpp
D3D12_GRAPHICS_PIPELINE_STATE_DESC pso{};
pso.pRootSignature = rootSignature.Get();
pso.VS = ShaderBytecode(vertexShader.Get());
pso.PS = ShaderBytecode(pixelShader.Get());
pso.InputLayout = { inputElements, static_cast<UINT>(std::size(inputElements)) };
```

HelperのLifetimeに注意します。

## 43. Shader Bytecode

```cpp
D3D12_SHADER_BYTECODE ShaderBytecode(IDxcBlob* blob)
{
    return {
        blob->GetBufferPointer(),
        blob->GetBufferSize()
    };
}
```

PSO作成呼出し中はBlobが生存している必要があります。

## 44. Root Signature

PSOが使うShaderのBinding契約と互換なRoot Signatureを指定します。

## 45. Blend State

Render TargetごとのBlend Enable、Source/Destination Factor、Operation、Write Mask等を指定します。

## 46. Opaque Blend

Blend無効、全Channel Writeを基本とします。Default State Helperを自前で明示できます。

## 47. Alpha Blend

Straight/Premultiplied AlphaをTexture/Shader出力とBlend Factorで一致させます。

## 48. Independent Blend

複数RTVごとに異なるBlend設定を使う場合に有効にします。

## 49. Logic Op

Blendとは別の論理演算です。Support/Format/用途を確認します。

## 50. Sample Mask

```cpp
pso.SampleMask = UINT_MAX;
```

通常は全Sampleを有効にします。

## 51. Rasterizer State

Fill Mode、Cull Mode、Front Counter-clockwise、Depth Bias、Depth Clip、MSAA等を指定します。

## 52. Cull Mode

Back/Front/NoneをMeshのWinding規約に合わせます。座標系とNegative Scaleも考慮します。

## 53. Front Counter-clockwise

Front FaceのWindingを定義します。Model Import/Projectionだけを見て推測せず、Engine規約を固定します。

## 54. Depth Bias

Shadow Map等で使います。Constant/Slope/ClampをPSO Variantへ含めます。

## 55. Conservative Rasterization

対応Tierと用途を確認する発展機能です。通常Triangleに必須ではありません。

## 56. Depth/Stencil State

Depth Enable/Write Mask/Func、Stencil Enable/Read/Write Mask、Front/Back Face Operationを指定します。

## 57. Depth無効のTriangle

最初のClear/TriangleではDepth無効、DSV Format UNKNOWNで構成できます。

## 58. Depth有効

Depth Bufferを使うPSOではDepth StateとDSV Formatを実際のDSV Resource/Viewへ一致させます。

## 59. Reversed-Z

Depth Clear、Comparison、Projection、Near/Farを一貫して変えます。PSO Depth Funcだけ反転しません。

## 60. Primitive Topology Type

```cpp
pso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
```

PSOでは大分類を指定します。

## 61. IA Primitive Topology

Draw前に`IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST)`等の具体Topologyを設定します。

## 62. Topology二段階

PSOのTypeとCommand Listの具体Topologyが互換である必要があります。

## 63. Render Target数

```cpp
pso.NumRenderTargets = 1;
pso.RTVFormats[0] = swapChainFormat;
```

未使用SlotはUNKNOWNにします。

## 64. RTV Format互換

PSO作成時Formatと実際にBindするRTV Formatを一致させます。Swap Chain/HDR/Offscreen切替でVariantが必要です。

## 65. DSV Format

Depth未使用なら`DXGI_FORMAT_UNKNOWN`、使用時はDSV Formatを指定します。

## 66. Sample Description

```cpp
pso.SampleDesc.Count = 1;
pso.SampleDesc.Quality = 0;
```

RTV/DSVのSample Countと一致させます。

## 67. Sample Count Variant

MSAA Countが変わるとPSO Variantが必要です。Swap Chain自体はFlip ModelでCount 1です。

## 68. Strip Cut Value

Triangle Stripで特定IndexをRestartとして使う設定です。Triangle Listでは通常Disabledです。

## 69. Node Mask

Single-node基本実装では0を使います。

## 70. Cached PSO

以前保存したCached BlobをPSO作成へ渡せます。Device/Driver/内容互換性を扱います。

## 71. PSO Flags

通常は`D3D12_PIPELINE_STATE_FLAG_NONE`です。特殊Flagは対応要件を確認します。

## 72. CreateGraphicsPipelineState

```cpp
ComPtr<ID3D12PipelineState> pipelineState;
ThrowIfFailed(
    device->CreateGraphicsPipelineState(
        &pso,
        IID_PPV_ARGS(&pipelineState)),
    "ID3D12Device::CreateGraphicsPipelineState");

pipelineState->SetName(L"Triangle Opaque PSO");
```

## 73. PSO作成失敗

Root Signature、Shader、Input、Format、Sample、State互換をDebug LayerとHRESULTで調べます。

## 74. SetPipelineState

```cpp
commandList->SetPipelineState(pipelineState.Get());
```

Draw前にRoot Signatureと共に正しいPSOをBindします。

## 75. Reset時Initial PSO

Command List `Reset`へ初期PSOを渡せます。最初に使用するPSOが決まる場合に利用できます。

## 76. Compute PSO Desc

```cpp
D3D12_COMPUTE_PIPELINE_STATE_DESC compute{};
compute.pRootSignature = computeRootSignature.Get();
compute.CS = ShaderBytecode(computeShader.Get());
```

## 77. CreateComputePipelineState

```cpp
ComPtr<ID3D12PipelineState> computePso;
ThrowIfFailed(
    device->CreateComputePipelineState(
        &compute,
        IID_PPV_ARGS(&computePso)),
    "Create Compute PSO");
```

## 78. Pipeline State Stream

SubobjectをStreamとして記述する拡張可能なPSO生成方式があります。新しいPipeline Stage/State対応で利用します。

## 79. Stream Subobject

TypeとDataをAlignmentどおり配置します。Helper Template等でLayout Errorを避けます。

## 80. PSO Key

```cpp
struct GraphicsPsoKey
{
    ShaderId vs;
    ShaderId ps;
    RootSignatureId root;
    InputLayoutId input;
    BlendStateId blend;
    RasterizerStateId rasterizer;
    DepthStateId depth;
    RenderTargetLayoutId targets;
};
```

## 81. Keyへ含めるもの

PSO内容へ影響する全Fieldを含めます。Pointer Addressだけでは再起動後の安定Keyになりません。

## 82. Hash Collision

Hashだけで同一と判断せず、Key本体比較も行います。

## 83. PSO Cache

同じKeyのPSOを再利用し、Draw/Frameごとに作成しません。

## 84. Thread-safe Cache

複数Loading Workerが同じPSOを重複生成しないよう、Concurrent Map/Future/Single-flight等を使います。

## 85. Lazy Creation

必要時に生成できますが、戦闘中初回Stutterの原因になります。Loading/Warm-upと組み合わせます。

## 86. Prewarm

Level/Character/Effectで必要なPSO Variantを事前生成します。全組合せを無制限に作りません。

## 87. Pipeline Library

`ID3D12PipelineLibrary`でPipelineを名前/Blobから保存・Loadできます。Driver互換性と失敗Fallbackを扱います。

## 88. StorePipeline

作成済みPSOをLibraryへ登録します。Nameの一意性とVersionを管理します。

## 89. LoadGraphicsPipeline

LibraryからDescとNameを使ってPSOをLoadできます。失敗したら通常作成へFallbackします。

## 90. Library Serialize

Library Blobを取得しFileへ保存できます。破損、Version、Adapter/Driver変更時に破棄します。

## 91. Cache Header

Engine Version、Shader Compiler Version、Adapter LUID/Vendor/Device、Driver、Schema Hash等を保存します。

## 92. Cached Blob

`ID3D12PipelineState::GetCachedBlob`を保存し、次回Descへ渡す方式もあります。

## 93. Cacheは正しさの必須条件ではない

Cacheなし/無効でも通常PSO生成で動作するFallbackを持ちます。

## 94. Shader Variant

Skinning、Alpha Test、Normal Map、Shadow Pass等のFeature組合せをDefine/Separate Shaderで作ります。

## 95. Variant Explosion

Boolean Feature数に対し組合せが指数的に増えます。Runtime Branch、Data-driven Feature、Pass分割とのTrade-offを検討します。

## 96. Permutation Domain

有効な組合せだけを型/Ruleで列挙し、無意味なVariantをBuildしません。

## 97. MaterialとPSO

Material Parameter値は通常PSO Keyへ含めず、Shader/Blend/Cull/Depth等Pipelineを変えるFeatureだけ含めます。

## 98. Transparent Variant

Blend、Depth Write、Cull、Sort RuleがOpaqueと異なります。Material分類から適切なPSOを選びます。

## 99. Shadow Variant

Pixel Shader省略/Alpha Test、Depth Bias、DSV Format、RTV数0等の専用PSOを使います。

## 100. Hot Reload

Source変更をCompileし、新Bytecodeで新PSOを作り、成功後にAtomic/Frame境界でHandleを切り替えます。

## 101. Compile失敗時

現在の正常Shader/PSOを維持し、ErrorをEditor/Logへ表示します。描画全体を失わないようにします。

## 102. Layout変更

Shader Resource Binding/Inputが変わった場合、Root Signature/Material/Input Layout互換を再検証します。

## 103. 旧PSO Lifetime

GPUが旧PSOを使用中なら即解放せず、最終使用Fence後にDeferred Releaseします。

## 104. Device Lost

Shader Bytecode、PSO Desc/Key、Root Signature MetadataからPSOを再生成します。

## 105. Triangle描画準備

Vertex Buffer Resource/View、Viewport、Scissor、RTV、Root Signature、PSOが必要です。

## 106. Triangle Command順

```text
PRESENT -> RENDER_TARGET Barrier
Set RTV / Viewport / Scissor
Clear
Set Root Signature
Set PSO
IASetPrimitiveTopology
IASetVertexBuffers
DrawInstanced(3, 1, 0, 0)
RENDER_TARGET -> PRESENT Barrier
```

## 107. DrawInstanced

```cpp
commandList->DrawInstanced(
    3,
    1,
    0,
    0);
```

Vertex Count、Instance Count、Start Vertex、Start Instanceの順です。

## 108. PSOとDynamic State

Viewport、Scissor、Primitive Topology詳細、Descriptor/Root Argument等はCommand Listで別設定します。PSOに全状態が含まれるわけではありません。

## 109. Debug Layer

PSO/Root互換、RTV Format、Input Layout、未設定State等を確認します。作成時とDraw時のMessageを両方見ます。

## 110. PIX

Drawを選び、PSO、Shader Source/DXIL、Input、Root Binding、Rasterizer、Depth/Blend、Outputを確認します。

## 111. Unit Test

PSO Key Equality/Hash、Variant Rule、Input Offset、RT Format Layout、Cache Header、Dependency HashをTestします。

## 112. Integration Test

色付きTriangleを描画し、Shader/PSO Variant、Cull、Blend、RTV Formatを切り替えて期待画像を比較します。

## 113. よくある失敗：Compile Error未表示

Statusだけ失敗しError Blobを捨てます。Path/Entry/Target/Argumentsと全文をLogします。

## 114. よくある失敗：Debug Shaderで性能測定

`-Od`等のBuildを計測します。Release相当の最適化DXILで測ります。

## 115. よくある失敗：Input Offset不一致

C++ Padding/Field順とInput Layoutがずれます。`offsetof`、`sizeof`、Reflectionで検証します。

## 116. よくある失敗：RTV Format不一致

PSOにUNORM、実RTVに別Format等をBindします。Render Target LayoutをPSO Keyへ含めます。

## 117. よくある失敗：Topology不一致

PSOはTriangle Type、IAはLine List等を設定します。両方を対応させます。

## 118. よくある失敗：毎Draw PSO生成

作成Cost/Stutterを生みます。CacheしLoading/Warm-upで作ります。

## 119. よくある失敗：旧PSO即解放

Hot Reload直後にGPU参照中Objectを解放します。Fence付きDeferred Releaseを使います。

## 120. 実装Checklist

- [ ] DXC Utils/Compiler/Include Handlerを生成できる。
- [ ] Entry/Target/Argumentsを明示する。
- [ ] Error OutputとCompile Statusを確認する。
- [ ] DXIL/PDB/Reflection/Dependencyを管理する。
- [ ] HLSL SemanticとInput Layoutを一致させる。
- [ ] Root SignatureとShader Bindingを検証する。
- [ ] Blend/Rasterizer/Depth Stateを明示する。
- [ ] RTV/DSV FormatとSample CountをPSOへ含める。
- [ ] PSO Key/Cache/Variant Ruleを持つ。
- [ ] Runtime初回作成Stutterを計測する。
- [ ] Hot Reload失敗時に旧PSOを維持する。
- [ ] 旧PSOをFence後に解放する。

## 121. 理解確認問題

1. D3D12でStateをPSOへまとめる理由を説明してください。
2. DXC API成功とShader Compile成功の違いを説明してください。
3. Input LayoutとVS Semanticを一致させる方法を説明してください。
4. PSO RTV Formatが実RTVと一致すべき理由を説明してください。
5. PSO Topology TypeとIA Topologyの違いを説明してください。
6. PSO Cache Keyへ含める情報を挙げてください。
7. Variant Explosionを抑える方法を説明してください。
8. Hot Reloadで旧PSOを即解放できない理由を説明してください。

## 122. 章末要点

- DXCでHLSLをCompileし、Error/Warning、DXIL、PDB、Reflectionを管理します。
- Graphics PSOはRoot Signature、Shader、Input、Blend、Rasterizer、Depth、Target Formatをまとめます。
- Shader/Input/Root/RTV/DSV/Sampleの互換性を作成前後に検証します。
- PSOは不変ObjectとしてKey/Cacheし、Drawごとに生成しません。
- Variant数をRuleで制限し、必要PSOをLoading/Warm-upします。
- Pipeline Library/Cached Blobは無効時の通常生成Fallbackを持たせます。
- Hot Reloadは新PSO成功後に切替え、旧ObjectをFence後に解放します。

## 123. 公式資料

- [Managing graphics pipeline state](https://learn.microsoft.com/en-us/windows/win32/direct3d12/managing-graphics-pipeline-state-in-direct3d-12)
- [D3D12_GRAPHICS_PIPELINE_STATE_DESC](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/ns-d3d12-d3d12_graphics_pipeline_state_desc)
- [ID3D12Device::CreateGraphicsPipelineState](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12device-creategraphicspipelinestate)
- [ID3D12Device::CreateComputePipelineState](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12device-createcomputepipelinestate)
- [ID3D12GraphicsCommandList::SetPipelineState](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12graphicscommandlist-setpipelinestate)
- [ID3D12PipelineLibrary](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nn-d3d12-id3d12pipelinelibrary)
- [DirectX Shader Compiler repository](https://github.com/microsoft/DirectXShaderCompiler)
- [IDxcCompiler3](https://learn.microsoft.com/en-us/windows/win32/api/dxcapi/nn-dxcapi-idxccompiler3)
- [HLSL Shader Model 6](https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/hlsl-shader-model-6-0-features-for-direct3d-12)
- [Input layout description](https://learn.microsoft.com/en-us/windows/win32/direct3d12/d3d12-input-layout)

次章では、Resource Description、Heap Type、Committed/Placed Resource、Upload Buffer、Suballocation、GPU Virtual Address、Memory Lifetimeを扱います。
