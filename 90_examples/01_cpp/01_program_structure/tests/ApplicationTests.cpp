#include "Application.h"

#include <iostream>
#include <string>

namespace
{
// 外部Test Frameworkをまだ使わず、期待値と実値を比較する最小Test Helperを作る。
int ExpectEqual(const std::string& actual, const std::string& expected, const char* testName)
{
    if (actual == expected)
    {
        std::cout << "[PASS] " << testName << '\n';
        return 0;
    }

    std::cerr << "[FAIL] " << testName << "\n  expected: " << expected
              << "\n  actual:   " << actual << '\n';
    return 1;
}

int ExpectEqual(const int actual, const int expected, const char* testName)
{
    if (actual == expected)
    {
        std::cout << "[PASS] " << testName << '\n';
        return 0;
    }

    std::cerr << "[FAIL] " << testName << "\n  expected: " << expected
              << "\n  actual:   " << actual << '\n';
    return 1;
}
} // namespace

int main()
{
    const cpp_study::Application application;
    int failureCount = 0;

    // 正常入力、境界入力、不正入力を分けて検証する。
    failureCount += ExpectEqual(
        application.BuildGreeting("Alice"),
        "こんにちは、Alice！ C++実習を開始します。",
        "名前入りの挨拶");

    failureCount += ExpectEqual(
        application.BuildGreeting(""),
        "こんにちは、Player！ C++実習を開始します。",
        "空の名前は既定名へ変換");

    failureCount += ExpectEqual(
        cpp_study::Application::CalculateNextLevelExperience(3), 900,
        "レベル3の必要経験値");

    failureCount += ExpectEqual(
        cpp_study::Application::CalculateNextLevelExperience(0), 0,
        "レベル0は0を返す");

    std::cout << "失敗数: " << failureCount << '\n';
    // CTestは0を成功、0以外を失敗として判定する。
    return failureCount == 0 ? 0 : 1;
}
