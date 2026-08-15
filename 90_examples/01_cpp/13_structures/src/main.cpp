#include "CombatData.h"

#include <iostream>

int main()
{
    // { }によるAggregate初期化。Memberの宣言順に値を対応させる。
    cpp_study::CombatStats enemy{
        "Training Drone", 100, 100, 18, 5, cpp_study::Vector2{2.0F, 3.0F}
    };

    cpp_study::Move(enemy, cpp_study::Vector2{1.5F, -1.0F});
    const cpp_study::DamageResult result = cpp_study::ApplyDamage(enemy, 32);

    std::cout << "名前: " << enemy.name << '\n';
    std::cout << "位置: (" << enemy.position.x << ", " << enemy.position.y << ")\n";
    std::cout << "入力Damage: " << result.rawDamage << '\n';
    std::cout << "適用Damage: " << result.appliedDamage << '\n';
    std::cout << "残りHP: " << result.remainingHp << '\n';
    std::cout << "撃破: " << std::boolalpha << result.defeated << '\n';
    std::cout << "Data有効: " << cpp_study::IsValid(enemy) << '\n';
    return 0;
}

