#pragma once
#include <xm/NonCopyable.hpp>

#include <cassert>

namespace xm {
/**
 * singleton base class, you should manual create object before use it,
 * also keep it alive when used.
 *
 * usage:
 * @code
 * class TestClass : public SingletonBase<TestClass> {
 * public:
 *     TestClass() {}
 *     void foo() {}
 * };
 *
 * TestClass instance; // manual create object and keep it alive while used.
 * TestClass::Singleton()->foo();
 * @endcode
 */
template <typename T>
class SingletonBase : public NonCopyable {
public:
    /** return singleton instance. */
    static T* Singleton() {
        return sSingletonInstance;
    }

protected:
    SingletonBase() {
        assert(!sSingletonInstance && "already instanced!!!");
        sSingletonInstance = static_cast<T*>(this);
    }

    ~SingletonBase() {
        sSingletonInstance = nullptr;
    }

private:
    static T* sSingletonInstance;
};

template <typename T>
T* SingletonBase<T>::sSingletonInstance = nullptr;
}  // namespace xm
