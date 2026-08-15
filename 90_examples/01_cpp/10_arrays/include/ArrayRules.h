#pragma once
#include <array>
#include <cstddef>

namespace cpp_study
{
using ComboDamage = std::array<int, 4>;
using Grid = std::array<std::array<int, 3>, 2>;
[[nodiscard]] int TotalDamage(const ComboDamage& values) noexcept;
[[nodiscard]] int DamageAt(const ComboDamage& values, std::size_t index);
[[nodiscard]] ComboDamage ApplyBonus(const ComboDamage& values, int bonus) noexcept;
[[nodiscard]] int SumGrid(const Grid& grid) noexcept;
} // namespace cpp_study
