#pragma once

#include <utility>
#include <cassert>

template <typename T>
class Singleton {
public:
    Singleton(const Singleton&) = delete;
    Singleton& operator=(const Singleton&) = delete;

    Singleton(Singleton&&) = delete;
    Singleton& operator=(Singleton&&) = delete;

    static Singleton& Instance() {
        static Singleton instance;
        return instance;
    }

    T& GetObj() {
        return obj_;
    }

private:
    Singleton() = default;
    T obj_;
};


template <typename T>
class FirstInitSingleton {
public:
    FirstInitSingleton(const FirstInitSingleton&) = delete;
    FirstInitSingleton& operator=(const FirstInitSingleton&) = delete;

    FirstInitSingleton(FirstInitSingleton&&) = delete;
    FirstInitSingleton& operator=(FirstInitSingleton&&) = delete;

    static FirstInitSingleton& Instance() {
        assert(obj_ != nullptr && "FirstInitSingleton used uninitialized");
        static FirstInitSingleton instance;
        return instance;
    }

    template <typename... Args>
    static void Init(Args&&... args) {
        assert(obj_ == nullptr && "FirstInitSingleton initialized more than once");
        obj_ = new T(std::forward<Args>(args)...);
    }

    T& GetObj() {
        return *obj_;
    }

private:
    FirstInitSingleton() = default;
    static inline T* obj_ = nullptr;
};
