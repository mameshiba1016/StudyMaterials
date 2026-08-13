# 06U Unity

Unreal Engine編の後に学ぶUnity教材です。Unityの操作手順だけでなく、C#の言語仕様、Managed Memory、Unity Objectの特殊な寿命、Serialization、実行順序、Engine内部との境界を理解します。

## 制作予定ノート

1. C#基礎とC++との差
2. 値型・参照型・Boxing・GC
3. Assembly、Namespace、asmdef
4. Unity EditorとProject構造
5. GameObject・Component・Transform
6. MonoBehaviourのLifecycleと実行順序
7. Unity Objectのnull・Destroy・Serialization
8. Scene・Prefab・Prefab Variant
9. New Input System
10. Time・FixedUpdate・Coroutine・async
11. 2D/3D PhysicsとLayer
12. Character Controller
13. Animator・Animation Event・Root Motion
14. CinemachineとCamera
15. URP/HDRP・Shader Graph・Material
16. Lighting・Shadow・Post Processing
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
34. Unity就職作品チェックリスト

## 方針

- `Update`へ何でも詰め込まず、責任と時間軸を分離します。
- `MonoBehaviour`を全データの所有者にせず、純粋C#のDomain LogicとEngine境界を分けます。
- Inspectorへ表示された状態とRuntimeの真実を区別します。
- GC Allocation、Main Thread制約、Native ResourceのDisposeを扱います。
- APIは利用するUnity Versionで変化するため、執筆時に公式資料とPackage Versionを確認します。
- 使用例は後から独立した`.cs`とUnity Projectとして`90_examples`へ追加します。
