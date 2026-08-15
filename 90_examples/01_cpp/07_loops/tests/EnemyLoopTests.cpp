#include "EnemyLoop.h"
#include <iostream>

namespace { int Check(bool ok,const char* n){if(ok){std::cout<<"[PASS] "<<n<<'\n';return 0;}std::cerr<<"[FAIL] "<<n<<'\n';return 1;} }
int main()
{
    using cpp_study::Enemy;
    int f=0;
    std::vector<Enemy> enemies{{"A",30,true},{"Boss",100,true},{"Off",50,false}};
    cpp_study::ApplyAreaDamage(enemies,40);
    f+=Check(enemies[0].health==0&&!enemies[0].active,"defeat update");
    f+=Check(enemies[1].health==60,"living update");
    f+=Check(enemies[2].health==50,"continue inactive");
    f+=Check(cpp_study::CountLivingEnemies(enemies)==1,"living count");
    f+=Check(cpp_study::FindFirstBoss(enemies)==1,"find boss");
    std::vector<Enemy> none{};
    f+=Check(cpp_study::FindFirstBoss(none)==none.size(),"not found sentinel");
    f+=Check(cpp_study::SimulateRecoveryTicks(10,100,20,3)==70,"bounded while");
    f+=Check(cpp_study::SimulateRecoveryTicks(10,100,0,999)==10,"zero recovery break");
    f+=Check(cpp_study::SimulateRecoveryTicks(150,100,10,3)==100,"initial clamp");
    std::cout<<"失敗数: "<<f<<'\n';return f==0?0:1;
}
