#include "ActionRules.h"

#include <iostream>

int main()
{
    cpp_study::ActionState state{
        .stamina = 70,
        .attackCost = 25,
        .isStunned = false,
        .isGrounded = true,
        .allowsAirAttack = false,
    };

    std::cout << std::boolalpha;
    std::cout << "攻撃可能: " << cpp_study::CanStartAttack(state) << '\n';

    // =は代入演算子。右辺の計算結果でstate.staminaを更新する。
    state.stamina = cpp_study::ConsumeStamina(state.stamina, state.attackCost);
    std::cout << "攻撃後Stamina: " << state.stamina << '\n';

    std::size_t comboIndex = 2;
    comboIndex = cpp_study::NextComboIndex(comboIndex, 3);
    std::cout << "次Combo Index: " << comboIndex << '\n';

    std::cout << "HP比率: " << cpp_study::CalculateHealthRatio(35, 100) << '\n';
    std::cout << "整数除算 35 / 100: " << (35 / 100) << '\n';
    return 0;
}
