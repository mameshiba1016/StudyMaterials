#include "DamageFunctions.h"
#include <algorithm>

namespace cpp_study
{
int ClampDamage(const int damage, const int minimum, const int maximum) noexcept
{
    // 呼出し側が上下限を逆に渡しても、有効な順へ正規化する。
    const int low = std::min(minimum, maximum);
    const int high = std::max(minimum, maximum);
    return std::clamp(damage, low, high);
}

int CalculateDamage(const int attack, const int defense) noexcept
{
    return ClampDamage(std::max(attack, 0) - std::max(defense, 0));
}

DamageResult CalculateDamage(const DamageInput& input) noexcept
{
    // 大きい処理を「入力正規化→倍率→防御→Clamp」という小さい段階に分ける。
    const int safeAttack = std::max(input.attack, 0);
    const double safeMultiplier = std::max(input.multiplier, 0.0);
    const double criticalMultiplier = input.critical ? 1.5 : 1.0;
    const int raw = static_cast<int>(safeAttack * safeMultiplier * criticalMultiplier);
    const int reduced = raw - std::max(input.defense, 0);
    return DamageResult{raw, reduced, ClampDamage(reduced)};
}

std::string BuildDamageMessage(const std::string& attackerName, const DamageResult& result)
{
    const std::string& safeName = attackerName.empty() ? std::string{"Unknown"} : attackerName;
    return safeName + " dealt " + std::to_string(result.finalDamage) + " damage.";
}
} // namespace cpp_study
