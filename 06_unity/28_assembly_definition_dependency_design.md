# Assembly Definition・依存設計

> 対象: Unity 6.0。Assembly Definitionはcompile単位と参照可能範囲を定義する。folder整理だけでarchitectureが良くなるわけではない。

## 1. Assemblyとは

C# sourceはcompileされ、型とIL/native変換前情報を含むassemblyになります。asmdefがない多くのscriptは`Assembly-CSharp.dll`へ入ります。

大規模化すると:

- 一変更で広範囲を再compile。
- 全codeが互いを参照可能。
- Editor/Platform/Test境界が曖昧。
- reuseが難しい。

asmdefで明示的なlibraryへ分けます。

## 2. asmdefの効果

`.asmdef`をfolderへ置くと、そのfolderと、別asmdef/asmrefで遮られていない子folderのscriptが一assemblyになります。

```text
Assets/Game/Combat/Game.Combat.asmdef
  Damage.cs
  Hit.cs
  Internal/
    Resolver.cs
```

folder名やasmdef file名ではなく、InspectorのNameがassembly名です。

## 3. 依存方向

Assembly Aの型がAssembly Bの型を使うならA→B referenceが必要です。

```text
Presentation → Application → Domain
Infrastructure ────────────→ Domain
Editor Tools → Runtime Modules
Tests → Tested Module
```

内側のDomainからUI、Editor、network implementationへ逆依存させません。

## 4. 循環参照

```text
Combat → UI
UI → Combat
```

Unityはassembly循環参照を許しません。片方へreferenceを追加して解決するのではなく:

- interface/eventを下位contractへ移す。
- shared kernelへ値型だけ抽出。
- mediator/application層を作る。
- 二moduleを本当に一単位なら統合。

## 5. Compile順は依存から決まる

「このassemblyを先にcompile」という任意番号は設定しません。参照graphの下位からcompileされます。初期化実行順とは別問題です。

## 6. 小さく分け過ぎない

一class一assemblyは:

- asmdef管理増。
- reference graph複雑化。
- compile/setup overhead。
- internal API共有困難。
- package define設定重複。

変更理由、再利用単位、platform/editor境界で分けます。

## 7. 大き過ぎない

`Game.Runtime`一つだけでは:

-変更が全runtimeをrecompile。
-依存違反をcompilerが止めない。
- feature削除/reuse困難。

domain moduleとengine adapterを適度に分離します。

## 8. 推奨例

```text
Game.Core
  ID、Result、math、contracts

Game.Combat.Domain
  damage、state、commands

Game.Combat.Unity
  MonoBehaviour、Animator、Physics bridge

Game.UI
Game.Audio
Game.Streaming
Game.Editor
Game.Tests.EditMode
Game.Tests.PlayMode
```

実際の境界はprojectの変更頻度とteam ownershipで決めます。

## 9. Domain assembly

可能なら`No Engine References`を有効にし、UnityEngineなしのpure C# libraryにします。

利点:

-高速unit test。
- Unity Object lifecycleから独立。
-再利用しやすい。
- engine dependency混入をcompile errorで検出。

## 10. No Engine References

有効にするとUnityEngine/UnityEditorへの自動参照を加えません。`Vector3`、`Mathf`、`Debug`等も使えません。

`System.Numerics`や独自value typeを使うか、Unity bridge境界で変換します。

## 11. Editor assembly

```text
Runtime/Game.Combat.asmdef
Editor/Game.Combat.Editor.asmdef
  includePlatforms: Editor
  references: Game.Combat
```

`UnityEditor`をruntime assemblyへ入れません。Editor folderでも、親asmdef配下では自動的にEditor assemblyへ行かないため明示asmdef/asmrefが必要です。

## 12. Test assembly

```text
Tests/EditMode/Game.Combat.Editor.Tests.asmdef
Tests/PlayMode/Game.Combat.Tests.asmdef
```

- tested runtimeへのreference。
- Test Assemblies参照。
- Editor/Player platform。
- test codeをproduction Playerから除外。

## 13. Platform assembly

```text
Game.Platform.Windows
Game.Platform.Android
Game.Platform.ConsoleX
```

Include/Exclude Platformsでcompile対象を限定します。共通interfaceは`Game.Platform.Contracts`等のplatform非依存assemblyへ。

## 14. IncludeとExclude

同じasmdefでInclude PlatformsとExclude Platformsを同時使用しません。Inspectorが生成するplatform名を使い、手書きJSON typoを避けます。

対象外platformではassembly自体が存在しないため、参照元も同じconstraint/adapterを持つ必要があります。

## 15. Define Constraints

指定symbolが全て成立する時だけassemblyをcompile/referenceします。

```text
ENABLE_ONLINE
!DISABLE_TELEMETRY
```

script内`#if`を大量に散らす代わりにassembly単位でfeatureを切れます。

## 16. Scripting Define Symbol

Project/Build Profile側symbolはglobalなcompile条件です。

-名前衝突しないprefix。
- production/development差を記録。
- CIがprofile/symbolを検証。
- undefined時の代替implementation。

symbolの組合せ爆発をtestします。

## 17. Version Defines

Unity/Package Version式に応じてsymbolをassemblyへ定義します。

例:

```text
Resource: com.unity.inputsystem
Expression: [1.7.0,2.0.0)
Define: GAME_INPUT_SYSTEM_1_7_OR_NEWER
```

正確な式syntaxをUnity 6資料で確認し、package API adapter内だけで使います。

## 18. Package API adapter

feature code全体へversion `#if`を散らさず:

```text
Game.Input.Contracts
  IGameInput

Game.Input.Unity
  Version Defineを使うadapter

Game.Combat
  IGameInputだけ参照
```

package updateの影響をadapterへ限定します。

## 19. Auto Referenced

defaultではpredefined assemblyがcustom assemblyを自動参照します。無効にするとAssembly-CSharp等から暗黙利用できず、不要な再compile dependencyを減らせます。

これはPlayer buildへ含む/含まない設定そのものではありません。

## 20. Predefined Assemblyとの混在

asmdef外のAssembly-CSharpはAuto Referencedなcustom assemblyを参照できますが、custom assemblyからAssembly-CSharpを明示参照できません。

段階導入で移動不能になるため、下位libraryからasmdef化し、上位scriptを後で移します。

## 21. Use GUIDs

assembly referenceを名前でなくasmdef Asset GUIDとして保存します。

利点:

- asmdef name/file renameへ強い。

注意:

- `.meta`削除でGUIDが変わる。
- external moveでmetaを置き忘れない。
- diff readability。

team方針を統一します。

## 22. asmref

Assembly Definition Referenceは別folderのscriptを既存asmdefへ所属させます。

用途:

- package layout上folderを分けたい。
-複数Editor folderを一Editor assemblyへ。
- generated codeを特定assemblyへ。

新assemblyを作るfileではありません。

## 23. Override References

custom assemblyは通常compatibleなprecompiled plugin DLLを自動参照します。Override Referencesを有効にし、実際に必要なDLLだけ明示できます。

利点:

- plugin変更時の不要recompile減。
-依存を明示。
-型名衝突を減らす。

platformごとのplugin compatibilityと合わせます。

## 24. Precompiled Plugin

DLL InspectorのAuto Referencedを無効にすると、asmdefからexplicit referenceが必要です。

- CPU architecture。
- Editor/Player。
- platform。
- managed/native plugin。
- load failure。
- license。

compile参照とruntime native binary配置を区別します。

## 25. Allow Unsafe Code

`unsafe`/pointerを使うassemblyだけ有効にします。project全体へ広げず、低水準moduleへ閉じ込め、Burst/NativeContainer safetyと別物だと理解します。

## 26. Root Namespace

新規script等のnamespace基準に使えます。assembly名とnamespaceは同じである必要はありませんが、通常は対応させると探索しやすくなります。

```text
Assembly: Studio.Game.Combat
Namespace: Studio.Game.Combat
```

## 27. Public API

assemblyを跨ぐ型は`public`が必要です。何でもpublicにせず:

- contracts/value typesだけpublic。
- implementationはinternal。
- factory/facade経由。
- mutable collectionを公開しない。
- Unity Object ownershipをcontractへ記載。

## 28. InternalsVisibleTo

test/friend assemblyへinternalを公開できます。

```csharp
[assembly: System.Runtime.CompilerServices.InternalsVisibleTo(
    "Studio.Game.Combat.Tests")]
```

循環architectureを隠すためにfriend assemblyを乱用しません。

## 29. Interface placement

依存逆転ではinterfaceを「利用側/内側のpolicy」に置きます。

```text
Combat Domain defines ICombatClock
Unity Adapter implements UnityCombatClock
Composition Root connects them
```

implementation側assemblyへinterfaceを置くとDomainが外側へ依存します。

## 30. Composition Root

具象implementationを組み立てる最上位assemblyを用意します。

```text
Game.Bootstrap
 → Combat.Domain
 → Combat.Unity
 → UI
 → Audio
```

Domain同士がservice locator/global singletonで隠れて結合しないようにします。

## 31. Event contract

module間eventにはpure valueを使います。

```csharp
public readonly record struct DamageApplied(
    int SourceId,
    int TargetId,
    int Amount);
```

GameObject/Component referenceを下位Domain eventへ持ち込みません。

## 32. Shared Kernel

ID、Result、small math等の本当に共有する型だけ置きます。

`Game.Common`へ何でも移すと巨大な依存中心になり、全変更が波及します。ownerを説明できないutilityは設計を見直します。

## 33. Feature slice

layerだけでなくfeature単位assemblyもあります。

```text
Combat.Domain
Combat.Unity
Inventory.Domain
Inventory.Unity
```

feature cohesionとlayer dependencyを組み合わせます。小規模projectで過剰分割しません。

## 34. Compile time

変更したassemblyと、それを参照する上位assemblyがrecompileされます。

安定した下位assembly:

```text
Core ← Domain ← Unity/Presentation ← Bootstrap
```

頻繁に変わる上位codeの変更でCoreは再compileされません。逆方向にすると波及が増えます。

## 35. Domain Reload

asmdef分割はcompile範囲を減らせても、script recompile後のDomain Reload/Asset refresh時間が支配する場合があります。Profiler/Editor Iteration Profiler等でcompile、reload、importを分けて測ります。

## 36. API変更波及

public type/signature変更は参照assemblyをrecompileし、code修正も必要になります。

- stable contract。
- additive evolution。
- obsolete migration period。
- adapter。
- semantic version/package化。

## 37. Package化

再利用moduleはUPM package layoutへ:

```text
Runtime/
Editor/
Tests/Runtime/
Tests/Editor/
Samples~/
Documentation~/
package.json
```

package内assemblyは外部package referenceも明示します。

## 38. Naming

reverse domain/organization prefixで衝突を避けます。

```text
Studio.Game.Combat
Studio.Game.Combat.Editor
Studio.Game.Combat.Tests
```

`Runtime`や`Scripts`だけの曖昧なassembly名を避けます。

## 39. Dependency rule

例:

```text
Core: external dependencyなし
Domain: Coreのみ
Application: Domain + contracts
Unity Adapter: Application + Unity packages
Presentation: Application + Unity UI
Bootstrap: 全具象を組み立て
Editor: 対象Runtime + UnityEditor
Tests: tested assembly
```

documentだけでなくasmdef referenceをCI検査します。

## 40. Dependency graph tool

`CompilationPipeline.GetAssemblies`等でassembly/reference graphを取得できます。

検査:

- cycle（Unity自体も拒否）。
- forbidden layer edge。
- Editor reference from Runtime。
- high fan-in/fan-out。
- orphan assembly。
- Auto Referenced方針。

## 41. Fan-in・Fan-out

- Fan-in高: 多数から参照される。API変更影響大、安定性が必要。
- Fan-out高: 多数へ依存する。責務過多/上位composition候補。

数だけで悪とせずmoduleの役割と変化率を見ます。

## 42. Platform abstraction

```text
Game.Save.Contracts
  ISaveBackend

Game.Save.Desktop
Game.Save.Console

Game.Bootstrap.<Platform>
```

platform assemblyはinclude platform、共通consumerはcontractsだけ参照します。

## 43. Optional feature

featureなしでもconsumerをcompileできるようNull implementationをcontracts側/別assemblyへ用意します。

```text
ITelemetry
 ├─ RealTelemetry（define/packageあり）
 └─ NullTelemetry（常に利用可能）
```

consumer全体を`#if`だらけにしません。

## 44. Serializationと型移動

MonoBehaviour/ScriptableObjectのassembly、namespace、class名変更はserialized referenceを壊し得ます。

- `MovedFrom`等のmigration attribute。
- meta/GUID維持。
- Prefab/Scene/Asset validation。
- branch merge。
- backup。

asmdef導入でtype identityが変わる影響をsmall batchでtestします。

## 45. ReflectionとAssembly名

assembly-qualified type name、reflection scan、DI container、serialization binderがassembly名へ依存する場合があります。

asmdef rename前に検索し、save/network formatへassembly-qualified nameを永続化しないようにします。

## 46. Addressables・Stripping

asmdef分割後、reflection/AssetBundleだけで使う型がstrippingされる可能性があります。

- Preserve/link.xml。
- registration code。
- Player build test。
- Addressables content build。

Editor compile成功だけで確認を終えません。

## 47. Migration手順

1. 現在のdependencyを検索。
2. Core/pure leaf moduleからasmdef作成。
3. compile errorで隠れ依存を発見。
4. runtime/editorを分離。
5. testsを分離。
6. feature assemblyを段階作成。
7. Player全platform build。
8. serialized Asset/Scene検証。

巨大な一括移動を避けます。

## 48. よくある失敗

### compile errorごとにreference追加

architecture違反を固定化します。依存方向を確認する。

### Common assembly肥大化

全moduleが相互結合。owner/変更理由で分割する。

### RuntimeからEditor参照

Player build失敗。Editor assemblyへ移す。

### platform assemblyを常時参照

対象外platformでassembly不在。contracts + platform bootstrap。

### asmdefを増やせばcompileが必ず速い

reload/importや上位波及が支配する場合。計測する。

## 49. Review checklist

- moduleの責務/ownerが一文で言えるか。
- dependencyが内側へ向くか。
- cycleをinterface/mediatorで除いたか。
- DomainはUnityEngineなしにできるか。
- Runtime/Editor/Testを分離したか。
- platform/optional featureにfallbackがあるか。
- public APIを最小化したか。
- Commonが肥大化していないか。
- GUID/metaを管理したか。
- serialized type移動を検証したか。
-全Player targetでbuildしたか。
- compile/reload時間を計測したか。

## 50. 学習確認問題

1. asmdefがcompile時間を減らせる理由は何か。
2. assembly循環参照をどう解消するか。
3. No Engine Referencesの利点は何か。
4. Auto Referenced無効化はbuild除外と同じか。
5. asmrefとasmdefの違いは何か。
6. Version Definesをadapterへ閉じ込める理由は何か。
7. interfaceを利用側Domainへ置く理由は何か。
8. Common assemblyが危険な理由は何か。
9. asmdef導入でserialized Assetが壊れ得る理由は何か。
10. compile time以外にDomain Reloadを測る理由は何か。

## 51. 公式資料

- [Unity Manual: Assembly definitions introduction](https://docs.unity3d.com/6000.0/Documentation/Manual/assembly-definitions-intro.html)
- [Create assembly assets](https://docs.unity3d.com/6000.0/Documentation/Manual/assembly-definitions-creating.html)
- [Reference assemblies](https://docs.unity3d.com/6000.0/Documentation/Manual/assembly-definitions-referencing.html)
- [Assembly Definition properties](https://docs.unity3d.com/6000.0/Documentation/Manual/class-AssemblyDefinitionImporter.html)
- [Assembly Definition file format](https://docs.unity3d.com/6000.0/Documentation/Manual/AssemblyDefinitionFileFormat.html)
- [Script compilation](https://docs.unity3d.com/6000.0/Documentation/Manual/script-compilation.html)
- [Assembly definitions in packages](https://docs.unity3d.com/6000.0/Documentation/Manual/cus-asmdef.html)
- [CompilationPipeline API](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/Compilation.CompilationPipeline.html)

## 52. まとめ

- asmdefはcompile単位と参照可能範囲を定義するarchitecture境界。
-依存をPresentation/AdapterからApplication/Domain/Coreへ一方向にする。
-循環はreference追加でなくcontract、mediator、統合で解消する。
- Runtime、Editor、Test、Platform、optional packageをassemblyで分離する。
- public APIを小さくし、implementationをinternalへ閉じる。
- compile時間だけでなくserialized data、stripping、Player build、reload時間を検証する。
