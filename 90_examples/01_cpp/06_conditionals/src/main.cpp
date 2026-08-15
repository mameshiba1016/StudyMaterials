#include "ActionDecision.h"
#include <iostream>

int main()
{
    const cpp_study::DecisionContext context{
        .health = 100, .stamina = 55, .stunned = false,
        .canAct = true, .input = cpp_study::InputCommand::HeavyAttack};
    const auto action = cpp_study::DecideAction(context);
    std::cout << "選択Action: " << cpp_study::ToString(action) << '\n';
    std::cout << "Poise Damage 75はKnockback: " << std::boolalpha
              << (cpp_study::DecideHitReaction(75) == cpp_study::HitReaction::Knockback) << '\n';
    return 0;
}
