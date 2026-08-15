#include "ArrayRules.h"
#include <iostream>

int main()
{
    const cpp_study::ComboDamage combo{20, 35, 50, 90};
    std::cout << "要素数: " << combo.size() << '\n';
    std::cout << "先頭Damage: " << cpp_study::DamageAt(combo, 0) << '\n';
    std::cout << "合計Damage: " << cpp_study::TotalDamage(combo) << '\n';
    const auto boosted = cpp_study::ApplyBonus(combo, 5);
    std::cout << "Bonus後の最終段: " << boosted.back() << '\n';
    const cpp_study::Grid grid{{{{1,2,3}},{{4,5,6}}}};
    std::cout << "2次元配列合計: " << cpp_study::SumGrid(grid) << '\n';
    return 0;
}
