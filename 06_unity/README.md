# 06U Unity

Unreal Engine編の後に学ぶUnity教材です。Unityの操作手順だけでなく、C#の言語仕様、Managed Memory、Unity Objectの特殊な寿命、Serialization、実行順序、Engine内部との境界を理解します。

## ノート一覧

1. [C#とC++の違い、Unity Scriptの全体像](01_csharp_cpp_unity_overview.md)
2. [Unity Project、Asset、Package、Assembly](02_project_assets_assemblies.md)
3. [GameObject、Component、Transform、Prefab](03_gameobject_component_prefab.md)
4. [MonoBehaviourのLifecycleと実行順序](04_monobehaviour_lifecycle.md)
5. [値型、参照型、Managed Memory、GC](05_managed_memory_gc.md)
6. [UnityEngine.Object・null・Destroy・Serialization](06_unity_object_serialization.md)
7. [Scene管理・非同期遷移・永続System](07_scene_management.md)
8. [Prefab・Nested Prefab・Prefab Variant](08_prefab_variants.md)
9. [New Input System・Action・入力バッファ](09_new_input_system.md)
10. [Time・Update・FixedUpdate・Coroutine・async/await](10_time_fixedupdate_coroutine_async.md)
11. [2D/3D Physics・Collider・Layer・戦闘判定](11_physics_2d_3d_layers.md)
12. [Character Controller・Character Motor・移動設計](12_character_controller_motor.md)
13. [Animator・Animation Event・Avatar・Root Motion](13_animator_animation_root_motion.md)
14. [Cinemachine・Camera・Lock-on・Camera Collision](14_cinemachine_camera_lockon.md)
15. [Render Pipeline・URP/HDRP・Shader Graph・Material](15_render_pipeline_shader_graph_material.md)
16. [Lighting・Shadow・Probe・Post Processing](16_lighting_shadow_post_processing.md)

## 今後制作するノート

17. ScriptableObjectとデータ駆動設計
18. UI Toolkit・uGUI・Text
19. Audio・VFX Graph・Particle System
20. Addressables・AssetBundle・非同期ロード
21. Save・Settings・Localization
22. Object PoolとGC Allocation削減
23. Profiler・Frame Debugger・Memory Profiler
24. Job System・NativeContainer・Burst
25. ECS/DOTS・Baking・System・Component Data
26. Editor拡張・Property Drawer・Tool制作
27. Test Framework・CI・Build
28. Assembly Definitionと依存設計
29. 3Dアクション戦闘
30. 敵AI・NavMesh・Boss
31. Multiplayer基礎
32. Mobile/Console/PC最適化
33. Unreal Engineとの比較
34. Unity編・完成確認表

## 方針

- `Update`へ何でも詰め込まず、責任と時間軸を分離します。
- `MonoBehaviour`を全データの所有者にせず、純粋C#のDomain LogicとEngine境界を分けます。
- Inspectorへ表示された状態とRuntimeの真実を区別します。
- GC Allocation、Main Thread制約、Native ResourceのDisposeを扱います。
- APIは利用するUnity Versionで変化するため、執筆時に公式資料とPackage Versionを確認します。
- 使用例は後から独立した`.cs`とUnity Projectとして`90_examples`へ追加します。
