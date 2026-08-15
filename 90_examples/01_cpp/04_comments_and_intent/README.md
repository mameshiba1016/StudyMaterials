# C++実習04：コメントの種類と処理意図

## 目的

CommentをCodeの日本語訳ではなく、Codeだけでは残らない意図、単位、前提、所有権、境界、失敗時の方針を伝えるために使います。

## Commentの種類

- `//`：一行または局所的な理由
- `/* ... */`：複数行にまたがる設計判断
- `///`：公開APIのDocumentation Comment
- `TODO`：作業内容だけでなく理由、条件、追跡先、期限を付ける
- `FIXME`：既知の不具合と影響範囲を記録する

## 良いComment

- 時間の単位が秒である。
- Pause時にUIとGameplayを一致させる。
- 回復倍率0を既定値へ戻す理由。
- Float境界に許容誤差を使う理由。
- 引数を借用し保存しない所有権。

## 避けるComment

```cpp
// remainingを0以上にする
remaining = std::max(remaining, 0.0);
```

このCommentはCodeを読めば分かります。「UIへ負時間を渡さずReady境界を0へ統一する」のように理由を書きます。

## Visual StudioでBuild・Test・実行

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\build.ps1 -Configuration Debug -Run
```

生成後は`build/cpp_comments_and_intent.sln`をVisual Studioで開けます。

## 期待結果

```text
基礎Cooldown: 8 秒
経過時間: 3 秒
回復倍率: 1.5
残り時間: 3.5 秒
使用可能: false
```

CTestは一件のTest Executableを実行し、通常、完了、Pause、不正倍率、負入力、Ready境界を検証します。

## Comment保守の原則

Code変更時にCommentも変更します。Codeと矛盾した古いCommentはCommentなしより危険です。型、関数名、Testで表現できる仕様はそちらへ移し、Commentは「なぜ」に集中させます。

## 変更課題

1. `Cooldown.h`のDocumentationから事前条件と事後条件を探す。
2. 時間単位をMillisecondへ変え、必要な名前とCommentをすべて更新する。
3. 意図を説明していないCommentを一つ改善する。
4. Doxygen等で`///`からAPI文書を生成する方法を調べる。
