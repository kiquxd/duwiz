#pragma once

#include <filesystem>
#include <mutex>

namespace fs = std::filesystem;

struct Cacher {
    std::mutex mutex;

    bool IsDirCached(const fs::path& path);
    size_t GetCachedSize(const fs::path& path);
    void Update(const fs::path& path, size_t size);
};
