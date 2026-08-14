# Audio・AudioMixer・Particle System・VFX Graph

> 対象: Unity 6。VFX GraphはPackage/Render Pipeline/Graphics APIで対応差がある。Unity 6公式資料ではHDRPでproduction-ready、URP/mobileの完全対応は継続開発中とされるため、対象platformを実機確認すること。

## 1. 戦闘Presentationの流れ

```text
Combat Simulation
 → Hit Result（damage、position、normal、material、severity）
 → Presentation Event
   ├─ Audio Director
   ├─ VFX Director
   ├─ Camera Impulse
   ├─ UI
   └─ Animation Reaction
```

Audio/VFX callback内でdamageを確定しません。Combat結果を表現するconsumerとして扱います。

## 2. Audioの基本構造

- AudioClip: sample dataとImport設定。
- AudioSource: clipを再生し、volume/pitch/spatial等を設定。
- AudioListener: 聴取位置。通常Main Camera付近に一つ。
- AudioMixer: group、volume、effect、snapshot、routing。
- Audio Settings: DSP buffer、voice数、speaker mode等。

```text
AudioSource
 → AudioMixerGroup
   → SFX / Voice / Music / UI
     → Master
       → Audio Output
```

## 3. AudioListener

Scene内のListener重複は警告や意図しない結果を招きます。Camera交代/CutsceneでもAudioListener ownerを一つにします。

ListenerをCameraへ完全追従させると、Third-person CameraがPlayerから離れた際に3D音の距離感が変わります。

選択:

- Camera位置。
- Player位置。
- CameraとPlayerの補間点。

方向はCamera、位置はPlayer寄りというcustom listener rigも検討します。

## 4. AudioClip Import

主な設定:

- Force To Mono。
- Normalize。
- Load In Background。
- Ambisonic。
- Load Type。
- Compression Format。
- Quality。
- Sample Rate Setting。
- Preload Audio Data。

Platform overrideでMobile/Console/PC別にformat/qualityを調整します。

## 5. Load Type

### Decompress On Load

load時に展開しPCM相当をmemoryへ。再生時CPU/latencyを抑えやすいがmemoryが大きい。短く頻繁なSFXに向きます。

### Compressed In Memory

圧縮状態でmemoryへ置き、再生時decode。memoryを抑えるがDSP CPUが増えます。中程度ClipやBGM候補。

### Streaming

disk等から逐次読み込み。長いBGM/Voiceに向く。stream buffer、I/O、同時stream数を考慮します。

目安を暗記せずAudio ProfilerでSample Sound Memory、Streaming CPU、DSP CPUを測ります。

## 6. Compression Format

- PCM: 無圧縮、高memory/size、低decode cost。
- ADPCM: 軽いdecode、中程度圧縮。短い反復SFX候補。
- Vorbis/MP3等: 高圧縮、decode cost。Music/Voice候補。

loop seam、transient、pitch変化、platform decoderを実音で確認します。

## 7. AudioSource

重要property:

- clip。
- output AudioMixerGroup。
- mute/volume/pitch。
- loop。
- playOnAwake。
- spatialBlend。
- priority。
- min/max distance。
- rolloff。
- dopplerLevel。
- spread。
- reverb zone mix。

Prefab defaultとruntime requestを分け、各scriptが無秩序にSource propertyを残さないようPool release時にresetします。

## 8. 2Dと3D Audio

- spatialBlend=0: 2D。Listener距離・方向の影響なし。
- spatialBlend=1: 3D。
- 中間: blend。

用途:

- UI、Music: 2D。
- Hit、Footstep、Enemy Voice: 3D。
- Player自身のattack: 2D寄りと3Dのlayerを分ける場合。

Player SFXがCamera後方へpanしすぎると操作feedbackが弱くなるため、listener設計とspatial blendを調整します。

## 9. Rolloff

距離減衰:

- Logarithmic。
- Linear。
- Custom curve。

```text
Min Distance: full-volume領域の目安
Max Distance: 減衰末端の目安
```

Max Distanceを大きくすれば聞こえやすい一方、多数voiceが残ります。Boss予兆等は3D音+UI/2D cueを併用します。

## 10. Doppler

Source/Listenerの相対速度でpitchが変わります。高速Character/Projectileで過剰に変化することがあるため、必要effectだけ有効にします。

Transform teleportやpool再配置が巨大velocityとして解釈されないかtestします。

## 11. PlayとPlayOneShot

`AudioSource.Play`はsourceのclipを再生します。同じSourceで再度Playするとrestart等の挙動になります。

`PlayOneShot`は一つのSourceから重ねてClipを鳴らせますが、個々のvoice停止・追跡・property管理が弱くなります。

重要Voiceは専用pooled Source、軽いSFXはOneShotと使い分けます。

## 12. Audio Voice

AudioSource数と実際のAudio Voice数は同じではありません。real voice上限を超えるとpriority/volume等でvirtualizeされます。

Audio Profilerで:

- Playing。
- Paused。
- Virtual。
- OneShot。
- Priority。
- Volume。
- Distance。

を確認します。

## 13. Voice Priority

Unityのpriority数値の方向をAPIで確認し、意味名へ変換します。

```text
Critical UI/Dialogue
Player Combat
Boss Telegraph
Nearby Enemy
Environment
Far Ambience
```

全Sourceを最高priorityにすると優先制御が無意味です。

## 14. Voice Limit

同種音を無制限に鳴らさない仕組み:

- global maximum。
- event/category maximum。
- emitter maximum。
- distance culling。
- cooldown。
- newest/oldest/quietest replacement。
- same-frame merge。

10体へ同時hitしたとき、hit SFXを10個完全重ねるより、1～3voiceへ集約しvolume/pitchを調整した方が明瞭です。

## 15. Audio Event Definition

```csharp
using UnityEngine;
using UnityEngine.Audio;

[CreateAssetMenu(menuName = "Game/Audio/Event")]
public sealed class AudioEventDefinition : ScriptableObject
{
    [SerializeField] private AudioClip[] clips;
    [SerializeField] private AudioMixerGroup output;
    [SerializeField, Range(0.0f, 1.0f)] private float volume = 1.0f;
    [SerializeField] private Vector2 pitchRange = Vector2.one;
    [SerializeField, Range(0.0f, 1.0f)] private float spatialBlend = 1.0f;
    [SerializeField, Min(1)] private int voiceLimit = 8;

    public AudioClip[] Clips => clips;
    public AudioMixerGroup Output => output;
    public float Volume => volume;
    public Vector2 PitchRange => pitchRange;
    public float SpatialBlend => spatialBlend;
    public int VoiceLimit => voiceLimit;
}
```

runtime voice countをAsset fieldへ書きません。Audio Directorがevent IDごとの状態を持ちます。

## 16. Random variation

同じSFXの機械的反復を避け:

- 複数Clip。
- pitch微差。
- volume微差。
- cooldown。
- shuffle bag。

完全randomで同じClipが連続しないようshuffle bagを使えます。Gameplay replayで音再現が必要ならrandom seed/selection indexを記録します。

## 17. Audio Director

```csharp
public readonly struct AudioPlayRequest
{
    public AudioPlayRequest(
        AudioEventDefinition definition,
        Vector3 position,
        uint ownerId)
    {
        Definition = definition;
        Position = position;
        OwnerId = ownerId;
    }

    public AudioEventDefinition Definition { get; }
    public Vector3 Position { get; }
    public uint OwnerId { get; }
}
```

Director責務:

- Clip選択。
- Voice limit。
- Pool。
- Mixer routing。
- distance/priority。
- duplicate merge。
- debug log。

## 18. AudioSource Pool

```text
Request
 → Pool.Get Source
 → Transform/property/clip設定
 → Play
 → 終了検出
 → Stop/reset
 → Pool.Release
```

reset項目:

- clip。
- output。
- loop。
- volume/pitch。
- spatialBlend。
- rolloff/min/max。
- doppler。
- bypass。
- Transform parent/position。
- scheduled state。

Clip length/pitchだけでrelease時刻を推定するとpause/schedule/streamでずれる場合があります。再生状態とowner tokenを管理します。

## 19. AudioMixer

```text
Master
├─ Music
├─ SFX
│  ├─ Player
│  ├─ Enemy
│  └─ Environment
├─ Voice
└─ UI
```

groupへEffectを追加:

- EQ。
- Compressor。
- Ducking。
- Reverb。
- Low-pass。

Mixer構造はvolume設定、snapshot、voice readabilityの基盤です。

## 20. dB

AudioMixer volumeは通常dBです。UI Slider 0～1を線形にdBへ渡すと感覚が不自然です。

```csharp
public static float LinearToDecibels(float linear)
{
    const float minimumLinear = 0.0001f;
    return 20.0f * Mathf.Log10(
        Mathf.Max(minimumLinear, linear));
}
```

0は`-80 dB`等のmute値へ明示mappingします。保存値は0～1、適用時dB変換等の契約を決めます。

## 21. Exposed Parameter

Mixer parameterをExposeし`AudioMixer.SetFloat`で変更できます。文字列名はcompileで検出されないため定数/validatorを使います。

UI Sliderの変更を毎frameではなくvalue changed時に適用し、Save settingsへ保存します。

## 22. Snapshot

複数Mixer property状態を保存しtransitionできます。

例:

- Gameplay。
- Pause（SFX low-pass/volume低下）。
- Underwater。
- Finisher。
- Dialogue focus。

複数SystemがSnapshotを直接Transitionすると競合します。Audio State Directorがrequest priority/tokenを管理します。

## 23. Ducking

Voice中にMusic/SFXを一時的に下げ、聞き取りやすくします。

- sidechain/compressor。
- Snapshot。
- script envelope。

Boss予兆voiceを優先する場合も、すべてのattackでMusicを大きく揺らさないようthreshold/hold/releaseを調整します。

## 24. DSP Clock

`AudioSettings.dspTime`はaudio systemの高精度clockとしてscheduleへ使えます。

```csharp
double startAt = AudioSettings.dspTime + 0.2;
audioSource.PlayScheduled(startAt);
```

BGMのgapless transition、rhythm同期に`PlayScheduled`を使います。`Time.time`/frame callbackだけではsample精度の開始を保証しません。

## 25. Music Transition

```text
Track A playing
 → next bar DSP time計算
 → Track B preload
 → PlayScheduled(nextBar)
 → A SetScheduledEndTime(nextBar)
```

BPM、sample rate、intro/loop/outro、bar offset metadataを持ちます。Streaming loadが間に合わない場合のfallbackを設計します。

## 26. Pause

選択:

- `AudioListener.pause`等で全体停止。
- Mixer Snapshotで減衰。
- Music継続、SFX停止。
- UI音だけ継続。

Gameplay timeScale=0だけではAudioが自動で仕様通り止まるとは限りません。Audio DirectorがGame FlowからPause stateを受けます。

## 27. AudioとCharacter交代

- 旧loop voice停止/fade。
- 新Character voice bank。
- attack途中voiceを残すか。
- listener target。
- 旧async clip load。
- voice owner generation。

Pooled CharacterのAudioSourceをdisableしただけでloop/scheduled voiceが完全resetされたと決めつけず、release procedureを作ります。

## 28. Audio Accessibility

- Music/SFX/Voice/UI別volume。
- Subtitle。
- speaker name。
- sound direction indicator。
- mono audio。
- dynamic range preset。
- tinnitus/high-frequencyへの配慮。

重要予兆をAudioだけへ依存させずVisual/UIも併用します。

## 29. Particle System

CPU側を中心に柔軟なParticle effectを作るGameObject Componentです。

主module:

- Main。
- Emission。
- Shape。
- Velocity/Force/Limit Velocity。
- Color/Size/Rotation over Lifetime。
- Noise。
- Collision/Trigger。
- Texture Sheet Animation。
- Trails。
- Sub Emitters。
- Renderer。
- Lights。

## 30. Particle lifetime

```text
Emit at t0
 → start lifetime
 → modules evaluate over normalized age
 → particle dies
```

System durationとparticle lifetimeは別です。Emission停止後も既存particleが残ります。

## 31. Simulation Space

- Local: emitter移動へparticleが追従。
- World: 発生後はWorldに残る。
- Custom: 指定Transform空間。

Sword trail sparksをWorldへ残すか、Characterへ追従させるかで選びます。Pool再配置前にparticleをclearしないと旧位置のparticleが新位置へ飛ぶ場合があります。

## 32. Scaling Mode

Hierarchy/Local/Shape等、Transform scaleをparticleのどこへ反映するか選びます。Prefabを非一様scaleするとparticle size/velocity/shapeが予想外になります。

VFX prefab root scale=1を規約にし、公開parameterでsizeを調整する方が安定します。

## 33. Emission

- Rate over Time。
- Rate over Distance。
- Burst。

Hit effectはBurst、trailはDistance、ambientはTime等。

Rate over Distanceはemitter teleportで巨大数をemitする可能性があるため、warp時reset/clearします。

## 34. Particle random

Main moduleのRandom Seedを固定すると再現可能です。自動seedはvariationに向きます。

Replay/recording/visual test:

- fixed seed。
- fixed delta simulation。
- deterministic parameter。

Physics collision等を含む完全一致は別途testします。

## 35. Curves

`MinMaxCurve`:

- Constant。
- Two Constants。
- Curve。
- Two Curves。

Inspector Curveは便利ですが、複雑Curveを全particleでsampleするcost、Asset diff、rangeを確認します。

## 36. Renderer

- Billboard。
- Stretched Billboard。
- Horizontal/Vertical Billboard。
- Mesh。
- Trail。

Material、Render Queue、sorting、shadow、GPU instancing、pivot、alignmentを設定します。

Transparent particleはOverdrawが主costになりやすいです。particle数だけでなくscreen占有pixelを測ります。

## 37. Overdraw

```text
大型半透明quad × 多数 × 4K
 → 同じpixelを何十回もfragment shading
```

対策:

- quad size削減。
- particle数削減。
- alpha textureの空白削減。
- mesh particle。
- lower resolution buffer。
- additive/opaque clipの選択。
- camera distance culling。

## 38. Particle Collision

World/Planes等でcollisionできますが高costです。

- quality。
- voxel/cache。
- radius scale。
- layer mask。
- collision messages。

Gameplay hit判定へParticle Collisionを使いません。VFX collisionは見た目のspark/decal用です。

## 39. Sub Emitters

birth/death/collision/trigger等から別Particle Systemを発生できます。連鎖でparticle数が爆発しないよう最大値を計算します。

```text
100 parent
 × deathで10 child
 = 1000 child
```

## 40. Trails

Weapon slashやprojectile trailに使えます。

- ratio。
- lifetime。
- minimum vertex distance。
- texture mode。
- world/local。
- ribbon count。

頂点数と透明overdrawをProfiler/Frame Debuggerで測ります。

## 41. Stop Action

System完了時:

- None。
- Disable。
- Destroy。
- Callback。

PoolではCallbackからreleaseできます。

```csharp
using UnityEngine;
using UnityEngine.Pool;

[RequireComponent(typeof(ParticleSystem))]
public sealed class ReturnParticleToPool : MonoBehaviour
{
    public IObjectPool<ParticleSystem> Pool { private get; set; }
    private ParticleSystem system;

    private void Awake()
    {
        system = GetComponent<ParticleSystem>();
        var main = system.main;
        main.stopAction = ParticleSystemStopAction.Callback;
    }

    private void OnParticleSystemStopped()
    {
        Pool?.Release(system);
    }
}
```

Child systemも含め全particleが完了する条件をtestします。

## 42. Particle Pool reset

Get:

- Transform位置/回転。
- random seed。
- parameter/module。
- clear。
- play。

Release:

- stop emitting and clear。
- callback/listener解除。
- trail clear。
- child reset。
- GameObject inactive。

`StopEmitting`は既存particleを残し、`StopEmittingAndClear`は削除します。用途を分けます。

## 43. VFX Graph

GPU上で大量particleをsimulationするnode-based systemです。

```text
Spawner
 → Initialize
 → Update
 → Output

Blackboard Properties
Events
Systems
Contexts
Blocks
Operators
```

大量particle、complex GPU simulationに強い一方、CPU Gameplayとの細かな双方向連携、platform/compute対応、readbackに注意します。

## 44. Particle SystemとVFX Graph

| 観点 | Particle System | VFX Graph |
|---|---|---|
| Simulation | 主にCPU | GPU |
| Particle数 | 少～中 | 大量向き |
| GameObject連携 | 容易 | exposed property/event |
| CPU readback | 扱いやすい | 高cost/非同期 |
| Platform | 広い | compute/Pipeline制約 |
| Gameplay collision | 使わない | 使わない |

小さいhit effectまで全てVFX Graphへする必要はありません。

## 45. VFX Graph Package

Core PackageはEditor Versionへ対応します。Unity 6公式Manual上、HDRPはproduction-ready、URP/mobile supportは対応状況を確認する必要があります。

Build target:

- Compute Shader support。
- Graphics API。
- URP/HDRP Renderer。
- Mobile。
- XR。
- Web。

を早期に実機testします。

## 46. VFX Graph Event

`Play`/`Stop`やcustom eventでSpawner flowを起動できます。

```csharp
using UnityEngine;
using UnityEngine.VFX;

public sealed class HitVfxPlayer : MonoBehaviour
{
    private static readonly int HitPositionId =
        Shader.PropertyToID("HitPosition");

    [SerializeField] private VisualEffect visualEffect;

    public void PlayAt(Vector3 position)
    {
        visualEffect.SetVector3(HitPositionId, position);
        visualEffect.SendEvent("OnHit");
    }
}
```

property/event名は文字列/hash契約なのでGraph validatorを用意します。

## 47. Exposed Property

- float/vector/color。
- Texture。
- Mesh。
- Gradient/Curve等、対応type。
- GraphicsBuffer。

Propertyを毎frame送るとCPU→GPU uploadがあります。値変更時だけ、またはまとめて送ります。

Character bone追従:

- Transform Binder。
- scriptでposition。
- Skinned Mesh sampling。

PackageのProperty Binder機能とcostを確認します。

## 48. Bounds

GPU particle位置をCPUが完全に知らないため、VFX Boundsが不適切だと画面内でもcullされます。

- fixed bounds。
- automatic bounds/record。
- effect最大移動範囲。
- Character teleport。
- world/local space。

Boundsを巨大にすると常にrenderされ、cullingが効きません。

## 49. Capacity

VFX Systemの最大particle capacityはGPU memory/draw workloadへ影響します。

```text
spawnRate × maxLifetime ≈ simultaneous particle count
```

Burst、subsystem、stripを含めpeakを見積もり、余裕を持たせつつ巨大capacityにしません。

## 50. VFX output

- Quad。
- Mesh。
- Particle Strip。
- Decal等、Pipeline対応。
- Lit/Unlit。
- Six-way smoke。

Lit smokeはnormal/lighting dataとsample costが増えます。重要effectに限定します。

## 51. VFX collision

Depth buffer、SDF、plane等のGPU collision手法があります。

制約:

- screen spaceは画面外情報なし。
- depth thickness。
- SDF memory。
- Gameplay Physicsとは一致しない。

見た目のbounce/killに使い、damage判定へ戻しません。

## 52. GPU Readback

VFX particle結果をCPUへ読むAsync GPU Readback等は同期/latency/costがあります。毎particle collisionをCPUへ戻してGameplay処理する設計を避けます。

Gameplayが先にhitを確定し、その結果をVFX Graphへ送ります。

## 53. Decal

Hit mark、ground effectへDecalを使えます。Pipeline対応、transparent対象、normal blend、atlas、draw count、lifetimeを確認します。

Decal pool:

- position/normal alignment。
- z-fighting offset。
- size/rotation random。
- surface type filter。
- lifetime/fade。
- maximum per area/global。

## 54. Hit VFX Request

```csharp
public readonly struct HitVfxRequest
{
    public HitVfxRequest(
        Vector3 position,
        Vector3 normal,
        int surfaceType,
        int severity,
        uint attackInstanceId)
    {
        Position = position;
        Normal = normal;
        SurfaceType = surfaceType;
        Severity = severity;
        AttackInstanceId = attackInstanceId;
    }

    public Vector3 Position { get; }
    public Vector3 Normal { get; }
    public int SurfaceType { get; }
    public int Severity { get; }
    public uint AttackInstanceId { get; }
}
```

VFX Directorがsurface/severity/qualityからeffect definitionを選びます。

## 55. Surface Response

```text
Hit Result
 → Surface ID
   ├─ Metal: sparks + metallic SFX
   ├─ Stone: dust + stone SFX
   ├─ Flesh: stylized impact
   └─ Shield: energy ring + parry SFX
```

Physics Material名をstring比較せず、Surface Component/ID/Material mapping Assetを使います。

## 56. Effect Definition

```csharp
[CreateAssetMenu(menuName = "Game/VFX/Effect Definition")]
public sealed class EffectDefinition : ScriptableObject
{
    [SerializeField] private GameObject prefab;
    [SerializeField, Min(1)] private int poolCapacity = 8;
    [SerializeField, Min(0.0f)] private float maximumLifetime = 3.0f;
    [SerializeField] private bool alignToNormal = true;

    public GameObject Prefab => prefab;
    public int PoolCapacity => poolCapacity;
    public float MaximumLifetime => maximumLifetime;
    public bool AlignToNormal => alignToNormal;
}
```

runtime pool/listをAssetへ書きません。

## 57. VFX Director

- definition resolve。
- pool。
- spawn budget。
- distance/visibility。
- quality tier。
- duplicate merge。
- lifetime/cancel。
- surface mapping。
- debug stats。

Hit callbackから直接`Instantiate(vfxPrefab)`を散らしません。

## 58. VFX Budget

```text
Per frame:
max new effects
max active hit effects
max decals
max transparent particles
max lights
max audio voices
```

同frame多数hit:

- critical/player effect優先。
- distant effect drop。
- same position merge。
- particle count reduction。
- light/shadow off。

## 59. Quality Tier

High:

- VFX Graph full capacity。
- lit smoke。
- distortion。
- additional light。
- decals/trails。

Low:

- Particle System簡易版。
- lower spawn/capacity。
- unlit。
- no distortion/light。
- shorter lifetime。

Prefab/Graph差替えをEffect Definitionからresolveします。

## 60. Combat Readability

Effectは派手さより意味を伝えます。

- Player attackとEnemy attackの色/shape。
- parry可能。
- unblockable。
- area boundary。
- hit confirm。
- invincible。
- Character switch。

Bloom/transparent smokeでEnemy silhouetteを隠さないよう、Camera viewで調整します。

## 61. Colorだけに依存しない

危険度を:

- shape。
- timing。
- motion。
- sound。
- icon。
- outline。
- screen cue。

でも表します。Color blindness settingでpaletteを変更できるようEffect propertyをdata化します。

## 62. Time Scale

Particle System:

- Simulation Speed。
- Delta Time scaled/unscaled設定。

VFX Graph:

- update mode/time property。
- custom delta。

Audio:

- timeScaleとは別。
- pitch変更をSlow Motionへ連動するか。

Hit Stop中:

- VFXを止める。
- impact ringだけunscaledで進める。
- Audio pitch snapshot。

effectごとに時計を表にします。

## 63. Character交代

- weapon trail停止/clear。
- loop auraを旧Characterから解除。
- 新Character aura。
- Audio loop fade。
- bone binder target変更。
- old async load generation。
- pooled effect parent解除。

Worldに残すhit sparkとCharacterへ追従するauraを区別します。

## 64. Scene遷移

Persistent VFX/Audio DirectorがScene objectをparent/targetに持つとunload後fake nullになります。

- one-shotをScene寿命へbound。
- Musicはpersistent。
- poolをScene別またはglobal。
- async result generation。
- Mixer Snapshot復旧。
- Listener/Camera再bind。

## 65. Debug

Audio:

- event ID。
- Clip。
- Source。
- owner。
- priority。
- distance。
- real/virtual。
- Mixer Group。
- elapsed。

VFX:

- effect ID。
- active/pool count。
- particle count/capacity。
- spawn rejected reason。
- bounds。
- GPU time/overdraw。

Development overlayとring bufferを作り、毎framelogを避けます。

## 66. Profiler

Audio Profiler:

- DSP CPU。
- Streaming CPU。
- Audio memory。
- voice count。
- virtual voice。

Particle:

- CPU simulation。
- rendering。
- overdraw。
- module cost。

VFX Graph:

- VFX Profiler/debug panel。
- GPU Profiler。
- Frame Debugger。
- RenderDoc。
- memory。

Editorだけでなくtarget deviceをprofileします。

## 67. よくある不具合

- SceneにAudioListenerが複数。
- Camera位置だけをListenerにして距離感が不自然。
- 全ClipをDecompress On Loadしてmemory超過。
- 長いBGMをPCM。
- 全AudioSourceを最高priority。
- PlayOneShotを無制限に重ねる。
- Mixer Slider 0～1をdBへ線形代入。
- 複数SystemがSnapshotを奪い合う。
- pool releaseでAudioSource propertyをresetしない。
- particleをclearせず旧位置から再利用。
- 全Particle Collision messageをGameplayへ使う。
- Sub Emitterでparticle数が爆発。
- screenを覆う透明particleでoverdraw。
- VFX Boundsが小さく突然消える。
- Capacityを根拠なく最大。
- GPU readbackでdamage判定。
- Hit callbackからPrefabを直接生成。
- Character交代後も旧trail/auraが残る。

## 68. Test Matrix

| 観点 | Test |
|---|---|
| Audio | 1/多数voice、distance、pause、device change |
| Import | Decompress、Compressed、Streaming |
| Mixer | snapshot競合、mute、volume保存 |
| Time | normal、slow、hit stop、pause |
| Particle | local/world、teleport、pool、off-screen |
| VFX | capacity、bounds、URP/HDRP、low/high quality |
| Combat | 同時hit、multi-hit、parry、switch |
| Scene | unload、persistent music、async result |
| Device | low GPU/CPU、mobile/console target |
| Accessibility | volume categories、subtitle、effect reduction |

## 69. 設計チェックリスト

- Combat結果からtyped Presentation Eventを送るか。
- Listenerの位置/方向ownerは一つか。
- Clip Importを用途別に設定したか。
- Audio Voice limit/priorityがあるか。
- AudioMixer groupとSnapshot競合を管理するか。
- DSP scheduleが必要なMusicをframe clockで始めていないか。
- Particle/VFXをpoolし全状態をresetするか。
- Particle System/VFX Graphをplatform要件で選んだか。
- VFX Graph bounds/capacityを測ったか。
- Gameplay collisionとVFX collisionを分離したか。
- effect spawn budgetとdrop ruleがあるか。
- Character交代/Scene unloadでloop/trailを解除するか。
- Audio/VFXをPlayer設定で軽減できるか。
- target deviceでDSP/GPU/overdrawをprofileしたか。

## 公式資料

- [Unity Manual: Audio](https://docs.unity3d.com/6000.0/Documentation/Manual/AudioOverview.html)
- [Unity Manual: Audio files](https://docs.unity3d.com/6000.0/Documentation/Manual/AudioFiles.html)
- [Unity Manual: Audio Mixer](https://docs.unity3d.com/6000.0/Documentation/Manual/AudioMixer.html)
- [Unity Manual: Audio Profiler](https://docs.unity3d.com/6000.0/Documentation/Manual/ProfilerAudio.html)
- [Unity API: AudioSource.PlayScheduled](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/AudioSource.PlayScheduled.html)
- [Unity Manual: Particle System](https://docs.unity3d.com/6000.0/Documentation/Manual/ParticleSystems.html)
- [Unity Manual: Particle System Main module](https://docs.unity3d.com/6000.0/Documentation/Manual/PartSysMainModule.html)
- [Unity API: ObjectPool](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/Pool.ObjectPool_1.html)
- [Unity Manual: Visual Effect Graph](https://docs.unity3d.com/6000.0/Documentation/Manual/com.unity.visualeffectgraph.html)
- [Unity Manual: Render pipeline feature comparison](https://docs.unity3d.com/6000.0/Documentation/Manual/render-pipelines-feature-comparison.html)

