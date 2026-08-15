#pragma once
#include <string>

namespace cpp_study
{
struct DamageInput final { int attack{0}; int defense{0}; double multiplier{1.0}; bool critical{false}; };
struct DamageResult final { int rawDamage{0}; int reducedDamage{0}; int finalDamage{0}; };

// 小さいintは値渡しする。呼出し元と独立したCopyなので変更しても影響しない。
[[nodiscard]] int ClampDamage(int damage, int minimum = 1, int maximum = 9999) noexcept;

// 同じ関数名でも引数型/個数が異なるOverloadを定義できる。
[[nodiscard]] int CalculateDamage(int attack, int defense) noexcept;
[[nodiscard]] DamageResult CalculateDamage(const DamageInput& input) noexcept;

// stringはCopy Costを避け、変更しないconst参照で借用する。
[[nodiscard]] std::string BuildDamageMessage(const std::string& attackerName, const DamageResult& result);
} // namespace cpp_study
