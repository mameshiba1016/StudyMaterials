#include "ActionDecision.h"
#include <iostream>

namespace { int Check(bool value, const char* name) { if (value) { std::cout << "[PASS] " << name << '\n'; return 0; } std::cerr << "[FAIL] " << name << '\n'; return 1; } }

int main()
{
    using namespace cpp_study;
    int failures = 0;
    failures += Check(DecideAction({0, 100, false, true, InputCommand::HeavyAttack}) == ActionDecision::Defeated, "defeated priority");
    failures += Check(DecideAction({100, 100, true, true, InputCommand::HeavyAttack}) == ActionDecision::Stunned, "stun priority");
    failures += Check(DecideAction({100, 55, false, true, InputCommand::HeavyAttack}) == ActionDecision::HeavyAttack, "heavy attack");
    failures += Check(DecideAction({100, 39, false, true, InputCommand::HeavyAttack}) == ActionDecision::Idle, "heavy stamina boundary");
    failures += Check(DecideAction({100, 20, false, true, InputCommand::Dodge}) == ActionDecision::Dodge, "dodge boundary");
    failures += Check(DecideAction({100, 15, false, true, InputCommand::LightAttack}) == ActionDecision::LightAttack, "light boundary");
    failures += Check(DecideHitReaction(0) == HitReaction::None, "no reaction");
    failures += Check(DecideHitReaction(1) == HitReaction::Flinch, "flinch");
    failures += Check(DecideHitReaction(50) == HitReaction::Knockback, "knockback boundary");
    failures += Check(DecideHitReaction(100) == HitReaction::Launch, "launch boundary");
    std::cout << "失敗数: " << failures << '\n';
    return failures == 0 ? 0 : 1;
}
