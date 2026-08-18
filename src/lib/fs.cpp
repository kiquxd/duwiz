#include "fs.h"
#include "singleton.h"
#include <filesystem>
#include <stop_token>
#include <system_error>
#include <format>
#include <vector>
#include <fstream>

#include <iostream>
#include <string>
#include <magic.h>

DirectoryIterator::DirectoryIterator(const std::string& path)
    : path(path) {
}

std::optional<size_t> DirectoryIterator::IsFileOrCached() {
    auto& cacher = Singleton<Cacher>::Instance().GetObj();
    if (cacher.IsCached(path)) {
        return cacher.GetCachedSize(path);
    }
    return std::nullopt;
}

[[maybe_unused]] ScanResult DirectoryIterator::SyncSizeUpdate(std::stop_token token) {
    if (token.stop_requested()) {
        return ScanResult::Cancelled();
    }
    if (auto res = IsFileOrCached(); res.has_value()) {
        return ScanResult::Ready(res.value());
    }

    size_t totalSize = 0;
    std::error_code ec;
    fs::directory_iterator iter(path, ec);
    if (ec) {
        return ScanResult::Error();
    }

    for (auto entry: iter) {
        if (token.stop_requested()) {
            return ScanResult::Cancelled();
        }

        if (entry.is_symlink(ec)) {
            continue;
        }
        if (ec) {
            return ScanResult::Error();
        }

        if (entry.is_directory(ec)) {
            DirectoryIterator subdirIter(entry.path());
            auto res = subdirIter.SyncSizeUpdate(token);
            if (res.IsReady()) {
                totalSize += res.Get();
            } else if (res.IsCancelled()) {
                return ScanResult::Cancelled();
            }
        } else {
            size_t entrySize;
            try {
                entrySize = entry.file_size();
            } catch (...) {
                entrySize = 0;
            }
            totalSize += entrySize;
        }

        if (ec) {
            return ScanResult::Error();
        }
    }

    auto& cacher = Singleton<Cacher>::Instance().GetObj();
    cacher.Update(path, totalSize);

    return ScanResult::Ready(totalSize);
}

std::vector<DirectoryIterator> DirectoryIterator::GetSubdirs() const {
    std::vector<DirectoryIterator> result;
    fs::directory_iterator iter(path);
    for (auto entry: iter) {
        result.push_back(DirectoryIterator(entry.path()));
    }
    return result;
}

bool Cacher::IsCached(const fs::path& path) {
    try {
        if (fs::is_symlink(path)) {
            return true;
        }
        if (!fs::is_directory(path)) {
            return true;
        }
    } catch (...) {}

    std::lock_guard guard(mutex);

    return sizes.contains(path);
}

size_t Cacher::GetCachedSize(const fs::path& path) {
    assert(IsCached(path));

    try {
        if (fs::is_symlink(path)) {
            return 0;
        }
        if (!fs::is_directory(path)) {
            return fs::file_size(path);
        }
    } catch (...) {}

    std::lock_guard guard(mutex);

    return sizes[path];
}

void Cacher::Update(const fs::path& path, size_t size) {
    if (IsCached(path)) {
        return;
    }
    
    std::lock_guard guard(mutex);

    sizes[path] = size;
}

void Cacher::Invalidate(const fs::path& path) {
    std::lock_guard guard(mutex);

    sizes.erase(path);
}


std::string formatEntrySize(size_t size) {
    if (size < 1024) {
        return std::to_string(size) + " bytes";
    }
    double outRes = static_cast<double>(size);
    std::vector<std::string> suffix = {"bytes", "Kb", "Mb", "GiB", "Tb"};
    size_t index = 0;
    while (outRes >= 1024) {
        outRes /= 1024;
        ++index;
    }
    return std::format("{:.1f}", outRes) + ' ' + suffix[index];
}

std::string formatEntryStatus(size_t doneDirs, size_t totalDirs, size_t curSize) {
    if (doneDirs == totalDirs) {
        return "finished • size: " + formatEntrySize(curSize);
    }
    return "loading " + std::to_string(doneDirs) + " / " + std::to_string(totalDirs) + " • partial: " + formatEntrySize(curSize);
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
