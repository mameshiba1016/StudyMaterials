# 3Dアクション戦闘システム

> 対象: Unity 6.0。ここでは特定作品を複製せず、高速な三人称3Dアクションに共通する入力、状態、コンボ、判定、回避、受け流し、ロックオン、ヒット演出の原理を学びます。

## 1. 戦闘を一枚岩にしない

戦闘は次の処理を直列につないだsystemです。

```text
Device Input
  → Input Sampling / Buffer
  → Player Intent
  → Actionの実行可否判定
  → Combat State遷移
  → Animation / Movement
  → Hit Query
  → Damage Resolution
  → Reaction / Camera / Audio / VFX
```

`PlayerCombat.cs`一つへ全部書くと、animation変更がdamage計算を壊し、敵にも同じdamage処理を再利用できません。入力、規則、表示を分離します。

## 2. 最低限の責任分割

```text
PlayerInputReader     入力deviceを読む
InputBuffer           短時間だけ入力意図を保存
CombatBrain           現在stateから次actionを決定
ActionDefinition      attackの静的data
ActionRunner          actionの時間進行
HitDetector           対象候補を検出
DamageResolver        damage・guard・無敵を解決
Health                HPを所有
ReactionController    被弾反応を選択
CombatPresenter       Animator・音・VFX・cameraへ通知
```

「誰が真実を所有するか」を一つに決めます。HPを`Health`とUIの両方で減らしてはいけません。

## 3. 入力とゲーム内の意図を分ける

Input System固有の`InputAction.CallbackContext`を戦闘domainへ渡しません。

```csharp
public enum CombatCommand
{
    LightAttack,   // 弱攻撃を試みるという意図。
    HeavyAttack,   // 強攻撃を試みるという意図。
    Dodge,         // 回避を試みるという意図。
    Guard,         // guard開始または維持の意図。
    LockOn         // lock-on切替の意図。
}

public readonly record struct BufferedCommand(
    CombatCommand Command,
    double Time);  // frame番号ではなく入力時刻を保存すると可変frame rateでも期限を測れる。
```

こうするとAI、replay、network入力も同じ`CombatCommand`を生成できます。

## 4. 入力samplingの注意

Input ActionのcallbackはInput System updateの一部で発火し、設定されたupdate modeに応じて`Update`または`FixedUpdate`より前になります。callback内で直接attackを開始すると、game state更新順に依存します。

推奨はcallbackで意図を記録し、gameplay側の決まった地点で消費することです。

```csharp
public sealed class PlayerInputReader : MonoBehaviour
{
    [SerializeField] private InputActionReference lightAttack;
    public event Action<CombatCommand, double>? CommandIssued;

    private void OnEnable()
    {
        lightAttack.action.performed += OnLightAttack;
        lightAttack.action.Enable();
    }

    private void OnDisable()
    {
        // 無効化されたobjectをdelegateが参照し続けないよう必ず解除する。
        lightAttack.action.performed -= OnLightAttack;
        lightAttack.action.Disable();
    }

    private void OnLightAttack(InputAction.CallbackContext context)
    {
        // callbackでは戦闘stateを変更せず、入力が起きた事実だけ通知する。
        CommandIssued?.Invoke(CombatCommand.LightAttack, context.time);
    }
}
```

## 5. 入力buffer

攻撃終了の数frame前に次attackを押しても受理する仕組みです。「何でも予約」ではなく、commandごとに寿命と優先度を決めます。

```csharp
public sealed class CombatInputBuffer
{
    private readonly List<BufferedCommand> commands = new();
    private readonly double lifetimeSeconds;

    public CombatInputBuffer(double lifetimeSeconds)
        => this.lifetimeSeconds = lifetimeSeconds;

    public void Push(CombatCommand command, double time)
    {
        // 同一command連打を全保存するか上書きするかはgame design。
        commands.Add(new BufferedCommand(command, time));
    }

    public bool TryConsume(
        double now,
        Func<CombatCommand, bool> canExecute,
        out CombatCommand result)
    {
        // 古い入力から検査する例。回避優先などを入れるならpriority比較を別途行う。
        for (int i = 0; i < commands.Count; i++)
        {
            BufferedCommand entry = commands[i];

            if (now - entry.Time > lifetimeSeconds)
                continue; // 期限切れは後で一括削除する。

            if (!canExecute(entry.Command))
                continue; // 今は実行不能でも期限内なら残す設計。

            result = entry.Command;
            commands.RemoveAt(i); // 一度消費した入力を再実行しない。
            RemoveExpired(now);
            return true;
        }

        RemoveExpired(now);
        result = default;
        return false;
    }

    private void RemoveExpired(double now)
        => commands.RemoveAll(x => now - x.Time > lifetimeSeconds);
}
```

## 6. command bufferで決めるべき仕様

- 押下とreleaseを別commandにするか。
- 同一入力を複数保存するか。
- attack中のdodgeをattack入力より優先するか。
- pause中の時刻をbuffer寿命へ含むか。
- hit stop中にも入力受付時間を進めるか。
- deviceを跨いだ同時入力をどう解決するか。

## 7. 戦闘state

booleanを増やすだけの設計は矛盾を作ります。

```text
isAttacking = true
isDodging   = true
isStunned   = true
```

排他的な主状態は一つのstateとして表します。

```csharp
public enum CombatState
{
    Locomotion,
    Attacking,
    Dodging,
    Guarding,
    Stunned,
    KnockedDown,
    Dead
}
```

ただし`Grounded`や`LockedOn`は主状態と直交する場合があります。すべてを一enumへ詰めると組合せ爆発するため、直交状態は別component/state regionにします。

## 8. 状態遷移を表にする

| 現在 | 入力・event | 条件 | 次状態 |
|---|---|---|---|
| Locomotion | Light | staminaあり | Attacking |
| Attacking | Light | combo受付中 | 次のAttacking |
| Attacking | Dodge | dodge cancel中 | Dodging |
| Any except Dead | lethal damage | HP 0 | Dead |
| Guarding | guard break | posture 0 | Stunned |

`if`を書く前に表を作ると、未定義遷移が見えます。

## 9. Action Definitionをdata化する

```csharp
[CreateAssetMenu(menuName = "Combat/Action Definition")]
public sealed class CombatActionDefinition : ScriptableObject
{
    [field: SerializeField] public string ActionId { get; private set; } = "";
    [field: SerializeField] public int AnimatorStateHash { get; private set; }
    [field: SerializeField] public float Duration { get; private set; } = 0.5f;
    [field: SerializeField] public float StaminaCost { get; private set; } = 10f;
    [field: SerializeField] public float Damage { get; private set; } = 10f;
    [field: SerializeField] public float HitStopSeconds { get; private set; } = 0.04f;
    [field: SerializeField] public AnimationCurve ForwardMotion { get; private set; }
        = AnimationCurve.Linear(0f, 0f, 1f, 0f);
    [field: SerializeField] public ActionWindow[] Windows { get; private set; }
        = Array.Empty<ActionWindow>();
}

public enum ActionWindowType
{
    HitActive,       // 攻撃判定を有効化する時間。
    ComboAccept,     // 次段入力を予約できる時間。
    DodgeCancel,     // 回避へ遷移できる時間。
    Turn,            // target方向へ旋回できる時間。
    SuperArmor       // 怯みを無効化する時間。damage無効とは別。
}

[Serializable]
public struct ActionWindow
{
    public ActionWindowType Type;
    [Range(0f, 1f)] public float NormalizedStart;
    [Range(0f, 1f)] public float NormalizedEnd;
}
```

ScriptableObject assetは静的定義です。実行中のcombo段数や命中済みtarget集合をassetへ保存すると、全characterが状態を共有してしまいます。

## 10. normalized timeと実時間

`normalizedTime = elapsed / duration`ならaction速度を変えても窓を相対指定できます。一方、厳密なparry 0.1秒などは秒指定の方が意図を保ちます。

- animation比率に結び付く窓: normalized time。
- 人間の反応時間に結び付く窓: seconds。
- network/deterministic simulation: fixed tick。

どの時間軸かをfield名へ含めます。

## 11. Animationを戦闘の唯一の真実にしない

Animator State Machineは表示遷移に強い一方、gameplay ruleまでAnimator parameterへ隠すとtestが難しくなります。

```text
CombatBrainが「Attack02を開始」
  → gameplay action timeを進める
  → Animatorへ表示要求
```

Animatorはpresenter寄りにし、damageやresource消費のauthorityはgameplay側へ置きます。

## 12. Animation Eventの扱い

Animation Eventはclip上の時点からmethodを呼べて便利ですが:

- clip名・method名変更に弱い。
- blendやspeed変更でgameplay timingが分かりにくい。
- import animationへeventが埋まる。
- unit testしにくい。

足音やVFXなど表示eventには有効です。致命的なdamage ruleは明示的なaction timelineで管理する方が追跡しやすいです。

## 13. StateMachineBehaviour

Animator stateのenter/update/exit callbackを受けられます。ただしここへ戦闘全体を入れるとAnimator依存が強くなります。表示同期、IK重み、effect通知など狭い用途に限定します。

## 14. Root Motionとcode movement

### Root Motion

- animation authored軌道と足運びが一致しやすい。
- attackの踏み込み表現に強い。
- target距離や坂への動的補正が必要。

### Code Motion

- collision、network、数値調整を統制しやすい。
- animationとの足滑り対策が必要。

実務ではlocomotionはcode、attack displacementはcurve/root motionを調整してcharacter motorへ要求するhybridも有効です。`transform.position`を複数systemから直接変更しません。

## 15. Motion request

```csharp
public readonly record struct MotionRequest(
    Vector3 WorldVelocity,
    Quaternion? DesiredFacing,
    bool UseGravity);

public interface ICharacterMotor
{
    // 実移動は一箇所へ集約し、attackとlocomotionの競合を解決する。
    void Move(in MotionRequest request, float deltaTime);
}
```

## 16. Comboを配列番号だけで決めない

branchを持つならgraphとして考えます。

```text
Light1 ─Light→ Light2 ─Light→ Light3
   └Heavy→ Launcher
Light2 ─Heavy→ Sweep
AnyAttack ─Dodge→ Dodge
```

edgeには「必要command」「受付window」「resource」「地上/空中」「target条件」を持たせます。

## 17. Combo edge

```csharp
[Serializable]
public struct ComboTransition
{
    public CombatCommand RequiredCommand;
    public CombatActionDefinition NextAction;
    public float EarliestNormalizedTime;
    public float LatestNormalizedTime;
}
```

同じcommandに複数edgeがあるなら、距離、方向入力、空中状態、priorityから一意に選びます。

## 18. Cancelは例外ではなく遷移規則

cancelは再生中actionを無条件に止める機能ではありません。

```text
Attack startup:  cancel不可
Attack active:   特定skillのみcancel可
Attack recovery: dodge/jump/次comboへcancel可
Hit confirm時:   特別なcancel routeを開く
```

`CanCancel(from, to, time, context)`を一箇所で判断し、各button handlerへ散らしません。

## 19. Hitboxの方式

- 武器Colliderをtriggerとして有効化。
- 骨の前frame位置からsweep/cast。
- authored shapeをcharacter基準でquery。
- projectileを生成。

高速武器では現在位置のOverlapだけだとframe間をすり抜けます。前位置から現在位置までsweepするか、複数sampleします。

## 20. Queryとcollision callback

能動攻撃は`Physics.OverlapSphereNonAlloc`などのqueryで明示的に検出すると、攻撃windowとの対応を管理しやすくなります。頻繁な標準Overlap系は結果配列を割り当てるため、事前確保bufferを検討します。

```csharp
public sealed class SphereHitDetector : MonoBehaviour
{
    [SerializeField] private Transform origin;
    [SerializeField] private float radius = 1f;
    [SerializeField] private LayerMask hittableLayers;

    // 毎回newしない。容量超過時は残りが返らないため、Profilerと警告で調整する。
    private readonly Collider[] results = new Collider[32];

    public int Collect(Span<Collider> destination)
    {
        int count = Physics.OverlapSphereNonAlloc(
            origin.position,
            radius,
            results,
            hittableLayers,
            QueryTriggerInteraction.Collide);

        int copyCount = Mathf.Min(count, destination.Length);
        for (int i = 0; i < copyCount; i++)
            destination[i] = results[i];

        return copyCount;
    }
}
```

## 21. LayerMaskは最初のfilter

全Colliderを拾ってから`GetComponent`で選別すると無駄が増えます。Physics Layerで候補を絞り、team、alive、invulnerableなどgameplay条件を後段で判定します。

## 22. GetComponentの境界

```csharp
public interface IHurtbox
{
    IDamageReceiver Owner { get; }
    HurtboxType Type { get; }
}

public interface IDamageReceiver
{
    DamageResult Receive(in DamageRequest request);
}
```

一体に複数hurtboxがあるため、Collider単位ではなく`Owner`単位で重複排除します。

## 23. 一振り一回の命中

```csharp
public sealed class HitSession
{
    private readonly HashSet<IDamageReceiver> hitTargets = new();

    public bool TryMark(IDamageReceiver target)
        => hitTargets.Add(target); // 初回だけtrue。

    public void BeginNewSwing()
        => hitTargets.Clear();     // 次の攻撃判定session開始時だけclear。
}
```

multi-hit攻撃ならhit interval、最大hit数、同一target再命中時間をdata化します。

## 24. Damage RequestとResult

```csharp
public readonly record struct DamageRequest(
    int AttackId,
    GameObject Instigator,
    float BaseDamage,
    float PostureDamage,
    Vector3 HitPoint,
    Vector3 HitDirection,
    DamageTags Tags);

[Flags]
public enum DamageTags
{
    None       = 0,
    Melee      = 1 << 0,
    Projectile = 1 << 1,
    Unblockable= 1 << 2,
    Launch     = 1 << 3
}

public readonly record struct DamageResult(
    bool Accepted,
    bool WasInvulnerable,
    bool WasBlocked,
    bool WasParried,
    bool WasLethal,
    float AppliedDamage);
```

攻撃側は「命中したつもり」ではなく`DamageResult`を見てhit stop、音、effectを変えます。

## 25. Damage解決順

一例:

```text
対象が死亡済みか
→ team/friendly fire
→ 無敵
→ parry
→ guard方向とunblockable
→ armor/耐性
→ HP適用
→ posture適用
→ reaction選択
→ 結果event発行
```

順序は仕様です。分散した`if`ではなく、一つのresolverまたは明示pipelineにします。

## 26. 無敵と無反応を分ける

- Invulnerable: damageを受けない。
- Super Armor: damageは受けるが通常怯みを無効化。
- Untargetable: lock-on候補にならない。
- Intangible: collision/hit query対象外。

`isInvincible`一つでは表現不足です。

## 27. Dodge

Dodgeには少なくとも次を定義します。

- startup、invulnerability、recovery。
- 移動方向決定時点。
- stamina cost。
- input buffer/cancel規則。
- collision処理。
- lock-on時と非lock-on時の向き。

無敵時間をanimation clipの見た目だけに依存させず、action windowとして検証可能にします。

## 28. Parry

Parryは攻撃側と防御側の時間窓が重なるだけでは足りません。

- 攻撃がparry可能tagか。
- attackerとdefenderの向き。
- 一つのattackを複数回parryしないか。
- 成功時に誰をstunするか。
- hit stop中のclock。
- projectile反射の所有権。

結果を`DamageResult.WasParried`として同じ解決経路へ戻すと演出を統一できます。

## 29. Guard方向

```csharp
public static bool IsInsideGuardCone(
    Vector3 defenderForward,
    Vector3 directionToAttacker,
    float halfAngleDegrees)
{
    Vector3 a = Vector3.ProjectOnPlane(defenderForward, Vector3.up).normalized;
    Vector3 b = Vector3.ProjectOnPlane(directionToAttacker, Vector3.up).normalized;
    return Vector3.Dot(a, b) >= Mathf.Cos(halfAngleDegrees * Mathf.Deg2Rad);
}
```

zero vector、異なるup軸、高低差の仕様も決めます。

## 30. Hit Reaction

reactionはdamage量だけでなく次から選びます。

- 軽/重、launch、knockdown。
- hit方向。
- airborne/grounded。
- armor/posture break。
- 現在actionのinterrupt耐性。
- lethalか。

animation名をdamage resolverへ直書きせず、意味的な`ReactionRequest`を発行します。

## 31. Hit Stop

Hit stopは命中の瞬間を強調する短い停止です。単純に`Time.timeScale = 0`するとUI、全敵、camera、networkまで止まります。

選択肢:

- attacker/victim Animatorとmotorだけ速度0。
- local timescale componentを用意。
- global timeScaleを使うがunscaled系との境界を明示。

複数hit stop要求が重なるとき、最大値、加算、上書きの規則が必要です。

## 32. Hit Stop scheduler

```csharp
public sealed class LocalHitStop
{
    private float remainingUnscaledSeconds;

    public bool IsStopped => remainingUnscaledSeconds > 0f;

    public void Request(float seconds)
    {
        // 短い要求で既存の長い停止を打ち消さない。
        remainingUnscaledSeconds = Mathf.Max(remainingUnscaledSeconds, seconds);
    }

    public void Tick(float unscaledDeltaTime)
    {
        remainingUnscaledSeconds = Mathf.Max(
            0f,
            remainingUnscaledSeconds - unscaledDeltaTime);
    }
}
```

## 33. Camera shakeとImpulse

camera揺れはdamage処理からcameraを直接探して呼ばず、`HitConfirmed` eventをpresentationが受け取ります。Cinemachine Impulseを使う場合もpackage versionでAPIが変わり得るため、そのversionのmanual/APIを確認します。

強度は「damageに完全比例」ではなく、attack category、距離、画面占有率、連続hit時の上限で調整します。揺らし過ぎは視認性と酔いを悪化させます。

## 34. Lock-on候補検索

候補scoreの例:

```text
score = angleWeight    * viewport中心からの角度
      + distanceWeight * 距離
      + occlusionPenalty
      + currentTargetBonus
```

current targetへbonusを与えると、僅かな順位変化によるtargetのちらつきを防げます。

## 35. 画面内判定

`Camera.WorldToViewportPoint`で:

- `z > 0`か。
- `x`,`y`が許容範囲内か。
- ray/sphere castで遮蔽されていないか。
- target pointが胸/中心など適切か。

を確認します。毎frame全敵を検索せず、近傍候補cacheと更新間隔を使います。

## 36. Target切替

右入力なら、現在targetから見たscreen spaceの右側候補を選びます。world spaceの左右だけではcamera角度と一致しません。

```text
candidateScreenX - currentScreenX > threshold
```

同じ側では水平差、垂直差、距離をscore化します。

## 37. Soft Targeting

常時lock-onでなくてもattack開始時に近い敵へ短時間だけ:

- facing補正。
- attack trajectory補正。
- 軽い吸着移動。

を行えます。補正角、最大移動距離、遮蔽、入力方向を無視しない上限が必要です。

## 38. Rotation authority

同じframeでcamera-relative locomotion、lock-on、attack tracking、root motionがrotationを書くと震えます。

priority例:

```text
Dead/Stunned > Attack authored turn > Dodge direction > Lock-on > Locomotion
```

最終rotationを一つのmotorへ渡します。

## 39. UpdateとFixedUpdate

- Input/command/action判断: 通常`Update`側。
- Rigidbody physics操作: `FixedUpdate`側。
- camera: target移動後の`LateUpdate`相当。

入力をFixedUpdateだけで読むと短い押下を取り逃す可能性があります。Updateでbufferし、physics tickで必要なrequestを消費します。

## 40. 可変delta timeと固定tick

`elapsed += Time.deltaTime`はsingle-playerの多くで十分ですが、再現性が必要ならfixed simulation tickを使います。見た目のanimation時間とgameplay simulation tickを混同しません。

## 41. Eventの使い分け

- 即時の戻り値が必要: method call (`ReceiveDamage`)。
- 一対多の副作用: event (`DamageApplied`)。
- 遅延/順序制御が必要: command/event queue。

damage可否をevent購読者の誰かに決めさせる設計は結果が不明瞭です。

## 42. Event payload

`OnHit()`だけでは情報不足です。攻撃ID、instigator、victim、hit point、resultをimmutable payloadで渡します。ただし`Collider`などScene objectを長期間queueへ保持する場合は寿命に注意します。

## 43. Object Poolとの境界

hit VFX、damage number、projectileはpool候補です。返却時に:

- Particleを停止・clear。
- event購読解除。
- target参照をnull化。
- coroutine/cancellationを停止。
- transform/local scaleを初期化。

を行います。poolは状態resetの責任を増やします。

## 44. Audioの重複制御

連続hitで同じ音を無制限に鳴らすとclipします。voice limit、cooldown、pitch variation、重要度、距離を設計します。parry成功音など重要eventは通常hit音より高priorityにします。

## 45. VFXとgameplay判定を分離

trailの見た目をhitboxとして使わず、同じaction data/timeから別々に駆動します。VFXが重くてskipされてもdamage規則は変わりません。

## 46. 戦闘Debug表示

Scene/Game viewへ次を可視化します。

- hit shapeとsweep軌道。
- active/cancel/invulnerable window。
- 現state、action ID、normalized time。
- buffer中commandと残り寿命。
- lock-on候補score。
- damage resolverの拒否理由。

「当たらない」を数字と図形で診断できるようにします。

## 47. Gizmos例

```csharp
private void OnDrawGizmosSelected()
{
    if (origin == null)
        return;

    Gizmos.color = Color.red;
    Gizmos.DrawWireSphere(origin.position, radius);
}
```

Editor用表示をruntime判定そのものにしません。

## 48. Testすべき純粋規則

- 入力bufferの期限境界。
- combo windowの開始/終了ちょうど。
- cancel priority。
- invulnerable、guard、parryの解決順。
- 同一target重複命中防止。
- lethal damage。
- lock-on scoreとtie-break。
- stamina不足。

純粋C#へ分ければAnimatorやSceneなしでEdit Mode testできます。

## 49. Play Mode Test

Physics、Animator、GameObject寿命が関わる部分はPlay Modeで確認します。

- 高速sweepがframe rate差で命中するか。
- disabled/destroyed targetを安全に扱えるか。
- animation blend中にwindowが二重発火しないか。
- pooled projectileが再利用時に以前のownerを持たないか。

## 50. Performance確認

- 毎frameの`Find*`を避ける。
- Physics Layerでquery候補を減らす。
- query buffer溢れを計測する。
- LINQ、closure、boxingをhot pathで確認する。
- Animator parameter hashを事前計算する。
- VFX/Audio/projectileをpoolする。
- 多数AIのtarget queryを時間分散する。

最適化前にProfilerでCPU、GC、Physics、Renderingの実測を取ります。

## 51. よくある失敗

### Animation Eventだけでdamage

clip変更で規則が壊れ、testしにくい。action timelineをauthorityにする。

### boolの組合せでstate管理

矛盾stateになる。排他的stateと直交stateを分ける。

### Collider一個ごとにdamage

同じ敵の複数hurtboxへ多重命中する。receiver単位で重複排除する。

### `Time.timeScale`だけでhit stop

全systemへ副作用が出る。local/global停止範囲を仕様化する。

### ScriptableObjectへruntime状態

複数instanceで共有される。definitionとruntime instanceを分ける。

### Animatorがgameplay authority

遷移blendや表示都合がruleへ漏れる。domain stateを別に持つ。

## 52. 最小実装順

1. 一段attackと一体のdummy。
2. 入力buffer。
3. action state/window。
4. hit queryと重複排除。
5. damage request/result。
6. dodgeと無敵。
7. combo graph/cancel。
8. lock-on/soft targeting。
9. hit reaction/stop/camera/audio/VFX。
10. debug表示、test、profile。

各段階を動作確認してから増やします。

## 53. 推奨folderとassembly

```text
Game.Combat.Domain
  Commands/
  Actions/
  Damage/
  State/

Game.Combat.Unity
  Input/
  Physics/
  Animation/
  Presentation/

Game.Combat.Tests
```

Domainは可能ならUnityEngine非依存にし、Vectorなどが必要なら小さい独自値型またはadapter境界を検討します。

## 54. 完成確認表

- [ ] 入力callbackから直接stateを壊していない。
- [ ] buffer寿命と優先度が仕様化されている。
- [ ] state遷移表がある。
- [ ] definitionとruntime状態が分離されている。
- [ ] hitbox/hurtbox/receiverの責任が分かれている。
- [ ] 同一攻撃の重複命中を防いでいる。
- [ ] damage解決順をtestできる。
- [ ] dodge/parry/guard/cancelの時間窓が可視化できる。
- [ ] movement/rotation authorityが一つである。
- [ ] hit stopが停止対象を明示している。
- [ ] Physics query buffer溢れを検出できる。
- [ ] ProfilerでGC allocationとPhysics costを確認した。
- [ ] 特定frame rateだけで成立する挙動がない。

## 55. 確認問題

1. なぜInput callback内でattackを直接開始せず、commandへ変換するのか。
2. 入力bufferとcombo受付windowは何が違うか。
3. 排他的stateと直交stateの例を挙げてください。
4. ScriptableObjectへ命中済みtarget集合を置いてはいけない理由は何か。
5. 高速な武器に現在位置のOverlapだけでは不足する理由は何か。
6. NonAlloc queryのbufferが小さ過ぎると何が起きるか。
7. Hurtbox単位でなくreceiver単位で重複排除する理由は何か。
8. 無敵とsuper armorの違いは何か。
9. Hit stopにglobal timeScaleを使う場合の副作用を挙げてください。
10. Animatorをgameplayの唯一のauthorityにすると何が難しくなるか。

## 56. 公式資料

- [Unity 6 Animation State Machine](https://docs.unity3d.com/ja/6000.0/Manual/AnimationStateMachines.html)
- [Unity 6 State Machine Behaviour](https://docs.unity3d.com/ja/current/Manual/StateMachineBehaviours.html)
- [Unity 6 Input System](https://docs.unity3d.com/ja/current/Manual/com.unity.inputsystem.html)
- [InputAction API](https://docs.unity3d.com/Packages/com.unity.inputsystem@1.17/api/UnityEngine.InputSystem.InputAction.html)
- [Physics Query Optimization](https://docs.unity3d.com/6000.0/Documentation/Manual/physics-optimization-raycasts-queries.html)

Package APIは導入versionと一致するdocumentを参照してください。
