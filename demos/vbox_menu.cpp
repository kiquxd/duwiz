#include <ftxui/component/component_options.hpp>
#include <ftxui/screen/terminal.hpp>
#include <ftxui/dom/elements.hpp>   // for filler, text, hbox, vbox
#include <ftxui/screen/screen.hpp>  // for Full, Screen
#include <string>
#include <vector>
#include "ftxui/component/component.hpp"
#include "ftxui/component/screen_interactive.hpp"
#include "ftxui/dom/elements.hpp"
 
#include "ftxui/dom/node.hpp"      // for Render
 
int main() {
    using namespace ftxui;

    std::vector<std::string> options = {"[###  ] first/dir", "[##   ] second/dir", "[#    ] simple_file"};
    std::vector<std::string> backup = {"[###  ] first/dir", "[##   ] second/dir", "[#    ] simple_file"};
    auto menuOpt = MenuOption();
    menuOpt.entries_option.transform = [](const EntryState& state) {
        Element e = text(state.label);
        
        if (state.focused) {
            e = text("> " + state.label) | bold | color(Color::Cyan);
        } else if (state.active) {
            e = text("  " + state.label) | color(Color::GrayDark);
        } else {
            e = text("  " + state.label);
        }
        return e;
    };
    int selected = 0;

    menuOpt.on_enter = [&]() {
        for (int i = selected; i < selected + 3; ++i) {
            options[i % 3] = backup[i % 3];
        }
        options[selected] += " (selected)";
    };

    auto menu = Menu(&options, &selected, menuOpt);

    std::string openedVimNostalgia = "";

    auto renderer = Renderer(menu, [&] {
        return vbox({
            text("--- Dir List ---") | bold | color(Color::Blue),
            separator(),
            menu->Render() | frame | border,
            separator(),
            text(openedVimNostalgia) | color(Color::Grey0),
        }) | size(WIDTH, LESS_THAN, 40);
    });

    auto screen = ScreenInteractive::TerminalOutput();

    auto event_handler = CatchEvent(renderer, [&](Event event) {
        if (event == Event::Character('q') || event == Event::Character('Q')) {
            screen.Exit(); 
            return true;
        }

        if (event == Event::Character(':')) {
            openedVimNostalgia = "No VIM functionality is supported yet :(";
            return true;
        }

        openedVimNostalgia = "";
        return false;
    });

    screen.Loop(event_handler);

    return 0;
}
