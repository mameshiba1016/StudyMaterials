# DirectX 11：HLSL・Shader Compile・Reflection

この章では、GPU ProgramをHLSLで記述し、Direct3D 11が使用できるShader BytecodeへCompileする流れを学びます。Source、Entry Point、Target Profile、Macro、Include、Compiler Flag、Error Blob、事前Compile、Cache、Reflectionを分離して理解します。

## 1. HLSLとは

HLSLはHigh-Level Shader Languageの略で、Direct3DのProgrammable Pipeline Stageを記述する言語です。C++に似た構文がありますが、実行場所、型、Memory、並列実行規則は異なります。

## 2. CPU Programとの違い

```text
C++  : CPU上でGame全体、Resource管理、Command発行
HLSL : GPU上で頂点・Pixel・Threadなど大量の要素を並列処理
```

HLSL関数をC++関数のように直接呼ぶのではなく、CompileしたShader ObjectをPipelineへBindingしてDrawします。

## 3. Shader Stage

- Vertex Shader：頂点ごとの変換。
- Hull Shader：Tessellation制御。
- Domain Shader：Tessellation後の頂点生成。
- Geometry Shader：Primitive単位処理。
- Pixel Shader：RasterizeされたFragmentの色計算。
- Compute Shader：Drawから独立した汎用並列処理。

## 4. 最小Vertex Shader

```hlsl
struct VSInput
{
    float3 position : POSITION;
};

struct VSOutput
{
    float4 position : SV_Position;
};

VSOutput VSMain(VSInput input)
{
    VSOutput output;
    output.position = float4(input.position, 1.0f);
    return output;
}
```

## 5. 最小Pixel Shader

```hlsl
float4 PSMain() : SV_Target0
{
    return float4(0.2f, 0.6f, 1.0f, 1.0f);
}
```

`SV_Target0`はOutput MergerのRender Target Slot 0へ対応します。

## 6. Entry Point

Compile開始地点となる関数名です。同じHLSL Fileに`VSMain`と`PSMain`を置き、それぞれ別TargetでCompileできます。

## 7. Target Profile

```text
vs_5_0 = Vertex Shader、Shader Model 5.0
ps_5_0 = Pixel Shader、Shader Model 5.0
cs_5_0 = Compute Shader、Shader Model 5.0
```

StageとShader Modelを文字列で指定します。

## 8. Feature Levelとの関係

Device Feature LevelとTarget Profileは関連しますが同じ値ではありません。対象Feature Levelが保証するShader Modelと機能を確認してTargetを決めます。

## 9. Semantic

```hlsl
float3 position : POSITION;
float2 uv       : TEXCOORD0;
float4 position : SV_Position;
```

値の意味とPipeline間の対応を示します。文字列を飾りとして付けているのではありません。

## 10. System-value Semantic

`SV_Position`、`SV_Target`、`SV_VertexID`など`SV_`で始まるSemanticはPipelineが特別な意味を持つ値です。

## 11. HLSLのScalar型

`bool`、`int`、`uint`、`half`、`float`、`double`などがあります。ただし実際の精度や対応はTargetと演算によるため、型名だけで性能を断定しません。

## 12. Vector型

```hlsl
float2 uv;
float3 normal;
float4 color;
```

成分は`.x/.y/.z/.w`または`.r/.g/.b/.a`で参照できます。

## 13. Swizzle

```hlsl
float2 xy = value.xy;
float3 rgb = color.bgr;
float4 copy = scalar.xxxx;
```

成分の抽出・並べ替えを簡潔に書けます。

## 14. Matrix型

```hlsl
float4x4 worldViewProjection;
```

CPU側の行列Layout、転置、`mul`の引数順をRenderer規約として統一します。

## 15. mul

```hlsl
float4 clipPosition = mul(float4(position, 1.0f), worldViewProjection);
```

Vectorを左に置くか右に置くかで意味が変わります。C++側の行列規約と組み合わせて決めます。

## 16. Constant Buffer

```hlsl
cbuffer PerObject : register(b0)
{
    float4x4 worldViewProjection;
    float4 baseColor;
};
```

CPUから頻繁に渡す小さな定数をまとめます。Packing規則はC++の単純な構造体配置と常に同じではありません。

## 17. Resource Register

```hlsl
Texture2D colorTexture : register(t0);
SamplerState linearSampler : register(s0);
RWTexture2D<float4> outputTexture : register(u0);
```

`b`はConstant Buffer、`t`はShader Resource、`s`はSampler、`u`はUAV Slotです。

## 18. 明示Registerの利点

Binding契約がSource上で見えます。一方で大規模RendererではSlot衝突を避ける共通規約や自動生成が必要です。

## 19. Source Fileの拡張子

通常`.hlsl`を使います。拡張子だけでStageは決まらず、Entry PointとTarget Profileが決めます。

## 20. Compilerの選択

Direct3D 11のShader Model 5系ではFXC系CompilerとDXBC Bytecodeが基本です。DXCは新しいCompilerですが、Shader Model 6のDXILをDirect3D 11へそのまま渡す設計とは分けて考えます。

## 21. D3DCompile

```cpp
HRESULT D3DCompile(
    const void* sourceData,
    SIZE_T sourceSize,
    const char* sourceName,
    const D3D_SHADER_MACRO* defines,
    ID3DInclude* includeHandler,
    const char* entryPoint,
    const char* target,
    UINT flags1,
    UINT flags2,
    ID3DBlob** code,
    ID3DBlob** errors);
```

各引数を名前で読めるようにします。

## 22. ID3DBlob

連続したBinary Dataを所有するCOM Objectです。Compile成功時のBytecodeと、警告・Error Messageの両方に使われます。

## 23. BlobのData取得

```cpp
const void* data = blob->GetBufferPointer();
const SIZE_T size = blob->GetBufferSize();
```

Pointerの寿命はBlobの寿命に依存します。

## 24. FileからCompileするAPI

```cpp
Microsoft::WRL::ComPtr<ID3DBlob> bytecode;
Microsoft::WRL::ComPtr<ID3DBlob> messages;

HRESULT hr = D3DCompileFromFile(
    path.c_str(),
    nullptr,
    D3D_COMPILE_STANDARD_FILE_INCLUDE,
    "VSMain",
    "vs_5_0",
    compileFlags,
    0,
    bytecode.GetAddressOf(),
    messages.GetAddressOf());
```

## 25. Source Name

Memory上のSourceを`D3DCompile`する場合も、診断用のFile名を渡すとError Messageに場所が表示されやすくなります。

## 26. Error Blobは失敗時だけではない

Compile成功でもWarningが入る場合があります。`SUCCEEDED(hr)`だけを見てMessage Blobを捨てず、内容があればLogへ出します。

## 27. Messageを安全に読む

```cpp
if (messages)
{
    const auto* text = static_cast<const char*>(
        messages->GetBufferPointer());
    const SIZE_T size = messages->GetBufferSize();
    LogCompilerMessage(std::string_view{text, size});
}
```

Null終端を決めつけずSizeも使えます。

## 28. HRESULTの処理順

```text
call compiler
-> output message blob if present
-> check HRESULT
-> verify bytecode exists
-> create shader object
```

Messageを出す前に例外を投げると、肝心の行番号を失います。

## 29. Debug Compile Flag

```cpp
UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined(_DEBUG)
flags |= D3DCOMPILE_DEBUG;
flags |= D3DCOMPILE_SKIP_OPTIMIZATION;
#endif
```

Debug情報と最適化抑制により解析しやすくします。

## 30. Release Compile Flag

```cpp
UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
flags |= D3DCOMPILE_OPTIMIZATION_LEVEL3;
```

Release用Bytecodeは最適化し、Debug版と別Artifactとして管理します。

## 31. WarningをErrorとして扱う

`D3DCOMPILE_WARNINGS_ARE_ERRORS`でWarningをBuild失敗にできます。段階的に導入し、既知Warningを放置しない運用にします。

## 32. Strictness

`D3DCOMPILE_ENABLE_STRICTNESS`は厳格なCompile規則を有効にします。古い曖昧なShaderを温存せず、基本Flagとして扱います。

## 33. Optimization Level

0から3までの最適化Levelがあります。Compile時間、Debugしやすさ、実行性能の目的に合わせます。

## 34. Macro定義

```cpp
const D3D_SHADER_MACRO macros[] =
{
    {"USE_SKINNING", "1"},
    {"MAX_LIGHTS", "8"},
    {nullptr, nullptr}
};
```

最後はNull Sentinelです。

## 35. Shader Variant

Macroの組合せでSkinning有無、Light数、QualityなどをCompileできます。しかし組合せ爆発を招くため、Variant Keyと利用数を管理します。

## 36. Macro値は文字列

数値も`"8"`のような文字列で渡します。C++の一時文字列Pointerを保存して寿命切れにしないようにします。

## 37. Include

```hlsl
#include "Common.hlsli"
```

共通構造体、定数、関数を分割できます。循環Includeと名前衝突を避けます。

## 38. Standard File Include

`D3D_COMPILE_STANDARD_FILE_INCLUDE`は標準File解決を使います。信頼できないPathをそのまま受け入れるAsset Pipelineにはしません。

## 39. Custom ID3DInclude

Virtual File System、Package、Dependency追跡を使う場合は`ID3DInclude::Open/Close`を実装します。返したMemoryの寿命を`Close`まで保証します。

## 40. Include Guard

```hlsl
#ifndef COMMON_HLSLI
#define COMMON_HLSLI
// declarations
#endif
```

同じDeclarationの多重展開を防ぎます。

## 41. Compile結果からShaderを作る

```cpp
Microsoft::WRL::ComPtr<ID3D11VertexShader> shader;
ThrowIfFailed(device->CreateVertexShader(
    bytecode->GetBufferPointer(),
    bytecode->GetBufferSize(),
    nullptr,
    shader.GetAddressOf()));
```

CompileとDevice Object作成は別工程です。

## 42. Bytecodeを保持する理由

Vertex Shader BytecodeはInput Layout作成に必要です。Shader Object作成直後に必ず捨ててよいとは限りません。

## 43. Pixel Shader作成

```cpp
Microsoft::WRL::ComPtr<ID3D11PixelShader> shader;
ThrowIfFailed(device->CreatePixelShader(
    bytecode->GetBufferPointer(),
    bytecode->GetBufferSize(),
    nullptr,
    shader.GetAddressOf()));
```

Stageに合ったCreate Methodを使います。

## 44. CompileとBindingを混同しない

Shaderを作成してもPipelineでは有効になりません。`VSSetShader`や`PSSetShader`でContextへBindingします。

## 45. Runtime Compile

Application実行中にHLSL SourceをCompileします。開発中のHot Reloadには便利ですが、Compile待ち、Compiler DLL配布、Source公開、環境差が生じます。

## 46. Offline Compile

Build工程でBytecodeを生成し、Gameは完成Binaryだけを読みます。製品実行時の停止とCompiler依存を減らせます。

## 47. 推奨する分離

```text
development: source + dependency watch + compile + hot reload
shipping: validated precompiled bytecode + manifest
```

同じCompiler設定を再現できるBuild Toolを用意します。

## 48. Shader Cache Key

```text
source content hash
include dependency hashes
entry point
target profile
macro set
compiler version
compile flags
```

File更新時刻だけではInclude変更やBranch差を取り逃す場合があります。

## 49. Cacheの原子更新

Compile成功したBytecodeだけを一時Fileから置換します。失敗した結果で最後の正常Shaderを壊さないようにします。

## 50. Hot Reload

変更を検出してBackgroundでCompileし、成功したShader Objectを安全なFrame境界で交換します。失敗時は旧Shaderを維持しMessageを表示します。

## 51. Threading

Deviceの作成MethodはThread Safeな設計を利用できますが、Compiler、File Watcher、Renderer交換の所有権を明確にします。Context BindingはRender Threadへ渡します。

## 52. Reflectionとは

Compile済みBytecodeからInput Parameter、Constant Buffer、Resource Binding、Instruction統計などのMetadataを調べる仕組みです。

## 53. D3DReflect

```cpp
Microsoft::WRL::ComPtr<ID3D11ShaderReflection> reflection;
ThrowIfFailed(D3DReflect(
    bytecode->GetBufferPointer(),
    bytecode->GetBufferSize(),
    IID_PPV_ARGS(reflection.GetAddressOf())));
```

## 54. Shader Description

```cpp
D3D11_SHADER_DESC desc{};
ThrowIfFailed(reflection->GetDesc(&desc));
```

Input/Output Parameter数、Constant Buffer数、Bound Resource数などを取得できます。

## 55. Input Parameter Reflection

```cpp
for (UINT i = 0; i < desc.InputParameters; ++i)
{
    D3D11_SIGNATURE_PARAMETER_DESC input{};
    ThrowIfFailed(reflection->GetInputParameterDesc(i, &input));
    Log(input.SemanticName, input.SemanticIndex, input.Mask);
}
```

Vertex Input Layoutの検証や生成に利用できます。

## 56. Semantic Nameの寿命

Reflection Descriptor内の文字列PointerはReflection Objectに依存します。長期保存するなら自分の`std::string`へCopyします。

## 57. Bound Resource Reflection

```cpp
for (UINT i = 0; i < desc.BoundResources; ++i)
{
    D3D11_SHADER_INPUT_BIND_DESC binding{};
    ThrowIfFailed(reflection->GetResourceBindingDesc(i, &binding));
    Log(binding.Name, binding.Type, binding.BindPoint, binding.BindCount);
}
```

Texture、Sampler、Constant BufferのSlot契約を検査できます。

## 58. Constant Buffer Reflection

名前からConstant Bufferを取得し、Size、Variable数、各VariableのOffsetとSizeを調べられます。

## 59. C++構造体との照合

Reflectionで得たBuffer Sizeと`sizeof(CpuConstants)`を比較します。Variable Offsetも検査すればPadding不一致を早期発見できます。

## 60. Reflectionを実行時に残すか

EditorやDebug Buildでは強力です。ReleaseではReflection結果を事前Manifestへ変換し、Bytecodeから不要情報を除く設計もあります。

## 61. Reflectionは型安全を自動保証しない

Metadataを取得しただけではC++側Bindingは直りません。期待Schemaと比較し、不一致なら明確に失敗させます。

## 62. Shader Assetの構造

```cpp
struct ShaderBytecode
{
    std::vector<std::byte> bytes;
    std::string entryPoint;
    std::string target;
    std::uint64_t variantKey = 0;
};
```

Byte列だけでなく再現と診断に必要なMetadataを持ちます。

## 63. Compile結果型

```cpp
struct ShaderCompileResult
{
    Microsoft::WRL::ComPtr<ID3DBlob> bytecode;
    std::string messages;
    HRESULT result = E_FAIL;
};
```

例外だけに頼らず、Tool UIへMessageを返せる形です。

## 64. Source Encoding

Compilerへ渡すByte列のEncoding、BOM、File PathのWide/UTF-8変換を統一します。日本語Commentを使う場合もBuild環境差を確認します。

## 65. Error Messageに含める情報

```text
canonical source path
entry point
target profile
macro set
compiler flags
include stack
compiler output
```

「Compile失敗」だけでは修正できません。

## 66. よくある失敗：Targetを取り違える

Vertex Entryを`ps_5_0`でCompileするなどStageが一致しません。Shader Asset定義でEntryとTargetを一組にします。

## 67. よくある失敗：Error Blobを表示しない

HRESULTだけを表示し、HLSLの行番号と内容を捨てます。Messageを先にLogへ出します。

## 68. よくある失敗：Debug Flagを製品へ残す

最適化されない大きなBytecodeを配布します。Build ConfigurationごとのCompiler FlagをTestします。

## 69. よくある失敗：IncludeをCache Keyから除外

共通Fileを直してもShaderが再Compileされません。Dependency GraphとContent Hashを含めます。

## 70. よくある失敗：Hot Reload失敗で旧Shaderを破棄

Compile成功を確認する前に現在ShaderをResetします。新Objectを一時所有し、完全成功後に交換します。

## 71. よくある失敗：行列規約が不一致

C++側転置、HLSLの`mul`順、Matrix Packingが食い違います。単位行列だけでなく回転・移動を含むTestを作ります。

## 72. Compile Test

- 全Entry PointをDebug/Release FlagでCompileする。
- Warningを収集する。
- 意図的なSyntax ErrorでFile名と行番号が出る。
- Include変更でCacheが無効化される。
- Macro Variantごとに期待Bytecodeを得る。

## 73. Reflection Test

- Input SemanticとIndexが期待値に一致する。
- Constant Buffer SizeがC++構造体と一致する。
- ResourceのType、Slot、Countが一致する。
- 不要なBindingを検出する。
- Reflection Object破棄後に文字列Pointerを保持しない。

## 74. Hot Reload Test

- 成功時だけFrame境界で交換する。
- 失敗時に旧Shaderで描画継続する。
- Include変更も検出する。
- 連続保存を統合する。
- Renderer終了時にBackground Jobを安全に止める。

## 75. 完成確認表

- [ ] HLSLとC++の実行場所を区別できる。
- [ ] Entry PointとTarget Profileを説明できる。
- [ ] SemanticとSystem-value Semanticを読める。
- [ ] `D3DCompileFromFile`の各引数を説明できる。
- [ ] Error Blobを成功時も確認できる。
- [ ] Debug/Release Compile Flagを分けられる。
- [ ] Macro、Include、Variantを管理できる。
- [ ] Runtime CompileとOffline Compileを使い分けられる。
- [ ] `D3DReflect`でBinding契約を検査できる。
- [ ] Cache Keyへ全Compile入力を含められる。

## 76. この章の要点

- HLSLはGPU Pipeline Stageを記述する言語です。
- Entry Pointは関数、Target ProfileはStageとShader Modelを指定します。
- Compile結果のBytecodeと診断Messageは別Blobです。
- 成功時にもWarning Blobを確認します。
- 開発時はHot Reload、配布時は検証済み事前Compileが安定します。
- Include、Macro、Compiler VersionをCache Keyへ含めます。
- ReflectionはInput、Constant Buffer、Resource Slotの契約検査に使えます。
- Shader Object作成とContext BindingはCompileとは別工程です。

## 77. 公式資料

- [HLSL reference](https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/dx-graphics-hlsl-reference)
- [D3DCompile](https://learn.microsoft.com/en-us/windows/win32/api/d3dcompiler/nf-d3dcompiler-d3dcompile)
- [D3DCompileFromFile](https://learn.microsoft.com/en-us/windows/win32/api/d3dcompiler/nf-d3dcompiler-d3dcompilefromfile)
- [D3D compiler constants](https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/d3dcompile-constants)
- [ID3DBlob](https://learn.microsoft.com/en-us/windows/win32/api/d3dcommon/nn-d3dcommon-id3d10blob)
- [D3DReflect](https://learn.microsoft.com/en-us/windows/win32/api/d3dcompiler/nf-d3dcompiler-d3dreflect)
- [ID3D11ShaderReflection](https://learn.microsoft.com/en-us/windows/win32/api/d3d11shader/nn-d3d11shader-id3d11shaderreflection)
- [D3D11_SHADER_DESC](https://learn.microsoft.com/en-us/windows/win32/api/d3d11shader/ns-d3d11shader-d3d11_shader_desc)
- [Shader model 5](https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/d3d11-graphics-reference-sm5)
- [Specifying compiler targets](https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/specifying-compiler-targets)

次章では、Compile済みBytecodeからVertex ShaderとPixel Shaderを作成し、各StageへBindingして最初の三角形を描く流れを扱います。
