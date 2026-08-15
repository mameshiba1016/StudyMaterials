#pragma once

#include <cstdint>
#include <string>

namespace cpp_study
{
// enum classは整数に意味のある名前を付け、別Enumやintとの誤混同を防ぐ。
enum class Element : std::uint8_t
{
    Physical,
    Fire,
    Ice,
    Electric
};

// CharacterStatsは異なる型の値を一つの意味ある単位へまとめる。
struct CharacterStats final
{
    // {}を使うMember初期化子により、Default構築しても未初期化値を残さない。
    std::string name{"Unknown"};
    std::int32_t level{1};
    std::int32_t health{100};
    float moveSpeed{5.0F};
    double accumulatedPlaySeconds{0.0};
    bool isPlayerControlled{false};
    Element element{Element::Physical};
};

// 入力値をGameの有効範囲へ正規化したCharacterStatsを値として返す。
[[nodiscard]] CharacterStats CreateCharacterStats(
    std::string name,
    std::int32_t level,
    std::int32_t health,
    float moveSpeed,
    bool isPlayerControlled,
    Element element);

[[nodiscard]] const char* ToString(Element element) noexcept;
} // namespace cpp_study
