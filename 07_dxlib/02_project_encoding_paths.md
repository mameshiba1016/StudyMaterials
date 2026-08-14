# Project設定・文字コード・Path

> 対象: Windows、MSVC、C++20、Windows版DXライブラリ。推奨方針はsource・narrow string・text dataをUTF-8へ統一し、境界でのみUTF-16へ変換することです。

## 1. 文字化けの正体

文字化けは「日本語が難しい」のではなく、同じbyte列を異なるencodingとして解釈した結果です。

```text
同じbytes: E3 81 82
UTF-8として解釈     → 「あ」
Shift_JIS等として解釈 → 別文字またはinvalid
```

stringが「何語か」ではなく、そのbyte列が何encodingかを追跡します。

## 2. 文字・code point・glyph

- Character: 人間が認識する文字という抽象概念。
- Code point: Unicodeで文字等へ割り当てた番号。例 `U+3042`。
- Code unit: encodingが扱う基本単位。UTF-8は8bit、UTF-16は16bit。
- Glyph: fontが画面へ描く形。

正しいcode pointでもfontにglyphがなければ豆腐記号になります。encoding問題とfont問題を区別します。

## 3. Unicodeはencoding名ではない

Unicodeはcharacter set/code point体系です。実際のbyte表現にはUTF-8、UTF-16、UTF-32等があります。

```text
U+0041  LATIN CAPITAL LETTER A
U+3042  HIRAGANA LETTER A
U+1F600 GRINNING FACE
```

「Unicodeで保存」だけではUTF-8/UTF-16、endian、BOMが不明です。

## 4. UTF-8

- 1 code pointを1～4 bytesで表す可変長encoding。
- ASCII 0x00～0x7Fと互換。
- web、data file、source、cross-platformで広く利用。
- byte indexと文字数が一致しない。

```cpp
const std::string text = u8"Aあ"; // C++20ではu8 literal型がchar8_t配列になる点に注意。
```

projectで`std::string` UTF-8を採用する場合、`char8_t`とのadapter方針を明示します。

## 5. UTF-16

- 16bit code unitを使う可変長encoding。
- BMP外code pointはsurrogate pairで2 code units。
- Windowsのwide APIはUTF-16LEを基本に扱う。
- `wchar_t`はWindowsで通常16bitだが、他platformでは幅が異なり得る。

`wstring.length()`も人間の文字数とは限りません。

## 6. Shift_JIS/Windows code page

Shift_JIS系は日本語Windowsで長く使われたmultibyte encodingです。

- 表現できる文字集合に制限。
- user/system localeに依存しやすい。
- byte値0x5C等の見え方で問題が起こり得る。
- 他言語・emoji・cross-platform dataに弱い。

legacy dataとの境界では必要でも、新規projectの内部標準にはUTF-8を推奨します。

## 7. 四つのencoding地点

```text
1. Source file encoding
   ↓ compilerが読む
2. Source character set
   ↓ literalを変換
3. Execution character set
   ↓ executable内のnarrow literal bytes
4. Runtime APIが期待するencoding
```

さらにfile data自身のencodingとfilesystem path APIのencodingがあります。一箇所だけUTF-8にしても全体は揃いません。

## 8. Source file encoding

`.cpp/.h`がdisk上で何bytesとして保存されるかです。Editor右下の表示だけでなく、repository全体をUTF-8へ統一します。

- UTF-8 with/without BOM。
- line ending CRLF/LF。
- `.editorconfig`。
- compiler option。
- formatter/tool対応。

BOMの有無とUTF-8であることは別です。modern MSVCでは`/utf-8`でBOMなしでも明示できます。

## 9. MSVC `/utf-8`

Microsoft公式では`/utf-8`はsource character setとexecution character setをUTF-8に設定し、概ね次と等価です。

```text
/source-charset:utf-8 /execution-charset:utf-8
```

Visual Studio:

```text
Project Properties
→ C/C++
→ Command Line
→ Additional Options
→ /utf-8
```

全Configuration/Platformへ適用されているか確認します。

## 10. Project fileでの設定

`.vcxproj`へUTF-8 compiler optionが表現される場合があります。UI名やproperty表現はVisual Studio versionで変わるため、実際のcompiler command lineをbuild logで確認します。

Property Sheetへ共通設定すると複数projectでずれにくくなります。

## 11. `.editorconfig`

```ini
root = true

[*]
charset = utf-8
end_of_line = crlf
insert_final_newline = true

[*.{cpp,h,hpp}]
indent_style = space
indent_size = 4
```

Editor保存形式を揃えます。ただし`.editorconfig`はcompiler execution charsetを設定しないため、`/utf-8`も必要です。

## 12. DXライブラリの文字列encoding

公式referenceではWindows版のnarrow文字列は初期状態でShift_JIS系として扱われ、`SetUseCharCodeFormat`で変更できます。

```cpp
// 必ず他のDXライブラリ関数より先に呼ぶ。
SetUseCharCodeFormat(DX_CHARCODEFORMAT_UTF8);
```

近年の公式回答でも、UTF-8利用時は`WinMain`冒頭で`ChangeWindowMode`や`DxLib_Init`より前に呼ぶよう案内されています。

## 13. 統一した初期化

```cpp
#include "DxLib.h"

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
    // DXライブラリが受け取るnarrow文字列のencodingをUTF-8にする。
    // この呼び出し自体を含め、指定された初期化前API以外を先に呼ばない。
    if (SetUseCharCodeFormat(DX_CHARCODEFORMAT_UTF8) == -1)
    {
        return -1;
    }

    // 以後、window title、path、描画文字列などnarrow文字列をUTF-8で渡す。
    ChangeWindowMode(TRUE);
    SetWindowText("日本語対応ゲーム");

    if (DxLib_Init() == -1)
    {
        return -2;
    }

    DrawString(32, 32, "日本語をUTF-8で描画", GetColor(255, 255, 255));

    DxLib_End();
    return 0;
}
```

戻り値の詳細は利用versionのreferenceを確認します。

## 14. 全DX文字列APIへ適用される契約

UTF-8 modeにした後は一部だけでなく、文字列を受け取るDXライブラリ関数へ渡すnarrow stringをUTF-8に揃えます。

```cpp
LoadGraph("Data/画像/主人公.png");
LoadSoundMem("Data/音声/決定.wav");
DrawString(0, 0, "開始", color);
SetWindowText("タイトル");
```

あるpathだけShift_JISへ変換すると、設定契約に反して失敗します。

## 15. `char`はencodingを持たない

```cpp
std::string text;
```

`std::string`はbytesのcontainerであり、それ自体にUTF-8というmetadataはありません。変数名、type wrapper、module contractでencodingを示します。

```cpp
struct Utf8Text
{
    std::string bytes;
};
```

強い型を使うとShift_JIS/UTF-8混在をcompile時に減らせます。

## 16. `wchar_t`も万能ではない

Windowsでは`std::wstring`をUTF-16 code unitsとしてwide APIへ渡せますが:

- platformで`wchar_t`幅が違う。
- surrogate pairがある。
- UTF-8 dataとの変換が必要。
- DXライブラリ側のwide/narrow mode設定とAPI suffixを確認する必要。

内部標準をUTF-8にし、Windows境界だけUTF-16へ変換する構成が明瞭です。

## 17. `char8_t`

C++20の`u8"..."`は`const char8_t[]`です。`const char*`を受けるlegacy APIへ暗黙変換できません。

```cpp
constexpr std::u8string_view title = u8"日本語";

// char8_tとcharのobject representationは1byteだが、境界変換を一箇所に閉じ込める。
std::string toUtf8Bytes(std::u8string_view value)
{
    return std::string(
        reinterpret_cast<const char*>(value.data()),
        value.size());
}
```

無秩序に`reinterpret_cast`を散らさず、UTF-8 adapterでのみ使います。

## 18. Literal方針

選択肢:

1. `/utf-8`を前提に通常`"日本語"`をUTF-8 bytesとして使う。
2. `u8` literal＋`char8_t`を内部標準にし、API境界で変換。
3. Windows wide API中心なら`L"..."`とUTF-16を使う。

教材では1を基本にし、`/utf-8`とDX UTF-8 modeをbuild/initializationで保証します。

## 19. `TCHAR`とUNICODE macro

Windows/legacy codeでは`TCHAR`、`TEXT()`、`_T()`、`UNICODE/_UNICODE`でnarrow/wide APIを切り替える仕組みがあります。

```cpp
const TCHAR* value = TEXT("Title");
```

便利な一方、実際の型/encodingがbuild設定で変わります。新規domain codeではexplicitな`std::string` UTF-8と`std::wstring` UTF-16を使い、Windows adapterだけTCHARへ対応する方が追跡しやすいです。

## 20. `A`/`W` Windows API

多くのWindows APIは:

```text
FunctionA → narrow/code page版
FunctionW → wide/UTF-16版
Function  → UNICODE macroによりA/Wへ展開
```

を持ちます。Windows境界では`W`版へUTF-16を渡すとsystem locale依存を減らせます。

## 21. UTF-8からUTF-16

Windows APIの`MultiByteToWideChar`を二段階で呼び、必要sizeを先に求めます。

```cpp
#include <Windows.h>
#include <stdexcept>
#include <string>
#include <string_view>

std::wstring utf8ToWide(std::string_view utf8)
{
    if (utf8.empty())
    {
        return {};
    }

    // 第1回はoutput=nullで必要なUTF-16 code unit数を取得する。
    const int required = MultiByteToWideChar(
        CP_UTF8,
        MB_ERR_INVALID_CHARS,
        utf8.data(),
        static_cast<int>(utf8.size()),
        nullptr,
        0);

    if (required <= 0)
    {
        throw std::runtime_error("Invalid UTF-8 or conversion failure");
    }

    std::wstring wide(static_cast<std::size_t>(required), L'\0');

    // 第2回で確保済みbufferへ実際のUTF-16 code unitsを書き込む。
    const int converted = MultiByteToWideChar(
        CP_UTF8,
        MB_ERR_INVALID_CHARS,
        utf8.data(),
        static_cast<int>(utf8.size()),
        wide.data(),
        required);

    if (converted != required)
    {
        throw std::runtime_error("UTF-8 to UTF-16 conversion failed");
    }

    return wide;
}
```

`size_t`から`int`への範囲超過もproduction codeでは事前検証します。

## 22. UTF-16からUTF-8

```cpp
std::string wideToUtf8(std::wstring_view wide)
{
    if (wide.empty())
    {
        return {};
    }

    const int required = WideCharToMultiByte(
        CP_UTF8,
        WC_ERR_INVALID_CHARS,
        wide.data(),
        static_cast<int>(wide.size()),
        nullptr,
        0,
        nullptr,
        nullptr);

    if (required <= 0)
    {
        throw std::runtime_error("Invalid UTF-16 or conversion failure");
    }

    std::string utf8(static_cast<std::size_t>(required), '\0');

    const int converted = WideCharToMultiByte(
        CP_UTF8,
        WC_ERR_INVALID_CHARS,
        wide.data(),
        static_cast<int>(wide.size()),
        utf8.data(),
        required,
        nullptr,
        nullptr);

    if (converted != required)
    {
        throw std::runtime_error("UTF-16 to UTF-8 conversion failed");
    }

    return utf8;
}
```

conversion失敗を空文字に潰すと「元々空」と区別できません。error型や`expected`も検討します。

## 23. Null terminator

上の例はlengthを明示して変換し、返却`std::string/wstring`のlogical sizeにterminatorを含めません。C APIへ`.c_str()`を渡すとcontainerが末尾nullを提供します。

embedded nullを許すdataとC文字列APIは相性が悪く、途中で切れます。path/textでembedded nullを拒否します。

## 24. 文字数とbyte数

```cpp
const std::string utf8 = "あ";

// UTF-8では通常3 bytes。人間が見る1文字とは一致しない。
const std::size_t bytes = utf8.size();
```

cursor移動、文字削除、最大文字数制限をbyte indexで行うとcode point途中を壊します。さらに結合文字やemoji sequenceではcode point数とgrapheme数も一致しません。

## 25. UTF-8を途中で切らない

network/save/UIのbyte上限で`substr(0, N)`するとmulti-byte sequence途中で切る可能性があります。

- Unicode対応libraryでcode point/grapheme boundaryを使う。
- validationしてinvalid sequenceを拒否/置換。
- protocolはbyte limit、UIはgrapheme limitと分ける。

自作の不完全decoderをproject各所へ複製しません。

## 26. Normalization

見た目が同じでもcode point列が違う文字があります。

```text
é = U+00E9
e + ◌́ = U+0065 U+0301
```

file name、player name、検索、ID比較でbyte一致だけにすると違いが出ます。正規化が必要な境界を決め、表示文字列を内部IDに使わないことが重要です。

## 27. Case conversion

ASCIIだけの`tolower`をUTF-8 bytesへ適用してもUnicode case foldingにはなりません。locale依存もあります。

asset IDはASCII小文字等へ制約し、user textのUnicode検索は対応library/APIを使います。

## 28. Pathは文字列に似た構造化data

pathには:

- root/drive。
- directory component。
- filename。
- extension。
- separator。
- relative/absolute。
- `.`/`..`。

があります。文字列連結だけで組み立てず`std::filesystem::path`を使います。

## 29. `std::filesystem::path`

```cpp
#include <filesystem>

namespace fs = std::filesystem;

const fs::path root = fs::path("Data");
const fs::path texture = root / "Texture" / "Player.png";
```

`operator/`がplatformのpath構造として結合します。`"Data/" + name`のseparator漏れを減らします。

## 30. Windowsのnative path

Windows上の`std::filesystem::path`はnative representationとしてwide文字列を扱う実装が一般的です。

```cpp
const std::wstring nativeWide = texture.wstring();
```

UTF-8からpathを作るAPIと`.u8string()`の型はC++ version/libraryで差があるため、adapterで吸収します。

## 31. UTF-8 path adapter

```cpp
fs::path pathFromUtf8(std::string_view utf8)
{
#ifdef _WIN32
    // Windowsでは明示変換したUTF-16からnative pathを構築する。
    return fs::path(utf8ToWide(utf8));
#else
    // Unix系ではpath bytesとしてUTF-8を採用するproject contract。
    return fs::path(std::string(utf8));
#endif
}
```

platform分岐をresource system全体へ散らさず、この境界へ閉じ込めます。

## 32. DXライブラリへpathを渡す

DX UTF-8 modeでは、DXライブラリへ渡すnarrow pathもUTF-8 bytesにします。

```cpp
const std::string playerPath = "Data/画像/Player.png";

const int handle = LoadGraph(playerPath.c_str());
if (handle == -1)
{
    // UTF-8 contract、working directory、file存在、logを確認する。
}
```

filesystemで存在確認するときとDX APIへ渡すときのencoding adapterを分けます。

## 33. Relative path

```text
Data/Texture/Player.png
```

はsource fileや`.vcxproj`位置ではなく、processのcurrent working directoryを基準に解決されます。

Visual StudioのDebugging > Working Directory、Explorer起動、shortcut、test runnerで異なる可能性があります。

## 34. Current working directory確認

```cpp
const fs::path current = fs::current_path();
```

debug logへ起動時に記録します。

```text
Executable Path: ...
Current Directory: ...
Asset Root: ...
```

「fileがあるのにLoadGraphが-1」は別directoryを見ていることが多いです。

## 35. Executable基準asset root

current directoryへ依存せず、executable locationからasset rootを決める方法があります。Windowsでは`GetModuleFileNameW`等を使いUTF-16 pathを取得します。

```cpp
fs::path executablePath(); // Windows adapterに実装を隠す。

const fs::path assetRoot = executablePath().parent_path() / "Data";
```

build outputへData folderをcopyするpipelineも必要です。

## 36. `GetModuleFileNameW`例

```cpp
fs::path executablePath()
{
    std::wstring buffer(1024, L'\0');

    for (;;)
    {
        const DWORD length = GetModuleFileNameW(
            nullptr,
            buffer.data(),
            static_cast<DWORD>(buffer.size()));

        if (length == 0)
        {
            throw std::runtime_error("GetModuleFileNameW failed");
        }

        // bufferに余裕があればpath全体を取得できたとみなす。
        if (length < buffer.size() - 1)
        {
            buffer.resize(length);
            return fs::path(buffer);
        }

        // pathが長い可能性があるので固定MAX_PATHへ依存せず拡張する。
        buffer.resize(buffer.size() * 2);
    }
}
```

APIのtruncation contractとlong path対応はtarget OS/manifestを含め公式資料で確認します。

## 37. Asset root class

```cpp
class AssetPaths final
{
public:
    explicit AssetPaths(fs::path root)
        : root_(std::move(root))
    {
    }

    [[nodiscard]] fs::path texture(std::string_view relativeUtf8) const
    {
        return root_ / "Texture" / pathFromUtf8(relativeUtf8);
    }

private:
    fs::path root_;
};
```

全classが`"../../Data"`を書くのを防ぎます。後でlogical asset IDへ進化できます。

## 38. Path traversal

user/network/save dataからrelative pathを受ける場合、`../`でasset root外へ出られないようにします。

```text
requested: ../../secret.txt
```

canonical/weakly_canonical後にroot配下か確認します。単純な文字列prefix比較はseparator、case、symbolic link等で破られる可能性があります。

## 39. File existence

```cpp
std::error_code error;
const bool exists = fs::exists(path, error);

if (error)
{
    // 「存在しない」と「確認自体に失敗」を区別する。
}
```

例外版/`error_code`版をerror policyで選びます。存在確認直後にfileが消えるTOCTOUがあるため、最終的にはopen/load結果も必ず確認します。

## 40. Canonical path

- `absolute`: relativeをabsoluteへするが`..`等を完全解決するとは限らない。
- `canonical`: 実在pathを正規化しsymbolic link等も解決。
- `weakly_canonical`: 一部未存在でも扱いやすい。
- `lexically_normal`: filesystemへ問い合わせず語彙的に整理。

用途とerror条件が違います。

## 41. Separator

Windowsは通常backslashもslashも多くのAPIで扱えますが、手動置換へ依存しません。

```cpp
fs::path path = fs::path("Data") / "Texture" / "A.png";
path.make_preferred();
```

表示用pathとAPI用native pathを分けます。

## 42. Extension

```cpp
if (path.extension() == ".png")
{
    // extension比較はcase sensitivity policyも決める。
}
```

`file.tar.gz`のstem/extension、先頭dot file、末尾dot等をtestします。extensionだけでfile内容の安全性を信用しません。

## 43. Asset IDとPathを分ける

```cpp
struct TextureId
{
    std::string value; // 例: "player/default"
};
```

Resource manifestがID→pathを解決します。

利点:

- folder変更がgameplay codeへ漏れない。
- localization/platform差を切り替えられる。
- typoをvalidationできる。
- archive/remote assetへ移行しやすい。

## 44. Config file encoding

JSON、CSV、script等もUTF-8へ統一し、readerへ明示します。

- BOMを許可/除去するか。
- invalid UTF-8を拒否/置換するか。
- newline CRLF/LF。
- normalization。
- parserのencoding contract。

sourceがUTF-8でもdata fileがShift_JISなら別変換が必要です。

## 45. UTF-8 BOM

UTF-8 BOM bytesは`EF BB BF`です。

- source editor/compilerがencoding検出に利用する場合。
- JSON/parserがBOMを許容する場合。
- BOMをtext先頭のdataとして誤解するparser。

があります。project policyを決め、parser testを行います。「BOM付きならUTF-8になる」のではなくUTF-8 fileへsignatureが付くという関係です。

## 46. Text file read

```cpp
std::ifstream input(path, std::ios::binary);
if (!input)
{
    throw std::runtime_error("Failed to open text file");
}

std::string bytes(
    std::istreambuf_iterator<char>(input),
    std::istreambuf_iterator<char>());

// 必要ならUTF-8 BOMを除去し、UTF-8 validation後にparserへ渡す。
```

text modeのnewline変換を意識し、encoding validationとparse errorを分けます。

## 47. File pathをlogへ出す

Windows native wide pathをnarrow consoleへ雑にcastしません。

```cpp
const std::string pathForLog = wideToUtf8(path.wstring());
logger.error("LoadGraph failed: " + pathForLog);
```

Console自体のencoding設定とlog file encodingも別問題です。log fileをUTF-8へ統一するとtoolで扱いやすくなります。

## 48. Console文字化け

DX windowの`DrawString`が正しくてもVisual Studio Output/consoleで文字化けすることがあります。

```text
Game text rendering path
≠ Debugger Output path
≠ Windows console code page
≠ Log file encoding
```

各出力先のAPI contractを個別に確認します。

## 49. Font glyph不足

encodingが正しくてもfontが文字を持たなければ表示できません。

- 日本語glyph。
- emoji/color font。
- fallback。
- font license。
- platform差。
- font texture memory。

「豆腐」はbyte dumpが正しければfont側を疑います。

## 50. Byte dump

文字化け診断では実際のbytesをhexで出します。

```cpp
void dumpBytes(std::string_view text)
{
    for (const unsigned char byte : text)
    {
        std::printf("%02X ", static_cast<unsigned int>(byte));
    }
    std::printf("\n");
}
```

source literal、file read後、API直前で比較すると、どこで変換/破損したか分かります。

## 51. Errorの分類

```text
Source decode error
Execution literal conversion error
Invalid UTF-8 runtime data
Wrong DX library char-code mode
Path resolution error
File not found/permission
Font glyph missing
Output console encoding mismatch
```

すべてを「文字化け」と呼ばず、段階を特定します。

## 52. Project directory方針

```text
Project/
├─ Source/
├─ Data/
│  ├─ Texture/
│  ├─ Sound/
│  ├─ Model/
│  ├─ Shader/
│  └─ Config/
├─ Tests/
├─ Tools/
└─ Build/
```

source treeとbuild outputを分け、build時に必要Dataだけをcopy/packageします。

## 53. Space・日本語path

開発環境のroot pathにspace/日本語があっても動くのが理想ですが、外部tool/build scriptが対応しない場合があります。

- pathを必ずquote。
- command string連結よりargument array/API。
- narrow ANSI APIを避ける。
- CIのASCII pathと日本語/space path双方をtest。

「日本語folderを使うな」だけで根本問題を隠さないようにします。

## 54. Case sensitivity

Windowsではcase-insensitiveに見える構成でも、archive、tool、他platform、Gitで違いが出ます。

```text
Data/Texture/Player.png
data/texture/player.png
```

asset path命名ruleを小文字等へ統一し、CIでcase mismatchを検査します。

## 55. Long path

Windowsのpath長制約はAPI、manifest、OS設定、toolで差があります。固定`MAX_PATH` bufferへ依存せずdynamic bufferを使い、deep folderを避け、実配布環境でtestします。

対応を宣言するだけで全dependency toolがlong path対応になるわけではありません。

## 56. Security

- user inputをformat stringにしない。
- path traversalを拒否。
- file size上限。
- invalid UTF sequence処理。
- null byte拒否。
- archive内path検証。
- save fileをasset実行pathとして信用しない。

```cpp
// 危険: userTextに%s等が含まれるとformatとして解釈される。
// DrawFormatString(x, y, color, userText.c_str());

// textそのものを描くAPIへdataとして渡す。
DrawString(x, y, userText.c_str(), color);
```

## 57. Project configuration checklist

- [ ] source fileはUTF-8か。
- [ ] MSVC `/utf-8`が全構成にあるか。
- [ ] `.editorconfig`があるか。
- [ ] `SetUseCharCodeFormat(DX_CHARCODEFORMAT_UTF8)`が最初のDX設定か。
- [ ] DX文字列APIへ全てUTF-8を渡しているか。
- [ ] internal UTF-8とWindows UTF-16の変換境界が一箇所か。
- [ ] current directoryとexecutable pathをlogしたか。
- [ ] asset rootを一元管理したか。
- [ ] pathを`std::filesystem::path`で結合したか。
- [ ] 日本語・space・long pathをtestしたか。
- [ ] file data encodingもUTF-8か。
- [ ] font glyph問題をencodingと区別したか。

## 58. よくある失敗

### SourceだけUTF-8

compiler execution charsetやDX API modeが違う。全経路を揃える。

### DrawStringだけ変換

LoadGraph等のpath APIが別encodingになる。DX文字列API全体の契約を統一する。

### `std::string`ならUTF-8と思う

metadataはない。producer/consumer contractを明示する。

### `wstring`なら一文字一要素と思う

surrogate pairがある。code unitとgraphemeを区別する。

### Source folder基準でrelative pathを書く

runtimeはcurrent working directory基準。asset rootを作る。

### file not foundをencodingだけのせいにする

working directory、case、permission、copy漏れも確認する。

## 59. 確認問題

1. UnicodeとUTF-8は何が違うか。
2. code point、code unit、glyphを説明してください。
3. MSVCの`/utf-8`は何を設定するか。
4. `SetUseCharCodeFormat`を他のDX関数より前に呼ぶ理由は何か。
5. DX UTF-8 mode後にLoadGraphへShift_JIS pathを渡してはいけない理由は何か。
6. `std::string::size()`が文字数でない理由は何か。
7. Windows API境界でUTF-16へ変換する利点は何か。
8. relative pathは何を基準に解決されるか。
9. asset IDとfilesystem pathを分ける利点は何か。
10. encoding問題とfont glyph問題をどう切り分けるか。

## 60. 次章への接続

次章ではGame Loopに時間を追加します。

```text
High-resolution clock
→ delta seconds
→ clamp
→ variable update / fixed simulation
→ accumulator / interpolation
→ sleep/VSync/frame pacing
```

文字列とpathを安定させた上で、frame rateに依存しないmovementと再現可能なsimulationへ進みます。

## 61. 公式資料

- [DXライブラリ公式・文字関係関数](https://dxlib.xsrv.jp/function/dxfunc_other.html)
- [DXライブラリ公式・仕様と特徴](https://dxlib.xsrv.jp/dxinfo.html)
- [Microsoft `/utf-8` compiler option](https://learn.microsoft.com/en-us/cpp/build/reference/utf-8-set-source-and-executable-character-sets-to-utf-8)
- [Microsoft MultiByteToWideChar](https://learn.microsoft.com/en-us/windows/win32/api/stringapiset/nf-stringapiset-multibytetowidechar)
- [Microsoft WideCharToMultiByte](https://learn.microsoft.com/en-us/windows/win32/api/stringapiset/nf-stringapiset-widechartomultibyte)
- [cppreference `std::filesystem::path`](https://en.cppreference.com/w/cpp/filesystem/path)

encoding、compiler、DXライブラリversionの組合せをbuild logと公式referenceで確認してください。
