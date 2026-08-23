#include "preview_view.h"

#include <preview/terminal.hpp>

#include <ftxui/dom/node.hpp>
#include <ftxui/dom/requirement.hpp>
#include <ftxui/screen/box.hpp>
#include <ftxui/screen/color.hpp>
#include <ftxui/screen/screen.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <variant>
#include <vector>

using namespace ftxui;

namespace {

struct Rgb {
    std::uint8_t red;
    std::uint8_t green;
    std::uint8_t blue;
};

Rgb PixelAt(const preview::PixelPreview& image, std::uint32_t x,
            std::uint32_t y) {
    if (y >= image.height || x >= image.width) return {0, 0, 0};
    const std::size_t offset = static_cast<std::size_t>(y) * image.stride +
                               static_cast<std::size_t>(x) * 4;
    const auto* pixel = image.pixels.data() + offset;
    const auto first = std::to_integer<std::uint8_t>(pixel[0]);
    const auto green = std::to_integer<std::uint8_t>(pixel[1]);
    const auto third = std::to_integer<std::uint8_t>(pixel[2]);
    const auto alpha = std::to_integer<std::uint8_t>(pixel[3]);
    const auto red = image.format == preview::PixelFormat::rgba8 ? first : third;
    const auto blue = image.format == preview::PixelFormat::rgba8 ? third : first;
    const auto blend = [alpha](std::uint8_t value) {
        return static_cast<std::uint8_t>(
            (static_cast<unsigned>(value) * alpha + 127) / 255);
    };
    return {blend(red), blend(green), blend(blue)};
}

std::pair<std::uint32_t, std::uint32_t> FitAnsiDimensions(
        const preview::PixelPreview& image, std::uint32_t max_columns,
        std::uint32_t max_rows) {
    max_columns = std::max<std::uint32_t>(max_columns, 1);
    max_rows = std::max<std::uint32_t>(max_rows, 1);
    const std::uint64_t max_pixel_rows =
        static_cast<std::uint64_t>(max_rows) * 2;
    std::uint32_t width = std::min(image.width, max_columns);
    std::uint32_t height = static_cast<std::uint32_t>(
        std::min<std::uint64_t>(image.height, max_pixel_rows));
    if (width == 0 || height == 0) return {0, 0};
    if (static_cast<std::uint64_t>(width) * image.height >
        static_cast<std::uint64_t>(height) * image.width) {
        width = std::max<std::uint32_t>(1, static_cast<std::uint32_t>(
            static_cast<std::uint64_t>(height) * image.width / image.height));
    } else {
        height = std::max<std::uint32_t>(1, static_cast<std::uint32_t>(
            static_cast<std::uint64_t>(width) * image.height / image.width));
    }
    return {width, height};
}

Element RenderPixels(const preview::PixelPreview& image,
                     std::uint32_t max_columns,
                     std::uint32_t max_rows) {
    const auto [width, height] =
        FitAnsiDimensions(image, max_columns, max_rows);
    Elements rows;
    rows.reserve((height + 1) / 2);
    for (std::uint32_t output_y = 0; output_y < height; output_y += 2) {
        Elements cells;
        cells.reserve(width);
        for (std::uint32_t output_x = 0; output_x < width; ++output_x) {
            const auto source_x = static_cast<std::uint32_t>(
                static_cast<std::uint64_t>(output_x) * image.width / width);
            const auto source_top = static_cast<std::uint32_t>(
                static_cast<std::uint64_t>(output_y) * image.height / height);
            const auto source_bottom = output_y + 1 < height
                ? static_cast<std::uint32_t>(
                    static_cast<std::uint64_t>(output_y + 1) *
                    image.height / height)
                : image.height;
            const Rgb top = PixelAt(image, source_x, source_top);
            const Rgb bottom = PixelAt(image, source_x, source_bottom);
            cells.push_back(text("▀") |
                color(Color::RGB(top.red, top.green, top.blue)) |
                bgcolor(Color::RGB(bottom.red, bottom.green, bottom.blue)));
        }
        rows.push_back(hbox(std::move(cells)));
    }
    return vbox(std::move(rows));
}

Element StyleSyntax(Element element, preview::SyntaxToken token) {
    using Token = preview::SyntaxToken;
    switch (token) {
        case Token::keyword: return element | color(Color::Magenta) | bold;
        case Token::type: return element | color(Color::Cyan);
        case Token::function: return element | color(Color::CornflowerBlue);
        case Token::string_literal: return element | color(Color::PaleGreen1);
        case Token::number: return element | color(Color::Yellow);
        case Token::comment: return element | color(Color::Grey50) | dim;
        case Token::preprocessor: return element | color(Color::LightSkyBlue1);
        case Token::property: return element | color(Color::Cyan);
        case Token::heading: return element | color(Color::Yellow) | bold;
        case Token::link: return element | color(Color::CornflowerBlue) | underlined;
        case Token::code_literal: return element | color(Color::PaleGreen1);
        case Token::emphasis: return element | italic;
        case Token::strong: return element | bold;
        case Token::strikethrough: return element | strikethrough;
        case Token::search_match:
            return element | color(Color::Black) | bgcolor(Color::Yellow);
        case Token::operator_symbol: return element | color(Color::LightSlateGrey);
    }
    return element;
}

std::string TextGutter(const preview::TextLine& line,
                       std::size_t number_width) {
    if (number_width == 0) return {};
    if (line.line_number == 0) {
        return std::string(number_width + 3, ' ');
    }
    if (line.wrapped_continuation) {
        return std::string(number_width, ' ') + " │ ";
    }
    const auto number = std::to_string(line.line_number);
    return std::string(number_width - std::min(number_width, number.size()), ' ') +
           number + " │ ";
}

Element RenderTextLine(const preview::TextLine& line,
                       std::size_t number_width) {
    Elements parts;
    std::size_t cursor = 0;
    for (const auto& style : line.styles) {
        const auto begin = static_cast<std::size_t>(style.byte_begin);
        const auto end = static_cast<std::size_t>(style.byte_end);
        if (begin < cursor || end < begin || end > line.text.size()) {
            return hbox({text(TextGutter(line, number_width)) |
                             color(Color::Grey50),
                         text(line.text)});
        }
        if (begin != cursor) {
            parts.push_back(text(line.text.substr(cursor, begin - cursor)));
        }
        parts.push_back(StyleSyntax(text(line.text.substr(begin, end - begin)),
                                    style.token));
        cursor = end;
    }
    if (cursor != line.text.size()) parts.push_back(text(line.text.substr(cursor)));
    return hbox({text(TextGutter(line, number_width)) | color(Color::Grey50),
                 hbox(std::move(parts))});
}

std::size_t TableWidth(const preview::TablePreview& table,
                       std::size_t column) {
    std::size_t width = table.columns[column].name.size();
    for (const auto& row : table.rows) {
        if (column < row.cells.size()) width = std::max(width, row.cells[column].size());
    }
    return std::clamp<std::size_t>(width, 3, 32);
}

Element TableCell(std::string value, std::size_t width,
                  preview::TableAlignment alignment, bool header = false) {
    if (value.size() > width) {
        const auto limit = width > 1 ? width - 1 : 0;
        std::size_t end = 0;
        while (end < value.size() && end < limit) {
            const auto first = static_cast<unsigned char>(value[end]);
            const std::size_t bytes = first < 0x80 ? 1
                : (first & 0xe0) == 0xc0 ? 2
                : (first & 0xf0) == 0xe0 ? 3
                : (first & 0xf8) == 0xf0 ? 4 : 1;
            if (end + bytes > limit) break;
            end += bytes;
        }
        value.resize(end);
        value += "…";
    }
    Element cell = text(std::move(value));
    if (alignment == preview::TableAlignment::right) cell = cell | align_right;
    if (alignment == preview::TableAlignment::center) cell = cell | center;
    if (header) cell = cell | bold | color(Color::Cyan);
    return cell | size(WIDTH, EQUAL, static_cast<int>(width + 2));
}

Element RenderTable(const preview::TablePreview& table) {
    if (table.columns.empty()) return text("Empty table") | dim;
    std::vector<std::size_t> widths;
    for (std::size_t column = 0; column < table.columns.size(); ++column) {
        widths.push_back(TableWidth(table, column));
    }
    Elements lines;
    Elements header;
    for (std::size_t column = 0; column < table.columns.size(); ++column) {
        if (column != 0) header.push_back(separator());
        header.push_back(TableCell(table.columns[column].name, widths[column],
                                   table.columns[column].alignment, true));
    }
    lines.push_back(hbox(std::move(header)));
    lines.push_back(separator());
    for (const auto& row : table.rows) {
        Elements cells;
        for (std::size_t column = 0; column < table.columns.size(); ++column) {
            if (column != 0) cells.push_back(separator());
            cells.push_back(TableCell(
                column < row.cells.size() ? row.cells[column] : "",
                widths[column], table.columns[column].alignment));
        }
        lines.push_back(hbox(std::move(cells)));
    }
    if (table.has_more) lines.push_back(text("… more rows") | dim);
    return vbox(std::move(lines));
}

Element RenderArchive(const preview::ArchivePreview& archive) {
    preview::TablePreview table;
    table.columns = {{"path", preview::TableAlignment::left},
                     {"kind", preview::TableAlignment::left},
                     {"size", preview::TableAlignment::right},
                     {"method", preview::TableAlignment::left}};
    table.has_more = archive.has_more;
    for (const auto& entry : archive.entries) {
        table.rows.push_back({{entry.path, entry.kind, std::to_string(entry.size),
                               entry.method}});
    }
    return RenderTable(table);
}

class KittyPlaceholderNode final : public Node {
public:
    KittyPlaceholderNode(std::uint32_t image_id, std::uint32_t columns,
                         std::uint32_t rows)
        : image_id_(image_id), columns_(columns), rows_(rows) {}

    void ComputeRequirement() override {
        requirement_.min_x = static_cast<int>(columns_);
        requirement_.min_y = static_cast<int>(rows_);
    }

    void Render(Screen& screen) override {
        const Box visible = Box::Intersection(box_, screen.stencil);
        if (visible.IsEmpty()) return;
        const auto red = static_cast<std::uint8_t>((image_id_ >> 16) & 0xff);
        const auto green = static_cast<std::uint8_t>((image_id_ >> 8) & 0xff);
        const auto blue = static_cast<std::uint8_t>(image_id_ & 0xff);
        for (int y = visible.y_min; y <= visible.y_max; ++y) {
            const auto row = static_cast<std::uint32_t>(y - box_.y_min);
            if (row >= rows_) break;
            for (int x = visible.x_min; x <= visible.x_max; ++x) {
                const auto column = static_cast<std::uint32_t>(x - box_.x_min);
                if (column >= columns_) break;
                auto placeholder = preview::terminal::kitty_placeholder(row, column);
                if (!placeholder) continue;
                auto& cell = screen.CellAt(x, y);
                cell.character = std::move(placeholder).value();
                cell.foreground_color = Color::RGB(red, green, blue);
                cell.background_color = Color::RGB(0, 0, 0);
            }
        }
    }

private:
    std::uint32_t image_id_;
    std::uint32_t columns_;
    std::uint32_t rows_;
};

class TerminalCommandNode final : public Node {
public:
    TerminalCommandNode(Element child, std::function<std::string()> take_command)
        : Node(Elements{std::move(child)}),
          take_command_(std::move(take_command)) {}

    void ComputeRequirement() override {
        children_[0]->ComputeRequirement();
        requirement_ = children_[0]->requirement();
    }
    void SetBox(Box box) override {
        Node::SetBox(box);
        children_[0]->SetBox(box);
    }
    void Render(Screen& screen) override {
        children_[0]->Render(screen);
        const Box visible = Box::Intersection(box_, screen.stencil);
        if (visible.IsEmpty()) return;
        std::string command = take_command_();
        if (!command.empty()) {
            auto& character =
                screen.CellAt(visible.x_min, visible.y_min).character;
            command.append(character);
            character = std::move(command);
        }
    }

private:
    std::function<std::string()> take_command_;
};

Element RenderResult(const preview::Preview& result, Element image) {
    Elements body;
    body.push_back(text(result.detected_format + " · " + result.detected_mime) |
                   color(Color::LightSlateGrey));
    for (const auto& item : result.metadata.items) {
        body.push_back(text(item.key + ": " + item.value));
    }
    if (!result.metadata.items.empty()) body.push_back(separator());

    if (const auto* content = std::get_if<preview::TextPreview>(&result.content)) {
        const auto& lines = content->display_lines.empty()
                                ? content->lines
                                : content->display_lines;
        std::size_t number_width = 0;
        for (const auto& line : lines) {
            if (line.line_number != 0) {
                number_width = std::max(
                    number_width, std::to_string(line.line_number).size());
            }
        }
        for (const auto& line : lines) {
            body.push_back(RenderTextLine(line, number_width));
        }
        if (content->has_more) {
            body.push_back(text("… more data available") |
                           color(Color::LightSlateGrey));
        }
    } else if (std::holds_alternative<preview::PixelPreview>(result.content)) {
        body.push_back(std::move(image));
    } else if (const auto* content =
                   std::get_if<preview::TablePreview>(&result.content)) {
        body.push_back(RenderTable(*content));
    } else if (const auto* content =
                   std::get_if<preview::ArchivePreview>(&result.content)) {
        body.push_back(RenderArchive(*content));
    } else if (const auto* content =
                   std::get_if<preview::UnsupportedContent>(&result.content)) {
        body.push_back(text(content->reason) | color(Color::Yellow));
    }
    for (const auto& warning : result.warnings) {
        body.push_back(text("warning: " + warning.message) |
                       color(Color::Yellow));
    }
    return vbox(std::move(body));
}

Element RenderSnapshot(const std::shared_ptr<const PreviewSnapshot>& snapshot,
                       Element image) {
    if (!snapshot || snapshot->status == PreviewStatus::Empty) {
        return text("No entry selected") | center;
    }
    Elements content;
    content.push_back(text(snapshot->path.filename().string()) | bold | center);
    content.push_back(separator());
    if (snapshot->status == PreviewStatus::Loading) {
        content.push_back(text(snapshot->message) |
                          color(Color::LightSlateGrey) | center);
    } else if (snapshot->status == PreviewStatus::Unavailable) {
        content.push_back(text(snapshot->message) |
                          color(Color::Yellow) | center);
    } else if (snapshot->result) {
        content.push_back(RenderResult(*snapshot->result, std::move(image)));
    }
    return vbox(std::move(content));
}

}  // namespace

struct PreviewRenderer::State {
    PreviewRenderOptions options;
    std::weak_ptr<const preview::Preview> attempted_result;
    std::uint32_t image_id = 0;
    std::uint32_t next_image_id = 1;
    std::uint32_t image_columns = 0;
    std::uint32_t image_rows = 0;
    bool image_ready = false;
    std::string pending_command;

    void DeleteActive() {
        if (image_id != 0) {
            auto command = preview::terminal::kitty_delete_image(image_id);
            if (command) pending_command += std::move(command).value();
        }
        attempted_result.reset();
        image_id = 0;
        image_columns = 0;
        image_rows = 0;
        image_ready = false;
    }

    void SetOptions(PreviewRenderOptions next) {
        next.max_image_columns = std::max<std::uint32_t>(next.max_image_columns, 1);
        next.max_image_rows = std::max<std::uint32_t>(next.max_image_rows, 1);
        next.cell_pixel_width = std::max<std::uint32_t>(next.cell_pixel_width, 1);
        next.cell_pixel_height = std::max<std::uint32_t>(next.cell_pixel_height, 1);
        if (options.image_backend != next.image_backend ||
            options.max_image_columns != next.max_image_columns ||
            options.max_image_rows != next.max_image_rows ||
            options.cell_pixel_width != next.cell_pixel_width ||
            options.cell_pixel_height != next.cell_pixel_height) {
            DeleteActive();
        }
        options = next;
    }

    bool Prepare(const std::shared_ptr<const PreviewSnapshot>& snapshot) {
        if (options.image_backend != PreviewImageBackend::KittyUnicode ||
            !snapshot || !snapshot->result) {
            DeleteActive();
            return false;
        }
        const auto* pixels = std::get_if<preview::PixelPreview>(
            &snapshot->result->content);
        if (!pixels) {
            DeleteActive();
            return false;
        }
        const auto columns = std::min({
            (pixels->width + options.cell_pixel_width - 1) /
                options.cell_pixel_width,
            options.max_image_columns,
            preview::terminal::kitty_max_placeholder_dimension});
        const auto rows = std::min({
            (pixels->height + options.cell_pixel_height - 1) /
                options.cell_pixel_height,
            options.max_image_rows,
            preview::terminal::kitty_max_placeholder_dimension});
        if (columns == 0 || rows == 0) {
            DeleteActive();
            return false;
        }
        if (attempted_result.lock() == snapshot->result &&
            image_columns == columns && image_rows == rows) {
            return image_ready;
        }

        DeleteActive();
        attempted_result = snapshot->result;
        image_columns = columns;
        image_rows = rows;
        image_id = next_image_id++;
        if (next_image_id > preview::terminal::kitty_max_image_id) next_image_id = 1;
        auto encoded = preview::terminal::make_kitty_image(
            *pixels, {.image_id = image_id, .columns = columns, .rows = rows});
        if (!encoded) {
            image_id = 0;
            return false;
        }
        std::string transmission = std::move(encoded).value().transmission;
        if (!pending_command.empty()) {
            transmission.insert(0, pending_command);
        }
        pending_command = std::move(transmission);
        image_ready = true;
        return true;
    }

    std::string TakeCommand() {
        return std::exchange(pending_command, {});
    }
};

PreviewRenderer::PreviewRenderer() : state_(std::make_shared<State>()) {}
PreviewRenderer::~PreviewRenderer() = default;

void PreviewRenderer::SetOptions(PreviewRenderOptions options) {
    state_->SetOptions(options);
}

Element PreviewRenderer::Render(
        const std::shared_ptr<const PreviewSnapshot>& snapshot) {
    const bool kitty = state_->Prepare(snapshot);
    Element image = text("");
    if (snapshot && snapshot->result) {
        if (const auto* pixels = std::get_if<preview::PixelPreview>(
                &snapshot->result->content)) {
            image = kitty
                ? std::make_shared<KittyPlaceholderNode>(
                    state_->image_id, state_->image_columns, state_->image_rows)
                : RenderPixels(*pixels, state_->options.max_image_columns,
                               state_->options.max_image_rows);
        }
    }
    Element content = RenderSnapshot(snapshot, std::move(image));
    const std::weak_ptr<State> weak_state = state_;
    return std::make_shared<TerminalCommandNode>(
        std::move(content), [weak_state] {
            const auto state = weak_state.lock();
            return state ? state->TakeCommand() : std::string{};
        });
}

std::string PreviewRenderer::TakeCleanupCommand() {
    state_->DeleteActive();
    return state_->TakeCommand();
}

Element RenderPreview(
        const std::shared_ptr<const PreviewSnapshot>& snapshot) {
    Element image = text("");
    if (snapshot && snapshot->result) {
        if (const auto* pixels = std::get_if<preview::PixelPreview>(
                &snapshot->result->content)) {
            image = RenderPixels(*pixels, pixels->width,
                                 (pixels->height + 1) / 2);
        }
    }
    return RenderSnapshot(snapshot, std::move(image));
}
