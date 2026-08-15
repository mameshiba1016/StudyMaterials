#include "ArrayRules.h"
#include <algorithm>
#include <numeric>

namespace cpp_study
{
int TotalDamage(const ComboDamage& values) noexcept
{
    // begin～endの全要素を0から加算する。
    return std::accumulate(values.begin(), values.end(), 0);
}

int DamageAt(const ComboDamage& values, const std::size_t index)
{
    // operator[]と違いat()は範囲外ならstd::out_of_rangeを投げる。
    return values.at(index);
}

ComboDamage ApplyBonus(const ComboDamage& values, const int bonus) noexcept
{
    ComboDamage result = values; // 入力を変更せずCopyを返す。
    for (int& value : result)
        value = std::max(value + bonus, 0);
    return result;
}

int SumGrid(const Grid& grid) noexcept
{
    int sum = 0;
    for (const auto& row : grid)
        for (const int cell : row)
            sum += cell;
    return sum;
}
} // namespace cpp_study
