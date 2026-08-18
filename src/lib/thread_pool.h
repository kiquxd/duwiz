#pragma once

#include <condition_variable>
#include <functional>
#include <mutex>
#include <optional>
#include <queue>
#include <thread>
#include <vector>
#include <cassert>
#include <atomic>
#include <future>

namespace runtime {

using Task = std::function<void()>;

class TaskQueue {
private:
    mutable std::mutex mutex_;
    std::queue<Task> queue_;
    std::condition_variable non_empty_;
    bool stopped_ = false;

public:
    [[maybe_unused]] std::optional<Task> PopFront();

    void Push(Task task);

    bool Empty() const {
        std::lock_guard guard(mutex_);
        return queue_.empty();
    }

    bool NonEmpty() const {
        return !Empty();
    }

    bool IsClosed() const {
        std::lock_guard guard(mutex_);
        return stopped_;
    }

    void Close() {
        std::lock_guard guard(mutex_);
        stopped_ = true;
        non_empty_.notify_all();
    }
};

class ThreadPool {
private:
    std::vector<std::thread> workers_;
    size_t num_threads_;
    TaskQueue queue_;

    void WorkerLoop();

public:
    ThreadPool(size_t num_threads)
        : num_threads_(num_threads) {
        assert(num_threads > 0); 
    }

    template <typename F>
    auto Submit(F&& function) -> std::future<std::invoke_result_t<F>> {
        using Result = std::invoke_result_t<F>;
        auto task = std::make_shared<std::packaged_task<Result()>>(
            std::forward<F>(function)
        );
        auto future = task->get_future();
        queue_.Push([task] {
            (*task)();
        });
        return future;
    }

    void Start();

    template <typename Predicate>
    void HelpUntil(Predicate&& done) {
        size_t retries = 0;
        while (!std::invoke(done)) {
            auto task = queue_.PopFront();
            if (task.has_value()) {
                (*task)();
            } else if (++retries == 5) {
                std::this_thread::yield();
                retries = 0;
            }
        }
    }

    void Stop();

    size_t Threads();

    ~ThreadPool() {
        if (workers_.size() > 0) {
            assert(queue_.IsClosed());
        }
    }
};

} // namespace runtime
