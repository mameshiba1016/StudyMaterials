#include "Cooldown.h"

#include <iostream>

int main()
{
    const cpp_study::CooldownInput input{
        .baseSeconds = 8.0,
        .elapsedSeconds = 3.0,
        .recoveryRate = 1.5,
        .paused = false,
    };

    const double remaining = cpp_study::CalculateRemainingCooldown(input);
    std::cout << "基礎Cooldown: " << input.baseSeconds << " 秒\n";
    std::cout << "経過時間: " << input.elapsedSeconds << " 秒\n";
    std::cout << "回復倍率: " << input.recoveryRate << '\n';
    std::cout << "残り時間: " << remaining << " 秒\n";
    std::cout << "使用可能: " << std::boolalpha
              << cpp_study::IsSkillReady(remaining) << '\n';
    return 0;
}
