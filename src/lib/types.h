#pragma once

#include <unordered_map>
#include <string>
#include <filesystem>
#include <cassert>
#include <vector>

#include "singleton.h"

namespace fs = std::filesystem;

typedef Singleton<std::unordered_map<fs::path, size_t>> CachedSize;
typedef Singleton<std::unordered_map<fs::path, bool>> CachedChecker;
