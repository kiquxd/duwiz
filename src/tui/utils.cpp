#include "utils.h"

bool KeyPressed(char ch, Event event) {
    return event == Event::Character(ch);
}

bool OneOfKeysPressed(const std::vector<char>& chars, Event event) {
    for (auto& c : chars) {
        if (KeyPressed(c, event)) {
            return true;
        }
    }
    return false;
}

void printHelpWindow(const std::string& binary) {
    auto screen = Screen::Create(Dimension::Full());

    std::string errorMsg =
        "Usage: " + binary + " [options]\n"
            "Options: \n"
            "  --help (-h)   Show this screen and exit\n"
            "  --path <path> Specify path from which duwiz should start\n"
            "  -j <threads>  Max parallel jobs/tasks that can be running";

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

Element RenderMultiLine(const std::string& textStr) {
    Elements lines;
    std::stringstream ss(textStr);
    std::string line;
    
    while (std::getline(ss, line, '\n')) {
        lines.push_back(text(line));
    }
    
    return vbox(std::move(lines));
}
