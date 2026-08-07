#include "dir_tools.h"
#include <cassert>
#include "types.h"

bool IsDirCached(const fs::path& path) {
    auto& cachedMap = CachedSize::Instance().GetObj();
    if (cachedMap.find(path) == cachedMap.end()) {
        return false;
    }
    
    auto& validMap = CachedChecker::Instance().GetObj();
    if (validMap.find(path) == validMap.end()) {
        return false;
    }
    return validMap[path];
}

size_t GetCachedSize(const fs::path &path) {
    auto& cachedMap = CachedSize::Instance().GetObj();

    assert(IsDirCached(path));

    return cachedMap[path];
}
