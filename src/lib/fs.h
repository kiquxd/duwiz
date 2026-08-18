#pragma once

#include <filesystem>
#include <mutex>
#include <stop_token>
#include <unordered_map>
#include <string>
#include <vector>
#include <optional>

namespace fs = std::filesystem;

struct ScanSession;
struct ScanResult;

struct DirectoryIterator {
    std::string path;

    DirectoryIterator(const std::string& path);

    [[maybe_unused]] ScanResult SyncSizeUpdate(std::stop_token token);
    [[maybe_unused]] size_t AsyncSizeUpdate();

    std::vector<DirectoryIterator> GetSubdirs() const;

private:

    std::optional<size_t> IsFileOrCached();

};

struct ScanResult {
private:
    enum class ScanResultEnum {
        Ready,
        Cancelled,
        Error
    };

    size_t size;
    ScanResultEnum result = ScanResultEnum::Cancelled;

    ScanResult(size_t size)
        : size(size)
        , result(ScanResultEnum::Ready) {
    }

    ScanResult()
        : size(0)
        , result(ScanResultEnum::Cancelled) {
    }
public:

    bool IsReady() {
        return result == ScanResultEnum::Ready;
    }

    bool IsCancelled() {
        return result == ScanResultEnum::Cancelled;
    }

    size_t Get() {
        if (result == ScanResultEnum::Ready) {
            return size;
        } else {
            // potential throw
            return 0;
        }
    }

    static ScanResult Ready(size_t size) {
        return ScanResult(size);
    }

    static ScanResult Cancelled() {
        auto res = ScanResult();
        res.result = ScanResultEnum::Cancelled;
        return res;
    }

    static ScanResult Error() {
        auto res = ScanResult();
        res.result = ScanResultEnum::Error;
        return res;
    }
};

struct SizeUpdate {
    uint64_t generation;
    size_t index;
    ScanResult result;
};

struct ScanSession {
    std::stop_source stopSource;
    size_t generation{0};
    std::atomic<int64_t> nextIndex{0};
    std::atomic<size_t> readyEntries{0};
    std::atomic<size_t> finishedEntries{0};
    std::atomic<size_t> totalSize{0};
    std::vector<fs::path> directEntries;

    void Publish(size_t index, ScanResult result);

    ScanSession(uint64_t generation, std::vector<fs::path> entries)
          : generation(generation)
          , directEntries(std::move(entries)) {}
};

struct Cacher {
private:
    std::mutex mutex;
    std::unordered_map<std::string, size_t> sizes;

public:
    bool IsCached(const fs::path& path);
    size_t GetCachedSize(const fs::path& path);
    void Update(const fs::path& path, size_t size);
    void Invalidate(const fs::path& path);
};

std::string formatEntrySize(size_t size);
std::string formatEntryStatus(size_t doneDirs, size_t totalDirs, size_t curSize);

enum class CreateResult {
    AlreadyExists, OpenOrCreateErr, Ok
};

CreateResult touch(const fs::path& path, std::string name);
CreateResult mkdir(const fs::path& path, std::string name);

std::string getFileType(const fs::path& path);
