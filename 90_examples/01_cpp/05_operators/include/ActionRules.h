#pragma once

#include <cstddef>

namespace cpp_study
{
struct ActionState final
{
    int stamina{100};
    int attackCost{25};
    bool isStunned{false};
    bool isGrounded{true};
    bool allowsAirAttack{false};
};

// 比較演算子と論理演算子を組み合わせ、攻撃開始条件を返す。
[[nodiscard]] bool CanStartAttack(const ActionState& state) noexcept;

// 複合代入を使ってStaminaを消費し、0未満へならないようにする。
[[nodiscard]] int ConsumeStamina(int currentStamina, int cost) noexcept;

// 剰余演算子でCombo Indexを0～comboCount-1へ循環させる。
[[nodiscard]] std::size_t NextComboIndex(
    std::size_t currentIndex,
    std::size_t comboCount) noexcept;

// 整数同士を先に割らず、doubleへ変換して正確な比率を返す。
[[nodiscard]] double CalculateHealthRatio(int health, int maxHealth) noexcept;
} // namespace cpp_study
