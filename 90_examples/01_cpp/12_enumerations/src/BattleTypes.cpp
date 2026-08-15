#include "BattleTypes.h"

namespace cpp_study
{
std::string_view ToString(const ActionState state) noexcept
{
    // switchで全列挙子を明示すると、状態追加時にCompiler警告から更新漏れを発見しやすい。
    // defaultを書かないのも、未処理の列挙子に対する警告を活用するためである。
    switch (state)
    {
    case ActionState::Idle:      return "Idle";
    case ActionState::Moving:    return "Moving";
    case ActionState::Attacking: return "Attacking";
    case ActionState::Dodging:   return "Dodging";
    case ActionState::Stunned:   return "Stunned";
    }
    return "Unknown"; // 壊れた外部DataをCastした場合にも参照可能な文字列を返す。
}

std::string_view ToString(const Element element) noexcept
{
    switch (element)
    {
    case Element::Physical: return "Physical";
    case Element::Fire:     return "Fire";
    case Element::Ice:      return "Ice";
    case Element::Electric: return "Electric";
    }
    return "Unknown";
}

bool CanTransition(const ActionState from, const ActionState to) noexcept
{
    // 同じ状態への再設定は状態遷移として扱わない。
    if (from == to)
        return false;

    // Stunned中は通常操作を受け付けず、Idleへの復帰だけを許可する。
    if (from == ActionState::Stunned)
        return to == ActionState::Idle;

    // 攻撃中はIdleへ終了するか、回避でCancelする場合だけを許可する。
    if (from == ActionState::Attacking)
        return to == ActionState::Idle || to == ActionState::Dodging;

    // この簡略Sampleでは、それ以外の異なる状態への遷移を許可する。
    return true;
}
} // namespace cpp_study

