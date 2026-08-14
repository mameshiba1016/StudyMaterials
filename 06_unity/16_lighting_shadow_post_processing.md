# Lighting・Shadow・Probe・Post Processing

> 対象: Unity 6 + URP 17を中心にする。HDRPやBuilt-inでは設定名・対応機能・単位が異なるため、使用Pipelineの公式資料を確認すること。

## 1. 光は「明るさ」だけではない

Lightingは物体の形、材質、空間、時間、攻撃の危険性をPlayerへ伝えます。

```text
Light Sources
├─ Direct Lighting
├─ Shadow
└─ Indirect Lighting / GI
        ↓
Material BRDF
        ↓
HDR Scene Color
        ↓
Exposure / Tone Mapping
        ↓
Bloom / Color Grading / Other Effects
        ↓
Display Color
```

Light強度だけを上げても、Exposureが下げれば画面の明るさは同じに見える場合があります。LightingとPost Processingを一体で調整します。

## 2. Lightの種類

### Directional Light

無限遠から同じ方向へ届く光。太陽・月に使います。位置ではなく方向が重要で、広いWorldへ影響します。

### Point Light

一点から全方向へ広がります。電球、爆発等。Shadow mapは複数方向分が必要なため高costになりやすいです。

### Spot Light

円錐範囲へ照射します。照明器具、攻撃予兆、懐中電灯等。方向、角度、rangeを持ちます。

### Area Light

面からの光を表しますが、Realtime/Baked対応や形状はPipelineで異なります。HDRPはより豊富です。

## 3. 光の単位

Pipeline/Light TypeによりLux、Lumen、Candela等の物理単位を選べる場合があります。

- Lumen: 光源から出る光束。
- Candela: 特定方向の光度。
- Lux: 面へ届く照度、1 lm/m²。
- Nits/cd/m²: 表面・displayの輝度。

物理単位を使ってもExposure、Material、Tone Mappingが不適切なら正しく見えません。Projectの基準値とExposure方針を決めます。

## 4. DirectとIndirect

### Direct Lighting

Lightからsurfaceへ直接届く光。Shadowで遮られます。

### Indirect Lighting

surfaceで反射して別surfaceへ届く光。Global Illuminationで近似・計算します。

```text
Sun → red wall → Character
         └─ 赤い間接光がCharacterへ回り込む
```

Indirectが無いとShadow側が真黒になりやすく、形状が読めません。Sky ambient、Lightmap、Light Probe、APV等を使います。

## 5. Realtime、Baked、Mixed

### Realtime

毎frame Lighting/Shadowを計算します。動くLightやCharacterに対応しますがGPU/CPU costがあります。

### Baked

静的geometryへの光を事前計算しLightmap等へ保存します。runtime costを抑え高品質な間接光を得られますが、Light/geometryの動的変更に制約があります。

### Mixed

RealtimeとBakedを組み合わせます。Baked Indirect、Shadowmask等のmodeはPipeline/Lighting Settingsで確認します。

一つのSceneで「どのobjectが動くか」「どのLightが動くか」を分類してmodeを選びます。

## 6. Contribute Global Illumination

Static geometryをbakeへ含める設定です。ただStatic flagは一種類ではなく、Batching、Occlusion、Navigation等の意味を分けて確認します。

Bakeへ含めるobject:

- Wall、Floor、建物。
- 大きく動かないprop。

含めない/注意:

- Character。
- 動くdoor/platform。
- 破壊object。
- runtime生成object。

動くobjectはLight Probe/APV等から間接光を受けます。

## 7. Lightmap

Lightmapはsurfaceのbaked lightingをTextureへ保存します。MeshにはLightmap用UVが必要です。

```text
Mesh UV0 : Base Color等
Mesh UV1/UV2相当 : Lightmap chart
  ↓ packing
Lightmap Atlas
  ↓ Renderer scale/offset
Surface sample
```

要件:

- chartが重ならない。
- padding。
- 適切なtexel density。
- distortionを抑える。
- seamを目立たせない。

Model ImportのGenerate Lightmap UVsは便利ですが、重要AssetはDCC toolで制御したUVも検討します。

## 8. Lightmap Texel Density

Lightmap Resolutionを全体へ上げるとmemory、bake time、load sizeが増えます。

優先:

- Playerが近づく壁・床。
- 大きなshadow detailが必要。
- Gameplay上重要なarena。

低優先:

- 遠景。
- 小さいprop。
- emission中心。
- 常に暗い/見えない面。

Scene ViewのLightmap Density表示で不均一を確認します。

## 9. Lightmap artifact

- UV overlap。
- padding不足によるbleeding。
- texel density不足。
- normal/smoothing問題。
- backfaceからのlight leak。
- 薄いwall。
- geometry intersection。
- denoiser artifact。
- compression。

解像度を上げる前に原因を切り分けます。薄い壁の裏から光が漏れる場合、厚み、probe bias、backface、chart等を見直します。

## 10. Light Probe

Light Probeは空間中のbaked indirect lightingをsampleし、動的Rendererへ補間して与えます。

```text
Probe tetrahedron
├─ Probe A SH coefficients
├─ Probe B
├─ Probe C
└─ Probe D
      ↓ object位置で補間
Dynamic Character Lighting
```

Spherical Harmonicsで低周波な方向性Lightingを表します。鋭い鏡面反射はReflection Probeの役割です。

## 11. Light Probe配置

- Characterが移動する高さ。
- 明暗境界。
- door/窓。
- 階段・上下階。
- 大きな色変化。
- 動くplatform経路。

一直線や一平面だけでなく3D volumeを作ります。Probeが壁の反対側の明るさを補間してlight leakしないよう、壁両側と境界へ配置します。

## 12. Light Probe Proxy Volume

大きいDynamic Rendererが一つのprobe sampleだけを使うと、object全体が均一に照らされます。LPPV等、Renderer volume内で複数sampleする仕組みを検討します。Pipeline対応を確認します。

## 13. Adaptive Probe Volumes

Unity 6 URPのAPVはprobe配置を自動化し、per-pixelで高品質なbaked lightingを提供します。

概念:

- Probe Volume。
- Baking Set。
- Brick/Cell密度。
- Probe validity。
- Dilation。
- Lighting Scenario。
- Streaming。

従来Light Probe Groupとの対応・制約を確認し、同じSceneで無秩序に混在させません。

## 14. APVの密度

geometryが細かい場所、明暗境界へ高密度brickが必要です。World全体を最高密度にするとbake dataとmemoryが増えます。

```text
Open Field : low density
Doorway    : high density
Indoor wall corners : high density
Far background : low density
```

Probe Adjustment Volume等で局所補正し、Rendering Debuggerでcell/validityを可視化します。

## 15. APV light leak

Probe sample rayが壁裏やgeometry内部を拾い、bright leak/black spotが出ます。

対策候補:

- geometry厚み。
- probe density。
- validity threshold。
- dilation。
- adjustment volume。
- bias/normal bias。
- bake set境界。

公式Troubleshootingを使い、見た目だけのrandom light追加で隠しません。

## 16. Reflection Probe

Reflection Probeは周囲をCubemapへcaptureし、PBRのspecular reflectionへ使います。

- Baked。
- Realtime。
- Custom Cubemap。
- Box Projection。
- Influence/Blend。
- Importance。

Metallic MaterialはReflection環境が無いと黒く見えやすいです。

## 17. Reflection Probe配置

- 部屋ごと。
- 屋内/屋外境界。
- 色・明るさが大きく変わる場所。
- glossy floor/metalが重要なarena。

Probe centerのCubemapがvolume全体で使われるため、大部屋一つだけではparallaxが不自然です。Box Projectionで局所補正できますが完全なplanar reflectionではありません。

## 18. Realtime Reflection Probe

Cubemapの6方向をrenderするため高costです。毎frame更新を避け:

- On Awake。
- Via Scripting。
- Time Slicing。
- 必要時のみ。
- resolution/culling maskを制限。

Reflection Probe Cameraに不要Character/VFX/UIが映らないようLayerを設定します。

## 19. Shadow Map

代表的なShadow Map:

```text
Light視点からDepthをrender
        ↓
Cameraから見たsurface pointをLight spaceへ変換
        ↓
Shadow Map depthと比較
        ↓
遮蔽判定
```

有限解像度なのでaliasing、acne、peter-panning等が起きます。

## 20. Shadow AcneとBias

同一surfaceのdepth比較誤差で縞が出るのがshadow acneです。Depth Bias/Normal Biasでsampleをずらします。

- Bias小さすぎ: acne。
- Bias大きすぎ: shadowがobjectから浮く。
- Normal Bias大きすぎ: 細いgeometryのshadow消失/形状変化。

Light角度、shadow resolution、mesh scale、normalでも変わります。Biasだけを極端にしません。

## 21. Shadow Cascade

Directional LightのCamera frustumを距離ごとに分割し、近距離へ高いtexel densityを割り当てます。

```text
Camera
├─ Cascade 0: near / high detail
├─ Cascade 1
├─ Cascade 2
└─ Cascade 3: far / low density
```

Cascade数を増やすと品質とcostが増え、split境界で変化が見える場合があります。Rendering Debuggerで色分け表示します。

## 22. Shadow Distance

Shadow Atlas解像度が同じなら、Shadow Distanceを広げるほど単位距離あたりtexel密度が下がります。

遠距離shadowが本当に必要か確認し:

- distanceを短くする。
- farはbaked shadow/ambientへ。
- Camera fogで遷移を隠す。
- Characterだけ近距離高品質。

## 23. Main/Additional Light Shadow

URPではMain Directional Shadow AtlasとAdditional Light用Atlas等を設定します。Point Lightは多方向shadowが必要で、Spotより高costです。

例:

```text
4 Spot Lights  → 4 shadow maps
1 Point Light  → 6 faces相当
合計10領域をAdditional Shadow Atlasへ配置
```

Atlas不足時、各Light resolutionが下がる可能性があります。Light数だけでなくmap face数を数えます。

## 24. Soft Shadow

PCF等で複数sampleしてedgeを柔らかくします。品質を上げるほどsample costが増えます。

Soft ShadowはLight sizeの物理表現にも関係しますが、Pipeline設定とLight設定の組合せを確認します。Mobileで全Light高品質soft shadowにしません。

## 25. Contact ShadowとScreen Space Shadow

近接する細部shadowやscreen spaceでの補助手法があります。画面外情報を持たず、Temporal/Depth artifactが出る場合があります。

用途:

- 足元接地。
- 小物接触。
- Character細部。

Baked/Shadow Map/AOを置き換える万能手段ではありません。

## 26. Shadow Casterの最適化

- 不要RendererのCast ShadowsをOff。
- 遠景LODではshadow casterを簡略化。
- 小さいVFX/transparentはshadow不要か確認。
- Character髪cardのshadow cost。
- Light culling mask/rendering layerで対象制限。
- Shadow Distance。
- Additional Light shadow数。

見えないCasterもLight frustum内ならshadow passへ描かれる可能性があります。

## 27. Lighting Layer / Rendering Layer

特定Lightを特定Rendererへだけ影響させる機能があります。Pipeline/Renderer modeによる対応を確認します。

用途:

- Character key light。
- VFX専用Light。
- EnvironmentとCharacterを分離。

多用するとLightingが物理的に不整合になり、Layer bitも有限です。Art Direction上の意図を記録します。

## 28. Character Lighting

Characterは背景へ埋もれず、浮きすぎない必要があります。

```text
Environment Direct/Indirect
+ Light Probe/APV
+ Reflection Probe
+ Character Key/Rim（必要時）
+ Material rim/hit flash
+ Post Processing
= final readability
```

Character専用Lightだけで解決すると、Sceneごとの色やshadowと不一致になります。まずProbe/Exposure/Materialを整えます。

## 29. 攻撃予兆とLight

危険攻撃をPoint Lightだけで示すと、Shadow/性能/色覚差に依存します。

- Animation silhouette。
- Ground decal。
- VFX shape。
- Emissive color。
- Audio。
- UI indicator。
- Camera framing。

Lightは補助channelとして使います。予兆の意味を色だけへ依存させません。

## 30. Emission

EmissionはMaterial自身が光って見える値です。Bloomと組み合わせて輝きますが、Realtime Lightのように周囲を自動で照らすとは限りません。

- Baked GIへ寄与する設定。
- HDR color強度。
- Bloom threshold。
- Exposure。
- Tone Mapping。

「Emission=白」だけではBloomしない場合があり、HDR rangeとPost Processing設定を確認します。

## 31. Camera HDR

HDR render targetは1を超える光強度を保持し、Bloom/Tone Mappingへ渡せます。LDRへclampされる前にPost Processingを行います。

cost:

- Render Target format/memory。
- bandwidth。
- platform対応。

Low QualityではHDRをOffにする選択肢もありますが、Material/VFXの見え方を別途調整します。

## 32. Volume System

```text
Global Volume
+ Local Volume A (weight by distance)
+ Local Volume B
+ Runtime Combat Volume
        ↓ priority / weight / override state
Final Volume Stack
```

- Is Global。
- Collider。
- Blend Distance。
- Weight。
- Priority。
- Volume Profile。
- Camera Volume Layer Mask。

CameraとVolume GameObjectのLayer設定が一致しないと適用されません。

## 33. Volume Profileの所有権

`sharedProfile`相当のAssetを書き換えるとProject Assetや他Volumeへ波及し得ます。runtime instanceを作るAPIやprofile propertyの意味を利用版で確認します。

```csharp
using UnityEngine;
using UnityEngine.Rendering;

public sealed class CombatVolumeController : MonoBehaviour
{
    [SerializeField] private Volume volume;

    public void SetWeight(float weight)
    {
        volume.weight = Mathf.Clamp01(weight);
    }
}
```

個々のeffect propertyを毎frame探さず、初期化時に`TryGet`してcacheします。

## 34. Bloom

明るいpixelを抽出し、blurして元画像へ合成します。

- Threshold。
- Intensity。
- Scatter。
- Clamp。
- Tint。
- Dirt Texture。

Bloomは「画面全体をぼかす」ものではありません。強すぎると攻撃effect、UI、Character輪郭が潰れます。

## 35. Tone Mapping

HDR scene colorをdisplay可能rangeへ変換します。

- None。
- Neutral。
- ACES系等、Pipelineによる選択。

Tone Mapperはcontrast、highlight roll-off、色へ影響します。LUT/Color Grading制作はTone Mapperを固定して行います。

## 36. Exposure

Sceneの明るさ基準です。URP/HDRPで対応するExposure modeが異なります。

高速アクションではAuto Exposureが暗所/爆発で急変すると敵を見失います。

- 固定Exposure。
- adaptation speed制限。
- arena単位profile。
- gameplay重要場面でlock。

暗さの演出と操作視認性を分けます。

## 37. Color AdjustmentsとColor Grading

- Post Exposure。
- Contrast。
- Color Filter。
- Hue/Saturation。
- LUT。
- Lift/Gamma/Gain等、Pipeline依存。

Damage時に画面全体を赤くすると敵攻撃色やUIが読めなくなる場合があります。Vignette、UI、Character effectと分担します。

## 38. White Balance

Temperature/TintでScene全体の色調を調整します。Light colorとWhite Balanceを同時に動かすと原因が追いにくいため:

1. Lightを物理/Art基準へ。
2. Exposure。
3. White Balance。
4. Color Grading。

の順でbaselineを整えます。

## 39. Ambient Occlusion

近接するsurfaceの隙間を暗くし形状を強調します。

- Baked AO。
- Material AO。
- SSAO。

重ねすぎると角が真黒になります。SSAOはscreen spaceなので画面外情報を持たず、noise/halo/Temporal artifactが出ます。

## 40. Motion Blur

速度を視覚的なblurへ変えます。高速Actionで速度感を出せますが:

- 敵のsilhouetteが消える。
- Camera rotationで全画面がblur。
- motion vector品質。
- UIへの影響。
- Player酔い。
- platform性能。

強度を設定可能、完全Off可能にします。Character/VFXだけの局所blurも検討します。

## 41. Depth of Field

焦点距離外をぼかします。

Gameplay中:

- Target切替で焦点が泳ぐ。
- 敵予兆がぼける。
- costが高い。

Cutscene/Photo Modeへ限定し、Combat中は弱める/Offにすることが多いです。

## 42. Vignette

画面周辺を暗く/着色します。Damage、low HP、Aim等に使えますが視野を狭めます。

複数SystemがIntensityを直接上書きせず:

```text
Base vignette
Damage request
Low HP request
Cutscene request
  ↓ max/add/priority rule
Final vignette
```

## 43. Chromatic Aberration・Film Grain

演出用途ですが、文字・輪郭・画面端の視認性を落とします。常時強く使わず、Player設定・品質設定・演出時間を制限します。

## 44. Anti-aliasing

- MSAA: geometry edge。Forward系と相性、transparent shader aliasには万能でない。
- FXAA: 軽量post-process、全体が少しsoft。
- SMAA: edge検出品質とcost。
- TAA: 時間sample、高品質だがghosting、motion vector、jitter。

Pipeline/Renderer/Camera対応を確認します。高速Character/VFXでTAA ghostingをtestします。

## 45. Dynamic ResolutionとUpscaling

GPU負荷に応じrender resolutionを下げ、upscaleします。

確認:

- UIをnative resolutionで描くか。
- Camera stacking。
- Screen Space effect。
- TAA/FSR等との組合せ。
- sharpness。
- Lock-on outline/細いVFX。

平均FPSだけでなく画質変動とframe pacingを評価します。

## 46. Combat Post Processing

```csharp
public readonly struct CombatPostFxRequest
{
    public CombatPostFxRequest(
        float bloomBoost,
        float vignette,
        float saturation,
        float duration)
    {
        BloomBoost = bloomBoost;
        Vignette = vignette;
        Saturation = saturation;
        Duration = duration;
    }

    public float BloomBoost { get; }
    public float Vignette { get; }
    public float Saturation { get; }
    public float Duration { get; }
}
```

Hit、Finisher、Low HP、Pauseのrequestを一つのPostFx Directorが合成します。終了した一件が全Effectをbaselineへ戻して他requestを壊さないようtokenを使います。

## 47. Hit Stopとの時間

Post Processing animationをscaled timeで進めると`timeScale=0`で止まります。

- Hit flash envelopeはunscaled。
- Gameplay buff colorはscaled。
- Pause menu blurはunscaled。
- CutsceneはTimeline time。

Effectごとのclockを明記します。

## 48. Additive SceneとLighting

Lighting Scene、Stage Scene、Gameplay SceneをAdditive loadする場合:

- Active Scene。
- Lighting Settings。
- Lightmap data。
- Reflection Probe。
- APV Baking Set/Scenario。
- Skybox。
- Volume Priority。
- duplicate Directional Light。

ロード順だけで偶然正しく見える構成にせず、Scene BootstrapがLighting stateを確定します。

## 49. 昼夜・Lighting Scenario

RealtimeですべてのGIを変えるのではなく、複数baked scenarioを切り替える仕組みがあります。APV Lighting Scenario等の対応を利用版で確認します。

切替:

- Direct Light rotation/color。
- Sky。
- Probe/APV data。
- Reflection。
- Fog/Post Processing。
- Gameplay visibility。

blend中のmemoryとstreaming costを計測します。

## 50. Debug

- Lighting Debug View。
- Lightmap Density。
- Texel Validity/UV overlap。
- Shadow Cascade visualization。
- Reflection Probe preview。
- Light Probe/APV cell/validity。
- Rendering Debugger。
- Frame Debugger。
- GPU Profiler。
- Overdraw。
- Volume stack。

見た目が暗いとき:

```text
Light intensity?
 → Exposure?
 → Material albedo/metallic?
 → Probe/GI?
 → Shadow?
 → AO?
 → Tone Mapping?
 → Color Grading?
```

順にisolated viewで確認します。

## 51. 性能

Lighting/Shadow cost:

- Realtime Shadow Light数。
- Point Light shadow 6方向。
- Shadow Atlas/Cascade/Resolution。
- Shadow Caster数。
- Additional Light per object/tile。
- Soft Shadow sample。
- Reflection Probe更新。
- APV memory/sample。
- Post Process pass/resolution。
- HDR Render Target。
- Motion Vector。

Low Quality例:

- Shadow Distance短縮。
- Cascade削減。
- Additional shadow Off。
- SSAO/DoF/Motion Blur Off。
- Bloom品質削減。
- HDR/Render Scaleをplatformに応じ調整。

## 52. よくある不具合

- Realtime/Baked/Mixedの役割を混同する。
- Dynamic CharacterがLightmapを直接受けると思う。
- Light Probeを一平面にだけ置く。
- Reflection Probe無しでMetalが黒い。
- Realtime Reflection Probeを毎frame全解像度更新。
- Shadow Distanceを広げれば品質が上がると思う。
- Point Light一個のshadow costをmap一枚と数える。
- Biasを上げすぎshadowが浮く。
- Thin wallでLight/APV leak。
- Volume Layer Maskが合わずeffectが出ない。
- shared Profile Assetをruntimeで書き換える。
- Bloomで白飛びを隠す。
- Auto Exposureで戦闘中の敵を見失う。
- Motion Blur/DoFを強制して酔いを招く。
- 複数SystemがVolume値を直接上書きする。
- Additive SceneでDirectional Light/Global Volumeが重複する。

## 53. Test Matrix

| 観点 | Test |
|---|---|
| Time | 昼、夜、室内、屋外、遷移 |
| Light | Realtime、Baked、Mixed、0/多数 |
| Object | Static、Character、moving prop、VFX |
| Probe | 境界、壁両側、階段、Scene seam |
| Shadow | near/far、cascade境界、thin mesh |
| Camera | FOV変化、dynamic resolution、stack |
| Combat | Hit、Dodge、Finisher、Low HP |
| Post | 全ON、個別OFF、accessibility設定 |
| Quality | Low/Medium/High |
| Device | target最低/推奨hardware、HDR/LDR |

## 54. 設計チェックリスト

- Direct/Indirect、Realtime/Baked/Mixedを説明できるか。
- Static/Dynamic分類がScene規約にあるか。
- Lightmap UVとtexel densityを確認したか。
- Character移動範囲にProbe/APVがあるか。
- Reflection Probeのvolume/更新頻度が適切か。
- Shadow Atlasへ必要map数が収まるか。
- Bias/Cascade/Distanceをdebug表示で調整したか。
- Characterの視認性を専用Lightだけへ依存していないか。
- Volume Profileのruntime所有権が明確か。
- Combat PostFx要求をDirectorが合成するか。
- Motion Blur/Shake/DoF等を無効化できるか。
- Additive SceneのLighting ownerが一つか。
- 最低hardwareでGPU時間とmemoryを測ったか。

## 公式資料

- [Unity Manual: Lighting](https://docs.unity3d.com/6000.0/Documentation/Manual/LightingOverview.html)
- [Unity Manual: Lighting Settings](https://docs.unity3d.com/6000.0/Documentation/Manual/class-LightingSettings.html)
- [Unity Manual: Lightmaps](https://docs.unity3d.com/6000.0/Documentation/Manual/Lightmappers.html)
- [Unity Manual: Light Probes](https://docs.unity3d.com/6000.0/Documentation/Manual/LightProbes.html)
- [Unity Manual: Reflection Probes](https://docs.unity3d.com/6000.0/Documentation/Manual/ReflectionProbes.html)
- [Unity Manual: URP Adaptive Probe Volumes](https://docs.unity3d.com/6000.0/Documentation/Manual/urp/probevolumes.html)
- [Unity Manual: Shadows in URP](https://docs.unity3d.com/6000.0/Documentation/Manual/urp/Shadows-in-URP.html)
- [Unity Manual: Optimize shadows in URP](https://docs.unity3d.com/6000.0/Documentation/Manual/shadows-optimization.html)
- [Unity Manual: Add post-processing in URP](https://docs.unity3d.com/6000.0/Documentation/Manual/urp/add-post-processing.html)
- [Unity Manual: URP post-processing effects](https://docs.unity3d.com/6000.0/Documentation/Manual/urp/EffectList.html)

