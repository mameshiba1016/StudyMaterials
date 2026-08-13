# 34 Gameplay Ability Systemの全体像

## 1. GASが解決する範囲

Gameplay Ability System（GAS）は、Ability、Cost、Cooldown、Attribute、Buff／Debuff、Tag、非同期処理、ネットワーク予測、演出通知を共通Frameworkで扱います。

```text
AbilitySystemComponent（ASC）
├─ Granted Gameplay Abilities
├─ Gameplay Tags
├─ Attribute Sets
├─ Active Gameplay Effects
└─ Gameplay Cues
```

GASは「攻撃Animationを自動で完成させる機能」ではありません。Collision、Targeting、Movement、Animation、Game固有RuleをGASのActivation／Effect／Taskへ接続します。

## 2. PluginとModule

Gameplay Ability System Pluginを有効化し、Module依存を追加します。

```csharp
PublicDependencyModuleNames.AddRange(new string[]
{
    "GameplayAbilities",
    "GameplayTags",
    "GameplayTasks"
});
```

## 3. 中心となる型

- `UAbilitySystemComponent`：Ability付与・発動、Tag、Attribute、Effectを管理。
- `UGameplayAbility`：攻撃、回避、被弾等の実行単位。
- `UAttributeSet`：Health、Attack、Stamina等の値。
- `UGameplayEffect`：Attribute／TagへInstant・Duration・Infiniteな変化を適用。
- `UAbilityTask`：Montage待機、Event待機等、Ability内の非同期処理。
- Gameplay Cue：VFX／SFX等のCosmetic表現。

## 4. Actor Info

ASCはOwner ActorとAvatar Actorを区別します。

- Owner Actor：ASCの論理的Owner。PlayerState等。
- Avatar Actor：現在Abilityを実行する身体。Character等。

```cpp
AbilitySystemComponent->InitAbilityActorInfo(OwnerActor, AvatarActor);
```

Character交代でPlayerStateにASCを置く場合、Ownerは同じでAvatarを新Characterへ更新します。Character固有ASCならOwner／Avatar共にCharacterとなる構成があります。

## 5. Abilityの一生

```text
AbilityをGiveAbility
  ↓ Activation要求
CanActivateAbility
  ↓ Cost / Cooldown / Tag検証
ActivateAbility
  ↓ CommitAbility
Ability Task・Effect・Montage
  ↓ 完了／Cancel
EndAbility
```

`EndAbility`を呼び忘れると、Abilityが実行中のまま残り、Block Tagや再発動を壊します。成功・失敗・中断の全経路で終了させます。

## 6. GASを使う判断

向く条件：

- 多数のAbility、Buff、Debuff、Cost、Cooldownがある。
- Tagで複雑な実行可否を統一したい。
- Multiplayer予測・複製が重要。
- Data駆動でAbilityを拡張したい。

小規模Single Playerで攻撃数も少ない場合、独自Combat Componentの方が理解・Debugしやすいこともあります。途中導入はData／状態移行Costが大きいため早期に評価します。

## 7. 独自Combat Systemとの境界

GAS採用例：

```text
Enhanced Input → Ability Activation
Gameplay Ability → Actionの進行
Gameplay Effect → Damage／Cost／Buff
Gameplay Tag → State／Block／Cancel
Ability Task → Montage／Event待機
既存Targeting／Trace → Target DataをAbilityへ返す
```

GASと独自Componentが両方HealthやCooldownを所有しないよう、唯一のAuthorityを決めます。

## 8. Predictionの基本姿勢

Client予測は入力応答を速くしますが、Serverが正式結果を決めます。予測可能なCost／Effectと、Serverだけが知るHit結果を分けます。Random、Target選択、Trace結果を無条件にClientへ信用させません。

## 参考

- [Gameplay Ability System](https://dev.epicgames.com/documentation/unreal-engine/gameplay-ability-system-for-unreal-engine)
- [Understanding GAS](https://dev.epicgames.com/documentation/en-us/unreal-engine/understanding-the-unreal-engine-gameplay-ability-system)
