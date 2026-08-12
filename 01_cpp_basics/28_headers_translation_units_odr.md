# ヘッダー・翻訳単位・ODR・リンク

複数ファイルへ分けたC++を理解するには、宣言と定義、翻訳単位、リンケージ、ODRを区別する必要があります。

## 宣言と定義

```cpp
// 宣言：名前と型を知らせる。同じ対象を指す宣言は複数回可能。
int CalculateScore(int baseScore, int multiplier);

// 定義：関数本体を与える。
int CalculateScore(int baseScore, int multiplier)
{
    return baseScore * multiplier;
}
```

変数では、領域を確保する定義と、別場所の定義を参照する`extern`宣言があります。

```cpp
// GameConfig.h
extern int gDifficulty;

// GameConfig.cpp
int gDifficulty{1};
```

変更可能なグローバル変数は依存関係、初期化順、スレッド安全性を悪化させるため、設定所有者や明示的な引数を優先します。

## 翻訳単位

各`.cpp`は、取り込んだヘッダーとともに独立してコンパイルされ、オブジェクトファイルになります。ある`.cpp`でだけ見える宣言は、別の`.cpp`へ自動共有されません。ヘッダーが宣言の共有契約になります。

## One Definition Rule（ODR）

プログラム内で、非`inline`関数や外部リンケージを持つ変数の定義は原則一つです。ヘッダーへ普通の関数定義を書き、複数`.cpp`からincludeすると多重定義リンクエラーになり得ます。

```cpp
// ヘッダーへ置けるinline関数。
inline int Double(int value)
{
    return value * 2;
}
```

`inline`の現代的に重要な意味は、複数翻訳単位に同一定義を置くことをODR上許す点です。コンパイラへ必ず命令を埋め込ませる保証ではありません。クラス定義内で定義されたメンバー関数は暗黙に`inline`です。

```cpp
inline constexpr int DefaultHealth{100}; // C++17のinline変数。
```

## 前方宣言

ポインタや参照だけを宣言する場合、完全な型定義をincludeせず前方宣言できることがあります。

```cpp
class Weapon;

class Player
{
public:
    void Equip(Weapon& weapon);
private:
    Weapon* equippedWeapon_{nullptr};
};
```

値メンバー、継承、`sizeof`、メンバーアクセスなど完全型が必要な操作では定義をincludeします。スマートポインタと不完全型では、デストラクタが実体化される位置にも注意が必要です。

## インクルード循環

`Player.h`が`Weapon.h`をincludeし、`Weapon.h`が`Player.h`をincludeする設計は順序依存のエラーを生みます。前方宣言、インターフェース抽出、依存方向の整理、実装を`.cpp`へ移すことで解消します。

## include what you use

自分のヘッダーが使用する型の定義・宣言を、別ヘッダーが偶然includeしてくれることへ依存しません。推移的includeはライブラリ更新で変わります。各`.cpp`では対応する自作ヘッダーを最初にincludeすると、そのヘッダー単独で必要情報を持つか検査しやすくなります。

## 静的初期化順序問題

異なる翻訳単位にある動的初期化を要する名前空間スコープオブジェクト同士の初期化順は安全に依存できない場合があります。

```cpp
Registry& GetRegistry()
{
    static Registry instance{};
    return instance;
}
```

関数ローカルstaticで初回利用時に構築する方法がありますが、グローバル状態の依存や終了時破棄順まで自動的に良設計になるわけではありません。

## ABI

コンパイラ、バージョン、標準ライブラリ、設定が異なるバイナリ間では、名前修飾、クラス配置、例外、ランタイムライブラリなどのABIが一致しない場合があります。DLL境界へSTL型や所有権を渡す設計では特に注意し、同じツールチェーンの使用やC ABIの境界、明示的な破棄APIを検討します。
