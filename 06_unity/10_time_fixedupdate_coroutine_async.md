# Time・Update・FixedUpdate・Coroutine・async/await

> 対象: Unity 6系。時間・非同期APIはUnity Versionによって差があるため、利用中のEditorに対応する公式資料も確認してください。

## 1. ゲームには複数の時間軸がある

「3秒待つ」だけでは仕様として不足です。PauseやSlow Motionの影響を受ける3秒か、現実時間の3秒か、Physics tickで数える3秒かを決めます。

```text
Render frame time
├─ scaled game time      : Time.deltaTime / Time.time
├─ unscaled real time    : Time.unscaledDeltaTime / Time.unscaledTime
├─ fixed simulation time : Time.fixedDeltaTime / Time.fixedTime
└─ independent clock     : server time、audio DSP time、独自simulation tick
```

- Gameplay移動、cooldown、buff: scaled game time。
- Pause Menu、Fade、入力表示: unscaled time。
- Rigidbody操作、固定tick simulation: fixed time。
- Network、Replay、Rollback: frameではなく明示的なtick/sequence。

## 2. deltaTimeの意味

`Time.deltaTime`は前回frameから現在frameまでのゲーム内秒数です。速度に掛けることでframe rateへの依存を減らします。

```csharp
using UnityEngine;

public sealed class ConstantMover : MonoBehaviour
{
    [SerializeField] private float speedMetersPerSecond = 5.0f;

    private void Update()
    {
        // speed [m/s] × deltaTime [s] = 今回の移動量 [m]
        float distanceThisFrame = speedMetersPerSecond * Time.deltaTime;
        transform.position += transform.forward * distanceThisFrame;
    }
}
```

ただし`deltaTime`を掛ければ完全に決定的になるわけではありません。衝突、入力sample、浮動小数点誤差、積分回数が変わるため、異なるframe rateで結果が完全一致するとは限りません。

### 単位で読む

```text
velocity [m/s] × deltaTime [s] = displacement [m]
angularSpeed [degree/s] × deltaTime [s] = angle [degree]
cooldownRemaining [s] - deltaTime [s] = remaining [s]
```

よくある誤り:

```csharp
// 毎frame 5m進むため、FPSが高いほど速い。
transform.position += transform.forward * 5.0f;

// すでに今回の移動量なのに二重にdeltaTimeを掛ける。
transform.position += integratedDisplacement * Time.deltaTime;

// 毎frame固定割合なのでframe rateで収束速度が変わる。
transform.position = Vector3.Lerp(transform.position, target, 0.1f);
```

## 3. Update、FixedUpdate、LateUpdate

### Update

render frameごとに通常1回呼ばれます。入力sample、非PhysicsなGameplay、timer、状態機械更新などに使います。frame間隔は一定ではありません。

### FixedUpdate

固定のゲーム内時間間隔へ追いつくため呼ばれます。render frame中に0回の場合も複数回の場合もあります。Rigidbodyへのforce/velocity操作など、Physics stepと合わせる処理に使います。

### LateUpdate

通常の`Update`後に呼ばれます。Character更新後に追従Cameraを合わせるなど、「他の更新後」という責務に使います。ただし偶然のScript実行順へ過度に依存させません。

```text
入力sample(Update)
  ↓ commandを保持
simulation/physics(FixedUpdate、必要回数)
  ↓ 最新姿勢
camera follow(LateUpdate)
  ↓ render
```

## 4. FixedUpdateはrender frameごとに1回ではない

`Time.fixedDeltaTime`は固定更新のゲーム内時間間隔です。既定値を決めつけずProject Settingsを確認します。処理落ち時は一つのrender frameでPhysics stepが複数回走り、追いつこうとします。

長いstallを無制限に追いかけるとspiral of deathになります。Unityには`Time.maximumDeltaTime`等の制御がありますが、設定とGameplayへの影響を計測します。

```csharp
using UnityEngine;

[RequireComponent(typeof(Rigidbody))]
public sealed class PhysicsMotor : MonoBehaviour
{
    private Rigidbody body;
    private Vector2 sampledMove;

    private void Awake() => body = GetComponent<Rigidbody>();

    // Input callback/Updateから最新の意図を渡す。
    public void SetMoveInput(Vector2 move)
    {
        sampledMove = Vector2.ClampMagnitude(move, 1.0f);
    }

    private void FixedUpdate()
    {
        Vector3 current = body.linearVelocity;
        body.linearVelocity = new Vector3(sampledMove.x * 5.0f, current.y, sampledMove.y * 5.0f);
    }
}
```

API名や推奨操作はUnity Version・Physics設計に合わせます。Transformを直接動かす方式とDynamic Rigidbody方式を無秩序に混ぜません。

## 5. 入力をFixedUpdateだけで読まない

押下edgeは短く発生するため、FixedUpdateだけで読むと取りこぼす可能性があります。

```text
Input callback/Update : Attack押下をtimestamp付きQueueへ積む
Fixed/Combat tick     : Queueから未消費commandを読む
```

連続値のMoveは最新値を保持し、AttackやDodgeはedgeをbufferへ積む、という区別が重要です。

## 6. timeScale

`Time.timeScale`はゲーム内時間の進み方をscaleします。`0.5`なら半速、`0`ならscaled game timeが止まります。しかしCPUやrenderingそのものを停止するswitchではありません。`Update`は呼ばれ得るため、Pause中もunscaled入力やUIを更新できます。

```csharp
using UnityEngine;

public sealed class PauseController : MonoBehaviour
{
    private float initialFixedDeltaTime;

    private void Awake()
    {
        initialFixedDeltaTime = Time.fixedDeltaTime;
    }

    public void SetPaused(bool paused)
    {
        Time.timeScale = paused ? 0.0f : 1.0f;

        // 同時にGameplay/UI Action Map、Audio、Cursor、Cameraの方針も切り替える。
    }

    private void OnDestroy()
    {
        // Sceneを抜けても全体時間を0のまま残さない。
        Time.timeScale = 1.0f;
        Time.fixedDeltaTime = initialFixedDeltaTime;
    }
}
```

公式APIは、Slow Motion中も現実時間あたりのFixedUpdate回数を一定にしたい場合、`fixedDeltaTime`も`timeScale`に応じて変える例を示します。ただしPhysics精度とCPU負荷のtrade-offはゲーム固有なので計測して決めます。

## 7. scaledとunscaled

```csharp
private void Update()
{
    // Slow/Pauseの影響を受けるGameplay時計。
    gameplayCooldown -= Time.deltaTime;

    // Pause中も進むUI時計。
    pauseMenuAnimation += Time.unscaledDeltaTime;
}
```

`timeAsDouble`、`unscaledTimeAsDouble`、`realtimeSinceStartupAsDouble`は、長時間稼働でfloat精度が問題になる時計に向きます。それでも時計の基準を混在させません。

### Timerの二方式

```csharp
// 減算方式
remaining = Mathf.Max(0.0f, remaining - Time.deltaTime);

// deadline方式
double readyAt = Time.timeAsDouble + cooldownSeconds;
bool ready = Time.timeAsDouble >= readyAt;
```

周期timerでは超過分を捨てず`timer -= interval`とするとdriftを抑えられます。ただし一frameで何回catch-upするかに上限を置きます。

## 8. Hit StopとSlow Motion

高速アクションのHit Stopを`timeScale = 0`だけで実装すると、UI、Camera、VFX、入力buffer、Physicsまで一括停止して意図しない結果になります。

設計候補:

1. 世界全体の`timeScale`を短時間変更する。
2. Character animation speedや局所simulation倍率だけを変える。
3. Attacker/VictimとCameraだけを止める。
4. presentation上の停止とGameplay simulationを分離する。

```text
Time Authority
├─ GameplayClock
├─ CharacterLocalScale
├─ AnimationScale
├─ VFXScale
├─ CameraClock
└─ UIClock(unscaled)
```

複数要求が重なる場合、最後に終了した一件が勝手に`timeScale=1`へ戻すと他のSlow Motionを破壊します。token、priority、stackを持つTime Authorityが要求を合成します。

## 9. Coroutineの内部像

CoroutineはC# iteratorをUnity Player Loop上で少しずつ再開する仕組みです。通常はthreadではなく、重い処理をCoroutineへ移しただけではCPU並列化されません。

```csharp
using System.Collections;
using UnityEngine;

public sealed class FadeExample : MonoBehaviour
{
    [SerializeField] private CanvasGroup canvasGroup;

    private IEnumerator FadeOut(float seconds)
    {
        float elapsed = 0.0f;

        while (elapsed < seconds)
        {
            elapsed += Time.unscaledDeltaTime;
            canvasGroup.alpha = Mathf.Clamp01(elapsed / seconds);

            // 次のframeまでiteratorの実行を中断する。
            yield return null;
        }

        canvasGroup.alpha = 1.0f;
    }
}
```

compilerは`IEnumerator`メソッドをstate machineへ変換し、local変数と再開位置を保持します。`yield return`はOS threadをsleepさせる命令ではありません。

## 10. 主なyield instruction

- `yield return null`: 次のframeで再開。
- `WaitForSeconds`: scaled timeで待つ。
- `WaitForSecondsRealtime`: unscaled real timeで待つ。
- `WaitForFixedUpdate`: 次のFixedUpdate timingまで待つ。
- `WaitForEndOfFrame`: frame末尾側まで待つ。利用制約をAPIで確認。
- `AsyncOperation`: Scene/Resource等の非同期処理完了を待つ。
- 別の`IEnumerator`/Coroutine: 子手順の完了を待つ。

`WaitForSeconds(1)`は正確なwall-clock deadlineを保証しません。長いframeの終端から待ち始め、再開も条件成立後のframe境界です。

## 11. Coroutineの停止と寿命

```csharp
private Coroutine activeRoutine;

public void RestartAction()
{
    if (activeRoutine != null)
    {
        StopCoroutine(activeRoutine);
    }

    activeRoutine = StartCoroutine(ActionRoutine());
}

private IEnumerator ActionRoutine()
{
    yield return null;
    activeRoutine = null;
}
```

注意点:

- `StopCoroutine`へ渡すhandle、iterator、method名の形式を混在させない。
- GameObject/MonoBehaviourの無効化・破棄で何が止まるか利用版の仕様を確認する。
- Coroutineが止まってもevent購読、入力lock、`timeScale`が自動復旧するとは限らない。
- 所有者より長寿命の処理はScene Flow等の長寿命ownerへ置く。
- Coroutine内の未処理例外後は後続処理が進まず、cleanupが残る危険がある。

重要なlockは終了・停止・例外・`OnDisable`の各経路から冪等に解除できる設計にします。

## 12. async/await

`async/await`は「待ちを含む処理」を上から読める形で書くC#機構です。I/O、Scene load、Web request、Unity event、frame待ちに向きます。`async`を付けただけではbackground threadになりません。

Unity 6には.NET `Task`に加えて`UnityEngine.Awaitable`があります。

```csharp
using System.Threading;
using UnityEngine;

public sealed class AsyncDelayExample : MonoBehaviour
{
    private async Awaitable FlashAsync(CancellationToken token)
    {
        await Awaitable.WaitForSecondsAsync(0.1f, token);
        Debug.Log("Flash");
    }
}
```

## 13. AwaitableとTaskの重要な違い

Unity公式資料上、`Awaitable`はallocationを抑えるためpoolされます。そのため同じ`Awaitable` instanceを複数回`await`してはいけません。

```csharp
Awaitable operation = Awaitable.NextFrameAsync();
await operation;

// 危険: poolへ返された同一instanceを再awaitする。
// await operation;
```

`Task`で可能な使い方を`Awaitable`にも当てはめず、戻り型ごとの契約を読みます。

## 14. continuationとmain thread

大半のUnity APIはmain threadから呼ぶ必要があります。Unity APIが返す`Awaitable`は、特記がなければmain threadで完了すると公式資料に説明されています。

```csharp
private async Awaitable<float> CalculateAsync()
{
    await Awaitable.BackgroundThreadAsync();

    // UnityEngine.Objectに触れず、純粋な数値計算だけを行う。
    float result = ExpensivePureCalculation();

    // Unity APIを呼ぶ前にmain threadへ戻る。
    await Awaitable.MainThreadAsync();
    return result;
}
```

background threadでGameObject生成、Transform操作、Scene loadを行いません。Development Buildの検査だけに頼ると、Releaseでcrashや予測不能な不具合になり得ます。

## 15. CancellationTokenと破棄後の継続

`await`中にMonoBehaviourやSceneが破棄されても、一般のasync処理が全て自動停止するとは限りません。

```csharp
using System;
using System.Threading;
using UnityEngine;

public sealed class LifetimeBoundSequence : MonoBehaviour
{
    private CancellationTokenSource lifetimeCts;

    private void Awake()
    {
        lifetimeCts = new CancellationTokenSource();
    }

    private void OnDestroy()
    {
        lifetimeCts.Cancel();
        lifetimeCts.Dispose();
    }

    public async Awaitable RunAsync()
    {
        try
        {
            await Awaitable.NextFrameAsync(lifetimeCts.Token);
            lifetimeCts.Token.ThrowIfCancellationRequested();

            // Unity Objectの特殊なnull判定も行う。
            if (this == null)
            {
                return;
            }

            Debug.Log("Still alive");
        }
        catch (OperationCanceledException)
        {
            // 正常な寿命終了。必要なcleanupはfinally等へ置く。
        }
    }
}
```

Unity 6には`destroyCancellationToken`や`Application.exitCancellationToken`もあります。利用VersionのAPIを確認し、独自CTSが必要か判断します。linked CTSを作った場合はDisposeします。

## 16. async voidを境界に限定する

`async void`は呼出側が完了待ち・例外取得をしにくいため、event handler等、戻り値を変えられない境界だけに限定します。

```csharp
private async Awaitable SaveAsync()
{
    // 保存本体。呼出側がawaitできる。
}

public async void OnSaveButtonClicked()
{
    try
    {
        await SaveAsync();
    }
    catch (System.Exception exception)
    {
        Debug.LogException(exception);
    }
}
```

Fire-and-forgetを暗黙に増やすと、Scene遷移後の古い処理、未観測例外、二重保存が発生します。

## 17. Coroutine、Awaitable、Task、Jobの選択

| 目的 | 主な候補 | 注意 |
|---|---|---|
| 数frameに渡る演出手順 | Coroutine / Awaitable | owner寿命とcancel |
| Unity frame eventを待つ | Awaitable / Coroutine | Unity Version |
| File・Network I/O | Task / 対応Async API | main threadへ戻る地点 |
| 重い短時間の並列数値計算 | Job System + Burst | thread-safe data |
| 長い純粋計算をbackgroundへ | Awaitable/Task + thread | Unity API禁止 |
| 固定tick Gameplay | 明示的simulation loop | render timeと分離 |

「非同期」と「並列」は別です。待機中にmain threadを塞がないことと、複数CPU coreで同時計算することを混同しません。

## 18. 戦闘Actionの時間管理

Animationだけを時計にすると、cancel window、hit判定、Slow Motion、replayの整合が崩れやすくなります。

```text
AttackDefinition
├─ startup ticks
├─ active ticks
├─ recovery ticks
├─ cancel windows
├─ hit stop request
└─ animation presentation mapping

Combat Simulationが判定
        ↓
Animation/VFX/Audioが表現
```

「frame」という曖昧語ではなく、render frame、animation normalized time、physics step、combat tickを区別します。

## 19. Character交代と古い処理

```csharp
private async Awaitable SwitchCharacterAsync(
    CharacterView oldCharacter,
    CharacterView nextCharacter,
    CancellationToken token)
{
    // 旧Characterの退場stateを開始。
    await Awaitable.NextFrameAsync(token);
    token.ThrowIfCancellationRequested();

    // Scene遷移等による破棄後に触らない。
    if (oldCharacter == null || nextCharacter == null)
    {
        return;
    }

    // Partyの現在Characterを切り替え、Camera/UIへ一度だけ通知する。
}
```

実際には`Idle → Exiting → Swapping → Entering → Idle`の状態機械、request ID、cancel policyを定義します。

## 20. よくある不具合

- Pause中に`WaitForSeconds`を使い、Pause Menu演出が進まない。
- `Update`と`FixedUpdate`の両方で同じtimerを減算する。
- FixedUpdateがrender frameごとに必ず1回だと思う。
- Coroutineへ重いloopを移しただけで軽くなったと思う。
- `StopCoroutine`でevent購読や入力lockも解除されたと思う。
- `async void`の例外と完了を追跡できない。
- background threadからUnity APIを呼ぶ。
- Scene unload後に古いcontinuationがCamera/UIへ書き込む。
- 同一`Awaitable`を複数回awaitする。
- 複数Systemが`timeScale`を直接上書きする。
- `deltaTime`を掛けたから決定的だと思う。

## 21. Test項目

- 30/60/120 FPSとframe pacingの乱れ。
- 1秒以上の意図的stall後のcatch-up。
- `timeScale`が1、0.5、0、復帰中。
- Scene unload、Object destroy、Application終了中のasync。
- Attack/交代button連打と同frame入力。
- Coroutine/async内で意図的に例外を発生。
- Domain Reload無効でPlayを繰り返す。
- Fixed timestep設定を変更。
- Pause中のUI、Camera、Audio、VFX、Input buffer。
- 長時間稼働時のtimer精度。

## 22. 設計チェックリスト

- 各timerがscaled/unscaled/fixed/独自tickのどれか説明できるか。
- 数値の単位が変数名または型で分かるか。
- input sampleとsimulation consumeを分けたか。
- Stop/cancel/例外/破棄の全経路で状態を復旧できるか。
- async methodの完了を誰が所有するか明確か。
- Unity APIへ戻る前にmain threadか確認したか。
- Slow Motion要求を一つのTime Authorityが合成しているか。
- 計測前に「Coroutineだから軽い」と判断していないか。

## 公式資料

- [Unity Scripting API: Time](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/Time.html)
- [Unity Scripting API: Time.deltaTime](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/Time-deltaTime.html)
- [Unity Scripting API: Time.timeScale](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/Time-timeScale.html)
- [Unity Manual: Time and frame rate management](https://docs.unity3d.com/6000.0/Documentation/Manual/time-and-frame-rate-management.html)
- [Unity Manual: Coroutines](https://docs.unity3d.com/6000.0/Documentation/Manual/Coroutines.html)
- [Unity Manual: Awaitable introduction](https://docs.unity3d.com/6000.0/Documentation/Manual/async-awaitable-introduction.html)
- [Unity Manual: Awaitable completion and continuation](https://docs.unity3d.com/6000.0/Documentation/Manual/async-awaitable-continuations.html)

