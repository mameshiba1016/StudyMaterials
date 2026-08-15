#include "CombatData.h"

#include <iostream>

namespace
{
int Check(const bool condition, const char* const name)
{
    if (condition) { std::cout << "[PASS] " << name << '\n'; return 0; }
    std::cerr << "[FAIL] " << name << '\n'; return 1;
}
}

int main()
{
    using namespace cpp_study;
    int failures = 0;

    const Vector2 vector{3.0F, 4.0F};
    failures += Check(vector.LengthSquared() == 25.0F, "vector length squared");

    CombatStats target{"Enemy", 100, 100, 10, 8, {1.0F, 2.0F}};
    failures += Check(IsValid(target), "valid stats");
    Move(target, {2.0F, -1.0F});
    failures += Check(target.position.x == 3.0F && target.position.y == 1.0F, "move by reference");

    const DamageResult first = ApplyDamage(target, 30);
    failures += Check(first.rawDamage == 30, "raw damage");
    failures += Check(first.appliedDamage == 22, "defense reduction");
    failures += Check(first.remainingHp == 78 && target.hp == 78, "hp mutation");
    failures += Check(!first.defeated, "survived hit");

    const DamageResult negative = ApplyDamage(target, -50);
    failures += Check(negative.appliedDamage == 0 && target.hp == 78, "negative damage clamped");
    const DamageResult lethal = ApplyDamage(target, 999);
    failures += Check(lethal.remainingHp == 0 && lethal.defeated, "lethal damage clamped");

    CombatStats invalid{"", 0, -1, -1, -1, {}};
    failures += Check(!IsValid(invalid), "invalid stats rejected");
    const CombatStats defaults{};
    failures += Check(defaults.name == "Unknown" && defaults.hp == 1, "member defaults");

    std::cout << "失敗数: " << failures << '\n';
    return failures == 0 ? 0 : 1;
}

