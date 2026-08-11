#include <string>
#include <vector>
#include <optional>

struct DirectoryIterator {
    std::string path;

    DirectoryIterator(const std::string& path);

    [[maybe_unused]] size_t SyncSizeUpdate();
    [[maybe_unused]] size_t AsyncSizeUpdate();

    std::vector<DirectoryIterator> GetSubdirs() const;

private:

    std::optional<size_t> IsFileOrCached();

    template <typename Callable, typename Int>
    void TraverseDirectory(Callable&& callback, Int& totalSize);
};
