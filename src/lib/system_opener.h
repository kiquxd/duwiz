#pragma once

#include <filesystem>
#include <string>

struct OpenResult {
    bool ok = false;
    std::string error;
};

OpenResult OpenWithSystemViewer(const std::filesystem::path& path);
