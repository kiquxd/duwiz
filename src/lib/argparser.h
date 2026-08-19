#include <string>
#include <unordered_set>

struct Config {
    std::string path = ".";
    size_t threads = 4;
    std::unordered_set<std::string> flags;
    bool invalid = false;
};

Config parseFlags(int argc, char** argv);
