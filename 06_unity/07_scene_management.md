# Scene管理・非同期遷移・永続System

## 1. Sceneは何を分ける単位か

SceneはGameObject群を保存・ロードする単位です。ただし「ゲーム全体の状態」そのものではありません。タイトル、戦闘ステージ、Lighting、常駐Systemなど、寿命とロード都合に応じて分けます。

```text
Bootstrap Scene（常駐）
├─ GameFlow / Save / Audio / Input owner
├─ UI Scene（必要に応じてAdditive）
└─ Stage Scene（交換してロード/アンロード）
```

## 2. SingleとAdditive

- `LoadSceneMode.Single`: 現在のScene群を置き換える基本モード。
- `LoadSceneMode.Additive`: 既存Sceneを残して追加ロードする。Stage、Lighting、UIの分割に使える。

AdditiveでロードされたSceneが自動的に「active scene」になるとは決めつけません。active sceneは新規GameObjectの所属先や一部設定に影響するため、必要なら`SceneManager.SetActiveScene`を明示します。

## 3. 同期ロードより非同期ロードを基本にする

Unity 6のAPI資料も、多くの場合は停止や引っ掛かりを避けるため`LoadSceneAsync`を推奨しています。

```csharp
using System.Collections;
using UnityEngine;
using UnityEngine.SceneManagement;

public sealed class SceneLoader : MonoBehaviour
{
    private bool isLoading;

    public void RequestLoad(string sceneName)
    {
        if (!isLoading)
        {
            StartCoroutine(LoadRoutine(sceneName));
        }
    }

    private IEnumerator LoadRoutine(string sceneName)
    {
        isLoading = true;

        AsyncOperation operation = SceneManager.LoadSceneAsync(sceneName, LoadSceneMode.Single);
        if (operation == null)
        {
            isLoading = false;
            yield break;
        }

        while (!operation.isDone)
        {
            // progressの意味やactivation前の上限は利用版のAPI仕様を確認する。
            // 表示用割合と内部進捗を同一視しない。
            yield return null;
        }

        isLoading = false;
    }
}
```

非同期は「別threadで全処理が安全に動く」という意味ではありません。また`AsyncOperation`は一般的な`CancellationToken`付きTaskと同じ取消モデルではありません。取消不能な処理にはrequest generationを持ち、古い完了結果を採用しない設計が有効です。

## 4. 古い非同期結果を捨てる

```csharp
private int loadGeneration;

public void BeginRequest(string sceneName)
{
    int myGeneration = ++loadGeneration;
    StartCoroutine(LoadAndCommit(sceneName, myGeneration));
}

private IEnumerator LoadAndCommit(string sceneName, int generation)
{
    AsyncOperation operation = SceneManager.LoadSceneAsync(sceneName, LoadSceneMode.Additive);
    yield return operation;

    if (generation != loadGeneration)
    {
        // より新しい要求がある。古い結果をactiveとして採用しない。
        // 必要ならロード済みSceneを安全にunloadする。
        yield break;
    }

    Scene loaded = SceneManager.GetSceneByName(sceneName);
    if (loaded.IsValid() && loaded.isLoaded)
    {
        SceneManager.SetActiveScene(loaded);
    }
}
```

本番では同名Scene、多重要求、失敗、unload中、Addressables利用時などを状態機械に含めます。

## 5. sceneLoaded eventの二重登録

```csharp
private void OnEnable()
{
    SceneManager.sceneLoaded += OnSceneLoaded;
}

private void OnDisable()
{
    SceneManager.sceneLoaded -= OnSceneLoaded;
}

private void OnSceneLoaded(Scene scene, LoadSceneMode mode)
{
    Debug.Log($"Loaded: {scene.name}, mode={mode}");
}
```

static eventに登録したまま購読者を破棄すると、意図しない生存や重複callbackの原因になります。登録場所と解除場所をコードレビューで対にします。

## 6. DontDestroyOnLoad

常駐させたいroot GameObjectに`DontDestroyOnLoad`を使えます。しかし各Sceneに同じManager Prefabを置くと、遷移のたびに重複生成されがちです。

```csharp
public sealed class GameRoot : MonoBehaviour
{
    private static GameRoot instance;

    private void Awake()
    {
        if (instance != null && instance != this)
        {
            Destroy(gameObject);
            return;
        }

        instance = this;
        DontDestroyOnLoad(gameObject);
    }
}
```

これは最小例です。staticのDomain Reload、test isolation、終了時、依存順序まで含めるならBootstrap Sceneで一度だけ構築する方が明瞭です。「何でもSingleton」は依存を隠すので避けます。

## 7. 遷移を状態機械として扱う

```text
Idle
 → FadeOut
 → StopGameplayInput
 → Save/Commit
 → Load
 → BindSceneServices
 → Spawn/RestoreParty
 → WarmUp
 → FadeIn
 → Playing
```

入力停止、Time Scale、Audio snapshot、Camera、UI、Object Pool、敵AIを誰がどの順序で切り替えるかを一つのFlowが管理します。ボタン、Trigger、敵死亡処理がそれぞれ直接`LoadSceneAsync`を呼ぶ構造は多重遷移を招きます。

## 8. Character交代とScene境界

Scene内Characterを永続Managerが直接保持すると、Scene unload後に破棄済み参照になります。永続層が保持すべきものは、Character ID、HP等の純粋な保存状態、生成recipeです。Scene実体はロード後にResolver/Installerが再bindします。

```text
Persistent PartyState: characterId, hp, energy
                    ↓ load後に復元
Scene CharacterView: Animator, Transform, VFX, Collider
```

## 9. Additive運用の注意

- どのSceneがactiveかを確認する。
- unload対象をnameだけで曖昧に選ばない。
- Cross-scene referenceを無秩序に作らず、公開Service経由でbindする。
- Lighting/Light Probe/NavMesh/Physics Sceneの仕様を利用版で確認する。
- Stage unload前にPoolやasync callbackがStage objectを参照していないか確認する。
- ロード完了と「プレイ可能」は別。Shader/animation/VFX等のwarm-upも計測する。

## 10. テスト項目

- 同じ遷移ボタンを連打する。
- ロード途中に別Scene要求を出す。
- Additive Sceneを逆順でunloadする。
- Domain Reload無効でPlayを繰り返す。
- 旧Character/Camera/UIへの参照が残らないか調べる。
- 失敗時に入力・Fade・Time Scaleを復旧できるか。

## 公式資料

- [Unity Scripting API: SceneManager.LoadScene](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/SceneManagement.SceneManager.LoadScene.html)
- [Unity Scripting API: SceneManager.LoadSceneAsync](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/SceneManagement.SceneManager.LoadSceneAsync.html)
- [Unity Scripting API: LoadSceneMode.Additive](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/SceneManagement.LoadSceneMode.Additive.html)
- [Unity Scripting API: Object.DontDestroyOnLoad](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/Object.DontDestroyOnLoad.html)

