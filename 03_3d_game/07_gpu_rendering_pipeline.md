# GPUレンダリングパイプライン

CPUはSceneから描画Commandを作り、GPUは大量のVertex・Pixelを並列処理します。GPU API呼出は即時完了ではなくCommand Queueへ積まれ、CPUとGPUが非同期に進みます。

## Frameの概略

```text
CPU: Visibility → Sort → Build Commands → Submit
GPU: Vertex → Primitive → Raster → Fragment/Pixel → Output Merge
```

GPUが前Frameを処理中にCPUが次Frameを準備できます。同期点を増やすと並列性を失います。

## Vertex Shader

各Vertexを処理しClip Positionと補間属性を出力します。

```hlsl
VSOutput MainVS(VSInput input)
{
    VSOutput output;
    float4 worldPosition = mul(Model, float4(input.position, 1.0));
    output.position = mul(ViewProjection, worldPosition);
    output.uv = input.uv;
    return output;
}
```

`mul`順、Matrix layoutはShader Language設定へ依存します。

## Primitive AssemblyとClipping

IndexからTriangleを作り、Clip Volume外を切ります。Geometry Shader等のOptional Stageもありますが、Architectureによって高コストになり得ます。

## Rasterization

Triangleが覆うPixel Sampleを生成し、Vertex属性を補間します。Perspective-correct補間、Front Face、Cull、Viewport、Scissorを適用します。

## Fragment/Pixel Shader

各候補Sampleの色や補助出力を計算します。Texture Sample、Lighting、Material評価を行います。`discard`はEarly-ZやGPU実行効率へ影響する可能性があります。

## Output Merger

Depth/Stencil Test、Blendを行いRender Targetへ書きます。順序とStateが重要です。

## Resource

- Buffer：Vertex、Index、Constant、Structured、Storage。
- Texture：2D、Cube、Array、Depth。
- Sampler：Filter、Address。
- Render Target：描画出力。
- Pipeline State：Shader、Blend、Raster、Depth等。

APIによってDescriptor/Binding Modelが異なります。

## Constant Data

Frame、View、Material、Objectごとの更新頻度で分けます。

```text
PerFrame ：time, light
PerView  ：viewProjection, camera
PerMaterial：baseColor, roughness
PerObject：model, objectId
```

小さな更新ごとにGPU Allocationせず、Upload Ring BufferとAlignmentを管理します。

## Draw Call

一回のDrawはPipeline State、Resource Binding、Vertex/Index範囲を使って実行します。CPU overheadがあるため、Instancing、Batching、Indirect Drawを使います。ただしVisibilityとMaterial順序を壊さないようにします。

## Pipeline State変更

Shader、Blend、Depth等の変更は高コストになり得ます。OpaqueをPipeline/MaterialでSortし、TransparentはDepth順を優先します。

## Synchronization

GPU使用中ResourceをCPUが破棄・上書きしてはいけません。

- Fenceで完了を追跡。
- Frames in FlightごとにBufferを分ける。
- Deferred Destruction Queue。
- Resource State/Barrierを正しく遷移。

毎Frame GPU完了を待つとPipelineが直列化します。

## Render Pass / Render Graph

Shadow、Depth Prepass、GBuffer、Lighting、Transparent、Post Process、UI等のPassがあります。Render GraphはResourceのRead/Write依存から順序、Barrier、一時Resource再利用を管理します。

## CPU-GPU Profiler

CPU MarkerとGPU Timestampを入れます。CPU Submitが速くてもGPUが遅い、またはPresent待ちでCPUが止まる場合があります。Frame CaptureでDraw、State、Texture、Shaderを検査します。

## Shader Compilation

実行中の初回Pipeline生成でStutterが起きます。VariantをBuild時に列挙・Compileし、Pipeline CacheをPrewarmします。無制限なFeature Macro組合せはVariant Explosionを起こします。
