# 文字・文字列・エンコーディング

文字列はゲームのUI、ログ、ファイル、通信、ローカライズで使います。C++の文字列型だけでなく、文字コードとエンコーディングの理解が不可欠です。

## 文字と文字列リテラル

```cpp
char grade{'A'};                  // 単一のchar。シングルクォート。
const char* text{"Player"};      // null終端文字列へのポインタ。ダブルクォート。
std::string name{"Anby"};        // std::stringが文字列データを管理。
```

`'A'`と`"A"`は異なります。後者は`A`の後ろに終端を示すnull文字`'\0'`を持つ配列リテラルです。

## `std::string`

```cpp
#include <string>

std::string playerName{"Player"};
playerName += " One";

std::size_t length{playerName.size()}; // charコード単位の数。人間が見る文字数とは限らない。
bool empty{playerName.empty()};
```

`std::string`は動的な文字列を所有・管理します。実装によっては短い文字列をオブジェクト内部へ格納するSmall String Optimizationを使いますが、規格上その実装は保証されません。

## C文字列

```cpp
const char message[]{'H', 'i', '\0'};
```

多くのC APIはnull文字までを文字列と解釈します。終端がなければ範囲外を読み続ける危険があります。`std::string::c_str()`はnull終端データへのポインタを取得できますが、文字列の変更などで無効化される可能性があるため保存期間に注意します。

## `std::string_view`

```cpp
#include <string_view>

void PrintLabel(std::string_view label)
{
    std::cout << label << '\n';
}
```

文字列を所有せず、既存の連続した文字範囲を参照する軽量ビューです。コピーを避けられますが、参照先より長生きするとダングリングします。必ずしもnull終端ではないため、C APIへ`.data()`をそのまま渡せるとは限りません。

## UTF-8と「文字数」

UTF-8では一つのUnicodeコードポイントが複数バイトになる場合があります。さらに、ユーザーが一文字と感じる書記素が複数コードポイントから成ることもあります。そのため`std::string::size()`は通常バイト数であり、画面上の文字数ではありません。

```cpp
std::string japanese{u8"日本"};
// C++の言語バージョンによってu8リテラルの型が異なります。
// size()を「2文字」とみなすコードを書いてはいけません。
```

文字の切り詰め、カーソル移動、改行、フォント描画、正規化、右から左の文字体系などは、Unicode対応ライブラリやゲームエンジンのテキストシステムを利用します。

## 数値との変換

```cpp
int score{1200};
std::string scoreText{std::to_string(score)};
```

文字列から数値への変換には`std::stoi`系や`std::from_chars`などがあります。外部入力は失敗し得るため、例外、エラーコード、範囲外、末尾の不要文字を処理します。セーブデータを文字列連結だけで自作解析するより、仕様の定まった形式とライブラリを使います。

## ローカライズ

画面文言をコードへ直接埋め込まず、安定したキーと翻訳データへ分離します。語順、複数形、性、数字・日時、フォント、禁則処理を考慮します。Unreal Engineでは`FString`、`FName`、ローカライズ対象の`FText`が別の目的を持ち、標準の`std::string`と同一ではありません。
