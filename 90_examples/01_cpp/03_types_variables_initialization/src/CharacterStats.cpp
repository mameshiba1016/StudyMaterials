#include "CharacterStats.h"

#include <algorithm>
#include <utility>

namespace cpp_study
{
CharacterStats CreateCharacterStats(
    std::string name,
    const std::int32_t level,
    const std::int32_t health,
    const float moveSpeed,
    const bool isPlayerControlled,
    const Element element)
{
    // std::moveで引数nameが所有する文字列Bufferを戻り値へ移せるようにする。
    // 移動後のnameは有効だが内容未規定なので、この後は読まない。
    return CharacterStats{
        .name = name.empty() ? "Unknown" : std::move(name),
        .level = std::clamp<std::int32_t>(level, 1, 100),
        .health = std::max<std::int32_t>(health, 0),
        .moveSpeed = std::max(moveSpeed, 0.0F),
        .accumulatedPlaySeconds = 0.0,
        .isPlayerControlled = isPlayerControlled,
        .element = element,
    };
}

const char* ToString(const Element element) noexcept
{
    switch (element)
    {
    case Element::Physical: return "Physical";
    case Element::Fire: return "Fire";
    case Element::Ice: return "Ice";
    case Element::Electric: return "Electric";
    }

    // Memory破損や不正Cast等で未定義Enum値が来てもNull Pointerを返さない。
    return "Unknown";
}
} // namespace cpp_study
