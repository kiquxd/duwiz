#include "lib/dir_iter.h"
#include "lib/thread_pool.h"
#include "lib/singleton.h"
#include "lib/argparser.h"
#include "lib/dir_tools.h"
#include "tui/fs_utils.h"
#include "tui/utils.h"

#include <ftxui/component/component.hpp>
#include <ftxui/component/component_options.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/screen/screen.hpp>
#include <ftxui/screen/color.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>
#include <ftxui/dom/node.hpp>

#include <cstdio>
#include <cstring>
#include <unordered_map>
#include <filesystem>
 

using namespace ftxui;
namespace fs = std::filesystem;

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

    MsgOr msgOrWarning;

    std::vector<std::string> dirEntries, fullPathEntries;
    std::vector<std::string> sizeEntries;

    auto updateEntries = [&] {
        DirectoryIterator iter(path);
        sizeEntries.clear();
        dirEntries.clear();
        fullPathEntries.clear();
        iter.AsyncSizeUpdate();

        auto& cacher = Singleton<Cacher>::Instance().GetObj();

        for (auto& subdirIter : iter.GetSubdirs()) {
            std::string fullPath = subdirIter.path;
            fullPathEntries.push_back(fullPath);
            size_t pos = fullPath.rfind("/");
            std::string shortPath = fullPath.substr(pos + 1);
            size_t dirSize = cacher.GetCachedSize(fullPath);
            auto sizeEntry = formatEntrySize(dirSize);
            dirEntries.push_back(std::move(shortPath));
            sizeEntries.push_back(std::move(sizeEntry));
        }

        msgOrWarning.Reset();
    };

    int selected = 0;

    int activeTab = 0;
    std::string inputMsg;
    char savedChar;

    auto doCd = [&] {
        if (!fs::is_directory(fullPathEntries[selected])) {
            msgOrWarning.SetWarning("Can only cd to directory");
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

    auto onSubmit = [&] {
        switch (savedChar) {
            case 'r': {
                std::string& fromPath = fullPathEntries[selected];
                std::string toPath = path.string() + "/" + inputMsg;
                fs::rename(fromPath, path.string() + "/" + inputMsg);
                msgOrWarning.SetMsg("Successfully renamed");

                auto& cacher = Singleton<Cacher>::Instance().GetObj();
                if (cacher.IsCached(fromPath)) {
                    size_t size = cacher.GetCachedSize(fromPath);
                    cacher.Invalidate(fromPath);
                    cacher.Update(toPath, size);
                } else {
                    assert(false && "Shouldn't happen (entry should be cached)");
                }
                return true;
            } case 'm': {
                auto res = mkdir(path, inputMsg);
                if (res == CreateResult::AlreadyExists) {
                    msgOrWarning.SetWarning("Directory already exists");
                    return true;
                }
                msgOrWarning.SetMsg("Directory successfully created");
                auto& cacher = Singleton<Cacher>::Instance().GetObj();
                cacher.Update(path.string() + "/" + inputMsg, 0);

                fullPathEntries.push_back(path.string() + "/" + inputMsg);
                dirEntries.push_back(inputMsg);
                sizeEntries.push_back(formatEntrySize(0));
                return true;
            } case 'c': {
                auto res = touch(path, inputMsg);
                switch (res) {
                    case CreateResult::AlreadyExists:
                        msgOrWarning.SetWarning("File already exists");
                        break;
                    case CreateResult::OpenOrCreateErr:
                        msgOrWarning.SetWarning("Failed to create or open file");
                        break;
                    case CreateResult::Ok:
                        msgOrWarning.SetMsg("Successfully created file");
                        fullPathEntries.push_back(path.string() + "/" + inputMsg);
                        dirEntries.push_back(inputMsg);
                        sizeEntries.push_back(formatEntrySize(0));
                        break;
                }
                return true;
            }
        }
        return false;
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

    auto mainRenderer = Renderer(dirMenu, [&] {
        return vbox({
            text("--- Dir List ---") | bold | color(Color::Blue),
            hbox({
                dirMenu->Render() | size(WIDTH, GREATER_THAN, MIN_DIR_ENTRY_LEN),
                sizeMenu->Render() | size(WIDTH, GREATER_THAN, MIN_SIZE_ENTRY_LEN),
            }) | frame | border,
            text("$ " + path.string()) | bold | color(Color::LightSlateGrey),
            msgOrWarning.Render()
        });
    });

    auto screen = ScreenInteractive::Fullscreen();

    auto mainEventHandler = CatchEvent(mainRenderer, [&](Event event) {
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

        if (OneOfKeysPressed({'c', 'C'}, event)) {
            activeTab = 1;
            savedChar = 'c';
            inputMsg = "";
            return true;
        }

        if (OneOfKeysPressed({'m', 'M'}, event)) {
            activeTab = 1;
            savedChar = 'm';
            inputMsg = "";
            return true;
        }

        if (OneOfKeysPressed({'r', 'R'}, event)) {
            activeTab = 1;
            savedChar = 'r';
            inputMsg = "";
            return true;
        }

        if (OneOfKeysPressed({'d', 'D'}, event)) {
            if (!fs::remove(fullPathEntries[selected])) {
                msgOrWarning.SetWarning("Failed to remove entry");
                return true;
            }
            auto& cacher = Singleton<Cacher>::Instance().GetObj();
            size_t size = cacher.GetCachedSize(fullPathEntries[selected]);
            cacher.Update(path, cacher.GetCachedSize(path) - size);

            fullPathEntries.erase(fullPathEntries.begin() + selected);
            dirEntries.erase(dirEntries.begin() + selected);
            sizeEntries.erase(sizeEntries.begin() + selected);
            return true;
        }

        msgOrWarning.Reset();

        return false;
    });

    auto inputField = Input(&inputMsg);

    auto promptRenderer = Renderer(inputField, [&] {
        return vbox({
            text("Input name:") | bold | color(Color::Yellow),
            separator(),
            inputField->Render() | border,
        }) | border | size(WIDTH, EQUAL, 40) | size(HEIGHT, EQUAL, 8);
    });

    auto container = Container::Tab({mainEventHandler, promptRenderer}, &activeTab);

    auto finalRenderer = Renderer(container, [&] {
        if (activeTab == 1) {
            return dbox({
                mainRenderer->Render() | dim,
                promptRenderer->Render() | clear_under | center
            });
        }
        return mainRenderer->Render();
    });

    auto promptEventHandler = CatchEvent(finalRenderer, [&](const Event& event) {
        if (event == Event::Return) {
            onSubmit();
            activeTab = 0;
            return true;
        }
        if (event == Event::Escape) {
            inputMsg = "";
            return true;
        }
        return false;
    });

    updateEntries();

    screen.Loop(promptEventHandler);
}
