# Editor拡張・Property Drawer・Tool制作

> 対象: Unity 6.0。`UnityEditor` APIはPlayerでは使えないため、Editor専用folderまたはAssembly Definitionへ分離すること。

## 1. Toolの目的

Editor Toolは単にbuttonを増やすものではありません。

- 人手の反復を自動化。
- 不正dataを入力時/build前に検出。
- 大量Assetを安全に編集。
- runtime codeへEditor都合を漏らさない。
- designerが意図を確認できるpreview/diffを提供。

## 2. Editor専用境界

方法:

- `Assets/.../Editor/`配下。
- Editor専用`.asmdef`。
- 小さい条件箇所だけ`#if UNITY_EDITOR`。

runtime assemblyから`UnityEditor`を参照するとPlayer buildが失敗します。基本はassembly境界で分離します。

## 3. Assembly構成

```text
Game.Runtime.asmdef
  ↑ runtime types
Game.Editor.asmdef
  references Game.Runtime
  includePlatforms: Editor
Game.Tests.Editor.asmdef
```

Editor assemblyからruntimeへ依存し、runtimeからEditorへ逆依存させません。

## 4. MenuItem

```csharp
using UnityEditor;

public static class ValidationMenu
{
    [MenuItem("Tools/Game/Validate Selected Assets")]
    private static void ValidateSelected()
    {
        foreach (UnityEngine.Object selected in Selection.objects)
        {
            UnityEngine.Debug.Log($"Validate: {selected.name}", selected);
        }
    }

    [MenuItem("Tools/Game/Validate Selected Assets", true)]
    private static bool CanValidateSelected()
    {
        return Selection.objects.Length > 0;
    }
}
```

validation methodは同じmenu pathで`true`を指定します。shortcut衝突やmenu階層をteam規約で管理します。

## 5. EditorWindow

```csharp
using UnityEditor;
using UnityEngine;
using UnityEngine.UIElements;

public sealed class CombatDataWindow : EditorWindow
{
    [MenuItem("Tools/Game/Combat Data")]
    private static void Open()
    {
        CombatDataWindow window = GetWindow<CombatDataWindow>();
        window.titleContent = new GUIContent("Combat Data");
        window.minSize = new Vector2(420f, 240f);
    }

    public void CreateGUI()
    {
        var title = new Label("Combat Data Validator");
        title.AddToClassList("tool-title");
        rootVisualElement.Add(title);

        var validateButton = new Button(RunValidation)
        {
            text = "Validate All"
        };
        rootVisualElement.Add(validateButton);
    }

    private void RunValidation()
    {
        Debug.Log("Validation started.");
    }
}
```

`CreateGUI`はroot visual element準備後に呼ばれます。UXML/USS loadとevent登録はここ以降に置きます。

## 6. UI Toolkit構造

- `VisualElement`: tree node。
- UXML: UI hierarchy/data。
- USS: style。
- C#: behavior。
- binding: SerializedObject等との同期。
- `ListView`/`TreeView`: virtualization可能な大量項目。

大量AssetをScrollViewへ全生成せず、ListViewのvirtualizationを使います。

## 7. Hot Reloadとstate

EditorWindowはScriptableObjectで、domain reload/layout復元に関わります。

-必要stateは`[SerializeField]`。
- VisualElement referenceは`CreateGUI`で再取得。
- eventを重複登録しない。
- static cacheをreload時にclear。
- selected Assetが削除された場合を扱う。

## 8. Custom Inspector

```csharp
using UnityEditor;

[CustomEditor(typeof(AttackDefinition))]
[CanEditMultipleObjects]
public sealed class AttackDefinitionEditor : Editor
{
    private SerializedProperty damage;
    private SerializedProperty hitStopSeconds;

    private void OnEnable()
    {
        damage = serializedObject.FindProperty("damage");
        hitStopSeconds = serializedObject.FindProperty("hitStopSeconds");
    }

    public override void OnInspectorGUI()
    {
        serializedObject.Update();

        EditorGUILayout.PropertyField(damage);
        EditorGUILayout.PropertyField(hitStopSeconds);

        if (!damage.hasMultipleDifferentValues && damage.intValue < 0)
        {
            EditorGUILayout.HelpBox(
                "Damage must be zero or greater.",
                MessageType.Error);
        }

        serializedObject.ApplyModifiedProperties();
    }
}
```

target fieldを直接代入せずSerializedObjectを使うと、Undo、Prefab override、multi-object edit、serializationへ統合できます。

## 9. SerializedObject

基本手順:

```text
serializedObject.Update()
 → SerializedPropertyを表示/変更
 → serializedObject.ApplyModifiedProperties()
```

field名文字列はrenameで壊れます。`nameof`が使える範囲、定数、Editor test、FormerlySerializedAsを検討します。

## 10. SerializedProperty

代表property:

- `propertyType`。
- `intValue/floatValue/stringValue/objectReferenceValue`。
- `isArray/arraySize`。
- `FindPropertyRelative`。
- `hasMultipleDifferentValues`。
- `isExpanded`。
- `propertyPath`。

array elementのinsert/delete挙動、ObjectReference deleteが二段階になるケース等を対象Versionで確認します。

## 11. Multi-object edit

複数選択時は値が混在します。

- `CanEditMultipleObjects`。
- `hasMultipleDifferentValues`。
-一括変更のUndo。
-一部だけinvalidな場合の表示。
- list length違い。

最初のtarget値で全targetを上書きしないようにします。

## 12. Undo

SerializedProperty外でobjectを変更する場合:

```csharp
Undo.RecordObject(targetObject, "Normalize Attack Data");
targetObject.Normalize();
EditorUtility.SetDirty(targetObject);
```

複数objectには`Undo.RecordObjects`。GameObject追加/破棄、component追加には対応Undo APIを使います。

Undo登録は変更前に行います。

## 13. DirtyとSave

`SetDirty`は変更通知であり、即disk保存と同義ではありません。

- Scene object変更はScene dirty/Undoを扱う。
- Asset変更はdirtyにする。
- userの未保存作業を勝手に全保存しない。
- batch/CI toolだけ明示SaveAssetsを検討。

## 14. Prefab Stage

Inspector/Scene Toolは通常SceneだけでなくPrefab Modeでも動きます。

- Prefab assetかinstanceか。
- override記録。
- nested prefab。
- Prefab Stage context。
- multi-scene editing。

Prefab instanceを直接変更してoverrideを記録しない事故を防ぎます。

## 15. PropertyDrawer

```csharp
using System;
using UnityEngine;

[Serializable]
public struct MinMaxFloat
{
    public float Min;
    public float Max;
}
```

```csharp
using UnityEditor;
using UnityEngine;

[CustomPropertyDrawer(typeof(MinMaxFloat))]
public sealed class MinMaxFloatDrawer : PropertyDrawer
{
    public override void OnGUI(
        Rect position,
        SerializedProperty property,
        GUIContent label)
    {
        EditorGUI.BeginProperty(position, label, property);

        SerializedProperty min = property.FindPropertyRelative("Min");
        SerializedProperty max = property.FindPropertyRelative("Max");

        position = EditorGUI.PrefixLabel(position, label);
        float half = (position.width - 4f) * 0.5f;

        min.floatValue = EditorGUI.FloatField(
            new Rect(position.x, position.y, half, position.height),
            min.floatValue);
        max.floatValue = EditorGUI.FloatField(
            new Rect(position.x + half + 4f, position.y, half, position.height),
            max.floatValue);

        EditorGUI.EndProperty();
    }
}
```

値を自動clampするならuser入力を勝手に変える影響を明示し、validation error表示との使い分けをします。

## 16. Drawerの高さ

複数行/HelpBoxを描く場合`GetPropertyHeight`をoverrideし、実描画高さと一致させます。固定`singleLineHeight`のままだと次propertyと重なります。

## 17. UI Toolkit PropertyDrawer

Unity 6では`CreatePropertyGUI`でVisualElementを返す方式もあります。IMGUI Inspectorとのfallback/対応範囲をVersion資料で確認します。

binding pathを正しく設定し、SerializedPropertyを長期cacheして無効参照にしないようにします。

## 18. PropertyAttribute

```csharp
public sealed class StableIdAttribute : PropertyAttribute { }
```

attribute + drawerでfield単位の表示/validationを再利用できます。ただしruntime assembly側attributeがUnityEditorへ依存しないよう、attributeはruntime、drawerはEditor assemblyへ置きます。

## 19. CustomEditor vs Drawer

- CustomEditor: object全体のworkflow。
- PropertyDrawer: 型/fieldの再利用表示。
- DecoratorDrawer: property前後の装飾。
- EditorWindow: 複数Asset/独立task。
- Scene Tool/Overlay: Scene View操作。

責務に合う拡張点を選びます。

## 20. AssetDatabase検索

```csharp
string[] guids = AssetDatabase.FindAssets(
    "t:AttackDefinition",
    new[] { "Assets/GameData/Combat" });

foreach (string guid in guids)
{
    string path = AssetDatabase.GUIDToAssetPath(guid);
    AttackDefinition asset =
        AssetDatabase.LoadAssetAtPath<AttackDefinition>(path);
}
```

project全体検索を毎frame/OnGUIで行わず、button、cache、Asset change eventで更新します。

## 21. GUIDとPath

pathはrename/moveで変わりますがGUIDは`.meta`と共に維持されます。

- source controlで`.meta`を必ず管理。
- string path hard-codeを限定。
- AssetReference/GUIDを用途で使う。
- package/read-only assetを変更しない。

## 22. Asset作成

```csharp
AttackDefinition asset = ScriptableObject.CreateInstance<AttackDefinition>();
asset.name = "New Attack";

string path = AssetDatabase.GenerateUniqueAssetPath(
    "Assets/GameData/Combat/NewAttack.asset");

AssetDatabase.CreateAsset(asset, path);
AssetDatabase.SaveAssets();
Selection.activeObject = asset;
EditorGUIUtility.PingObject(asset);
```

既存pathを上書きせず、unique pathとconfirmationを使います。

## 23. 一括編集の安全性

```text
Scan
 → Proposed Changesを表示
 → user確認
 → Undo group / backup
 → Apply
 → Validate
 → report
```

Preview/dry-runなしで数千Assetを変更しません。対象pathと件数を明示します。

## 24. StartAssetEditing

複数Asset変更中の個別importを止め、最後にまとめられます。

```csharp
AssetDatabase.StartAssetEditing();
try
{
    // 複数Assetの変更。
}
finally
{
    // exception時も必ず再開しないとEditorがimport停止状態になる。
    AssetDatabase.StopAssetEditing();
}
```

この状態で使えないAssetDatabase APIやimport dependencyを確認します。

## 25. AssetPostprocessor

import前後に設定/検証を適用します。

```csharp
public sealed class TextureImportRules : AssetPostprocessor
{
    private void OnPreprocessTexture()
    {
        if (!assetPath.StartsWith("Assets/Game/Textures/UI/"))
        {
            return;
        }

        var importer = (TextureImporter)assetImporter;
        importer.mipmapEnabled = false;
        importer.textureType = TextureImporterType.Sprite;
    }
}
```

無条件に全Textureを再設定せず、folder/rule versionを限定します。設定変更→reimport→再変更のloopを防ぎます。

## 26. Import rule version

`AssetPostprocessor.GetVersion()`等の仕組みでrule変更時のreimportを管理できます。数万Assetのreimport costをCI/作業時間として計画します。

## 27. Validation design

errorに含めるもの:

- severity。
- rule ID。
- asset path/GUID。
- object context。
- message。
- suggested fix。
- auto-fix可能か。

human-readable logだけでなくstructured resultを返します。

## 28. Validator例

```csharp
public readonly record struct ValidationIssue(
    string RuleId,
    MessageType Severity,
    string Message,
    UnityEngine.Object Context);

public static IEnumerable<ValidationIssue> Validate(
    AttackDefinition attack)
{
    if (attack.Damage < 0)
    {
        yield return new ValidationIssue(
            "COMBAT001",
            MessageType.Error,
            "Damage must be zero or greater.",
            attack);
    }
}
```

validation coreをUIから分離し、Inspector、Window、CIから同じruleを使います。

## 29. OnValidate

簡単なlocal validationには便利ですが、Editor上で予想以上の頻度/時機に呼ばれます。

- 他Asset検索や重い処理をしない。
- AssetDatabase mutationを再帰させない。
-値の正規化を最小限に。
- build全体validationは別toolへ。

## 30. Build validation

Build前に:

- duplicate stable ID。
- missing reference。
- Addressables label/address。
- localization missing key。
- platform import override。
- forbidden Editor reference。
- production endpoint。

を検査し、重大errorならbuildを停止します。通常Editor UXとbatchmode出力の両方を用意します。

## 31. Batch Mode

Editor methodをcommand lineから実行する場合:

- static entry point。
- deterministic input。
- dialogを出さない。
- clear exit code。
- log/report path。
- license/Package resolve。
- source control clean state。

CIでGUI前提APIを呼びません。

## 32. Progress API

長い処理は進捗とcancelを表示します。cancel時もfinallyでAsset editing、temporary data、event購読をcleanupします。

main threadを長時間blockする場合、chunk処理/EditorApplication.updateへ分割するか検討します。

## 33. SelectionとPing

issue rowをclickした時:

- `Selection.activeObject`。
- `EditorGUIUtility.PingObject`。
- Scene objectならframe/select。
- sub-assetならowner context。

toolは問題発見だけでなく修正地点まで案内します。

## 34. Scene Tool

Scene Viewでhitbox、spawn point、camera rail等を直接編集できます。

- `EditorTool`。
- Handles。
- Undo。
- local/world coordinate。
- selection/pivot。
- Prefab Mode。
- snapping。

Scene View描画中にruntime dataを毎repaint破壊しません。

## 35. HandlesとUndo

position handle変更例では`EditorGUI.BeginChangeCheck/EndChangeCheck`で変更を検出し、変更前にUndoを記録してSerializedPropertyへ反映します。

mouse drag中毎eventで巨大Assetを再importしないよう、確定時処理を分けます。

## 36. Gizmo

常時visualizeならGizmo、能動的編集ならTool/Handleが適します。

- selected時だけ描画。
- distance/LODで省略。
- color legend。
- depth test。
- Editor performance。

大量AI範囲を全て描いてScene Viewを重くしないでください。

## 37. Context Menu

`[ContextMenu]`やMenuItem validation/context pathで対象objectに近い操作を提供できます。破壊的操作は確認、Undo、対象件数を用意します。

## 38. Drag and Drop

UI Toolkit eventでAssetを受け取る時:

- type filter。
- folder/package/read-only判定。
- duplicate。
- null/deleted asset。
- visual feedback。
- drop後Undo/serialization。

external file dropはpath/security/import policyを確認します。

## 39. Toolの性能

- OnGUI/CreateGUIでAssetDatabase全検索しない。
- repaintを必要時だけ。
- List virtualization。
- thumbnail async/cache。
- serialized property cache無効化。
- domain reload後cache再構築。
- incremental validation。

Editor ToolもProfilerで計測できます。

## 40. Toolのテスト

- single/multi selection。
- Undo/Redo。
- Prefab asset/instance/stage。
- Scene未保存。
- read-only Package asset。
- missing/deleted reference。
- domain reload。
- assembly recompile。
- batchmode。
- exception途中のStartAssetEditing復旧。
- dry-runとapply差分一致。

## 41. よくある失敗

### targetを直接変更

Undo、Prefab override、multi-editを壊す。SerializedObject/Undoを使う。

### Editor codeがruntime assemblyへ入る

Player build失敗。Editor asmdefへ分離。

### OnGUIで全Asset検索

repaintごとに重い処理。cache/button/event更新。

### finallyなしStartAssetEditing

exception後にimport停止。必ずtry/finally。

### Auto-fixだけでpreviewなし

大量dataを意図せず変更。dry-run/diff/Undo。

### pathをstable ID扱い

move/renameで壊れる。GUID/referenceを用途別に使う。

## 42. Review checklist

- Editor専用assemblyか。
- SerializedObject/Propertyを使うか。
- Undo/Redoが正しいか。
- multi-object editを扱うか。
- Prefab override/Stageを扱うか。
- destructive batchにpreviewがあるか。
- Start/StopAssetEditingがfinallyか。
- AssetPostprocessorがimport loopを作らないか。
- validation coreがUIから分離されているか。
- batchmodeのexit code/reportがあるか。
- Domain Reload後にevent/cacheが安全か。
- tool自身のperformanceを測ったか。

## 43. 学習確認問題

1. Editor codeをruntime assemblyへ入れてはいけない理由は何か。
2. SerializedObjectを使う利点は何か。
3. Undo.RecordObjectは変更の前後どちらで呼ぶか。
4. PropertyDrawerとCustomEditorの役割差は何か。
5. multi-object editで混在値をどう検出するか。
6. StartAssetEditingにfinallyが必要な理由は何か。
7. AssetPostprocessorがloopする原因は何か。
8. GUIDとpathの違いは何か。
9. batch toolにdry-runが必要な理由は何か。
10. validation coreをWindowから分離する理由は何か。

## 44. 公式資料

- [Unity Manual: Editor UI with UI Toolkit](https://docs.unity3d.com/6000.0/Documentation/Manual/UIE-support-for-editor-ui.html)
- [Create a custom Editor window](https://docs.unity3d.com/6000.0/Documentation/Manual/UIE-HowTo-CreateEditorWindow.html)
- [EditorWindow API](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/EditorWindow.html)
- [SerializedObject API](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/SerializedObject.html)
- [SerializedProperty API](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/SerializedProperty.html)
- [PropertyDrawer API](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/PropertyDrawer.html)
- [Undo API](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/Undo.html)
- [AssetDatabase API](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/AssetDatabase.html)
- [AssetPostprocessor API](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/AssetPostprocessor.html)
- [StartAssetEditing API](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/AssetDatabase.StartAssetEditing.html)

## 45. まとめ

- Editor拡張はruntimeとassemblyで分離する。
- Inspector編集はSerializedObject/PropertyでUndo、Prefab、multi-editへ統合する。
- UI ToolkitのEditorWindowで独立workflowを作り、大量一覧はvirtualizeする。
- PropertyDrawerは再利用field、CustomEditorはobject全体、Scene Toolは空間編集に使う。
- Asset一括処理はscan、preview、Undo、apply、validateの順で安全に行う。
- AssetPostprocessorとbuild validationでdata品質を入口から守る。
