# C++実習11：文字列とエンコーディング

## 目的

`std::string`の所有と`std::string_view`の借用、UTF-8のByte数とCode Point数の違い、検索・連結・Trimを確認します。

## Visual StudioでBuild・Test・実行

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\build.ps1 -Configuration Debug -Run
```

生成後は`build/cpp_strings_encoding.sln`をVisual Studioで開けます。

## 期待結果

```text
Text: 相棒
Byte数: 6
Code Point数: 2
相棒 dealt 125 damage.
『棒』を含む: true
```

Testでは9条件を検証します。

## 重要点

- `std::string::size()`はUTF-8文字数ではなくByte数。
- Code Point数と画面上の文字（Grapheme Cluster）数も一致するとは限らない。
- `std::string_view`はDataを所有しない。元文字列より長く保持するとDanglingになる。
- MSVCへ`/utf-8`を全翻訳単位で指定する。
- Windows API境界ではUTF-16との変換が必要になる場合がある。
- この簡易Count関数は妥当なUTF-8入力を前提とし、完全なValidation/文字分割器ではない。

## 変更課題

1. 不正UTF-8を検出してErrorを返す。
2. `std::wstring`との違いを調べる。
3. Player名の最大Byte数と最大表示文字数を別々に検証する。
4. 一時`std::string`から作った`string_view`を保存してはいけない理由を説明する。
