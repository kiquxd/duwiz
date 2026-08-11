#include "dir_tools.h"
#include <cassert>
#include <filesystem>
#include <mutex>

bool Cacher::IsDirCached(const fs::path& path) {
    std::lock_guard guard(mutex);

    return sizes.contains(path);
}

size_t Cacher::GetCachedSize(const fs::path& path) {
    if (fs::is_symlink(path)) {
        return 0;
    }
    if (!fs::is_directory(path)) {
        return fs::file_size(path);
    }
    assert(IsDirCached(path));

    std::lock_guard guard(mutex);

    return sizes[path];
}

void Cacher::Update(const fs::path& path, size_t size) {
    if (IsDirCached(path)) {
        return;
    }
    
    std::lock_guard guard(mutex);

    sizes[path] = size;
}

void Cacher::Invalidate(const fs::path& path) {
    std::lock_guard guard(mutex);

    sizes.erase(path);
}
