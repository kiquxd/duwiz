#pragma once

#include <functional>
#include <mutex>
#include <optional>
#include <queue>
#include <thread>
#include <vector>
#include <cassert>
#include <atomic>

namespace runtime {

using Task = std::function<void()>;

class TaskQueue {
private:
    mutable std::mutex mutex_;
    std::queue<Task> queue_;

public:
    [[maybe_unused]] std::optional<Task> TryPopFront();

    void Push(Task task);

    bool Empty() const {
        std::lock_guard guard(mutex_);
        return queue_.empty();
    }

    bool NonEmpty() const {
        return !Empty();
    }
};

class ThreadPool {
private:
    std::vector<std::thread> workers_;
    size_t num_threads_;
    std::atomic<size_t> stop_calls_;
    TaskQueue queue_;

    void WorkerLoop();

public:
    ThreadPool(size_t num_threads)
        : num_threads_(num_threads)
        , stop_calls_(0) {
        assert(num_threads > 0); 
    }

    void Submit(Task task);

    void Start();

    template <typename Predicate>
    void HelpUntil(Predicate&& done) {
        size_t retries = 0;
        while (!std::invoke(done)) {
            auto task = queue_.TryPopFront();
            if (task.has_value()) {
                (*task)();
            } else if (++retries == 5) {
                std::this_thread::yield();
                retries = 0;
            }
        }
    }

    void Stop();

    ~ThreadPool() {
        if (workers_.size() > 0) {
            assert(stop_calls_.load() == 1);
        }
    }
};

} // namespace runtime
