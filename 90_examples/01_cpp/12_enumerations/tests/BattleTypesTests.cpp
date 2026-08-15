#include "BattleTypes.h"

#include <iostream>

namespace
{
int Check(const bool condition, const char* const name)
{
    if (condition)
    {
        std::cout << "[PASS] " << name << '\n';
        return 0;
    }
    std::cerr << "[FAIL] " << name << '\n';
    return 1;
}
}

int main()
{
    using namespace cpp_study;
    int failures = 0;
    failures += Check(ToString(ActionState::Attacking) == "Attacking", "state to string");
    failures += Check(ToString(Element::Electric) == "Electric", "element to string");
    failures += Check(!CanTransition(ActionState::Idle, ActionState::Idle), "same state rejected");
    failures += Check(CanTransition(ActionState::Attacking, ActionState::Dodging), "attack dodge cancel");
    failures += Check(!CanTransition(ActionState::Attacking, ActionState::Moving), "attack move rejected");
    failures += Check(CanTransition(ActionState::Stunned, ActionState::Idle), "stun recovery");
    failures += Check(!CanTransition(ActionState::Stunned, ActionState::Attacking), "stun attack rejected");

    const StatusEffect effects = StatusEffect::Poison | StatusEffect::Frozen;
    failures += Check(HasEffect(effects, StatusEffect::Poison), "poison flag present");
    failures += Check(!HasEffect(effects, StatusEffect::Burning), "burning flag absent");
    failures += Check(!HasEffect(RemoveEffect(effects, StatusEffect::Poison), StatusEffect::Poison),
                      "remove poison flag");
    failures += Check(!HasEffect(effects, StatusEffect::None), "none is not a searchable flag");

    std::cout << "失敗数: " << failures << '\n';
    return failures == 0 ? 0 : 1;
}

