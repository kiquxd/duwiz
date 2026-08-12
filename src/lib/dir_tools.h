#pragma once

#include <filesystem>
#include <mutex>
#include <string>
#include <unordered_map>

namespace fs = std::filesystem;

struct Cacher {
    std::mutex mutex;
    std::unordered_map<std::string, size_t> sizes;

    bool IsCached(const fs::path& path);
    size_t GetCachedSize(const fs::path& path);
    void Update(const fs::path& path, size_t size);
    void Invalidate(const fs::path& path);
};
