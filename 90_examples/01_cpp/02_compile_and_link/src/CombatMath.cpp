#include "CombatMath.h"

#include <algorithm>

namespace cpp_study::combat
{
DamageResult CalculateDamage(
    const int attack,
    const int defense,
    const bool isCritical) noexcept
{
    // 外部入力が負でも計算を壊さないよう、Game上の最小値0へ正規化する。
    const int safeAttack = std::max(attack, 0);
    const int safeDefense = std::max(defense, 0);

    // Critical時は基礎Attackの50%をBonusとする。整数除算なので小数部は切り捨てられる。
    const int criticalBonus = isCritical ? safeAttack / 2 : 0;

    // 防御力はそのままDamageから引く。この式は学習用であり、実際のGameでは
    // Level差、属性、Buff、丸め順、Overflow等も仕様として固定する必要がある。
    const int beforeClamp = safeAttack + criticalBonus - safeDefense;

    // Hitしたのに0以下にならない仕様として、最終Damageを最低1にClampする。
    const int finalDamage = std::max(beforeClamp, 1);

    return DamageResult{
        .baseDamage = safeAttack,
        .criticalBonus = criticalBonus,
        .defenseReduction = safeDefense,
        .finalDamage = finalDamage,
    };
}

const char* BuildConfigurationName() noexcept
{
    // NDEBUGは一般にRelease構成で定義される。PreprocessorによりCompileされる
    // Branch自体が変わり、Runtimeのif文とは違って片方だけがObject Codeへ入る。
#ifdef NDEBUG
    return "Release";
#else
    return "Debug";
#endif
}
} // namespace cpp_study::combat
