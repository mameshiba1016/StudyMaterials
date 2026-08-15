#include "LifetimeTrace.h"
#include <iostream>

namespace { int Check(bool ok,const char* n){if(ok){std::cout<<"[PASS] "<<n<<'\n';return 0;}std::cerr<<"[FAIL] "<<n<<'\n';return 1;} }
int main()
{
    int f=0;
    std::vector<std::string> e;
    {
        cpp_study::LifetimeTrace a{"A",e};
        { cpp_study::LifetimeTrace b{"B",e}; cpp_study::LifetimeTrace c{"C",e}; }
        e.push_back("body");
    }
    const std::vector<std::string> expected{"enter:A","enter:B","enter:C","exit:C","exit:B","body","exit:A"};
    f+=Check(e==expected,"reverse destruction order");
    const int first=cpp_study::NextStaticSequence();
    const int second=cpp_study::NextStaticSequence();
    f+=Check(second==first+1,"static persists between calls");
    f+=Check(cpp_study::BuildSafeLocalResult()=="local value copied or moved safely","safe value return");
    std::cout<<"失敗数: "<<f<<'\n';return f==0?0:1;
}
