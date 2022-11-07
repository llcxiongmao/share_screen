#pragma once
#include <llc/util/NonCopyable.hpp>

namespace llc {
namespace util {
template <typename T>
class Singleton : public NonCopyable {
public:
    static T* GetSingleton() {
        return sSingleton;
    }

protected:
    Singleton() {
        sSingleton = static_cast<T*>(this);
    }

    ~Singleton() {
        sSingleton = nullptr;
    }

private:
    static T* sSingleton;
};

template <typename T>
T* Singleton<T>::sSingleton = nullptr;
}  // namespace util
}  // namespace llc
