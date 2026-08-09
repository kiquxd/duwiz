#include "dir_tools.h"
#include <cassert>
#include <mutex>
#include "types.h"

bool Cacher::IsDirCached(const fs::path& path) {
    std::lock_guard guard(mutex);

    auto& validMap = CachedChecker::Instance().GetObj();
    if (validMap.find(path) == validMap.end()) {
        return false;
    }
    return validMap[path];
}

size_t Cacher::GetCachedSize(const fs::path& path) {
    assert(IsDirCached(path));
    
    auto& cachedMap = CachedSize::Instance().GetObj();
    std::lock_guard guard(mutex);

    return cachedMap[path];
}

void Cacher::Update(const fs::path& path, size_t size) {
    if (IsDirCached(path)) {
        return;
    }
    
    std::lock_guard guard(mutex);

    auto& cachedMap = CachedSize::Instance().GetObj();
    cachedMap[path] = size;

    auto& validMap = CachedChecker::Instance().GetObj();
    validMap[path] = true;
}
