#pragma once

#include <cstdint>
#include <string_view>

namespace cpp_study
{
// enum class（Scoped Enumeration）は列挙子を型のScope内へ閉じ込める。
// IdleではなくActionState::Idleと書くため、別の列挙型との名前衝突を防げる。
// 基底型をuint8_tに固定すると、保存形式や通信形式を設計するときSizeが明確になる。
enum class ActionState : std::uint8_t
{
    Idle,
    Moving,
    Attacking,
    Dodging,
    Stunned
};

enum class Element : std::uint8_t
{
    Physical = 0,
    Fire = 1,
    Ice = 2,
    Electric = 3
};

// 各値を1 bitずつに割り当てると、複数の状態を一つの整数に同時保存できる。
// 例: Poison | Burning は二つの異常が同時に有効であることを表す。
enum class StatusEffect : std::uint8_t
{
    None     = 0,
    Poison   = 1U << 0U,
    Burning  = 1U << 1U,
    Frozen   = 1U << 2U,
    Shocked  = 1U << 3U
};

[[nodiscard]] std::string_view ToString(ActionState state) noexcept;
[[nodiscard]] std::string_view ToString(Element element) noexcept;
[[nodiscard]] bool CanTransition(ActionState from, ActionState to) noexcept;

// C++はenum classにbit演算を自動提供しない。
// 許可したい演算だけをOverloadし、型安全なFlag操作を実現する。
[[nodiscard]] constexpr StatusEffect operator|(StatusEffect left, StatusEffect right) noexcept
{
    return static_cast<StatusEffect>(
        static_cast<std::uint8_t>(left) | static_cast<std::uint8_t>(right));
}

[[nodiscard]] constexpr bool HasEffect(StatusEffect effects, StatusEffect target) noexcept
{
    const auto all = static_cast<std::uint8_t>(effects);
    const auto flag = static_cast<std::uint8_t>(target);
    return flag != 0U && (all & flag) == flag;
}

[[nodiscard]] constexpr StatusEffect RemoveEffect(StatusEffect effects, StatusEffect target) noexcept
{
    return static_cast<StatusEffect>(
        static_cast<std::uint8_t>(effects) & ~static_cast<std::uint8_t>(target));
}
} // namespace cpp_study

