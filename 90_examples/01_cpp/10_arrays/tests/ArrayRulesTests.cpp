#include "ArrayRules.h"
#include <iostream>
#include <stdexcept>
namespace{int C(bool v,const char*n){if(v){std::cout<<"[PASS] "<<n<<'\n';return 0;}return 1;}}
int main(){int f=0;const cpp_study::ComboDamage a{20,35,50,90};f+=C(a.size()==4,"fixed size");f+=C(cpp_study::TotalDamage(a)==195,"sum");f+=C(cpp_study::DamageAt(a,3)==90,"last index");const auto b=cpp_study::ApplyBonus(a,5);f+=C(b[0]==25&&b[3]==95,"bonus");f+=C(a[0]==20,"input unchanged");bool threw=false;try{(void)cpp_study::DamageAt(a,4);}catch(const std::out_of_range&){threw=true;}f+=C(threw,"bounds check");const cpp_study::Grid g{{{{1,2,3}},{{4,5,6}}}};f+=C(cpp_study::SumGrid(g)==21,"2d sum");std::cout<<"失敗数: "<<f<<'\n';return f==0?0:1;}
