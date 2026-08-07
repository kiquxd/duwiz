#include "argparser.h"
#include <getopt.h>

Config parseFlags(int argc, char* argv[]) {
    Config config;

    static struct option long_options[] = {
        {"help", no_argument,       nullptr, 'h'},
        {"path", required_argument, nullptr, 'p'},
        {nullptr, 0,                nullptr, 0}
    };

    int opt;
    int option_index = 0;
    int jobs;

    while ((opt = getopt_long(argc, argv, "hj:p:", long_options, &option_index)) != -1) {
        switch (opt) {
            case 'h':
                config.flags.insert("help");

            case 'p':
                config.flags.insert("path");
                config.path = optarg;
                break;

            case 'j':
                config.flags.insert("jobs");
                jobs = std::stoi(optarg);
                if (jobs <= 0) {
                    config.invalid = true;
                }
                config.threads = jobs;
                break;

            case '?':
                config.invalid = true;
                break;

            default:
                config.invalid = true;
                break;
        }
    }

    if (config.path.empty()) {
        config.invalid = true;
    }

    return config;
}
