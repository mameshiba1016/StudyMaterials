# DXライブラリ：Save・Settings・File I/O

この章では、ゲームの進行・設定を安全に永続化し、AssetやDataを確実に読み込む設計を学びます。保存処理は「構造体をファイルへ書けば完了」ではありません。書込み途中の終了、容量不足、破損、旧Version、Pathの違い、権限不足、外部改変から復旧できて初めて実用的です。

> DXライブラリの `FileRead_*` は主に読込み用です。Save書込みにはC++標準File I/OやOS APIを使い、Asset読込みとUser Data書込みの責任を分けます。

## 1. 永続化するDataの分類

```text
Config
  Graphics、Audio、Language、Input、Accessibility

Progress
  Stage進行、解放状態、所持品、成長状態

Runtime checkpoint
  現在位置、HP、Encounter状態、Random状態

Cache
  再生成可能なShader Cache、Thumbnail、解析結果

Log/Telemetry
  診断情報、Crash直前の状態
```

重要度・更新頻度・復旧方法が違うDataを一つのFileへ詰め込まないようにします。

## 2. AssetとUser Dataの違い

- Assetは開発者が用意し、通常は読取り専用である。
- User Dataは実行中に生成され、書込み可能な専用Folderへ置く。
- Install先は権限や更新処理の都合で書込み可能とは限らない。
- Working Directoryは起動方法によって変化し得る。
- Cacheは消えても再生成できなければならない。

相対Pathを無条件で現在Directory基準にせず、Asset RootとUser Data Rootを起動時に確定します。

## 3. Pathを文字列連結しない

```cpp
#include <filesystem>

namespace fs = std::filesystem;

fs::path BuildSavePath(const fs::path& userDataRoot, std::string_view slotId)
{
    // 実運用ではslotIdを許可文字だけに検証してからPathへ使う。
    return userDataRoot / "Save" / (std::string(slotId) + ".json");
}
```

`/` 演算子なら区切り文字をPlatformに合わせられます。入力値をそのままPathへ足すと `..` や絶対PathによるPath Traversalが起きるため、Save Slot IDは列挙値や英数字IDへ制限します。

## 4. Path Traversal対策

```cpp
bool IsSafeSlotId(std::string_view id)
{
    if (id.empty() || id.size() > 32) return false;

    return std::ranges::all_of(id, [](unsigned char character)
    {
        return std::isalnum(character) || character == '_' || character == '-';
    });
}
```

Canonical PathによるRoot内判定も加えます。ただし存在しないPathに対するcanonical化やSymbolic Linkの扱いは慎重に設計します。

## 5. Errorを戻り値で表現する

```cpp
enum class FileError
{
    None,
    NotFound,
    PermissionDenied,
    InvalidPath,
    TooLarge,
    OpenFailed,
    ReadFailed,
    WriteFailed,
    FlushFailed,
    ReplaceFailed,
    Corrupt,
    UnsupportedVersion
};

template<class T>
struct FileResult final
{
    std::optional<T> value;
    FileError error{FileError::None};
    std::string detail;

    [[nodiscard]] bool HasValue() const noexcept { return value.has_value(); }
};
```

`bool` だけではUI表示、Log、Retry、Fallbackの判断材料が不足します。

## 6. DXライブラリのFile Handle

`FileRead_open` は成功時に0以外のHandle、失敗時に0を返します。使い終えたら `FileRead_close` が必要です。

```cpp
class UniqueDxFile final
{
public:
    explicit UniqueDxFile(int handle = 0) noexcept : handle_(handle) {}
    ~UniqueDxFile() { Reset(); }

    UniqueDxFile(const UniqueDxFile&) = delete;
    UniqueDxFile& operator=(const UniqueDxFile&) = delete;

    UniqueDxFile(UniqueDxFile&& other) noexcept
        : handle_(std::exchange(other.handle_, 0)) {}

    UniqueDxFile& operator=(UniqueDxFile&& other) noexcept
    {
        if (this != &other)
        {
            Reset();
            handle_ = std::exchange(other.handle_, 0);
        }
        return *this;
    }

    void Reset() noexcept
    {
        if (handle_ != 0)
        {
            FileRead_close(handle_);
            handle_ = 0;
        }
    }

    [[nodiscard]] int Get() const noexcept { return handle_; }
    [[nodiscard]] bool IsValid() const noexcept { return handle_ != 0; }

private:
    int handle_{0};
};
```

## 7. DX Fileを全読込みする

Asset FileはDX Archive内にあっても `FileRead_*` で読めます。

```cpp
FileResult<std::vector<std::byte>> ReadDxFile(
    const char* path,
    std::size_t maximumBytes)
{
    const LONGLONG signedSize = FileRead_size(path);
    if (signedSize < 0)
        return {{}, FileError::NotFound, path};

    const auto size = static_cast<std::uint64_t>(signedSize);
    if (size > maximumBytes || size > static_cast<std::uint64_t>(INT_MAX))
        return {{}, FileError::TooLarge, path};

    UniqueDxFile file{FileRead_open(path, FALSE)};
    if (!file.IsValid())
        return {{}, FileError::OpenFailed, path};

    std::vector<std::byte> bytes(static_cast<std::size_t>(size));
    if (size > 0)
    {
        const int readResult = FileRead_read(
            bytes.data(), static_cast<int>(bytes.size()), file.Get());

        // 使用版の公式Headerで戻り値の意味を確認し、短い読込みも失敗にする。
        if (readResult < 0 || static_cast<std::size_t>(readResult) != bytes.size())
            return {{}, FileError::ReadFailed, path};
    }

    return {std::move(bytes), FileError::None, {}};
}
```

File Sizeを信頼して無制限Allocationしないことが重要です。

## 8. SeekとTell

`FileRead_tell` は現在位置をbyte単位で返し、`FileRead_seek` は `SEEK_SET`、`SEEK_CUR`、`SEEK_END` から移動します。

```cpp
if (FileRead_seek(fileHandle, headerOffset, SEEK_SET) == -1)
{
    // File formatが要求する位置へ移動できなかった。
}

const LONGLONG currentPosition = FileRead_tell(fileHandle);
if (currentPosition < 0)
{
    // 現在位置取得失敗。
}
```

Offset加算のOverflow、File末尾超過、負の位置を検査します。

## 9. 非同期File Open

非同期読込みではOpen完了前にread・seek・closeしてはいけません。`CheckHandleASyncLoad` が処理中を示す間はHandleを操作しません。

```cpp
const int handle = FileRead_open("Data/LargeAsset.bin", TRUE);
if (handle == 0)
{
    // Open要求自体に失敗。
}

// 実ゲームでは毎Frame Pollし、Main LoopをBlockしない。
if (CheckHandleASyncLoad(handle) == FALSE)
{
    // 完了後にreadまたはcloseできる。
}
```

終了要求時も非同期Jobの完了・Cancel仕様を確認し、安全な時点でcloseします。

## 10. DX Archive

DX ArchiveはAsset群をArchive Fileへまとめ、Folderのように読める仕組みです。

- `SetUseDXArchiveFlag` で利用有無を設定する。
- `SetDXArchiveExtension` で検索Extensionを変更できる。
- `SetDXArchiveKeyString` の鍵は強固な秘密保護とは考えない。
- Archiveがなくても通常FileへFallbackする構成が可能。

Save DataをDX Archiveへ書く仕組みではありません。Asset PackagingとUser Saveを分離します。

## 11. Text FormatとBinary Format

| 観点 | Text | Binary |
|---|---|---|
| 人が読める | 得意 | 苦手 |
| Diff | 得意 | 苦手 |
| Size | 大きめ | 小さくできる |
| Parse Cost | 高め | 低くできる |
| Version変更 | Key方式なら柔軟 | 明示設計が必要 |
| 精度 | Format次第 | 型を保持しやすい |

SettingsはJSON等のText、頻繁で大規模なDataはBinaryなど、用途で選びます。

## 12. 生の構造体を書いてはいけない理由

```cpp
struct PlayerSave
{
    int level;
    bool unlocked;
    std::string name;
};

// 禁止例：write(reinterpret_cast<const char*>(&save), sizeof(save));
```

Padding、Endian、Compiler ABI、Pointer、`std::string` の内部表現が含まれ、別Buildで読めません。Fieldごとに明示的なFormatへ変換します。

## 13. Save Schema

```cpp
struct SaveData final
{
    std::uint32_t schemaVersion{3};
    std::string buildVersion;
    std::string slotId;
    std::int64_t playTimeSeconds{};
    std::string currentStageId;
    std::vector<std::string> unlockedStageIds;
};
```

Runtime ObjectのPointerやHandleを保存せず、安定したIDと値を保存します。

## 14. Header設計

Binary Saveには内容を識別するHeaderを置きます。

```cpp
struct SaveHeader final
{
    std::array<char, 4> magic{'S', 'A', 'V', 'E'};
    std::uint32_t formatVersion{3};
    std::uint32_t payloadSize{};
    std::uint32_t checksum{};
};
```

- Magicで別Fileを誤読しない。
- VersionでMigrationを選ぶ。
- Payload Sizeで短いFileや異常値を検出する。
- Checksumで偶発的破損を検出する。

`sizeof(Header)` の生書込みではなく、EndianとField幅を決めて個別Encodeします。

## 15. Endianness

Multi-byte整数はByte順を明示します。

```cpp
void WriteUint32LittleEndian(std::vector<std::byte>& output, std::uint32_t value)
{
    output.push_back(static_cast<std::byte>( value        & 0xFFu));
    output.push_back(static_cast<std::byte>((value >> 8)  & 0xFFu));
    output.push_back(static_cast<std::byte>((value >> 16) & 0xFFu));
    output.push_back(static_cast<std::byte>((value >> 24) & 0xFFu));
}
```

Reader側は残りbyte数を確認してから読みます。

## 16. Bounds Check付きReader

```cpp
class ByteReader final
{
public:
    explicit ByteReader(std::span<const std::byte> bytes) : bytes_(bytes) {}

    std::optional<std::uint32_t> ReadUint32LE()
    {
        if (Remaining() < 4) return std::nullopt;

        const auto b0 = std::to_integer<std::uint32_t>(bytes_[position_ + 0]);
        const auto b1 = std::to_integer<std::uint32_t>(bytes_[position_ + 1]);
        const auto b2 = std::to_integer<std::uint32_t>(bytes_[position_ + 2]);
        const auto b3 = std::to_integer<std::uint32_t>(bytes_[position_ + 3]);
        position_ += 4;
        return b0 | (b1 << 8) | (b2 << 16) | (b3 << 24);
    }

    [[nodiscard]] std::size_t Remaining() const noexcept
    {
        return bytes_.size() - position_;
    }

private:
    std::span<const std::byte> bytes_;
    std::size_t position_{};
};
```

破損Fileに対して範囲外Accessしないことが最優先です。

## 17. StringのSerialize

```text
[uint32 byteLength][UTF-8 bytes]
```

Readerは次を検査します。

- Lengthが上限以下か。
- 残りPayload内に収まるか。
- UTF-8として妥当か。
- Null byte等を許可するか。
- Display側の最大文字数を超えないか。

File内Lengthを信じて巨大Memoryを確保してはいけません。

## 18. Version Migration

古いSaveを現在Schemaへ段階的に変換します。

```text
V1 -> V2: difficulty fieldを追加
V2 -> V3: stage numberをstable stage IDへ変換
V3: current
```

```cpp
FileResult<SaveDataV3> MigrateToCurrent(const ParsedSave& source)
{
    switch (source.version)
    {
    case 1:
        return ConvertV2ToV3(ConvertV1ToV2(source.v1));
    case 2:
        return ConvertV2ToV3(source.v2);
    case 3:
        return ValidateV3(source.v3);
    default:
        return {{}, FileError::UnsupportedVersion, "Unknown save version"};
    }
}
```

Migration後すぐ上書きせず、読込み成功・Validation成功を確認します。

## 19. Forward Compatibility

新VersionのSaveを古いBuildで開くと、知らないFieldを失う可能性があります。基本は新Versionを拒否し、Fileを変更しません。「読めないからDefaultで上書き」は最悪の挙動です。

## 20. Validation

Parse成功はDataが妥当という意味ではありません。

```cpp
bool Validate(const GraphicsSettings& settings)
{
    if (settings.width < 640 || settings.width > 7680) return false;
    if (settings.height < 360 || settings.height > 4320) return false;
    if (settings.frameRateLimit < 30 || settings.frameRateLimit > 1000) return false;
    if (!std::isfinite(settings.uiScale)) return false;
    if (settings.uiScale < 0.5f || settings.uiScale > 2.0f) return false;
    return true;
}
```

NaNは比較をすり抜けるため `isfinite` を先に確認します。

## 21. Atomic Saveの考え方

本Fileへ直接上書きすると、途中終了で旧Dataも新Dataも失います。

```text
1. 同じDirectoryのtemporary Fileへ全Dataを書く
2. Stream errorを確認する
3. Flushする
4. 必要ならOSへ永続化を要求する
5. 旧FileをBackupへ保持する
6. temporaryを本FileへAtomic replaceする
7. Directory metadataの永続化もPlatform別に検討する
```

`std::filesystem::rename` の上書き挙動やAtomic性はPlatform・File Systemで異なります。Windowsでは専用Replace APIを抽象層で扱う設計も検討します。

## 22. Temporary File名

```cpp
fs::path MakeTemporaryPath(const fs::path& finalPath)
{
    fs::path temporary = finalPath;
    temporary += ".tmp";
    return temporary;
}
```

複数ProcessやThreadから同時保存する可能性があるなら、固定 `.tmp` では衝突します。Process ID、Counter、Random IDを含め、作成時に排他的に確保します。

## 23. Backup Rotation

```text
slot.sav      current
slot.sav.bak1 previous
slot.sav.bak2 older
```

新しいSaveのValidationに成功してから世代を回します。壊れたCurrentをBackupへ複製しないよう、Load時にもChecksumとSchemaを確認します。

## 24. Load Recovery順

```text
Currentを読む
  -> 成功: 使用
  -> 失敗: Backup 1
      -> 成功: 復旧を通知し使用
      -> 失敗: Backup 2
          -> 成功: 復旧を通知し使用
          -> 失敗: 新規Dataを提案するが破損Fileは勝手に消さない
```

どのFileから復旧したかをLogとUIへ示します。

## 25. ChecksumとHash

CRC等は偶発的破損検出に適しますが、攻撃者による改変防止にはなりません。改ざん検知が必要なら秘密鍵を使うMAC等が必要ですが、Client内の秘密は解析可能である点を理解します。

暗号化は破損対策、認証、Cheat対策の代わりではありません。目的を分けます。

## 26. Compression

圧縮は容量とI/Oを減らせますが、CPU負荷、破損時の影響範囲、最大展開Sizeという危険を増やします。

- Headerに非圧縮Sizeと圧縮Sizeを持つ。
- 両方へ上限を設ける。
- 圧縮率が異常なDataを拒否する。
- 展開後にChecksumを検証する。

小さなSettings Fileには不要な場合が多いです。

## 27. Settingsの分類

```cpp
struct AudioSettings final
{
    float master{1.0f};
    float music{0.8f};
    float soundEffect{0.8f};
    float voice{0.8f};
    bool muteWhenUnfocused{};
};

struct AccessibilitySettings final
{
    float uiScale{1.0f};
    bool reduceFlashing{};
    bool reduceCameraShake{};
    bool holdToToggle{};
    int colorVisionPreset{};
};
```

Gameplay Saveとは別Fileにして、進行Data破損時も設定を維持できるようにします。

## 28. Default・Loaded・Applied

Settingsには三つの段階があります。

```text
Default: CodeまたはDataで定義した安全値
Loaded : Fileから読んでValidation済みの値
Applied: Audio、Window、Renderer等へ実際に反映済みの値
```

反映に失敗した場合、Loaded値をそのまま保存し直さず、安全なApplied値へ戻します。

## 29. Graphics Settingsの確認Dialog

解像度やFull Screen変更で画面が見えなくなる可能性があります。

```text
旧設定を保持
 -> 新設定を仮適用
 -> 「この設定を維持しますか？」15秒表示
 -> Confirmなら保存
 -> Timeout/Cancelなら旧設定へ戻す
```

保存は適用成功とUser Confirmの後に行います。

## 30. Audio Settingsの適用

保存値は0～1へClampし、MasterとCategoryを掛けます。

```cpp
float EffectiveVolume(float master, float category)
{
    return std::clamp(master, 0.0f, 1.0f) *
           std::clamp(category, 0.0f, 1.0f);
}
```

Slider操作中はPreview適用し、Cancel時に元値へ戻します。

## 31. Input Bindingの保存

物理Key Codeだけでなく、Action ID、Device Type、Binding Type、修飾Key、Dead Zone等を保存します。

```cpp
struct SerializedBinding final
{
    std::string actionId;
    std::string deviceType;
    std::string controlPath;
    float deadZone{};
    bool inverted{};
};
```

Deviceが存在しなくても設定を消さず、Default Bindingを一時Fallbackにします。

## 32. Save Snapshot

Gameplay Objectを保存Threadから直接読むとData Raceや中途半端な状態が生じます。Main Threadの安全な時点でPlain Data Snapshotを作ります。

```cpp
struct SaveSnapshot final
{
    std::string stageId;
    VECTOR playerPosition{};
    int playerHealth{};
    std::vector<std::string> unlockedIds;
};

// Snapshot生成後、WorkerはGameplay Objectへ触れずSerializeとFile I/Oだけ行う。
```

## 33. 非同期Save Pipeline

```text
Main Thread
  -> Save要求をCoalesce
  -> 整合したSnapshotを作る
Worker Thread
  -> Serialize
  -> TemporaryへWrite/Flush
  -> Replace
Main Thread
  -> Completion Eventを受けUI更新
```

同じSlotへのSaveを同時実行せず、世代番号で古いJobが新しい結果を上書きしないようにします。

## 34. Save中の終了

終了要求時に選択肢があります。

- 短い猶予内でSave完了を待つ。
- 終了を一時保留し「保存中」を表示する。
- OS強制終了に備えてAtomic Saveで旧Fileを守る。

長時間無限に待たず、Timeoutと診断Logを持ちます。

## 35. Auto Save

Auto Saveは安全なGame Stateで実行します。

- Stage開始・終了。
- Checkpoint到達。
- Inventory確定後。
- 設定変更確定後。

攻撃判定更新中やScene破棄途中のSnapshotは避けます。連続EventはDebounceし、Storageへの過剰書込みを防ぎます。

## 36. Save Slot Metadata

Slot一覧表示のため、全Saveを完全Parseせずに済むMetadataを用意します。

```cpp
struct SaveSlotMetadata final
{
    std::string slotId;
    std::string stageDisplayKey;
    std::int64_t playTimeSeconds{};
    std::int64_t savedAtUnixSeconds{};
    std::string screenshotPath;
    std::uint32_t formatVersion{};
};
```

Thumbnailは別Fileにし、欠落してもSave本体を読めるようにします。

## 37. Clockへの注意

System ClockはUser変更や時刻同期で逆行できます。表示時刻にはWall Clock、処理間隔にはMonotonic Clockを使います。Slotの新旧判定を時刻だけへ依存せず、内部Sequence Numberも保存します。

## 38. Cloud Save競合

将来Cloud対応するなら単純な「新しい時刻が勝ち」ではData損失が起きます。

- Device ID、Revision、Parent Revisionを持つ。
- LocalとRemoteが同じParentから分岐したら競合扱いにする。
- Play Time、進行位置、保存時刻をUserへ示して選択させる。
- 選ばれなかった側をすぐ削除しない。

Inventoryの単純Mergeは複製Bugを生むため、Domainごとの規則が必要です。

## 39. File Lockと多重起動

二つのProcessが同じSlotへ書くと破損・巻戻りが起きます。Application単位Lock、Slot単位Lock、Revision比較を使います。Lock FileがCrash後に残る場合に備え、Process存在確認や期限を設計します。

## 40. Directory作成

```cpp
std::error_code error;
fs::create_directories(savePath.parent_path(), error);
if (error)
{
    return {{}, FileError::PermissionDenied, error.message()};
}
```

例外版と `error_code` 版を混ぜず、Error方針を統一します。作成後も対象がDirectoryか、Symbolic Linkで意図しない場所を指していないかを脅威Modelに応じて確認します。

## 41. C++ Streamの検査

```cpp
std::ofstream stream(temporaryPath, std::ios::binary | std::ios::trunc);
if (!stream.is_open())
    return FileError::OpenFailed;

stream.write(reinterpret_cast<const char*>(bytes.data()),
             static_cast<std::streamsize>(bytes.size()));
if (!stream)
    return FileError::WriteFailed;

stream.flush();
if (!stream)
    return FileError::FlushFailed;

stream.close();
if (!stream)
    return FileError::WriteFailed;
```

`write` を呼べたこととStorageへ永続化されたことは同じではありません。高い耐久性が必要ならPlatform別fsync相当を抽象化します。

## 42. TOCTOU

「存在確認してから開く」の間にFile状態が変わる問題をTime-of-check to time-of-useと呼びます。重要操作では、存在確認結果へ依存せず排他的OpenやAtomic Replaceの結果を直接扱います。

## 43. Security境界

Save、Config、Mod Data、Networkから得たFileは信用しません。

- Size上限。
- Collection要素数上限。
- String長上限。
- 再帰Depth上限。
- 整数Overflow検査。
- NaN/Infinity拒否。
- Path Root制限。
- Parser Timeoutや圧縮Bomb対策。

Parse失敗はCrashではなく診断可能なErrorにします。

## 44. LoggingとPrivacy

LogにはOperation、Slot ID、Error Code、Size、Version、所要時間を残します。ただしUser名を含む絶対Path、Token、個人情報、Save本文を無条件で記録しません。

```text
SaveFailed slot=auto1 error=FlushFailed bytes=4218 version=3 elapsed_ms=18
```

## 45. File I/O計測

- Read・Serialize・Write・Flush・Replaceの各時間。
- File Sizeと圧縮率。
- Main Thread Block時間。
- Auto Save要求数とCoalesce数。
- Recovery発生数。
- Migration所要時間。
- Cache Hit率。

平均だけでなく遅いStorageでの最大時間を計測します。

## 46. よくある不具合：Saveが消える

- 本Fileへ直接上書きした。
- Replace前にTemporaryを検証していない。
- 新Versionを旧BuildがDefaultで上書きした。
- Working Directoryへ保存していた。
- 複数Jobが順不同で同じSlotへ書いた。
- Cloud競合を時刻だけで解決した。

## 47. よくある不具合：一部設定だけ戻る

- Default、Loaded、Appliedを混同した。
- 反映失敗値を保存した。
- Settingsの一部だけ別File・Registryへ残っている。
- Save完了前にUIへ成功表示した。
- Locale依存の小数表現をParseした。

Settings全体をTransactionとしてApply・Confirm・Saveします。

## 48. よくある不具合：Assetが読めない

- Working Directoryが違う。
- 大文字・小文字の差を見逃した。
- `\` と `/`、文字コードが環境に合わない。
- DX Archive設定・Extension・Keyが不一致。
- 非同期Open完了前にreadした。
- Handleの失敗値0を有効扱いした。
- File Sizeを `int` へ狭めてOverflowした。

Resolved Path、Archive利用状態、Size、ErrorをLogへ出します。

## 49. Unit Test

- 空File、1byte、最大許容Size。
- Header途中、Payload途中で切れたFile。
- Magic違い、Checksum違い、未知Version。
- String Length・配列Countが異常に大きいFile。
- NaN、Infinity、範囲外設定。
- V1からCurrentまでの全Migration。
- Temporary書込み失敗、Flush失敗、Replace失敗。
- Current破損からBackup復旧。
- 同時Save要求の順序。

ParserへRandom byte列を与えるFuzz Testも有効です。

## 50. Integration Test

1. SaveしてApplicationを終了する。
2. 再起動して同じ状態へ戻ることを確認する。
3. Save中にProcessを停止し、旧または新の完全なFileが残ることを確認する。
4. Disk容量不足・書込み禁止Directoryを模擬する。
5. 旧BuildのSaveを新Buildで開く。
6. 新BuildのSaveを旧Buildで拒否し、変更しないことを確認する。
7. Window・Audio・Input設定のApply/Cancelを確認する。

## 51. Save Service設計

```cpp
class ISaveStorage
{
public:
    virtual ~ISaveStorage() = default;
    virtual FileResult<std::vector<std::byte>> Read(std::string_view slot) = 0;
    virtual FileError WriteAtomically(
        std::string_view slot,
        std::span<const std::byte> bytes) = 0;
};

class SaveService final
{
public:
    explicit SaveService(ISaveStorage& storage) : storage_(storage) {}

    FileResult<SaveData> Load(std::string_view slot);
    FileError Save(std::string_view slot, const SaveSnapshot& snapshot);

private:
    ISaveStorage& storage_;
};
```

Serialize、Storage、Gameplay Snapshotを分離するとMemory Storageで高速にTestできます。

## 52. 実装チェックリスト

- [ ] Asset RootとUser Data Rootを分離した。
- [ ] User入力をPathへ直接連結していない。
- [ ] File Size・String Length・Collection Countに上限がある。
- [ ] DX File Handleを必ずcloseする。
- [ ] 非同期Open完了前にHandleを操作しない。
- [ ] 生のC++構造体やPointerを保存していない。
- [ ] Magic、Version、Payload Size、Checksumがある。
- [ ] Readerが全Access前に残りSizeを確認する。
- [ ] NaN、Infinity、範囲外値を拒否する。
- [ ] Version Migrationを段階的にTestした。
- [ ] 新Versionを旧Buildで上書きしない。
- [ ] Temporary + Flush + Replaceで保存する。
- [ ] 検証済みBackupから復旧できる。
- [ ] Gameplayから整合したSnapshotを作る。
- [ ] 同じSlotへの非同期Saveを直列化した。
- [ ] SettingsのDefault・Loaded・Appliedを分離した。
- [ ] Save成功を完了Event後に表示する。
- [ ] Logへ秘密・個人情報を出していない。
- [ ] 破損・容量不足・強制終了をTestした。

## 53. 練習課題

1. RAII対応のDX File Readerを作る。
2. 最大Size付きのFile全読込みを実装する。
3. Little Endianの整数Writer/Readerを作る。
4. Header付きBinary Saveを作る。
5. Fileを1byteずつ切断し、全て安全にErrorになるかTestする。
6. V1・V2・V3のMigrationを作る。
7. Temporary FileからAtomic ReplaceするStorageをPlatform抽象化する。
8. 3世代BackupとRecoveryを実装する。
9. Graphics Settingsの15秒確認Dialogを作る。
10. Main Thread SnapshotとWorker Saveを実装する。
11. 同時Auto Save要求を一つへCoalesceする。
12. Save Slot MetadataとThumbnailを分離する。
13. Path Traversal入力に対するTestを書く。
14. ParserへRandom Dataを入力するFuzz Testを行う。

## 54. 理解確認

1. Install DirectoryへSaveすべきでない理由は何ですか。
2. `FileRead_open` の失敗値とclose責任は何ですか。
3. 非同期Open直後にcloseしてはいけない理由は何ですか。
4. C++構造体をそのままBinary保存できない理由は何ですか。
5. MagicとVersionは何を防ぎますか。
6. Parse成功後にもValidationが必要な理由は何ですか。
7. Atomic Saveが途中終了から旧Dataを守る仕組みは何ですか。
8. Checksumと改ざん防止が同じでない理由は何ですか。
9. Save SnapshotがData Raceを防ぐ仕組みは何ですか。
10. 新Versionを古いBuildがDefaultで上書きすべきでない理由は何ですか。

## 55. この章の到達点

- DXライブラリのFile APIを失敗値・寿命・非同期契約込みで扱える。
- Asset、Save、Settings、Cacheの保存場所と責任を分離できる。
- Bounds Check付きBinary FormatとVersion Migrationを設計できる。
- Temporary、Flush、Replace、Backupで破損耐性を持たせられる。
- Gameplay Snapshotと非同期PipelineでFrame停止を抑えられる。
- 設定の仮適用・確認・Rollbackを実装できる。
- 不正File、Path Traversal、巨大入力からApplicationを守れる。
- Unit・Integration・Fuzz Testで永続化の失敗経路を検証できる。

## 56. 公式・標準資料

- [DXライブラリ：File読込み・DX Archive関係](https://dxlib.xsrv.jp/function/dxfunc_other.html)
- [DXライブラリ：関数一覧](https://dxlib.xsrv.jp/dxfunc.html)
- [DXライブラリ：DX Archiveの概要](https://dxlib.xsrv.jp/dxtec.html)
- [cppreference：std::filesystem](https://en.cppreference.com/w/cpp/filesystem)
- [cppreference：std::basic_fstream](https://en.cppreference.com/w/cpp/io/basic_fstream)

File APIの戻り値、非同期状態、文字型、Archive設定時期は利用中の公式Headerとリファレンスで再確認してください。
