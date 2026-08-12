# 継承・仮想関数・多態性

継承は既存型を基底として新しい型を定義します。公開継承は原則として「派生型は基底型として正しく扱える」というis-a関係を表します。単なるコード再利用のためだけに使うと密結合になりやすいため、コンポジションと比較します。

## 公開継承

```cpp
class Actor
{
public:
    void SetPosition(float x, float y)
    {
        x_ = x;
        y_ = y;
    }

private:
    float x_{};
    float y_{};
};

class Enemy : public Actor
{
public:
    void Attack() {}
};
```

`Enemy`は`Actor`の公開インターフェースを持ち、`Actor&`や`Actor*`が必要な場所で扱えます。

## 仮想関数

```cpp
class Actor
{
public:
    virtual ~Actor() = default;

    virtual void Update(float deltaSeconds)
    {
        // 基底の既定処理。
    }
};

class Enemy final : public Actor
{
public:
    void Update(float deltaSeconds) override
    {
        // Enemy固有の処理。
    }
};
```

`Actor&`や`Actor*`経由で`Update`を呼んでも、実際の動的型が`Enemy`ならオーバーライド版が選ばれます。これが実行時多態性です。実装では仮想関数テーブルと隠れたポインタを使うことが一般的ですが、規格がその具体方式を義務付けているわけではありません。

## 仮想デストラクタ

基底ポインタ経由で派生オブジェクトを削除する設計では、基底デストラクタを仮想にします。

```cpp
std::unique_ptr<Actor> actor{std::make_unique<Enemy>()};
```

`Actor`のデストラクタが仮想なら破棄時に`Enemy`から正しい順番でデストラクタが呼ばれます。仮想でない基底経由の削除は未定義動作になり得ます。多態的基底は通常、公開仮想デストラクタまたは用途に合う保護された非仮想デストラクタを設計します。

## `override`・`final`

- `override`：基底の仮想関数を正しく上書きしていることをコンパイラに検査させる。
- `final`：それ以上のオーバーライドまたはクラス継承を禁止する。

引数の`const`や末尾`const`が違って意図せず別関数になる事故を防ぐため、オーバーライドには必ず`override`を付けます。

## 抽象クラスと純粋仮想関数

```cpp
class Damageable
{
public:
    virtual ~Damageable() = default;
    virtual void ApplyDamage(int amount) = 0;
};
```

`= 0`は純粋仮想関数です。`Damageable`自体は直接生成できず、派生型が契約を実装します。インターフェースとして使えますが、所有権、例外、安全な破棄も設計に含めます。

## オブジェクトスライシング

```cpp
Enemy enemy{};
Actor actor{enemy}; // 値として基底部分だけをコピーし、Enemy固有部分を失う。
```

多態的オブジェクトを値で基底型へコピーするとスライシングが起きます。実行時多態性が必要なら参照や所有スマートポインタを使います。値多態性が必要なら`std::variant`、型消去、clone設計など別手法を検討します。

## キャスト

`dynamic_cast`は多態的階層で実行時に型を検査できますが、頻繁に具体型を判定するコードは抽象化が崩れている兆候かもしれません。`static_cast`は実行時検査をせず、誤ったダウンキャストは危険です。C形式キャストは複数の変換をまとめて試すため、意図が曖昧になりやすく避けます。

## ゲーム設計での選択

ゲームオブジェクトの種類ごとに巨大な継承木を作ると、「飛ぶ敵かつ破壊可能なギミック」のような横断的組み合わせが難しくなります。共通契約に限定した浅い継承、コンポーネント、データ駆動設計、`variant`などを要件に応じて選びます。Unreal Engineでは`UObject`・`AActor`のフレームワーク継承が必要な場面がありますが、ゲーム機能まで無制限に継承へ寄せる必要はありません。
