#include "templates.h"

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


Element RenderMultiLine(const std::string& textStr) {
    Elements lines;
    std::stringstream ss(textStr);
    std::string line;
    
    while (std::getline(ss, line, '\n')) {
        lines.push_back(text(line));
    }
    
    return vbox(std::move(lines));
}
