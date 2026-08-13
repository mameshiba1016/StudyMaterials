# 46 Cook、Package、Shipping確認

## 1. Build、Cook、Stage、Package

- Build：C++とModuleをPlatform向けBinaryへ。
- Cook：AssetをTarget Platform用形式へ変換。
- Stage：必要Fileを配置。
- Package：配布可能なExecutable／Containerへまとめる。
- Deploy／Run：Deviceへ配置して実行。

Editorで動くことはPackaging成功を保証しません。

## 2. Configuration

- Debug／DebugGame：診断向け。
- Development：開発、Profiling、テスト。
- Test：Shippingに近く一部診断機能あり。
- Shipping：最適化され、開発Console等を制限した配布向け。

Shippingだけで初めて発生する条件コンパイル、Asset不足、Timing問題を確認します。

## 3. Cook漏れ

文字列Pathだけで参照したAssetはCook対象にならない場合があります。Primary Asset、Soft Reference、Primary Asset Label、Packaging設定で必要Assetを宣言します。

## 4. Editor依存を分離

Runtime Moduleが`UnrealEd`等Editor-only Moduleを依存しないようにします。Editor Toolは別Moduleへ置き、`WITH_EDITOR`の範囲とCooked buildで必要なDataを区別します。

## 5. Packaging Log

最後のErrorだけでなく最初のErrorを探します。WarningもMissing Asset、Redirector、Shader、Localization、Plugin対応の兆候になります。CIではWarning分類と失敗条件を管理します。

## 6. 配布前Checklist

- Clean machineで起動。
- Save／Loadと書込み権限。
- 全MapとGameMode。
- Gamepad接続・切断。
- Resolution、Fullscreen、Language。
- Shader初回Hitch。
- 長時間Memory増加。
- Network接続／切断。
- Crash ReportとLog方針。
- Third-party licenseとPlugin Platform対応。

## 7. Package Sizeと起動時間

不要AssetをCookしない、Editor Contentを除外、CompressionとLoad CPU CostのTrade-offを測ります。Size MapとAsset Auditで巨大な参照Chainを特定します。

## 8. Automation

UAT／BuildGraph等でBuild、Cook、Package、Automation Testを再現可能にします。手元Editorの一度きりの設定に依存させません。

## 9. 最終性能確認

Shipping相当BuildをTarget hardwareでProfileします。Editor Viewport、Development PCだけの数値を出荷性能とみなしません。代表戦闘、Boss、多数VFX、Character交代、長時間Playを測ります。

## 参考

- [Packaging Unreal Engine Projects](https://dev.epicgames.com/documentation/unreal-engine/packaging-your-project)
- [Building Multi-Platform Projects](https://dev.epicgames.com/documentation/en-us/unreal-engine/building-multi-platform-games-in-unreal-engine)
