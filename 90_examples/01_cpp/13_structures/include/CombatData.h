#pragma once

#include <string>

namespace cpp_study
{
// structは関連するDataへ名前を付け、一つの型としてまとめる仕組み。
// C++のstructはDataだけでなく、Member関数、Constructor、継承も持てる。
struct Vector2
{
    // Member初期化子により、Vector2 position; と書いても未初期化値にならない。
    float x{0.0F};
    float y{0.0F};

    [[nodiscard]] float LengthSquared() const noexcept;
};

// Publicな単純Dataとしてまとめ、外部で自由に編集する設計例。
// 不変条件を常に守らせたい型は、private Memberを持つclassの方が適する場合がある。
struct CombatStats
{
    std::string name{"Unknown"};
    int maxHp{1};
    int hp{1};
    int attack{0};
    int defense{0};
    Vector2 position{};
};

// 一つの関数から複数の意味ある結果を返すためのResult Object。
// boolやintを個別に返すより、各値の意味をMember名で表現できる。
struct DamageResult
{
    int rawDamage{0};
    int appliedDamage{0};
    int remainingHp{0};
    bool defeated{false};
};

[[nodiscard]] DamageResult ApplyDamage(CombatStats& target, int incomingDamage) noexcept;
void Move(CombatStats& actor, const Vector2& delta) noexcept;
[[nodiscard]] bool IsValid(const CombatStats& stats) noexcept;
} // namespace cpp_study

