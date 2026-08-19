#include "preview_service.h"

#include <algorithm>
#include <chrono>
#include <limits>
#include <system_error>
#include <utility>

namespace {

std::string ErrorMessage(const preview::Error& error) {
    return error.message.empty() ? "preview backend failed" : error.message;
}

}  // namespace

PreviewService::PreviewService(Notify notify)
    : notify_(std::move(notify))
    , snapshot_(std::make_shared<PreviewSnapshot>())
    , worker_([this](std::stop_token shutdown) { Worker(shutdown); }) {
}

PreviewService::~PreviewService() {
    active_stop_.request_stop();
    worker_.request_stop();
    changed_.notify_all();
}

void PreviewService::Request(std::filesystem::path path,
                             std::uint32_t columns,
                             std::uint32_t rows) {
    {
        std::lock_guard guard(mutex_);
        active_stop_.request_stop();
        active_stop_ = std::stop_source{};
        ++generation_;

        columns = std::max<std::uint32_t>(columns, 1);
        rows = std::max<std::uint32_t>(rows, 1);
        pending_ = Job{generation_, std::move(path), columns, rows,
                       active_stop_.get_token()};
        snapshot_ = std::make_shared<PreviewSnapshot>(PreviewSnapshot{
            .status = PreviewStatus::Loading,
            .path = pending_->path,
            .message = "Loading preview...",
            .result = nullptr,
        });
    }
    changed_.notify_all();
    NotifyUi();
}

void PreviewService::Clear() {
    {
        std::lock_guard guard(mutex_);
        active_stop_.request_stop();
        pending_.reset();
        ++generation_;
        snapshot_ = std::make_shared<PreviewSnapshot>();
    }
    changed_.notify_all();
    NotifyUi();
}

std::shared_ptr<const PreviewSnapshot> PreviewService::Snapshot() const {
    std::lock_guard guard(mutex_);
    return snapshot_;
}

void PreviewService::Publish(std::uint64_t generation,
                             PreviewSnapshot snapshot) {
    {
        std::lock_guard guard(mutex_);
        if (generation != generation_) {
            return;
        }
        snapshot_ = std::make_shared<PreviewSnapshot>(std::move(snapshot));
    }
    NotifyUi();
}

void PreviewService::NotifyUi() const {
    if (notify_) {
        notify_();
    }
}

void PreviewService::Worker(std::stop_token shutdown) {
    auto engine_result = preview::Engine::create();

    while (!shutdown.stop_requested()) {
        std::optional<Job> job;
        {
            std::unique_lock lock(mutex_);
            changed_.wait(lock, shutdown,
                          [this] { return pending_.has_value(); });
            if (shutdown.stop_requested()) {
                return;
            }
            job = std::move(pending_);
            pending_.reset();
        }

        // A small debounce prevents decoding every intermediate selection while
        // the user holds an arrow key.
        {
            std::unique_lock lock(mutex_);
            changed_.wait_for(lock, shutdown, std::chrono::milliseconds(60),
                              [&job] { return job->cancellation.stop_requested(); });
        }
        if (shutdown.stop_requested()) {
            return;
        }
        if (job->cancellation.stop_requested()) {
            continue;
        }

        std::error_code status_error;
        const auto status = std::filesystem::symlink_status(job->path, status_error);
        if (status_error) {
            Publish(job->generation, PreviewSnapshot{
                .status = PreviewStatus::Unavailable,
                .path = job->path,
                .message = "Cannot inspect file: " + status_error.message(),
                .result = nullptr,
            });
            continue;
        }
        if (!std::filesystem::is_regular_file(status)) {
            std::string kind = "Special filesystem entry";
            if (std::filesystem::is_directory(status)) kind = "Directory";
            if (std::filesystem::is_symlink(status)) kind = "Symbolic link";
            Publish(job->generation, PreviewSnapshot{
                .status = PreviewStatus::Unavailable,
                .path = job->path,
                .message = std::move(kind),
                .result = nullptr,
            });
            continue;
        }
        if (!engine_result) {
            Publish(job->generation, PreviewSnapshot{
                .status = PreviewStatus::Unavailable,
                .path = job->path,
                .message = ErrorMessage(engine_result.error()),
                .result = nullptr,
            });
            continue;
        }

        auto source_result = preview::open_local_file(job->path);
        if (!source_result) {
            Publish(job->generation, PreviewSnapshot{
                .status = PreviewStatus::Unavailable,
                .path = job->path,
                .message = ErrorMessage(source_result.error()),
                .result = nullptr,
            });
            continue;
        }

        preview::Request request;
        request.stop_token = job->cancellation;
        request.pixel_format = preview::PixelFormat::rgba8;
        request.viewport.text_columns = job->columns;
        request.viewport.text_rows = job->rows;
        request.viewport.target_pixel_width = job->columns;
        request.viewport.target_pixel_height = job->rows >
                std::numeric_limits<std::uint32_t>::max() / 2
            ? std::numeric_limits<std::uint32_t>::max()
            : job->rows * 2;

        auto result = engine_result.value().make_preview(*source_result.value(),
                                                         request);
        if (!result) {
            if (result.error().code == preview::Error::Code::cancelled) {
                continue;
            }
            Publish(job->generation, PreviewSnapshot{
                .status = PreviewStatus::Unavailable,
                .path = job->path,
                .message = ErrorMessage(result.error()),
                .result = nullptr,
            });
            continue;
        }

        Publish(job->generation, PreviewSnapshot{
            .status = PreviewStatus::Ready,
            .path = job->path,
            .message = {},
            .result = std::make_shared<preview::Preview>(
                std::move(result).value()),
        });
    }
}
