#pragma once
#include <cstddef>
#include <string>
#include <vector>

namespace cpp_study
{
struct Enemy final { std::string name; int health{100}; bool active{true}; };

void ApplyAreaDamage(std::vector<Enemy>& enemies, int damage) noexcept;
[[nodiscard]] std::size_t CountLivingEnemies(const std::vector<Enemy>& enemies) noexcept;
[[nodiscard]] std::size_t FindFirstBoss(const std::vector<Enemy>& enemies) noexcept;
[[nodiscard]] int SimulateRecoveryTicks(int health, int maxHealth, int recoveryPerTick, int maxTicks) noexcept;
} // namespace cpp_study
