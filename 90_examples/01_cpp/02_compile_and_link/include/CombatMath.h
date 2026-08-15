#pragma once

namespace cpp_study::combat
{
// DamageResultは計算結果と、その内訳を呼出し側へまとめて返す値Object。
struct DamageResult final
{
    int baseDamage{};
    int criticalBonus{};
    int defenseReduction{};
    int finalDamage{};
};

// Headerには関数の「宣言」を置く。
// 宣言により別の.cppは引数型、戻り値型、関数名を知ってCompileできる。
[[nodiscard]] DamageResult CalculateDamage(
    int attack,
    int defense,
    bool isCritical) noexcept;

// 同じ宣言を複数.cppがincludeしても、定義はCombatMath.cppに一つだけ置く。
[[nodiscard]] const char* BuildConfigurationName() noexcept;
} // namespace cpp_study::combat
