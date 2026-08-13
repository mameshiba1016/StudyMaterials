# 49 Unreal Engine編・完成確認表

この表は暗記試験ではありません。説明できない項目へ戻り、最小の検証Projectまたは後続の`90_examples`で確認します。

## C++とEngine基盤

- [ ] 標準C++、UHT、UBTの処理順を説明できる。
- [ ] `UCLASS`、`UPROPERTY`、`UFUNCTION`、`GENERATED_BODY`の役割を説明できる。
- [ ] `UObject` GCと通常C++ RAIIを使い分けられる。
- [ ] Hard／Weak／Soft Referenceを寿命から選べる。
- [ ] ModuleのPublic／Private依存を判断できる。

## Gameplay Framework

- [ ] Actor、Component、Pawn、Character、Controllerの責任を分けられる。
- [ ] Constructor、Construction、Initialize、BeginPlay、EndPlayを使い分けられる。
- [ ] GameMode、GameState、PlayerState、GameInstance、Subsystemへ状態を配置できる。
- [ ] PossessとCharacter交代時の接続解除・再接続を設計できる。

## 入力・移動・Camera

- [ ] Enhanced InputのAction、Context、Trigger、Modifierを説明できる。
- [ ] 入力Buffer、先行入力、Cancel Windowを区別できる。
- [ ] CharacterMovementと直接Transform更新を競合させない。
- [ ] Root Motion、Root Motion Source、Custom Modeの選択理由を説明できる。
- [ ] Camera、Control Rotation、Character Facingを分離できる。
- [ ] Target Lockの収集、Filter、Score、維持、切替を実装できる。

## Collision・Animation・戦闘

- [ ] Object Type、Trace Channel、Responseの組合せを説明できる。
- [ ] Line Trace、Sweep、Overlapを用途で選べる。
- [ ] 前Frameと現在Frameを結ぶ武器Sweepを説明できる。
- [ ] AnimBPをGameplay状態のAuthorityにしない理由を説明できる。
- [ ] Montage、Section、Slot、Notify Stateを使い分けられる。
- [ ] Montage中断時にHitbox、無敵、Cancel Windowを必ず清掃できる。
- [ ] Combo Graph、Damage Result、Poise、Guard、ParryをData駆動化できる。
- [ ] Motion Warpingの安全位置、上限、古いTargetを検証できる。

## AI・GAS

- [ ] Perception、Memory、Blackboard、Behavior TreeのData流れを説明できる。
- [ ] BT TaskのInProgress、Abort、Delegate解除を実装できる。
- [ ] EQSのGenerator、Context、Filter、Scoreを設計できる。
- [ ] Combat DirectorでAttack TokenとPosition Slotを管理できる。
- [ ] ASC、Ability、Attribute、Effect、Task、Cueの責任を説明できる。
- [ ] GASのOwner ActorとAvatar Actorを交代時に更新できる。

## Data・演出・UI

- [ ] Gameplay結果とNiagara／Audio／Camera演出を分離できる。
- [ ] UIをEvent駆動で更新しDelegateを解除できる。
- [ ] Data AssetとRuntime Stateを分離できる。
- [ ] Soft Referenceを非同期Loadし、古いRequest結果を捨てられる。
- [ ] Save DataへPointerではなく安定IDを保存できる。
- [ ] Save Version Migrationと書込み失敗を扱える。

## Network・性能・出荷

- [ ] Authority、Ownership、Role、Relevancyを区別できる。
- [ ] Replicated Property、RepNotify、RPCを使い分けられる。
- [ ] Client入力をServerで検証し、予測拒否を戻せる。
- [ ] Game Thread／Render Thread／GPUのどこが律速か測れる。
- [ ] Unreal Insightsで独自Scopeを解析できる。
- [ ] 同期Load、GC、初回Effect等のHitchを特定できる。
- [ ] DevelopmentとShipping PackageをTarget環境で確認できる。
- [ ] Cook漏れとEditor-only依存を診断できる。

## 高速3D戦闘アクション統合

- [ ] 入力から攻撃、Hit、Damage、演出までSequence IDで追跡できる。
- [ ] 回避、Parry、交代、被弾が同Frameでも一貫した優先順位を持つ。
- [ ] Camera外の敵攻撃へ公平な予告を設計できる。
- [ ] 多数敵でもAI、Animation、VFXをBudget内へ抑えられる。
- [ ] Character、敵、攻撃、演出を追加しても既存Codeの分岐が爆発しない。
- [ ] 低fps、遅延、壁際、Target死亡、中断を含む失敗条件をTestできる。

## 次の段階

ノートを読めることと実装できることは別です。次はUnity編、DXライブラリ編、DirectX編を学びつつ、`90_examples`に各概念の小さな実行例を作り、最後に複数Systemを統合します。
