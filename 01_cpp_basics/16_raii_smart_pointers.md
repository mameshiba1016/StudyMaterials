# RAII・所有権・スマートポインタ

RAII（Resource Acquisition Is Initialization）は、リソースの所有をオブジェクトの寿命へ結び付けるC++の中心的な設計です。コンストラクタなどで取得し、デストラクタで確実に解放します。

## RAIIが必要な理由

```cpp
void UnsafeFunction()
{
    Resource* resource{AcquireResource()};

    if (HasError())
    {
        return; // ReleaseResourceを忘れる経路。
    }

    ReleaseResource(resource);
}
```

手動解放は、早期return、例外、将来の変更で抜け落ちます。RAIIラッパーならスコープを抜ける際のデストラクタが解放を担当します。

## `std::unique_ptr`

```cpp
#include <memory>

std::unique_ptr<Player> player{std::make_unique<Player>()};
player->Update();
```

一つの`unique_ptr`が対象を排他的に所有します。コピーは禁止され、ムーブで所有権を移せます。

```cpp
std::unique_ptr<Player> CreatePlayer()
{
    return std::make_unique<Player>(); // 値として所有権を呼び出し元へ渡す。
}
```

所有者が明確で、破棄時に自動で`delete`されます。可能なら動的確保自体を避けて値として持ち、動的多態性や独立した寿命が必要なときに`unique_ptr`を検討します。

## `std::move`

```cpp
std::unique_ptr<Player> first{std::make_unique<Player>()};
std::unique_ptr<Player> second{std::move(first)};
```

`std::move`自体はデータを移動しません。式を右辺値として扱えるよう変換し、型のムーブ処理を選べるようにします。移動後の`first`は有効だが値が規定される範囲が型次第です。`unique_ptr`では空になります。移動後オブジェクトは、破棄または明示的に許された操作だけに使います。

## `std::shared_ptr`

複数の`shared_ptr`が共有所有し、共有所有者数が0になると対象を破棄します。

```cpp
std::shared_ptr<Resource> resource{std::make_shared<Resource>()};
std::shared_ptr<Resource> anotherOwner{resource};
```

便利ですが、所有者が誰か曖昧になり、参照カウント更新コスト、制御ブロック、循環参照の問題があります。「寿命を考えたくないから」使うものではなく、本当に共有所有が要件の場合だけ使います。

## `std::weak_ptr`

`shared_ptr`の対象を所有せず観測し、循環参照を断つために使います。

```cpp
std::weak_ptr<Resource> observer{resource};

if (std::shared_ptr<Resource> locked{observer.lock()})
{
    locked->Use(); // このスコープ中はshared_ptrが寿命を保持。
}
```

`expired()`を確認してから別途使用する間に他スレッドで破棄される可能性があるため、`lock()`の結果を使用します。

## カスタムデリータ

ファイルハンドル、GPUリソース、C APIのオブジェクトなど、`delete`以外で解放する対象もスマートポインタや専用RAIIクラスで管理できます。ただしGPUリソースの解放にはスレッドやコマンド完了順序など固有条件があり、単純な`delete`代替だけでは足りない場合があります。

## 所有権の設計語彙

- 値：そのオブジェクトが直接所有。
- `unique_ptr<T>`：排他的な動的所有。
- `shared_ptr<T>`：明確な共有所有。
- `weak_ptr<T>`：共有対象への非所有観測。
- `T*`・`T&`：多くの場合、寿命を延長しない非所有アクセス。
- ID・ハンドル：管理システム経由で対象を再検索し、世代番号などで削除後再利用を検出可能。

ゲームでは大量オブジェクトを中央管理し、外部はIDやハンドルを保持する設計も多く使われます。すべてを`shared_ptr`へすれば安全になるわけではありません。

## Unreal Engineとの違い

`UObject`派生型はUnrealのガベージコレクション規則に従います。標準の`unique_ptr`や`shared_ptr`を`UObject`所有へ機械的に使用してはいけません。`TObjectPtr`、`TWeakObjectPtr`、`UPROPERTY`などエンジンの追跡規則を専用章で扱います。一方、非`UObject`の通常C++型ではUnrealの`TUniquePtr`等が適する場合があります。
