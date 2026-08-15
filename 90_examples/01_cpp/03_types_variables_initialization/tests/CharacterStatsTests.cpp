#include "CharacterStats.h"

#include <cmath>
#include <iostream>
#include <string>

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

    // {}だけで構築し、Member初期化子が安全な既定値を与えることを確認する。
    const cpp_study::CharacterStats defaults{};
    failures += Check(defaults.name == "Unknown", "default name");
    failures += Check(defaults.level == 1, "default level");
    failures += Check(defaults.health == 100, "default health");
    failures += Check(!defaults.isPlayerControlled, "default bool");

    const auto normalized = cpp_study::CreateCharacterStats(
        "", -5, -300, -2.0F, true, cpp_study::Element::Fire);
    failures += Check(normalized.name == "Unknown", "empty name fallback");
    failures += Check(normalized.level == 1, "level lower clamp");
    failures += Check(normalized.health == 0, "health lower clamp");
    failures += Check(std::fabs(normalized.moveSpeed) < 0.0001F, "speed lower clamp");
    failures += Check(normalized.isPlayerControlled, "bool initialization");
    failures += Check(normalized.element == cpp_study::Element::Fire, "enum initialization");

    const auto highLevel = cpp_study::CreateCharacterStats(
        "Boss", 999, 5000, 3.0F, false, cpp_study::Element::Ice);
    failures += Check(highLevel.level == 100, "level upper clamp");
    failures += Check(std::string{cpp_study::ToString(highLevel.element)} == "Ice", "enum text");

    std::cout << "失敗数: " << failures << '\n';
    return failures == 0 ? 0 : 1;
}
