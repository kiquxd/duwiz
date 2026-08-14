#include <string>
#include <filesystem>

namespace fs = std::filesystem;

std::string formatEntrySize(size_t size);

enum class CreateResult {
    AlreadyExists, OpenOrCreateErr, Ok
};

CreateResult touch(const fs::path& path, std::string name);
CreateResult mkdir(const fs::path& path, std::string name);

std::string getFileType(const fs::path& path);
