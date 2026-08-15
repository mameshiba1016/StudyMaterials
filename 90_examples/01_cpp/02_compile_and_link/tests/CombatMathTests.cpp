#include "CombatMath.h"

#include <iostream>

namespace
{
int ExpectEqual(const int actual, const int expected, const char* name)
{
    if (actual == expected)
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
    using cpp_study::combat::CalculateDamage;
    int failures = 0;

    // 通常経路を確認する。120 - 35 = 85。
    failures += ExpectEqual(
        CalculateDamage(120, 35, false).finalDamage,
        85,
        "normal damage");

    // Critical経路を確認する。120 + 60 - 35 = 145。
    const auto critical = CalculateDamage(120, 35, true);
    failures += ExpectEqual(critical.criticalBonus, 60, "critical bonus");
    failures += ExpectEqual(critical.finalDamage, 145, "critical damage");

    // 防御力が攻撃力を超えても最低Damage 1になる境界を確認する。
    failures += ExpectEqual(
        CalculateDamage(10, 999, false).finalDamage,
        1,
        "minimum damage clamp");

    // 負の入力は0へ正規化される。不正入力の扱いもTestで仕様として固定する。
    const auto invalidInput = CalculateDamage(-10, -20, false);
    failures += ExpectEqual(invalidInput.baseDamage, 0, "negative attack normalization");
    failures += ExpectEqual(invalidInput.defenseReduction, 0, "negative defense normalization");
    failures += ExpectEqual(invalidInput.finalDamage, 1, "invalid input minimum damage");

    std::cout << "失敗数: " << failures << '\n';
    return failures == 0 ? 0 : 1;
}
