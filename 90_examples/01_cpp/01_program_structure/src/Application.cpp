#include "Application.h"

#include <iostream>
#include <string>

namespace cpp_study
{
std::string Application::BuildGreeting(const std::string& playerName) const
{
    // Headerに宣言したMember FunctionをClass名::関数名で定義する。
    // 空文字の場合も意味のある結果にし、呼出し側が表示不能にならないようにする。
    const std::string displayName = playerName.empty() ? "Player" : playerName;

    // std::string同士を連結して新しい値を返す。Local変数は関数終了時に破棄されるが、
    // 戻り値は値として呼出し元へ移動またはCopyされるためDangling Referenceにならない。
    return "こんにちは、" + displayName + "！ C++実習を開始します。";
}

int Application::Run() const
{
    constexpr int currentLevel = 3;
    // constexpr関数へCompile時定数を渡すため、この値はCompile時に求められる。
    constexpr int requiredExperience = CalculateNextLevelExperience(currentLevel);

    std::cout << BuildGreeting("相棒") << '\n';
    std::cout << "現在レベル: " << currentLevel << '\n';
    std::cout << "次のレベルに必要な経験値: " << requiredExperience << '\n';
    std::cout << "Application::Run は正常終了しました。" << '\n';

    // 0は成功を表す終了Code。mainがこの値をOSへ返す。
    return 0;
}
} // namespace cpp_study
