#include "ActionDecision.h"

namespace cpp_study
{
ActionDecision DecideAction(const DecisionContext& context) noexcept
{
    // Early Returnで最優先かつ以後の判定が不要な状態から処理する。
    if (context.health <= 0)
        return ActionDecision::Defeated;
    if (!context.canAct || context.stunned)
        return ActionDecision::Stunned;

    // switchは一つのEnum値に応じた分岐を一覧化できる。
    switch (context.input)
    {
    case InputCommand::Dodge:
        return context.stamina >= 20 ? ActionDecision::Dodge : ActionDecision::Idle;
    case InputCommand::HeavyAttack:
        return context.stamina >= 40 ? ActionDecision::HeavyAttack : ActionDecision::Idle;
    case InputCommand::LightAttack:
        return context.stamina >= 15 ? ActionDecision::LightAttack : ActionDecision::Idle;
    case InputCommand::None:
        return ActionDecision::Idle;
    }
    return ActionDecision::Idle;
}

HitReaction DecideHitReaction(const int poiseDamage) noexcept
{
    // 範囲を大きい側から調べ、境界が重複しないelse-if Chainにする。
    if (poiseDamage >= 100) return HitReaction::Launch;
    if (poiseDamage >= 50) return HitReaction::Knockback;
    if (poiseDamage > 0) return HitReaction::Flinch;
    return HitReaction::None;
}

const char* ToString(const ActionDecision decision) noexcept
{
    switch (decision)
    {
    case ActionDecision::Idle: return "Idle";
    case ActionDecision::LightAttack: return "LightAttack";
    case ActionDecision::HeavyAttack: return "HeavyAttack";
    case ActionDecision::Dodge: return "Dodge";
    case ActionDecision::Stunned: return "Stunned";
    case ActionDecision::Defeated: return "Defeated";
    }
    return "Unknown";
}
} // namespace cpp_study
