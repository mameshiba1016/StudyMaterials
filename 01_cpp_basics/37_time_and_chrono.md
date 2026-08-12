# 時間・`std::chrono`・ゲーム時間

時間には時刻、期間、ゲーム時間、実時間、固定ステップなど複数の意味があります。裸の`float`だけでは単位と基準が分からず、バグを生みます。

## clock・time_point・duration

```cpp
#include <chrono>

using Clock = std::chrono::steady_clock;

const Clock::time_point start{Clock::now()};
DoWork();
const Clock::time_point end{Clock::now()};

const std::chrono::duration<double> elapsed{end - start};
std::cout << elapsed.count() << " seconds\n";
```

- clock：時刻源。
- time_point：あるclock上の時点。
- duration：二時点の差や時間量。

## clockの選択

- `steady_clock`：単調増加し、経過時間計測に向く。
- `system_clock`：実世界の壁時計に対応し、時刻調整で飛ぶ可能性がある。
- `high_resolution_clock`：最も細かいとされるが、どのclockの別名か実装依存。

ゲームのフレーム経過時間へ`system_clock`を使うと、時刻同期やユーザー変更で負・巨大デルタになる危険があります。

## 単位

```cpp
using namespace std::chrono_literals;

constexpr auto hitStopDuration{80ms};
constexpr auto respawnDelay{3s};
```

型が単位変換を管理します。`duration_cast`で明示変換できますが、整数durationへ変換すると端数を切り捨てます。C++17以降の`floor`、`ceil`、`round`も目的に応じて使います。

## 可変デルタ時間

```cpp
position += velocity * deltaSeconds;
```

フレーム間隔に応じて移動量を調整します。しかし非常に大きなデルタでは壁抜け、制御不安定、アニメーション飛びが起きます。停止・ブレークポイント・ウィンドウ移動後のデルタを上限へクランプする設計があります。

## 固定時間ステップ

物理・決定的シミュレーションでは一定幅で更新します。

```cpp
constexpr double fixedStep{1.0 / 60.0};
accumulator += frameDelta;

while (accumulator >= fixedStep)
{
    Simulate(fixedStep);
    accumulator -= fixedStep;
}
```

描画は余り時間から前後状態を補間できます。一フレームが遅れた時に無制限に追いつこうとすると更新が増えてさらに遅れるspiral of deathが起きるため、最大反復数やデルタ上限を設計します。

## ゲーム時間と実時間

- ゲーム時間：ポーズやスローで停止・倍率変更される。
- 非スケール時間：UI、ネットワークタイムアウト等でゲーム速度の影響を受けない。
- 固定時間：物理シミュレーション用。
- オーディオ時間：サンプル精度の基準。

攻撃クールダウンがポーズ中に進むべきかなど、用途ごとにclockを選びます。

## 浮動小数点の蓄積

起動からの総秒数を`float`へ蓄積すると、値が大きくなるほど細かな時間差を表現できなくなります。高精度duration、整数tick、局所タイマー、基準時点との差を使います。

## スリープは正確なフレーム制御ではない

`sleep_for`は少なくとも指定期間ブロックする意図で、OSスケジューリングにより遅く復帰し得ます。フレーム制限は計測、スリープ、必要に応じ短い待機、VSync、レンダリングAPIの同期を組み合わせます。忙しいループだけで待つとCPUと電力を浪費します。

## プロファイリング

平均FPSだけでなく、フレーム時間の分布、最大値、パーセンタイル、CPU/GPUの待ち、各区間時間を測ります。60 FPSの予算は約16.67 msですが、CPUとGPUが単純に同じ直列予算を共有するとは限りません。
