#include <string>
#include <optional>

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

void printHelpWindow(const std::string& binary);
Element RenderMultiLine(const std::string& textStr);


// MsgOrWarning
struct MsgOr {
private:
    std::optional<std::string> buf_;
    bool isMsg_ = false;

    void Set(std::string content, bool flag) {
        buf_.emplace(std::move(content));
        isMsg_ = flag;
    }

public:
    void Reset() {
        isMsg_ = false;
        buf_.reset();
    }

    bool IsMsg() {
        return isMsg_;
    }

    void SetMsg(std::string msg) {
        Set(std::move(msg), true);
    }

    void SetWarning(std::string warn) {
        Set(std::move(warn), false);
    }

    Element Render() {
        if (!buf_.has_value()) {
            return text("");
        }
        return text(buf_.value()) | color(isMsg_ ? Color::PaleGreen1 : Color::PaleVioletRed1);
    }
};
