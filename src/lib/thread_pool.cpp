#include "thread_pool.h"

namespace runtime {

// ######## Task Queue ########

[[maybe_unused]] std::optional<Task> TaskQueue::TryPopFront() {
    std::lock_guard guard(mutex_);
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
}

// ######## Thread Pool ########

void ThreadPool::WorkerLoop() {
    while (stop_calls_.load() == 0) {
        auto task = queue_.TryPopFront();
        if (task.has_value()) {
            (*task)();
        }
    }
}

void ThreadPool::Submit(Task task) {
    queue_.Push(std::move(task));
}

void ThreadPool::Start() {
    for (size_t i = 0; i < num_threads_; ++i) {
        workers_.emplace_back([&]() {
            WorkerLoop();
        });
    }
}

void ThreadPool::Stop() {
    stop_calls_.fetch_add(1);
    for (size_t i = 0; i < num_threads_; ++i) {
        if (workers_[i].joinable()) {
            workers_[i].join();
        }
    }
}

}
