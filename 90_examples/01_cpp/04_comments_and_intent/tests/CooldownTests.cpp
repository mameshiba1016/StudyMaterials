#include "Cooldown.h"

#include <cmath>
#include <iostream>

namespace
{
int ExpectNear(const double actual, const double expected, const char* name)
{
    constexpr double tolerance = 0.000001;
    if (std::fabs(actual - expected) <= tolerance)
    {
        std::cout << "[PASS] " << name << '\n';
        return 0;
    }
    std::cerr << "[FAIL] " << name << ": expected=" << expected
              << ", actual=" << actual << '\n';
    return 1;
}
} // namespace

int main()
{
    int failures = 0;
    failures += ExpectNear(
        cpp_study::CalculateRemainingCooldown({8.0, 3.0, 1.5, false}),
        3.5,
        "recovery multiplier");
    failures += ExpectNear(
        cpp_study::CalculateRemainingCooldown({8.0, 100.0, 1.0, false}),
        0.0,
        "remaining time lower clamp");
    failures += ExpectNear(
        cpp_study::CalculateRemainingCooldown({8.0, 3.0, 1.5, true}),
        8.0,
        "pause freezes cooldown");
    failures += ExpectNear(
        cpp_study::CalculateRemainingCooldown({8.0, 3.0, 0.0, false}),
        5.0,
        "invalid recovery rate fallback");
    failures += ExpectNear(
        cpp_study::CalculateRemainingCooldown({-8.0, -3.0, 1.0, false}),
        0.0,
        "negative input normalization");
    failures += cpp_study::IsSkillReady(0.0) ? 0 : 1;
    failures += !cpp_study::IsSkillReady(0.1) ? 0 : 1;
    std::cout << "失敗数: " << failures << '\n';
    return failures == 0 ? 0 : 1;
}
