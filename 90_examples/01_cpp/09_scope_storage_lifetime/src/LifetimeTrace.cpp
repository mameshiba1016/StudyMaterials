#include "LifetimeTrace.h"
#include <utility>

namespace cpp_study
{
namespace
{
// 無名Namespaceの名前はこの翻訳単位内だけで見えるInternal Linkageを持つ。
constexpr int sequenceStep = 1;
}

LifetimeTrace::LifetimeTrace(std::string name, std::vector<std::string>& events)
    : name_(std::move(name)), events_(&events)
{
    events_->push_back("enter:" + name_);
}

LifetimeTrace::~LifetimeTrace()
{
    // Scopeを抜けるとAutomatic Objectは生成と逆順で破棄される。
    if (events_ != nullptr)
        events_->push_back("exit:" + name_);
}

int NextStaticSequence() noexcept
{
    // Function-local staticは最初の呼出し時に一度だけ初期化され、Program終了まで生存する。
    static int value = 0;
    value += sequenceStep;
    return value;
}

std::string BuildSafeLocalResult()
{
    std::string local = "local value copied or moved safely";
    // Localへの参照/Pointerではなく値を返すため、関数終了後も有効。
    return local;
}
} // namespace cpp_study
