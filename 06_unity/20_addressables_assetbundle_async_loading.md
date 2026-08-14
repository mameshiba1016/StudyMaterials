# Addressables・AssetBundle・非同期ロード・メモリ管理

> 対象: Unity 6.0、Addressables 2.7系。Package APIやBuild設定はVersionで変わるため、実際のProjectで固定したPackage Versionの公式資料も確認すること。

## 1. この章の到達点

この章では、次を区別して説明できる状態を目指します。

- C#の参照が消えること。
- UnityEngine.Objectを`Destroy`すること。
- Addressablesのhandleを`Release`すること。
- AssetBundle本体とBundle内Assetをunloadすること。
- download済みBundle cacheをdiskから消すこと。
- OSへmemory pageが返ること。

これらは同じ処理ではありません。`null`代入だけでtextureやmeshのnative memoryが直ちに解放される、とは限りません。

## 2. なぜ非同期ロードが必要か

大規模な3D action gameでは、全character、enemy、stage、voice、animation、VFXを起動時に常駐させられません。

```text
Bootに必要な最小content
  ↓
Title / Login
  ↓ download・catalog確認
Lobbyに必要なcontent
  ↓ preload
Battle stageに必要なcontent
  ↓ load・instantiate
Battle
  ↓ release
次の画面
```

非同期化の目的は「処理が速くなる」ことではありません。I/Oやdecode等の待ちをmain threadの長い停止にせず、進捗表示、cancel要求、失敗復旧を設計可能にすることです。

## 3. Asset参照方式の比較

### Inspectorの直接参照

```csharp
[SerializeField] private GameObject enemyPrefab;
```

簡潔で型安全ですが、参照先と依存AssetがPlayer buildやSceneの依存へ入りやすく、必要時だけremote配信する用途には不向きです。

### Resources

```csharp
GameObject prefab = Resources.Load<GameObject>("Enemies/SmallEnemy");
```

小規模な固定bootstrap用途では使えますが、文字列path、巨大化しやすいResources、粒度の粗い管理が問題になります。全contentをResourcesへ集めないでください。

### AssetBundleを直接管理

Bundleのbuild、依存、download、version、cache、unloadを自分で管理します。特殊な配信基盤以外では実装負担が大きくなります。

### Addressables

addressやlabelからlocationを解決し、依存AssetBundle、download、cache、load、reference countを統合します。内部でAssetBundle等を使いますが、AssetBundle APIの単純な別名ではありません。

## 4. Addressablesの内部経路

```text
Address / Label / AssetReference
        ↓ key解決
Resource Locator + Content Catalog
        ↓
IResourceLocation
        ↓ dependency収集
Resource Provider
        ↓ 必要ならnetwork/cache
AssetBundle
        ↓
Asset
        ↓ InstantiateAsyncなら
GameObject Instance
```

- Address: Assetを識別するkey。
- Label: 複数Assetを分類するkey。
- Catalog: keyとlocation等の対応表。
- Location: provider、internal ID、dependency等のload情報。
- Provider: AssetBundle、Scene等を実際に供給する処理。
- `AsyncOperationHandle`: operationの状態、結果、進捗、寿命を表すhandle。

## 5. 導入とVersion固定

Package ManagerからAddressablesを導入し、`Window > Asset Management > Addressables > Groups`で初期設定を作ります。

`Packages/manifest.json`とlock fileをversion管理し、team/build machineで同じPackage Versionを使います。「最新版のはず」に依存しません。

## 6. Addressを設計する

悪い例:

```text
Assets/Characters/Heroes/Hero_A/Prefabs/Hero_A_Final_v17.prefab
```

folder変更や命名変更がruntime keyへ漏れます。

一例:

```text
character/hero_a/prefab
character/hero_a/portrait
stage/city_night/scene
ui/battle/hud
```

ただしaddress文字列をgameplay code全体へ直書きするのも危険です。typed `AssetReference`、定数、catalog dataのいずれかに境界を作ります。

## 7. AssetReference

```csharp
using UnityEngine;
using UnityEngine.AddressableAssets;

[CreateAssetMenu(menuName = "Game/Character Content")]
public sealed class CharacterContent : ScriptableObject
{
    // InspectorでAddressableなGameObjectだけを割り当てる参照。
    // 文字列addressのtypoを減らし、data側でcontentを差し替えやすくする。
    [SerializeField] private AssetReferenceGameObject battlePrefab;

    public AssetReferenceGameObject BattlePrefab => battlePrefab;
}
```

AssetReferenceはAssetそのものを常駐させる直接参照ではなく、runtime loadに使う参照情報です。

## 8. AsyncOperationHandleの状態

主に確認するもの:

- `IsDone`: operationが完了したか。
- `Status`: `None`、`Succeeded`、`Failed`。
- `Result`: 成功時の結果。
- `OperationException`: 失敗原因。
- `PercentComplete`: operation内部の均等加重進捗。download byte率とは一致しない場合がある。
- `GetDownloadStatus()`: download byteの進捗。
- `Completed`: 完了callback。
- `Task`: `await`に利用できる環境でのTask。
- `IsValid()`: handleがまだ有効か。

handleは「ただのPromise」ではなく、resource寿命の帳簿でもあります。

## 9. 単一Assetをawaitでloadする

```csharp
using System;
using System.Threading;
using System.Threading.Tasks;
using UnityEngine;
using UnityEngine.AddressableAssets;
using UnityEngine.ResourceManagement.AsyncOperations;

public static class AddressableLoader
{
    public static async Task<AsyncOperationHandle<T>> LoadAsync<T>(
        object key,
        CancellationToken cancellationToken)
    {
        // API呼び出し時点でAddressables側のoperation/referenceが一つ作られる。
        AsyncOperationHandle<T> handle = Addressables.LoadAssetAsync<T>(key);

        try
        {
            // Addressables operation自体に一般的な強制cancel APIがあるとは限らない。
            // ここでは「呼び出し側が結果を待つこと」をcancel可能にする。
            while (!handle.IsDone)
            {
                cancellationToken.ThrowIfCancellationRequested();
                await Task.Yield();
            }

            if (handle.Status != AsyncOperationStatus.Succeeded)
            {
                throw new InvalidOperationException(
                    $"Addressable load failed. key={key}",
                    handle.OperationException);
            }

            // 呼び出し側へhandleの所有権を渡す。
            // 呼び出し側は利用終了時に必ずAddressables.Release(handle)する。
            return handle;
        }
        catch
        {
            // cancel/exception後も開始済みoperationのreferenceを放置しない。
            // Releaseにより完了後のresource解放へ繋がる。
            if (handle.IsValid())
            {
                Addressables.Release(handle);
            }

            throw;
        }
    }
}
```

`CancellationToken`がcancelされても、network requestやproviderの内部仕事が瞬時に止まる保証はありません。重要なのは、遅れて完了した結果を利用しないことと、handleを確実にreleaseすることです。

## 10. 所有権を表すlease

裸のhandleを各所へ配ると、二重ReleaseかRelease漏れが起こります。次は「一つのloadに一つのlease」を明示する簡易例です。

```csharp
using System;
using UnityEngine.AddressableAssets;
using UnityEngine.ResourceManagement.AsyncOperations;

public sealed class AddressableLease<T> : IDisposable
{
    private AsyncOperationHandle<T> handle;
    private bool disposed;

    public T Asset
    {
        get
        {
            if (disposed)
            {
                throw new ObjectDisposedException(nameof(AddressableLease<T>));
            }

            return handle.Result;
        }
    }

    public AddressableLease(AsyncOperationHandle<T> handle)
    {
        if (!handle.IsValid())
        {
            throw new ArgumentException("Invalid handle.", nameof(handle));
        }

        this.handle = handle;
    }

    public void Dispose()
    {
        if (disposed)
        {
            return; // Disposeの複数回呼び出しを安全にする。
        }

        disposed = true;

        if (handle.IsValid())
        {
            Addressables.Release(handle);
        }
    }
}
```

これはconcept例です。Unity Objectをbackground threadから操作しない、async例外の観測、Domain Reload設定、application終了順序も実Projectで検証します。

## 11. loadとreleaseの対称性

原則:

```text
LoadAssetAsync  1回 → Release 1回
InstantiateAsync 1回 → ReleaseInstance 1回
LoadSceneAsync 1回 → UnloadSceneAsync相当 1回
DownloadDependenciesAsync → 設定に応じてhandleをRelease
```

同じkeyを3回loadしたなら、内部Assetが共有されてもreference countは3回分増え得ます。1回だけReleaseして帳尻が合うとは考えません。

## 12. Assetをloadして自分でInstantiateする場合

```csharp
AsyncOperationHandle<GameObject> prefabHandle =
    Addressables.LoadAssetAsync<GameObject>(prefabReference);

await prefabHandle.Task;

if (prefabHandle.Status == AsyncOperationStatus.Succeeded)
{
    GameObject instance = UnityEngine.Object.Instantiate(prefabHandle.Result);

    // instanceがprefab内のTexture/Material等を使う間にhandleをreleaseすると、
    // 依存resourceの寿命が不足する設計になり得る。
    // instance破棄までprefabHandleをownerが保持する。
}
```

instanceの寿命とprefab handleの寿命を関連付けるownerを用意します。

## 13. InstantiateAsyncとReleaseInstance

```csharp
using UnityEngine;
using UnityEngine.AddressableAssets;
using UnityEngine.ResourceManagement.AsyncOperations;

public sealed class AddressableActor : MonoBehaviour
{
    [SerializeField] private AssetReferenceGameObject prefab;

    private AsyncOperationHandle<GameObject> instanceHandle;
    private bool hasInstance;

    public async void Spawn()
    {
        if (hasInstance)
        {
            return;
        }

        instanceHandle = prefab.InstantiateAsync(transform.position, transform.rotation);
        await instanceHandle.Task;

        if (instanceHandle.Status == AsyncOperationStatus.Succeeded)
        {
            hasInstance = true;
            return;
        }

        Debug.LogException(instanceHandle.OperationException);

        if (instanceHandle.IsValid())
        {
            Addressables.Release(instanceHandle);
        }
    }

    private void OnDestroy()
    {
        if (hasInstance && instanceHandle.IsValid())
        {
            // Addressables経由で生成したinstanceを破棄し、関連referenceも戻す。
            Addressables.ReleaseInstance(instanceHandle);
            hasInstance = false;
        }
    }
}
```

`async void`はUnity event入口以外では避け、service層では`Task`を返して例外を呼び出し側へ伝播させます。

## 14. AssetReferenceの多重load注意

`AssetReference.LoadAssetAsync()`は、先のloadをreleaseするまで同じAssetReference instanceで再度使えない制約があります。複数loadが必要なら`Addressables.LoadAssetAsync<T>(assetReference)`のkeyとして渡す設計を検討します。

多重loadが本当に必要か、上位repositoryで共有すべきかも先に判断します。

## 15. Coroutine・callback・Task

### Coroutine

```csharp
private IEnumerator LoadRoutine(object key)
{
    AsyncOperationHandle<GameObject> handle =
        Addressables.LoadAssetAsync<GameObject>(key);

    yield return handle;

    if (handle.Status == AsyncOperationStatus.Succeeded)
    {
        // handle.Resultを使用する。
    }

    Addressables.Release(handle);
}
```

### Completed callback

cache済み等の条件でcallback時機が変わり得ます。登録側のstate初期化をcallback登録より前に終え、同期的に近い完了でも壊れない設計にします。

### Task

直線的なerror handlingを書きやすい一方、platform/API対応とUnity main thread境界を確認します。どの方式でもhandle所有権は同じです。

## 16. 初期化を分離する

最初のAddressables APIは必要なら暗黙初期化も行います。初回battle loadへ初期化costを混ぜたくない場合、boot phaseで明示します。

```csharp
AsyncOperationHandle initialization = Addressables.InitializeAsync();
await initialization.Task;

if (initialization.Status != AsyncOperationStatus.Succeeded)
{
    throw initialization.OperationException;
}

// overloadのautoReleaseHandle設定を確認し、手動所有するhandleならReleaseする。
Addressables.Release(initialization);
```

初期化にはResourceManager、locator、runtime data、catalog等の準備が含まれます。

## 17. Labelと複数Asset

Label例:

```text
stage_city_night
character_hero_a
preload_battle_common
localization_ja_voice
```

Labelは便利ですが、巨大な`preload_all`を作ると粒度を失います。また一つのkeyが複数Assetへ対応するのに単一`LoadAssetAsync`を使うと最初の一つだけになるため、複数には`LoadAssetsAsync`を使います。

部分失敗時の`releaseDependenciesOnFailure`によって、失敗後に呼び出し側がreleaseすべきかが変わります。使用overloadの契約を確認してください。

## 18. ResourceLocationを先に解決する

同じkeyを繰り返し解決する箇所や、load前の検証では`LoadResourceLocationsAsync`を使えます。

```text
key
 ↓ locator検索
location list
 ↓ asset type検証・dependency確認
LoadAssetAsync(location)
```

location handle自体のReleaseも忘れません。locationをcacheする場合はcatalog更新との整合性を設計します。

## 19. Download sizeを事前確認する

```csharp
AsyncOperationHandle<long> sizeHandle =
    Addressables.GetDownloadSizeAsync(label);

await sizeHandle.Task;

long downloadBytes = sizeHandle.Status == AsyncOperationStatus.Succeeded
    ? sizeHandle.Result
    : 0L;

Addressables.Release(sizeHandle);
```

既にcache済みなら0になる場合があります。容量、通信環境、空きdisk、user同意を確認してからdownloadします。

## 20. Dependencyをpre-downloadする

```csharp
AsyncOperationHandle downloadHandle =
    Addressables.DownloadDependenciesAsync(label, autoReleaseHandle: false);

while (!downloadHandle.IsDone)
{
    DownloadStatus status = downloadHandle.GetDownloadStatus();

    // Percentはbytes基準。TotalBytesが0の初期状態も扱う。
    float progress = status.TotalBytes > 0
        ? (float)status.DownloadedBytes / status.TotalBytes
        : 0f;

    ShowDownloadProgress(progress, status.DownloadedBytes, status.TotalBytes);
    await System.Threading.Tasks.Task.Yield();
}

if (downloadHandle.Status != AsyncOperationStatus.Succeeded)
{
    Debug.LogException(downloadHandle.OperationException);
}

Addressables.Release(downloadHandle);
```

pre-downloadはAssetを必ず常駐loadする処理ではありません。必要Bundleをcacheへ準備し、battle直前のnetwork待ちを減らします。

## 21. PercentCompleteの誤解

operationが3段階なら各段階を同じ重みで扱う場合があり、download byteが90%でも`PercentComplete`が90%とは限りません。

download UI:

- `GetDownloadStatus()`のbytes。
- file検証やload/decodeの別phase表示。
- 通信停止と処理停止を区別。
- 残り時間は移動平均等で安定化。

## 22. Group設計

Groupは単なるfolderではなく、build/update/load単位へ影響します。

判断軸:

- 同時に使うか。
- 同じ頻度で更新するか。
- localかremoteか。
- 同じplatform設定か。
- 共有dependencyがあるか。
- Bundle粒度とrequest数。
- memory peakとunload時機。

一Asset一Bundleはrequest/header/catalog overheadを増やします。巨大一Bundleは小変更でdownload差分を大きくし、不要Assetも同居させ、解放粒度を粗くします。

## 23. Bundle Packing Mode

代表的な考え方:

- Pack Together: groupをまとめる。まとめて利用するcontent向け。
- Pack Separately: entryごと。独立更新/独立load向けだがBundle数増大に注意。
- Pack Together By Label: label構成でまとめる。labelの組み合わせ爆発に注意。

設定名や挙動はPackage Versionで確認し、Analyze結果と実build artifactを見ます。

## 24. 共有Dependency問題

複数Bundleが同じmaterial/textureを直接含めれば重複する可能性があります。共有AssetをAddressable entryとして独立Bundle化すると重複を抑えられる一方、dependency Bundleが増えて寿命が結合します。

```text
Character A Bundle ─┐
                    ├→ Shared Shader/Material Bundle
Character B Bundle ─┘
```

AをreleaseしてもBが使っていればshared Bundleは残ります。これはreference countが働いた正常な状態です。

## 25. Compression

一般的な性質:

- Uncompressed: 大きいがload処理が単純。platform/filesystem次第。
- LZ4: chunk単位で扱いやすくruntime向けの代表候補。
- LZMA: download sizeを抑えやすいが、load/cache変換costやmemory peakを考慮。

「最小fileが最速」とは限りません。network、storage、decompression CPU、cache、peak memoryを対象機で計測します。

## 26. LocalとRemote

Local content:

- applicationに同梱。
- 初回network不要。
- application updateなしでは差し替えにくい。

Remote content:

- CDN/serverから配信。
- content updateや初期install縮小が可能。
- offline、timeout、retry、version、cache、security、費用が必要。

Boot不能になる最小UI、error画面、通信再試行画面までremoteだけへ置かないようにします。

## 27. ProfileとBuild/Load Path

Environmentごとにpathを分けます。

```text
Development → local test server
Staging     → staging CDN
Production  → production CDN/version path
```

production clientがdevelopment URLを見る事故をbuild validationで止めます。URLにはcontent versionやplatformを含め、上書きとcache poisoningを避けます。

## 28. Catalog

Catalogはaddress/labelからlocationを探す索引です。Remote Catalogとhashを使えば更新を検出できます。

Catalog圧縮はfile sizeを減らす代わりにbuild/load時間へ影響します。catalog entry数、address長、label数もstartup memory/timeへ影響するため、むやみに増やしません。

## 29. Content Updateの原則

公開済みPlayer向け更新では、そのbuild時の`addressables_content_state.bin`を保存します。これは更新buildの基準資料であり、適当な別buildのものへ置換しません。

```text
Player Build v1 + Content State v1
             ↓
変更Assetを判定
             ↓
Update Previous Build
             ↓
新Catalog + 変更Bundle
```

全contentをnew buildとして作り直すと、変更していないremote Bundleまで再downloadさせる設計になり得ます。

## 30. Static contentと更新頻度

ほぼ変わらない大型stageと、頻繁に変わるbalance/UI/voiceを同じ更新単位へ詰めると、小変更で大型Bundleが再配信されます。

ただし細分化し過ぎも逆効果です。「一緒に使う」「一緒に変わる」を中心に粒度を決めます。

## 31. Runtime Catalog Update

安全な流れの一例:

```text
Boot
 → CheckForCatalogUpdates
 → userへdownload説明
 → UpdateCatalogs
 → DownloadDependencies
 → gameplay content load開始
```

既に関連Bundleをloadした後のcatalog更新は旧新Bundle競合を起こし得ます。基本はcontent load前に更新します。Unique Bundle IDsは競合回避に役立つ場合がありますが、memoryやdownload sizeを増やすtrade-offがあります。

## 32. Cacheはmemoryではない

```text
Remote server
  ↓ download
Disk cache
  ↓ load
AssetBundle metadata / compressed pages
  ↓ Asset load
Native object memory
  ↑
Managed wrapper / C# references
```

`Release`してmemoryから不要になってもdisk cacheは残り得ます。反対にcacheを消しても、現在load中のAssetが即座に破棄されるという意味ではありません。

## 33. Cache cleanup

Addressablesにはdependency cache削除用APIがありますが、使用中Bundle、次回再download、通信量を考慮します。

- update後の参照されない旧Bundleをcleanup。
- settings画面から全cache削除。
- disk不足時のpolicy。
- Wi-Fi限定downloadとの整合。

cacheを毎起動消すとCDNとuser通信量を浪費します。

## 34. AssetBundleを直接loadする内部理解

```csharp
using System.Collections;
using UnityEngine;

public sealed class NativeBundleExample : MonoBehaviour
{
    private IEnumerator LoadBundle(string absolutePath)
    {
        // fileから非同期にBundle containerを開く。
        AssetBundleCreateRequest bundleRequest =
            AssetBundle.LoadFromFileAsync(absolutePath);
        yield return bundleRequest;

        AssetBundle bundle = bundleRequest.assetBundle;
        if (bundle == null)
        {
            Debug.LogError($"Failed to load bundle: {absolutePath}");
            yield break;
        }

        // Bundle内の名前からAssetを非同期loadする。
        AssetBundleRequest assetRequest =
            bundle.LoadAssetAsync<GameObject>("BattleArena");
        yield return assetRequest;

        GameObject prefab = assetRequest.asset as GameObject;
        if (prefab != null)
        {
            Instantiate(prefab);
        }

        // false: Bundle containerのmetadata等を外すが、load済みAssetは残す。
        // その後、同じBundleから追加Assetをloadできない。
        bundle.Unload(unloadAllLoadedObjects: false);
    }
}
```

実用の直接管理にはmanifest dependency、hash/version、CRC、UnityWebRequest、cache、retry、同一Bundle多重load防止等がさらに必要です。そのため一般にはAddressablesが推奨されます。

## 35. AssetBundle.Unload(false)

Bundle header/containerをunloadし、load済みObjectを残します。

注意:

- 残したAssetの追跡と解放は別途必要。
- 同じBundleを再loadすると同一Assetの別instance/重複memory問題を招き得る。
- Bundleから後続Assetをloadできない。

## 36. AssetBundle.Unload(true)

Bundleと、そこからloadされたAssetをunloadします。まだSceneやcomponentが利用しているTexture/Material等まで対象にすればmissing表示や壊れた参照を起こします。

「trueの方が完全だから常に正しい」ではありません。利用者がゼロになったことをowner/reference countで証明してから行います。

## 37. DestroyとRelease

```csharp
GameObject instance = Instantiate(prefab);

// Scene instanceを破棄する。
Destroy(instance);

// Addressables loadで得たprefab/dependencyのreferenceを戻す。
Addressables.Release(prefabHandle);
```

両方が必要な設計があります。`Destroy(instance)`はload operation handleのReleaseではなく、`Release(handle)`は任意のScene instanceのDestroyと同義ではありません。

## 38. Resources.UnloadUnusedAssets

未使用と判定されたAssetをunloadする重いoperationです。頻繁にbattle中へ呼ばず、loading screen等の安全な区切りで計測して使います。

Addressablesの正しいReleaseを代替する「万能掃除機」ではありません。static field、event、singleton等の参照が残ればunusedになりません。

## 39. Resources.UnloadAsset

disk上の特定Assetをmemoryからunloadしますが、GameObjectやComponent等、利用可能な種類に制約があります。Addressables管理Assetへ勝手に混用せず、所有systemを一つにします。

## 40. ManagedとNativeの二層

`Texture2D`変数はmanaged wrapperを指し、pixel data等はnative側にもあります。

```text
C# reference
 → Managed UnityEngine.Object wrapper
   → Native Unity object
     → GPU resource / driver allocation
```

GCはC# heapを扱いますが、GPU textureをいつ解放するかまで単独で管理するわけではありません。Memory ProfilerでManaged、Native、Graphicsを区別します。

## 41. Scene load

Addressables Sceneもhandleを保持し、対応するunload APIで解放します。Additive Sceneでは依存関係が複雑になるため、Scene Lifetime Ownerを用意します。

```text
Persistent Systems Scene
 + Battle Stage Scene
 + Lighting Scenario Scene
 + UI Scene
```

どのSceneがlighting、NavMesh、audio、shared materialを所有するか定義します。

## 42. Stage streaming state machine

```text
Idle
 → CheckingSize
 → WaitingForConsent
 → Downloading
 → Loading
 → Activating
 → Active
 → Unloading
 → Idle

各stateから Failed / CancelRequested へ遷移
```

boolを数個並べるより、許可された遷移、保持handle、cleanupをstateごとに定義します。

## 43. 二段階のpreload

例:

1. Lobbyで次stageのBundleをdownload cacheへ置く。
2. Loading画面で必要Assetをmemoryへloadする。
3. battle開始直前に重要Prefabをwarm-upする。

download、memory load、shader/PSO warm-up、object pool生成は別costです。download済みだけで初回frame spikeが消えるとは限りません。

## 44. Character切替content

高速切替actionでは、現在characterだけloadして交代の瞬間にdownloadする設計は間に合いません。

```text
Current character: fully active
Party members: prefab/animation/core VFXをresident
Rare finisher content: phase前preload
Unused roster: released/cache only
```

resident setを明示し、party変更時に差分load/releaseします。

## 45. Combat content manifest

```csharp
using System.Collections.Generic;
using UnityEngine;
using UnityEngine.AddressableAssets;

[CreateAssetMenu(menuName = "Game/Combat Content Manifest")]
public sealed class CombatContentManifest : ScriptableObject
{
    [SerializeField] private AssetReferenceGameObject stage;
    [SerializeField] private List<AssetReferenceGameObject> partyCharacters;
    [SerializeField] private List<AssetReferenceGameObject> enemyArchetypes;

    public AssetReferenceGameObject Stage => stage;
    public IReadOnlyList<AssetReferenceGameObject> PartyCharacters => partyCharacters;
    public IReadOnlyList<AssetReferenceGameObject> EnemyArchetypes => enemyArchetypes;
}
```

ただしManifest自身が直接参照を持つせいで全部Playerへ入らないか、AssetReferenceとして意図通りになるかをbuild layoutで確認します。

## 46. Load transaction

複数loadの途中で一つ失敗したら、それまで成功したhandleを逆順でreleaseします。

```csharp
var acquiredHandles = new List<AsyncOperationHandle>();

try
{
    // 各load成功後にhandleをacquiredHandlesへ記録する。
    // 全成功した時だけownerへlistの所有権をcommitする。
}
catch
{
    for (int i = acquiredHandles.Count - 1; i >= 0; --i)
    {
        if (acquiredHandles[i].IsValid())
        {
            Addressables.Release(acquiredHandles[i]);
        }
    }

    throw;
}
```

database transactionのように「全成功ならcommit、途中失敗ならrollback」と考えると漏れを減らせます。

## 47. 重複requestをまとめるrepository

同一keyへ同時loadが来たとき、上位serviceがin-flight operationを共有すると重複を抑えられます。

ただし共有handleを利用者全員が勝手にReleaseしてはいけません。repository独自のlease countを持ち、最後の利用者が返した時だけAddressables handleをReleaseします。

```text
Caller A ─ acquire ─┐
                    ├→ Repository entry → one Addressables handle
Caller B ─ acquire ─┘

Caller A release → count 1
Caller B release → count 0 → Addressables.Release
```

## 48. Race condition

典型例:

1. Scene Aがload開始。
2. userがScene Bへ遷移。
3. Scene A ownerが破棄。
4. 古いloadが完了。
5. callbackが破棄済みownerへinstanceを生成。

対策:

- generation ID。
- CancellationToken。
- owner存在確認。
- 完了後も「まだ必要か」を再確認。
- 不要なら結果を即release。

## 49. Generation ID例

```csharp
private int loadGeneration;

public async Task ReloadAsync(object key)
{
    int myGeneration = ++loadGeneration;
    AsyncOperationHandle<GameObject> handle =
        Addressables.LoadAssetAsync<GameObject>(key);

    await handle.Task;

    // 自分より新しいrequestが発行済みなら、この結果は古い。
    if (myGeneration != loadGeneration)
    {
        if (handle.IsValid())
        {
            Addressables.Release(handle);
        }
        return;
    }

    // 最新結果としてownerへcommitする。
}
```

## 50. Error分類

- address/keyが存在しない。
- requested type不一致。
- dependency欠落。
- catalog取得失敗。
- hash/version不整合。
- timeout/network切断。
- disk空き容量不足。
- cache破損。
- server 404/403/5xx。
- Bundleが別platform向け。
- build時のcode stripping。
- Asset deserialize失敗。

user向けmessage、retry可能性、telemetry用原因を分けます。

## 51. Retry

無限即時retryは禁止です。

```text
attempt 1 → 1秒 + jitter
attempt 2 → 2秒 + jitter
attempt 3 → 4秒 + jitter
→ user操作へ戻す
```

404やversion不一致は待っても直らない場合があります。network一時障害だけを指数backoffし、request stormを避けます。

## 52. Securityと整合性

- HTTPS。
- trusted host/CDN。
- catalog/hash管理。
- AssetBundle CRC等、利用方式に応じた検証。
- URLを外部入力から無制限に組み立てない。
- codeはAssetBundleから追加できない前提でPlayer code strippingを確認。
- 秘密鍵をclientへ埋め込まない。

hashは必ずしも攻撃者に対する署名ではありません。threat modelに応じて配信基盤を設計します。

## 53. Editor Play Mode Script

代表的なmode:

- Asset Databaseを直接利用する高速iteration。
- 既存build contentを利用。
- simulation/buildを経た挙動。

Editorで直接Asset Databaseを読むmodeだけでは、Bundle構成、remote URL、cache、content update問題を再現できません。定期的にPlayer相当のcontent buildで試験します。

## 54. Build LayoutとAnalyze

確認項目:

- Bundleごとのsize。
- Assetとdependencyの所属。
- 重複Asset。
- implicit dependency。
- Bundle数。
- Scene依存。
- built-in shader等。
- update時に再buildされる範囲。

「Group windowが整理されて見える」だけではruntime構成を保証しません。生成物を確認します。

## 55. Memory peak

最終常駐量だけでなく遷移中peakを測ります。

```text
Old stage resident      2.0 GB
New bundle download/cache buffer
New stage loading       1.8 GB
Temporary decompression 0.5 GB
--------------------------------
Transition peak         4.3 GB + overhead
```

旧stageを残したまま新stageをloadする滑らかな遷移はpeak memoryを増やします。対象hardwareのbudget次第で、暗転して先に解放する設計も必要です。

## 56. MemoryがすぐOSへ戻らない理由

allocatorが解放領域を将来の再利用のため保持したり、driver/graphics API側で解放時機がずれたりします。Profiler上のlive object減少とOS process memory減少は同じ時刻にならない場合があります。

判定は単一数値ではなく、snapshot差分、object count、native allocation、graphics memory、長時間loopでの増加傾向を見ます。

## 57. Shaderと初回stutter

Assetを非同期loadしても、最初にshader variantを利用する瞬間のcompile/PSO生成、texture upload、Animator初期化等で停止し得ます。

content streaming計画には次も含めます。

- shader variant管理。
- pipeline warm-up。
- material/texture upload時機。
- object pool事前生成。
- animation graph初期化。
- audio decode/preload。

## 58. Object Poolとの関係

AddressablesとObject Poolは別責務です。

- Addressables: asset/dependencyを取得し、寿命を管理。
- Pool: 生成済みinstanceを再利用。

pool内にinstanceが1個でも残る間は、そのinstanceが必要とするAddressables leaseを保持します。poolをclearして全instanceをDestroyした後にasset handleをreleaseします。

## 59. Scene遷移cleanup順序

一例:

1. 新規spawn停止。
2. async requestへcancel要求/generation更新。
3. gameplay system停止。
4. poolを返却・破棄。
5. instanceを破棄。
6. Sceneをunload。
7. Addressables handleをrelease。
8. 必要ならunused cleanup。
9. memory計測。

依存関係により順番は変わります。重要なのはowner treeのleafから解放することです。

## 60. Loading UI

表示すべき情報:

- catalog確認中。
- download同意待ち。
- download bytes / total bytes。
- asset load中。
- initialization/warm-up中。
- retry/cancel。
- required disk size。
- error codeを直接晒さないuser向け説明。

progressを0.99で長時間止めないよう、phase別に意味を示します。

## 61. Offline設計

- 必須local contentだけで起動可能か。
- cache済みcontentで遊べる範囲。
- catalog server不通時に古いcatalogを使うか。
- update必須versionならどう案内するか。
- download途中からresume可能か。
- cache破損時に復旧できるか。

remote配信は正常系だけでは完成しません。

## 62. Platform差

- Bundleは通常target platformごとにbuildする。
- compression、filesystem、Web request、cache上限が異なる。
- console/mobileではsuspend/resumeとstorage抜去/容量不足を考える。
- Webではmemory/file access制約が異なる。
- case-sensitiveなserverとWindows Editorのpath差に注意。

Editorで成功したBundleを全platform共通だと思わないでください。

## 63. Telemetry

記録候補:

- content version/catalog version。
- keyをprivacy/securityに配慮して分類したID。
- phase別所要時間。
- download bytesとthroughput。
- cache hit/miss。
- retry count。
- error category。
- device/platform/network種別。
- transition peak memory。

個人情報や認証tokenをlogへ残しません。

## 64. Debug表示

development buildで次を表示できるようにします。

```text
Addressables initialized: true
Catalog version: ...
Active leases: 42
In-flight operations: 3
Resident content sets: battle_common, hero_a, city_night
Downloading: 128 MB / 512 MB
Last failure: timeout
```

ただしAddressables内部reference countと独自lease countを混同しません。

## 65. よくある失敗

### Resultだけ保存してhandleを捨てる

誰がReleaseするか分からなくなります。ownerはhandle/leaseも保存します。

### load直後にReleaseする

利用中instanceのdependency寿命が不足します。

### OnDestroyだけに依存する

objectがdisableされただけ、persistent owner、application終了順序等で意図通りにならない場合があります。明示的なlifecycle APIを用意します。

### Labelを全用途へ使う

意図しないAsset追加でdownload/memory量が増えます。Build validationでlabel membershipを検査します。

### Editor成功だけで完了にする

remote build、fresh install、cache hit、cache corruption、offline、低速回線を試験します。

### `UnloadUnusedAssets`を毎回呼ぶ

frame spikeを生み、根本のRelease漏れを隠します。

## 66. Test matrix

| 条件 | 確認 |
|---|---|
| Fresh install | catalogと全Bundleを取得できる |
| Cache hit | 余計な再downloadがない |
| Offline + cacheあり | 許可した範囲で動く |
| Offline + cacheなし | 復旧可能な案内になる |
| Download中切断 | timeout/retry/cancelが働く |
| Disk不足 | 破損を残さず説明する |
| Catalog更新 | 旧新Bundle競合がない |
| Scene往復100回 | handle/object/memoryが増え続けない |
| Low-memory device | peak budget内に収まる |
| Application suspend | 復帰時にstateが整合する |

## 67. Review checklist

- loadごとに対応するrelease ownerがいるか。
- handleをResultより先に捨てていないか。
- async失敗/cancel/古い結果でもcleanupするか。
- download progressとoperation progressを区別したか。
- Group粒度が利用・更新単位に合うか。
- 共有dependency重複をBuild Layoutで見たか。
- content state fileを公開buildごとに保存したか。
- catalog更新をcontent load前に行うか。
- memory peakを実機で測ったか。
- Pool resident中にAssetをreleaseしていないか。
- fresh cacheとwarm cacheの両方を試したか。
- platform別Bundleをbuildしたか。

## 68. 学習確認問題

1. C#変数を`null`にすることとAddressables.Releaseの違いは何か。
2. `Destroy(instance)`だけでprefab handleを解放したことになるか。
3. `PercentComplete`がdownload byte率と一致しない理由は何か。
4. AssetBundle.Unloadの`true`と`false`は何を変えるか。
5. 一Asset一Bundleと巨大一Bundleのtrade-offは何か。
6. catalogをgameplay開始後に更新する危険は何か。
7. cancelされたloadのhandleを放置してはいけない理由は何か。
8. poolとAddressables leaseの寿命をどう対応させるか。
9. transition中のpeak memoryが定常時より大きくなる理由は何か。
10. `addressables_content_state.bin`をなぜ保存するのか。

## 69. 公式資料

- [Unity Manual: Addressables package](https://docs.unity3d.com/6000.0/Documentation/Manual/com.unity.addressables.html)
- [Addressables: Loading Addressable assets](https://docs.unity3d.com/Packages/com.unity.addressables@2.7/manual/load-addressable-assets.html)
- [Addressables: Asynchronous loading](https://docs.unity3d.com/Packages/com.unity.addressables@2.7/manual/load-assets-asynchronous.html)
- [Addressables: Memory management](https://docs.unity3d.com/Packages/com.unity.addressables@2.7/manual/MemoryManagement.html)
- [Addressables API: LoadAssetAsync](https://docs.unity3d.com/Packages/com.unity.addressables@2.7/api/UnityEngine.AddressableAssets.Addressables.LoadAssetAsync.html)
- [Addressables API: Release](https://docs.unity3d.com/Packages/com.unity.addressables@2.7/api/UnityEngine.AddressableAssets.Addressables.Release.html)
- [Addressables API: DownloadDependenciesAsync](https://docs.unity3d.com/Packages/com.unity.addressables@2.7/api/UnityEngine.AddressableAssets.Addressables.DownloadDependenciesAsync.html)
- [Unity Manual: AssetBundles](https://docs.unity3d.com/6000.0/Documentation/Manual/assetbundles-section.html)
- [Unity Manual: Using AssetBundles natively](https://docs.unity3d.com/6000.0/Documentation/Manual/AssetBundles-Native.html)
- [Unity Scripting API: Resources.UnloadUnusedAssets](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/Resources.UnloadUnusedAssets.html)

## 70. まとめ

- Addressablesはaddressからlocationとdependencyを解決し、local/remote contentを非同期loadする仕組み。
- `AsyncOperationHandle`は完了通知だけでなくresource寿命の一部。
- loadとrelease、instantiateとrelease instanceを対称にする。
- cancelは「待つのを止める」だけの場合があるため、遅延完了後のcleanupが必要。
- Bundle粒度は同時利用、更新頻度、dependency、request数、memory peakで決める。
- disk cache、native memory、managed reference、Scene instanceは別々に管理する。
- 大規模actionではdownload、memory load、warm-up、resident setを段階化する。
- 正しさはEditorの一度の成功ではなく、更新、失敗、往復、実機memory試験で確認する。
