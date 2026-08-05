#pragma once

#include "types.h"

bool IsDirCached(const fs::path& path);
size_t GetCachedSize(const fs::path& path);
