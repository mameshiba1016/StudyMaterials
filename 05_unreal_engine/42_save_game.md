# 42 SaveGame、バージョン管理、非同期保存

## 1. SaveGameはWorldのSnapshotを自動保存しない

`USaveGame`派生へ保存したいDataを明示的にコピーし、Slotへ書き出します。

```cpp
UCLASS()
class UMySaveGame : public USaveGame
{
    GENERATED_BODY()

public:
    UPROPERTY(SaveGame)
    int32 SaveVersion = 1;

    UPROPERTY(SaveGame)
    FString CurrentStageId;

    UPROPERTY(SaveGame)
    TArray<FName> UnlockedCharacterIds;

    UPROPERTY(SaveGame)
    FPlayerSettingsData Settings;
};
```

Actor Pointer、Component Pointer、Timer、Montage Instanceを保存せず、再構築可能なIDと値を保存します。

## 2. Runtime→Save Data

```text
Runtime Model
  ↓ Validate / Normalize
SaveGame DTO
  ↓ Serialize
Slot File
```

SaveGame ObjectをRuntimeの唯一状態として直接書き換え続けるより、保存時に一貫したSnapshotを構築します。

## 3. 非同期保存

```cpp
FAsyncSaveGameToSlotDelegate Delegate;
Delegate.BindUObject(this, &USaveSubsystem::HandleSaveCompleted, RequestId);

UGameplayStatics::AsyncSaveGameToSlot(
    SaveObject,
    SlotName,
    UserIndex,
    Delegate);
```

非同期保存はFrame Hitchを避けるため有効です。保存中に次の保存要求が来た場合、Queue、最新Snapshotで置換、順次実行などの規則を決めます。

## 4. 書込み中のData変更

非同期処理へ渡したSave Objectを同時に変更しないよう、要求ごとにSnapshot Objectを作ります。完了CallbackではRequest IDとSlotを確認します。

## 5. 非同期Load

Load完了時にClass Cast、Version、Data範囲を検証します。Fileがない場合はDefaultを作り、破損時のFallbackとUser通知を設計します。

```cpp
void USaveSubsystem::HandleLoadCompleted(
    const FString& LoadedSlot,
    int32 LoadedUserIndex,
    USaveGame* LoadedObject)
{
    UMySaveGame* Save = Cast<UMySaveGame>(LoadedObject);
    if (!Save || !ValidateSave(*Save))
    {
        LoadDefaults();
        return;
    }

    MigrateToLatestVersion(*Save);
    ApplySave(*Save);
}
```

## 6. Version Migration

```cpp
switch (Save.SaveVersion)
{
case 1:
    MigrateV1ToV2(Save);
    [[fallthrough]];
case 2:
    MigrateV2ToV3(Save);
    break;
default:
    break;
}
```

古いVersionから段階的に最新へ移します。Enumの数値変更、Asset Rename、削除Character ID、数値範囲変更を考慮します。

## 7. Slot設計

- Profile／進行Data。
- User設定。
- Checkpoint／Run Data。
- Auto Save Backup。

更新頻度と重要度の違うDataを分けると、設定変更ごとに巨大進行Dataを書かずに済みます。

## 8. 安全性

- 書込み失敗を成功扱いしない。
- 保存中にApplication終了要求が来る場合を考慮。
- Backup／世代管理を検討。
- Cloud Save競合にTimestampだけでなくRevisionを使う。
- Clientから送られた競争Dataを信頼しない。
- 個人情報や秘密情報を平文Saveへ置かない。

## 9. Character／戦闘Data

保存する：Character ID、Level、装備ID、解放Ability、設定、Stage進行。

通常保存しない：現在Montage位置、Hitbox Window、Target Actor Pointer、Delegate、ASCの内部Handle。必要ならCheckpoint用の安定した高水準状態へ変換します。

## 10. テスト

- Slotなし、空Data、破損Data。
- 旧VersionからMigration。
- 保存中の連続要求。
- Character AssetがRename／削除済み。
- Disk容量不足・書込み失敗。
- Level遷移中にLoad完了。
- Default値からSave→LoadのRound Trip。

## 参考

- [Saving and Loading Your Game](https://dev.epicgames.com/documentation/en-us/unreal-engine/saving-and-loading-your-game-in-unreal-engine)
