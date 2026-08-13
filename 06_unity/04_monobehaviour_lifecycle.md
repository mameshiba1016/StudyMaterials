# 04 MonoBehaviourのLifecycleと実行順序

## 1. 代表的な順序

```text
Awake
  ↓ Objectが有効なら
OnEnable
  ↓ 最初のFrame前
Start
  ↓
FixedUpdate（0回以上）
Update
LateUpdate
  ↓ 描画
  ...
OnDisable
OnDestroy
```

Physicsの固定Step回数はRender Frameごとに一定ではありません。

## 2. Awake

自身の必須Component取得、内部Object生成、設定検証へ使います。他Objectの`Start`完了を仮定しません。

```csharp
private void Awake()
{
    combatState = new CombatStateMachine(config);
}
```

## 3. OnEnable／OnDisable

```csharp
private void OnEnable()
{
    health.Changed += HandleHealthChanged;
}

private void OnDisable()
{
    health.Changed -= HandleHealthChanged;
}
```

Event、Input Action、Manager登録を対にします。Object Poolでは何度も呼ばれるため二重登録を防ぎます。

## 4. Start

全有効ObjectのAwake後に最初のFrame前で呼ばれますが、Object間のStart順へ依存する設計は避けます。明示Initialize、Bootstrap、Service登録完了Eventを使います。

## 5. Update

入力、非PhysicsのState更新、Timer等。移動量には`Time.deltaTime`を使いますが、入力のPressed EventをFixedUpdateだけで拾うと取り逃す可能性があります。

## 6. FixedUpdate

Physics Stepに同期する処理へ使います。RigidbodyへForceを加える等が候補です。`Time.fixedDeltaTime`を使い、1 Render Frameに複数回／0回になり得ることを理解します。

## 7. LateUpdate

Character移動後にCameraを追従する等、通常Updateの結果を消費します。ただしScript間の呼出順をLateUpdateだけで完全解決せず、Camera System内で責任を集約します。

## 8. OnDestroy

Native Unity Objectが破棄される時の清掃です。Application終了時の全順序を無条件に信用せず、`OnDisable`で購読解除できる構造にします。

## 9. 実行順序依存

Script Execution Order設定や`DefaultExecutionOrder`は最後の手段です。多くの場合、明示的な呼出、Data依存、Bootstrapで解決できます。数値順序が増えると隠れたGlobal依存になります。

## 10. Coroutine

Coroutineは別Threadではなく、指定Yield条件まで処理を分割しPlayer Loop上で再開します。Owner無効化／破棄、Scene遷移、Cancel時の停止を設計します。

```csharp
private IEnumerator RecoveryRoutine()
{
    yield return new WaitForSeconds(recoverySeconds);
    stateMachine.FinishRecovery(actionSequence);
}
```

古いActionのCoroutineが新Actionを終了させないようSequence IDを確認します。

## 11. 高速戦闘の更新順

```text
Input収集 → Command Buffer
→ Combat State更新
→ Character Movement
→ Animation Parameter
→ Camera LateUpdate
→ Presentation
```

各MonoBehaviourの偶然のUpdate順でなく、Systemの明示的Pipelineとして構築します。
