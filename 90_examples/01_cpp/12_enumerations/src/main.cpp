#include "BattleTypes.h"

#include <iostream>

int main()
{
    using cpp_study::ActionState;
    using cpp_study::Element;
    using cpp_study::StatusEffect;

    const ActionState current = ActionState::Attacking;
    const ActionState requested = ActionState::Dodging;
    const Element attackElement = Element::Electric;

    // operator|で複数Flagを合成する。整数ではなくStatusEffect型のまま扱える。
    StatusEffect effects = StatusEffect::Poison | StatusEffect::Burning;

    std::cout << "現在状態: " << cpp_study::ToString(current) << '\n';
    std::cout << "要求状態: " << cpp_study::ToString(requested) << '\n';
    std::cout << "遷移可能: " << std::boolalpha
              << cpp_study::CanTransition(current, requested) << '\n';
    std::cout << "攻撃属性: " << cpp_study::ToString(attackElement) << '\n';
    std::cout << "毒状態: " << cpp_study::HasEffect(effects, StatusEffect::Poison) << '\n';
    std::cout << "凍結状態: " << cpp_study::HasEffect(effects, StatusEffect::Frozen) << '\n';

    effects = cpp_study::RemoveEffect(effects, StatusEffect::Poison);
    std::cout << "毒解除後: " << cpp_study::HasEffect(effects, StatusEffect::Poison) << '\n';
    return 0;
}

