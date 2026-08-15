#include "EnemyLoop.h"
#include <iostream>

int main()
{
    std::vector<cpp_study::Enemy> enemies{{"Drone", 40, true}, {"Boss", 200, true}, {"Inactive", 50, false}};
    cpp_study::ApplyAreaDamage(enemies, 50);
    std::cout << "生存Enemy数: " << cpp_study::CountLivingEnemies(enemies) << '\n';
    std::cout << "Boss Index: " << cpp_study::FindFirstBoss(enemies) << '\n';
    std::cout << "3 Tick後HP: " << cpp_study::SimulateRecoveryTicks(10, 100, 20, 3) << '\n';

    // do-whileは本体を最低一回実行する。Menu入力等に使える。
    int demonstrationCount = 0;
    do { ++demonstrationCount; } while (demonstrationCount < 1);
    std::cout << "do-while実行回数: " << demonstrationCount << '\n';
    return 0;
}
