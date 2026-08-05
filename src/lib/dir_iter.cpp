#include "dir_iter.h"
#include "dir_tools.h"

DirectoryIterator::DirectoryIterator(const std::string& path)
    : path(path) {
}

[[maybe_unused]] size_t DirectoryIterator::UpdateActualSize() {
    if (IsDirCached(path)) {
        return GetCachedSize(path);
    }
    // TODO: add thread_pool integration
    size_t size = 0;
    fs::directory_iterator iter(path);
    for (auto entry: iter) {
        if (entry.is_directory()) {
            DirectoryIterator subdir(entry.path());
            size += subdir.UpdateActualSize();
        } else {
            size += entry.file_size();
        }
    }
    return size;
}

std::vector<DirectoryIterator> DirectoryIterator::GetSubdirs() const {
    std::vector<DirectoryIterator> result;
    fs::directory_iterator iter(path);
    for (auto entry: iter) {
        result.push_back(DirectoryIterator(entry.path()));
    }
    return result;
}
