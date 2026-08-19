#include "lib/preview_service.h"
#include "tui/preview_view.h"

#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>

#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <variant>

namespace {

using namespace std::chrono_literals;

[[noreturn]] void Fail(const std::string& message) {
    std::cerr << "preview integration test failed: " << message << '\n';
    std::exit(1);
}

void Require(bool condition, const std::string& message) {
    if (!condition) Fail(message);
}

}  // namespace

int main() {
    const std::filesystem::path corpus(PREVIEW_TEST_CORPUS);
    std::mutex notification_mutex;
    std::condition_variable notification;
    std::size_t notification_count = 0;

    PreviewService service([&] {
        {
            std::lock_guard guard(notification_mutex);
            ++notification_count;
        }
        notification.notify_all();
    });

    const auto wait_for_path = [&](const std::filesystem::path& path) {
        const auto deadline = std::chrono::steady_clock::now() + 5s;
        std::unique_lock lock(notification_mutex);
        while (std::chrono::steady_clock::now() < deadline) {
            const auto snapshot = service.Snapshot();
            if (snapshot->path == path &&
                snapshot->status != PreviewStatus::Loading) {
                return snapshot;
            }
            const auto observed = notification_count;
            notification.wait_until(lock, deadline, [&] {
                return notification_count != observed;
            });
        }
        Fail("timed out waiting for " + path.string());
    };

    const auto text_path = corpus.parent_path().parent_path() / "README.md";
    service.Request(text_path, 60, 10);
    auto snapshot = wait_for_path(text_path);
    Require(snapshot->status == PreviewStatus::Ready, "text preview is not ready");
    Require(snapshot->result &&
            std::holds_alternative<preview::TextPreview>(snapshot->result->content),
            "text payload is missing");

    // The last request must win even when the previous one is still debouncing.
    const auto jpeg_path = corpus / "sample.jpg";
    const auto png_path = corpus / "sample.png";
    service.Request(jpeg_path, 20, 8);
    service.Request(png_path, 20, 8);
    snapshot = wait_for_path(png_path);
    Require(snapshot->status == PreviewStatus::Ready, "PNG preview is not ready");
    Require(snapshot->result &&
            std::holds_alternative<preview::PixelPreview>(snapshot->result->content),
            "PNG pixel payload is missing");

    auto screen = ftxui::Screen::Create(ftxui::Dimension::Fixed(80),
                                        ftxui::Dimension::Fixed(24));
    ftxui::Render(screen, RenderPreview(snapshot));
    const std::string rendered = screen.ToString();
    Require(rendered.find("sample.png") != std::string::npos,
            "FTXUI output does not contain the selected filename");
    Require(rendered.find("image/png") != std::string::npos,
            "FTXUI output does not contain image metadata");

    const auto pdf_path = corpus / "hello_world.pdf";
    service.Request(pdf_path, 30, 10);
    snapshot = wait_for_path(pdf_path);
    Require(snapshot->status == PreviewStatus::Ready, "PDF preview is not ready");
    Require(snapshot->result && snapshot->result->detected_format == "pdf",
            "PDF format was not detected");
    Require(std::holds_alternative<preview::PixelPreview>(snapshot->result->content),
            "PDF pixel payload is missing");

    service.Request(corpus, 30, 10);
    snapshot = wait_for_path(corpus);
    Require(snapshot->status == PreviewStatus::Unavailable,
            "directory must not be read as a regular file");
    Require(snapshot->message == "Directory", "directory status is unclear");

    return 0;
}
