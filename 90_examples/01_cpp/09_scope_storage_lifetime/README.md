# C++実習09：スコープ・記憶域期間・寿命

## 目的

Objectが「どこから見えるか」と「いつ存在するか」を分けて理解し、生成・破棄順を実行結果で確認します。

## 用語

- Scope：名前を参照できる範囲。
- Lifetime：Objectが存在し使用できる期間。
- Automatic Storage：Blockへ入ると生成され、抜けると破棄。
- Static Storage：Program全体にわたりStorageが存在。
- Thread Storage：Threadごとに存在。
- Dynamic Storage：明示的な所有Objectから管理される領域。

## Visual StudioでBuild・Test・実行

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\build.ps1 -Configuration Debug -Run
```

生成後は`build/cpp_scope_storage_lifetime.sln`をVisual Studioで開けます。

## 期待結果

```text
enter:scene
enter:effect
exit:effect
scene-update
exit:scene
Static sequence: 1, 2
local value copied or moved safely
```

Testでは逆順破棄、Static値の持続、安全な値戻しを検証します。

## 危険例（実行しない）

Local変数への参照やPointerを返すと、関数終了時に対象が破棄されDanglingになります。このSampleは`std::string`を値で返します。また、内側Scopeで外側と同名変数を宣言するShadowingは読解ミスを生むため避けます。

## 変更課題

1. 三段Nested Scopeを作り破棄順を予想してから実行する。
2. Constructor/DestructorへInstance番号を追加する。
3. Function-local staticの初期化が一度だけであるTestを追加する。
4. 所有権を持つDynamic Lifetimeを`std::unique_ptr`で追加する。
