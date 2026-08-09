#include "dir_iter.h"
#include "dir_tools.h"
#include "singleton.h"
#include "thread_pool.h"
#include "types.h"
#include <atomic>
#include <filesystem>
#include <system_error>

DirectoryIterator::DirectoryIterator(const std::string& path)
    : path(path) {
}

[[maybe_unused]] size_t DirectoryIterator::UpdateActualSize() {
    Cacher cacher;
    if (cacher.IsDirCached(path)) {
        return cacher.GetCachedSize(path);
    }
    auto& pool = FirstInitSingleton<runtime::ThreadPool>::Instance().GetObj();
    if (!fs::is_directory(path)) {
        return fs::file_size(path);
    }

    std::atomic<size_t> totalSize = 0;
    
    std::mutex mutex;

    std::atomic<size_t> runningCalls{0};

    std::error_code ec;
    fs::directory_iterator iter(path, ec);
    if (ec) {
        return 0;
    }

    for (auto entry: iter) {
        if (entry.is_directory()) {
            runningCalls.fetch_add(1, std::memory_order::relaxed);
            pool.Submit([&, subdirPath = entry.path()] {
                DirectoryIterator subdirIter(subdirPath);
                totalSize.fetch_add(subdirIter.UpdateActualSize());
                runningCalls.fetch_sub(1, std::memory_order::relaxed);
            });
        } else {
            size_t entrySize;
            try {
                entrySize = entry.file_size();
            } catch (...) {
                entrySize = 0;
            }
            totalSize.fetch_add(entrySize);
        }
    }

    pool.HelpUntil([&] { return runningCalls.load(std::memory_order::relaxed) == 0; });

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
