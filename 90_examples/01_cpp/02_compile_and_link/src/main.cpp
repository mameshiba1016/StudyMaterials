#include "CombatMath.h"

#include <iostream>

int main()
{
    // CompilerはHeaderの宣言を見て、この呼出しが型として正しいか検査する。
    // 実際の関数本体は別翻訳単位CombatMath.cppにあるため、Linkerが後で結合する。
    const cpp_study::combat::DamageResult normal =
        cpp_study::combat::CalculateDamage(120, 35, false);
    const cpp_study::combat::DamageResult critical =
        cpp_study::combat::CalculateDamage(120, 35, true);

    std::cout << "Build構成: "
              << cpp_study::combat::BuildConfigurationName() << '\n';
    std::cout << "通常Damage: " << normal.finalDamage << '\n';
    std::cout << "Critical Bonus: " << critical.criticalBonus << '\n';
    std::cout << "Critical Damage: " << critical.finalDamage << '\n';
    std::cout << "combat_math.libとのLinkに成功しました。\n";
    return 0;
}
