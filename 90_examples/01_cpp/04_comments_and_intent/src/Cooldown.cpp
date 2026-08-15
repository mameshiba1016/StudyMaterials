#include "Cooldown.h"

#include <algorithm>

namespace cpp_study
{
double CalculateRemainingCooldown(const CooldownInput& input) noexcept
{
    // 「baseをmaxする」の言い換えではなく、負のAsset値をRuntimeへ伝播させない意図を書く。
    const double safeBaseSeconds = std::max(input.baseSeconds, 0.0);
    const double safeElapsedSeconds = std::max(input.elapsedSeconds, 0.0);

    /*
     * Pause中は表示と判定を同じ値へ固定する。
     * elapsedSecondsは外側のTime Systemが更新停止する設計だが、この関数側でも
     * pausedを扱うことで、UIとGameplayが別の呼出し順でも結果を一致させる。
     */
    if (input.paused)
    {
        return safeBaseSeconds;
    }

    // 回復倍率0以下は除算不能または時間逆行になるため、既定倍率1.0へ戻す。
    const double safeRecoveryRate = input.recoveryRate > 0.0
        ? input.recoveryRate
        : 1.0;

    const double recoveredSeconds = safeElapsedSeconds * safeRecoveryRate;

    // UIへ負の残り時間を渡さず、Skill Readyの境界を0へ統一する。
    return std::max(safeBaseSeconds - recoveredSeconds, 0.0);
}

bool IsSkillReady(const double remainingSeconds) noexcept
{
    // 浮動小数の丸めで1e-12秒程度が残っても、使用不能に見えない許容誤差を使う。
    constexpr double readyToleranceSeconds = 0.000001;
    return remainingSeconds <= readyToleranceSeconds;
}
} // namespace cpp_study
