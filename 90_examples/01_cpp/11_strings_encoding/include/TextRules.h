#pragma once
#include <cstddef>
#include <string>
#include <string_view>

namespace cpp_study
{
[[nodiscard]] std::size_t CountUtf8CodePoints(std::string_view text) noexcept;
[[nodiscard]] bool Contains(std::string_view text, std::string_view keyword) noexcept;
[[nodiscard]] std::string BuildBattleMessage(std::string_view actor, int damage);
[[nodiscard]] std::string TrimAsciiSpaces(std::string_view text);
} // namespace cpp_study
