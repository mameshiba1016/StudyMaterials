#include "TextRules.h"
#include <algorithm>

namespace cpp_study
{
std::size_t CountUtf8CodePoints(const std::string_view text) noexcept
{
    std::size_t count = 0;
    for (const unsigned char byte : text)
    {
        // UTF-8のContinuation Byteはbit Pattern 10xxxxxx。
        // それ以外をCode Pointの開始Byteとして数える。
        if ((byte & 0xC0U) != 0x80U)
            ++count;
    }
    return count;
}

bool Contains(const std::string_view text, const std::string_view keyword) noexcept
{
    return text.find(keyword) != std::string_view::npos;
}

std::string BuildBattleMessage(const std::string_view actor, const int damage)
{
    // string_viewは所有しないため、結果を長期保持する場合はstd::stringへCopyする。
    const std::string safeActor = actor.empty() ? "Unknown" : std::string{actor};
    return safeActor + " dealt " + std::to_string(std::max(damage, 0)) + " damage.";
}

std::string TrimAsciiSpaces(const std::string_view text)
{
    const auto first = text.find_first_not_of(' ');
    if (first == std::string_view::npos)
        return {};
    const auto last = text.find_last_not_of(' ');
    return std::string{text.substr(first, last - first + 1)};
}
} // namespace cpp_study
