#include "LifetimeTrace.h"
#include <iostream>

int main()
{
    std::vector<std::string> events;
    {
        cpp_study::LifetimeTrace scene{"scene", events};
        {
            cpp_study::LifetimeTrace effect{"effect", events};
        }
        events.push_back("scene-update");
    }
    for (const std::string& event : events)
        std::cout << event << '\n';
    std::cout << "Static sequence: " << cpp_study::NextStaticSequence()
              << ", " << cpp_study::NextStaticSequence() << '\n';
    std::cout << cpp_study::BuildSafeLocalResult() << '\n';
    return 0;
}
