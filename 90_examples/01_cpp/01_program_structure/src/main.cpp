#include "Application.h"

#include <exception>
#include <iostream>

// mainはConsole Programの標準的な入口。
// OSがProgramを開始するとRuntime初期化後にmainが呼ばれる。
int main()
{
    try
    {
        // Stack上にApplicationを生成する。mainのScopeを抜けると自動的に破棄される。
        const cpp_study::Application application;

        // 実処理をApplicationへ委譲し、その終了CodeをそのままOSへ返す。
        return application.Run();
    }
    catch (const std::exception& error)
    {
        // 予期しない標準例外をProgram境界で記録する。
        // 本格的なProgramではFile LogやCrash Reportにも残す。
        std::cerr << "未処理の例外: " << error.what() << '\n';
        return 1;
    }
    catch (...)
    {
        // 型不明の例外でも異常終了Codeを返す。詳細が失われるため通常は投げない。
        std::cerr << "型を特定できない例外が発生しました。\n";
        return 2;
    }
}
