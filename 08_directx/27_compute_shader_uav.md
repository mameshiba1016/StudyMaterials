# DirectX 11：Compute Shader・UAV

この章では、描画とは独立してGPUへ汎用並列計算を命令するCompute Shaderと、ShaderからResourceを書き換えるためのUAVを学びます。Thread構造、Resource生成、Binding、Dispatch、同期、競合、Counter、間接描画への接続までを扱います。

## 1. Compute Shaderとは

Compute ShaderはVertexやPixelを直接生成する段階ではなく、配列・Texture・Particle等のDataを多数のThreadで処理するProgrammable Stageです。

```text
CPU             GPU Compute Shader              後続処理
resource設定 -> Dispatch(x, y, z) -> buffer更新 -> Draw/次のDispatch
```

## 2. 描画Shaderとの違い

Vertex Shaderには頂点数、Pixel ShaderにはRasterizerが生成したPixel候補があります。一方Compute Shaderでは、実行するThread数とDataへの対応をProgram側が決めます。

## 3. 得意な処理

- Particle更新
- SkinningやAnimation Data生成
- Image Filter、Blur、Bloom
- Tile/Cluster単位のLight分類
- GPU CullingとVisible List生成
- Prefix Sum、Reduction、Sort
- Simulation、Noise、Procedural生成

## 4. 苦手な処理

Data量が少ない処理、分岐が激しくThread間の仕事量が偏る処理、毎回CPUへ結果を戻す処理はGPU化のOverheadが利益を上回ることがあります。

## 5. UAVとは

UAVはUnordered Access Viewの略で、ShaderからResourceを順不同に読み書きするViewです。Resource本体とViewを分けるDirect3D 11の設計はSRVやRTVと同じです。

```text
ID3D11Buffer / ID3D11Texture2D
 ├─ SRV : Shaderから基本的に読み取る
 └─ UAV : Shaderから読み書きする
```

## 6. Unorderedの意味

複数Threadの実行順序は保証されません。「DispatchしたThreadがID順に実行される」という意味ではなく、任意順序のAccessを可能にするという意味です。

## 7. Feature Level

Compute Shader 5.0の完全な機能はDirect3D 11世代を想定します。対応Feature LevelとFormat Supportを確認し、必要な機能を初期化時に検証します。

## 8. 最小HLSL

```hlsl
RWStructuredBuffer<float4> outputData : register(u0);

[numthreads(64, 1, 1)]
void CSMain(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    outputData[dispatchThreadID.x] *= 2.0f;
}
```

`u0`はUAV Slot 0、`numthreads`は一つのThread Group内のThread数です。

## 9. 三段階の実行単位

```text
Dispatch Group数 × numthreads = 論理Thread総数
```

Group、Group内Thread、Dispatch全体Threadを区別することが最初の要点です。

## 10. numthreads

```hlsl
[numthreads(threadX, threadY, threadZ)]
```

Compile時に決まるThread Group寸法です。積がGroup当たりのThread数になります。Hardware上の実行単位やResource制限を考慮して決めます。

## 11. Dispatch

```cpp
context->Dispatch(groupCountX, groupCountY, groupCountZ);
```

引数はThread数ではなくThread Group数です。

## 12. 1次元DataのGroup数

```cpp
constexpr UINT threadsPerGroup = 64;
const UINT groups = (elementCount + threadsPerGroup - 1) / threadsPerGroup;
context->Dispatch(groups, 1, 1);
```

切り上げ除算により端数を含む最後のGroupも実行します。

## 13. 範囲外Guard

```hlsl
cbuffer ComputeConstants : register(b0)
{
    uint elementCount;
    float deltaTime;
    float2 padding;
};

[numthreads(64, 1, 1)]
void CSMain(uint3 id : SV_DispatchThreadID)
{
    if (id.x >= elementCount)
        return;

    // 有効範囲だけ処理する。
}
```

切り上げた最後のGroupには余分なThreadが含まれるため、Buffer境界外を必ず防ぎます。

## 14. SV_DispatchThreadID

Dispatch全体における一意な3次元Thread座標です。通常は配列IndexやTexture座標へ直接対応させます。

## 15. SV_GroupID

現在のThread Group座標です。Tile単位処理やGroupごとの出力位置を決める際に使用します。

## 16. SV_GroupThreadID

一つのGroup内におけるThread座標です。`groupshared`配列のIndexへよく使います。

## 17. SV_GroupIndex

Group内の3次元座標を一次元へ線形化したIndexです。Group内共有配列を一次元で扱うときに便利です。

## 18. Structured Buffer

要素ごとに同じ構造を持つBufferです。CPU側の`StructureByteStride`とHLSL側構造体のSize/Layoutを一致させます。

```hlsl
struct Particle
{
    float3 position;
    float life;
    float3 velocity;
    float size;
};

StructuredBuffer<Particle> inputParticles : register(t0);
RWStructuredBuffer<Particle> outputParticles : register(u0);
```

## 19. CPU側構造体

```cpp
struct ParticleGPU
{
    DirectX::XMFLOAT3 position;
    float life;
    DirectX::XMFLOAT3 velocity;
    float size;
};

static_assert(sizeof(ParticleGPU) == 32);
```

HLSLとのField順、Size、Alignmentを明示的に確認します。

## 20. Structured Buffer生成

```cpp
D3D11_BUFFER_DESC desc{};
desc.ByteWidth = sizeof(ParticleGPU) * particleCapacity;
desc.Usage = D3D11_USAGE_DEFAULT;
desc.BindFlags = D3D11_BIND_SHADER_RESOURCE |
                 D3D11_BIND_UNORDERED_ACCESS;
desc.CPUAccessFlags = 0;
desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
desc.StructureByteStride = sizeof(ParticleGPU);

ComPtr<ID3D11Buffer> particleBuffer;
ThrowIfFailed(device->CreateBuffer(&desc, nullptr, &particleBuffer));
```

## 21. Structured BufferのSRV

```cpp
D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
srvDesc.Format = DXGI_FORMAT_UNKNOWN;
srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
srvDesc.Buffer.FirstElement = 0;
srvDesc.Buffer.NumElements = particleCapacity;

ComPtr<ID3D11ShaderResourceView> particleSRV;
ThrowIfFailed(device->CreateShaderResourceView(
    particleBuffer.Get(), &srvDesc, &particleSRV));
```

Structured BufferのFormatは通常`DXGI_FORMAT_UNKNOWN`です。

## 22. Structured BufferのUAV

```cpp
D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
uavDesc.Format = DXGI_FORMAT_UNKNOWN;
uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
uavDesc.Buffer.FirstElement = 0;
uavDesc.Buffer.NumElements = particleCapacity;
uavDesc.Buffer.Flags = 0;

ComPtr<ID3D11UnorderedAccessView> particleUAV;
ThrowIfFailed(device->CreateUnorderedAccessView(
    particleBuffer.Get(), &uavDesc, &particleUAV));
```

## 23. Raw Buffer

ByteAddressBufferとRWByteAddressBufferはByte Offsetで32-bit値を読み書きします。異種DataやAtomic用の低水準表現に向きますが、型とOffset管理の責任が増えます。

## 24. Typed Buffer

`Buffer<float4>`や`RWBuffer<uint>`はViewのDXGI Formatによって要素型が決まります。Structured、Raw、Typedを用途に合わせて区別します。

## 25. RWTexture2D

```hlsl
Texture2D<float4> sourceTexture : register(t0);
RWTexture2D<float4> resultTexture : register(u0);

[numthreads(8, 8, 1)]
void CSMain(uint3 id : SV_DispatchThreadID)
{
    resultTexture[id.xy] = sourceTexture.Load(int3(id.xy, 0));
}
```

Image処理は2次元Thread配置がData構造と対応しやすくなります。

## 26. UAV対応Texture生成

Texture作成時に`D3D11_BIND_UNORDERED_ACCESS`を含めます。使用FormatがUAVで必要な操作をSupportするか確認します。

```cpp
textureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE |
                        D3D11_BIND_UNORDERED_ACCESS;
```

## 27. Compute Shader生成

```cpp
ComPtr<ID3DBlob> bytecode = CompileShader(
    L"particle_update.hlsl", "CSMain", "cs_5_0");

ComPtr<ID3D11ComputeShader> computeShader;
ThrowIfFailed(device->CreateComputeShader(
    bytecode->GetBufferPointer(),
    bytecode->GetBufferSize(),
    nullptr,
    &computeShader));
```

Compile Error Blobは省略せずLogへ出します。

## 28. Binding順序

```cpp
context->CSSetShader(computeShader.Get(), nullptr, 0);
context->CSSetConstantBuffers(0, 1, constants.GetAddressOf());
context->CSSetShaderResources(0, 1, inputSRV.GetAddressOf());
context->CSSetUnorderedAccessViews(0, 1, outputUAV.GetAddressOf(), nullptr);
context->Dispatch(groups, 1, 1);
```

HLSLの`b#`、`t#`、`u#`とAPI Slotを一致させます。

## 29. UAV Initial Count

`CSSetUnorderedAccessViews`の最後の引数はAppend/ConsumeまたはCounter付きUAVのCounter初期値を指定できます。`nullptr`はCounterを変更しない指定です。

## 30. Unbind

```cpp
ID3D11ShaderResourceView* nullSRV = nullptr;
ID3D11UnorderedAccessView* nullUAV = nullptr;
context->CSSetShaderResources(0, 1, &nullSRV);
context->CSSetUnorderedAccessViews(0, 1, &nullUAV, nullptr);
context->CSSetShader(nullptr, nullptr, 0);
```

同じResourceを後続Passで別用途へBindする前に明示的に外します。

## 31. SRVとUAVの同時Binding禁止

同じSubresourceを入力SRVと出力UAVへ同時にBindしてはいけません。Debug LayerのWarningを確認し、Ping-Pong Resourceを使用します。

## 32. Ping-Pong Buffer

```text
pass 0 : AをSRVで読む -> BをUAVへ書く
pass 1 : BをSRVで読む -> AをUAVへ書く
```

前Frame結果を次Frame入力へするSimulationに適します。

## 33. Dispatch間の可視性

Direct3D 11 Immediate Contextへ順番にCommandを発行すれば後続Commandとの順序は保たれます。ただしBinding Hazardを避けるためViewを切り替え、Thread Group内部では必要なBarrierをShaderに書きます。

## 34. groupshared Memory

```hlsl
groupshared float sharedValues[64];
```

同一Group内だけで共有できる高速なMemoryです。Group間では共有できず、容量にも上限があります。

## 35. GroupMemoryBarrierWithGroupSync

```hlsl
sharedValues[SV_GroupIndex] = sourceValue;
GroupMemoryBarrierWithGroupSync();
// ここから全Threadが書き終えたsharedValuesを読む。
```

Group内のMemory操作を同期し、全ThreadがBarrierへ到達するまで待ちます。

## 36. Barrierと分岐

一部ThreadだけがBarrierへ到達する分岐は未定義動作や停止の原因になります。BarrierはGroup内の全Threadが到達するControl Flowへ置きます。

## 37. Group間同期はできない

一つのDispatch内でGroup AがGroup Bの完了を待つ一般的なBarrierはありません。段階を分ける必要がある場合は複数Dispatchに分割します。

## 38. Race Condition

複数Threadが同じ場所へ読み書きすると、実行順序により結果が変わります。Threadごとに固有出力を割り当てるかAtomic操作を使います。

## 39. Interlocked操作

```hlsl
RWStructuredBuffer<uint> histogram : register(u0);

uint previous;
InterlockedAdd(histogram[bucket], 1, previous);
```

Atomicは競合を正しく処理しますが、同じAddressへ集中すると直列化されPerformanceが低下します。

## 40. AppendStructuredBuffer

```hlsl
AppendStructuredBuffer<uint> visibleIndices : register(u0);

if (isVisible)
    visibleIndices.Append(objectIndex);
```

書き込み先IndexをCounterから自動取得し、可変長ListをGPU上で生成します。

## 41. Append UAV作成

UAV Descriptorの`Buffer.Flags`へ`D3D11_BUFFER_UAV_FLAG_APPEND`を指定します。HLSL宣言とView Flagの組合せを一致させます。

## 42. Counter付きStructured Buffer

`D3D11_BUFFER_UAV_FLAG_COUNTER`はRWStructuredBufferに隠しCounterを持たせる用途です。Append型との用途とHLSL操作を混同しません。

## 43. Counter Reset

```cpp
UINT initialCount = 0;
context->CSSetUnorderedAccessViews(
    0, 1, visibleUAV.GetAddressOf(), &initialCount);
```

FrameごとにListを作り直す場合はCounterを0へResetします。`UINT(-1)`は現在値を保持する指定です。

## 44. CopyStructureCount

```cpp
context->CopyStructureCount(
    argumentBuffer.Get(),
    byteOffset,
    visibleUAV.Get());
```

UAVの隠しCounterをBufferへCopyし、間接描画引数やReadbackへ利用します。

## 45. Indirect Drawへの接続

GPU CullingでVisible Instance数を作り、Argument BufferへCountを格納すればCPUが個数を読み戻さず`DrawIndexedInstancedIndirect`へ接続できます。

## 46. Indirect Argument Buffer

Buffer作成時に`D3D11_RESOURCE_MISC_DRAWINDIRECT_ARGS`を指定します。APIが要求するArgument LayoutとByte Offsetを厳密に合わせます。

## 47. CPU Readback

GPU結果をCPUで読むにはStaging ResourceへCopyし、完了後にMapします。毎Frame即座にMapするとGPU/CPU同期が発生し、Parallelismを失います。

## 48. 非同期Readback設計

複数のReadback BufferをRing化し、数Frame前の結果を読む設計にすると待機を減らせます。遅延を許容できるDebug統計等に向きます。

## 49. 2D Dispatch寸法

```cpp
constexpr UINT tileX = 8;
constexpr UINT tileY = 8;
const UINT groupsX = (width  + tileX - 1) / tileX;
const UINT groupsY = (height + tileY - 1) / tileY;
context->Dispatch(groupsX, groupsY, 1);
```

Shader側ではWidthとHeightの両方をGuardします。

## 50. Thread Group Size選択

64、128、256 Threadや`8x8`、`16x16`等を候補としてProfileします。Register使用量、groupshared容量、Memory Access、Hardware Wave幅、分岐がOccupancyへ影響します。

## 51. Memory Coalescing

隣接Threadが隣接AddressへAccessするとMemory Transferを効率化しやすくなります。AoSとSoAの選択もAccess Patternに基づいて決めます。

## 52. AoSとSoA

```text
AoS : Particle{position, velocity, life} の配列
SoA : positions[], velocities[], lives[]
```

すべてのFieldを同時に使うならAoS、特定Fieldのみ大量に処理するならSoAが有利な場合があります。計測して決めます。

## 53. Divergence

同じ実行Group内で条件分岐の経路が分かれると、両経路を順番に処理する場合があります。処理種別ごとにDataを分類すると改善することがあります。

## 54. Reduction

総和・最大値等を求める処理です。各Threadが値を読み、groupshared上で段階的に半減加算し、Groupごとの結果を別Dispatchで統合します。

## 55. Prefix Sum

入力列の累積値を作ります。可視要素を隙間なく詰めるCompactionやParticle生成位置計算の基礎Algorithmです。

## 56. Compute Blur

HorizontalとVerticalの二Passへ分けるSeparable Filterにより、二次元KernelのSample数を減らせます。TileとHalo領域をgroupsharedへCacheする発展があります。

## 57. Particle更新例

```hlsl
[numthreads(64, 1, 1)]
void CSMain(uint3 id : SV_DispatchThreadID)
{
    if (id.x >= particleCount) return;

    Particle p = inputParticles[id.x];
    p.velocity += gravity * deltaTime;
    p.position += p.velocity * deltaTime;
    p.life -= deltaTime;
    outputParticles[id.x] = p;
}
```

Simulation更新と描画を分離し、更新後BufferをVertex Shader側から読みます。

## 58. GPU Culling例

各Threadが一ObjectのBoundsをFrustumと比較し、可視ObjectのIndexをAppend Bufferへ追加します。そのListをInstance Data生成やIndirect Drawへ渡します。

## 59. Fast Action描画への応用

大量Effect Particle、Trail Point更新、Decal分類、Light List、Skinning、GPU Culling等を候補にできます。ただしGameplay判定の正解DataをGPUだけへ閉じ込めるとCPU Logicとの同期が難しくなります。

## 60. GameplayとRenderingの境界

Damage判定、敵AI、確定Collision等はCPU側のAuthoritative Stateとして保ち、Compute Shaderは視覚表現や大量の独立計算に使うと責務を整理しやすくなります。

## 61. Determinism

Atomicの追加順や浮動小数点加算順は実行ごとに一致しない可能性があります。完全な再現性が必要なSimulationへ使う場合はAlgorithmと要件を慎重に設計します。

## 62. Debug Layer

SRV/UAV Hazard、無効なFormat、Slot、View範囲等を検出するためDebug Layerを有効にします。Dispatch後のWarningを放置しません。

## 63. PIX・GPU Capture

CaptureでCompute Pass、Dispatch寸法、Binding Resource、UAV内容、実行時間を確認します。出力Bufferを可視化用TextureやDebug Geometryへ変換する方法も有効です。

## 64. Shader Debug用Data

```hlsl
RWStructuredBuffer<uint> debugFlags : register(u1);

if (!all(isfinite(value)))
    InterlockedOr(debugFlags[0], 1);
```

GPU上のNaNや範囲外状態をFlagへ記録し、低頻度でCPUへ読み戻します。

## 65. Timestamp Query

Compute Pass前後にTimestampを置き、GPU時間を計測します。CPUのCommand発行時間だけではGPU Costを判断できません。

## 66. よくある失敗：Dispatch数の誤解

`Dispatch(elementCount, 1, 1)`へすると、`numthreads(64,1,1)`なら必要数の64倍のThreadを起動します。Group数へ切り上げ変換します。

## 67. よくある失敗：境界Checkなし

最後のGroupの余剰ThreadがBufferやTextureの範囲外へAccessします。SizeをConstant Bufferで渡してGuardします。

## 68. よくある失敗：同一ResourceをSRV/UAVへBind

入力と出力を同一Subresourceへ重ねる設計を避け、Ping-Pongまたは別領域を使います。Debug Layerで実際のBindingを確認します。

## 69. よくある失敗：Counter未Reset

Append Counterが前Frame値から増え続け、Capacityを超えます。Pass開始時のInitial Countを明示します。

## 70. よくある失敗：毎Frame同期Readback

GPU結果を直後にMapしてCPUが待つとCompute化の利益を失います。GPU内で後続Drawへ渡すか、遅延Readbackへ変更します。

## 71. よくある失敗：BarrierでGroup間同期

`GroupMemoryBarrierWithGroupSync`が同期するのは同一Groupだけです。全Groupの中間結果が必要ならDispatchを分けます。

## 72. Resource寿命

一時UAV/SRVを毎Frame作成せず、SizeやFormatをKeyに再利用します。Resize時だけ再生成するResourceと固定Capacity Resourceを分けます。

## 73. Compute Passの設計情報

```cpp
struct ComputePass
{
    ID3D11ComputeShader* shader;
    std::vector<ID3D11ShaderResourceView*> inputs;
    std::vector<ID3D11UnorderedAccessView*> outputs;
    UINT groupX;
    UINT groupY;
    UINT groupZ;
};
```

実装ではLifetime、Ownership、Slot、Counter Reset、Hazard解除も含む明確なPass APIを設計します。

## 74. 実装順序

1. 一つのStructured Bufferを固定値で更新する。
2. SRV入力とUAV出力を分離する。
3. 端数要素と範囲Guardを試す。
4. Texture Filterを作る。
5. groupsharedとBarrierを学ぶ。
6. Append CounterとCopyStructureCountを使う。
7. GPU CullingとIndirect Drawへ接続する。
8. PIXで性能とHazardを検証する。

## 75. Unit Test可能な部分

Group数の切り上げ、Buffer Capacity、Argument Layout、CPU/HLSL構造体Size、Culling式等はCPU側Testへ分離できます。

## 76. GPU Test

小さな既知入力をDispatchし、Staging BufferへReadbackして期待値と比較します。0件、1件、Group境界、境界+1、最大Capacityを試します。

## 77. Particle Test項目

- `deltaTime = 0`で状態不変。
- GravityだけでVelocityが期待値になる。
- 63、64、65要素で境界外書込みがない。
- Ping-Pongの入力と出力が正しく交換される。
- Capacity到達時にAppendを安全に制御する。

## 78. Debug Checklist

- [ ] `numthreads`とDispatch Group数を区別している。
- [ ] 全Buffer/Texture Accessに範囲保証がある。
- [ ] CPU/HLSLのStrideとLayoutが一致している。
- [ ] SRVとUAVの同時Bindingがない。
- [ ] 後続Pass前に不要なViewをUnbindしている。
- [ ] Append/Counterの初期値が意図どおりである。
- [ ] BarrierへGroup内全Threadが到達する。
- [ ] Readbackが毎Frameの同期点になっていない。
- [ ] Debug LayerにWarningがない。
- [ ] GPU時間を計測している。

## 79. 理解確認問題

1. `numthreads(8,8,1)`で1920x1080を処理するDispatch寸法を求めてください。
2. `SV_GroupThreadID`と`SV_DispatchThreadID`の違いを説明してください。
3. 同じTextureをSRVとUAVへ同時Bindできない理由を説明してください。
4. Group間Reductionを一つのBarrierだけで完了できない理由を説明してください。
5. Append BufferのCounterを間接描画へ渡す流れを説明してください。
6. 即時ReadbackがPerformanceを落とす理由を説明してください。

## 80. 章末要点

- Compute ShaderはDataとThreadの対応を自分で設計するGPU並列処理Stageです。
- UAVはShaderからBufferやTextureを書き換えるViewです。
- Dispatch引数はThread数ではなくGroup数です。
- 範囲Guard、Race、Atomic、BarrierのScopeを正しく扱います。
- SRV/UAV Hazardを避け、Pass後にBindingを整理します。
- Append、Counter、Indirect Drawで結果をGPU内に保てます。
- 最適なGroup寸法やGPU化の価値は必ずProfileで判断します。

## 81. 公式資料

- [Compute Shader Overview](https://learn.microsoft.com/en-us/windows/win32/direct3d11/direct3d-11-advanced-stages-compute-shader)
- [ID3D11DeviceContext::Dispatch](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11devicecontext-dispatch)
- [ID3D11Device::CreateUnorderedAccessView](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11device-createunorderedaccessview)
- [ID3D11DeviceContext::CSSetUnorderedAccessViews](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11devicecontext-cssetunorderedaccessviews)
- [D3D11_BUFFER_UAV_FLAG](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/ne-d3d11-d3d11_buffer_uav_flag)
- [ID3D11DeviceContext::CopyStructureCount](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11devicecontext-copystructurecount)
- [numthreads](https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/sm5-attributes-numthreads)
- [SV_DispatchThreadID](https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/sv-dispatchthreadid)
- [GroupMemoryBarrierWithGroupSync](https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/groupmemorybarrierwithgroupsync)

次章では、Immediate ContextとDeferred Context、Command List、Thread Safety、Job Systemとの境界を扱います。
