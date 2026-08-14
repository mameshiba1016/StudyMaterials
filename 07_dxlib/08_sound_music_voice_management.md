# 第8章 Sound・Music・Voice管理

音は映像と同じくResourceですが、時間と同時再生という性質があります。本章ではAPIを直接各所から呼ばず、SE、BGM、Voice、UIを一つのAudio Systemで管理します。

## 1. 音が聞こえるまで

```text
音声File → Decode → Sample列 → Buffer/Stream
→ Voice再生 → Volume/Pan/Pitch → Mix → Audio Device
```

Sample Rateは1秒当たりの標本数、Channel数はMono/Stereoなど、Bit Depthは各Sampleの精度です。

## 2. Sound Handle

```cpp
const int handle = LoadSoundMem("assets/audio/hit.wav");
if (handle == -1) return -1;

PlaySoundMem(handle, DX_PLAYTYPE_BACK, TRUE);
StopSoundMem(handle);
DeleteSoundMem(handle);
```

Handleは内部音声Resourceの識別値です。再生中に削除する場合は停止してから削除します。

## 3. 再生形式

- `DX_PLAYTYPE_NORMAL`: 再生終了まで呼出側を待たせる用途。Game Loopでは通常避ける。
- `DX_PLAYTYPE_BACK`: Background再生。短いSEの基本。
- `DX_PLAYTYPE_LOOP`: 停止するまでLoop。BGMや環境音向け。

第三引数`TopPositionFlag`が`TRUE`なら先頭から、`FALSE`なら停止位置・指定位置から再開します。

## 4. 再生状態

```cpp
const int state = CheckSoundMem(handle);
if (state == 1) { /* 再生中 */ }
else if (state == 0) { /* 停止中 */ }
else { /* Error */ }
```

戻り値`-1`を「停止中」と混同しません。待機LoopでMain Threadを塞がず、毎Frame状態を更新します。

## 5. RAII

```cpp
#include <utility>

class UniqueSound final
{
public:
    UniqueSound() = default;
    explicit UniqueSound(int h) : handle_(h) {}
    ~UniqueSound() { Reset(); }

    UniqueSound(const UniqueSound&) = delete;
    UniqueSound& operator=(const UniqueSound&) = delete;

    UniqueSound(UniqueSound&& rhs) noexcept
        : handle_(std::exchange(rhs.handle_, -1)) {}

    UniqueSound& operator=(UniqueSound&& rhs) noexcept
    {
        if (this != &rhs)
        {
            Reset();
            handle_ = std::exchange(rhs.handle_, -1);
        }
        return *this;
    }

    void Reset()
    {
        if (handle_ != -1)
        {
            StopSoundMem(handle_);
            DeleteSoundMem(handle_);
            handle_ = -1;
        }
    }

    [[nodiscard]] int Get() const noexcept { return handle_; }
    [[nodiscard]] bool IsValid() const noexcept { return handle_ != -1; }

private:
    int handle_ = -1;
};
```

DXライブラリ終了前にAudio Systemを破棄します。

## 6. Sound AssetとPlayback Voice

Sound Assetは読み込んだ音、Playback Voiceは「今鳴っている一回」です。同じHit音が同時に複数鳴るなら、Asset一つに対して複数Voiceが必要です。

## 7. 重複再生

`DuplicateSoundMem`で別Handleを用意できます。ただしStreaming形式や長時間Dataには制約があります。短いSE用Voice Poolとして事前複製し、BGMへ乱用しません。

```cpp
struct SoundVoice final
{
    UniqueSound sound{};
    std::uint64_t startedAtFrame = 0;
    int priority = 0;
};
```

## 8. Voice Pool

同一SEを連打するときは停止中Voiceを選び、空きがなければPolicyに従います。

```text
空きVoiceを使う
→ なければ低Priorityを奪う
→ 同Priorityなら最古を奪う
→ 重要度が低ければ新規再生を捨てる
```

無制限複製はMemoryと音量を増やします。

## 9. 次回再生だけの設定

```cpp
ChangeNextPlayVolumeSoundMem(180, handle);
ChangeNextPlayPanSoundMem(-80, handle);
PlaySoundMem(handle, DX_PLAYTYPE_BACK, TRUE);
```

既に鳴っている同HandleのVoiceまで変えたくない場合に重要です。

## 10. Volumeの階層

```text
Final = Master × Category × Asset × Instance × Distance × Ducking
```

各値を0～1で保持し、最後に0～255へ変換します。

```cpp
#include <algorithm>

[[nodiscard]] constexpr int ToDxVolume(float linear)
{
    return static_cast<int>(std::clamp(linear, 0.0F, 1.0F) * 255.0F);
}
```

## 11. Category

```cpp
enum class AudioCategory { Bgm, Se, Voice, Ui, Ambience };

struct AudioBus final
{
    float volume = 1.0F;
    bool muted = false;
};
```

設定画面ではMaster、BGM、SE、Voiceを別々に変更できるようにします。

## 12. Decibel

人間の音量知覚は線形ではありません。UI Sliderをそのまま線形Gainへすると小さい側の調整が難しくなります。

```cpp
#include <cmath>

[[nodiscard]] float DbToLinear(float db)
{
    return std::pow(10.0F, db / 20.0F);
}
```

0 dBは等倍、-6 dBは概ね半分の振幅です。Muteは十分小さい値ではなく明示状態にします。

## 13. Pan

`ChangePanSoundMem`の範囲は-255～255です。公式仕様では正値で左音量、負値で右音量を下げます。反対側を増幅する正確なPanとは異なります。

```cpp
[[nodiscard]] int PositionToPan(float relativeX, float audibleRange)
{
    if (audibleRange <= 0.0F) return 0;
    return static_cast<int>(std::clamp(relativeX / audibleRange,
                                      -1.0F, 1.0F) * 255.0F);
}
```

## 14. 距離減衰

```cpp
[[nodiscard]] constexpr float DistanceGain(float distance,
                                           float inner, float outer)
{
    if (distance <= inner) return 1.0F;
    if (distance >= outer) return 0.0F;
    const float t = (distance - inner) / (outer - inner);
    return 1.0F - t;
}
```

実用ではCurve、遮蔽、Camera基準かPlayer基準かを仕様化します。

## 15. Pitchと周波数

Pitch変更は同じ素材へ変化を付けられます。極端な変更は長さや音質も変えます。足音へ小さい乱数を加える場合、Gameplay用乱数列と分離します。

## 16. BGMはStreaming

長いBGMを全展開するとMemoryを多く消費します。`SetCreateSoundDataType`でStreaming向け形式を設定してからLoadする方式があります。設定は後続Loadへ影響するため、専用Loaderに閉じ込めて復元します。

## 17. SEはMemory常駐

短く頻繁なSEは事前にMemoryへ読み、即座に再生します。戦闘中にFile I/OとDecodeを起こさないようScene開始前にPreloadします。

## 18. BGM Controller

```cpp
enum class BgmState { Stopped, FadingIn, Playing, FadingOut };

struct BgmController final
{
    int currentHandle = -1;
    int nextHandle = -1;
    BgmState state = BgmState::Stopped;
    float fadeSeconds = 0.0F;
    float elapsed = 0.0F;
};
```

再生、停止、曲交換を状態機械にし、各Sceneが直接BGMを奪い合わないようにします。

## 19. Crossfade

現在曲をFade Outしながら次曲をFade Inします。二曲を同時再生するため別Handle/Voiceが必要です。Gain Curveを線形にすると中間が小さく感じる場合があり、Equal-power Curveも検討します。

## 20. Loop点

曲の先頭・末尾以外でLoopする場合、Sample単位のLoop点とCodec遅延を考えます。ミリ秒指定からSampleへの丸めでClickが出ないか実機確認します。

## 21. PauseとResume

`StopSoundMem`後、`PlaySoundMem(..., FALSE)`で位置を戻さず再開する契約がありますが、Backendや版の挙動を公式資料で確認します。確実性が必要なら停止前の再生位置を取得・保存し、再設定します。

## 22. Game PauseのPolicy

- BGMは継続、SEは停止。
- 全音をPause。
- UI音だけ許可。
- Voiceは継続して字幕と同期。

時間Scaleを0にするだけではAudio Deviceは止まりません。Audio側へ明示的にPause命令を送ります。

## 23. Voiceと字幕

Voice再生Eventに字幕ID、話者、表示時間を関連付けます。音声が無効でも字幕は表示できる構造にします。字幕時間をFrame数ではなくTimelineで管理します。

## 24. Voice Priority

重要会話、戦闘掛け声、被Damage声を同時に無制限再生しません。重要会話は中断不可、同一Characterの短い掛け声は置換などPolicyを定義します。

## 25. Ducking

重要Voice再生中にBGMやSEを一時的に下げます。

```text
Voice開始 → BGM BusをAttack時間で低下
Voice継続 → 低下を維持
Voice終了 → Release時間で復帰
```

瞬時変更は不自然なので補間します。

## 26. Audio Event

```cpp
enum class SoundId { PlayerAttack, HitLight, HitHeavy, UiConfirm };

struct PlaySoundRequest final
{
    SoundId id{};
    Vec2 worldPosition{};
    float volume = 1.0F;
    float pitchCents = 0.0F;
    int priority = 0;
};
```

GameplayはHandleではなく論理IDを発行し、Audio SystemがAsset、Bus、Voiceを解決します。

## 27. 同一Frameの抑制

多数の敵へ同時Hitすると同じSEが重なり過大音量になります。同じIDを一つへまとめる、最大数を設ける、位置を平均するなどConcurrency Ruleを設定します。

## 28. Cooldown

足音や連射音には最小再生間隔を設けられます。ただし入力受付や攻撃判定のCooldownとAudio演出のCooldownは別物です。

## 29. Determinism

音声再生成功・終了時刻を戦闘結果へ使いません。Audio Deviceがない環境でもSimulationが同じ結果になる必要があります。

## 30. Resource Cache

正規化PathまたはSound IDごとに一度Loadし、参照CountやScene Scopeで解放します。Streaming BGMとMemory SEは同じCache Entryへ混ぜず、Load方式もKeyへ含めます。

## 31. Loading失敗

必須Voiceが失敗しても字幕で継続する、SE欠落はLogして無音、必須BGM欠落は代替曲などFallback Policyを決めます。`-1`を再生APIへ渡しません。

## 32. Audio Device消失

出力Deviceの切断・変更があり得ます。Audio失敗でGame全体をCrashさせず、再初期化、Mute継続、User通知を選びます。

## 33. Thread

Mixingは別Threadで動く場合があります。Main ThreadのObjectをAudio callbackから直接触らず、Command QueueとSnapshotで受け渡します。DXライブラリAPIのThread制約は公式仕様を正とします。

## 34. Latency

入力から音が出るまでにはGame update、Command提出、Audio Buffer、Deviceの遅延があります。短いBufferは低遅延ですが途切れに弱くなります。体感が重要な攻撃音は実機で測ります。

## 35. 音割れ

多数の音を加算すると最大振幅を超え、Clippingします。個別音量を下げ、同時数を制限し、Masterへ余裕（Headroom）を残します。「全部255」は安全ではありません。

## 36. Debug表示

再生Voice数、Category別音量、BGM状態、奪取回数、捨てたRequest数、Load失敗、現在位置を表示します。音が聞こえない場合も内部状態を目で追えます。

## 37. よくある不具合

- 音が鳴らない: Handle、Volume、Mute、再生形式、File Path、Deviceを確認。
- 一度しか鳴らない: 再生位置を先頭へ戻すFlagを確認。
- 連打で前音が切れる: 同じVoiceを奪っている。Pool/複製を使う。
- BGMでMemory急増: 全展開ではなくStreamingを検討。
- Scene後も鳴る: 所有Scopeと停止Policyが不明確。
- Pause後に位置が違う: 保存・再設定契約とBackend挙動を確認。

## 38. Test可能な計算

```cpp
static_assert(ToDxVolume(0.0F) == 0);
static_assert(ToDxVolume(1.0F) == 255);
static_assert(DistanceGain(0.0F, 2.0F, 10.0F) == 1.0F);
static_assert(DistanceGain(10.0F, 2.0F, 10.0F) == 0.0F);
```

実APIはAdapterへ閉じ込め、Volume合成、減衰、Priority選択を単体Testします。

## 39. 設計チェックリスト

- [ ] Load失敗とCheck失敗を区別した。
- [ ] 再生中に停止してからHandleを削除する。
- [ ] SE、BGM、Voice、UIのBusを分けた。
- [ ] 長いBGMをStreamingした。
- [ ] 重複SEにVoice Poolと上限がある。
- [ ] 次回再生だけのVolume/Panを使い分けた。
- [ ] Pause PolicyとVoice Priorityを定義した。
- [ ] Audio Eventが戦闘結果を変更しない。
- [ ] DX終了前にAudio Resourceを破棄する。
- [ ] Voice数、Clipping、Latencyを計測した。

## 40. 実践課題

1. Move-onlyな`UniqueSound`を作る。
2. Category VolumeとMuteを実装する。
3. 8VoiceのSE Poolと最古Voice奪取を作る。
4. 距離減衰とPan付きWorld SEを作る。
5. BGM Crossfade状態機械を作る。
6. Voiceと字幕、Duckingを同期する。
7. Pause/Resumeを全Categoryで検証する。
8. 同一FrameのHit音を抑制する。
9. Audio Debug Monitorを作る。

## 41. 公式資料

- [DXライブラリ Sound関数リファレンス](https://dxlib.xsrv.jp/function/dxfunc_sound.html)
- [DXライブラリ その他関数・Sound Data Type](https://dxlib.xsrv.jp/function/dxfunc_other.html)
- [DXライブラリ 関数一覧](https://dxlib.xsrv.jp/dxfunc.html)

利用中バージョンの再生形式、Streaming制約、複製可否、再生位置単位、Thread制約を公式資料で確認してください。

## 42. 次章への接続

次章では2D CollisionとSpatial Queryを扱い、形状、接触、Layer、Broad Phase、Ray Cast、攻撃判定を描画・音声Eventへ安全につなぎます。
