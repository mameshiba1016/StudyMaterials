# C++実習05：演算子

## 目的

Stamina、攻撃条件、Combo、HP比率を題材に、演算子が型と値へどのように作用するかを実行して確認します。

## 扱う演算子

- 算術：`+ - * / %`
- 比較：`== != < <= > >=`
- 論理：`! && ||`
- 代入：`= += -= *= /=`
- Increment：`++`（前置・後置の違いをREADME課題で確認）
- 型変換：`static_cast<double>`
- 括弧：優先順位に依存せず意図を明示

## Visual StudioでBuild・Test・実行

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\build.ps1 -Configuration Debug -Run
```

生成後は`build/cpp_operators.sln`をVisual Studioで開けます。

## 期待結果

```text
攻撃可能: true
攻撃後Stamina: 45
次Combo Index: 0
HP比率: 0.35
整数除算 35 / 100: 0
```

CTestは一件のTest Executableを実行し、内部12条件を検証します。

## 重要な内部処理

- `&&`と`||`はShort-circuit評価を行います。
- `%`でComboを循環できますが、右辺0は未定義動作です。
- `35 / 100`は整数除算なので`0`です。
- `double`へ変換してから割ると`0.35`になります。
- Signed Integer Overflowは未定義動作なので、実用Codeでは演算前の範囲検査が必要です。
- 浮動小数は完全一致ではなく、用途に応じ許容誤差で比較します。

## よくある失敗

- `=`と`==`を混同する。
- `a && b || c`を括弧なしで書き意図を誤読する。
- 0除算または0剰余を行う。
- 整数除算後に`double`へCastし、失われた小数を戻せると思う。
- `index++`と`++index`の式中の値を混同する。

## 変更課題

1. Stamina自然回復を`+=`で追加し上限へClampする。
2. Comboを逆方向に循環させ、負値と剰余の扱いを調べる。
3. `&&`右辺に呼出し回数を数える関数を置きShort-circuitをTestする。
4. 大きいDamageの乗算Overflowを演算前に検出する。
