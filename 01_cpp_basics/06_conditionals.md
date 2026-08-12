# 条件分岐

条件分岐は、実行時の状態によって処理経路を選びます。ゲームでは「HPが0以下なら死亡」「攻撃ボタンが押され、行動可能なら攻撃」のようなルールを表現します。

## `if`・`else if`・`else`

```cpp
int hp{35};

// 丸括弧内の条件をboolとして評価します。
if (hp <= 0)
{
    // 条件がtrueの場合だけ実行されます。
    std::cout << "Defeated\n";
}
else if (hp <= 30)
{
    // 最初の条件がfalseで、この条件がtrueの場合だけ実行されます。
    std::cout << "Danger\n";
}
else
{
    // 先行する条件がすべてfalseの場合に実行されます。
    std::cout << "Battle continues\n";
}
```

この例では`hp`が35なので最後の`else`です。一つの連鎖では最初に成立した枝だけが実行されます。

## 条件に使える値

`bool`が基本ですが、整数やポインタなどは条件文脈で真偽へ変換されます。整数の0は`false`、0以外は`true`です。ただし、意味を明確にするため比較を明示する場合があります。

```cpp
int remainingEnemies{3};

if (remainingEnemies > 0)
{
    // 敵が残っていることが明確に読める。
}
```

## 波括弧を省略しない

本体が一文なら波括弧を省略できますが、後から行を追加した際の事故や読み違いを避けるため、本教材では原則として書きます。

```cpp
if (isInvincible)
{
    damage = 0;
}
```

## `switch`

一つの整数・列挙値を複数の定数候補と比較する場合に適します。

```cpp
enum class CharacterState
{
    Idle,
    Move,
    Attack,
    Dead
};

CharacterState state{CharacterState::Attack};

switch (state)
{
case CharacterState::Idle:
    std::cout << "Idle\n";
    break; // switchを抜ける。省略すると次のcaseへ処理が流れる。

case CharacterState::Move:
    std::cout << "Move\n";
    break;

case CharacterState::Attack:
    std::cout << "Attack\n";
    break;

case CharacterState::Dead:
    std::cout << "Dead\n";
    break;
}
```

すべての列挙値を列挙したい場合、安易に`default`を置かないことで、新しい状態を追加した際にコンパイラ警告を得られる場合があります。外部入力など未知値を扱う設計では`default`が必要なこともあります。

## 条件演算子

```cpp
int hp{20};
const char* message{hp > 0 ? "Alive" : "Dead"};
```

`条件 ? true時の式 : false時の式`という形です。値を選択する短い処理に向きます。副作用を含む複雑な分岐を無理に一行へ詰め込まず、通常の`if`を使います。

## ガード節

異常・不成立条件で早期に関数を抜けると、正常処理のネストを浅くできます。

```cpp
void TryAttack(bool isAlive, bool hasStamina)
{
    if (!isAlive)
    {
        return;
    }

    if (!hasStamina)
    {
        return;
    }

    // ここでは「生存中かつスタミナあり」という前提が成立しています。
    std::cout << "Attack\n";
}
```

## よくある失敗

- `==`のつもりで`=`を書き、条件内で代入する。
- 浮動小数点値を完全一致で比較する。
- 条件を二重否定だらけにして読みにくくする。
- 状態の組み合わせを大量の`bool`で表し、不可能な状態まで作れるようにする。
- `if`の順番が仕様の優先順位になっていることを見落とす。
