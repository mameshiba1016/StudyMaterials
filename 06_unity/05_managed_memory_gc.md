# 05 値型、参照型、Managed Memory、GC

## 1. 値型と参照型

- 値型：`int`、`float`、`bool`、`struct`、`enum`。値そのものを保持。
- 参照型：`class`、配列、`string`、Delegate。Objectへの参照を保持。

```csharp
Vector3 a = new(1, 2, 3); // Vector3はstruct。
Vector3 b = a;            // 値がCopyされる。
b.x = 99;                 // a.xは変化しない。

EnemyData x = new();      // class Instance。
EnemyData y = x;          // 同じObjectへの参照をCopy。
y.health = 0;             // xから見てもhealthは0。
```

## 2. StackとManaged Heap

Method呼出の局所値等はScripting Stackで管理され、参照型ObjectはManaged Heapへ確保されます。ただしJIT／IL2CPP最適化やEscape等があるため「structなら必ず物理Stack」と断定せず、意味上の値／参照Semanticsを理解します。

## 3. GC Allocation

Heap上で不要になり到達不能となったManaged ObjectはGCが回収します。回収時の走査はFrame Timeへ影響するため、毎Frameの不要Allocationを減らします。

Allocation例：

- `new`によるclass／配列。
- 文字列連結。
- LINQの一部処理。
- Closureを生成するLambda。
- Boxing。
- Collectionの容量拡張。

Profilerの`GC.Alloc`で実測します。

## 4. Boxing

```csharp
int value = 42;
object boxed = value;       // 値型をobjectとしてHeapへBoxing。
int restored = (int)boxed;  // Unboxing。
```

Interface／object引数、非Generic Collection、Format等で暗黙Boxingが起き得ます。高頻度Loopで確認します。

## 5. Collection再利用

```csharp
private readonly List<Enemy> candidates = new(64);

private void CollectCandidates()
{
    candidates.Clear(); // Capacityは維持して再利用。
    targetingWorld.FillCandidates(candidates);
}
```

返り値として毎Frame新Listを作るより、呼出側Bufferへ書き込むAPIが有効です。ただし共有Bufferの再入と所有権を明確にします。

## 6. UnityEngine.Objectは二重の寿命

Unity ObjectはManaged WrapperとNative C++ Counterpartを持ちます。`Destroy(component)`はNative側を破棄予約しますが、Managed Wrapperは参照がなくなりGCされるまで存在し得ます。

```csharp
Destroy(target);

if (target == null)
{
    // UnityがOverloadした==では破棄済みObjectもnull相当になり得る。
}
```

`?.`や`??`はUnityのOverloadした`==`を使わないため、破棄済みUnity Objectで同じ判定にならない点へ注意します。

## 7. Destroyは即時C#解放ではない

`Destroy`はGameObject／Component等を破棄しますが、C#の参照変数を全て自動でnullへする意味ではありません。Event、static Field、ListがManaged Wrapperを保持し続けることがあります。

## 8. Native Resource

GCは`NativeArray`、Job Resource、File／Stream等の決定的解放を代行する設計ではありません。`Dispose`、`using`、Ownerの`OnDestroy`等で解放します。

```csharp
using NativeArray<float> values = new(128, Allocator.TempJob);
// Scope終了時にDispose。
```

実際のAPIとC# Versionで利用可能な構文を確認します。

## 9. GC.Collectを常用しない

手動GCや`Resources.UnloadUnusedAssets`は重い処理です。Allocation原因を直さず毎Frame呼びません。Loading Screen等、停止を許容できる場所で計測して使います。

## 10. 高速アクションのAllocation対策

- Physics QueryはNonAlloc APIまたは再利用Bufferを検討。
- Target候補Listを再利用。
- Hitごとの文字列Logを通常Buildで生成しない。
- Damage Number／VFXをPool。
- Coroutineの大量開始を避ける。
- UIの毎Frame文字列FormatをEvent更新へ。

## 11. 参考

- [Unity 6：Managed Memory](https://docs.unity3d.com/jp/current/Manual/performance-managed-memory-introduction.html)
- [UnityEngine.Objectの特殊な寿命](https://docs.unity3d.com/cn/6000.0/Manual/class-Object.html)
