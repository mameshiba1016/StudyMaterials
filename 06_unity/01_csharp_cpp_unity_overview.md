# 01 C#とC++の違い、Unity Scriptの全体像

## 1. Unityで書くC#はどこで動くか

```text
.cs Source
  ↓ C# Compiler
Managed Assembly（DLL / IL）
  ↓ Mono または IL2CPP
Native Platform上で実行
  ↓ UnityEngine API境界
Unity Native Engine（主にC++）
```

EditorではC# ScriptがUnityのNative EngineへAPIを通じて命令します。`Transform.position`のようなPropertyアクセスも、単なる自作ClassのField読取とはCostと境界が異なる場合があります。

## 2. 最初のScript

```csharp
using UnityEngine; // UnityEngine Namespace内の型を短い名前で使う。

public sealed class Rotator : MonoBehaviour // ComponentとしてGameObjectへ付けるClass。
{
    [SerializeField] // privateのままInspectorへSerialization対象として公開する。
    private float degreesPerSecond = 90.0f;

    private void Update() // 有効なComponentをUnityが毎Frame呼ぶMessage。
    {
        float deltaAngle = degreesPerSecond * Time.deltaTime;
        transform.Rotate(0.0f, deltaAngle, 0.0f, Space.Self);
    }
}
```

- `using`はC++の`#include`ではなくNamespace名の省略。
- `:`はここでは`MonoBehaviour`継承。
- `sealed`は派生禁止。
- `[SerializeField]`はAttribute。
- `Update`は自分でLoopから直接呼ばずUnityのPlayer Loopが呼ぶ。

## 3. C++との主な違い

| C++ | C#／Unity |
|---|---|
| HeaderとSourceを分ける | 通常Classを`.cs`へ記述 |
| 手動／RAII中心の資源管理 | Managed ObjectはGC、Native資源は別途解放 |
| Pointer／Reference | Managed Reference中心 |
| Template | Generic |
| Destructor | Finalizerは決定的でない。`IDisposable`等を使う |
| Compile／Link | AssemblyへCompile、Backendで実行／変換 |
| Macro中心の条件分岐 | Attribute、Reflection、Conditional Compilation |

## 4. Property

```csharp
public float CurrentHealth { get; private set; }

public float HealthRatio => MaxHealth > 0.0f
    ? CurrentHealth / MaxHealth
    : 0.0f;
```

PropertyはFieldのように読めますがGetter／Setter処理です。毎FrameのLoop内で重いPropertyを何度も呼ばないよう、実装とCostを理解します。

## 5. DelegateとEvent

```csharp
public event System.Action<float, float> HealthChanged;

private void NotifyHealthChanged()
{
    HealthChanged?.Invoke(currentHealth, maxHealth);
}
```

UIがHealthを毎Frame監視せずEventを購読できます。購読者は寿命に応じて解除し、静的Eventによる参照保持を避けます。

## 6. ExceptionとUnity Callback

Exceptionを通常の状態分岐に使いません。Unity Callback内で未処理Exceptionが出ると、そのFrameの関数処理が中断します。Inspector未設定等は`Awake`で検証し、明確なContext付きで報告します。

## 7. Pure C#とMonoBehaviourを分ける

```csharp
public sealed class DamageCalculator
{
    public float Calculate(float attack, float defense)
    {
        return Mathf.Max(0.0f, attack - defense);
    }
}
```

数式、Combo Rule、State MachineをPure C#へ置くと、SceneなしでTestしやすくなります。MonoBehaviourはUnity Object参照とLifecycleのAdapterを担当します。

## 8. Unity Objectの特殊性

`MonoBehaviour`、`GameObject`、`Texture`等は`UnityEngine.Object`派生で、Managed C# ObjectとNative C++側Objectの関係を持ちます。このため通常C# Objectと同じnull／寿命だと思わないことが重要です。
