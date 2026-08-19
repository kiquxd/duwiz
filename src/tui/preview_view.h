#pragma once

#include "lib/preview_service.h"

#include <ftxui/dom/elements.hpp>

#include <memory>

ftxui::Element RenderPreview(
    const std::shared_ptr<const PreviewSnapshot>& snapshot);
