#include "preview_view.h"

#include <ftxui/screen/color.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
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
    if (y >= image.height) {
        return {0, 0, 0};
    }
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

Element RenderPixels(const preview::PixelPreview& image) {
    Elements rows;
    rows.reserve((image.height + 1) / 2);
    for (std::uint32_t y = 0; y < image.height; y += 2) {
        Elements cells;
        cells.reserve(image.width);
        for (std::uint32_t x = 0; x < image.width; ++x) {
            const Rgb top = PixelAt(image, x, y);
            const Rgb bottom = PixelAt(image, x, y + 1);
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
        case Token::operator_symbol: return element | color(Color::LightSlateGrey);
    }
    return element;
}

Element RenderTextLine(const preview::TextLine& line) {
    Elements parts;
    std::size_t cursor = 0;
    for (const auto& style : line.styles) {
        const auto begin = static_cast<std::size_t>(style.byte_begin);
        const auto end = static_cast<std::size_t>(style.byte_end);
        if (begin < cursor || end < begin || end > line.text.size()) {
            return text(line.text);
        }
        if (begin != cursor) {
            parts.push_back(text(line.text.substr(cursor, begin - cursor)));
        }
        parts.push_back(StyleSyntax(text(line.text.substr(begin, end - begin)),
                                    style.token));
        cursor = end;
    }
    if (cursor != line.text.size()) {
        parts.push_back(text(line.text.substr(cursor)));
    }
    return hbox(std::move(parts));
}

Element RenderResult(const preview::Preview& result) {
    Elements body;
    body.push_back(text(result.detected_format + " · " + result.detected_mime) |
                   color(Color::LightSlateGrey));
    for (const auto& item : result.metadata.items) {
        body.push_back(text(item.key + ": " + item.value));
    }
    if (!result.metadata.items.empty()) {
        body.push_back(separator());
    }

    if (const auto* content = std::get_if<preview::TextPreview>(&result.content)) {
        for (const auto& line : content->lines) {
            body.push_back(RenderTextLine(line));
        }
        if (content->has_more) {
            body.push_back(text("… more data available") |
                           color(Color::LightSlateGrey));
        }
    } else if (const auto* content =
                   std::get_if<preview::PixelPreview>(&result.content)) {
        body.push_back(RenderPixels(*content));
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

}  // namespace

Element RenderPreview(const std::shared_ptr<const PreviewSnapshot>& snapshot) {
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
        content.push_back(RenderResult(*snapshot->result));
    }
    return vbox(std::move(content));
}
