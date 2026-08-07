#include "dir_iter.h"
#include "dir_tools.h"
#include "singleton.h"
#include "thread_pool.h"
#include <atomic>
#include <condition_variable>
#include <filesystem>
#include <functional>
#include <iostream>

DirectoryIterator::DirectoryIterator(const std::string& path)
    : path(path) {
}

[[maybe_unused]] size_t DirectoryIterator::UpdateActualSize() {
    if (IsDirCached(path)) {
        return GetCachedSize(path);
    }
    auto& pool = FirstInitSingleton<runtime::ThreadPool>::Instance().GetObj();
    if (!fs::is_directory(path)) {
        return fs::file_size(path);
    }

    std::atomic<size_t> totalSize = 0;
    
    std::condition_variable isFinished;
    std::mutex mutex;

    std::atomic<size_t> runningCalls{1};

    std::function<void(const fs::path&)> dirCoro = [&](const fs::path& path) {
        fs::directory_iterator iter(path);
        for (auto entry: iter) {
            if (entry.is_directory()) {
                runningCalls.fetch_add(1);
                
                auto poolCoro = [&, path = entry.path()] {
                    dirCoro(path);
                };
                pool.Submit(poolCoro);
            } else {
                totalSize.fetch_add(entry.file_size(), std::memory_order::relaxed);
            }
        }
        if (runningCalls.fetch_sub(1) == 1) {
            isFinished.notify_one();
        }
    };

    dirCoro(path);

    std::unique_lock<std::mutex> lock(mutex);
    isFinished.wait(lock, [&] { return runningCalls.load() == 0; });

    return totalSize.load(std::memory_order::relaxed);
}

std::vector<DirectoryIterator> DirectoryIterator::GetSubdirs() const {
    std::vector<DirectoryIterator> result;
    fs::directory_iterator iter(path);
    for (auto entry: iter) {
        result.push_back(DirectoryIterator(entry.path()));
    }
    return result;
}
