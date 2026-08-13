# 32 NavigationとEQS

## 1. Navigationの役割

Nav MeshはAI Agentが通行可能な領域を表し、Path Followingが経路に沿ってPawnを移動させます。

```text
目的地
  ↓ Nav Meshへ投影
Path Query
  ↓ Path Point列
Path Following
  ↓ CharacterMovementへ移動入力
Pawn移動
```

NavMeshBoundsVolume、Agent半径・高さ、段差、斜面設定がCharacter Capsuleと一致しているか確認します。

## 2. MoveTo

Behavior TreeのMove ToではAcceptable Radius、Goal半径、Partial Path、Moving Goal追従、Strafe等を設定します。

攻撃距離が200cmなのにAcceptable Radiusが5cmなら、AIは不必要にTargetへ密着します。Combat RangeとCapsule半径から到達条件を決めます。

## 3. MoveTo完了と失敗

失敗原因：

- Nav Mesh外の目的地。
- Dynamic障害物。
- Agent設定不一致。
- Target移動による再Path。
- 他AIによる混雑。
- Partial Path禁止。

失敗時に同じMoveToを毎Frame再発行せず、別位置検索、短い待機、Fallbackへ移ります。

## 4. EQSの構造

```text
Querier
  ↓ Context（Target、Self、Combat Area等）
Generatorが候補Point／Actorを生成
  ↓
TestでFilter・Score
  ↓
Best Itemまたは上位Random Item
```

Generatorは候補を作り、Testは距離、Path、Trace、Dot等で落とす／採点します。

## 5. 近接敵の攻撃位置Query

```text
Generator: Target周囲の円環Point
Filter:
  - Nav Mesh上
  - 壁内部でない
  - Playerへ到達可能
  - 他の予約Slotと近すぎない
Score:
  + 希望攻撃距離
  + 現在位置からPathが短い
  + Camera前方を避ける／難易度に応じる
```

最高点だけを全AIが選ぶと重なるため、Combat Directorの予約情報をContext／Testへ渡すか、上位候補から分散選択します。

## 6. 遠距離敵の位置Query

- TargetへのLine of Sightあり。
- 希望距離範囲。
- Playerから直接到達しにくい。
- 味方の射線を塞がない。
- Coverがある。

Trace Testを大量候補へ掛ける前に距離等の安いFilterで候補数を落とします。Test順序とCostを意識します。

## 7. Dynamic Parameter

敵種や難易度ごとに希望距離、検索半径をNamed ParameterとしてQueryへ渡すとAsset再利用ができます。Parameter名の不一致はData Validationと定数化で検出します。

## 8. 非同期結果と状態

EQS実行中にTarget死亡、AI Stun、Phase変更が起こり得ます。結果受領時にRequest ID、現在Target、Behavior状態を再検証します。古い結果をそのままBlackboardへ書きません。

## 9. Query頻度

- 毎Frame実行しない。
- 現在位置が無効、Targetが大きく移動、Slot喪失時に再Query。
- AIごとに実行Frameを分散。
- 遠距離AIは低頻度。
- 結果を一定時間維持するHysteresisを持つ。

## 10. Navigationと高速Action

Nav Meshは通常移動経路に使い、Dash／Jump Attack／Motion Warpの瞬間軌道をそのまま保証しません。Action開始前にCapsule Sweep、着地点、壁、段差を別途検証します。

## 11. Debug

- `show Navigation`でNav Mesh確認。
- AI DebuggerでPath FollowingとEQS Score。
- EQS Testing Pawnで候補・Filter・Scoreを可視化。
- Visual Loggerで移動失敗理由を記録。

## 参考

- [Environment Query System](https://dev.epicgames.com/documentation/unreal-engine/environment-query-system-in-unreal-engine)
- [EQS User Guide](https://dev.epicgames.com/documentation/en-us/unreal-engine/environment-query-system-user-guide-in-unreal-engine)
