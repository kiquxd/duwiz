#include "thread_pool.h"
#include <mutex>

namespace runtime {

// ######## Task Queue ########

[[maybe_unused]] std::optional<Task> TaskQueue::PopFront() {
    std::unique_lock lock(mutex_);
    if (queue_.empty() && !stopped_) {
        non_empty_.wait(lock, [&] { return !queue_.empty() || stopped_; });
    }

    if (queue_.empty()) {
        return std::nullopt;
    }

    Task task = queue_.front();
    queue_.pop();
    return task;
}

void TaskQueue::Push(Task task) {
    std::lock_guard guard(mutex_);
    queue_.push(std::move(task));
    non_empty_.notify_one();
}

// ######## Thread Pool ########

void ThreadPool::WorkerLoop() {
    while (true) {
        auto task = queue_.PopFront();
        if (task.has_value()) {
            (*task)();
        } else {
            break;
        }
    }
}

void ThreadPool::Start() {
    for (size_t i = 0; i < num_threads_; ++i) {
        workers_.emplace_back([&]() {
            WorkerLoop();
        });
    }
}

void ThreadPool::Stop() {
    queue_.Close();
    for (size_t i = 0; i < num_threads_; ++i) {
        if (workers_[i].joinable()) {
            workers_[i].join();
        }
    }
}

size_t ThreadPool::Threads() {
    return num_threads_;
}

}
