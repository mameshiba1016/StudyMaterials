# UnityEngine.Object・null・Destroy・Serialization

> 対象: Unity 6系を中心とする。UnityやPackageの版によって細部は変わるため、実プロジェクトの公式Manual/APIも確認すること。

## 1. C#オブジェクトとUnity Objectは同じではない

通常のC#クラスのインスタンスはManaged Heap上にあり、到達不能になるとGCの回収候補になります。一方、`GameObject`、`Component`、`Texture`、`Material`など多くのUnity型は`UnityEngine.Object`を継承し、概念上は次の二層を持ちます。

```text
C#側のManaged Wrapper ── instance ID等で参照 ── Engine側のNative Object(C++)
```

したがって「C#変数がある」「Engine側実体が生存している」「Sceneに存在する」は別の事実です。GCはC#ラッパーを扱いますが、Native Objectの寿命を`null`代入だけで即座に終わらせるものではありません。

## 2. Unity特有のnull判定

`UnityEngine.Object`は`==`演算子を独自実装しています。Native Objectが破棄済みなら、Managed Wrapperが残っていても`obj == null`が`true`になり得ます。俗にfake nullと呼ばれる挙動です。

```csharp
using UnityEngine;

public sealed class TargetHolder : MonoBehaviour
{
    [SerializeField] private GameObject target;

    private void Update()
    {
        // UnityEngine.Object.operator == を通す。
        // targetのNative側実体が破棄済みの場合もtrueになり得る。
        if (target == null)
        {
            return;
        }

        target.transform.Rotate(0.0f, 90.0f * Time.deltaTime, 0.0f);
    }
}
```

`ReferenceEquals(target, null)`は純粋なC#参照だけを調べ、Unityの破棄済み判定を通しません。また`?.`や`??`はUnityのオーバーロード済み`==`を利用するとは限らないため、`UnityEngine.Object`の生存確認を短縮構文だけに任せない方が読みやすく安全です。

## 3. Destroyは予約であり、その行で消滅完了とは限らない

```csharp
private void Defeat(GameObject enemy)
{
    // Engineに破棄を依頼する。通常、実際の破棄は現在のUpdate loopの後で処理される。
    Destroy(enemy);

    // この後にenemyを通常利用する設計は避ける。
    // 「破棄要求済み」を独自状態でも表し、同じ敵への二重処理を止めると堅牢。
}
```

重要な区別:

- `Destroy(component)`: そのComponentだけを破棄する。
- `Destroy(gameObject)`: GameObjectと付随Componentを破棄する。
- `Destroy(obj, seconds)`: 遅延破棄を予約する。ゲーム進行時間との関係をAPI資料で確認する。
- `DestroyImmediate`: 主にEditor用。Play中の通常ロジックで安易に使わない。列挙中の即時破棄は状態を壊しやすい。

破棄前後の代表的な流れは`OnDisable`、`OnDestroy`ですが、「必ず保存処理が完了する場所」と決めつけてはいけません。重要データは明示的な保存トランザクションで確定します。

## 4. 参照切れを防ぐ所有権

高速アクションでCharacterを交代するとき、CameraやUIが旧Characterの`Transform`を握り続ける事故が起きます。個々のSystemがScene内オブジェクトを勝手に検索するのではなく、Player層が現在Characterを公開し、変更通知と解除を対にします。

```csharp
using System;
using UnityEngine;

public sealed class PlayerParty : MonoBehaviour
{
    public Transform CurrentCharacter { get; private set; }
    public event Action<Transform> CurrentCharacterChanged;

    public void SwitchTo(Transform next)
    {
        if (next == null || next == CurrentCharacter)
        {
            return;
        }

        CurrentCharacter = next;
        CurrentCharacterChanged?.Invoke(next);
    }
}
```

購読側は`OnEnable`で登録したら`OnDisable`で解除します。ラムダ式をその場で二度書くと別delegateになり解除できないため、同じメソッドまたは保持したdelegateを使います。

## 5. Unity Serializationの目的

Unity Serializationは、Inspector編集値、Scene、Prefab、Assetなどを保存・復元する仕組みです。一般的なJSON SerializerやC#の実行時オブジェクトグラフ保存と同一ではありません。

```csharp
using UnityEngine;

public sealed class AttackSettings : MonoBehaviour
{
    // Inspectorに公開しつつ、外部コードからの直接代入は禁止する。
    [SerializeField] private float damage = 10.0f;

    // propertyは通常のUnity field serialization対象ではない。
    public float Damage => damage;

    // staticはインスタンス保存値ではない。
    public static int GlobalAttackCount;

    // 明示的に保存対象外。
    [System.NonSerialized] public float RuntimeComboTimer;
}
```

基本規則:

- fieldを保存する。通常のpropertyは直接の対象にしない。
- `public` field、または`[SerializeField] private` fieldを使う。
- `static`、`const`、`readonly`などは用途と対応状況を区別する。
- Unityが対応するprimitive、enum、Unity型、配列、`List<T>`、`[Serializable]`型などを使う。
- 多次元配列、入れ子Container、`Dictionary`などは標準field serializationでそのまま扱えない場合がある。対応版の仕様を確認し、SerializableなList表現へ変換することも検討する。

## 6. Inspector値とfield initializer

```csharp
[SerializeField] private int maxHp = 100;
```

`= 100`は新規Component等の初期値にはなりますが、すでにScene/Prefabへ保存された`maxHp`がある場合、ロード時は保存値が復元されます。「コードを100から200へ変えたのにInspectorでは100のまま」は故障ではありません。既存serialized dataをどう移行するかが別途必要です。

field名変更には`FormerlySerializedAs`、データ構造の変更には移行処理やAsset更新を検討します。安易なrenameは既存データ喪失につながります。

## 7. 値として保存されるclassとSerializeReference

通常のcustom serializable classは値としてインライン保存され、同じC#インスタンスを複数fieldから参照する同一性や、多態的な派生型を期待通り保持できないことがあります。参照としてのmanaged object graphや多態性が必要なら`[SerializeReference]`を検討します。

```csharp
using System;
using UnityEngine;

[Serializable]
public abstract class ActionNode
{
    public abstract void Execute();
}

[Serializable]
public sealed class DamageNode : ActionNode
{
    [SerializeField] private float damage;
    public override void Execute() => Debug.Log($"Damage: {damage}");
}

public sealed class ActionAssetHost : MonoBehaviour
{
    // 派生型情報を含むmanaged referenceとして保存する意図を明示する。
    [SerializeReference] private ActionNode root;
}
```

ただし、編集UI、型名変更、AOT/strip、循環構造、差分管理を含む運用コストがあります。単純な攻撃値ならScriptableObjectや明示的なenum+dataの方が保守しやすい場合があります。

## 8. Serialization callback

`ISerializationCallbackReceiver`の`OnBeforeSerialize`/`OnAfterDeserialize`は、保存不能な構造を保存可能な補助Listへ変換する用途などに使えます。ただしcallbackの呼出文脈やスレッドについて、通常Gameplay callbackと同じだと仮定してUnity APIを大量に呼ばないでください。変換は小さく決定的にし、Gameplay初期化は`Awake`等の責務へ分けます。

## 9. Domain Reloadを無効化したときのstatic事故

EditorのPlay Mode設定でDomain Reloadを省略するとPlay開始が速くなる一方、static fieldやstatic eventが前回Playの値を保持し得ます。結果として購読が二重化し、攻撃1回でdamage callbackが2回動くことがあります。

対策:

- staticを「必ずゼロから始まる」と仮定しない。
- subsystem registration等、利用版に適した初期化地点で明示的にresetする。
- event登録と解除を対にする。
- Domain Reload有効・無効の両方でtestする。

## 10. 実戦チェックリスト

- 参照の所有者と破棄責任が説明できるか。
- `Destroy`後の参照を同frameで再利用していないか。
- Scene遷移後も旧Sceneのdelegateが残っていないか。
- Inspector値とコード初期値を混同していないか。
- renameで既存Prefab/Sceneデータが消えないか。
- runtime stateをPrefab assetへ誤ってApplyしていないか。
- static cache/eventをPlay再開時にresetできるか。

## 公式資料

- [Unity Manual: Script serialization](https://docs.unity3d.com/6000.0/Documentation/Manual/script-serialization.html)
- [Unity Scripting API: Object.Destroy](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/Object.Destroy.html)
- [Unity Manual: Enter Play mode details](https://docs.unity3d.com/6000.0/Documentation/Manual/domain-reloading.html)

