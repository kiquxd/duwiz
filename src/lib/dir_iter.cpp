#include "dir_iter.h"
#include "dir_tools.h"
#include "singleton.h"
#include "thread_pool.h"
#include <atomic>
#include <condition_variable>
#include <filesystem>
#include <mutex>
#include <system_error>

DirectoryIterator::DirectoryIterator(const std::string& path)
    : path(path) {
}

template <typename Callable, typename Int>
void DirectoryIterator::TraverseDirectory(Callable&& callback, Int& totalSize) {
    std::error_code ec;
    fs::directory_iterator iter(path, ec);
    if (ec) {
        return;
    }

    for (auto entry: iter) {
        if (fs::is_symlink(entry.path())) {
            continue;
        }

        if (entry.is_directory()) {
            callback(entry);
        } else {
            size_t entrySize;
            try {
                entrySize = entry.file_size();
            } catch (...) {
                entrySize = 0;
            }
            if constexpr (requires { totalSize.fetch_add(entrySize); }) {
                totalSize.fetch_add(entrySize, std::memory_order::relaxed);            
            } else {
                totalSize += entrySize;
            }
        }
    }
}

std::optional<size_t> DirectoryIterator::IsFileOrCached() {
    if (fs::is_symlink(path)) {
        return 0;
    }
    if (!fs::is_directory(path)) {
        return fs::file_size(path);
    }

    auto& cacher = Singleton<Cacher>::Instance().GetObj();
    if (cacher.IsDirCached(path)) {
        return cacher.GetCachedSize(path);
    }
    return std::nullopt;
}

[[maybe_unused]] size_t DirectoryIterator::SyncSizeUpdate() {
    if (auto res = IsFileOrCached(); res.has_value()) {
        return res.value();
    }

    size_t totalSize = 0;
    auto callback = [&](auto entry) {
        DirectoryIterator dirIter(entry.path());
        totalSize += dirIter.SyncSizeUpdate();
    };

    TraverseDirectory(std::move(callback), totalSize);

    auto& cacher = Singleton<Cacher>::Instance().GetObj();
    cacher.Update(path, totalSize);

    return totalSize;
}

[[maybe_unused]] size_t DirectoryIterator::AsyncSizeUpdate() {
    if (auto res = IsFileOrCached(); res.has_value()) {
        return res.value();
    }

    auto& pool = FirstInitSingleton<runtime::ThreadPool>::Instance().GetObj();
    auto& cacher = Singleton<Cacher>::Instance().GetObj();

    std::atomic<size_t> totalSize = 0;
    
    std::mutex mutex;
    std::condition_variable isFinished;

    std::atomic<size_t> runningCalls{0};

    std::error_code ec;
    fs::directory_iterator iter(path, ec);
    if (ec) {
        return 0;
    }

    auto callback = [&](auto entry) {
        runningCalls.fetch_add(1, std::memory_order::relaxed);
        pool.Submit([&, subdirPath = entry.path()] {
            DirectoryIterator subdirIter(subdirPath);
            totalSize.fetch_add(subdirIter.SyncSizeUpdate());
            if (runningCalls.fetch_sub(1, std::memory_order::relaxed) == 1) {
                isFinished.notify_one();
            }
        });
    };

    TraverseDirectory(std::move(callback), totalSize);

    std::unique_lock lock(mutex);
    isFinished.wait(lock, [&] { return runningCalls.load(std::memory_order::relaxed) == 0; });

    cacher.Update(path, totalSize.load());

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
