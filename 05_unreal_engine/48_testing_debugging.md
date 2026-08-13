# 48 デバッグ、Automation Test、障害診断

## 1. 問題を段階へ分類する

```text
Build/UHT → Asset/Data → Lifecycle → Gameplay State
→ Animation/Collision → Network → Presentation → Performance
```

見た目が出ない問題でも、そもそもHitが成立していないのか、Result後のCueだけ失敗したのかを分けます。

## 2. Log Category

```cpp
DECLARE_LOG_CATEGORY_EXTERN(LogGameCombat, Log, All);
DEFINE_LOG_CATEGORY(LogGameCombat);

UE_LOG(LogGameCombat, Verbose,
    TEXT("ActionStart Owner=%s Tag=%s Sequence=%d"),
    *GetNameSafe(GetOwner()),
    *ActionTag.ToString(),
    SequenceId);
```

`LogTemp`だけに集約せず、Combat、AI、Asset、Save等に分類します。Actor名、World、Role、Sequence ID、理由を含めます。

## 3. ensure、check、verify

- `check`：成立しないと続行不能なProgrammer Error。Shippingでの扱いも理解する。
- `ensure`：異常を報告しつつ可能なら継続。
- `verify`：式の評価が必要な検証。

User入力やNetwork Dataの通常失敗へ`check`を使いません。安全に拒否してLogを残します。

## 4. Debug Draw

Target候補、Trace、Hitbox、Guard角、Nav Path、Warp位置、Combat Slotを色分けします。Console Variableで機能別に切り替え、Shippingへ不要描画を残しません。

## 5. Automation Testの層

- Unitに近いTest：Damage式、Cancel Rule、Score、Migration。
- UObject／World Test：Componentの状態遷移。
- Functional Test：Map上で入力→攻撃→Hit→死亡。
- Network Test：Server／ClientのReplication。
- Performance Test：代表戦闘のFrame／Memory。

## 6. 純粋関数をTestしやすくする

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FDamageClampTest,
    "Game.Combat.Damage.Clamp",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDamageClampTest::RunTest(const FString& Parameters)
{
    const float Result = CalculateFinalDamage(MakeNegativeDamageCase());
    TestEqual(TEXT("Damage never becomes negative"), Result, 0.0f);
    return true;
}
```

WorldやSingletonへ依存しない計算を分離すると高速に多数Caseを試せます。

## 7. 戦闘状態遷移Test

```text
Idle + Attack → Startup
Startup + Dodge（Window閉）→ 拒否
Active + Hit → HitConfirmed
Recovery + Combo（Window開）→ 次Action
任意State + Stun → 中断・全Window閉
```

正常経路だけでなく中断と同時発生をTestします。

## 8. Network Test条件

- 0／50／150ms遅延。
- Packet Lossと並替え。
- Listen／Dedicated Server。
- Client途中参加。
- Character交代と同Frameの被弾。
- Prediction拒否とRollback。

## 9. Crash診断

Call Stack、最初のError、Crash Context、直前Log、Build Commit、Map、再現入力を保存します。nullになった場所だけでなく、参照を保持した所有者と破棄Eventを追います。

## 10. 再現性

Random Seed、Action Sequence、入力Timestamp、Target選択Score、Network条件を記録します。「時々起きる」を同じ条件で再生可能にします。

## 11. Data Validation

Montage Section、Notify Window、Tag、Soft Asset、Skeleton、Gameplay Effect、Combo Graph、Save VersionをCook前に検査します。Runtimeで初めてnullを発見する段階を減らします。

## 12. 完了条件

Bug修正は「一度動いた」ではなく、原因を説明でき、失敗CaseのTestを追加し、類似箇所を検索し、Package Buildでも確認して完了です。
