# UnityとUnreal Engineの対応関係・設計比較

> 対象: Unity 6.0とUnreal Engine 5.x。どちらが常に上という比較ではなく、同じ概念を別Engineへ翻訳し、誤った一対一対応を避けるための章です。

## 1. 比較の目的

Engineを移るとき、API名だけを置換すると設計を誤ります。

```text
Unityで何を達成していたか
→ その責任・寿命・所有者・data flowを特定
→ Unrealの標準frameworkで対応する責任を探す
→ 足りない部分だけ独自実装
```

`MonoBehaviour`をそのまま`AActor`へ、`ScriptableObject`を無条件に`UDataAsset`へ置換するのではなく、用途から判断します。

## 2. 全体対応表

| Unity | Unreal Engine | 注意点 |
|---|---|---|
| GameObject | Actorに近い | Actorはclass自体へ振る舞いを持てる |
| Component | ActorComponent | SceneComponentだけTransformを持つ |
| Transform | SceneComponent階層 | Actorのroot componentがworld Transform基準 |
| Scene | Level/World | World、Persistent Level、sublevelも考慮 |
| Prefab | Blueprint Classに近い | construction、class継承の性質が異なる |
| MonoBehaviour | Actor/ActorComponent | 一つの完全な対応型ではない |
| ScriptableObject | Data Assetに近い | UObject asset、Primary Asset設計が別途ある |
| Animator | AnimInstance/Animation Blueprint | Montage、Notify、Graphへ責任分散 |
| CharacterController | Character Movement | movement modelとnetwork機能が異なる |
| NavMeshAgent | AIController＋Path Following/Pawn | Agent単体対応では不足 |
| Addressables | Asset Manager/Soft Reference | label/groupとPrimary Asset体系は別概念 |
| uGUI/UI Toolkit | UMG/Slate | runtime/editor UI層の構成が異なる |
| Particle/VFX Graph | Cascade/Niagara | 現行UEではNiagaraが中心 |

## 3. 言語: C#とC++

Unity gameplayは主にC#、Unreal gameplayはC++とBlueprintを組み合わせます。

### Unity C#

- Managed Runtime、GC。
- assembly/asmdef。
- reflection/serializationにUnity独自規則。
- compile iterationが比較的軽い。

### Unreal C++

- native C++、RAIIと明示的所有権。
- UObject系にはReflection/GC層。
- UBT/UHTとmodule。
- macroとgenerated code。

Unreal C++は標準C++にEngine固有object modelを追加したもので、C#のclassを機械翻訳しただけでは安全になりません。

## 4. Compile単位

| Unity | Unreal |
|---|---|
| `.asmdef` | Module `.Build.cs` |
| assembly reference | module dependency |
| Assembly-CSharp | game module |
| Editor assembly | Editor module |
| Define Constraints | target/module/compiler definitions |

どちらもfolder分けだけでは依存を制限できません。compilerが検証するmodule/assembly境界を使います。

## 5. Reflection

UnityはC# reflectionとUnity serialization/Inspector規則を使います。UnrealはUHTが`UCLASS`、`UPROPERTY`、`UFUNCTION`等を解析し、generated codeとReflection metadataを作ります。

```csharp
[SerializeField] private float health;
```

```cpp
UPROPERTY(EditAnywhere, BlueprintReadOnly)
float Health = 100.0f;
```

属性とmacroは似て見えますが、生成時期、対象型、GC/replication連携が異なります。

## 6. Object model

Unityの`UnityEngine.Object`はnative objectをmanaged wrapperから参照する特殊な二層構造です。Unrealの`UObject`はReflection、GC、serialization等の基盤となるnative objectです。

どちらも通常の言語objectと同じnull/寿命だと仮定しません。

## 7. GameObjectとActor

UnityのGameObjectはComponent containerで、Transformを必ず持ちます。UnrealのActorはUObject subclassでworldへspawnでき、Actor class自身へlogicを実装でき、Componentも所有できます。

```text
Unity:
GameObject
├─ Transform
├─ CharacterMotor : MonoBehaviour
└─ Health : MonoBehaviour

Unreal:
ACharacter
├─ Root Capsule : SceneComponent
├─ SkeletalMeshComponent
├─ CharacterMovementComponent
└─ HealthComponent : ActorComponent
```

## 8. Component

Unity ComponentはGameObjectのTransformを共有します。Unrealでは:

- `UActorComponent`: Transformなしのlogic component。
- `USceneComponent`: Transformとattachmentあり。
- `UPrimitiveComponent`: rendering/collision等のscene primitive。

位置不要なHealthをSceneComponentにしない、という選択が明示的です。

## 9. Transform hierarchy

UnityではGameObjectごとにTransformがありparent/childを作ります。UnrealではActor内のSceneComponent attachment hierarchyがあり、root componentがActor Transformの基準です。Actor同士のattachmentも可能です。

Scale、socket attachment、absolute/relative Transform、physics attachmentの規則を別々に確認します。

## 10. PrefabとBlueprint Class

どちらも再利用可能なobject templateに使えますが:

- PrefabはGameObject/Component階層asset。
- Blueprint ClassはUClassを生成するvisual scripting/class asset。
- Blueprintはlogic、function、event graphを持てる。
- Prefab VariantとBlueprint inheritanceのoverride規則は同一でない。

「Prefab＝Blueprint」と覚えるのは入口としては有効でも、class/instance/serialization差を学ぶ必要があります。

## 11. SceneとWorld/Level

UnityはSceneをload/additive loadし、GameObject集合を管理します。UnrealはWorldの中にPersistent Levelやstreaming level/world partition等を持ちます。

| 目的 | Unity | Unreal |
|---|---|---|
| 基本map | Scene | Level/Map |
| 追加load | Additive Scene | Level Streaming/World Partition |
| 永続system | DontDestroyOnLoad等 | GameInstance/Subsystem等 |
| Scene object検索 | Scene API | World/Actor API |

## 12. Lifecycle

Unity:

```text
Awake → OnEnable → Start → Update/FixedUpdate/LateUpdate → OnDisable → OnDestroy
```

Unreal:

```text
Construction/Component registration
→ BeginPlay
→ Tick
→ EndPlay
→ destruction/GC
```

Editor construction、spawn、component initialization、network spawnで詳細順序が変わります。名前の似たcallbackを一対一対応させません。

## 13. Tick/Update

Unityは各MonoBehaviourのUpdate群、PlayerLoop、Script Execution Orderがあります。UnrealはActor/Component Tick、Tick Group、prerequisiteを持ちます。

共通原則:

- 毎frame不要なobjectを登録しない。
- 時間軸を分ける。
- 更新順を暗黙にしない。
- manager一個へ全処理を集めるか分散tickかを計測する。

## 14. MemoryとGC

### Unity

- C# managed heapをGC。
- UnityEngine.Object native側は`Destroy`。
- NativeContainer等は明示Dispose。

### Unreal

- UObject reference graphをGC。
- 非UObject C++ objectはRAII、smart pointer、明示所有。
- Actorは`Destroy`後にworldから終了し、UObject memory回収は後。

Unrealで全て`new/delete`、Unityで全てGC任せ、どちらも誤りです。

## 15. 参照

Unity:

- serialized UnityEngine.Object reference。
- managed strong/weak reference。
- Addressables handle。

Unreal:

- `UPROPERTY` object pointer/TObjectPtr。
- weak object pointer。
- soft object/class pointer。
- shared/weak pointer（非UObject）。

参照型の選択は寿命、load、GC、editor serializationを変えます。

## 16. Data asset

UnityのScriptableObjectとUnrealのData Assetはいずれもproject assetとしてdataを共有できます。

```csharp
[CreateAssetMenu]
public sealed class AttackDefinition : ScriptableObject
{
    public float Damage;
    public float Duration;
}
```

```cpp
UCLASS()
class UAttackData : public UPrimaryDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere)
    float Damage = 10.0f;
};
```

runtime mutable stateを共有assetへ入れない原則は共通です。

## 17. Primary Asset

Unrealの`UPrimaryDataAsset`はPrimary Asset IDやAsset Bundleを通じAsset Manager管理へ向きます。UnityのScriptableObjectそのものにはAddressables address/group/labelの意味はなく、Addressables側でassetを登録します。

Data containerとruntime loading identityを分けて理解します。

## 18. InspectorとDetails Panel

Unity Inspectorはserialized field、CustomEditor、PropertyDrawer等で拡張します。Unreal Details PanelはUPROPERTY metadata、Details customization、Slate等で拡張します。

Editor表示だけを整えてruntime validationを忘れない点は共通です。

## 19. Serialization

Unity serializationはfield、対応型、asset/Scene reference、domain reload等の規則を持ちます。Unreal serializationはUProperty metadata、CDO/default、package/cook、SaveGame等と関係します。

constructor初期値を変更しても既存serialized instanceへ反映されない問題は両方で起こり得ます。

## 20. Gameplay Framework

UnityにはUnrealのGameplay Frameworkに完全対応する標準class階層がありません。必要に応じ独自にPlayer、Game Rule、Session stateを組みます。

Unrealには:

- GameMode。
- GameState。
- PlayerState。
- PlayerController。
- Pawn/Character。
- AIController。

があり、特にnetwork authority/lifetimeを含む役割が標準化されています。

## 21. PlayerとController

UnityではInput component、player character、cameraを独自構成することが多いです。UnrealはControllerがPawnをPossessし、「意思決定主体」と「world内身体」を分けます。

Unityへ同じ分離を持ち込むなら:

```text
Input/AI Command Source
→ Character Intent
→ Character Motor/Combat
```

とinterface化できます。Unreal class名だけを模倣する必要はありません。

## 22. Character movement

UnityではCharacterController、Rigidbody、独自motorを選びます。UnrealのACharacterはCapsule、Skeletal Mesh、CharacterMovementComponentを標準構成し、walking/falling等やnetwork movement機能を持ちます。

高速actionの特殊移動では、どちらも標準移動のauthority、Root Motion、collision、network predictionとの統合が課題です。

## 23. Input

| Unity Input System | Unreal Enhanced Input |
|---|---|
| Input Action | Input Action |
| Action Map | Mapping Contextに近い |
| Binding | Key Mapping |
| Interaction | Triggerに近い |
| Processor | Modifierに近い |

名称が似ていますがAPIと評価pipelineは別です。最終的にengine inputをgame commandへ変換する設計は共通化できます。

## 24. Animation

Unity:

- Animator Controller。
- State Machine/Blend Tree。
- Avatar/Layer/Mask。
- Animation Event。
- Root Motion。

Unreal:

- Animation Blueprint/AnimInstance。
- State Machine/Blend Space。
- Montage/Slot。
- Anim Notify。
- Root Motion/Motion Warping。

Unrealではlocomotion graphとaction Montageを分ける構成が一般的です。UnityでもBase locomotionとaction layer/playableを分離できます。

## 25. Animation EventとAnim Notify

どちらもanimation timelineから通知できます。足音・VFXには便利ですが、damage authority全体をclip通知へ依存させるとtest・network・速度変更が難しくなります。

gameplay action timelineを真実にし、Notify/Eventは表示同期または明示event adapterとして使う原則は共通です。

## 26. Root Motion

Unityの`OnAnimatorMove`/Animator deltaと、UnrealのMontage Root Motion/CharacterMovement統合ではAPIが異なります。共通課題:

- collisionへどう適用するか。
- network authority。
- targetへの距離補正。
- animationとsimulation位置のずれ。
- cancel時の残りmotion。

## 27. Motion Warping

UnrealにはMotion Warping plugin/frameworkがあります。UnityではAnimation Rigging、Playables、root motion補正、独自warping等を組み合わせる場合があります。

Unityに同名標準機能がないから実現不能ではなく、必要責任を分解して選びます。

## 28. Physics

Unity 3DはPhysXを中心にCollider/Rigidbody/Physics queryを提供します。Unreal 5はChaos Physicsを中心にCollision Channel、Object Type、Trace等を提供します。

| Unity | Unreal |
|---|---|
| Layer Collision Matrix | Collision Profile/Channel Response |
| Raycast | Line Trace |
| SphereCast | Sphere Sweep |
| OverlapSphere | Sphere Overlap |
| Trigger | Overlap response/event |

filter設定、continuous detection、fixed step、threadingは同じ値ではありません。

## 29. Hit判定

UnityのPhysics QueryとUnrealのTrace/Sweepはいずれも高速武器の軌道判定に使えます。

共通構造:

```text
Action active window
→ previous/current socket間をsweep
→ Layer/Channel filter
→ Hurtbox/Hit Interface
→ 同一attack重複除外
→ Damage Resolver
```

Engine APIより上のdomain ruleはほぼ同じ設計で説明できます。

## 30. Camera

UnityはCameraとCinemachine packageを組み合わせることが多く、UnrealはCameraComponent、SpringArm、PlayerCameraManager、Camera Modifier等を使います。

follow、aim、collision、lock-on、shake、blendの責任を分ける原則は共通です。Cinemachine Virtual CameraとUnreal Camera Actorを完全同一とは扱いません。

## 31. Navigation

UnityはAI Navigation/NavMeshSurface/NavMeshAgent中心です。UnrealはNavigation System、NavMesh、AIController、Path Following、EQS等を組み合わせます。

UnityのNavMeshAgentは移動simulationまで持つのに対し、UnrealではController/Pawn/Movement Componentとの協調が強い点を意識します。

## 32. AI

| Unityでの一般構成 | Unreal標準 |
|---|---|
| 独自Perception | AI Perception |
| 独自Blackboard | Blackboard |
| 独自/外部Behavior Tree | Behavior Tree |
| 独自空間query | EQS |
| NavMeshAgent | AIController＋Path Following |

Unityでもpackage/assetを導入できますが、built-in architectureとしての統合度が違います。

## 33. Ability/Combat framework

UnrealのGameplay Ability SystemはAbility、Attribute、Gameplay Effect、Gameplay Tag、Cue、Task、network prediction等を統合します。

UnityにはGASと同一の標準frameworkはありません。第29章で作った:

- Action Definition/Runner。
- Damage/Attribute。
- Tag/State。
- Cost/Cooldown。
- Presentation event。
- Network prediction adapter。

をproject要件に合わせ統合するか、第三者solutionを評価します。

## 34. Gameplay Tag

Unreal Gameplay Tagは階層tagとqueryをEngine/GAS全体で利用できます。Unityではenum、bit flag、ScriptableObject ID、string/hash、独自tag database等を選びます。

Unity built-in GameObject Tagは一object一tag等の制約があり、GAS Gameplay Tagの直接代替ではありません。

## 35. UI

Unity runtime UIはuGUI/UI Toolkit、UnrealはUMG（内部Slate）を主に使います。

- Canvas/layout invalidation。
- input focus/navigation。
- resolution/safe area。
- event-driven state更新。
- world-space UI。
- localization。

などの課題は共通ですが、retained/immediate、widget lifecycle、binding costを別々にprofileします。

## 36. VFX

Unity Particle System/VFX GraphとUnreal NiagaraはCPU/GPU particle、data interface、event等を提供します。

gameplay logicをVFX simulation結果に依存させず、authoritative hit resultからVFX commandを発行する原則は共通です。

## 37. Audio

UnityはAudioSource、AudioMixer等、UnrealはAudio Component、Sound Cue/MetaSounds、Submix等を使います。

voice limit、priority、pool、spatialization、platform codec、重要eventの可聴性という設計課題は共通です。

## 38. Asset loading

Unity:

- direct serialized reference。
- Resources（用途注意）。
- Addressables/AssetBundle。
- AsyncOperationHandle。

Unreal:

- hard object/class reference。
- soft reference。
- Streamable Manager。
- Asset Manager/Primary Asset。

hard reference graphが意図せず巨大loadを起こす問題、handle/ownershipを明示する必要は共通です。

## 39. AddressablesとAsset Manager

| 観点 | Unity Addressables | Unreal Asset Manager |
|---|---|---|
| identity | address/GUID等 | Primary Asset ID |
| grouping | Group/Label | Primary Asset Type/Rule/Bundle |
| indirect ref | AssetReference | Soft Object/Class Reference |
| async result | AsyncOperationHandle | Streamable Handle等 |
| build | content build/catalog/bundle | cook/chunk/package |

用語を直訳せず、load、reference count、release、patch unitを対応させます。

## 40. Networking

UnityではNetcode for GameObjects/Entities等を選択しarchitectureを構成します。UnrealはActor Replication、Property Replication、RPC、Role/Authority等をEngineへ統合しています。

共通原則:

- server authority。
- ownershipとconnection。
- persistent stateとevent。
- client prediction/reconciliation。
- relevancy/interest management。
- validation。

Unrealの`Replicated`を付けるだけでもgame-specific predictionは完成しません。

## 41. Multiplayer action

Unreal GASはability replication/predictionを支援します。UnityではNetcode solutionとcombat domainを統合して独自予測を構成する場合があります。

どちらでもinstant damageのauthority、hit compensation、animation開始tick、prediction rejection cleanupを明示します。

## 42. Editor tool

Unity:

- EditorWindow/UI Toolkit。
- CustomEditor/PropertyDrawer。
- AssetPostprocessor。
- MenuItem。

Unreal:

- Editor Module。
- Slate/Editor Utility Widget。
- Detail customization。
- Asset Action/Data Validation/Commandlet。

runtime moduleからeditor dependencyを逆参照しない原則は同じです。

## 43. Build pipeline

Unity:

```text
C# compile → asset import/build → player build → IL2CPP等 → package
```

Unreal:

```text
UBT/UHT compile → cook → stage → package → archive
```

UnityのAssetBundle buildとUnreal cook/chunk、Development/ReleaseとDevelopment/Shippingを雑に同一視せず、content変換とcode buildを分けて理解します。

## 44. Platform abstraction

Unityはasmdef、interface、platform define、native pluginで分離します。Unrealはmodule、platform abstraction、Target.cs/Build.cs、preprocessor、platform moduleで分離します。

domainへ`#if UNITY_ANDROID`や`#if PLATFORM_WINDOWS`を散らさずadapter境界へ閉じ込めます。

## 45. Testing

UnityはTest FrameworkのEdit/Play Mode、UnrealはAutomation Test、Functional Test等を使います。

純粋rule:

- Unity: UnityEngine非依存C# assembly。
- Unreal: UObject/World非依存standard C++ module/class。

へ分けると高速testが可能です。Engine object integrationは別test layerへ置きます。

## 46. Profiling

| Unity | Unreal |
|---|---|
| Unity Profiler | Unreal Insights |
| Profile Analyzer | Insights session比較/分析 |
| Memory Profiler | Memory Insights/LLM等 |
| Frame Debugger | RenderDoc/GPU Visualizer等 |
| ProfilerMarker | TRACE/QUICK_SCOPE系等 |

tool名は違っても、target build、worst-case capture、CPU/GPU/Memory分類、変更後再計測の手順は同じです。

## 47. Rendering philosophy

Unityはrender pipelineをURP/HDRP等から選び、package/settings/shaderを構成します。Unrealはdeferred/forward設定に加えNanite、Lumen、Virtual Shadow Maps等をEngine ecosystem内で提供します。

feature名で優劣を決めず、target hardware、art direction、team skill、frame/memory budget、platform対応で選びます。

## 48. Source access

UnrealはEngine sourceへアクセスし、実装を追跡・build・修正できるworkflowがあります。Unityは公開package sourceやreference source、native側black boxの範囲が異なります。

Source accessは強力ですが、Engine forkはmerge/upgrade costを生みます。Unityでもpackage forkは同様の保守責任を持ちます。

## 49. Version control

両方ともtext sourceに加えbinary assetを多く扱います。

- Unity `.meta`を必ず管理。
- Unreal `.uasset/.umap`のbinary merge制約。
- GUID/asset path/reference。
- Git LFS等。
- lock運用。
- generated/cache directory除外。

Engine変更よりasset conflictがteam productivityを支配することがあります。

## 50. Iteration

Unity C# compile/domain reload、Unreal C++ compile/Live Coding、Blueprint compileにはそれぞれ異なる待ちと制約があります。

- module/assembly分割。
- data-driven tuning。
- hot reloadへ依存し過ぎない。
- editor再起動/clean build条件。
- automated content validation。

を整備します。

## 51. 同じCombat architectureを移植する

Engine非依存概念:

```text
Input Command
→ Combat State
→ Action Definition/Instance
→ Window/Cancel
→ Hit Query Adapter
→ Damage Resolver
→ Presentation Events
```

Unity adapter:

```text
Input System / Animator / Physics / Cinemachine / VFX Graph
```

Unreal adapter:

```text
Enhanced Input / Anim Montage / Trace / Camera Manager / Niagara
```

まずdomain vocabularyを保ち、Engine APIを外周へ置きます。

## 52. UnityからUnrealへ移る際の罠

- 全classをActorにする。
- UObjectを通常C++ pointerとして寿命管理する。
- BlueprintをPrefabだけと考える。
- GameModeへclient表示stateも置く。
- Tickへ全logicを書く。
- hard referenceでasset load graphを巨大化。
- GASを理解せず単純skill classとして使う。
- UHT macro/generated codeを標準C++と混同する。

## 53. UnrealからUnityへ移る際の罠

- UnityにGameMode/PlayerState標準classがあると仮定する。
- GameObject TagをGameplay Tag代替にする。
- ScriptableObjectへruntime mutable stateを置く。
- `Destroy`せずmanaged参照をnullにするだけ。
- CharacterControllerにnetwork predictionまで期待する。
- AnimatorをAnimation Blueprintと完全同一視する。
- Resourcesへ全assetを置く。

## 54. Engine選択の観点

- target platform。
- visual/scale requirement。
- teamのC#/C++/Blueprint skill。
- tool customization。
- multiplayer requirement。
- package/plugin dependency。
- source access requirement。
- build/iteration time。
- licensing/terms。
- 採用・教育・保守体制。

料金・license・platform supportは変わるため、決定時に最新公式情報を確認します。

## 55. 学習の相乗効果

Unityで学んだComponent、data-driven、GC、profilingはUnrealでも役立ちます。Unrealで学んだGameplay Framework、authority、native memory、module境界はUnityの独自architecture改善に役立ちます。

Engine固有APIより:

- ownership。
- lifetime。
- dependency direction。
- time axis。
- authority。
- data flow。
- measurement。

を説明できることが移植可能な知識です。

## 56. 比較演習

次の機能を両Engineで図にしてください。

1. Player spawnから入力可能まで。
2. 一回の近接攻撃とdamage。
3. asset非同期loadと解放。
4. 敵AIが発見して接近するまで。
5. network上で回避と命中を判定するまで。

class名だけでなく、authority、寿命、event、data ownerを書きます。

## 57. 完成確認表

- [ ] GameObjectとActorを完全同一視していない。
- [ ] ActorComponentとSceneComponentの違いを説明できる。
- [ ] PrefabとBlueprint Classの違いを説明できる。
- [ ] Managed GC、Unity native object、UObject GC、RAIIを区別できる。
- [ ] ScriptableObject/Data AssetとAddressables/Asset Managerを分けている。
- [ ] UnityにUnreal Gameplay Framework相当を自分で設計する必要を理解した。
- [ ] Input/Animation/Physicsの対応用語を説明できる。
- [ ] GASとUnity独自combat frameworkの範囲差を説明できる。
- [ ] network authorityの原則を両Engineへ適用できる。
- [ ] domainとEngine adapterを分離できる。
- [ ] build、test、profilingの対応関係を説明できる。
- [ ] Engine選択をfeature名だけで判断していない。

## 58. 確認問題

1. なぜMonoBehaviourとActorは一対一対応ではないのか。
2. Unity ComponentとUnreal SceneComponentのTransform差は何か。
3. PrefabとBlueprint Classの重要な違いは何か。
4. UnityとUnrealのobject寿命を比較してください。
5. ScriptableObjectとPrimary Data Assetの範囲差は何か。
6. UnityのGameObject TagがGameplay Tagの直接代替でない理由は何か。
7. NavMeshAgentとAIController＋Path Followingの責任差は何か。
8. GASがUnityの単一標準classに対応しない理由は何か。
9. AddressablesとAsset Managerで共通して管理すべき問題は何か。
10. Combat domainを両Engineへ移植可能にする境界は何か。

## 59. 関連ノート

- [Unreal Engine編・全49章](../05_unreal_engine/README.md)
- [Unity Assembly Definition・依存設計](28_assembly_definition_dependency_design.md)
- [Unity 3Dアクション戦闘](29_3d_action_combat.md)
- [Unity Multiplayer基礎](31_multiplayer_fundamentals.md)
- [Unity Platform最適化](32_mobile_console_pc_optimization.md)

## 60. 公式資料

- [Unity 6 GameObject](https://docs.unity3d.com/6000.0/Documentation/Manual/GameObjects.html)
- [Unity 6 ScriptableObject](https://docs.unity3d.com/6000.0/Documentation/Manual/class-ScriptableObject.html)
- [Unreal Engine Game Objects for Unity developers](https://dev.epicgames.com/documentation/en-us/unreal-engine/game-objects-in-unreal-engine)
- [Writing Unreal Engine code for Unity developers](https://dev.epicgames.com/documentation/en-us/unreal-engine/writing-code-in-unreal-engine-for-unity-developers)
- [Unreal Engine Data Assets](https://dev.epicgames.com/documentation/en-us/unreal-engine/data-assets-in-unreal-engine)
- [Unreal Engine Gameplay Ability System](https://dev.epicgames.com/documentation/en-us/unreal-engine/gameplay-ability-system-for-unreal-engine)

EngineとpackageのAPI、license、platform対応は更新されるため、利用versionの公式資料を確認してください。
