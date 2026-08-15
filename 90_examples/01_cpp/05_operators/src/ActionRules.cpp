#include "ActionRules.h"

#include <algorithm>

namespace cpp_study
{
bool CanStartAttack(const ActionState& state) noexcept
{
    // &&は左から評価し、左辺がfalseなら右辺を評価しないShort-circuitを行う。
    const bool hasEnoughStamina = state.stamina >= state.attackCost;
    const bool movementCondition = state.isGrounded || state.allowsAirAttack;

    // !、&&、||には優先順位があるが、括弧と意味別変数で意図を明確にする。
    return hasEnoughStamina && !state.isStunned && movementCondition;
}

int ConsumeStamina(const int currentStamina, const int cost) noexcept
{
    int result = std::max(currentStamina, 0);
    const int safeCost = std::max(cost, 0);

    // result = result - safeCostと同じ意味を複合代入演算子で表す。
    result -= safeCost;
    return std::max(result, 0);
}

std::size_t NextComboIndex(
    const std::size_t currentIndex,
    const std::size_t comboCount) noexcept
{
    // 0での剰余は未定義動作なので、演算前に必ず除外する。
    if (comboCount == 0)
    {
        return 0;
    }
    return (currentIndex + 1U) % comboCount;
}

double CalculateHealthRatio(const int health, const int maxHealth) noexcept
{
    if (maxHealth <= 0)
    {
        return 0.0;
    }

    const int clampedHealth = std::clamp(health, 0, maxHealth);
    // int / intなら小数部を失うため、割る前にdoubleへ明示変換する。
    return static_cast<double>(clampedHealth) / static_cast<double>(maxHealth);
}
} // namespace cpp_study
