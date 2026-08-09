#include "lib/dir_iter.h"
#include "tui/templates.h"
#include "lib/thread_pool.h"
#include "lib/singleton.h"
#include "lib/argparser.h"

#include <cstdio>
#include <cstring>
#include <format>
#include <ftxui/component/component.hpp>
#include <ftxui/component/component_options.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/screen/screen.hpp>
#include <ftxui/screen/color.hpp>

#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>
#include <unordered_map>
#include <filesystem>
 
#include "ftxui/dom/node.hpp"

using namespace ftxui;
namespace fs = std::filesystem;

void printHelpWindow(const std::string& binary) {
    auto screen = Screen::Create(Dimension::Full());

    std::string errorMsg = std::format(
        "Usage: {} [options]\n"
            "Options: \n"
            "  --help (-h)   Show this screen and exit\n"
            "  --path <path> Specify path from which ya-ncdu should start\n"
            "  -j <threads>  Max parallel jobs/tasks that can be running",
            binary
    );

    auto document = vbox({
            filler(),
            hbox({
                filler(),
                RenderMultiLine(errorMsg),
                filler(),
            }),
            filler()
        }) | size(WIDTH, GREATER_THAN, 40);

    Render(screen, document);

    screen.Print();
}

constexpr size_t MIN_DIR_ENTRY_LEN = 24;
constexpr size_t MAX_DIR_ENTRY_LEN = 48;

constexpr size_t MIN_SIZE_ENTRY_LEN = 12;
constexpr size_t MAX_SIZE_ENTRY_LEN = 24;

int main(int argc, char** argv) {
    Config config = parseFlags(argc, argv);
    if (config.invalid || config.flags.contains("help")) {
        printHelpWindow(argv[0]);
        return 0;
    }

    size_t num_threads = config.threads;
    fs::path path = config.path;

    FirstInitSingleton<runtime::ThreadPool>::Init(num_threads);
    FirstInitSingleton<runtime::ThreadPool>::Instance().GetObj().Start();

    std::unordered_map<std::string, std::string> getParentPath;
    std::unordered_map<std::string, size_t> getPrevSelectedByPath;
    getParentPath[path] = path;

    std::string warningCdNotToDir = "";

    std::vector<std::string> dirEntries, fullPathEntries;
    std::vector<std::string> sizeEntries;

    auto updateSizeEntry = [&](size_t size) {
        if (size < 1024) {
            return std::to_string(size) + " bytes";
        }
        size *= 10;
        std::vector<std::string> suffix = {"bytes", "Kb", "Mb", "Gb", "Tb"};
        size_t index = 0;
        while (size >= 1024) {
            size /= 1024;
            ++index;
        }
        double res = static_cast<double>(size) / 10;
        return std::format("{:.1f}", res) + ' ' + suffix[index];
    };

    auto updateEntries = [&] {
        DirectoryIterator iter(path);
        sizeEntries.clear();
        dirEntries.clear();
        fullPathEntries.clear();
        for (auto& subdirIter : iter.GetSubdirs()) {
            std::string fullPath = subdirIter.path;
            fullPathEntries.push_back(fullPath);
            size_t pos = fullPath.rfind("/");
            std::string shortPath = fullPath.substr(pos + 1);
            size_t dirSize = subdirIter.UpdateActualSize();
            auto sizeEntry = updateSizeEntry(dirSize);
            dirEntries.push_back(std::move(shortPath));
            sizeEntries.push_back(std::move(sizeEntry));
        }

        warningCdNotToDir = "";
    };

    int selected = 0;

    auto doCd = [&] {
        if (!fs::is_directory(fullPathEntries[selected])) {
            warningCdNotToDir = "Can only cd to directory";
            return;
        }
        getParentPath[fullPathEntries[selected]] = path;
        path = fullPathEntries[selected];

        getPrevSelectedByPath[path] = selected;

        updateEntries();
    };

    auto undoCd = [&] {
        std::string next = path;
        path = getParentPath[path];
        updateEntries();

        selected = getPrevSelectedByPath[next];
    };

    auto menuOption = MenuOption();
    menuOption.on_enter = doCd;

    menuOption.entries_option.transform = [&](const EntryState& state) {
        Element e;
        if (state.active) {
            e = text(state.label) | bold | color(Color::Black) | bgcolor(Color::LightSlateGrey);
        } else {
            if (fs::is_directory(path.string() + "/" + state.label)) {
                e = text(state.label) | color(Color::Blue);
            } else {
                e = text(state.label) | color(Color::White);
            }
        }
        return e;
    };

    auto sizeMenuOption = MenuOption();
    sizeMenuOption.entries_option.transform = [&](const EntryState& state) {
        Element e = text(state.label);
        if (state.active) {
            e = text(state.label) | bold | color(Color::Black) | bgcolor(Color::LightSlateGrey);
        }
        return e;
    };

    auto dirMenu = Menu(&dirEntries, &selected, menuOption);
    auto sizeMenu = Menu(&sizeEntries, &selected, sizeMenuOption);

    auto renderer = Renderer(dirMenu, [&] {
        return vbox({
            text("--- Dir List ---") | bold | color(Color::Blue),
            hbox({
                dirMenu->Render() | size(WIDTH, GREATER_THAN, MIN_DIR_ENTRY_LEN),
                sizeMenu->Render() | size(WIDTH, GREATER_THAN, MIN_SIZE_ENTRY_LEN),
            }) | frame | border,
            text("$ " + path.string()) | bold | color(Color::Grey0),
            text(warningCdNotToDir) | color(Color::Grey0)
        });
    });

    auto screen = ScreenInteractive::Fullscreen();
    
    auto event_handler = CatchEvent(renderer, [&](Event event) {
        if (OneOfKeysPressed({'q', 'Q'}, event)) {
            screen.Exit();
            auto& pool = FirstInitSingleton<runtime::ThreadPool>::Instance().GetObj();
            pool.Stop();
            return true;
        }

        if (event == Event::ArrowLeft) {
            undoCd();
            return true;
        }

        if (event == Event::ArrowRight) {
            doCd();
            return true;
        }

        return false;
    });

    updateEntries();

    screen.Loop(event_handler);
}
