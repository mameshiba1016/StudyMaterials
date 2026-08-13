# 14 高速アクション向け入力バッファとCommand設計

## 1. 入力検出とAction開始は同じではない

攻撃Buttonを押した瞬間、Characterが前の攻撃硬直中なら即座に次の攻撃を開始できません。しかし入力を捨てると操作感が悪くなります。短時間だけ意図を保存し、Cancel可能な時点で消費するのが入力バッファです。

```text
Enhanced Inputが押下を検出
        ↓
Commandへ変換してBufferへ保存
        ↓
Combat SystemがCancel Windowを開く
        ↓
期限内かつ実行可能なCommandを選ぶ
        ↓
コスト消費・状態遷移・Animation開始
```

## 2. Commandデータ

```cpp
UENUM()
enum class ECombatCommandType : uint8
{
    LightAttack,
    HeavyAttack,
    Dodge,
    SwitchCharacter,
    Assist
};

USTRUCT()
struct FBufferedCombatCommand
{
    GENERATED_BODY()

    ECombatCommandType Type = ECombatCommandType::LightAttack;
    double InputTimeSeconds = 0.0;
    FVector2D Direction = FVector2D::ZeroVector;
    int32 Sequence = 0; // 同時刻付近の入力順を安定して判定する番号。
};
```

物理キーではなく意味を保存します。必要なら入力方向、押下／解放、対象候補、Device情報を持たせますが、寿命の短い生ポインタを不用意に保存しません。

## 3. 固定容量Buffer

```cpp
class FCombatInputBuffer
{
public:
    void Push(const FBufferedCombatCommand& Command)
    {
        // 入力連打で無制限に増えないよう上限を設ける。
        if (Commands.Num() >= MaxCommands)
        {
            Commands.RemoveAt(0);
        }
        Commands.Add(Command);
    }

    void RemoveExpired(double NowSeconds, double LifetimeSeconds)
    {
        Commands.RemoveAll(
            [NowSeconds, LifetimeSeconds](const FBufferedCombatCommand& Command)
            {
                return NowSeconds - Command.InputTimeSeconds > LifetimeSeconds;
            });
    }

private:
    static constexpr int32 MaxCommands = 8;
    TArray<FBufferedCombatCommand> Commands;
};
```

実製品では先頭削除の頻度を測り、Ring Buffer等も検討します。ただし数件のBufferで複雑化する前に、正しさとデバッグ容易性を優先します。

## 4. 実行可否と優先順位

```cpp
bool UCombatComponent::CanExecute(ECombatCommandType Type) const
{
    switch (Type)
    {
    case ECombatCommandType::Dodge:
        return Stamina >= DodgeCost && CurrentAction.CanCancelIntoDodge();

    case ECombatCommandType::LightAttack:
        return CurrentAction.CanCancelIntoAttack();

    case ECombatCommandType::SwitchCharacter:
        return PartyState && PartyState->CanSwitch();

    default:
        return false;
    }
}
```

「回避は攻撃より優先」「交代は特定硬直を無視できる」などの規則は、if文の順番へ偶然依存させず、優先度表またはCancel Ruleとしてデータ化します。

## 5. Animation Notifyとの関係

Animation NotifyでCancel Windowを開閉する方法は調整しやすい一方、Animationだけを戦闘状態の唯一のAuthorityにすると、再生中断やBlendで不整合が起きます。

```cpp
void UCombatComponent::OpenCancelWindow(ECancelWindow Window)
{
    ActiveCancelWindow = Window;
    TryConsumeBufferedCommand();
}

void UCombatComponent::CloseCancelWindow(ECancelWindow Window)
{
    if (ActiveCancelWindow == Window)
    {
        ActiveCancelWindow = ECancelWindow::None;
    }
}
```

Combat Stateが規則を所有し、Notifyは時間上の合図として使います。Montage終了・中断時にはWindowを必ず閉じるFail-safeを用意します。

## 6. 入力Bufferと先行入力を分ける

- **入力Buffer**：今は実行不能な入力を短時間保持。
- **先行入力**：特定Action中に次の分岐を予約。
- **同時押し判定**：時間窓内の複数入力を一つのCommandへ解釈。
- **Hold判定**：押下から解放までの時間を評価。

全部を一つのbool群で表すと状態が残留します。Command、期限、消費済み、現在Action IDを持ち、古いAction由来の入力を区別します。

## 7. Character交代と支援攻撃

交代入力は次の結果へ分岐し得ます。

```text
通常時        → 即時交代
攻撃Cancel中  → Cancel交代
被弾危険時    → 極限支援候補
交代不能      → Bufferまたは拒否Feedback
```

入力層がどの交代を実行するか決めるのではなく、Party／Combat Systemへ`SwitchCharacter` Commandを渡し、現在文脈からActionを選択します。

## 8. 時間の扱い

- 入力猶予はFrame数ではなく秒で定義するとFrame Rate依存を避けやすい。
- PauseやTime Dilationの影響を受ける時間か、実時間かを選ぶ。
- ネットワークではClient時刻をそのままAuthority判定へ信用しない。
- Replay／テストでは決定的に再現できるSequenceとTimestampを記録する。

## 9. デバッグ表示

画面上に次を時系列表示すると、操作感の問題をコードへ結び付けやすくなります。

```text
12.410 Attack Pressed
12.411 Buffered: LightAttack, expires 12.561
12.498 CancelWindow Open: Attack
12.499 Consumed: LightAttack
12.500 State: Attack01 → Attack02
```

入力されたのに検出されなかったのか、検出したが期限切れか、規則で拒否されたか、Animation開始に失敗したかを分離して記録します。

## 10. テスト項目

- 低Frame Rateと高Frame Rateで猶予が同等か。
- Button連打でBufferが無限成長しないか。
- 回避と攻撃の同時入力が規則通りか。
- Montage中断後にCancel Windowが残らないか。
- キャラクター交代後、旧CharacterのBufferが発火しないか。
- Pause解除直後に古い入力が実行されないか。
- GamepadとKeyboardで同じAction規則を通るか。

入力の気持ちよさは、猶予を広げるだけでなく「何が受理され、いつ実行され、拒否時に何を返すか」を一貫させることで生まれます。
