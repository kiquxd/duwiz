#include "fs_utils.h"

#include <format>
#include <vector>
#include <fstream>

#include <iostream>
#include <string>
#include <magic.h>

std::string exec_command(const std::string& cmd) {
    char buffer[128];
    std::string result;
    // Открываем пайп на чтение (скрытый запуск процесса)
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
    if (!pipe) return "Ошибка выполнения команды.";
    
    while (fgets(buffer, sizeof(buffer), pipe.get()) != nullptr) {
        result += buffer;
    }
    return result;
}

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

std::string getFileType(const fs::path& path) {
    magic_t magic_cookie = magic_open(MAGIC_MIME_TYPE);
    
    if (magic_cookie == nullptr) {
        std::cerr << "Failed to initialize libmagic." << std::endl;
        return "err";
    }

    if (magic_load(magic_cookie, nullptr) != 0) {
        std::cerr << "Cannot load magic database: " << magic_error(magic_cookie) << std::endl;
        magic_close(magic_cookie);
        return "err";
    }

    const char* mimeRaw = magic_file(magic_cookie, path.c_str());
    
    if (mimeRaw == nullptr) {
        std::cerr << "Error identifying file: " 
                  << magic_error(magic_cookie) << std::endl;
        return "err";
    }

    std::string mimeType(mimeRaw);

    magic_close(magic_cookie);
    return mimeType;
}
