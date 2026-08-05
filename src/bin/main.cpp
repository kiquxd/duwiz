#include "../lib/dir_iter.h"
#include "../tui/templates.h"

#include <cstring>
#include <ftxui/component/component.hpp>
#include <ftxui/component/component_options.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/screen/screen.hpp>
#include <ftxui/screen/color.hpp>

#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>
#include <iostream>
#include <unordered_map>
 
#include "ftxui/dom/node.hpp"

using namespace ftxui;

void PrintHelpWindow() {
    auto screen = ScreenInteractive::TerminalOutput();

    auto renderer = Renderer([&] { 
        return vbox({
            filler(),
            hbox({
                filler(),
                text("This is help window, press q to exit"),
                filler(),
            }),
            filler()
        });
    });

    auto event_handler = CatchEvent(renderer, [&](Event event) {
        if (OneOfKeysPressed({'q', 'Q'}, event)) {
            screen.Exit();
            return true;
        }

        return false;
    });

    screen.Loop(event_handler);
}

int main(int argc, char** argv) {
    if (argc != 3) {
        PrintHelpWindow();
        return 0;
    }
    fs::path path = "unspecified";
    if (!strcmp(argv[1], "--path")) {
        path = argv[2];
    }

    std::unordered_map<std::string, std::string> getParentPath;
    getParentPath[path] = path;

    fs::path parent = path;

    std::string warningCdNotToDir = "";

    bool isActualForCwd = false;
    std::vector<std::string> entries, backup;

    auto updateEntries = [&] {
        DirectoryIterator iter(path);
        entries.clear();
        for (auto& subdirIter : iter.GetSubdirs()) {
            entries.push_back(subdirIter.path);
        }

        backup = entries;
        warningCdNotToDir = "";
    };

    int selected = 0;
    int prev_selected = -1;

    auto doCd = [&] {
        if (!fs::is_directory(entries[selected])) {
            warningCdNotToDir = "Can only cd to directory";
            return;
        }
        parent = path;
        path = entries[selected];
        getParentPath[path] = parent;
        updateEntries();
    };

    auto undoCd = [&] {
        path = parent;
        parent = getParentPath[parent];
        updateEntries();
    };

    auto menuOption = MenuOption();
    menuOption.on_enter = doCd;

    menuOption.entries_option.transform = [&](const EntryState& state) {
        Element e;
        if (state.focused) {
            e = text(state.label) | bold | color(Color::Black) | bgcolor(Color::LightSlateGrey);
        } else {
            if (fs::is_directory(state.label)) {
                e = text(state.label) | color(Color::Blue);
            } else {
                e = text(state.label) | color(Color::White);
            }
        }
        return e;
    };

    auto menu = Menu(&entries, &selected, menuOption);

    auto renderer = Renderer(menu, [&] {
        return vbox({
            text("--- Dir List ---") | bold | color(Color::Blue),
            menu->Render() | frame | border,
            text(warningCdNotToDir) | color(Color::Grey0)
        }) | size(WIDTH, LESS_THAN, 40);
    });

    auto screen = ScreenInteractive::TerminalOutput();
    
    auto event_handler = CatchEvent(renderer, [&](Event event) {
        if (OneOfKeysPressed({'q', 'Q'}, event)) {
            screen.Exit();
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
