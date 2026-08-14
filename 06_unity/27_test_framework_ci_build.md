# Test Framework・CI・Build

> 対象: Unity 6.0。Unity Test FrameworkはEditor Versionに固定されたCore Package。Performance Testing APIはUnity 6.0向けreleased版3.2系。

## 1. Testの目的

Testはbugがない証明ではなく、重要な契約を繰り返し確認する仕組みです。

```text
Code change
 → Compile
 → Static/Data Validation
 → EditMode Tests
 → PlayMode Tests
 → Player Build
 → Smoke/Device Tests
 → Artifact保存
```

## 2. Test Pyramid

- 多数の高速pure C# unit test。
- 中程度のUnity integration/EditMode test。
- 少数のPlayMode/Scene test。
- さらに少数の実機end-to-end test。

全てを重いScene testにすると遅く不安定になります。

## 3. Testable design

MonoBehaviourからdomain logicをpure C#へ分けます。

```csharp
public sealed class DamageCalculator
{
    public int Calculate(int attack, int defense)
    {
        // 最低1damageというgame rule。
        return System.Math.Max(1, attack - defense);
    }
}
```

Unity APIなしなら高速でdeterministicにtestできます。

## 4. NUnit test

```csharp
using NUnit.Framework;

public sealed class DamageCalculatorTests
{
    [Test]
    public void Calculate_DefenseBelowAttack_SubtractsDefense()
    {
        var calculator = new DamageCalculator();

        int result = calculator.Calculate(attack: 100, defense: 30);

        Assert.That(result, Is.EqualTo(70));
    }

    [Test]
    public void Calculate_DefenseExceedsAttack_ReturnsMinimumDamage()
    {
        var calculator = new DamageCalculator();

        int result = calculator.Calculate(10, 999);

        Assert.That(result, Is.EqualTo(1));
    }
}
```

test名に条件、操作、期待結果を含めます。

## 5. Arrange・Act・Assert

```text
Arrange: input/dependencyを準備
Act: 一つのbehaviorを実行
Assert: observable resultを確認
```

一testへ多数の無関係なassertを詰めません。

## 6. TestCase

```csharp
[TestCase(100, 30, 70)]
[TestCase(10, 10, 1)]
[TestCase(0, 0, 1)]
public void Calculate_ReturnsExpected(
    int attack,
    int defense,
    int expected)
{
    Assert.That(
        new DamageCalculator().Calculate(attack, defense),
        Is.EqualTo(expected));
}
```

境界値をtable化できます。失敗時にどのcaseか分かる値を使います。

## 7. Float comparison

```csharp
Assert.That(actual, Is.EqualTo(expected).Within(0.0001f));
```

浮動小数点を無条件にexact equalityで比べません。許容誤差の根拠をdomainから決めます。

## 8. EditMode Test

Editor内で実行されるtestです。

- pure C# logic。
- ScriptableObject/data validation。
- serialization/migration。
- Editor tool。
- AssetDatabaseを使うintegration。

Editor状態へ依存するとparallel/repeatで不安定になるためcleanupします。

## 9. PlayMode Test

Play lifecycle、Scene、GameObject、frame経過等をtestします。

- Awake/Start/Update。
- Coroutine。
- Physics。
- Scene transition。
- pooling lifecycle。
- UI/input integration。

EditModeより遅いので、Unity runtime behaviorが必要な契約だけに使います。

## 10. UnityTest

```csharp
using System.Collections;
using NUnit.Framework;
using UnityEngine;
using UnityEngine.TestTools;

public sealed class LifetimeTests
{
    [UnityTest]
    public IEnumerator Object_Destroyed_IsNullAfterFrame()
    {
        GameObject instance = new("Temporary");
        Object.Destroy(instance);

        yield return null; // Destroyが処理されるframe境界を待つ。

        Assert.That(instance == null, Is.True);
    }
}
```

待機frame数を適当に増やさず、どのlifecycle boundaryを待つかコメントします。

## 11. SetUp・TearDown

```csharp
[SetUp]
public void SetUp() { }

[TearDown]
public void TearDown()
{
    // 作成したObject、World、NativeContainer、temporary Assetを解放。
}
```

assert/exception失敗時にもTearDownがcleanupできる設計にします。

## 12. OneTimeSetUp

fixture全体で高価なresourceを共有できますが、test間state汚染を招きます。各testが独立する方を優先し、本当にimmutableなfixtureだけ共有します。

## 13. LogAssert

期待するwarning/errorを明示します。

```csharp
LogAssert.Expect(
    LogType.Error,
    "Invalid attack definition.");
```

予期しないerror logを放置しません。文字列完全一致が脆い場合、正規表現やstable error IDを検討します。

## 14. Dependency injection

```csharp
public interface IClock
{
    double Now { get; }
}

public sealed class FakeClock : IClock
{
    public double Now { get; set; }
}
```

Time、random、file、networkをinterface/valueとして注入すると、sleepや実通信なしでtestできます。

## 15. Random test

- seedを明示。
- failure時seedを出力。
- property-based testは再現可能に。
- production random sequenceへ影響させない。

「たまに落ちる」testを作りません。

## 16. Time test

実時間を待つよりsimulation methodへdelta/tickを渡します。

```csharp
state.Tick(0.1f);
state.Tick(0.1f);
Assert.That(state.Remaining, Is.EqualTo(0.8f).Within(0.0001f));
```

PlayModeでTimeScale/frame timingへ依存する範囲を最小にします。

## 17. Scene test

- Scene名stringではなくbuild data/constantを管理。
- async load完了を待つ。
- persistent Sceneをcleanup。
- test順へ依存しない。
- active Sceneを明示。
- Addressables handleをrelease。

Scene全体を毎test loadせずfixture設計を見直します。

## 18. Physics test

fixed step、collision layer、transform sync、solver差を考慮します。

手動Physics Sceneを作り明示simulationできる場合は、frame待ちよりdeterministicにします。対象Unity APIを確認します。

## 19. Input test

New Input Systemのtest supportでvirtual device/eventを使い、実controllerを必須にしません。

- action map enable/disable。
- press/release順。
- hold/tap timeout。
- device scheme。
- rebinding。
- test後device削除。

## 20. Save migration test

公開済みschemaごとのfixtureをrepositoryへ保存します。

```text
v1.json → currentへmigration → expected model
v2.json → currentへmigration → expected model
future.json → 安全に拒否
corrupt.json → backup/recovery
```

古いfixtureを「不要」と消すと互換性を検査できません。

## 21. Asset validation test

全AttackDefinition等を検索し:

- stable ID重複。
- missing reference。
- invalid range。
- Addressables設定。
- localization key。

を検査します。遅いproject-wide testはcategoryを分け、CIで定期実行します。

## 22. ECS/Job test

- Test Worldを作成。
- Systemをupdate。
- JobをComplete。
- ECBをplayback。
- World/NativeContainerをDispose。
- Burst on/off結果。
- iteration orderへ依存しない。

Default Worldや他testのEntityを共有しません。

## 23. Performance Test

Unity 6向けPerformance Testing API 3.2系はwarm-up、measurement、metadata収集を支援します。

```text
Warmup
 → Measurementを複数回
 → median/percentile/outlier
 → baselineと比較
```

一回の`Stopwatch`値で回帰判定しません。

## 24. Performance noise

- Editor background import。
- thermal/clock。
- OS process。
- shader warm-up。
- disk cache。
- development checks。
- Profiler接続。

同一device、複数sample、warm-up、許容thresholdを使います。

## 25. Assembly Definition

```text
Game.Runtime
Game.Tests.EditMode
  references Game.Runtime
  optional test assemblies
Game.Tests.PlayMode
  references Game.Runtime
```

test codeをPlayer production assemblyへ含めず、test assembly flag/platformを設定します。

## 26. InternalsVisibleTo

private implementationを無理にpublicにせず、必要ならtest assemblyへinternal公開します。ただしinternal detailばかりtestするとrefactorに脆くなります。observable contractを優先します。

## 27. Test doubles

- Fake: 単純な実装。
- Stub: 固定response。
- Spy: 呼出記録。
- Mock: interaction expectation。

何でもmockせず、value/pure implementationで十分か検討します。

## 28. Flaky test

原因:

- test順依存。
- static state。
- real time/network。
-不定なframe数。
- async未完了。
- random seedなし。
- shared file/path。
- platform差。

retryで隠さずroot causeを直します。隔離する場合もownerと期限を持たせます。

## 29. Test category

```csharp
[Category("Fast")]
[Category("AssetValidation")]
[Category("Device")]
```

PRごと、nightly、release candidateで実行集合を分けます。重要testを遅いcategoryへ追いやり永久に走らない状態を避けます。

## 30. Command line test

概念例:

```text
Unity.exe
 -batchmode
 -nographics
 -projectPath <project>
 -runTests
 -testPlatform EditMode
 -testResults <results.xml>
 -logFile <editor.log>
```

正確な引数、終了code、graphics要否はUnity Version/targetで確認します。

## 31. CI Pipeline

```text
Checkout
 → Unity/Package cache準備
 → Compile/Validation
 → EditMode
 → PlayMode
 → Build content/player
 → Smoke test
 → reports/artifacts upload
```

失敗してもlog、test XML、BuildReportを保存します。

## 32. Reproducible build

- Unity Version固定。
- Package manifest/lock管理。
- Build Profile/target固定。
- scripting backend/architecture固定。
- environment変数を記録。
- content catalog state。
- build number/commit hash埋込み。
-同じsourceから同じ設定。

timestamp/signing等でbinary完全一致しない要因も記録します。

## 33. Cache

Library/Package/IL2CPP cacheはCIを高速化しますが、cache keyが粗いと古い生成物を使います。

key候補:

- Unity Version。
- target platform。
- manifest/lock hash。
- relevant project settings。
- pipeline version。

cacheなしclean buildも定期実行します。

## 34. LicenseとSecrets

- license credentialをrepository/log/artifactへ出さない。
- CI secret storeを使う。
- signing keyを最小権限に。
- PR from forkへrelease secretを渡さない。
- credential rotation。
- build outputの署名段階を分離。

## 35. Build Script

```csharp
using System;
using System.Linq;
using UnityEditor;
using UnityEditor.Build.Reporting;

public static class CiBuild
{
    public static void BuildWindows()
    {
        string output = GetRequiredArgument("-buildOutput");

        string[] scenes = EditorBuildSettings.scenes
            .Where(scene => scene.enabled)
            .Select(scene => scene.path)
            .ToArray();

        var options = new BuildPlayerOptions
        {
            scenes = scenes,
            locationPathName = output,
            target = BuildTarget.StandaloneWindows64,
            options = BuildOptions.None
        };

        BuildReport report = BuildPipeline.BuildPlayer(options);
        BuildSummary summary = report.summary;

        if (summary.result != BuildResult.Succeeded)
        {
            throw new InvalidOperationException(
                $"Build failed: {summary.result}");
        }
    }

    private static string GetRequiredArgument(string name)
    {
        string[] args = Environment.GetCommandLineArgs();
        int index = Array.IndexOf(args, name);

        if (index < 0 || index + 1 >= args.Length)
        {
            throw new ArgumentException($"Missing argument: {name}");
        }

        return args[index + 1];
    }
}
```

CI entry pointはdialogを出さず、失敗時にnon-zero exitへ繋がるexception/reportを返します。

## 36. BuildReport

確認/保存:

- `BuildResult`。
- total time/size。
- errors/warnings。
- output files。
- stripping情報。
- step別duration。

BuildPipeline呼出しが戻っただけで成功扱いしません。

## 37. Build callback

- `IPreprocessBuildWithReport`。
- `IProcessSceneWithReport`。
- `IPostprocessBuildWithReport`。

validation、Scene修正、artifact処理に使えます。callback順、Editor state mutation、失敗時cleanupを明示します。

## 38. Build Profile

Unity 6のBuild Profileでplatform/config設定をAsset化できます。

```text
Windows Development
Windows Release
Android Development
Android Release
```

production endpoint、development symbol、profiler/debug optionsを混ぜません。

## 39. DevelopmentとRelease

- Development Build。
- Script Debugging/Allow Debugging。
- Autoconnect Profiler。
- Deep profiling support。
- assertions/safety checks。
- code stripping。

release artifactへdebug機能を誤って含めないvalidationを入れます。

## 40. Code Stripping

IL2CPP/managed strippingはreflection、serialization、AssetBundleだけで参照される型を除く場合があります。

- `link.xml`。
- Preserve attribute。
- generated registration。
- build-time analysis。
- Player smoke test。

strippingを無効化して隠すだけでなく必要typeを明示します。

## 41. Addressables Build

Player Buildとの順序、content state、remote catalog、target platformを統一します。

- clean/new buildとcontent updateを区別。
- `addressables_content_state.bin`保存。
- catalog/hash/Bundle artifact。
- CDN upload前validation。
- Playerとcontent version対応。

## 42. Artifact

保存候補:

- Player build。
- symbols。
- test result XML。
- Editor/Player log。
- BuildReport。
- Addressables catalog/content state。
- performance result。
- dependency/version manifest。

retention、access permission、size、個人情報を管理します。

## 43. Smoke Test

build成功後に最低限:

- process起動。
- title到達。
- fatal logなし。
- local save path作成。
- basic Scene load。
- controlled shutdown。

compile/build成功だけではruntime起動を保証しません。

## 44. Device Matrix

- supported OS。
- GPU vendor/API。
- low/high memory。
- locale/timezone。
- controller/touch。
- offline/slow network。
- fresh install/update。

全組合せを毎PRで走らせず、risk-basedでPR/nightly/releaseへ分けます。

## 45. Failure report

必要情報:

- test/build name。
- commit/build ID。
- Unity/Package Version。
- platform/device。
- seed/input fixture。
- expected/actual。
- stack trace/log。
- screenshot/capture。
- reproduction command。

secretやuser dataを除去します。

## 46. Review checklist

- pure logicをUnity lifecycleから分けたか。
- EditMode/PlayModeを適切に選んだか。
- testが独立しcleanupするか。
- time/random/networkを制御したか。
- schema fixtureを保存したか。
- flaky testをretryで隠していないか。
- Test assemblyをruntimeから分離したか。
- CIがUnity/Package Versionを固定したか。
- BuildReportのresultを確認したか。
- release secret/debug optionを検証したか。
- Player smoke/device testがあるか。
- failure artifactを保存するか。

## 47. 学習確認問題

1. EditModeとPlayMode Testの役割差は何か。
2. pure C# testを多くする利点は何か。
3. UnityTestでframeを待つ理由を明示すべきなのはなぜか。
4. random testのseedを保存する理由は何か。
5. flaky testをretryだけで処理してはいけない理由は何か。
6. BuildPipelineの戻り後にBuildReportを見る理由は何か。
7. DevelopmentとRelease Profileを分ける理由は何か。
8. code strippingがPlayerだけでbugを起こす理由は何か。
9. content state fileをartifact化する理由は何か。
10. build後のsmoke testが必要な理由は何か。

## 48. 公式資料

- [Unity Manual: Test Framework](https://docs.unity3d.com/6000.0/Documentation/Manual/com.unity.test-framework.html)
- [Unity Test Framework Manual](https://docs.unity3d.com/Packages/com.unity.test-framework@latest/)
- [Performance Testing API](https://docs.unity3d.com/6000.0/Documentation/Manual/com.unity.test-framework.performance.html)
- [Unity command line arguments](https://docs.unity3d.com/6000.0/Documentation/Manual/EditorCommandLineArguments.html)
- [BuildPipeline API](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/BuildPipeline.html)
- [BuildPipeline.BuildPlayer](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/BuildPipeline.BuildPlayer.html)
- [Build Player Pipeline](https://docs.unity3d.com/6000.0/Documentation/Manual/BuildPlayerPipeline.html)
- [BuildReport API](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/Build.Reporting.BuildReport.html)

## 49. まとめ

- pure logic、EditMode、PlayMode、Player/device testを役割で分ける。
- testはtime、random、file、network、static stateを制御して独立させる。
- CIは固定Version、再現可能なcommand、明確な終了code、failure artifactを持つ。
- BuildPipelineの実行後はBuildReportを検査し、Player smoke testまで行う。
- release設定、secret、stripping、Addressables contentを自動validationする。
