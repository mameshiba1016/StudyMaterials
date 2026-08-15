#include "TextRules.h"
#include <iostream>
#include <string>

int main()
{
    const std::string name = "相棒"; // UTF-8では2文字だが6Byte。
    std::cout << "Text: " << name << '\n';
    std::cout << "Byte数: " << name.size() << '\n';
    std::cout << "Code Point数: " << cpp_study::CountUtf8CodePoints(name) << '\n';
    std::cout << cpp_study::BuildBattleMessage(name, 125) << '\n';
    std::cout << "『棒』を含む: " << std::boolalpha << cpp_study::Contains(name, "棒") << '\n';
    return 0;
}
