#include "CharacterStats.h"

#include <cstdint>
#include <iostream>
#include <limits>

int main()
{
    // autoは右辺から型を推論する。この変数の実際の型はCharacterStats。
    const auto character = cpp_study::CreateCharacterStats(
        "ActionHero", 12, 850, 7.5F, true, cpp_study::Element::Electric);

    std::cout << std::boolalpha;
    std::cout << "名前: " << character.name << '\n';
    std::cout << "Level: " << character.level << '\n';
    std::cout << "HP: " << character.health << '\n';
    std::cout << "移動速度: " << character.moveSpeed << '\n';
    std::cout << "Player操作: " << character.isPlayerControlled << '\n';
    std::cout << "属性: " << cpp_study::ToString(character.element) << '\n';

    // sizeofは型が占めるByte数を返す。固定幅整数ならData Formatを明確にしやすい。
    std::cout << "int32_tのSize: " << sizeof(std::int32_t) << " bytes\n";
    std::cout << "int32_tの最大値: "
              << std::numeric_limits<std::int32_t>::max() << '\n';
    return 0;
}
