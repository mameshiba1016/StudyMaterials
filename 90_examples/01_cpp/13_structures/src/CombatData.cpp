#include "CombatData.h"

#include <algorithm>

namespace cpp_study
{
float Vector2::LengthSquared() const noexcept
{
    // const Member関数なので、この関数内からxとyを書き換えないことをCompilerが保証する。
    // Square rootを取らない二乗距離は、大小比較だけならLengthより低Costで使える。
    return x * x + y * y;
}

DamageResult ApplyDamage(CombatStats& target, const int incomingDamage) noexcept
{
    // targetは非const参照なので、呼び出し元が持つ実体のhpを書き換える。
    // 負Damageによる意図しない回復を避け、まず0以上へClampする。
    const int rawDamage = std::max(incomingDamage, 0);
    const int appliedDamage = std::max(rawDamage - std::max(target.defense, 0), 0);
    target.hp = std::clamp(target.hp - appliedDamage, 0, std::max(target.maxHp, 0));

    // C++20のDesignated Initializer。宣言順にMember名を指定する必要がある。
    return DamageResult{
        .rawDamage = rawDamage,
        .appliedDamage = appliedDamage,
        .remainingHp = target.hp,
        .defeated = target.hp == 0
    };
}

void Move(CombatStats& actor, const Vector2& delta) noexcept
{
    // actorは変更対象、deltaは読取専用。参照により構造体全体のCopyを避ける。
    actor.position.x += delta.x;
    actor.position.y += delta.y;
}

bool IsValid(const CombatStats& stats) noexcept
{
    return !stats.name.empty() && stats.maxHp > 0 &&
           stats.hp >= 0 && stats.hp <= stats.maxHp &&
           stats.attack >= 0 && stats.defense >= 0;
}
} // namespace cpp_study

