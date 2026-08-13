# 47 高速3D戦闘アクションの総合Architecture

## 1. 全体の依存方向

```text
Enhanced Input / AIController
          ↓ Command
Combat / Ability System ← Action Definition / Data Asset
          ↓                         ↓
Movement・Animation・Targeting・Collision
          ↓ Hit Candidate
Damage / Defense / Attribute System
          ↓ Result Event
Presentation（Niagara・Audio・Camera・UI）

Party System → Possess・Character交代
Combat Director → 複数敵の攻撃権
Save / Asset Manager → 永続Data・非同期Load
Replication → 正式状態をServerから配布
```

上位層が下位の具体Assetへ直接依存しすぎず、Command、Result、Tag、Data Definitionを境界にします。

## 2. Module分割例

```text
GameCore        基本Tag、共通Data型、Interface
GameCombat      Action、Damage、Defense、Targeting
GameAI          AIController、BT Task、Combat Director
GamePresentation Camera、Cue解決、UI View Model
GameRuntime     GameMode、Party、Save、Asset管理
GameEditor      Data Validation、Editor Tool
```

これは唯一の正解ではありません。循環依存を避け、Runtime ModuleからEditor Moduleへ依存しないことが重要です。

## 3. Characterの構成

```text
AActionCharacter
├─ CharacterMovementComponent
├─ SkeletalMeshComponent
├─ CombatComponent または ASC
├─ Defense／Attribute Component
├─ TargetingComponent
├─ MotionWarpingComponent
└─ Camera Components（Player用構成の場合）
```

Character本体はComponentの組立てとFramework Lifecycleの仲介を担当し、全処理を巨大Classへ集めません。

## 4. 1回の攻撃の流れ

```text
1. Input ActionがAttack Commandを生成
2. Input BufferへTimestamp付きで格納
3. Combat SystemがState・Cost・Cancel Ruleを検証
4. Action Sequence IDを発行
5. Targetと安全なWarp Transformを決定
6. Montage／Ability Task開始
7. NotifyでHit Windowを開く
8. 前Frameから現在Frameの武器SocketをSweep
9. Team・無敵・多重Hitを検証
10. Damage Resultを確定
11. HP／Poise／Guardへ適用
12. Cue、Hit Stop、Camera、UIへResult通知
13. Montage終了・中断の全経路でWindowと参照を清掃
```

各段階へSequence IDを渡すと、古いNotify、非同期Load、Network応答を現在Actionへ誤適用しにくくなります。

## 5. 状態の唯一の所有者

| 状態 | 所有候補 |
|---|---|
| 現在Action・Cancel Window | Combat Component／ASC |
| Capsule移動・床・Velocity | CharacterMovement |
| 最終Pose | AnimInstance |
| HP／Stamina | Attribute／Health System |
| 現在Lock Target | Targeting System |
| 編成・交代Cooldown | Party System |
| 敵の同時攻撃権 | Combat Director |
| 画面表示値 | View Model（元Dataを購読） |

同じ状態を複数boolで複製せず、他SystemはEventまたは読取Snapshotを使います。

## 6. 交代の流れ

```text
Switch Command
  ↓ Party Systemが条件検証
新Character AssetがLoad済みか
  ↓ 安全な出現位置をSweep
旧ActionのCancel／退場Window
  ↓ 新CharacterをActivate
Server Possess / Actor Info更新
  ↓ Camera・HUD・Target引継ぎ
Support Action開始
  ↓ 旧Characterを待機状態へ
```

途中失敗に備え、PrepareとCommitを分けます。

## 7. AIの流れ

```text
Perception Event → Target Memory
  ↓ Blackboard
Behavior Tree／Utility選択
  ↓ Combat DirectorへToken要求
EQSで攻撃位置取得
  ↓ 共通Combat Command
CharacterのCombat Systemが最終検証
```

AIもPlayerと同じDamage・Cost・Cancel規則を使います。

## 8. Presentationの流れ

```text
Gameplay Result
  ↓ Gameplay Tag / Presentation Event
ResolverがCharacter・Attack・SurfaceからData選択
  ├─ Niagara
  ├─ Audio
  ├─ Camera Shake
  ├─ Controller Rumble
  └─ UI
```

演出が失敗・省略されてもGameplay結果は変わりません。

## 9. 非同期と寿命

すべての遅延結果で次を確認します。

- Ownerはまだ有効か。
- Worldは同じか。
- Request IDは最新か。
- Action Sequenceは現在と同じか。
- Targetはまだ有効・敵対・生存か。
- Character交代でAvatarが変わっていないか。

## 10. Frame Budget設計

- 入力・Player戦闘：毎Frameでも小さく保つ。
- Target候補収集：空間Query＋必要時更新。
- AI意思決定：Eventと時間分散。
- EQS：状態変化時／低頻度。
- VFX／Animation：LODとSignificance。
- Asset：戦闘前Preload。

設計段階から「何体×何回×何Frame」を見積もります。
