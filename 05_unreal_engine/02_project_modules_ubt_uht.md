# 02 プロジェクト、モジュール、UBT、UHT

## 1. 代表的なプロジェクト構造

```text
MyGame/
├─ MyGame.uproject              プロジェクト記述
├─ Config/                      設定ファイル
├─ Content/                     uasset、umapなどのコンテンツ
├─ Source/
│  ├─ MyGame.Target.cs          ゲーム用ターゲット
│  ├─ MyGameEditor.Target.cs    エディタ用ターゲット
│  └─ MyGame/
│     ├─ MyGame.Build.cs        モジュールのビルドルール
│     ├─ Public/                他モジュールへ公開するヘッダ
│     └─ Private/               実装と非公開ヘッダ
├─ Plugins/                     プラグイン
├─ Binaries/                    ビルド済みバイナリ（生成物）
├─ Intermediate/                中間生成物
├─ Saved/                       ログ、自動保存など
└─ DerivedDataCache/            派生データのキャッシュ
```

`Binaries`や`Intermediate`はソースの本体ではなく再生成可能な領域です。問題解決のため削除する場合もありますが、何を消すか、再生成に何が必要かを理解してから行います。

## 2. TargetとModuleの違い

- **Target**：「ゲーム」「エディタ」「サーバー」など、最終的に何をビルドするか。
- **Module**：機能を分割したコンパイル・リンク上の単位。Runtime、Editor専用、Plugin内などに分けられます。

1つのTargetが複数Moduleを含みます。戦闘システムが成長したら、ゲーム本体、戦闘Runtime、開発用Editor拡張のように責任と依存方向を分けられます。

## 3. Build.csを読む

```csharp
using UnrealBuildTool;

public class MyGame : ModuleRules
{
    public MyGame(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",        // 基本型や低水準機能。
            "CoreUObject", // UObjectとリフレクション基盤。
            "Engine",      // Actor、World、Component等。
            "InputCore"    // 入力関連の基本機能。
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "EnhancedInput" // このモジュール内部の実装だけで使う依存。
        });
    }
}
```

これはC++ではなく、UBTが読むC#のルールです。

- **Public依存**：自モジュールの公開ヘッダが、その依存モジュールの型を必要とする場合。
- **Private依存**：`.cpp`や非公開ヘッダの内部だけで必要な場合。

何でもPublicへ入れると、利用側へ不要な依存が伝播し、ビルド時間と結合度が増えます。公開ヘッダでは前方宣言を使い、実体が必要な`.cpp`でincludeする設計が有効です。

## 4. UBTの役割

UBTはおおむね次を判断します。

1. 選択TargetとConfiguration（Development、Shipping等）。
2. 必要なModuleと依存順序。
3. includeパス、プリプロセッサ定義、コンパイルオプション。
4. 変更されたファイルと再ビルド範囲。
5. C++コンパイラとリンカへ渡す命令。

IDEのソリューションは編集や操作の入口ですが、UEビルドの真の規則はTarget.cs、Build.cs、uproject、plugin記述などにあります。IDEへファイルを追加しただけでModule設定が正しくなるとは限りません。

## 5. UHTの役割

UHTは`UCLASS`、`USTRUCT`、`UENUM`、`UFUNCTION`、`UPROPERTY`などを含むヘッダを解析し、UEの型情報と補助コードを生成します。その結果を通常のC++コンパイラもコンパイルします。

```cpp
#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "AttackDefinition.generated.h" // 原則、最後のinclude。

USTRUCT(BlueprintType)
struct FAttackDefinition
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    float Damage = 10.0f;
};
```

`.generated.h`の位置、マクロ周辺の構文、対応していない型などを間違えると、通常のC++コンパイルより前にUHTで止まることがあります。生成ファイルを直接編集して直してはいけません。元のヘッダまたはビルド設定を直します。

## 6. コンパイルエラーを段階別に切り分ける

| 症状 | 主に疑う場所 |
|---|---|
| UHT、generated、reflection関連 | マクロ、`.generated.h`、対応型、ヘッダ構文 |
| include fileが見つからない | include記述、Build.cs依存、Public/Private境界 |
| unresolved external symbol | 宣言だけで定義がない、Module依存、API公開マクロ |
| Editorでは動くがShippingで失敗 | Editor専用依存、条件コンパイル、アセット参照 |
| 起動時にModuleをロードできない | バイナリ不整合、Module記述、ビルド対象 |

## 7. MYGAME_APIの意味

`MYGAME_API`のようなマクロは、WindowsのDLL export/import等へ展開され、Module境界を越えて使うシンボルを公開します。同じModule内部だけの型には不要な場合がありますが、別Moduleから継承・生成・呼び出しする公開型では重要です。

これはアクセス指定の`public:`とは別物です。`public:`はC++コード上でのアクセス権、`MYGAME_API`は主にバイナリ境界でシンボルを見せるための指定です。

## 参考：Epic Games公式ドキュメント

- [Unreal Engine Modules](https://dev.epicgames.com/documentation/unreal-engine/unreal-engine-modules)
- [Unreal Build Tool](https://dev.epicgames.com/documentation/en-us/unreal-engine/unreal-build-tool-in-unreal-engine)
- [Unreal Header Tool](https://dev.epicgames.com/documentation/en-us/unreal-engine/unreal-header-tool-for-unreal-engine)
