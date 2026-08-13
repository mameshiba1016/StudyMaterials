# 03 GameObject、Component、Transform、Prefab

## 1. GameObjectはComponentの容器

GameObject自身は名前、Active状態、Layer、Tagを持ち、機能はComponentを組み合わせて作ります。すべてのGameObjectはTransformを持ちます。

```text
Player GameObject
├─ Transform
├─ CharacterController
├─ PlayerInput
├─ ActionCharacter（自作MonoBehaviour）
└─ TargetingComponent（自作MonoBehaviour）
```

継承でPlayer／Enemyの全組合せを作るより、ComponentとPure C# Serviceを組み合わせます。

## 2. Component取得

```csharp
private CharacterController characterController;

private void Awake()
{
    if (!TryGetComponent(out characterController))
    {
        Debug.LogError("CharacterController is required.", this);
        enabled = false;
    }
}
```

構成が固定なら`Awake`で一度取得し、毎Frame`GetComponent`しません。`[RequireComponent]`で必須構成も宣言できます。

## 3. SerializeField

```csharp
[SerializeField] private Transform weaponSocket;
[SerializeField, Min(0.0f)] private float moveSpeed = 6.0f;
```

Inspector公開のためFieldを`public`にせず、変更経路を制限します。Runtime状態と設定Dataを区別します。

## 4. Transform階層

```csharp
Vector3 worldPosition = transform.position;
Vector3 localPosition = transform.localPosition;
Vector3 worldPoint = transform.TransformPoint(localPoint);
Vector3 localDirection = transform.InverseTransformDirection(worldDirection);
```

位置と方向、WorldとLocalを混ぜません。親の非一様ScaleはCollider、Rotation、子Transformへ予想外の影響を与えるため、Character RootのScaleは原則1を維持します。

## 5. Activeとenabled

- `gameObject.SetActive(false)`：GameObject階層を非Active化。
- `Behaviour.enabled = false`：特定Behaviourを無効化。
- `activeSelf`：自身の設定。
- `activeInHierarchy`：親を含め実際にActiveか。

非Active化で`OnDisable`が呼ばれ、Update等が止まります。Object Poolでは破棄せずActiveを切り替えるため、再利用時の状態Resetが必要です。

## 6. InstantiateとDestroy

```csharp
GameObject instance = Instantiate(prefab, position, rotation, parent);
Destroy(instance);
```

大量の弾やVFXを頻繁にInstantiate／DestroyするとNative／Managed両面の負荷になります。計測後にPoolを使います。

## 7. Prefab

Prefabは再利用可能なGameObject階層Assetです。Scene InstanceはPrefabとの接続とOverrideを持ちます。Prefab Variantで共通Enemyから能力差を派生できます。

PrefabはC++のClass定義と完全に同じではありません。Serialized Object GraphとAsset参照を持つTemplateとして理解します。

## 8. 参照方法

優先候補：Inspectorで明示参照、同一ObjectのComponent Cache、生成時Injection、Registry／Service。`Find`、Tag全検索、`Camera.main`等を毎Frameの中心処理へ置きません。

## 9. Character交代

交代CharacterをPrefabから生成する場合、先読み、Spawn位置検証、入力接続、Camera Target、UI購読、旧Character無効化をTransactionとして扱います。Prefab生成完了だけで戦闘可能とみなしません。
