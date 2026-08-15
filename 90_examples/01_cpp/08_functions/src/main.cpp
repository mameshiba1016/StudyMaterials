#include "DamageFunctions.h"
#include <iostream>

int main()
{
    const cpp_study::DamageInput input{120, 30, 1.25, true};
    const auto result = cpp_study::CalculateDamage(input);
    std::cout << "Raw Damage: " << result.rawDamage << '\n';
    std::cout << "Reduced Damage: " << result.reducedDamage << '\n';
    std::cout << "Final Damage: " << result.finalDamage << '\n';
    std::cout << cpp_study::BuildDamageMessage("Hero", result) << '\n';
    std::cout << "Simple Overload: " << cpp_study::CalculateDamage(50, 20) << '\n';
    return 0;
}
