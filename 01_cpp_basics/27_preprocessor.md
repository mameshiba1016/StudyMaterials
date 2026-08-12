# プリプロセッサ

プリプロセッサは、C++のコンパイルより前にソーステキストを処理します。`#include`、マクロ、条件付きコンパイルなどを担当します。型を理解しないテキスト処理なので、現代C++の型安全な機能で置き換えられる場面ではそちらを優先します。

## `#include`

```cpp
#include <vector>      // 実装の検索パスから標準・外部ヘッダーを探す慣例。
#include "Player.h"   // 現在のプロジェクト側ヘッダーを探す慣例。
```

概念的には、指定ヘッダーの内容をその位置へ取り込みます。一つの`.cpp`と、そこから取り込まれたヘッダー群がプリプロセスされて翻訳単位になります。`#include`はコンパイル済みライブラリをリンクする命令ではありません。

## オブジェクト形式マクロ

```cpp
#define MAX_ENEMIES 100
```

これは型を持つ変数ではなく、後続トークンを置換します。スコープも通常のC++規則に従いません。定数には次を優先します。

```cpp
inline constexpr int MaxEnemies{100};
```

型、スコープ、デバッガでの可視性、言語規則を利用できます。

## 関数形式マクロの危険

```cpp
#define SQUARE(x) x * x

int wrong{SQUARE(2 + 3)};
// 展開結果は 2 + 3 * 2 + 3 となり、25ではなく11。
```

括弧を増やしても、副作用を持つ引数が複数回評価される問題があります。

```cpp
#define DOUBLE(x) ((x) + (x))
int value{2};
int result{DOUBLE(++value)}; // ++valueが複数回現れ、意図が危険。
```

型安全な`constexpr`関数やテンプレートを使います。

```cpp
template<typename T>
constexpr T Square(T value)
{
    return value * value;
}
```

## 条件付きコンパイル

```cpp
#if defined(_DEBUG)
    std::cout << "Debug build\n";
#endif
```

プラットフォーム、コンパイラ、機能、ビルド設定によってコードを切り替えます。ただし条件が増えると、実際にコンパイルされない経路が腐敗します。可能なら共通インターフェースの実装ファイルをビルドシステム側で選択します。

```cpp
#if defined(_WIN32)
    // Windows固有。
#elif defined(__linux__)
    // Linux固有。
#else
    #error Unsupported platform
#endif
```

識別子の二重アンダースコア、先頭アンダースコア＋大文字などは実装予約です。独自マクロへ使いません。

## `#pragma once`とインクルードガード

```cpp
#ifndef STUDY_MATERIALS_PLAYER_H
#define STUDY_MATERIALS_PLAYER_H

class Player {};

#endif
```

同じヘッダーが一翻訳単位内で複数回処理されるのを防ぎます。`#pragma once`も広く対応されていますが標準規格の指令ではありません。プロジェクト規約に従います。

## 定義済みマクロ

`__FILE__`や`__LINE__`はログや診断に利用できます。C++20では`std::source_location`により、マクロを減らしながら呼出位置を取得できます。

## Unreal Engineのマクロ

`UCLASS`、`UPROPERTY`、`GENERATED_BODY`などはUnreal Header ToolとReflectionのために必要で、単純に「マクロは悪いから排除」とはできません。エンジンのコード生成規則、配置、対応型、GC追跡の意味を専用章で理解します。
