#include "fs_utils.h"

#include <format>
#include <vector>
#include <fstream>

std::string formatEntrySize(size_t size) {
    if (size < 1024) {
        return std::to_string(size) + " bytes";
    }
    size *= 10;
    std::vector<std::string> suffix = {"bytes", "Kb", "Mb", "Gb", "Tb"};
    size_t index = 0;
    while (size >= 1024) {
        size /= 1024;
        ++index;
    }
    double res = static_cast<double>(size) / 10;
    return std::format("{:.1f}", res) + ' ' + suffix[index];
}

CreateResult touch(const fs::path& path, std::string name) {
    std::string fullPath(path.string() + "/" + name);
    if (fs::exists(fullPath)) {
        return CreateResult::AlreadyExists;
    }
    std::ofstream fileStream(path.string() + "/" + name);

    if (fileStream.is_open()) {
        fileStream.close();
    } else {
        return CreateResult::OpenOrCreateErr;
    }
    return CreateResult::Ok;
}

CreateResult mkdir(const fs::path& path, std::string name) {
    if (!fs::create_directory(path.string() + "/" + name)) {
        return CreateResult::AlreadyExists;
    }
    return CreateResult::Ok;
}
