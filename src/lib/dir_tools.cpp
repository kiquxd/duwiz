#include "dir_tools.h"

bool IsDirCached(const fs::path& path) {
    auto& cachedDirs = CachedSize::getInstance();
    auto& cachedMap = cachedDirs.GetObj();
    if (cachedMap.find(path) == cachedMap.end()) {
        return false;
    }
    
    auto& validDirs = CachedChecker::getInstance();
    auto& validMap = validDirs.GetObj();
    if (validMap.find(path) == validMap.end()) {
        return false;
    }
    return validMap[path];
}

size_t GetCachedSize(const fs::path &path) {
    auto& cachedDirs = CachedSize::getInstance();
    auto& cachedMap = cachedDirs.GetObj();

    assert(IsDirCached(path));

    return cachedMap[path];
}
