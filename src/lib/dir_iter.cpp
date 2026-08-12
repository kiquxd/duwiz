#include "dir_iter.h"
#include "dir_tools.h"
#include "singleton.h"
#include "thread_pool.h"
#include <filesystem>
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
            totalSize += entrySize;
        }
    }
}

std::optional<size_t> DirectoryIterator::IsFileOrCached() {
    auto& cacher = Singleton<Cacher>::Instance().GetObj();
    if (cacher.IsCached(path)) {
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
    if (auto result = IsFileOrCached()) {
        return *result;
    }

    auto& pool =
        FirstInitSingleton<runtime::ThreadPool>::Instance().GetObj();
    auto& cacher = Singleton<Cacher>::Instance().GetObj();

    size_t totalSize = 0;
    std::vector<std::future<size_t>> futures;

    auto submitSubdir = [&](const auto& entry) {
        futures.push_back(
            pool.Submit([subdirPath = entry.path()] {
                DirectoryIterator iterator(subdirPath.string());
                return iterator.SyncSizeUpdate();
            })
        );
    };

    TraverseDirectory(submitSubdir, totalSize);

    for (auto& future : futures) {
        totalSize += future.get();
    }

    cacher.Update(path, totalSize);
    return totalSize;
}

std::vector<DirectoryIterator> DirectoryIterator::GetSubdirs() const {
    std::vector<DirectoryIterator> result;
    fs::directory_iterator iter(path);
    for (auto entry: iter) {
        result.push_back(DirectoryIterator(entry.path()));
    }
    return result;
}
