#pragma once
#include <string>
#include <vector>

namespace cpp_study
{
// Objectの生成・破棄を外部Logへ記録し、Automatic Lifetimeを観察するClass。
class LifetimeTrace final
{
public:
    LifetimeTrace(std::string name, std::vector<std::string>& events);
    ~LifetimeTrace();
    LifetimeTrace(const LifetimeTrace&) = delete;
    LifetimeTrace& operator=(const LifetimeTrace&) = delete;
private:
    std::string name_;
    std::vector<std::string>* events_{}; // 呼出し元Logを非所有で借用する。
};

[[nodiscard]] int NextStaticSequence() noexcept;
[[nodiscard]] std::string BuildSafeLocalResult();
} // namespace cpp_study
