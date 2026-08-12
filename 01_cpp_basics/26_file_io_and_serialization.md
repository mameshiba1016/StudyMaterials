# ファイル入出力・パス・シリアライズ

ゲームは設定、セーブ、ステージ、アセットメタデータ、ログを読み書きします。ファイル操作は失敗する前提で設計します。

## テキストファイルの読込

```cpp
#include <fstream>
#include <string>

std::ifstream input{"config.txt"};
if (!input)
{
    std::cerr << "Failed to open config.txt\n";
    return 1;
}

std::string line{};
while (std::getline(input, line))
{
    std::cout << line << '\n';
}

if (input.bad())
{
    std::cerr << "I/O error while reading\n";
}
```

`while (!input.eof())`で読むのは典型的な誤りです。EOFフラグは読み取りが終端を越えて失敗した後に立つため、読み取り操作そのものを条件にします。

## 書込

```cpp
std::ofstream output{"settings.txt", std::ios::trunc};
if (!output)
{
    return 1;
}

output << "volume=0.8\n";
output.flush();

if (!output)
{
    std::cerr << "Write failed\n";
}
```

ファイルを開けても、ディスク満杯、権限、デバイス切断などで後の書込が失敗します。デストラクタによるclose時の失敗は通常取得しづらいため、重要データは明示的に状態を確認します。

## バイナリファイル

```cpp
std::ifstream input{"data.bin", std::ios::binary};
```

バイナリは「高速で安全な構造体コピー」という意味ではありません。構造体の生バイト保存には次の問題があります。

- パディングと未初期化バイト。
- CPUのエンディアン。
- コンパイラ・ABI・型サイズ差。
- ポインタ値は次回起動時に無意味。
- クラスの不変条件や仮想関数情報。
- バージョン更新時の互換性。

フィールドごとに仕様化し、固定幅整数、エンディアン、長さ、バージョン、検証方法を定めます。

## `std::filesystem`

```cpp
#include <filesystem>

namespace fs = std::filesystem;

fs::path saveDirectory{"saves"};
std::error_code error{};
fs::create_directories(saveDirectory, error);

if (error)
{
    std::cerr << error.message();
}
```

パスを単なる`std::string`連結で作らず、`fs::path`と`operator/`を使います。相対パスはプロセスの現在作業ディレクトリに依存し、起動方法で変わるため、実行ファイル、ユーザーデータ、アセットルートなど基準を明確にします。

## セーブデータの安全な更新

1. 新内容を同じファイルへ直接上書きしない。
2. 一時ファイルへ完全に書く。
3. flush・closeとエラーを確認する。
4. 必要ならチェックサムや構造検証を行う。
5. 既存ファイルのバックアップを考慮する。
6. OS上で可能な安全な置換方法を使う。

突然の電源断まで保証するには、ファイルシステムとOSの永続化保証を理解する必要があります。

## シリアライズ形式

- JSON：人間が読みやすいが、サイズ、解析コスト、数値精度、コメント非対応などに注意。
- YAML：表現力が高いが、仕様とパーサーの安全性・複雑性に注意。
- CSV：単純な表に適するが、引用符、改行、文字コードの正しい解析が必要。
- Protocol Buffers等：スキーマと互換性を管理しやすい。
- 独自バイナリ：要件へ最適化できるが、検証・互換性・ツールをすべて設計する責任がある。

信頼できないデータを読む際は、長さ、個数、再帰深度、整数オーバーフロー、圧縮爆弾、パストラバーサルを検証します。

## アセットとセーブを分ける

開発時アセット、ビルド済みアセット、ユーザー設定、セーブ、キャッシュ、ログは寿命と更新方法が異なります。同じディレクトリや同じローダーへ混在させず、読み取り専用・書き込み可能領域を分けます。Unreal EngineではPak/IoStore、Config、SaveGameなど固有の仕組みを専用章で扱います。
