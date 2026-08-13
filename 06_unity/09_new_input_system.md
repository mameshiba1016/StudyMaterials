# New Input System・Action・入力バッファ

> Input SystemはPackageとして更新される。Unity Editorの版だけでなく、Package Managerで実際の`com.unity.inputsystem`版を確認すること。

## 1. DeviceではなくActionを読む

悪い密結合:

```csharp
// 特定keyとGameplay処理が直結し、Gamepad、rebind、UI切替が拡張しにくい。
// if (Keyboard.current.spaceKey.wasPressedThisFrame) DealDamage();
```

推奨する流れ:

```text
Device Control
 → Binding / Control Scheme
 → Input Action
 → Player command
 → Combat state machineが受理判定
 → Gameplay event / hit判定
```

入力callbackの中で直接damageを与えません。入力は「攻撃したい」というcommandであり、stun中か、cancel可能frameか、energyが足りるかをCombat層が決めます。

## 2. 用語

- `InputActionAsset`: `.inputactions`に保存されるAction全体。
- `Action Map`: Gameplay、UI、Debug等の文脈単位。
- `Action`: Move、Attack、Dodge、SwitchCharacter等の意図。
- `Binding`: key/button/stickとActionの対応。
- `Composite Binding`: WASD等を1つのVectorへ合成する。
- `Interaction`: Tap、Hold、MultiTap等、入力成立条件。
- `Processor`: Deadzone、Normalize、Scale等、値の加工。
- `Control Scheme`: Keyboard&Mouse、Gamepad等のDevice組合せ。

## 3. Action phase

Action callbackには概ね`started`、`performed`、`canceled`があります。どのphaseが発生するかはAction TypeやInteractionに依存するため、Attackは押した瞬間、Chargeは開始/保持成立/解除というように設計してtestします。

```csharp
using UnityEngine;
using UnityEngine.InputSystem;

public sealed class PlayerInputReader : MonoBehaviour
{
    [SerializeField] private InputActionReference moveAction;
    [SerializeField] private InputActionReference attackAction;

    public Vector2 Move { get; private set; }
    public bool AttackRequested { get; private set; }

    private void OnEnable()
    {
        moveAction.action.Enable();
        attackAction.action.Enable();

        moveAction.action.performed += OnMove;
        moveAction.action.canceled += OnMoveCanceled;
        attackAction.action.performed += OnAttack;
    }

    private void OnDisable()
    {
        // 登録した同じmethodを解除する。二重購読を防ぐ。
        moveAction.action.performed -= OnMove;
        moveAction.action.canceled -= OnMoveCanceled;
        attackAction.action.performed -= OnAttack;

        moveAction.action.Disable();
        attackAction.action.Disable();
    }

    private void OnMove(InputAction.CallbackContext context)
    {
        Move = context.ReadValue<Vector2>();
    }

    private void OnMoveCanceled(InputAction.CallbackContext context)
    {
        Move = Vector2.zero;
    }

    private void OnAttack(InputAction.CallbackContext context)
    {
        // ここではGameplay結果を確定せず、要求だけを記録する。
        AttackRequested = true;
    }

    public bool ConsumeAttack()
    {
        if (!AttackRequested)
        {
            return false;
        }

        AttackRequested = false;
        return true;
    }
}
```

## 4. 連続値とedge入力

- Move/Aim: 現在値を保持し、simulation updateで読む。
- Attack/Dodge/Switch: 押下edgeをcommand/bufferへ積む。
- Charge: started、held duration、canceled/performedの意味を仕様化する。

入力callbackの回数と`Update`/`FixedUpdate`の回数は同じとは限りません。PhysicsをFixedUpdateで動かす場合も、短い押下をbool一個だけで雑に上書きせず、timestamp付きbufferを使うと取りこぼしを減らせます。

## 5. 入力バッファ

```csharp
using System.Collections.Generic;

public enum PlayerCommandType
{
    Attack,
    Dodge,
    SwitchNext
}

public readonly struct PlayerCommand
{
    public PlayerCommand(PlayerCommandType type, double time, uint sequence)
    {
        Type = type;
        Time = time;
        Sequence = sequence;
    }

    public PlayerCommandType Type { get; }
    public double Time { get; }
    public uint Sequence { get; }
}

public sealed class CommandBuffer
{
    private readonly Queue<PlayerCommand> queue = new();
    private uint nextSequence;

    public void Push(PlayerCommandType type, double time)
    {
        queue.Enqueue(new PlayerCommand(type, time, nextSequence++));
    }

    public bool TryPop(double now, double validSeconds, out PlayerCommand command)
    {
        while (queue.Count > 0)
        {
            PlayerCommand candidate = queue.Dequeue();
            if (now - candidate.Time <= validSeconds)
            {
                command = candidate;
                return true;
            }
        }

        command = default;
        return false;
    }
}
```

実戦ではcommand種別ごとの保持時間、同一command圧縮、優先度、pause中の時計、rollback/replay用tickを定義します。`Time.time`、unscaled time、Input System event timeのどれを基準にするか混在させません。

## 6. Action Mapで文脈を切る

```text
Gameplay Map: Move / Camera / Attack / Dodge / Switch
UI Map: Navigate / Submit / Cancel / Point / Click
PhotoMode Map: Camera / Capture / Exit
```

Menu中にGameplay Mapも有効だと、決定buttonで攻撃が発生することがあります。状態遷移の一箇所でmapを切り替え、例外的な同時有効化は明示します。

## 7. PlayerInput

`PlayerInput`はAction、Device pairing、通知方法などをまとめる高水準Componentです。local multiplayerではPlayerごとにActionのcopyとDeviceを扱えます。

通知方式にはSend Messages、Unity Events、C# Events等があり、版による表示名も確認します。文字列messageは手軽ですがrename耐性や追跡性を考え、大規模ロジックでは型付きcallbackやgenerated C# wrapperも検討します。

## 8. Generated C# class

`.inputactions` AssetからC# wrapperを生成すると、文字列でAction名を検索する箇所を減らせます。

```csharp
// class名やinterface名はInput Actions Editorの生成設定に依存する概念例。
private GameInputActions actions;

private void Awake()
{
    actions = new GameInputActions();
}

private void OnEnable()
{
    actions.Gameplay.Enable();
}

private void OnDisable()
{
    actions.Gameplay.Disable();
}
```

wrapperを`new`した所有者は、callback解除、Disable、必要ならDisposeまで寿命を管理します。`PlayerInput.actions`はPlayerごとのcopyを扱う場合があるため、Project AssetのactionとPlayer instance側actionを混同しません。

## 9. Character交代では入力所有者を交換しない

3人編成のCharacterを切り替えるたびに各CharacterがDeviceを直接読むと、旧Characterの購読解除漏れが起きます。

```text
PlayerInputOwner（Player寿命）
 → CommandBuffer
 → Party/Combat Director
 → Current Characterへcommandを配送
```

DeviceとAction MapはPlayer層が所有し、Characterは渡されたcommandを処理します。交代中の無敵、登場攻撃、cooldown、入力先切替を一つのstate transitionとして扱えます。

## 10. RebindingとDevice変更

- Binding IDを安定識別に使い、表示文字列だけを保存keyにしない。
- override保存/読込のAPIは利用Package版を確認する。
- 同じcontrolの重複、Cancel key、必須binding消失を検査する。
- Gamepad切断・再接続時のpauseや表示更新を設計する。
- Glyph表示は現在Control Schemeとbindingを元に更新する。
- stick driftにはdeadzoneを使うが、Aim精度とのtrade-offを実機測定する。

## 11. よくある不具合

- `OnEnable`ごとに匿名lambdaを登録し解除できず、攻撃が多重発火する。
- GameplayとUIのmapが同時に有効で両方反応する。
- `performed`だけを使い、button releaseが必要なchargeを実装できない。
- callback内でAnimator、damage、VFXを全部直接呼び、test不能になる。
- `Update`で毎frame入力Queueをclearし、FixedUpdateが読む前に消える。
- Character交代後も旧CharacterがActionを購読する。
- Package更新後にgenerated wrapper/API差分を確認しない。

## 12. テスト行列

| 観点 | 例 |
|---|---|
| Device | Keyboard&Mouse、Xbox系、PlayStation系相当、切断 |
| Frame rate | 30、60、120、瞬間的なstall |
| Context | Gameplay、Pause、UI、Scene遷移、Photo Mode |
| Input | 短押し、長押し、同時押し、連打、反対方向同時入力 |
| Character | 通常、交代中、死亡、stun、登場直後 |
| Time | timeScale 0、slow motion、unscaled UI |

## 公式資料

- [Input System Manual: Actions](https://docs.unity3d.com/Packages/com.unity.inputsystem@1.11/manual/Actions.html)
- [Input System Manual: PlayerInput](https://docs.unity3d.com/Packages/com.unity.inputsystem@1.11/manual/PlayerInput.html)
- [Input System Manual: Interactions](https://docs.unity3d.com/Packages/com.unity.inputsystem@1.11/manual/Interactions.html)
- [Input System Manual: Action Bindings](https://docs.unity3d.com/Packages/com.unity.inputsystem@1.11/manual/ActionBindings.html)

