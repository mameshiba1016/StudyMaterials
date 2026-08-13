# Blackboard・AI共有データ

Blackboardは知覚・Decision・Action間で共有する名前付きデータです。便利ですが、型なしGlobal変数置場にすると依存が見えなくなります。

## 型付きKey

```cpp
enum class BlackboardKey
{
    TargetEntity,
    LastKnownPosition,
    HasLineOfSight,
    AttackToken,
    HomePosition
};
```

Keyごとに期待型をSchemaとして定義します。文字列Keyの誤字や型不一致をLoad/Compile時に検出します。

## Entry Metadata

```cpp
struct BlackboardEntryMetadata
{
    BlackboardKey key{};
    BlackboardValueType type{};
    bool persistent{};
    bool replicated{};
};
```

値のSource、最終更新tick、Confidenceも持つとDebugしやすくなります。

## Scope

- Local：一体のAI。
- Squad：小隊共有Target、役割。
- Encounter：攻撃Token、Phase。
- Global：Game全体のAlarm等。

寿命と所有者を明確にし、Scene終了後の参照を残しません。

## Observer

Key変更時にBT再評価等を通知できます。Callback中にBlackboardを再変更すると再入・循環が起きるため、変更QueueまたはPhaseを設けます。

## Entity Handle

生Pointerでなく世代付きHandleを保存し、読取時に有効性を検査します。Target破棄時に関連KeyをまとめてClearします。

## Snapshot

非同期Decision Jobへ渡す場合、Mutable Blackboardを直接共有せずSnapshotを作ります。完了反映時にVersionを比較します。

## Derived Data

`TargetDistance`を保存するとTarget/位置更新で古くなります。安価な派生値はContext生成時に計算し、CacheするならDependencyとInvalidationを管理します。

## Debug

Key、型、値、更新時刻、Source、Observer、Scopeを一覧表示し、変更履歴を記録します。
