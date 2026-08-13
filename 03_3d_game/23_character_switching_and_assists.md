# Character交代・支援Action

複数Character戦闘では、入力対象を入れ替えるだけでなく、所有権、状態、位置、Camera、Target、Resourceを一つの遷移として扱います。

## PartyとControlled Character

```cpp
struct PartyState
{
    std::vector<EntityHandle> members{};
    std::size_t controlledIndex{};
    EntityHandle lockTarget{};
};
```

PartyがMemberを必ず所有するとは限りません。World所有Entityへの世代付きHandleを保持し、破棄・死亡を検査します。

## 交代Request

```cpp
struct SwitchRequest
{
    EntityHandle from{};
    EntityHandle to{};
    SwitchReason reason{};
    int requestedTick{};
};
```

入力時に即Pointerを書き換えず、Combat Phaseの安全地点で検証・実行します。

## 検証

- 交代先が生存・ロード済み。
- Cooldown/Resource。
- 現状態が交代可能。
- 同tickに別Requestがない。
- Spawn位置がNav/Collision上で有効。

## Atomic Transition

```text
旧Character入力停止
→ 旧Character退場Action
→ 新Character位置/Facing決定
→ Collision安全性検査
→ 新Character登場Action・無敵
→ Camera Target/Input所有更新
→ Lock Target継承
```

途中失敗時に半分だけ交代しないよう、必要情報を先に準備します。

## Position

新Characterを旧Character位置へ瞬間配置する場合、壁・敵Colliderとの重なりをOverlap/Capsule Castで解消します。支援攻撃ならTargetとの相対位置を候補生成し、安全な点を選びます。

## Off-field状態

非操作Characterを非表示・非Collisionにするか、World内AIとして残すかで設計が変わります。Timer、Cooldown、Buff、EnergyがOff-field中も進むかを個別に定義します。

## 支援Action

支援者を一時EntityとしてSpawnし、Action終了後に退場させます。操作CharacterのStateを奪わず、同時Hit解決とCamera Framingへ参加します。

## Defensive Assist

Incoming Attackに対するWindowで交代入力を受け、Parry/Dodge Resultへ変換します。通常交代と同じ入力でも、攻撃Contextにより優先します。攻撃一回につき成功一度をAttack Instanceで記録します。

## Resource

支援Point等はRequest時、Action開始時、成功時のどこで消費するかを統一します。失敗Requestで消費しないTransactional処理にします。

## Camera

Camera Followを新Characterへ瞬間切替せず、Combat状況に応じBlendします。ただしParry等は瞬間的なFraming Eventを優先できます。Target Lock Handleは有効なら継承します。

## Animation/VFX同期

旧退場、新登場、支援攻撃を共通Timeline Eventで同期します。Animation Eventだけを交代完了の権威にせず、Gameplay tickを基準にします。

## Debug

現在操作者、交代可能性、拒否理由、Cooldown、Spawn候補、入力Owner、Camera Blend、支援Instanceを表示します。
