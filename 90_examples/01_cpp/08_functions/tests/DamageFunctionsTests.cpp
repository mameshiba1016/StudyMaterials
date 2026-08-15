#include "DamageFunctions.h"
#include <iostream>

namespace { int Check(bool ok,const char* n){if(ok){std::cout<<"[PASS] "<<n<<'\n';return 0;}std::cerr<<"[FAIL] "<<n<<'\n';return 1;} }
int main()
{
    int f=0;
    f+=Check(cpp_study::ClampDamage(-5)==1,"default minimum");
    f+=Check(cpp_study::ClampDamage(20000)==9999,"default maximum");
    f+=Check(cpp_study::ClampDamage(5,10,0)==5,"reversed limits");
    f+=Check(cpp_study::CalculateDamage(50,20)==30,"simple overload");
    f+=Check(cpp_study::CalculateDamage(10,50)==1,"simple minimum");
    const auto result=cpp_study::CalculateDamage({120,30,1.25,true});
    f+=Check(result.rawDamage==225,"pipeline raw");
    f+=Check(result.reducedDamage==195,"pipeline defense");
    f+=Check(result.finalDamage==195,"pipeline final");
    f+=Check(cpp_study::CalculateDamage({-10,-20,-1.0,false}).finalDamage==1,"invalid normalization");
    f+=Check(cpp_study::BuildDamageMessage("Hero",result)=="Hero dealt 195 damage.","message");
    std::cout<<"失敗数: "<<f<<'\n';return f==0?0:1;
}
