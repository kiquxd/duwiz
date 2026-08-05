#include "types.h"

struct DirectoryIterator {
    std::string path;

    DirectoryIterator(const std::string& path);

    [[maybe_unused]] size_t UpdateActualSize();

    std::vector<DirectoryIterator> GetSubdirs() const;
};
