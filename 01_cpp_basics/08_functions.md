# 関数

関数は、名前を付けた処理単位です。重複を減らすだけでなく、前提・入力・出力・責任の境界を表します。

## 定義と呼び出し

```cpp
// int：戻り値の型。
// CalculateDamage：関数名。
// int attack, int defense：呼び出し元から値を受け取る仮引数。
int CalculateDamage(int attack, int defense)
{
    int damage{attack - defense};

    if (damage < 0)
    {
        damage = 0;
    }

    // 計算結果を呼び出し元へ返します。ここでこの関数の実行は終了します。
    return damage;
}

int main()
{
    // 40と15は実引数。戻り値25でresultを初期化します。
    int result{CalculateDamage(40, 15)};
    return 0;
}
```

## `void`

呼び出し元へ値を返さない関数の戻り値型です。

```cpp
void PrintDamage(int damage)
{
    std::cout << "Damage: " << damage << '\n';
}
```

`void`関数でも`return;`を使って早期終了できます。

## 値渡し

```cpp
void ApplyLocalChange(int value)
{
    value = 0; // 仮引数として作られた別のintを変更する。
}

int hp{100};
ApplyLocalChange(hp);
// hpは100のままです。
```

値渡しは関数用の値を作るため、関数内の変更が元のオブジェクトへ直接反映されません。`int`など小さな型では分かりやすく安全な基本選択です。

## 参照渡し

```cpp
void ApplyDamage(int& hp, int damage)
{
    // hpは呼び出し元オブジェクトの別名なので、元の値を変更します。
    hp -= damage;

    if (hp < 0)
    {
        hp = 0;
    }
}
```

変更しない大きなオブジェクトは`const T&`で受け取ることがあります。所有権を受け取らず、nullを許さず、元を変更しない意図を表せます。小さな型は値渡しの方が単純です。

## 宣言と定義

```cpp
// 宣言：関数の名前と型をコンパイラへ知らせます。
int Add(int left, int right);

int main()
{
    return Add(2, 3);
}

// 定義：実際の処理本体です。
int Add(int left, int right)
{
    return left + right;
}
```

ヘッダーへ宣言、`.cpp`へ定義を置くファイル分割の基礎になります。

## オーバーロード

同じスコープで同じ名前を使い、仮引数の型・個数が異なる関数を定義できます。

```cpp
int Max(int a, int b);
float Max(float a, float b);
```

戻り値型だけを変えたオーバーロードはできません。呼び出し側が戻り値を使わない場合、どちらを呼ぶか決められないためです。

## デフォルト引数

```cpp
void PlaySound(float volume = 1.0F);

PlaySound();     // 1.0Fを使用。
PlaySound(0.5F); // 0.5Fを使用。
```

通常は宣言側へ一度だけ書きます。引数の意味が不明瞭になる場合、設定構造体や明確な関数名を検討します。

## 再帰

関数が自分自身を呼び出す方法です。木構造の走査などを自然に表せますが、終了条件が必須です。各呼び出しは通常コールスタックを消費し、深すぎればスタックオーバーフローを起こします。ゲームの毎フレーム処理では深さとコストを把握します。

## 良い関数の目安

- 名前から目的が分かる。
- 一つの抽象度と責任に集中する。
- 前提条件と副作用が明確である。
- 無関係なグローバル状態へ依存しない。
- 入力を不必要に変更しない。
- 単体でテストしやすい。

行数だけで良し悪しは決まりません。短くても隠れた副作用の多い関数は扱いにくく、ある程度長くても一つの明確なアルゴリズムを表す関数は理解しやすい場合があります。
