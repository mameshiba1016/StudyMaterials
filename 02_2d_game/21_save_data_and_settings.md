# セーブデータ・設定・バージョン移行

セーブは実行中オブジェクトのメモリをそのまま保存する処理ではありません。永続化する意味データをSchemaとして定義し、検証、原子的更新、Version移行を行います。

## 分類

- Settings：音量、解像度、キー設定、言語。
- Profile：解放状況、実績、累積記録。
- Save Slot：進行、所持品、現在地点。
- Run State：一回の挑戦中データ。
- Cache：失っても再生成可能。

寿命と復旧方法が違うためファイルを分けます。

## 永続モデル

```cpp
struct SaveDataV3
{
    int schemaVersion{3};
    std::string checkpointId{};
    std::vector<std::string> unlockedAbilityIds{};
    int currency{};
    std::uint64_t playTimeSeconds{};
};
```

Entityポインタ、Texture Handle、関数、現在のAnimation内部参照を保存しません。安定IDと意味値へ変換します。

## 読込検証

- ファイルサイズ上限。
- Versionが対応範囲か。
- 必須Field。
- 数値範囲。
- 配列要素数上限。
- IDがMaster Dataに存在するか。
- 重複・矛盾。
- UTF-8等の妥当性。

Parse成功とゲーム上有効は別です。

## Version移行

```text
V1 → MigrateToV2 → MigrateToV3 → 現行Model
```

古いVersionから一段ずつ変換するとテストしやすくなります。未知の未来Versionを古いゲームで上書きしません。移行前にBackupを保持します。

## 安全な書込

```text
1. Save ModelのSnapshotを作る
2. 一時ファイルへSerialize
3. Flush/Closeとエラー確認
4. 必要なら再読込検証
5. 旧SaveをBackup
6. 一時ファイルを正式名へ置換
```

同じファイルへ直接上書き中に電源断すると両方失います。OS・ファイルシステムの原子的置換APIを利用します。

## 非同期Save

Game Worldをワーカースレッドから直接読むとデータ競合します。Game Threadで不変Snapshotを作り、Serialize/I/Oだけワーカーへ渡します。終了時は未完了Saveを待つか、完了状態をUIへ示します。

## Auto Save

画面へ保存中表示を出し、短時間連続要求をまとめます。Boss戦中などI/OによるStutterを避ける時点を選びます。Checkpoint到達とSave成功を区別し、失敗時に通知・再試行します。

## Settings適用

Settingsは変更直後にPreviewし、Apply/Cancelを扱います。解像度変更は一定時間で確認されなければ元へ戻します。キー割当は必須操作を失わない検証が必要です。

## IntegrityとSecurity

Checksumは偶発破損を検出できますが改ざん防止ではありません。暗号化してもClientが鍵を持つため完全防止は困難です。競争・課金に関わる権威データはServer側で検証します。秘密情報をSaveへ置きません。

## Cloud Save

LocalとCloudの更新時刻だけでは時計ずれがあります。Revision、Device ID、内容Summaryを比較し、競合時はユーザーへ選択肢を示します。自動MergeできるFieldとできない進行を分けます。

## テスト

- 全旧VersionのFixture読込。
- 空・途中切断・巨大・改ざんデータ。
- 書込失敗、容量不足。
- 連続Save、終了中Save。
- 言語・Path・権限差。
- Round Trip後に意味が一致するか。
