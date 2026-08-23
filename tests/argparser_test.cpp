#include "lib/argparser.h"

#include <cstdlib>
#include <iostream>
#include <string>

namespace {

constexpr const char* kTestHome = "/tmp/duwiz-argparser-home";
constexpr const char* kExplicitPath = "/tmp/duwiz-argparser-test";

}  // namespace

int main(int argc, char** argv) {
    if (unsetenv("HOME") != 0) {
        std::cerr << "could not unset HOME for the fallback test\n";
        return 1;
    }
    if (const Config fallback; fallback.path != ".") {
        std::cerr << "unexpected fallback path: " << fallback.path << '\n';
        return 1;
    }

    if (setenv("HOME", kTestHome, 1) != 0) {
        std::cerr << "could not set HOME for the test\n";
        return 1;
    }

    const Config config = parseFlags(argc, argv);
    const bool explicit_options = argc > 1;
    const std::string expected_path = explicit_options ? kExplicitPath : kTestHome;
    const size_t expected_threads = explicit_options ? 3 : 1;

    if (config.invalid || config.path != expected_path ||
        config.threads != expected_threads) {
        std::cerr << "unexpected config: path=" << config.path
                  << ", threads=" << config.threads
                  << ", invalid=" << config.invalid << '\n';
        return 1;
    }

    return 0;
}
