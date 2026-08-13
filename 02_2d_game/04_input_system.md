# 入力システム

入力システムは物理デバイス状態を、ゲーム上の意味を持つアクションへ変換します。ゲームコードが特定キーを直接読むと、再割当、ゲームパッド、UI、リプレイが難しくなります。

## 三つの状態

```cpp
struct ButtonState
{
    bool isDown{};     // 現在押されている。
    bool wasPressed{}; // このフレームで押されていない→押された。
    bool wasReleased{};// このフレームで押された→離された。
};
```

前フレームと現在を比較します。

```cpp
ButtonState MakeButtonState(bool previousDown, bool currentDown)
{
    return ButtonState{
        .isDown = currentDown,
        .wasPressed = !previousDown && currentDown,
        .wasReleased = previousDown && !currentDown
    };
}
```

- 移動：`isDown`を継続使用。
- ジャンプ開始：`wasPressed`を一度使用。
- 溜め攻撃解放：`wasReleased`を使用。

OSのキーリピートを「押した瞬間」として使うと、環境設定で回数が変わります。

## アクションマッピング

```cpp
enum class InputAction
{
    MoveLeft,
    MoveRight,
    Jump,
    Attack,
    Dodge,
    Pause
};
```

`Space`ではなく`Jump`をゲームへ渡します。一つのアクションへキーボード、パッド、アクセシビリティ入力を複数割当できます。

```cpp
if (input.WasPressed(InputAction::Jump))
{
    player.TryJump();
}
```

## 軸入力

```cpp
struct Vector2
{
    float x{};
    float y{};
};

Vector2 moveInput{input.GetAxis2D("Move")};
```

デジタルキーは-1、0、1、アナログスティックは連続値を返します。斜め入力`(1, 1)`をそのまま速度へ掛けると長さが`√2`になり、斜めだけ速くなります。長さが1を超える場合に正規化・クランプします。

## デッドゾーン

アナログスティックは手を離しても微小値を返すことがあります。

```cpp
Vector2 ApplyRadialDeadZone(Vector2 value, float deadZone)
{
    const float length{Length(value)};
    if (length <= deadZone)
    {
        return {};
    }

    const float remappedLength{(length - deadZone) / (1.0F - deadZone)};
    return Normalize(value) * std::clamp(remappedLength, 0.0F, 1.0F);
}
```

軸ごとのデッドゾーンは斜め方向の形が四角くなります。移動にはradial、トリガーにはaxialなどデバイスに合わせます。内側だけでなく外側デッドゾーンや応答曲線もあります。

## 入力バッファ

着地直前にジャンプを押した時、数tickだけ入力を保存すると操作感が改善します。

```cpp
if (input.WasPressed(InputAction::Jump))
{
    jumpBufferRemainingSeconds = jumpBufferDurationSeconds;
}

jumpBufferRemainingSeconds = std::max(
    0.0,
    jumpBufferRemainingSeconds - deltaSeconds
);

if (canJump && jumpBufferRemainingSeconds > 0.0)
{
    Jump();
    jumpBufferRemainingSeconds = 0.0;
}
```

同様に、足場から離れた直後でもジャンプ可能にするcoyote timeがあります。受付時間はゲーム時間か実時間か、ポーズ中に消費するかを決めます。

## 入力コンテキスト

ゲームプレイ中のAttackボタンが、メニューでは決定になります。入力レイヤーまたはコンテキストを切り替えます。

```text
Debug Console（最優先）
Pause Menu
Gameplay
```

上位が入力を消費したら下位へ渡さない設計、同時に通知する設計をアクションごとに定めます。

## フォーカス喪失

キーを押したままウィンドウが非アクティブになり、離したイベントを受け取れない場合があります。フォーカス喪失時に全ボタン状態を解放扱いへし、復帰時の誤移動を防ぎます。オンラインでは自動ポーズできない場合もあります。

## テキスト入力

プレイヤー名入力は物理キーコードを文字へ変換するだけでは不十分です。IME、合成中文字、クリップボード、Unicode、キーボード配列を扱うため、OS・ライブラリのテキスト入力イベントを使います。

## リプレイとAI

ゲームロジックへ`InputCommand`を渡す設計なら、人間入力、AI、ネットワーク、記録再生を同じ経路へ接続できます。

```cpp
struct PlayerCommand
{
    Vector2 move{};
    bool jumpPressed{};
    bool attackPressed{};
    bool dodgePressed{};
};
```

入力イベントそのものではなく、シミュレーションtickごとのコマンドを記録すると再現性を管理しやすくなります。

## アクセシビリティ

キー再割当、長押しをトグルへ変更、連打補助、同時押し緩和、スティック感度、振動強度、左右反転を設定可能にします。必須アクションの割当解除、重複割当、デバイス切断時の処理も必要です。
