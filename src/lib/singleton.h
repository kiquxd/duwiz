#pragma once

template <typename T>
class Singleton {
public:
    Singleton(const Singleton&) = delete;
    Singleton& operator=(const Singleton&) = delete;

    Singleton(Singleton&&) = delete;
    Singleton& operator=(Singleton&&) = delete;

    static Singleton& getInstance() {
        static Singleton obj;
        return obj;
    }

    T& GetObj() {
        return obj;
    }

private:
    Singleton() = default;
    T obj;
};
