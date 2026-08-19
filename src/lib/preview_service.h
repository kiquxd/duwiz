#pragma once

#include <preview/preview.hpp>

#include <condition_variable>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <stop_token>
#include <string>
#include <thread>

enum class PreviewStatus {
    Empty,
    Loading,
    Ready,
    Unavailable,
};

struct PreviewSnapshot {
    PreviewStatus status = PreviewStatus::Empty;
    std::filesystem::path path;
    std::string message;
    std::shared_ptr<const preview::Preview> result;
};

class PreviewService {
public:
    using Notify = std::function<void()>;

    explicit PreviewService(Notify notify);
    ~PreviewService();

    PreviewService(const PreviewService&) = delete;
    PreviewService& operator=(const PreviewService&) = delete;

    void Request(std::filesystem::path path, std::uint32_t columns,
                 std::uint32_t rows);
    void Clear();

    [[nodiscard]] std::shared_ptr<const PreviewSnapshot> Snapshot() const;

private:
    struct Job {
        std::uint64_t generation;
        std::filesystem::path path;
        std::uint32_t columns;
        std::uint32_t rows;
        std::stop_token cancellation;
    };

    void Worker(std::stop_token shutdown);
    void Publish(std::uint64_t generation, PreviewSnapshot snapshot);
    void NotifyUi() const;

    Notify notify_;
    mutable std::mutex mutex_;
    std::condition_variable_any changed_;
    std::optional<Job> pending_;
    std::stop_source active_stop_;
    std::uint64_t generation_ = 0;
    std::shared_ptr<const PreviewSnapshot> snapshot_;
    std::jthread worker_;
};
