# パーティクル・画面演出・ゲームフィール

パーティクルは短命な多数の要素を生成し、位置、速度、寿命、色、サイズを時間変化させる視覚システムです。命中火花、砂煙、残像、環境表現に使います。

## Particleデータ

```cpp
struct Particle
{
    Vector2 position{};
    Vector2 velocity{};
    Vector2 acceleration{};
    float rotationRadians{};
    float angularVelocity{};
    float ageSeconds{};
    float lifetimeSeconds{1.0F};
    float startSize{1.0F};
    float endSize{};
    Color startColor{Color::White};
    Color endColor{Color::Transparent};
    bool isAlive{true};
};
```

正規化寿命`t = age / lifetime`から色とサイズを補間します。`lifetime <= 0`は生成時に拒否します。

## 更新

```cpp
void UpdateParticle(Particle& particle, float deltaSeconds)
{
    particle.ageSeconds += deltaSeconds;
    if (particle.ageSeconds >= particle.lifetimeSeconds)
    {
        particle.isAlive = false;
        return;
    }

    particle.velocity += particle.acceleration * deltaSeconds;
    particle.position += particle.velocity * deltaSeconds;
    particle.rotationRadians += particle.angularVelocity * deltaSeconds;
}
```

視覚だけなら可変更新で十分な場合があります。戦闘判定をParticleへ持たせず、ゲームルールと表示を分離します。

## Emitter

EmitterはSpawn Rate、Burst数、形状、速度範囲、寿命、色カーブを定義します。`rate * delta`の小数残量を蓄積しないとFPSで生成数が変わります。

```cpp
spawnAccumulator += particlesPerSecond * deltaSeconds;
const int spawnCount{static_cast<int>(spawnAccumulator)};
spawnAccumulator -= spawnCount;
```

## Poolとデータ配置

毎Particleを`new/delete`せず、`vector`や固定Poolへ格納します。死亡要素を末尾とswapして削除すれば順序不要のParticleを定数時間で除去できます。大量更新ではPosition、Velocity等を別配列にするSoAも候補ですが、計測して選びます。

## 乱数

VFX用乱数列を戦闘乱数と分離します。Particle数変更でCritical判定まで変わる設計は再現性を壊します。デバッグ時はseedを固定可能にします。

## Blend

- Alpha Blend：煙、破片。
- Additive：光、火花。
- Multiply等：影・特殊表現。

Blendごとに描画をまとめ、順序とTexture切替を管理します。Additiveは白飛びしやすいためHDR・露出・色を確認します。

## ゲームフィールを作る要素

- ヒットストップ。
- Camera Shake。
- 画面フラッシュ、色収差等。
- Sprite squash/stretch。
- 残像。
- SEとPitch variation。
- Controller vibration。
- 敵のBlink、ノックバック。

全部を最大にすると情報が読めません。攻撃の重要度に応じて強度を階層化します。

## 演出イベント

戦闘側が「HeavyHit at position, direction, intensity」の意味イベントを送り、VFX・Audio・Cameraがそれぞれ反応します。戦闘コードが具体TextureやSoundファイルを直接操作しない設計です。

## アクセシビリティ

画面揺れ、点滅、色変化、振動、残像は設定で強度を下げられるようにします。光過敏性への配慮として高速点滅を避け、重要情報を色だけに依存させません。

## 性能予算

同時Particle数、Draw Call、Overdraw、Textureサイズ、更新時間へ上限を設けます。画面外Emitter停止時に、Simulationを止めるのか時間だけ進めるのかを仕様化します。
