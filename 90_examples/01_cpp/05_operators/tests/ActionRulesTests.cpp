#include "ActionRules.h"

#include <cmath>
#include <iostream>

namespace
{
int Check(const bool condition, const char* name)
{
    if (condition)
    {
        std::cout << "[PASS] " << name << '\n';
        return 0;
    }
    std::cerr << "[FAIL] " << name << '\n';
    return 1;
}
} // namespace

int main()
{
    int failures = 0;
    failures += Check(cpp_study::CanStartAttack({50, 25, false, true, false}), "attack allowed");
    failures += Check(!cpp_study::CanStartAttack({10, 25, false, true, false}), "insufficient stamina");
    failures += Check(!cpp_study::CanStartAttack({50, 25, true, true, false}), "stun blocks attack");
    failures += Check(!cpp_study::CanStartAttack({50, 25, false, false, false}), "air attack blocked");
    failures += Check(cpp_study::CanStartAttack({50, 25, false, false, true}), "air attack allowed");
    failures += Check(cpp_study::ConsumeStamina(70, 25) == 45, "compound subtraction");
    failures += Check(cpp_study::ConsumeStamina(10, 25) == 0, "stamina clamp");
    failures += Check(cpp_study::ConsumeStamina(10, -5) == 10, "negative cost normalization");
    failures += Check(cpp_study::NextComboIndex(2, 3) == 0, "modulo wrap");
    failures += Check(cpp_study::NextComboIndex(2, 0) == 0, "zero modulo guard");
    failures += Check(std::fabs(cpp_study::CalculateHealthRatio(35, 100) - 0.35) < 0.000001, "floating division");
    failures += Check(cpp_study::CalculateHealthRatio(35, 0) == 0.0, "division by zero guard");
    std::cout << "失敗数: " << failures << '\n';
    return failures == 0 ? 0 : 1;
}
