# Save・Settings・Localization

> 対象: Unity 6.0、Localization 1.5系。保存先、同期API、cloud機能、console要件はplatformごとに異なるため、最終的には対象platformの仕様を確認すること。

## 1. 三つの責務を分ける

```text
Save Data
  └─ game進行、所持品、解放状態、checkpoint

Settings
  └─ 音量、画質、入力、字幕、言語、accessibility

Localization
  └─ 翻訳文字列、locale別asset、書式、font、layout
```

Settingsも保存されますが、game進行と更新頻度、適用先、破損時のdefault、account共有範囲が違います。一つの巨大`GameData`へ全部詰めないでください。

## 2. Save Dataの要件

- process終了後も残る。
- application更新後も読める。
- 書き込み途中の終了で壊れにくい。
- 旧versionから移行できる。
- 壊れた時に検出・復旧できる。
- 複数slot/profileを扱える。
- platformの保存規則に従う。
- gameplay threadを長時間止めない。
- debug可能だが秘密情報を漏らさない。

保存は`File.WriteAllText`一行だけではなく、data contractとlifecycleの設計です。

## 3. 保存してよいもの

結果を保存し、導出可能な一時状態は原則として再構築します。

保存候補:

- player IDではないlocal profile ID。
- story進行flag。
- 解放済みstage/character。
- inventoryのitem IDと個数。
- checkpoint ID。
- settings。
- schema version。
- save sequence/timestamp。

保存を避ける候補:

- Scene内GameObject参照。
- runtime instance ID。
- `Transform`やComponentそのもの。
- Addressables handle。
- animationの内部一時state。
- cache可能な集計値。
- access token、秘密鍵。

## 4. Stable ID

Prefab名やlist indexを永続IDにするとrename/reorderで壊れます。

```csharp
// 保存されるdataは、このstable IDを通してruntime definitionへ解決する。
// 例: "character.hero_a"、"item.heal_small"。
public readonly record struct ContentId(string Value);
```

IDを公開後に変更する場合はalias/migration tableを用意します。重複IDはbuild時に検査します。

## 5. DTOとruntime modelを分ける

DTOはdisk形式、runtime modelはgameの正しい状態です。

```text
JSON / Binary
  ↓ deserialize
Save DTO（不正値を含む可能性）
  ↓ validate・migrate・normalize
Runtime Model（invariantを満たす）
```

deserializeしたobjectを無検証でgameplayへ渡しません。

## 6. Save DTO例

```csharp
using System;
using System.Collections.Generic;

[Serializable]
public sealed class SaveFileDto
{
    // schemaVersionはapplication versionではない。
    // 保存形式を変更するたびにmigration判断へ使う番号。
    public int schemaVersion;

    // 書き込みごとに増やす。main/backupの新旧判定に使える。
    public long sequence;

    // UTCのISO 8601文字列など、timezoneに依存しない形式を使う。
    public string savedAtUtc;

    public ProgressDto progress = new();
    public List<InventoryEntryDto> inventory = new();
}

[Serializable]
public sealed class ProgressDto
{
    public string checkpointId = "checkpoint.start";
    public List<string> unlockedContentIds = new();
}

[Serializable]
public sealed class InventoryEntryDto
{
    public string itemId = string.Empty;
    public int quantity;
}
```

field initializerがdeserialize方式によって期待通り働くかをtestします。欠落field、`null`、未知fieldを含む旧/新dataも入力してください。

## 7. JsonUtilityの特徴

`JsonUtility`はUnity serialization規則に近い軽量JSON APIです。

- `[Serializable]`な型とfield中心。
- propertyや一般的な`Dictionary`等に制約。
- polymorphismや複雑なschemaには不向き。
- human-readableでdebugしやすい。
- private fieldは`[SerializeField]`等の規則を確認。

```csharp
string json = UnityEngine.JsonUtility.ToJson(dto, prettyPrint: true);
SaveFileDto loaded = UnityEngine.JsonUtility.FromJson<SaveFileDto>(json);
```

JSONだから安全、互換性が自動、改ざん不可という意味ではありません。

## 8. Format選択

### JSON

読みやすくmigration/debugしやすい。file size、parse allocation、型表現に注意。

### Binary

compact/high-speedにできるが、format設計、endianness、version、toolが必要。`BinaryFormatter`は安全性上使いません。

### Database

大量record、query、transactionに向く場合があるが、導入とschema migrationが増えます。

小さなsingle-player saveなら、version付きJSON + atomic replaceから始めるのが理解しやすい選択です。

## 9. 保存先

Unityでは通常`Application.persistentDataPath`配下を使います。

```csharp
using System.IO;
using UnityEngine;

string saveDirectory = Path.Combine(Application.persistentDataPath, "Saves");
string savePath = Path.Combine(saveDirectory, "slot_00.json");
```

`Application.dataPath`はPlayer data/application bundle側で、platformによってread-onlyです。current directoryやAssets folderへ保存しません。

Bundle Identifierを変えるとplatform上のpersistent path継続性へ影響する場合があります。

## 10. Pathを外部入力から作らない

slot名に`../`や絶対pathを許すと保存directory外へ出られます。

```csharp
private static string GetSlotFileName(int slot)
{
    if (slot is < 0 or > 9)
    {
        throw new System.ArgumentOutOfRangeException(nameof(slot));
    }

    // 数値から内部生成し、user入力をfile名へ直接使わない。
    return $"slot_{slot:D2}.json";
}
```

profile表示名と保存file名を分離します。

## 11. Atomic saveの考え方

直接main fileへ上書き中にpower lossすると、途中までのfileだけが残ります。

```text
1. DTO snapshot作成
2. serialize
3. temp fileへwrite
4. flush/close
5. tempを検証
6. mainをbackupへ
7. tempをmainへatomic replace/rename
8. directory state更新
```

atomic rename/replaceの保証はfilesystem/platformで異なります。console SDKのsave APIがあるならその契約へ従います。

## 12. Save repository例

```csharp
using System;
using System.IO;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using UnityEngine;

public sealed class JsonSaveRepository
{
    private readonly string directory;
    private readonly SemaphoreSlim gate = new(1, 1);

    public JsonSaveRepository(string directory)
    {
        this.directory = directory;
    }

    public async Task SaveAsync(
        int slot,
        SaveFileDto snapshot,
        CancellationToken cancellationToken)
    {
        // 同じrepositoryへの同時書き込みを直列化し、temp/main競合を防ぐ。
        await gate.WaitAsync(cancellationToken);

        try
        {
            Directory.CreateDirectory(directory);

            string mainPath = Path.Combine(directory, GetSlotFileName(slot));
            string tempPath = mainPath + ".tmp";
            string backupPath = mainPath + ".bak";

            // Unity objectをbackground threadで読むのではなく、main thread側で
            // 作成済みのpure DTO snapshotだけをserializeする。
            string json = JsonUtility.ToJson(snapshot, prettyPrint: false);
            byte[] bytes = new UTF8Encoding(false).GetBytes(json);

            // FileOptions.WriteThrough等の対応/効果はplatformで検証する。
            await using (var stream = new FileStream(
                tempPath,
                FileMode.Create,
                FileAccess.Write,
                FileShare.None,
                bufferSize: 16 * 1024,
                useAsync: true))
            {
                await stream.WriteAsync(bytes, cancellationToken);
                await stream.FlushAsync(cancellationToken);
            }

            // tempが最低限deserialize可能か確認してからmainへ昇格する。
            ValidateSerializedFile(tempPath);

            if (File.Exists(mainPath))
            {
                // File.Replaceのplatform対応とatomicityを実機確認する。
                File.Replace(tempPath, mainPath, backupPath);
            }
            else
            {
                File.Move(tempPath, mainPath);
            }
        }
        finally
        {
            gate.Release();
        }
    }

    private static string GetSlotFileName(int slot)
    {
        if (slot is < 0 or > 9)
        {
            throw new ArgumentOutOfRangeException(nameof(slot));
        }

        return $"slot_{slot:D2}.json";
    }

    private static void ValidateSerializedFile(string path)
    {
        string json = File.ReadAllText(path, Encoding.UTF8);
        SaveFileDto dto = JsonUtility.FromJson<SaveFileDto>(json);

        if (dto == null || dto.schemaVersion <= 0)
        {
            throw new InvalidDataException("Temporary save validation failed.");
        }
    }
}
```

これは学習用の骨格です。`File.Replace`の可否、flush保証、cloud/container APIは対象platform向けに抽象化します。

## 13. Main threadとsnapshot

Unity APIとUnity Objectはmain thread制約があります。background threadへScene objectを渡して巡回させません。

```text
Main thread
  Runtime Modelからimmutable DTO snapshotを生成
            ↓
Worker / async I/O
  serialize + write
            ↓
Main thread
  completion通知
```

snapshot作成中にgame stateが変わらないよう、command境界、copy、lockではなくgame loop phase等で整合させます。

## 14. Save requestのcoalescing

1 frameに10回save要求が来ても10回disk書き込みする必要は通常ありません。

```text
Request A ─┐
Request B ─┼→ Dirty flag → 最新snapshotを1回保存
Request C ─┘
```

ただしcheckpoint確定やpurchase相当の重要eventでは、完了を待つ必要があるかを設計します。

## 15. Autosave timing

候補:

- checkpoint到達。
- stage clear。
- inventory変更をdebounce。
- settings変更。
- application pause/focus loss。
- 正常終了。

終了時だけに依存しません。mobile suspend後に終了callbackが十分な時間動く保証はありません。

## 16. PauseとQuit

`OnApplicationPause`、`OnApplicationFocus`、`OnApplicationQuit`の呼ばれ方はplatformで異なります。

- pause時には小さいsnapshotを素早く保存。
- 長いnetwork uploadを終了eventだけで始めない。
- write中の再入を防ぐ。
- timeout/中断後もtemp fileから復旧可能にする。

実機でhome button、強制終了、battery切れ相当を試験します。

## 17. Load pipeline

```text
file存在確認
 → bytes読込
 → size上限確認
 → checksum/auth検証
 → deserialize
 → schema判定
 → migration
 → semantic validation
 → normalize
 → runtime model生成
```

deserialize成功はdataが正しいという意味ではありません。quantityが負、未知ID、同一item重複等を検証します。

## 18. Validation例

```csharp
public static void ValidateAndNormalize(SaveFileDto dto)
{
    if (dto == null)
    {
        throw new InvalidDataException("Save data is null.");
    }

    if (dto.inventory == null)
    {
        dto.inventory = new List<InventoryEntryDto>();
    }

    foreach (InventoryEntryDto entry in dto.inventory)
    {
        if (string.IsNullOrWhiteSpace(entry.itemId))
        {
            throw new InvalidDataException("Inventory item ID is empty.");
        }

        // game ruleに合わせた上限。int overflowや極端値をruntimeへ入れない。
        entry.quantity = Math.Clamp(entry.quantity, 0, 9999);
    }
}
```

不正値を黙って全部補正するか、slot全体を拒否するかはdata重要度ごとに決めます。

## 19. Schema version

```text
Application version: 1.8.3
Save schema version: 7
Content catalog version: 2026.08.14
```

別々に管理します。application patchでsave形式が変わらないことも、同じapplication version中にcontentだけ変わることもあります。

## 20. Migration chain

```csharp
public static SaveFileDto MigrateToCurrent(SaveFileDto dto)
{
    const int currentVersion = 3;

    while (dto.schemaVersion < currentVersion)
    {
        dto = dto.schemaVersion switch
        {
            1 => MigrateV1ToV2(dto),
            2 => MigrateV2ToV3(dto),
            _ => throw new InvalidDataException(
                $"Unsupported schema: {dto.schemaVersion}")
        };
    }

    if (dto.schemaVersion > currentVersion)
    {
        // 新しいapplicationで作られたsaveを古いapplicationが壊して上書きしない。
        throw new InvalidDataException("Save data is newer than this build.");
    }

    return dto;
}
```

各migrationは入力versionを限定し、出力versionを必ず一つ進め、fixtureでtestします。

## 21. Migrationでrenameを扱う

```csharp
private static readonly Dictionary<string, string> ItemIdAliases = new()
{
    ["potion_small"] = "item.heal_small",
    ["coin"] = "currency.soft"
};

private static string MigrateItemId(string oldId)
{
    return ItemIdAliases.TryGetValue(oldId, out string newId)
        ? newId
        : oldId;
}
```

公開済みIDを削除しただけにせず、未知IDをdrop、placeholder化、errorのどれにするか決めます。

## 22. BackupとRecovery

load順の一例:

1. mainを検証。
2. mainが壊れていればbackupを検証。
3. tempがmainより新しく完全なら復旧候補。
4. userへ復旧結果を通知。
5. 両方失敗なら新規dataを作る前に壊れたfileを隔離。

壊れたfileを即上書きすると調査と復旧可能性を失います。

## 23. Checksumと改ざん

Checksum/CRCは偶発的破損の検出に使えますが、攻撃者はdataとchecksumを両方変更できます。

HMACはsecretが必要ですが、client内secretは解析され得ます。競争性や課金に関わるauthoritative dataはserver側を正とし、local saveの暗号化だけを信頼しません。

暗号化は機密性、署名/HMACは真正性、checksumは破損検出で目的が違います。

## 24. Cloud save conflict

```text
Device A: sequence 105, played offline
Device B: sequence 103, different progress
Cloud:    sequence 104
```

timestampだけで後勝ちにするとclockずれや別進行を消します。

選択肢:

- server revision + optimistic concurrency。
- field別merge可能なCRDT/operation log。
- userへslot選択。
- inventory等はserver authoritative。
- conflict copyを残す。

local save repositoryとcloud sync serviceを分けます。

## 25. PlayerPrefsの用途

`PlayerPrefs`は少量の`int`、`float`、`string`を保存する簡易key-valueです。

向く例:

- 初回起動済みflag。
- 小さなgraphics option。
- 選択locale ID。

向かない例:

- 大規模game進行。
- inventory list。
- 機密情報。
- transactionが必要な複数field。

platformごとにregistry/file/browser storage等の実装と容量制限が異なります。

## 26. PlayerPrefsのkey管理

```csharp
public static class PreferenceKeys
{
    public const string MasterVolume = "settings.audio.master.v1";
    public const string Locale = "settings.locale.v1";
    public const string ScreenMode = "settings.video.screen_mode.v1";
}
```

文字列をUI component各所へ散らさず、default、version、migrationをSettings Repositoryへ集約します。

## 27. Settings model

```csharp
using System;

[Serializable]
public sealed class GameSettings
{
    public int schemaVersion = 1;

    // UIでは0..100を表示しても、domainでは0..1へ正規化する。
    public float masterVolume = 1f;
    public float musicVolume = 0.8f;
    public float sfxVolume = 0.8f;

    public int qualityLevel = 2;
    public bool subtitles = true;
    public float cameraSensitivity = 1f;
    public bool reduceCameraShake;
    public string localeCode = "system";
}
```

Settingsもschema versionとvalidationを持ちます。ただし壊れた場合は安全なdefaultへ戻せる設計にします。

## 28. Settingsの三段階

```text
Stored Settings
  diskに確定済み

Editing Settings
  options画面で変更中

Applied Settings
  Engine/Systemsへ現在反映中
```

resolution等は「適用→15秒以内に確認→確認なしなら元へ戻す」が必要です。sliderはpreview反映し、Cancelでsnapshotへ戻す設計もあります。

## 29. Settings service

```csharp
public interface ISettingsApplier
{
    void ApplyAudio(GameSettings settings);
    void ApplyGraphics(GameSettings settings);
    void ApplyGameplay(GameSettings settings);
    void ApplyAccessibility(GameSettings settings);
}

public sealed class SettingsService
{
    private readonly ISettingsApplier applier;
    private GameSettings stored;
    private GameSettings editing;

    public SettingsService(ISettingsApplier applier, GameSettings initial)
    {
        this.applier = applier;
        stored = Clone(initial);
        editing = Clone(initial);
    }

    public void Preview(System.Action<GameSettings> edit)
    {
        edit(editing);
        Normalize(editing);
        applier.ApplyAudio(editing);
        applier.ApplyGameplay(editing);
    }

    public void Cancel()
    {
        editing = Clone(stored);
        ApplyAll(editing);
    }

    public void Confirm()
    {
        stored = Clone(editing);
        // repositoryへ非同期保存要求を送る。
    }

    private void ApplyAll(GameSettings value)
    {
        // 設定categoryを担当systemへまとめて反映する。
        applier.ApplyAudio(value);
        applier.ApplyGraphics(value);
        applier.ApplyGameplay(value);
        applier.ApplyAccessibility(value);
    }

    private static GameSettings Clone(GameSettings value)
    {
        // この小さなserializable DTOではJSON round-tripでdeep copyする例。
        // 高頻度処理ではallocationを測り、copy constructor等へ置き換える。
        string json = UnityEngine.JsonUtility.ToJson(value);
        return UnityEngine.JsonUtility.FromJson<GameSettings>(json);
    }

    private static void Normalize(GameSettings value)
    {
        // disk/旧version/UIから範囲外値が来てもEngineへ渡さない。
        value.masterVolume = UnityEngine.Mathf.Clamp01(value.masterVolume);
        value.musicVolume = UnityEngine.Mathf.Clamp01(value.musicVolume);
        value.sfxVolume = UnityEngine.Mathf.Clamp01(value.sfxVolume);
        value.qualityLevel = UnityEngine.Mathf.Max(0, value.qualityLevel);
        value.cameraSensitivity = UnityEngine.Mathf.Clamp(
            value.cameraSensitivity,
            0.1f,
            5f);
    }
}
```

`Clone`で同一参照を返すと、editingの変更がstoredにも入ってCancel不能です。この例は理解しやすさのためJSON round-tripを使いますが、実装ではcopy constructorも選べます。

## 30. Volumeとdecibel

UIのlinear値をAudioMixerのdBへ変換します。

```csharp
private static float LinearToDecibels(float linear)
{
    // log10(0)を避け、mute相当の下限へclampする。
    float safe = UnityEngine.Mathf.Clamp(linear, 0.0001f, 1f);
    return 20f * UnityEngine.Mathf.Log10(safe);
}
```

0を完全muteとして別flag/下限dBにし、slider知覚が自然になるcurveを使います。

## 31. Graphics settings

- resolution。
- display mode。
- refresh rate。
- VSync/frame cap。
- render scale。
- shadow quality/distance。
- texture quality。
- anti-aliasing。
- post processing。
- quality presetとcustom状態。

全設定が全platformで変更可能とは限りません。非対応項目はUIを隠すか説明します。

## 32. Gameplay・Accessibility settings

- camera sensitivity/invert。
- aim assist。
- camera shake量。
- motion blur。
- hit stop/flash軽減。
- subtitle、speaker name、size、background。
- colorだけに依存しないUI。
- hold/toggle。
- rapid input代替。
- audio dynamic range。
- vibration。

accessibilityは後付けの一つのcheckboxではなく、camera、UI、audio、input、VFXへ跨る設定です。

## 33. Input binding save

New Input Systemのbinding overrideをJSON等へ保存できますが、action/binding GUIDやasset変更との互換性を考えます。

- device scheme別。
- duplicate/conflict検査。
- reserved input。
- reset to default。
- binding asset更新後のmissing override。
- glyph表示更新。

game進行saveと別file/sectionにし、controller故障で操作不能でも初期化可能にします。

## 34. Localizationは翻訳だけではない

- string。
- plural/grammar。
- number/date/currency formatting。
- font/glyph。
- text direction。
- line break/layout。
- voice/image/texture。
- input glyph。
- cultural表現。
- legal text。
- subtitle timing。

日本語文を英単語へ置換するだけでは完成しません。

## 35. Localeとlanguage

`en`と`en-US`、`pt-BR`、`zh-Hans`等、language・region・scriptを区別します。保存には表示名ではなくstable locale identifierを使います。

```text
System language
 → supported localeへmatch
 → userが明示選択していればそれを優先
 → fallback locale
```

`English`という翻訳された表示文字列を識別keyにしません。

## 36. Localization package

Unity 6.0ではLocalization 1.5系がreleased packageです。

主な要素:

- Localization Settings。
- Locale。
- Locale Selector。
- String Database / String Table。
- Asset Database / Asset Table。
- `LocalizedString` / `LocalizedAsset<T>`。
- Smart Strings。
- Pseudo-localization。
- CSV/XLIFF/Google Sheets等のworkflow。

## 37. Initialization

Localization初期化にはlocaleやpreload tableの準備が含まれ、基本は非同期です。起動画面で完了を待つか、localization済みUIが表示される前に準備します。

同期初期化はmain threadをblockし、WebGL等で制約があるため、安易に有効化せずloading phaseを設計します。

## 38. Locale selection

```csharp
using System.Linq;
using System.Threading.Tasks;
using UnityEngine.Localization;
using UnityEngine.Localization.Settings;

public static class LocaleService
{
    public static async Task<bool> SelectAsync(string localeCode)
    {
        // Localization systemの非同期初期化完了を待つ。
        await LocalizationSettings.InitializationOperation.Task;

        Locale locale = LocalizationSettings.AvailableLocales.Locales
            .FirstOrDefault(x => x.Identifier.Code == localeCode);

        if (locale == null)
        {
            return false; // 未対応localeを無理に設定しない。
        }

        LocalizationSettings.SelectedLocale = locale;
        return true;
    }
}
```

locale変更直後はlocalized Assetの非同期loadもあり得ます。画面の半分だけ旧言語になる中間状態をどう見せるか決めます。

## 39. String Table

Collection例:

```text
UI_Common
  button.confirm
  button.cancel
  error.network.timeout

Battle
  hud.combo
  result.clear_time
  tutorial.dodge
```

keyへ原文を使うと原文修正がidentity変更になります。意味を表すstable keyを使い、translator向けcontext/metadataを付けます。

## 40. LocalizedString

```csharp
using UnityEngine;
using UnityEngine.Localization;
using TMPro;

public sealed class LocalizedLabel : MonoBehaviour
{
    [SerializeField] private LocalizedString textReference;
    [SerializeField] private TMP_Text target;

    private void OnEnable()
    {
        // locale変更やtable load完了時にも更新されるeventを購読する。
        textReference.StringChanged += OnStringChanged;
    }

    private void OnDisable()
    {
        // 再Enableのたびに重複購読しないよう対称に解除する。
        textReference.StringChanged -= OnStringChanged;
    }

    private void OnStringChanged(string localizedValue)
    {
        target.text = localizedValue;
    }
}
```

`async`で一度だけ取得して終わる方式と、locale変更へ追従するevent方式を用途で分けます。

## 41. Smart String

文字列をcodeで連結しないで、翻訳側が語順を変えられるtemplateにします。

悪い例:

```csharp
label.text = playerName + " obtained " + count + " items";
```

考え方:

```text
Key: result.item_obtained
English: {player} obtained {count} items.
Japanese: {player}はアイテムを{count}個獲得した。
```

plural、choose、conditional、list formattingを言語ごとに設定します。英語の単数/複数ruleを全言語へ押し付けません。

## 42. Argumentsの更新

```csharp
[SerializeField] private LocalizedString resultMessage;

public void SetResult(string playerName, int itemCount)
{
    // Smart Stringのplaceholderへ渡す値。
    resultMessage.Arguments = new object[] { playerName, itemCount };

    // 引数変更を通知し、表示購読者へ再評価させる。
    resultMessage.RefreshString();
}
```

reflection based selectorは便利ですが、renameでruntime errorになり得ます。重要UIはtestし、translatorへplaceholderの意味と型を伝えます。

## 43. 数値と日時

`ToString()`のdefault cultureへ任せません。

- decimal separator。
- digit grouping。
- percent位置。
- date順序。
- 12/24時間。
- timezone。
- 単位系。

saveにはculture-neutral形式、表示時にselected locale形式を使います。表示文字列を再parseしてgameplay値へ戻さないでください。

## 44. String Table preload

初回表示でtable load待ちが発生する場合があります。

- boot/error/common UIはpreload候補。
- 全table preloadはstartup memory/time増加。
- stage固有tutorialはstage前load候補。
- locale変更時のload peakを測る。

Addressablesと同様、必要集合と寿命を設計します。

## 45. Asset Table

locale別に差し替え得るAsset:

- text入りtexture。
- tutorial image。
- voice clip。
- locale固有font。
- legal image。
- video。

できる限りimageへ文字を焼き込まず、text componentで再利用します。Asset差替えが必要ならfile sizeとAddressables dependencyを確認します。

## 46. Voice localization

```text
Dialogue ID
 ├─ localized subtitle string
 ├─ localized voice AudioClip
 ├─ speaker display name
 ├─ timing/marker
 └─ fallback policy
```

subtitleとvoiceを別々のindexで管理してずれないよう、stable dialogue IDで束ねます。audio languageとtext languageを別設定にするgameでは二つのlocale選択を持ちます。

## 47. FontとGlyph

一つのLatin font assetだけでは日本語、中国語、韓国語、Arabic等を表示できません。

- TMP Font Assetのatlas mode。
- fallback chain。
- dynamic/static atlas。
- glyph coverage。
- atlas texture memory。
- font license。
- bold/italic/weight。
- emoji/symbol/input glyph。

missing glyphの□を自動test/screenshotsで検出します。

## 48. CJK font memory

CJK全glyphを巨大atlasへ詰めるとmemory/build sizeが増えます。

選択肢:

- 使用文字setを生成してstatic atlas。
- dynamic atlas + fallback。
- locale別font asset/addressable group。
- common UIとrare glyphを分割。

Smart Stringのplaceholderから入るplayer名など、table literalだけを走査しても全必要文字は得られない点に注意します。

## 49. Right-to-left

Arabic/Hebrew等では単に文字列を右寄せするだけではありません。

- bidi algorithm。
- shaping。
- punctuation/number混在。
- iconとtext順序。
- horizontal layout反転。
- controller glyph配置。
- animation方向が意味を持つ場合。

利用するText renderer/packageの対応を確認し、native speaker/linguistic QAを行います。

## 50. Layout expansion

翻訳後の長さは大きく変わります。

- fixed width buttonへ文字を詰めない。
- wrapping/min/max size。
- auto sizeの下限。
- ScrollView。
- safe area。
- 200%程度のpseudo expansion test。
- subtitleの最大行数。

fontを極端に小さくして押し込むのはaccessibilityを損ないます。

## 51. Pseudo-localization

翻訳完成前に次を検出します。

- hard-coded string。
- clipping/overflow。
- glyph不足。
- concatenation。
- locale変更追従漏れ。
- text画像。
- layout方向問題。

accent追加、文字伸長、bracket付加等で未対応箇所を目立たせます。pseudo localeは実翻訳品質の代わりではありません。

## 52. Translator context

keyと原文だけでは誤訳を招きます。

渡す情報:

- 画面snapshot。
- speakerと相手。
- 性別/人数/敬語。
- character制限。
- placeholder説明。
- buttonか文章か。
- 用語集。
- 禁則語・表記規則。
- voice尺。

同じ英語`Charge`でも、料金、突撃、溜め攻撃で意味が違います。

## 53. Import・Export workflow

CSV、XLIFF、Google Sheets等を使う場合も、table assetと外部sourceのどちらを正とするか決めます。

```text
Export
 → Translator編集
 → Validation
 → Import
 → Unity diff review
 → Automated checks
 → Linguistic QA build
```

key列やmetadataをspreadsheetのsort/copyで壊さないよう、stable IDとvalidationを使います。

## 54. Localization validation

- 全localeに必須keyがある。
- 空文字とmissingを区別。
- placeholder集合がsourceと一致。
- Smart String parse成功。
- duplicate keyなし。
- forbidden hard-coded UI stringを検出。
- font coverage。
- asset table entry type一致。
- audio file存在/長さ。
- character limit超過。

Editor toolやCIで自動化します。

## 55. Missing translation policy

候補:

- project localeへfallback。
- keyを目立つ形式で表示。
- 空欄禁止。
- development buildだけ警告。
- telemetryでmissing keyを収集。

productionで静かに空文字にするとbuttonの意味が消えます。何が欠けたか追跡可能にします。

## 56. Locale change eventの波及

```text
SelectedLocaleChanged
 ├─ localized strings refresh
 ├─ localized assets reload
 ├─ font/fallback change
 ├─ number/date formatter change
 ├─ layout再計算
 ├─ voice language policy
 └─ Settings save
```

変更中にAssetが非同期loadされるため、loading overlay、旧Asset保持、全準備後swap等のtransactionを検討します。

## 57. LocaleとSettingsの起動順

```text
1. 最小local boot UI
2. Settings読込
3. 保存localeを検証
4. Localization初期化
5. selected locale設定
6. common table/font preload
7. localized title表示
```

Localizationを先にsystem languageで初期化してからsave localeへ変えると、二重loadや一瞬の言語切替が起こり得ます。

## 58. Save中にlocalized文字列を保存しない

悪い例:

```json
{ "equippedWeapon": "炎の剣" }
```

正しい方向:

```json
{ "equippedWeaponId": "weapon.fire_sword" }
```

表示時だけString Tableで名前へ変換します。localeを変えてもsave identityは変わりません。

## 59. Error message設計

低層errorをそのままuserへ出しません。

```text
Internal: SaveIOException(path, HResult, operation)
Domain:   SaveWriteFailed(slot, recoverable=true)
UI key:   error.save.write_failed
```

developer logには診断情報、UIにはlocalizableで安全な説明を渡します。pathやaccount tokenを画面/logへ露出しません。

## 60. Save test

- 新規fileなし。
- 正常file。
- 空file。
- JSON途中切れ。
- schema旧版ごとのfixture。
- schema未来版。
- unknown field。
- missing field/null list。
- quantity境界/overflow。
- main破損 + backup正常。
- tempのみ残存。
- disk full。
- permission denied。
- write中強制終了。
- 1000回save/load round trip。

## 61. Settings test

- default値。
- 全slider最小/最大。
- invalid persisted値のclamp。
- Apply/Cancel/Confirm。
- resolution確認timeout。
- audio mixer反映。
- device非対応option。
- input binding conflict/reset。
- locale未対応code fallback。
- application再起動後の一致。

## 62. Localization test matrix

| 条件 | 確認 |
|---|---|
| 各Locale | missing key/assetなし |
| Long text | clippingしない |
| CJK | glyphとline break正常 |
| RTL | shaping/layout正常 |
| Plural 0/1/2/large | 文法が正しい |
| Dynamic player name | glyph/escape/長さに耐える |
| Runtime locale change | 全UI/assetが更新 |
| Scene transition後 | event購読が重複しない |
| Offline | 必要table/fontを取得可能 |
| Pseudo locale | hard-coded textを発見できる |

## 63. Performance

- save snapshot作成時間。
- serialization allocation。
- file write size/time。
- compression/encryption cost。
- table preload時間とmemory。
- locale変更時Asset peak。
- font atlas増加。
- layout rebuild spike。

毎frameJSON化しません。Profiler markerをphase別に入れます。

## 64. Thread safety

複数save request、cloud download、settings変更が同時に来ます。

- repository単位のserialization gate。
- immutable snapshot。
- sequence/revision。
- main threadへ結果をmarshal。
- cancel後のtemp cleanup。
- application終了中の新規request拒否。

`lock`中に長いI/Oやcallbackを実行しない設計にします。

## 65. Observability

記録候補:

- save schema version。
- operation typeとduration。
- bytes。
- success/failure category。
- recoveryにmain/backup/tempのどれを使ったか。
- migration path。
- selected locale。
- missing localization key count。

save本文、player名、会話内容等を不用意にtelemetryへ送りません。

## 66. Debug tools

- slot一覧とmetadata表示。
- JSON export/importはdevelopment限定。
- schema fixture loader。
- intentional corruption button。
- migration preview/diff。
- locale即時切替。
- missing translation highlight。
- pseudo locale。
- font glyph inspector。
- settings reset category別。

production buildで任意file importやcheat機能を残さないようcompile symbol/permissionを確認します。

## 67. Review checklist

- runtime modelとSave DTOを分けたか。
- stable content IDを保存したか。
- schema versionとmigration chainがあるか。
- temp/main/backupの復旧を扱うか。
- 保存中断を実機で試したか。
- user入力からpathを作っていないか。
- Unity Objectをworker threadで触っていないか。
- Settingsのstored/editing/appliedを区別したか。
- locale identifierを保存したか。
- UI文字列をcodeで連結していないか。
- Smart String placeholderを検査したか。
- font fallback/glyph coverageを確認したか。
- pseudo localeと長文でlayout検査したか。
- localized表示名をgameplay IDにしていないか。

## 68. 学習確認問題

1. application versionとsave schema versionを分ける理由は何か。
2. main fileへ直接上書きする危険は何か。
3. deserialize後にsemantic validationが必要な理由は何か。
4. PlayerPrefsへinventoryを保存しない方がよい理由は何か。
5. Settingsのeditingとappliedを分ける場面は何か。
6. localized item名ではなくstable IDを保存する理由は何か。
7. Smart Stringが文字列連結より優れる点は何か。
8. String Tableを全てpreloadするtrade-offは何か。
9. CJK font atlasのmemoryをどう管理するか。
10. runtime locale変更をtransactionとして考える理由は何か。

## 69. 公式資料

- [Unity Scripting API: Application.persistentDataPath](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/Application-persistentDataPath.html)
- [Unity Scripting API: PlayerPrefs](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/PlayerPrefs.html)
- [Unity Scripting API: JsonUtility](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/JsonUtility.html)
- [Unity Manual: JSON Serialization](https://docs.unity3d.com/6000.0/Documentation/Manual/json-serialization.html)
- [Unity Manual: Localization package](https://docs.unity3d.com/6000.0/Documentation/Manual/com.unity.localization.html)
- [Localization: Localization Settings](https://docs.unity3d.com/Packages/com.unity.localization@1.5/manual/LocalizationSettings.html)
- [Localization: String Tables](https://docs.unity3d.com/Packages/com.unity.localization@1.5/manual/StringTables.html)
- [Localization: Smart Strings](https://docs.unity3d.com/Packages/com.unity.localization@1.5/manual/Smart/SmartStrings.html)
- [Localization: Asset Tables](https://docs.unity3d.com/Packages/com.unity.localization@1.5/manual/AssetTables.html)
- [Localization: Pseudo-localization](https://docs.unity3d.com/Packages/com.unity.localization@1.5/manual/Pseudo-Localization.html)

## 70. まとめ

- Saveはversion付きdata contractであり、atomic write、backup、migration、validationまで含む。
- Unity Objectではなくstable IDとpure DTOを保存する。
- Settingsはstored、editing、appliedを分け、system別applierへ反映する。
- PlayerPrefsは少量の簡易設定向けで、複雑な進行dataのdatabaseではない。
- Localizationは文字列だけでなく、文法、Asset、font、layout、locale別書式を扱う。
- 表示名をidentityにせず、String/Asset Tableのstable keyを使う。
- locale変更、save中断、旧schema、長文、missing glyphを正常系と同じ重要度でtestする。
