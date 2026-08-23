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

std::string StripAnsi(std::string_view input) {
    std::string output;
    for (std::size_t position = 0; position < input.size();) {
        if (input[position] == '\x1b' && position + 1 < input.size() &&
            input[position + 1] == '[') {
            position += 2;
            while (position < input.size()) {
                const auto byte = static_cast<unsigned char>(input[position++]);
                if (byte >= 0x40 && byte <= 0x7e) break;
            }
            continue;
        }
        output.push_back(input[position++]);
    }
    return output;
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
    const auto& text_preview =
        std::get<preview::TextPreview>(snapshot->result->content);
    bool has_syntax = false;
    for (const auto& line : text_preview.lines) {
        has_syntax = has_syntax || !line.styles.empty();
    }
    Require(text_preview.syntax_language == preview::SyntaxLanguage::markdown,
            "Markdown language was not detected");
    Require(has_syntax, "Markdown semantic syntax spans are missing");
    Require(!text_preview.display_lines.empty(),
            "Markdown terminal rendering is missing");
    for (const auto& line : text_preview.display_lines) {
        std::size_t columns = 0;
        for (const unsigned char byte : line.text) {
            if ((byte & 0xc0) != 0x80) ++columns;
        }
        Require(columns <= 55,
                "text wrapping does not reserve the line number gutter");
    }
    auto markdown_screen = ftxui::Screen::Create(
        ftxui::Dimension::Fixed(80), ftxui::Dimension::Fixed(24));
    ftxui::Render(markdown_screen, RenderPreview(snapshot));
    const std::string rendered_markdown = markdown_screen.ToString();
    Require(rendered_markdown.find("preview") != std::string::npos,
            "rendered Markdown heading is missing");
    Require(rendered_markdown.find("# preview") == std::string::npos,
            "rendered Markdown still exposes heading source markers");
    Require(rendered_markdown.find("1 │") != std::string::npos,
            "rendered text does not show source line numbers");

    auto numbered_result = std::make_shared<preview::Preview>();
    numbered_result->detected_format = "text";
    preview::TextPreview numbered_text;
    numbered_text.display_lines = {
        {.text = "first", .line_number = 1},
        {.text = "continued", .line_number = 1, .wrapped_continuation = true},
        {.text = "tenth", .line_number = 10},
    };
    numbered_result->content = std::move(numbered_text);
    auto numbered_snapshot = std::make_shared<PreviewSnapshot>(PreviewSnapshot{
        .status = PreviewStatus::Ready,
        .path = "numbered.txt",
        .result = numbered_result,
    });
    auto numbered_screen = ftxui::Screen::Create(
        ftxui::Dimension::Fixed(40), ftxui::Dimension::Fixed(10));
    ftxui::Render(numbered_screen, RenderPreview(numbered_snapshot));
    const auto rendered_numbers = StripAnsi(numbered_screen.ToString());
    const auto column_of = [&](std::string_view value) {
        const auto position = rendered_numbers.find(value);
        if (position == std::string::npos) return position;
        const auto line = rendered_numbers.rfind('\n', position);
        return position - (line == std::string::npos ? 0 : line + 1);
    };
    Require(column_of("first") != std::string::npos &&
                column_of("first") == column_of("continued") &&
                column_of("first") == column_of("tenth") &&
                rendered_numbers.find("│ continued") != std::string::npos,
            "line number gutter is not aligned to its widest number");

    auto table_result = std::make_shared<preview::Preview>();
    table_result->detected_format = "csv";
    table_result->detected_mime = "text/csv";
    table_result->content = preview::TablePreview{
        .columns = {{"name", preview::TableAlignment::left},
                    {"count", preview::TableAlignment::right}},
        .rows = {{{"alpha", "12"}}, {{"beta", "3"}}},
        .delimiter = ',',
        .has_header = true,
    };
    auto table_snapshot = std::make_shared<PreviewSnapshot>(PreviewSnapshot{
        .status = PreviewStatus::Ready,
        .path = "sample.csv",
        .result = table_result,
    });
    auto table_screen = ftxui::Screen::Create(
        ftxui::Dimension::Fixed(80), ftxui::Dimension::Fixed(24));
    ftxui::Render(table_screen, RenderPreview(table_snapshot));
    Require(table_screen.ToString().find("alpha") != std::string::npos &&
                table_screen.ToString().find("count") != std::string::npos,
            "semantic table preview is not rendered by FTXUI");

    auto archive_result = std::make_shared<preview::Preview>();
    archive_result->detected_format = "zip";
    archive_result->detected_mime = "application/zip";
    archive_result->content = preview::ArchivePreview{
        .entries = {{"docs/readme.txt", "file", "deflate", 42, 20}},
    };
    auto archive_snapshot = std::make_shared<PreviewSnapshot>(PreviewSnapshot{
        .status = PreviewStatus::Ready,
        .path = "sample.zip",
        .result = archive_result,
    });
    auto archive_screen = ftxui::Screen::Create(
        ftxui::Dimension::Fixed(80), ftxui::Dimension::Fixed(24));
    ftxui::Render(archive_screen, RenderPreview(archive_snapshot));
    Require(archive_screen.ToString().find("docs/readme.txt") !=
                std::string::npos,
            "archive entries are not rendered by FTXUI");

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

    PreviewRenderer kitty_renderer;
    kitty_renderer.SetOptions({
        .image_backend = PreviewImageBackend::KittyUnicode,
        .max_image_columns = 20,
        .max_image_rows = 8,
        .cell_pixel_width = 1,
        .cell_pixel_height = 1,
    });
    auto kitty_screen = ftxui::Screen::Create(
        ftxui::Dimension::Fixed(80), ftxui::Dimension::Fixed(24));
    ftxui::Render(kitty_screen, kitty_renderer.Render(snapshot));
    const std::string kitty_frame = kitty_screen.ToString();
    Require(kitty_frame.find("\x1b_Ga=T,f=32") != std::string::npos,
            "Kitty frame does not upload RGBA pixels");
    Require(kitty_frame.find("\xf4\x8e\xbb\xae") != std::string::npos,
            "Kitty frame does not contain Unicode image placeholders");

    kitty_screen.Clear();
    ftxui::Render(kitty_screen, kitty_renderer.Render(snapshot));
    Require(kitty_screen.ToString().find("\x1b_Ga=T,f=32") ==
                std::string::npos,
            "unchanged Kitty image was uploaded more than once");
    Require(kitty_renderer.TakeCleanupCommand().find("a=d,d=I") !=
                std::string::npos,
            "Kitty renderer does not release image data on cleanup");

    PreviewRenderer ansi_renderer;
    ansi_renderer.SetOptions({
        .image_backend = PreviewImageBackend::Ansi,
        .max_image_columns = 20,
        .max_image_rows = 8,
    });
    auto ansi_screen = ftxui::Screen::Create(
        ftxui::Dimension::Fixed(80), ftxui::Dimension::Fixed(24));
    ftxui::Render(ansi_screen, ansi_renderer.Render(snapshot));
    Require(ansi_screen.ToString().find("\x1b_G") == std::string::npos,
            "ANSI fallback unexpectedly emitted Kitty commands");

    const auto pdf_path = corpus / "hello_world.pdf";
    service.Request(pdf_path, 30, 10);
    snapshot = wait_for_path(pdf_path);
    Require(snapshot->status == PreviewStatus::Ready, "PDF preview is not ready");
    Require(snapshot->result && snapshot->result->detected_format == "pdf",
            "PDF format was not detected");
    Require(std::holds_alternative<preview::UnsupportedContent>(
                snapshot->result->content),
            "PDF must be delegated to the system viewer");

    service.Request(corpus, 30, 10);
    snapshot = wait_for_path(corpus);
    Require(snapshot->status == PreviewStatus::Unavailable,
            "directory must not be read as a regular file");
    Require(snapshot->message == "Directory", "directory status is unclear");

    return 0;
}
