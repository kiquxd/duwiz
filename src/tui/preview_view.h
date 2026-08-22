#pragma once

#include "lib/preview_service.h"

#include <ftxui/dom/elements.hpp>

#include <cstdint>
#include <memory>
#include <string>

enum class PreviewImageBackend {
    Ansi,
    KittyUnicode,
};

struct PreviewRenderOptions {
    PreviewImageBackend image_backend = PreviewImageBackend::Ansi;
    std::uint32_t max_image_columns = 1;
    std::uint32_t max_image_rows = 1;
    std::uint32_t cell_pixel_width = 10;
    std::uint32_t cell_pixel_height = 20;
};

class PreviewRenderer {
public:
    PreviewRenderer();
    ~PreviewRenderer();

    PreviewRenderer(const PreviewRenderer&) = delete;
    PreviewRenderer& operator=(const PreviewRenderer&) = delete;

    void SetOptions(PreviewRenderOptions options);
    ftxui::Element Render(
        const std::shared_ptr<const PreviewSnapshot>& snapshot);

    // Call after the FTXUI loop has restored the terminal. The returned APC
    // removes any image data retained by Kitty.
    std::string TakeCleanupCommand();

private:
    struct State;
    std::shared_ptr<State> state_;
};

// Dependency-free ANSI renderer retained for tests and non-Kitty terminals.
ftxui::Element RenderPreview(
    const std::shared_ptr<const PreviewSnapshot>& snapshot);
