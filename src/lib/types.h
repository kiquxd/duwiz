#pragma once

#include <unordered_map>
#include <filesystem>

#include "singleton.h"

namespace fs = std::filesystem;

typedef Singleton<std::unordered_map<fs::path, size_t>> CachedSize;
typedef Singleton<std::unordered_map<fs::path, bool>> CachedChecker;
