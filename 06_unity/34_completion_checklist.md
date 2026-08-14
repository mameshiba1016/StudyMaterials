# Unity編・完成確認表

> この章は暗記試験ではありません。各項目を「説明できる」「小さく実装できる」「不具合時に原因を切り分けられる」の3段階で確認します。

## 1. 判定level

| Level | 判定 | 意味 |
|---:|---|---|
| 0 | 未確認 | 用語をまだ説明できない |
| 1 | 説明 | 自分の言葉で目的・仕組み・注意点を説明できる |
| 2 | 実装 | 資料を参照しながら最小例を作れる |
| 3 | 診断 | 内部処理を根拠にbug・性能問題を切り分けられる |

Level 1を飛ばしてcodeだけ写すと、条件が変わったとき直せません。Level 3は全API暗記ではなく、観測方法と調査経路を持つ状態です。

## 2. 確認方法

各章で次を行います。

1. ノートを閉じて概念図を書く。
2. 主要用語を他の用語と比較する。
3. 小さいScene/純粋C# testで実装する。
4. 意図的に一箇所壊し、Profiler/Debugger/Logで原因を探す。
5. Unity/Package versionを記録し公式資料でAPIを確認する。

「動いた」はLevel 2の途中です。なぜ動くか、いつ壊れるかまで確認します。

## 3. C#とUnity runtime

関連: [第1章](01_csharp_cpp_unity_overview.md)、[第5章](05_managed_memory_gc.md)

- [ ] C# source、assembly、IL、runtime実行の流れを説明できる。
- [ ] C++の値semantics/RAIIとC# managed reference/GCを比較できる。
- [ ] class、struct、record、interfaceの用途を選べる。
- [ ] 値渡し、参照共有、boxingを説明できる。
- [ ] nullable referenceとUnityEngine.Objectのnullを混同しない。
- [ ] delegate、event、lambdaの参照寿命を説明できる。
- [ ] closureがallocationや予期しない変数捕捉を起こす例を作れる。
- [ ] exceptionを通常の毎frame分岐として使わない理由を説明できる。

診断課題: 毎frame allocationするLINQ/closureをProfilerで発見し、挙動を変えず修正してください。

## 4. Project・Asset・Package・Assembly

関連: [第2章](02_project_assets_assemblies.md)、[第28章](28_assembly_definition_dependency_design.md)

- [ ] `Assets`、`Packages`、`ProjectSettings`、`Library`の役割を説明できる。
- [ ] `.meta`とGUIDをversion controlへ含める理由を説明できる。
- [ ] Package Manager dependencyとpackage versionを確認できる。
- [ ] assemblyとnamespaceとfolderの違いを説明できる。
- [ ] asmdefのreference方向を図にできる。
- [ ] Runtime、Editor、Tests、platform固有assemblyを分離できる。
- [ ] 循環依存をinterface/event/composition rootで解消できる。
- [ ] compile時間とarchitectureの両面からasmdef粒度を判断できる。

診断課題: Editor namespaceをruntime assemblyから参照してbuildが失敗する例を作り、依存境界で直してください。

## 5. GameObject・Component・Transform

関連: [第3章](03_gameobject_component_prefab.md)

- [ ] GameObjectがComponent containerであることを説明できる。
- [ ] Component compositionと継承の使い分けを説明できる。
- [ ] world/local position、rotation、scaleを変換できる。
- [ ] parent変更時にworld poseを維持するか選べる。
- [ ] negative/non-uniform scaleのcollision/rotationへの影響を理解する。
- [ ] `GetComponent`の成功・失敗を安全に扱える。
- [ ] tag/name検索にsystem architectureを依存させない。
- [ ] Scene object、Prefab asset、runtime instanceを区別できる。

診断課題: parent scaleによって子の武器hitboxが歪む原因をTransform行列から説明してください。

## 6. MonoBehaviour lifecycle

関連: [第4章](04_monobehaviour_lifecycle.md)、[第10章](10_time_fixedupdate_coroutine_async.md)

- [ ] Awake、OnEnable、Start、Update、OnDisable、OnDestroyの役割を説明できる。
- [ ] object間callback順を暗黙に仮定しない設計ができる。
- [ ] enabled、activeSelf、activeInHierarchyの差を説明できる。
- [ ] event購読を適切なlifecycleで解除できる。
- [ ] Update、FixedUpdate、LateUpdateへ処理を割り振れる。
- [ ] Script Execution Orderをarchitectureの代用品にしない。
- [ ] domain reload設定差でstatic stateが残る問題を理解する。
- [ ] application quit中のcleanupへ過剰依存しない。

診断課題: pool再利用後にevent callbackが二重発火する問題を購読lifecycleから直してください。

## 7. Managed Memory・Unity Object・Native Resource

関連: [第5章](05_managed_memory_gc.md)、[第6章](06_unity_object_serialization.md)

- [ ] stack、managed heap、native memoryの違いを説明できる。
- [ ] managed referenceがなくなることとUnity objectの`Destroy`を区別できる。
- [ ] destroyed UnityEngine.Objectの特殊なnull比較を説明できる。
- [ ] `Destroy`が即時memory解放と同義でないことを理解する。
- [ ] NativeArray等をDisposeする責任を説明できる。
- [ ] static/event/collectionがobjectを保持するmemory leakを診断できる。
- [ ] Memory Profiler snapshotからroot referenceを追える。
- [ ] finalizerへresource解放timingを依存させない。

診断課題: Sceneを戻るたびmemoryが増える例で、managed/native/assetのどこに残っているか切り分けてください。

## 8. Serialization

関連: [第6章](06_unity_object_serialization.md)、[第17章](17_scriptableobject_data_driven_design.md)

- [ ] Unityがserializeするfieldとしないpropertyを説明できる。
- [ ] `[SerializeField]`がaccess modifierを変えないことを理解する。
- [ ] constructor初期値変更が既存assetへ反映されない理由を説明できる。
- [ ] Scene/Prefab/ScriptableObject referenceのGUID/file ID概念を理解する。
- [ ] type/field rename時のmigrationを設計できる。
- [ ] Inspector dataを信用する前にvalidationできる。
- [ ] runtime stateをassetへ誤保存しない。
- [ ] `SerializeReference`等が必要な場合のtrade-offを調査できる。

診断課題: script rename後のMissing Scriptまたはfield data消失を再現し、安全な移行手順を作ってください。

## 9. Scene・Prefab

関連: [第7章](07_scene_management.md)、[第8章](08_prefab_variants.md)

- [ ] Single/Additive Scene loadの用途を説明できる。
- [ ] persistent systemを重複生成しないbootstrapを作れる。
- [ ] async load、activation、依存asset準備を別状態として扱える。
- [ ] Scene unload前に跨ぎ参照をcleanupできる。
- [ ] Prefab、Nested Prefab、Variant、instance overrideを区別できる。
- [ ] Apply/Revertが意図しないinstanceへ影響する条件を理解する。
- [ ] Prefab assetへruntime stateを戻さない。
- [ ] Scene transition失敗・cancel・二重要求を扱える。

診断課題: titleとgame Sceneを往復してManagerが二重になる問題を所有者とlifecycleから直してください。

## 10. Input System

関連: [第9章](09_new_input_system.md)

- [ ] Action、Action Map、Binding、Interaction、Processorを区別できる。
- [ ] device inputをgame commandへ変換できる。
- [ ] callbackとpollingの使い分けを説明できる。
- [ ] performed/canceledの意味をAction typeとInteractionから説明できる。
- [ ] input bufferへ時刻・command・期限を保存できる。
- [ ] gameplay/UIのAction Mapを安全に切り替えられる。
- [ ] rebindingとbinding overrideを保存できる。
- [ ] keyboard/gamepad/touchのdevice差をdomainから分離できる。

診断課題: 低fps時に短いbutton入力を取り逃す例を、callbackとbufferで直してください。

## 11. Time・Coroutine・async

関連: [第10章](10_time_fixedupdate_coroutine_async.md)

- [ ] scaled/unscaled timeを用途別に選べる。
- [ ] deltaTimeを二重適用しない。
- [ ] FixedUpdateが描画frameと一対一でないことを説明できる。
- [ ] Coroutineがthreadでないことを説明できる。
- [ ] yield instructionが待つ時間軸を説明できる。
- [ ] async/awaitのcontinuationとUnity Main Thread制約を理解する。
- [ ] object破棄/pool返却時に非同期処理をcancelできる。
- [ ] hit stop、pause、network clockを別時間軸として設計できる。

診断課題: pause中に止まるべきでないUI animationとtimeoutを適切なclockへ移してください。

## 12. Physics

関連: [第11章](11_physics_2d_3d_layers.md)

- [ ] 2D/3D Physics APIを混在させない。
- [ ] Collider、Rigidbody、Trigger、Layer Matrixを説明できる。
- [ ] collision callbackとqueryを用途別に選べる。
- [ ] Raycast、Shape Cast、Overlapの差を説明できる。
- [ ] LayerMaskとQueryTriggerInteractionを明示できる。
- [ ] 高速objectのtunnelingとcontinuous/sweep対策を説明できる。
- [ ] NonAlloc query buffer溢れを検出できる。
- [ ] Physics処理をProfilerで計測できる。

診断課題: 低fps時だけ剣が敵をすり抜ける問題をprevious/current pose間sweepで直してください。

## 13. Character Motor

関連: [第12章](12_character_controller_motor.md)

- [ ] CharacterController、Rigidbody、独自motorを比較できる。
- [ ] input方向をcamera-relative world方向へ変換できる。
- [ ] ground detection、slope、step、gravityを説明できる。
- [ ] movement requestと実Transform更新を分離できる。
- [ ] attack、dodge、knockbackの移動priorityを決められる。
- [ ] rotation authorityを一箇所へ集約できる。
- [ ] collision resolutionとvisual smoothingを分離できる。
- [ ] frame rate差の移動testを作れる。

診断課題: locomotionとattack root motionが同時にpositionを書いて震える問題をauthorityから解決してください。

## 14. Animation

関連: [第13章](13_animator_animation_root_motion.md)

- [ ] Animator Controller、State、Transition、Blend Treeを説明できる。
- [ ] parameter hashを利用できる。
- [ ] Layer/Avatar Maskを用途別に設定できる。
- [ ] normalized timeとsecondsの違いを説明できる。
- [ ] Animation Eventを表示通知とgameplay authorityで使い分ける。
- [ ] Root Motionとcode motionを比較できる。
- [ ] blend中の二重event/状態不整合を診断できる。
- [ ] Animator cullingでgameplayが変化しない設計ができる。

診断課題: transition blend中にattack hitboxが二重に有効化される原因を追跡してください。

## 15. Camera・Lock-on

関連: [第14章](14_cinemachine_camera_lockon.md)

- [ ] gameplay camera rig、view、aim、collisionの責任を分けられる。
- [ ] camera updateをtarget移動後に行える。
- [ ] lock-on候補を距離・screen角・遮蔽でscore化できる。
- [ ] current target bonusでちらつきを防げる。
- [ ] target切替をscreen spaceで判断できる。
- [ ] camera collisionと壁貫通をsphere cast等で扱える。
- [ ] shake/impulseをdamage domainからeventで分離できる。
- [ ] package versionに合うCinemachine APIを確認できる。

診断課題: 狭い場所でcameraが振動する問題をcollision hitとdampingの順序から分析してください。

## 16. Rendering・Shader・Lighting

関連: [第15章](15_render_pipeline_shader_graph_material.md)、[第16章](16_lighting_shadow_post_processing.md)

- [ ] Built-in/URP/HDRPの選択がproject全体へ与える影響を説明できる。
- [ ] material、shader、pass、variant、keywordを区別できる。
- [ ] opaque/transparent、depth、blendの基本を説明できる。
- [ ] SRP Batcher、Instancing、batchingを比較できる。
- [ ] realtime/baked/mixed lightingを選べる。
- [ ] Light Probe/Reflection Probe/lightmapを説明できる。
- [ ] shadow distance/cascade/resolutionのcostを説明できる。
- [ ] Frame Debugger/GPU Profilerでdrawを追跡できる。

診断課題: 同じmaterialに見えるobjectがbatchされない理由をshader variantとmaterial instanceから調べてください。

## 17. ScriptableObject・Data-driven

関連: [第17章](17_scriptableobject_data_driven_design.md)

- [ ] definitionとruntime instanceを分けられる。
- [ ] ScriptableObjectを共有dataとして利用できる。
- [ ] stable IDとasset参照の違いを説明できる。
- [ ] OnValidate/custom validationで不正dataを検出できる。
- [ ] data versionとmigrationを設計できる。
- [ ] asset間循環referenceを避けられる。
- [ ] immutableなdomain inputへ変換できる。
- [ ] runtime buildでassetを書き換えてsaveできると誤認しない。

診断課題: 全敵が同じcooldown状態を共有するbugを、definition/runtime分離で直してください。

## 18. UI

関連: [第18章](18_ui_toolkit_ugui_text_hud.md)

- [ ] uGUIとUI Toolkitの用途・制約を比較できる。
- [ ] UIをgame stateの所有者にしない。
- [ ] event-drivenにHP/HUDを更新できる。
- [ ] Canvas rebuild/layout costを診断できる。
- [ ] input focus/navigation/device切替を扱える。
- [ ] resolution、safe area、aspect ratioへ対応できる。
- [ ] string allocationとText更新頻度を制御できる。
- [ ] world-space indicatorのoff-screen/occlusionを扱える。

診断課題: HPが変わっていないのに毎frameUIを再構築する処理をevent駆動へ変更してください。

## 19. Audio・Particle・VFX

関連: [第19章](19_audio_particle_vfx_graph.md)

- [ ] AudioSource、AudioClip、AudioMixerの役割を説明できる。
- [ ] clip load typeを用途で選べる。
- [ ] voice priority/limitを設計できる。
- [ ] Particle SystemとVFX Graphを比較できる。
- [ ] transparent overdrawと最大particle数を計測できる。
- [ ] hit resultからpresentation eventを発行できる。
- [ ] pooled effectを完全resetできる。
- [ ] gameplay判定をVFX結果へ依存させない。

診断課題: 連続hit時に重要なparry音が消える問題をvoice priorityから直してください。

## 20. Addressables・Async Load

関連: [第20章](20_addressables_assetbundle_async_loading.md)

- [ ] direct reference、Resources、Addressablesを比較できる。
- [ ] address、GUID、label、group、catalog、bundleを区別できる。
- [ ] dependencyを含むload memoryを説明できる。
- [ ] AsyncOperationHandleの所有者とrelease時点を決められる。
- [ ] instantiateとasset loadのcostを分けて計測できる。
- [ ] duplicate dependencyをBuild Layout等で調べられる。
- [ ] remote catalog/content updateのversion整合性を考えられる。
- [ ] Scene transition中の旧新asset同時保持peakを測れる。

診断課題: Addressablesをreleaseしてもmemoryが減らない理由を、別handle/reference/native memoryから調査してください。

## 21. Save・Settings・Localization

関連: [第21章](21_save_settings_localization.md)

- [ ] save schemaとruntime object graphを分けられる。
- [ ] version、migration、defaultを設計できる。
- [ ] temporary fileとatomic replaceで破損耐性を作れる。
- [ ] save失敗・容量不足をuserへ安全に通知できる。
- [ ] settingsを適用前/確定後へ分けられる。
- [ ] display/input/audio設定のplatform差を扱える。
- [ ] textをcodeから分離しlocale切替できる。
- [ ] plural、font fallback、RTL等を調査できる。

診断課題: 旧version saveを読み込んでも失敗せずdefault補完するmigration testを作ってください。

## 22. Pool・Allocation

関連: [第22章](22_object_pool_gc_allocation.md)

- [ ] poolが生成costとGCを減らす一方、常駐memoryを増やすことを説明できる。
- [ ] capacityをpeak同時数から決められる。
- [ ] rent/returnの二重実行を検出できる。
- [ ] event、Coroutine、target、Transform、Physicsをresetできる。
- [ ] pool ownerとScene unloadの関係を決められる。
- [ ] collection/array/StringBuilder等を適切に再利用できる。
- [ ] poolingしない方がよい低頻度objectを判断できる。
- [ ] GC allocation改善をProfilerで実証できる。

診断課題: pooled projectileが以前のownerへdamageを帰属するbugをreset contractで防いでください。

## 23. Profiling

関連: [第23章](23_profiler_frame_debugger_memory_profiler.md)、[第32章](32_mobile_console_pc_optimization.md)

- [ ] target device上のDevelopment Buildを計測できる。
- [ ] CPU/GPU/Memory/I/O boundを分類できる。
- [ ] TimelineとHierarchyを使い分けられる。
- [ ] ProfilerMarkerを意味単位へ入れられる。
- [ ] averageと95/99 percentileを確認できる。
- [ ] Frame Debuggerでpass/draw順を追える。
- [ ] Memory snapshotを比較できる。
- [ ] 変更前後を同条件で再計測できる。

診断課題: 解像度を半分にしてもfpsが変わらないcaptureから、次に調べる場所を説明してください。

## 24. Job System・Burst

関連: [第24章](24_job_system_nativecontainer_burst.md)

- [ ] Jobをthreadそのものと混同しない。
- [ ] NativeContainerのownership、safety、Disposeを説明できる。
- [ ] dependency JobHandleを正しく接続できる。
- [ ] schedule直後のCompleteが並列性を失う理由を説明できる。
- [ ] Burst可能なdata-oriented計算を分離できる。
- [ ] managed object/Unity APIのMain Thread制約を理解する。
- [ ] race conditionをsafety errorだけに依存せず設計で防ぐ。
- [ ] job化overheadに見合うwork量を計測できる。

診断課題: Jobが速くならない例をTimelineで確認し、Complete位置またはbatch sizeを改善してください。

## 25. ECS・DOTS

関連: [第25章](25_ecs_dots_baking_system_component_data.md)

- [ ] Entity、Component Data、System、Archetype/Chunkを説明できる。
- [ ] GameObject OOPとのdata layout差を説明できる。
- [ ] Authoring/Bakerとruntime entityを区別できる。
- [ ] structural changeのcostを説明できる。
- [ ] queryとsystem update dependencyを理解する。
- [ ] Enableable component等で頻繁な状態変更を検討できる。
- [ ] Hybrid境界とpresentation同期を設計できる。
- [ ] 全systemをECS化せず大量data処理へ適用できる。

診断課題: 毎frame大量Entityをadd/removeしている処理をdata/state表現から見直してください。

## 26. Editor Tool

関連: [第26章](26_editor_extension_property_drawer_tools.md)

- [ ] runtime/editor assemblyを分離できる。
- [ ] CustomEditor、PropertyDrawer、EditorWindowを用途別に選べる。
- [ ] SerializedObject/SerializedPropertyを通してUndo/multi-editを守れる。
- [ ] AssetDatabase変更を適切にbatch化できる。
- [ ] toolでassetを変更する前にvalidation/previewできる。
- [ ] Undo、dirty、saveの責任を説明できる。
- [ ] domain logicをEditor UIから分離しtestできる。
- [ ] toolが大規模projectでも全asset scanを乱発しない。

診断課題: custom inspector変更がUndoできない問題をSerializedProperty経由へ直してください。

## 27. Test・CI・Build

関連: [第27章](27_test_framework_ci_build.md)

- [ ] Edit ModeとPlay Mode testを使い分けられる。
- [ ] Arrange/Act/Assertを明確に書ける。
- [ ] frame/time依存testを安定化できる。
- [ ] asset/Scene/prefab validation testを作れる。
- [ ] platform buildをCIから再現できる。
- [ ] secret/signing情報をrepositoryへ置かない。
- [ ] test result、log、build artifactを保存できる。
- [ ] flaky testを放置せず原因分類できる。

診断課題: machine速度で時々失敗する`WaitForSeconds` testを明示condition/tickへ変更してください。

## 28. Dependency Architecture

関連: [第28章](28_assembly_definition_dependency_design.md)

- [ ] Presentation → Application → Domainの依存方向を説明できる。
- [ ] DomainをMonoBehaviourから分離できる。
- [ ] interfaceを利用側に置く理由を説明できる。
- [ ] composition rootでimplementationを組み立てられる。
- [ ] eventによる循環依存隠しを見抜ける。
- [ ] shared kernelを巨大な雑多moduleにしない。
- [ ] internal APIとtest accessを設計できる。
- [ ] package/feature単位で再利用可能な境界を作れる。

診断課題: CombatがUIを直接参照しUIもCombatを参照する循環を、event/port境界で解消してください。

## 29. 3D Action Combat

関連: [第29章](29_3d_action_combat.md)

- [ ] Input→Command→State→Action→Hit→Damage→Presentationを図示できる。
- [ ] command bufferの寿命・優先度・消費規則を実装できる。
- [ ] 排他的stateと直交stateを分けられる。
- [ ] Action Definitionとruntime Action Instanceを分けられる。
- [ ] combo graph、cancel window、hit confirmを設計できる。
- [ ] hitbox/hurtbox/receiverと重複命中を扱える。
- [ ] dodge、invulnerable、super armor、guard、parryを区別できる。
- [ ] hit stop、lock-on、soft targetingをauthority衝突なく統合できる。

診断課題: 同じ攻撃が一体の複数Colliderへ3回damageを与える問題をreceiver単位のHitSessionで直してください。

## 30. Enemy AI・Boss

関連: [第30章](30_enemy_ai_navmesh_boss.md)

- [ ] Perception、Memory、Decision、Actionを分離できる。
- [ ] worldの真実とAIの最終既知情報を区別できる。
- [ ] FSM、Behavior Tree、Utility AIを用途別に選べる。
- [ ] NavMeshAgentのpathPending/status/remainingDistanceを正しく扱える。
- [ ] Complete/Partial/Invalid pathのfallbackを持つ。
- [ ] combat slotとattack tokenを実装できる。
- [ ] reaction delayとtelegraphで公平性を作れる。
- [ ] Boss phase閾値飛び越し、部位、arena、cleanupを設計できる。

診断課題: 壁越しでPlayerを追い続ける敵をPerception/Memory timeout/Search stateで修正してください。

## 31. Multiplayer

関連: [第31章](31_multiplayer_fundamentals.md)

- [ ] Host、Dedicated Server、Client、Authority、Ownershipを説明できる。
- [ ] Lobby/Relay/Sessionとgameplay replicationを分けられる。
- [ ] clientからstate結果でなく入力commandを送れる。
- [ ] network tick、sequence、snapshotを設計できる。
- [ ] Reliable/Unreliableをmessage性質で選べる。
- [ ] RPC eventとpersistent stateを区別しlate joinへ対応できる。
- [ ] remote interpolationとlocal prediction/reconciliationを説明できる。
- [ ] server-authoritative damage、lag compensation、validationを設計できる。

診断課題: 100ms latencyでlocal playerが重く、remote playerが瞬間移動する問題をpredictionとsnapshot bufferで分離して直してください。

## 32. Platform Optimization

関連: [第32章](32_mobile_console_pc_optimization.md)

- [ ] target fpsからframe budgetを計算できる。
- [ ] minimum/typical/high device matrixを作れる。
- [ ] CPU/GPU/Memory/I/Oの最大bottleneckから改善できる。
- [ ] texture/mesh/audio importをplatform別に設定できる。
- [ ] draw、overdraw、shadow、resolution、variantを計測できる。
- [ ] streaming、pool、LOD、AI更新をpeakに合わせられる。
- [ ] mobile thermal/battery/lifecycleを長時間testできる。
- [ ] console/PC固有のmemory、input、resolution、hardware幅を扱える。

診断課題: 起動直後60fps、10分後40fpsになるmobile buildをthermal/GPU/CPU timingから調査してください。

## 33. Unity・Unreal比較

関連: [第33章](33_unity_unreal_engine_comparison.md)

- [ ] GameObjectとActorの共通点・相違点を説明できる。
- [ ] ComponentとActorComponent/SceneComponentを比較できる。
- [ ] PrefabとBlueprint Classを完全同一視しない。
- [ ] Unity Object/managed GCとUObject GC/RAIIを比較できる。
- [ ] ScriptableObject/Data AssetとAddressables/Asset Managerを区別できる。
- [ ] Animator/Animation Blueprint、Physics/Chaos、AI frameworkを対応付けられる。
- [ ] Unity独自combat frameworkとUnreal GASの範囲差を説明できる。
- [ ] Engine非依存DomainとEngine adapterへ分けられる。

診断課題: 第29章の一攻撃をUnity APIなしのdomain図にし、Unity/Unreal両方のadapterを書き出してください。

## 34. 総合Architecture確認

次の依存を一方向にしてください。

```text
Unity Presentation / Adapters
  Input, Animator, Physics, Camera, UI, Audio, VFX, Network
                         ↓
Application
  command routing, use case, orchestration
                         ↓
Domain
  combat rule, damage, state, AI scoring, save schema rule
```

- [ ] DomainからUnityEngine APIを除く範囲を説明できる。
- [ ] runtime stateのownerが一つに決まっている。
- [ ] object lifetimeとevent購読が対になっている。
- [ ] Update/Fixed/Unscaled/Network tickを区別している。
- [ ] asset definitionとinstance stateを分けている。
- [ ] gameplay authorityとpresentationを分けている。
- [ ] Debug/Test/Profile hookを後付けでなく設計に含めている。
- [ ] optional package/APIをadapterへ閉じ込めている。

## 35. 総合実装課題A: 小さい戦闘arena

次を一つずつ追加します。

1. camera-relative移動とground motor。
2. lock-on候補検索。
3. 三段comboと入力buffer。
4. dodge、guard、parry。
5. 一体のFSM/Utility敵。
6. combat slotとattack tokenを持つ複数敵。
7. Boss phase一段階。
8. event-driven HUD、Audio、VFX。
9. Save/Settings。
10. target device profiling。

各段階でEdit/Play testとProfiler captureを残し、次機能を追加する前に不明なauthorityをなくします。

## 36. 総合実装課題B: Failure injection

意図的に次の問題を作り、診断します。

- missing asset/reference。
- invalid ScriptableObject data。
- command buffer期限境界。
- physics query buffer overflow。
- duplicate event subscription。
- Addressables release忘れ。
- pool reset漏れ。
- partial NavMesh path。
- 150ms latency/5% packet loss。
- memory budget超過。

例外が出る問題だけでなく、「動くが間違う」「時間後に悪化する」問題も扱います。

## 37. 説明試験

資料を見ず、各3分以内で説明してください。

1. C# objectとUnityEngine.Objectの寿命。
2. UpdateとFixedUpdate。
3. ScriptableObject definitionとruntime state。
4. Input bufferとcancel window。
5. Root Motionとmotor authority。
6. Hitbox/Hurtbox/Damage Resolver。
7. NavMeshと意思決定AI。
8. RPCとreplicated state。
9. Client predictionとreconciliation。
10. CPU boundとGPU bound。

説明中に「Unityがいい感じにやる」が出た部分は、次に内部処理を調べる対象です。

## 38. Code review確認

- [ ] class名が責任を表す。
- [ ] public APIが必要最小限。
- [ ] null/invalid inputの契約が明確。
- [ ] time unitがfield名で分かる。
- [ ] ownershipとDispose/Unsubscribeが対になる。
- [ ] magic numberがdefinition/configへ移っている。
- [ ] hot pathに不要allocation/searchがない。
- [ ] async/coroutineがcancel可能。
- [ ] logに原因・ID・stateが含まれる。
- [ ] testが実装詳細でなく規則を確認する。

## 39. Debug確認

- [ ] 現在state/action/timeを表示できる。
- [ ] input buffer内容を表示できる。
- [ ] hit shape、ground query、camera collisionをGizmo表示できる。
- [ ] NavMesh path/statusとAI scoreを表示できる。
- [ ] asset handle/reference ownerを追跡できる。
- [ ] network tick/RTT/correctionを表示できる。
- [ ] performance markerとframe time graphがある。
- [ ] Debug機能をReleaseで無効化・制限できる。

## 40. Test確認

- [ ] pure domain ruleはEdit Modeで高速testする。
- [ ] GameObject/Physics/Animator統合はPlay Modeでtestする。
- [ ] boundary valueを含む。
- [ ] frame rate/tick rate差を含む。
- [ ] destroyed/disabled/pooled objectを含む。
- [ ] Scene load/unloadを含む。
- [ ] late join/disconnectを含む。
- [ ] target platform build smoke testを含む。

## 41. Performance確認

- [ ] target fps、memory、load time budgetが数値化されている。
- [ ] worst-case戦闘を実機計測した。
- [ ] 95/99 percentileを確認した。
- [ ] GC Alloc、Physics、Animation、Renderingを分類した。
- [ ] GPU overdraw/shadow/post effectを確認した。
- [ ] asset load/instantiate hitchを分けた。
- [ ] Scene遷移memory peakを確認した。
- [ ] 変更前後を同条件で比較した。

## 42. Version・再現性確認

- [ ] Unity Editor versionを固定した。
- [ ] `Packages/manifest.json`とlock fileを管理した。
- [ ] package versionに対応するdocumentationを参照した。
- [ ] Build Profile/Player Settingsをversion controlした。
- [ ] random seedと再現条件をlogできる。
- [ ] save/protocol/content versionがある。
- [ ] CI build手順がlocalと一致する。
- [ ] upgrade前にbranch/build/test baselineを取る。

## 43. 「全て理解した」の判定

全checkboxを一度埋めても、すべてのAPIを永遠に覚えたという意味ではありません。完成の基準は:

- 未知のAPIを公式資料から調べられる。
- 現象を責任、寿命、時間、authority、data flowへ分解できる。
- 小さい再現例を作れる。
- Profiler/Debugger/Testから根拠を集められる。
- 修正後に副作用を再検証できる。

ことです。

## 44. 次の段階: 独立した使用例

本編は概念と内部処理のノートです。次段階では`90_examples`へ、各概念の独立した`.cs`、test、最小Unity Project構成を追加します。

推奨順:

1. C#値型/参照型/GCのConsole例。
2. MonoBehaviour lifecycle観測Scene。
3. Input buffer純粋C# test。
4. Character Motor。
5. Combat Action/Hit/Damage。
6. AI/Navigation。
7. Addressables ownership。
8. Job/Burst。
9. Multiplayer prediction。
10. 総合統合例。

一例へ全概念を詰めず、失敗例と修正版も分けて比較します。

## 45. Unity編全章

1. [C#とC++・Unity Script](01_csharp_cpp_unity_overview.md)
2. [Project・Asset・Package・Assembly](02_project_assets_assemblies.md)
3. [GameObject・Component・Prefab](03_gameobject_component_prefab.md)
4. [MonoBehaviour Lifecycle](04_monobehaviour_lifecycle.md)
5. [Managed Memory・GC](05_managed_memory_gc.md)
6. [Unity Object・Serialization](06_unity_object_serialization.md)
7. [Scene管理](07_scene_management.md)
8. [Prefab Variant](08_prefab_variants.md)
9. [Input System](09_new_input_system.md)
10. [Time・Coroutine・async](10_time_fixedupdate_coroutine_async.md)
11. [Physics](11_physics_2d_3d_layers.md)
12. [Character Motor](12_character_controller_motor.md)
13. [Animator・Root Motion](13_animator_animation_root_motion.md)
14. [Camera・Lock-on](14_cinemachine_camera_lockon.md)
15. [Render Pipeline・Shader](15_render_pipeline_shader_graph_material.md)
16. [Lighting・Post Processing](16_lighting_shadow_post_processing.md)
17. [ScriptableObject・Data-driven](17_scriptableobject_data_driven_design.md)
18. [UI・HUD](18_ui_toolkit_ugui_text_hud.md)
19. [Audio・Particle・VFX](19_audio_particle_vfx_graph.md)
20. [Addressables・Async Load](20_addressables_assetbundle_async_loading.md)
21. [Save・Settings・Localization](21_save_settings_localization.md)
22. [Object Pool・GC削減](22_object_pool_gc_allocation.md)
23. [Profiler](23_profiler_frame_debugger_memory_profiler.md)
24. [Job・NativeContainer・Burst](24_job_system_nativecontainer_burst.md)
25. [ECS・DOTS](25_ecs_dots_baking_system_component_data.md)
26. [Editor Tool](26_editor_extension_property_drawer_tools.md)
27. [Test・CI・Build](27_test_framework_ci_build.md)
28. [Assembly Definition・依存設計](28_assembly_definition_dependency_design.md)
29. [3D Action Combat](29_3d_action_combat.md)
30. [Enemy AI・NavMesh・Boss](30_enemy_ai_navmesh_boss.md)
31. [Multiplayer](31_multiplayer_fundamentals.md)
32. [Platform最適化](32_mobile_console_pc_optimization.md)
33. [Unity・Unreal比較](33_unity_unreal_engine_comparison.md)
34. Unity編・完成確認表（この章）

## 46. 最終自己評価

```text
C# / Memory                 Level __ / 3
Unity Object / Lifecycle    Level __ / 3
Scene / Asset / Loading     Level __ / 3
Input / Time / Physics      Level __ / 3
Character / Animation       Level __ / 3
Camera / Rendering / VFX    Level __ / 3
UI / Audio / Save           Level __ / 3
Architecture / Test         Level __ / 3
Job / ECS / Performance     Level __ / 3
Combat / AI / Boss          Level __ / 3
Multiplayer                 Level __ / 3
Platform / Engine比較       Level __ / 3
```

Level 0や1は失敗ではなく、次に読む章を示す地図です。一定期間後に同じ評価をやり直し、説明・実装・診断のどこが伸びたか確認します。
