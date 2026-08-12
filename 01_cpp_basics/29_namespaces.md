# 名前空間・名前探索・`using`

名前空間は識別子を論理的にまとめ、ライブラリや機能間の名前衝突を防ぎます。

## 名前空間の定義

```cpp
namespace combat
{
    int CalculateDamage(int attack, int defense)
    {
        return attack > defense ? attack - defense : 0;
    }
}

int damage{combat::CalculateDamage(30, 10)};
```

`combat::`でどの名前空間の関数かを明示します。名前空間は同名で複数箇所に分けて定義でき、内容が一つの名前空間へ集約されます。

## 入れ子名前空間

```cpp
namespace study::game::ai
{
    class BehaviorTree {};
}
```

C++17以降の簡潔な記法です。プロジェクト、モジュール、サブシステムなど安定した分類へ使います。ディレクトリ階層を機械的にすべて写して深くしすぎないようにします。

## `using`宣言

```cpp
using std::string;
string name{"Player"};
```

特定の名前だけを現在のスコープへ導入します。短い関数内など限定範囲なら読みやすさに役立ちます。

## `using namespace`

```cpp
using namespace std;
```

名前空間内の多くの候補を名前探索へ加えます。特にヘッダーで使うと、そのヘッダーをincludeした全コードへ影響し、曖昧性や将来の名前衝突を生みます。ヘッダーでは使用しません。`.cpp`の狭いスコープでも、教育・共同開発では`std::`を明示すると由来が分かりやすくなります。

## 名前空間エイリアス

```cpp
namespace fs = std::filesystem;
fs::path path{"save.dat"};
```

長い名前空間を明確な短縮名へできます。ヘッダーで公開すると利用者の名前空間へ影響するため配置を考えます。

## 無名名前空間

```cpp
namespace
{
    constexpr int InternalBufferSize{256};
    void InternalHelper() {}
}
```

名前へ内部リンケージを与え、その翻訳単位だけで使う実装詳細を表します。ヘッダーへ置くと翻訳単位ごとに別実体を作るため、通常は`.cpp`で使います。

## ADL（引数依存名前探索）

未修飾の関数呼び出しでは、通常の探索に加えて引数型に関連する名前空間も探索されます。

```cpp
using std::swap;
swap(left, right);
```

この慣用形により、型と同じ名前空間にある専用`swap`をADLで見つけ、なければ`std::swap`を使えます。`std`名前空間へ勝手な関数や型を追加することは原則禁止です。ユーザー定義型に対する標準が許可した特殊化だけ、厳密な規則に従います。

## Unreal Engineとの関係

UnrealのReflection対象型には命名・コード生成上の制約があり、`UCLASS`などを名前空間内へ自由に置けないケースがあります。通常C++ヘルパーとReflection型で扱いが異なるため、エンジンの対応バージョンの規則を確認します。
