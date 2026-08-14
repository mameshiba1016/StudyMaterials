#pragma once

#include <string>

// cpp_study名前空間は、このSampleの名前を標準Libraryや別Projectの名前から分離する。
namespace cpp_study
{
// Applicationは「Programが行う処理」をまとめ、main関数を小さく保つClass。
// 宣言をHeaderへ置くことで、main.cppとTestが同じ公開Interfaceを読める。
class Application final
{
public:
    // 引数で名前を受け取り、挨拶文を値として返す。
    // const参照により呼出し元の文字列をCopyせず、constにより変更しないことを保証する。
    [[nodiscard]] std::string BuildGreeting(const std::string& playerName) const;

    // 現在Levelから次Levelに必要な経験値を計算する純粋な処理。
    // constexprなので、定数引数ならCompile時にも計算できる。
    [[nodiscard]] static constexpr int CalculateNextLevelExperience(const int currentLevel) noexcept
    {
        // 不正な0以下のLevelを0として扱い、負の経験値を返さない。
        return currentLevel > 0 ? currentLevel * currentLevel * 100 : 0;
    }

    // Program全体の通常処理を実行し、OSへ返す終了Codeを返す。
    [[nodiscard]] int Run() const;
};
} // namespace cpp_study
