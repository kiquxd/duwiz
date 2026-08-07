#pragma once

#include <filesystem>

namespace fs = std::filesystem;

bool IsDirCached(const fs::path& path);
size_t GetCachedSize(const fs::path& path);
