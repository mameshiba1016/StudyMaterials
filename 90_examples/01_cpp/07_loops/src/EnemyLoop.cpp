#include "EnemyLoop.h"
#include <algorithm>

namespace cpp_study
{
void ApplyAreaDamage(std::vector<Enemy>& enemies, const int damage) noexcept
{
    const int safeDamage = std::max(damage, 0);
    // 参照で受けるRange-based forなので、Vector内のEnemy自体を変更する。
    for (Enemy& enemy : enemies)
    {
        // inactive Enemyは残り処理を飛ばし、次要素へ進む。
        if (!enemy.active)
            continue;
        enemy.health = std::max(enemy.health - safeDamage, 0);
        if (enemy.health == 0)
            enemy.active = false;
    }
}

std::size_t CountLivingEnemies(const std::vector<Enemy>& enemies) noexcept
{
    std::size_t count = 0;
    // const参照によりCopyせず、要素を変更しない。
    for (const Enemy& enemy : enemies)
        if (enemy.active && enemy.health > 0)
            ++count;
    return count;
}

std::size_t FindFirstBoss(const std::vector<Enemy>& enemies) noexcept
{
    // Indexが必要な処理では通常forを使う。条件は必ずsize未満。
    for (std::size_t index = 0; index < enemies.size(); ++index)
    {
        if (enemies[index].active && enemies[index].name == "Boss")
            return index; // 発見時点でLoopと関数を終了する。
    }
    return enemies.size(); // sizeを「未発見」のSentinelとする。
}

int SimulateRecoveryTicks(int health, const int maxHealth, const int recoveryPerTick, const int maxTicks) noexcept
{
    health = std::clamp(health, 0, std::max(maxHealth, 0));
    const int safeRecovery = std::max(recoveryPerTick, 0);
    int tick = 0;

    // 終了条件に加えて上限を持ち、不正Dataでも無限Loopを防ぐ。
    while (health < maxHealth && tick < std::max(maxTicks, 0))
    {
        health = std::min(health + safeRecovery, maxHealth);
        ++tick;
        // 回復量0なら状態が変わらないため早期終了する。
        if (safeRecovery == 0)
            break;
    }
    return health;
}
} // namespace cpp_study
