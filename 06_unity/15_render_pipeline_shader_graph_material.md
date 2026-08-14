# Render Pipeline・URP/HDRP・Shader Graph・Material

> 対象: Unity 6、URP 17を中心に扱う。Render Pipeline PackageとShader GraphはUnity Versionに対応する版を使用すること。

## 1. Renderingの全体像

```text
Scene Objects
├─ Mesh / Skinned Mesh
├─ Material
├─ Light / Reflection Probe
└─ Camera
      ↓ Culling
Visible Renderers
      ↓ Sorting / Batching
Render Pipeline
├─ Shadow Pass
├─ Depth / GBuffer / Forward Pass
├─ Opaque
├─ Sky
├─ Transparent
├─ Post Processing
└─ UI
      ↓
Render Target / Screen
```

Shaderは単独で画面を作るのではなく、Pipelineが「いつ・どのpassで・どのbufferへ」実行するかを決めます。

## 2. 三つのRender Pipeline

### Built-in Render Pipeline

Unity従来のPipeline。既存Assetとの互換性は広い一方、SRP向け機能や今後の方針を公式資料で確認します。

### Universal Render Pipeline

MobileからPC/Consoleまで幅広くscaleさせるSRP。2D Renderer、Forward/Forward+、Renderer Feature、Render Graph、統合Post Processing等を持ちます。高速アクションを広いhardwareへ展開する場合の有力候補です。

### High Definition Render Pipeline

高性能PC/Console向け。高度なLighting、Volumetric、Water、Ray/Path Tracing等を持つ一方、対応platformとcostが大きくなります。

Pipelineは単なるQuality設定ではなく、Shader、Material、Post Processing、Camera、custom passの基盤です。Project途中の変更は大規模移行になります。

## 3. Pipelineを選ぶ基準

- Target platform。
- 目標FPSと解像度。
- Character/Enemy表示数。
- Lighting/VFX要件。
- Mobile/XR/Web対応。
- 使用AssetのShader対応。
- TeamのShader技術。
- custom render pass要件。
- Memory・build size。

「最も高機能だからHDRP」ではなく、必要な画作りを性能予算内で満たすPipelineを選びます。

## 4. Render Pipeline Asset

SRPではRender Pipeline AssetをGraphics SettingsやQuality levelへ割り当てます。割当が無ければBuilt-inが使用されます。

```text
Graphics Settings
└─ Default Render Pipeline Asset

Quality Settings
├─ Low    → Low URP Asset
├─ Medium → Medium URP Asset
└─ High   → High URP Asset
```

Quality別AssetでShadow距離、追加Light、Render Scale、MSAA等を変えられます。Quality切替時にPipeline Asset自体が変わるため、runtime featureとshader variantをtestします。

## 5. URP AssetとRenderer Data

概念的な分担:

- URP Asset: Pipeline全体のquality/feature設定。
- Renderer Data: Cameraが使うRendererとRenderer Feature。
- Camera Additional Data: Renderer選択、Post Processing、Camera Stack等。

一つのURP Assetに複数Rendererを持ち、Gameplay Camera、2D、特殊Cameraで選べます。ただしRendererごとにfeature、buffer、shader variant、性能が変わるため管理表を作ります。

## 6. URP 17とRender Graph

Unity 6のURP 17では新規ProjectでRender Graphが既定有効と公式Upgrade Guideに記載されています。Compatibility Modeで旧式APIを使える場合もありますが、長期的なcustom passはRender Graph対応を検討します。

Render Graphの目的:

- PassとResource依存を宣言。
- 未使用Passのculling。
- temporary resource lifetimeの管理。
- memory aliasing等の最適化。

「順番にCommandBufferへ命令を書く」だけでなく、Passが読む/書くTextureを明示する考え方が重要です。

## 7. ForwardとDeferred

### Forward

Object shading時にLightを評価します。透明、MSAA、Mobile等で扱いやすい一方、多数Lightでcostが増えます。

### Forward+

screen/tile等でLight候補を管理し、多数Lightへのscaleを改善します。hardware要件とURP feature対応を確認します。

### Deferred

まずMaterial情報をG-bufferへ書き、Lighting passで評価します。多数Lightに強い場合がありますが、G-buffer bandwidth、MSAA、透明、Material表現にtrade-offがあります。

Pipeline/Rendererごとの対応表を利用版で確認し、GPU captureで測ります。

## 8. 描画の主要段階

```text
CPU
├─ Culling
├─ Renderer収集
├─ Sort
├─ Batch作成
└─ Draw command提出

GPU
├─ Vertex processing
├─ Primitive assembly / clipping
├─ Rasterization
├─ Fragment processing
├─ Depth/Stencil test
├─ Blending
└─ Render Target write
```

CPU bottleneckとGPU bottleneckは対策が異なります。Draw Call削減が常にGPU fragment costを解決するわけではありません。

## 9. MaterialとShader

- Shader: GPU program、pass、property定義、render state。
- Material: Shaderを選び、property値・texture・keywordを保持するAsset。
- Renderer: MeshとMaterialを使ってdraw対象になる。

```csharp
Renderer renderer = GetComponent<Renderer>();

// sharedMaterial: Asset参照。変更すると同じMaterialを使う他Rendererへ波及し得る。
Material shared = renderer.sharedMaterial;

// material: instanceを生成する可能性がある。無意識に呼ぶとMaterial増殖。
Material instance = renderer.material;
```

runtimeで色を変えるために毎回`renderer.material`へアクセスすると、Material instanceとbatch分断が増えます。

## 10. MaterialPropertyBlock

Material Assetを複製せず、Renderer単位のpropertyを上書きできます。

```csharp
using UnityEngine;

public sealed class HitFlashRenderer : MonoBehaviour
{
    private static readonly int HitFlashId =
        Shader.PropertyToID("_HitFlash");

    private readonly MaterialPropertyBlock block = new();
    private Renderer cachedRenderer;

    private void Awake()
    {
        cachedRenderer = GetComponent<Renderer>();
    }

    public void SetHitFlash(float value)
    {
        // 既存Block値を保持する必要がある場合は取得してから変更する。
        cachedRenderer.GetPropertyBlock(block);
        block.SetFloat(HitFlashId, value);
        cachedRenderer.SetPropertyBlock(block);
    }
}
```

SRP BatcherとMaterialPropertyBlockのbatching関係はPipeline/Unity Versionで確認します。「instanceを作らないから必ず最速」と決めつけずProfiler/Frame Debuggerで確認します。

## 11. PBRの基本

物理ベースRenderingでは主に:

- Base Color/Albedo。
- Metallic。
- Smoothness/Roughness。
- Normal。
- Ambient Occlusion。
- Emission。

Metallic workflowでは金属はMetallic≈1、非金属は≈0を基本にし、中間値は混合surface等の意図で使います。Smoothnessは反射の鋭さへ影響します。

PBR MaterialはLighting環境が無ければ正しく評価できません。Reflection Probe、Sky、Lightmap、Exposureも同時に整えます。

## 12. Color Space

### Gamma

値を非線形なdisplay空間に近い形で扱う箇所があります。

### Linear

Lighting計算をlinear空間で行い、最後にdisplay変換します。多くの現代的PBRではLinearを基本に検討します。Platform対応を確認します。

Texture Import:

- Base Color: sRGB有効。
- Normal Map: Texture TypeをNormal Map。
- Mask/Metallic/Roughness/AO: data textureなのでsRGBを無効にすることが多い。
- HDR texture: formatとrangeを確認。

色textureと数値data textureを混同するとMaterialが正しく見えません。

## 13. Vertex ShaderとFragment Shader

### Vertex処理

Object spaceのvertexをWorld/View/Clip spaceへ変換し、UV、normal、tangent等をfragment側へ渡します。

```text
Object Position
 → Model Matrix
 → World Position
 → View Matrix
 → View Position
 → Projection Matrix
 → Clip Position
```

### Fragment処理

Rasterized pixel候補ごとにTexture sample、Lighting、color等を計算します。Overdrawが多いと同じscreen pixelへ何度もfragment処理します。

## 14. Coordinate Space

- Object/Local Space。
- World Space。
- View Space。
- Tangent Space。
- Clip Space。
- Normalized Device Coordinates。
- Screen/UV Space。

Shader Graph nodeのSpace設定を必ず確認します。World normalとTangent normalを直接加算しないで、適切にTransformします。

## 15. NormalとTangent

Normal Mapは通常Tangent Spaceの微細法線です。Meshのvertex normal、tangent、bitangentからTBN basisを作りWorld等へ変換します。

注意:

- Model Importのnormal/tangent計算。
- mirrored UV。
- negative scale。
- Normal texture Import Type。
- compression。
- Two-sided materialの裏面normal。

## 16. Shader Graphの構造

```text
Blackboard Properties
  ↓
Nodes / Sub Graphs
  ↓
Vertex Context
Fragment Context
  ↓
Master Stack / Target
  ↓
Generated Shader / Variants
```

Graphは「codeを書かないShader」ではなく、Shader codeを生成するvisual programmingです。Texture sample数、branch、space変換、variantは仍然存在します。

## 17. Blackboard Property

- Display Name: Inspector表示。
- Reference Name: code/Materialから使うShader property名。
- Exposed。
- Default value。
- HLSL declaration/override等、版による設定。

Reference Nameを変更すると既存Materialに保存された値やscriptの`PropertyToID`が切れる可能性があります。命名を早期に固定します。

```csharp
private static readonly int DissolveAmountId =
    Shader.PropertyToID("_DissolveAmount");
```

## 18. Sub Graph

再利用するnode処理をSub Graphへ分けます。

例:

- Triplanar mapping。
- Fresnel rim。
- Hit flash。
- Dissolve mask。
- Character shadow tint。
- Packed mask decode。

Sub Graphが巨大化して隠れcostにならないよう、input/outputのspace、range、texture sample数をdocumentします。

## 19. KeywordとShader Variant

Keywordでcompile-time分岐するとfeature組合せごとにShader Variantが増えます。

```text
3個のboolean keyword → 最大2³ = 8 combinations
10個               → 最大2¹⁰ = 1024 combinations
```

Pipeline、Lighting、Fog、Shadow、Material featureも組合せへ加わります。Variant explosionは:

- Import/Build時間増加。
- Build size増加。
- runtime warm-up/stutter。
- memory増加。

Local/Global keyword、Enum keyword、dynamic branchのtrade-offを考え、不要variantをstripします。

## 20. Branch

Shader GraphのBranch nodeが必ず「片側だけ無料」になるわけではありません。GPU、条件のuniformity、compiler最適化により両側計算やdivergenceが起きます。

- Materialごとに固定: keyword/variant候補。
- pixelごとに変わる: dynamic branchまたはmath。
- 小さい処理: lerpの方が単純な場合。

最終generated codeとGPU captureで確認します。

## 21. Surface Type

### Opaque

Depthへ書き、通常はfront-to-back等で描画しやすい。早期depth rejectが効きやすい。

### Transparent

通常はdepth testしつつdepth writeせず、back-to-front sortとblendingを行います。三角形単位の完全sortingではないため、透明object同士や自己交差で破綻します。

透明を多用すると:

- Overdraw。
- sorting artifact。
- shadow/deferred制約。
- Post Processing順序問題。

髪、VFX、DissolveでOpaque/Alpha Clipを使えるか検討します。

## 22. Alpha Clipping

Alphaがthreshold未満のfragmentを破棄します。草、網、髪card等で透明sortingを避けられます。

trade-off:

- jagged edge/aliasing。
- MSAA/Alpha-to-Coverage。
- Shadow passでも同じclipが必要。
- threshold animationでpixel popping。
- early-zへの影響。

## 23. Blending

代表:

- Alpha blend: `SrcAlpha, OneMinusSrcAlpha`。
- Premultiplied alpha。
- Additive: 発光VFX。
- Multiply: darkening。

色がpremultipliedかstraightかをTexture制作とShaderで揃えます。間違えるとedge haloが出ます。

## 24. Depth

Depth BufferはCameraから見た奥行きを保存し、近いsurfaceを残します。

重要:

- Near/Far planeでprecisionが変わる。
- Transparentは通常Depth Writeしない。
- Depth TextureはScreen Space effect、soft particle、outline等で使う。
- Depth Priming/Prepassはcostとoverdrawのtrade-off。
- reversed-Z等はGraphics API/Pipelineが抽象化するがcustom shaderで前提を確認。

## 25. Stencil

Stencil Bufferはpixel単位の小さな整数値でmask処理に使えます。

- Character outline。
- portal。
- UI mask。
- selective effect。

複数featureが同じbitを奪い合わないようStencil bit allocation表を作ります。

## 26. Render QueueとSorting

概念的順序:

```text
Background
Geometry / Opaque
Alpha Test
Transparent
Overlay
```

Render Queueを無理にずらして問題を隠すと、shadow、depth、他effectとの順序が壊れます。Queue、ZTest、ZWrite、Blendの意味を組で確認します。

## 27. Draw Call・SetPass・Batch

- Draw Call: GPUへgeometry drawを指示。
- SetPass/State change: Shader/Material/render stateの切替。
- Batch: 複数objectを効率よくまとめる。

同じMaterialを共有しても、Light、Shadow、pass、keyword、property、Mesh等でbatchが分かれます。StatsだけでなくFrame Debuggerで「なぜ分かれたか」を確認します。

## 28. SRP Batcher

SRP BatcherはcompatibleなShaderのMaterial property layoutを整理し、Shader variant間のCPU state setupを効率化します。

Shader側のcompatible要件があります。Shader Graph生成ShaderはPipeline対応状況を確認します。

```text
SRP Batcher compatible
├─ UnityPerDraw data
├─ UnityPerMaterial data
└─ stable constant buffer layout
```

GPU Instancingとは目的・仕組みが異なります。両者のどちらが有効かをFrame Debugger/Profilerで確認します。

## 29. GPU Instancing

同じMesh/Materialを多数描く際、instanceごとのTransform/propertyをまとめてdrawできます。

向く例:

- 草。
- 同じprop。
- projectile。
- debris。

向かない/制約:

- Skinned Meshの一般的な描画。
- Material/keywordが異なる。
- instance数が少ない。
- per-instance dataが多い。

MaterialPropertyBlockでinstanced propertyを渡すにはShader側宣言も必要です。

## 30. Static/Dynamic Batching

Pipelineとplatformで対応・推奨が変わります。

- Static batching: 動かないgeometryをまとめる。memory増加に注意。
- Dynamic batching: 小Mesh等をCPUでまとめる。現代hardwareではCPU costとのtrade-off。
- SRP Batcher。
- GPU Instancing。
- GPU Resident Drawer。

「Batchingを全部ON」ではなく、ProjectのbottleneckとUnity 6のPipeline機能を測ります。

## 31. Skinned Mesh

Characterはbone matrixでvertexを変形します。

cost:

- vertex数。
- bone influence数。
- bone数。
- update when offscreen。
- BlendShape。
- shadow caster。
- material/submesh数。

Character一体のMaterial slotが多いとdraw passが増えます。顔、髪、体、武器の品質要件とdraw budgetを決めます。

## 32. Shader Pass

Lit Shaderは画面色用passだけではありません。

- Forward/Deferred pass。
- ShadowCaster。
- DepthOnly。
- DepthNormals。
- Meta（baking）。
- MotionVectors。
- SceneSelection等Editor用。

Custom ShaderでShadowだけ消える、SSAOへnormalが出ない、Motion Vectorが無い等は必要pass不足が原因になり得ます。

## 33. Custom Function

Shader GraphからHLSL Custom Functionを呼べます。

用途:

- 複雑math。
- 既存HLSL library再利用。
- Graphで表しにくいloop/構造。

```hlsl
void Remap01_float(
    float Value,
    float MinValue,
    float MaxValue,
    out float Out)
{
    float range = max(MaxValue - MinValue, 1e-5);
    Out = saturate((Value - MinValue) / range);
}
```

float/half suffix、include path、precision、SRP関数差、platform compilerを確認します。

## 34. Precision

- Float: 精度高、cost/帯域が増える場合。
- Half: Mobile等で有利な場合、range/precision制約。

World positionや大きい時間値をhalfで扱うと破綻します。Colorやnormalの一部はhalf候補です。Target GPUでcompile結果と見た目を確認します。

## 35. Time node

ShaderのTimeを使うとUV scroll、dissolve、pulseを作れますが:

- Pause/Slow Motionと一致するか。
- 長時間でfloat精度が落ちる。
- network/replayで同期しない。
- Materialごとの開始時刻を持てない。

Gameplay同期が必要ならscriptからnormalized progressをpropertyで渡します。

## 36. Hit Flash

```text
Base Lit Color
  ↓
HitFlash amount
  ↓ lerp/add
Flash Color
  ↓
Emission/Base Colorへ出力
```

Damage SystemがMaterialを直接探さず:

```text
Hit Event
 → Character Presentation
 → HitFlash Controller
 → MaterialPropertyBlock
```

同じRendererにDissolveやOutlineもPropertyBlockを書く場合、一つのPresentation Material ControllerがBlockを集約します。各Componentが`SetPropertyBlock`で互いの値を消さないようにします。

## 37. Dissolve

基本:

```text
Noise/Object Position
 → Mask
 → threshold比較
 → Alpha Clip
 → edge範囲
 → Emission
```

注意:

- Object/World/UV spaceの選択。
- Characterが動いてもpatternを固定するか。
- ShadowCasterでもclip。
- Depth passでもclip。
- edge幅のsmoothstep。
- Property値のrange。
- transparentでなくOpaque+Alpha Clipを使えるか。

## 38. Outline

方式:

- Inverted hull: Meshをnormal方向へ膨らませ裏面描画。
- Screen Space: Depth/Normal/Color差からedge検出。
- Stencil + Fullscreen。
- Post Processing。

Inverted hull:

- draw増加。
- hard edge/normal問題。
- scaleと幅。
- 穴や凹形状。

Screen Space:

- depth/normal textureが必要。
- 解像度依存。
- Object選択mask。
- temporal jitter。

Lock-on TargetだけOutlineする場合、Stencil/Rendering Layer等で対象をmaskします。

## 39. Rim Light

Fresnel的な`1 - dot(N, V)`で輪郭を強調できます。

```hlsl
float rim = pow(
    saturate(1.0 - dot(normalWS, viewDirectionWS)),
    rimPower);
```

常時強すぎるRimはLightingを平坦にします。Combat中、Lock-on、invincible等のpresentation stateで強度を制御します。

## 40. Screen Space Texture

Scene Color、Depth、Normal、Opaque Texture等をsampleするeffectがあります。

用途:

- distortion。
- refraction。
- soft particle。
- depth fade。
- outline。

Pipeline Asset/RendererでTexture生成を有効にすると追加copy/pass costが発生します。Shader一つのため全CameraでOpaque Textureを作る価値があるか測ります。

## 41. Renderer Feature

URP Renderer Feature/Passでcustom描画を挿入できます。

- selected object pass。
- fullscreen effect。
- custom depth/mask。
- outline。
- decal/特殊shadow。

Unity 6 URPではRender Graph対応とCompatibility Modeの違いを確認します。

設計:

- injection point。
- input texture宣言。
- output resource。
- Camera type filter。
- Scene/Game/Preview/Reflection Camera。
- XR。
- resource lifetime。
- pass culling。

## 42. Volume

URP/HDRPはVolumeでPost Processingやenvironment overrideを管理します。

- Global Volume。
- Local Volume + Collider。
- Priority。
- Blend Distance。
- Weight。
- Profile Asset。

runtimeでshared Profile Assetを書き換えると他Volume/Sceneへ影響する可能性があります。instance/profileの所有権を確認します。

## 43. 戦闘用Post Processing

- Bloom。
- Color Adjustments。
- Vignette。
- Chromatic Aberration。
- Motion Blur。
- Depth of Field。

Hit時にすべて最大化せず、視認性と酔いを優先します。複数effect要求はtoken/priorityで合成します。

```text
Base Environment Volume
+ Damage Volume weight
+ Finisher Volume weight
+ Pause Volume weight
= final stack
```

## 44. Shader Variant Warm-up

初めてShader Variantを使う瞬間にcompile/driver preparationでstutterする場合があります。

- Build時strip。
- Shader Variant Collection等、利用版の仕組み。
- Loading screenでwarm-up。
- 実機でcold/warm起動test。
- variant log。
- Graphics APIごとの差。

全variantを無制限に収集するとbuild/memoryが増えます。実際に使うfeature setを管理します。

## 45. Pipeline移行

Built-in→URP/HDRP:

1. branchとbackup。
2. Package/Unity Version固定。
3. Pipeline Asset作成・割当。
4. Material converter。
5. custom Shader書換え。
6. Lighting/Post Processing移行。
7. Camera/Renderer Feature。
8. VFX/Particle/UI。
9. Sceneごとのvisual regression。
10. 全Target platformでperformance測定。

Pink MaterialはShaderがPipeline非対応・compile失敗・missing等のsignalです。無理にStandardへ戻さず原因を確認します。

## 46. Debug Tool

- Frame Debugger: draw順、pass、batch理由。
- Rendering Debugger: Pipeline feature可視化。
- Render Graph Viewer。
- Profiler CPU/GPU。
- GPU Profiler/RenderDoc/Xcode等。
- Shader Graph Heatmap。
- Memory Profiler。
- Stats。

```text
症状: GPUが遅い
 → resolutionを下げて改善? fragment/bandwidth寄り
 → shadowを切って改善? shadow pass寄り
 → object数を減らして改善? draw/vertex寄り
 → overdraw表示
 → GPU captureでpass別時間
```

## 47. 性能予算

60 FPSならframe全体は約16.67 msですが、Renderingが全時間を使えるわけではありません。

例:

```text
CPU simulation
CPU rendering submission
GPU shadow
GPU opaque
GPU transparent/VFX
GPU post processing
UI
margin
```

resolution、platform、熱、battery、最悪sceneを含めて予算化します。平均だけでなくp95/p99 frame timeを見ます。

## 48. よくある不具合

- Pipelineを途中で気軽に切り替える。
- URP 16以前のcustom passをURP 17へ無確認で使用。
- `renderer.material`でMaterial instanceを大量生成。
- PropertyBlockを複数scriptが上書きし合う。
- sRGBのdata textureを使い数値が歪む。
- Tangent/World normalを直接混ぜる。
- TransparentでDepth/sorting問題を無視。
- Keywordを増やしvariantが爆発。
- Branch nodeなら片側costが消えると思う。
- Shader GraphならGPU costを考えなくてよいと思う。
- DissolveのShadow/Depth passが一致しない。
- Screen textureを一effectのため全Cameraで生成。
- Post ProcessingをCamera酔い設定なしで強制。
- Frame Debuggerを見ずDraw Call数だけで判断。
- Editor/Game Viewだけで性能判断。

## 49. Test Matrix

| 観点 | Test |
|---|---|
| Pipeline | URP Asset Low/Medium/High |
| API | DirectX 11/12、Vulkan、Metal等の対象 |
| Camera | Base、Overlay、Scene、Preview、Reflection |
| Material | Opaque、Clip、Transparent、Double-sided |
| Lighting | 0/多数Light、Shadow、Lightmap、Probe |
| Character | Hit Flash、Dissolve、Outline、off-screen |
| VFX | Overdraw少/多、低解像度、高解像度 |
| Time | Pause、Slow Motion、長時間稼働 |
| Build | cold start、variant warm-up、strip後 |
| Device | 最低/推奨hardware、熱状態 |

## 50. 設計チェックリスト

- Target platformからPipelineを選んだか。
- Unity/URP/Shader Graph版を固定したか。
- Quality別Pipeline Assetをtestしたか。
- Material Assetとruntime propertyの所有権を分けたか。
- Color textureとdata textureのsRGBを区別したか。
- Coordinate Spaceを各nodeで確認したか。
- Transparent/Alpha Clipを意図で選んだか。
- Shader Variant数を計測・stripしたか。
- SRP Batcher/Instancingの実結果を確認したか。
- Character effectがShadow/Depth/Motion Vectorでも正しいか。
- Renderer FeatureがRender Graph対応か。
- Post Processingを無効化・軽減できるか。
- Frame DebuggerとGPU captureで測ったか。
- 最低hardwareの最悪戦闘sceneで予算内か。

## 公式資料

- [Unity Manual: Choose a render pipeline](https://docs.unity3d.com/6000.0/Documentation/Manual/choose-a-render-pipeline.html)
- [Unity Manual: Render pipeline feature comparison](https://docs.unity3d.com/6000.0/Documentation/Manual/render-pipelines-feature-comparison.html)
- [Unity Manual: Set a render pipeline](https://docs.unity3d.com/6000.0/Documentation/Manual/srp-setting-render-pipeline-asset.html)
- [Unity Manual: URP](https://docs.unity3d.com/6000.0/Documentation/Manual/urp/urp-introduction.html)
- [Unity Manual: Upgrade to URP 17](https://docs.unity3d.com/6000.0/Documentation/Manual/urp/upgrade-guide-unity-6.html)
- [Unity Manual: Configure URP performance](https://docs.unity3d.com/6000.0/Documentation/Manual/urp/configure-for-better-performance.html)
- [Shader Graph 17 Manual](https://docs.unity3d.com/Packages/com.unity.shadergraph@17.0/manual/index.html)
- [Unity Manual: SRP Batcher](https://docs.unity3d.com/6000.0/Documentation/Manual/SRPBatcher.html)
- [Unity Manual: GPU instancing](https://docs.unity3d.com/6000.0/Documentation/Manual/GPUInstancing.html)
- [Unity Manual: Frame Debugger](https://docs.unity3d.com/6000.0/Documentation/Manual/FrameDebugger.html)

