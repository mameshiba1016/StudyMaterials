#pragma once

namespace cpp_study
{
/// SkillのCooldown計算に必要な入力。
/// 時間の単位はすべて秒であり、倍率は1.0を基準とする。
struct CooldownInput final
{
    double baseSeconds{0.0};
    double elapsedSeconds{0.0};
    double recoveryRate{1.0};
    bool paused{false};
};

/// Cooldownの残り時間を秒で返す。
///
/// @param input 基礎時間、経過時間、回復倍率、Pause状態。
/// @return 0以上の残り秒数。不正な負値は安全側へ正規化される。
///
/// 所有権：inputを借用し、保存も変更もしない。
/// 失敗条件：例外は投げない。0以下の回復倍率は1.0として扱う。
[[nodiscard]] double CalculateRemainingCooldown(const CooldownInput& input) noexcept;

/// 残り時間が許容誤差以内ならSkillを使用可能と判定する。
[[nodiscard]] bool IsSkillReady(double remainingSeconds) noexcept;
} // namespace cpp_study
