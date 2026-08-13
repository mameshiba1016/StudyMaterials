# オーディオ・SE・BGM・ミキシング

音はゲーム状態を伝え、操作の結果を補強します。ファイルを再生するだけでなく、Voice管理、Bus、同時発音、遅延、Scene遷移を設計します。

## 音声データと再生Voice

- Audio Clip/Buffer：PCMまたは圧縮音声データ。
- Voice/Source：再生位置、音量、Pitch、Loop等を持つ再生インスタンス。

同じSEを複数回鳴らすには、Clipを共有しVoiceを複数生成します。Clip HandleとVoice Handleを混同しません。

```cpp
struct PlaySoundRequest
{
    SoundId sound{};
    AudioBusId bus{};
    float volume{1.0F};
    float pitch{1.0F};
    Vector2 worldPosition{};
    EntityId owner{};
};
```

## Bus

```text
Master
├─ BGM
├─ SFX
│  ├─ Player
│  └─ Enemy
├─ Voice
└─ UI
```

各Busで音量、Mute、Effectを管理します。設定値0～1をそのまま線形Gainへすると、人間の音量感覚に対して操作が偏るため、dBまたは知覚に合うカーブへ変換します。

```text
gain = 10^(decibels / 20)
```

0 gainは負の無限dB相当なので特別扱いします。

## 同時発音制限

弾100発が同時に鳴ると音割れ・Voice枯渇・CPU負荷になります。

- Sound単位の最大Voice数。
- Owner単位の制限。
- 最も古い・小さい・遠いVoiceを停止するVoice Stealing。
- 短時間の同音をまとめる。
- Priorityによる選択。

重要なParry音が環境音に奪われないよう優先度を設計します。

## 2D音と位置音

UI・BGMはListener位置に依存しない2D音、世界SEは距離・Panを適用します。

```cpp
float attenuation{1.0F - std::clamp(distance / maximumDistance, 0.0F, 1.0F)};
float pan{std::clamp(relativeX / panDistance, -1.0F, 1.0F)};
```

線形減衰は一例で、自然な曲線や最小距離を使います。Camera中心とPlayer位置のどちらをListenerにするかで画面端の音が変わります。

## 遅延

SEを鳴らす命令から実際の出力まで、Audio Bufferとミキサースレッドによる遅延があります。バッファを小さくすると遅延は減りますが、処理が間に合わず音切れしやすくなります。アクション音はプリロードし、初回再生時のデコード・確保を避けます。

## StreamingとDecompress

- 短いSE：全体をメモリへDecodeして低遅延再生。
- 長いBGM/Voice：圧縮データをStreaming Decodeしてメモリ削減。

Sceneロード中にI/Oが競合するとStreamingが途切れます。専用Buffer、優先度、先読みを用意します。

## BGM遷移

```text
現在BGMをFade Out
次BGMを開始/Fade In
```

単純停止では不自然です。Beat/Bar境界でSection切替するAdaptive Musicでは、サンプル時刻とBPMを使います。ゲームフレーム時間だけでは音楽同期精度が不足します。

## Ducking

Voice再生中にBGMを下げるなど、Sidechainで他BusのGainを一時低下させます。急変によるクリックを避け、Attack/Release時間で平滑化します。

## ヒットストップと音

ゲーム時間停止中も音は通常進みます。命中SEは止めず、低Pitch・Filter等でスローを演出する場合があります。Audio ClockとGame Clockを分けます。

## スレッド

Audio callbackはリアルタイム制約があります。

- 動的確保を避ける。
- mutex待ち、ファイルI/O、ログを避ける。
- Game ThreadからLock-free Queue等でCommandを渡す。
- callback内でゲームオブジェクトへ直接触れない。

ライブラリが内部処理していても、callbackを提供するAPIでは規約を確認します。

## Handleと終了通知

再生Voiceは自然終了・Voice Stealing・Scene遷移で消えます。古いHandleへ停止命令を送らないよう世代付きHandleを使います。終了CallbackをGame Threadへ通知する場合、Ownerがまだ生存するか検査します。

## 設定と保存

Master、BGM、SFX、Voiceを別々に保存し、起動直後から適用します。Mute時も再生位置を進めるか、停止するかをBusごとに決めます。Audio Device変更・切断から復旧できる設計も必要です。

## デバッグ

- Active Voice数と上限。
- BusごとのMeter、Gain、Mute。
- Streaming Buffer不足。
- 再生中Sound ID、Owner、Priority。
- Voice Stealing回数。
- Audio callback時間とUnderrun。

音量はピークだけでなくLoudnessも確認し、ヘッドルームを確保してクリッピングを防ぎます。
