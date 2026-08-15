#pragma once

namespace cpp_study
{
enum class InputCommand { None, LightAttack, HeavyAttack, Dodge };
enum class ActionDecision { Idle, LightAttack, HeavyAttack, Dodge, Stunned, Defeated };
enum class HitReaction { None, Flinch, Knockback, Launch };

struct DecisionContext final
{
    int health{100};
    int stamina{100};
    bool stunned{false};
    bool canAct{true};
    InputCommand input{InputCommand::None};
};

[[nodiscard]] ActionDecision DecideAction(const DecisionContext& context) noexcept;
[[nodiscard]] HitReaction DecideHitReaction(int poiseDamage) noexcept;
[[nodiscard]] const char* ToString(ActionDecision decision) noexcept;
} // namespace cpp_study
