#include <ftxui/component/component.hpp>
#include <ftxui/component/component_options.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/screen/screen.hpp>
#include <ftxui/screen/color.hpp>

#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>
 
#include "ftxui/dom/node.hpp"

using namespace ftxui;

bool KeyPressed(char, Event);
bool OneOfKeysPressed(const std::vector<char>&, Event);

Element RenderMultiLine(const std::string& textStr);
