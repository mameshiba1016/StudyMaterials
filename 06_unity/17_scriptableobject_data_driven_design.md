# ScriptableObject・データ駆動設計・Asset検証

> ScriptableObjectはProject Assetとして共有する静的定義に強い。現在HPやcombo進捗のようなruntime state、製品版のSave Fileとは区別すること。

## 1. ScriptableObjectとは

`ScriptableObject`は`UnityEngine.Object`を継承するdata containerです。`MonoBehaviour`と違いGameObjectへattachせず、通常はProject内のAssetとして保存します。

```text
CharacterDefinition.asset（1個の共有定義）
        ↑ reference
Character Prefab
        ↓ Instantiate
Runtime Character A ── Runtime State A
Runtime Character B ── Runtime State B
```

Prefabへ大きな不変dataを直接持たせるとinstanceごとにcopyされます。ScriptableObject Assetをreferenceすれば同じ定義を共有できます。

## 2. 四種類のdataを分ける

### Static Definition

開発時に作る不変に近い設定。

- 最大HP基準。
- Attack定義。
- Animation/VFX/Prefab参照。
- Camera profile。
- AI tuning。

ScriptableObject向きです。

### Runtime State

Play中に個体ごとに変化。

- 現在HP。
- energy。
- cooldown。
- combo step。
- current target。

通常のC# object/Component等が所有します。

### Save Data

製品版でdiskへ永続化。

- 解放Character。
- 設定。
- progress。

JSON/binary/database/cloud等の保存形式へ変換します。ScriptableObject Assetを製品版で書き換えて保存する仕組みではありません。

### Session/Transient Data

Scene遷移中だけ保持するparty selection、spawn request等。ServiceやGame Session stateが所有します。

## 3. 最小の定義Asset

```csharp
using UnityEngine;

[CreateAssetMenu(
    fileName = "CharacterDefinition",
    menuName = "Game/Character Definition",
    order = 10)]
public sealed class CharacterDefinition : ScriptableObject
{
    [SerializeField] private string characterId;
    [SerializeField, Min(1)] private int baseMaxHp = 100;
    [SerializeField, Min(0.0f)] private float moveSpeed = 6.0f;
    [SerializeField] private GameObject characterPrefab;

    public string CharacterId => characterId;
    public int BaseMaxHp => baseMaxHp;
    public float MoveSpeed => moveSpeed;
    public GameObject CharacterPrefab => characterPrefab;
}
```

`CreateAssetMenu`はAsset作成menuを追加するだけです。class instanceを自動生成・登録する機能ではありません。

## 4. Assetは共有参照

複数Character instanceが同じ`CharacterDefinition`を参照する場合、Asset fieldを書き換えると全員から見える値が変わります。

```csharp
// 危険な設計例:
// definition.CurrentHp -= damage;
//
// CurrentHpを共有Assetへ置くと、同じ定義を使う全instanceが同じHPを共有する。
```

runtime stateを分離します。

```csharp
public sealed class CharacterRuntimeState
{
    public CharacterRuntimeState(CharacterDefinition definition)
    {
        Definition = definition;
        CurrentHp = definition.BaseMaxHp;
    }

    public CharacterDefinition Definition { get; }
    public int CurrentHp { get; private set; }

    public void ApplyDamage(int amount)
    {
        CurrentHp = Mathf.Max(0, CurrentHp - Mathf.Max(0, amount));
    }
}
```

## 5. Assetとruntime clone

一時的に変更可能なScriptableObject instanceが必要なら`Instantiate(asset)`または`CreateInstance<T>()`を使えます。ただし「なぜ普通のC# classでなくScriptableObject cloneなのか」を明確にします。

```csharp
private CharacterDefinition runtimeCopy;

private void Awake()
{
    runtimeCopy = Instantiate(sourceDefinition);
    runtimeCopy.name = $"{sourceDefinition.name} (Runtime Copy)";
}

private void OnDestroy()
{
    if (runtimeCopy != null)
    {
        Destroy(runtimeCopy);
    }
}
```

cloneは元Assetとは別instanceですが、内部の`UnityEngine.Object`参照は同じAssetを指す場合があります。deep copyだと決めつけません。

## 6. CreateInstance

```csharp
RuntimeEventChannel channel =
    ScriptableObject.CreateInstance<RuntimeEventChannel>();
```

Assetとして保存されないruntime instanceはownerが寿命を管理します。HideFlagsを使う高度なEditor/runtime objectは保存・unload・明示破棄の規則を理解します。

多くのpure runtime stateは通常classで十分です。

## 7. Character Definition

```csharp
using System.Collections.Generic;
using UnityEngine;

[CreateAssetMenu(menuName = "Game/Characters/Definition")]
public sealed class ActionCharacterDefinition : ScriptableObject
{
    [Header("Identity")]
    [SerializeField] private string stableId;
    [SerializeField] private string displayNameKey;

    [Header("Assembly")]
    [SerializeField] private GameObject prefab;
    [SerializeField] private Avatar avatar;
    [SerializeField] private RuntimeAnimatorController animatorController;

    [Header("Stats")]
    [SerializeField, Min(1)] private int maxHp = 100;
    [SerializeField, Min(0.0f)] private float moveSpeed = 6.0f;

    [Header("Combat")]
    [SerializeField] private List<AttackDefinition> attacks = new();

    public string StableId => stableId;
    public string DisplayNameKey => displayNameKey;
    public GameObject Prefab => prefab;
    public IReadOnlyList<AttackDefinition> Attacks => attacks;
}
```

display name文字列をdataへ直接固定せずLocalization keyへする設計もあります。

## 8. Attack Definition

```csharp
using UnityEngine;

[CreateAssetMenu(menuName = "Game/Combat/Attack Definition")]
public sealed class AttackDefinition : ScriptableObject
{
    [SerializeField] private string attackId;
    [SerializeField, Min(0)] private int damage;
    [SerializeField, Min(0)] private int startupTicks;
    [SerializeField, Min(1)] private int activeTicks = 1;
    [SerializeField, Min(0)] private int recoveryTicks;
    [SerializeField] private AnimationClip animationClip;
    [SerializeField] private GameObject hitVfxPrefab;
    [SerializeField] private AudioClip hitAudio;
    [SerializeField] private HitShapeDefinition hitShape;

    public string AttackId => attackId;
    public int Damage => damage;
    public int StartupTicks => startupTicks;
    public int ActiveTicks => activeTicks;
    public int RecoveryTicks => recoveryTicks;
    public int TotalTicks => startupTicks + activeTicks + recoveryTicks;
}
```

Animation Eventを攻撃判定の真実にせず、tick dataとAnimation presentationを同じAttack Definitionから構築します。

## 9. 小さいSerializable value

すべてを別ScriptableObject AssetにするとAsset数と参照追跡が増えます。Attack専用の小さな値は`[Serializable] struct/class`でinline保存します。

```csharp
[System.Serializable]
public struct HitShapeDefinition
{
    [SerializeField] private Vector3 localCenter;
    [SerializeField] private Vector3 halfExtents;
    [SerializeField] private float forwardOffset;

    public Vector3 LocalCenter => localCenter;
    public Vector3 HalfExtents => halfExtents;
    public float ForwardOffset => forwardOffset;
}
```

判断:

- 複数Assetから共有: ScriptableObject。
- 一つのhostだけが所有: inline value。
- 多態graph: `SerializeReference`を慎重に検討。
- runtime only: plain C#。

## 10. IDとAsset参照

Unity Asset参照はGUID/fileID等で管理されます。Editor内参照には強い一方、Save Dataへ直接Unity Objectをserializeする設計はplatform/version/Content更新で扱いづらいです。

```text
Asset reference（Editor/Build content）
CharacterDefinition.asset

Stable ID（Save/Network/External data）
"character_fire_001"
```

Stable IDを別途持ち、runtime registryでDefinitionへresolveします。

## 11. Stable ID

要件:

- renameしても変えない。
- 重複しない。
- 空でない。
- 大文字小文字規則。
- 一度releaseしたIDを別意味へ再利用しない。
- Save migration tableを用意できる。

Asset名/pathをSave IDにするとrename/moveでSaveが壊れます。

## 12. Registry

```csharp
using System;
using System.Collections.Generic;
using UnityEngine;

[CreateAssetMenu(menuName = "Game/Characters/Registry")]
public sealed class CharacterRegistry : ScriptableObject
{
    [SerializeField] private List<ActionCharacterDefinition> definitions = new();

    private Dictionary<string, ActionCharacterDefinition> byId;

    public void BuildRuntimeIndex()
    {
        byId = new Dictionary<string, ActionCharacterDefinition>(
            StringComparer.Ordinal);

        foreach (ActionCharacterDefinition definition in definitions)
        {
            if (definition == null)
            {
                continue;
            }

            // 実装ではTryAdd失敗をfatal validation errorとして報告する。
            byId.TryAdd(definition.name, definition);
        }
    }

    public bool TryGet(
        string id,
        out ActionCharacterDefinition definition)
    {
        return byId.TryGetValue(id, out definition);
    }
}
```

概念例では`name`を使っていますが、本番は公開`StableId`を使います。Editor validationでも重複を検出します。

## 13. Dictionaryをserializeしない構成

Unity標準serializationは一般的な`Dictionary`を直接保存しないため:

- Inspectorでは`List<Entry>`を保存。
- runtimeでDictionary indexを構築。
- duplicate/空keyをvalidatorで拒否。

`ISerializationCallbackReceiver`で変換する方法もありますが、callbackで重いUnity APIを呼ばず、小さい決定的変換に限定します。

## 14. Read-only API

Inspector用fieldを`[SerializeField] private`にし、外部へgetterのみ公開します。

```csharp
[SerializeField] private List<AttackDefinition> attacks = new();
public IReadOnlyList<AttackDefinition> Attacks => attacks;
```

ただし`IReadOnlyList`は「元Listが絶対変更されない」保証ではありません。Asset自体のruntime mutationをCoding Rule/Architectureで禁止します。

## 15. Immutable runtime snapshot

Asset値をruntime開始時にpure C# valueへcopyすると、途中変更やthread制約から分離できます。

```csharp
public readonly struct AttackRuntimeSpec
{
    public AttackRuntimeSpec(AttackDefinition source)
    {
        Damage = source.Damage;
        StartupTicks = source.StartupTicks;
        ActiveTicks = source.ActiveTicks;
        RecoveryTicks = source.RecoveryTicks;
    }

    public int Damage { get; }
    public int StartupTicks { get; }
    public int ActiveTicks { get; }
    public int RecoveryTicks { get; }
}
```

Simulation/Job/NetworkへUnity Objectを渡さず、必要な数値snapshotだけを渡せます。

## 16. AssetをSave Fileにしない

Unity公式Manualは、deployed buildでScriptableObject AssetをProject Assetとして保存更新する用途ではなく、開発時に保存済みdataをruntimeで利用すると説明しています。

Save:

```csharp
[System.Serializable]
public sealed class CharacterSaveData
{
    public string characterId;
    public int level;
    public int experience;
}
```

Load後:

```text
Save characterId
 → Registry resolve
 → CharacterDefinition
 → level等からRuntime State構築
```

## 17. Play Mode中のAsset変更

EditorでPlay中にAsset fieldを書き換えると、変更がAssetへ残る場合があります。Scene instance fieldの変更がPlay終了で戻る感覚と混同しないでください。

対策:

- Static Definitionをruntimeで変更しない。
- custom InspectorでPlay中編集をdisable/warning。
- runtime cloneまたはplain state。
- Version Control diffを確認。
- Asset databaseを自動保存するEditor toolを慎重に使う。

## 18. OnEnable

`ScriptableObject.OnEnable`はAsset/instanceがmemoryへloadされたとき等に呼ばれます。

用途:

- nonserialized cacheをreset。
- 小さいruntime indexのlazy準備。

危険:

- 呼出回数を一回だけと思う。
- Scene serviceへregister。
- Gameplay開始処理。
- Assetを書き換える。
- 実行順を前提にする。

明示的なGame Bootstrapがregistryをinitializeする方が追跡しやすい場合があります。

## 19. OnValidate

Inspector値変更やload等、Editor operation中に呼ばれるvalidation callbackです。頻繁に、main thread以外から呼ばれる可能性も公式APIに注意書きがあります。

```csharp
private void OnValidate()
{
    maxHp = Mathf.Max(1, maxHp);
    moveSpeed = Mathf.Max(0.0f, moveSpeed);
}
```

OnValidateへ向く:

- clamp。
- 空文字の検出。
- local field間の小さい整合。

向かない:

- Scene object生成。
- AssetDatabase大量scan。
- file write。
- Camera render。
- network。
- 重い全Project検証。

## 20. ClampとErrorを区別する

自動修正:

- negative radiusを0へ。
- percentageを0～1へ。

Errorとして止める:

- Stable ID重複。
- 必須Prefab null。
- Attack ID重複。
- combo循環が不正。
- startup+active+recoveryとClip timing不整合。

重大Errorを無言で勝手に直すとdata authorが問題を認識できません。Validator reportを出します。

## 21. Validate interface

```csharp
using System.Collections.Generic;

public interface IDataValidatable
{
    void CollectValidationIssues(List<DataValidationIssue> issues);
}

public enum ValidationSeverity
{
    Info,
    Warning,
    Error
}

public readonly struct DataValidationIssue
{
    public DataValidationIssue(
        ValidationSeverity severity,
        string message)
    {
        Severity = severity;
        Message = message;
    }

    public ValidationSeverity Severity { get; }
    public string Message { get; }
}
```

Asset自身はpureなruleを提供し、Editor toolが全Assetを列挙・report・選択表示します。

## 22. Attack validation

```csharp
public void CollectValidationIssues(List<DataValidationIssue> issues)
{
    if (string.IsNullOrWhiteSpace(attackId))
    {
        issues.Add(new(
            ValidationSeverity.Error,
            "Attack ID is empty."));
    }

    if (activeTicks <= 0)
    {
        issues.Add(new(
            ValidationSeverity.Error,
            "Active ticks must be greater than zero."));
    }

    if (animationClip == null)
    {
        issues.Add(new(
            ValidationSeverity.Error,
            "Animation clip is missing."));
    }
}
```

Error messageにAsset path、field名、関連ID、修正hintを付けると大量dataを直しやすくなります。

## 23. Cross-asset validation

一Asset内だけでは検出できない問題:

- ID重複。
- 参照graph循環。
- Registry未登録。
- Addressables label漏れ。
- Character Controllerに必要Component無し。
- Animator Controller Parameter不足。
- VFX/Audio key未登録。

Build前/CIでProject全体validatorを走らせます。

## 24. Editor Toolとruntime assemblyを分ける

```text
Game.Data.Runtime.asmdef
├─ ScriptableObject definitions
├─ pure validation rules
└─ runtime registry

Game.Data.Editor.asmdef
├─ AssetDatabase scanner
├─ custom Inspector
├─ validation window
└─ build preprocessor
```

`UnityEditor` namespaceをruntime assemblyへ入れるとPlayer buildが失敗します。Editor folder/asmdefで境界を作ります。

## 25. SerializedObject

Custom Editorではtarget fieldを直接代入するより`SerializedObject`/`SerializedProperty`を使うとUndo、multi-object edit、Prefab override、serializationへ統合しやすくなります。

```csharp
#if UNITY_EDITOR
using UnityEditor;

[CustomEditor(typeof(AttackDefinition))]
public sealed class AttackDefinitionEditor : Editor
{
    public override void OnInspectorGUI()
    {
        serializedObject.Update();
        DrawDefaultInspector();
        serializedObject.ApplyModifiedProperties();
    }
}
#endif
```

実際はEditor assemblyへ分け、`#if`だけへ依存しない構成を推奨します。

## 26. UndoとDirty

Editor ToolがAssetを変更するとき:

- Undoを記録。
- SerializedObjectを使う。
- 必要に応じdirty mark/save。
- AssetDatabase refresh/saveを乱用しない。

直接fieldを書き換えただけではUndo/保存へ正しく統合されない場合があります。

## 27. Sub-asset

一つのAsset file内へ複数`UnityEngine.Object`をsub-assetとして保存できます。

向く:

- Graph node。
- State。
- 複合definitionの内部object。

注意:

- AssetDatabase APIはEditor-only。
- owner/sub-assetの寿命。
- Undo。
- hide flags。
- rename/delete。
- GUIDはfile共通、fileIDで識別。
- Version Control diffの読みやすさ。

単純dataまで全部sub-assetにしません。

## 28. SerializeReferenceとの比較

`[SerializeReference]`:

- host Asset内部のmanaged polymorphic graph。
- null/shared reference/cycleを表現。
- hostを跨いで共有しない。
- type renameに注意。
- inlineよりoverhead。

ScriptableObject:

- 独立したUnity Asset/reference。
- 複数hostから共有。
- Inspectorで別Assetとして管理。
- Addressables等で個別load可能。

Behaviour Tree node graph等でどちらを使うか、共有、差分、load、Editor UXから決めます。

## 29. Strategy Asset

ScriptableObjectにmethodを持たせるdata+strategy構成もできます。

```csharp
public abstract class DamageFormula : ScriptableObject
{
    public abstract int Calculate(in DamageContext context);
}

[CreateAssetMenu(menuName = "Game/Combat/Flat Damage Formula")]
public sealed class FlatDamageFormula : DamageFormula
{
    [SerializeField] private int damage = 10;

    public override int Calculate(in DamageContext context)
    {
        return Mathf.Max(0, damage);
    }
}
```

Asset methodは共有instance上で呼ばれるため、Character固有mutable fieldを持たせません。pure functionに近く保ちます。

## 30. Event Channel

ScriptableObject Event Channel patternはScene間参照を緩められます。

```csharp
[CreateAssetMenu(menuName = "Game/Events/Void Channel")]
public sealed class VoidEventChannel : ScriptableObject
{
    public event System.Action Raised;

    public void Raise()
    {
        Raised?.Invoke();
    }
}
```

危険:

- Assetがglobal event bus化。
- 誰がpublish/subscribeするか見えない。
- Domain Reload無効でdelegate残留。
- Scene unload時の解除漏れ。
- gameplay順序が隠れる。

用途を限定し、`OnEnable`/`OnDisable`登録解除、debug listener一覧、typed payloadを整えます。重要Domain Logicは明示依存の方が読みやすいことも多いです。

## 31. Runtime Set pattern

Active Enemy等をAssetのListへ登録するpatternがありますが、Assetへruntime stateを置くため注意が必要です。

- Play終了後の残留。
- Domain Reload無効。
- Scene load/unload。
- duplicate登録。
- null cleanup。
- Test間の汚染。

Session Serviceのplain runtime collectionをdependency injectionする方が安全な場合があります。

## 32. 設定の継承

ScriptableObject AssetにはC++/C# classのようなAsset instance継承はありません。class inheritanceは可能でも、Base Asset値をVariant Assetが自動継承する仕組みとは別です。

代替:

- base definition参照 + override fields。
- composition。
- Editor copy tool。
- Prefab Variant。
- Addressable labels/groups。

```csharp
[System.Serializable]
public struct OptionalFloatOverride
{
    [SerializeField] private bool overrideValue;
    [SerializeField] private float value;

    public float Resolve(float baseValue)
    {
        return overrideValue ? value : baseValue;
    }
}
```

## 33. 循環参照

```text
CharacterDefinition
 → AttackDefinition
   → VFXDefinition
     → CharacterDefinition
```

Unity Object参照は循環できても、load bundle分割、validation、unload、依存理解が難しくなります。

依存方向:

```text
Low-level assets（Texture/Audio/Clip）
        ↑
Presentation Definitions
        ↑
Attack Definitions
        ↑
Character Definitions
        ↑
Registry
```

下位Assetから上位Characterへ戻さない規則を作ります。

## 34. Asset dependencyとMemory

CharacterDefinitionがPrefab、Texture、Audio、全VFXへ直接referenceすると、そのDefinitionをloadしただけで依存Assetもload対象になる場合があります。

- Hard reference。
- Addressable reference。
- ID/keyによる遅延resolve。

常時必要なcore dataと、戦闘開始時だけ必要なheavy presentationを分けます。Addressables章で詳しく扱います。

## 35. Addressablesへの接続

```text
CharacterCatalog entry
├─ stable ID
├─ light-weight stats
└─ AssetReference Character Prefab
             ↓ async load
       runtime instantiate
```

ScriptableObjectに`AssetReference`等を持たせる場合:

- load handleのowner。
- release。
- cancel/古い結果。
- dependency download。
- missing content。
- Editor direct modeとBuild mode差。

を設計します。

## 36. Localization

表示文字列を直接Assetへ保存すると言語ごとにAssetを増やすか、runtime置換が必要です。

- stable localization key。
- LocalizedString等Package型。
- fallback。
- format argument。
- font/glyph。

Definitionは「何を表示するかのkey」、UIは「現在言語でresolve」を担当します。

## 37. Balance Data

Attack damage等をScriptableObjectで管理するとInspector編集しやすい一方、大量行の横比較・一括編集はSpreadsheet/CSV/Databaseが向く場合があります。

Pipeline:

```text
Source of Truth
Spreadsheet / JSON / DB
        ↓ importer
Generated/Updated ScriptableObject Assets
        ↓ validation
Runtime Build
```

手編集Assetと自動生成Assetを混ぜず、どちらがSource of Truthか明記します。

## 38. Float秒とtick

Animation durationを秒、Combatをtickで持つ場合、変換を一箇所にします。

```csharp
public static int SecondsToTicks(
    float seconds,
    int ticksPerSecond)
{
    return Mathf.Max(
        0,
        Mathf.RoundToInt(seconds * ticksPerSecond));
}
```

丸め規則、0 tick active禁止、Animation speed変更、60Hz以外をvalidatorで確認します。

## 39. VersioningとMigration

Asset schema変更:

- field rename: `FormerlySerializedAs`。
- type変更。
- data分割。
- ID変更。
- list→sub-asset。
- old default更新。

```csharp
[SerializeField, HideInInspector]
private int dataVersion = 1;
```

Editor migration toolは:

1. 対象versionをscan。
2. Undo/backup。
3. deterministic変換。
4. validate。
5. diff review。
6. version更新。

を行います。

## 40. Inspectorの値とcode default

```csharp
[SerializeField] private int damage = 10;
```

既存Assetに`damage=10`が保存済みなら、code defaultを20へ変えても自動で20になりません。新規Asset defaultと既存data migrationを区別します。

## 41. Asset rename・move

Unity Editor内でmove/renameし`.meta`とGUIDを維持します。OS Explorerでmetaを失うと参照切れになります。

Version Control:

- Assetとmetaを一緒にcommit。
- Force Text。
- Visible Meta Files。
- Smart Merge等。
- GUID conflictを検出。

## 42. Runtime null

Asset referenceも`UnityEngine.Object`なので特殊なnull semanticsがあります。Addressable release、Scene unload、Editor recompilation等の寿命を考えます。

必須referenceは起動後に毎framenull checkするより、Build/Bootstrap validationで早期failさせます。

## 43. Test

### Pure rule test

DefinitionからRuntime Specへ変換し、damage/tick ruleをUnity Scene無しでtestします。

### Asset validation test

Project内全Definitionをscanし:

- null。
- duplicate ID。
- range。
- graph cycle。
- Prefab component。
- Animator Parameter。

を検査します。

### Play Mode test

AssetからPrefabをspawnし、Animation/VFX/Addressableまで統合確認します。

## 44. Data diff

YAML Asset diffは数値変更をreviewできますが、list reorderやmanaged referenceで大きなdiffになる場合があります。

- stable ordering。
- unique ID。
- 自動sortの時期。
- generated fieldを分離。
- one asset per logical definition。
- massive nested Assetを避ける。

Merge conflictを減らす設計もdata architectureの一部です。

## 45. Character交代用data

```text
Party Slot
├─ Character stable ID
├─ current HP/energy（runtime/save）
└─ cooldown state

Character Registry
└─ stable ID → CharacterDefinition
                    ├─ Prefab
                    ├─ Attacks
                    ├─ Camera Profile
                    └─ Presentation references
```

交代時はID/Definitionから新しいruntime Characterを組み立てます。Definition Assetへ現在Character flagを書きません。

## 46. Combo Graph

```csharp
[System.Serializable]
public sealed class ComboEdge
{
    [SerializeField] private AttackDefinition from;
    [SerializeField] private AttackDefinition to;
    [SerializeField] private int earliestCancelTick;

    public AttackDefinition From => from;
    public AttackDefinition To => to;
    public int EarliestCancelTick => earliestCancelTick;
}
```

Validation:

- from/to null。
- 同じedge重複。
- cancel tickがfrom total外。
- 到達不能Attack。
- 意図しないcycle。
- input command mapping漏れ。

Animation Controller transitionではなくCombat graphをdataとして検証できます。

## 47. Enemy/Boss data

分離:

- Enemy Definition: Prefab、base stats、senses。
- Ability Definition。
- AI Brain/Behaviour Tree Asset。
- Phase Definition。
- Loot Definition。
- Presentation Profile。

巨大`BossDefinition`一つへ全てnestedすると、変更衝突とload dependencyが増えます。責務Assetをcompositionします。

## 48. よくある不具合

- Current HPをScriptableObject Assetへ保存する。
- ScriptableObjectを製品版Save Fileだと思う。
- Play Mode中のAsset変更を残す。
- cloneが全参照をdeep copyすると考える。
- plain runtime stateまでScriptableObjectにする。
- OnEnableを一度だけのBootstrapと思う。
- OnValidateでScene生成やAsset全scanを行う。
- 重大Errorを無言でclampする。
- Asset名/pathを永続IDにする。
- Registry ID重複を後勝ちで隠す。
- Event Channelをglobal busとして乱用する。
- runtime listenerを解除しない。
- Assetが循環参照する。
- heavy Prefab/VFXのhard referenceでmemoryへ全部loadする。
- code default変更で既存Assetも更新されたと思う。
- Editor APIをruntime assemblyへ入れる。

## 49. Test Matrix

| 観点 | Test |
|---|---|
| Asset | 新規、既存、rename、move、duplicate |
| ID | empty、duplicate、旧version、migration |
| Reference | null、missing、cycle、heavy dependency |
| Runtime | 1体、同定義複数体、pool、Scene reload |
| Editor | Undo、multi-edit、Play中変更、Domain Reload無効 |
| Save | unknown ID、old ID、missing content |
| Build | Editor API混入、strip、Addressable mode |
| Data | min/max、0 tick、invalid combo |

## 50. 設計チェックリスト

- Static Definition、Runtime State、Save Dataを分離したか。
- Assetをruntimeで変更しない規則があるか。
- stable IDはrename/moveから独立しているか。
- ID重複をCIで検出するか。
- 共有すべきdataだけScriptableObjectにしたか。
- 小さい所有値はinline、共有値はAssetにしたか。
- Asset dependency方向が一方向か。
- OnValidateを軽いlocal validationへ限定したか。
- Project全体validatorとBuild gateがあるか。
- Editor/runtime assemblyを分離したか。
- Asset schema migration手順があるか。
- hard referenceによるmemory loadを確認したか。
- Character/Attack dataからruntime snapshotを作れるか。
- Save Dataはstable IDからDefinitionをresolveするか。

## 公式資料

- [Unity Manual: ScriptableObject](https://docs.unity3d.com/6000.0/Documentation/Manual/class-ScriptableObject.html)
- [Unity API: ScriptableObject](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/ScriptableObject.html)
- [Unity API: CreateAssetMenuAttribute](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/CreateAssetMenuAttribute.html)
- [Unity API: ScriptableObject.OnEnable](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/ScriptableObject.OnEnable.html)
- [Unity API: MonoBehaviour.OnValidate](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/MonoBehaviour.OnValidate.html)
- [Unity Manual: Serialization rules](https://docs.unity3d.com/6000.0/Documentation/Manual/script-serialization-rules.html)
- [Unity API: SerializeReference](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/SerializeReference.html)
- [Unity Manual: How Unity uses serialization](https://docs.unity3d.com/6000.0/Documentation/Manual/script-serialization-how-unity-uses.html)
- [Unity API: SerializedObject](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/SerializedObject.html)
- [Unity Manual: Asset metadata](https://docs.unity3d.com/6000.0/Documentation/Manual/AssetMetadata.html)

