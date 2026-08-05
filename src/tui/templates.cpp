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
