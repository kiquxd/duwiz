#include "lib/fs.h"
#include "lib/thread_pool.h"
#include "lib/singleton.h"
#include "lib/argparser.h"
#include "lib/preview_service.h"
#include "lib/system_opener.h"
#include "tui/preview_view.h"
#include "tui/utils.h"

#include <ftxui/component/component.hpp>
#include <ftxui/component/component_options.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/screen/screen.hpp>
#include <ftxui/screen/color.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>
#include <ftxui/dom/node.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string_view>
#include <unordered_map>
#include <filesystem>

#include <sys/ioctl.h>
#include <unistd.h>
 

using namespace ftxui;
namespace fs = std::filesystem;

constexpr size_t MIN_DIR_ENTRY_LEN = 48;

constexpr size_t MIN_SIZE_ENTRY_LEN = 12;

namespace {

struct CellPixels {
    std::uint32_t width = 10;
    std::uint32_t height = 20;
};

std::string Lowercase(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char ch) {
                       return static_cast<char>(std::tolower(ch));
                   });
    return value;
}

std::string Environment(std::string_view name) {
    const char* value = std::getenv(std::string(name).c_str());
    return value ? value : "";
}

bool ContainsKitty(std::string value) {
    return Lowercase(std::move(value)).find("kitty") != std::string::npos;
}

PreviewImageBackend SelectImageBackend(const ScreenInteractive& screen) {
    const std::string requested = Lowercase(Environment("YA_NCDU_IMAGE_BACKEND"));
    if (requested == "ansi") return PreviewImageBackend::Ansi;
    if (requested == "kitty") return PreviewImageBackend::KittyUnicode;

    // Raw Kitty APC commands require explicit passthrough inside tmux. Until
    // that is implemented, auto mode chooses the reliable ANSI renderer.
    if (!Environment("TMUX").empty()) return PreviewImageBackend::Ansi;
    const bool kitty = ContainsKitty(Environment("TERM")) ||
        ContainsKitty(Environment("TERM_PROGRAM")) ||
        ContainsKitty(screen.TerminalName()) ||
        ContainsKitty(screen.TerminalEmulatorName());
    return kitty ? PreviewImageBackend::KittyUnicode
                 : PreviewImageBackend::Ansi;
}

CellPixels DetectCellPixels() {
    winsize size{};
    if (::ioctl(STDOUT_FILENO, TIOCGWINSZ, &size) == 0 &&
        size.ws_col != 0 && size.ws_row != 0 &&
        size.ws_xpixel != 0 && size.ws_ypixel != 0) {
        return {
            std::max<std::uint32_t>(1, size.ws_xpixel / size.ws_col),
            std::max<std::uint32_t>(1, size.ws_ypixel / size.ws_row),
        };
    }
    return {};
}

std::pair<std::uint32_t, std::uint32_t> BoundPixelViewport(
        std::uint64_t width, std::uint64_t height) {
    constexpr std::uint64_t max_dimension = 4096;
    constexpr std::uint64_t max_pixels = 4 * 1024 * 1024;
    width = std::clamp<std::uint64_t>(width, 1, max_dimension);
    height = std::clamp<std::uint64_t>(height, 1, max_dimension);
    if (width * height > max_pixels) {
        const double scale = std::sqrt(
            static_cast<double>(max_pixels) /
            static_cast<double>(width * height));
        width = std::max<std::uint64_t>(1,
            static_cast<std::uint64_t>(width * scale));
        height = std::max<std::uint64_t>(1,
            static_cast<std::uint64_t>(height * scale));
    }
    return {static_cast<std::uint32_t>(width),
            static_cast<std::uint32_t>(height)};
}

}  // namespace

enum TabIndex : int {
    Browser = 0,
    Prompt = 1,
    Popup = 2
};

int main(int argc, char** argv) {
    Config config = parseFlags(argc, argv);
    if (config.invalid || config.flags.contains("help")) {
        printHelpWindow(argv[0]);
        return 0;
    }

    auto screen = ScreenInteractive::Fullscreen();

    int selected = 0;
    int activeTab = TabIndex::Browser;
    bool previewFullscreen = false;
    std::string inputMsg;
    char savedChar;

    std::shared_ptr<ScanSession> activeSession;
    uint64_t nextGeneration = 0;

    std::mutex mutex;
    std::vector<SizeUpdate> pendingSizeUpdates;
    bool customEventQueued = false;

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
    std::string loadingStatus;

    PreviewService previewService([&screen] {
        screen.PostEvent(Event::Custom);
    });
    PreviewRenderer previewRenderer;

    auto requestPreview = [&] {
        if (selected < 0 || static_cast<size_t>(selected) >= fullPathEntries.size()) {
            previewService.Clear();
            return;
        }
        const int availableColumns = previewFullscreen
            ? screen.dimx() - 4
            : screen.dimx() - static_cast<int>(MIN_DIR_ENTRY_LEN +
                                                MIN_SIZE_ENTRY_LEN + 6);
        const int availableRows = previewFullscreen
            ? screen.dimy() - 6
            : screen.dimy() - 12;
        const auto columns = static_cast<std::uint32_t>(
            std::max(20, availableColumns));
        const auto rows = static_cast<std::uint32_t>(
            std::max(4, availableRows));
        const auto imageRows = rows > 6 ? rows - 6 : 1;
        const auto backend = SelectImageBackend(screen);
        const CellPixels cellPixels = DetectCellPixels();
        const auto [pixelWidth, pixelHeight] = backend ==
                PreviewImageBackend::KittyUnicode
            ? BoundPixelViewport(
                static_cast<std::uint64_t>(columns) * cellPixels.width,
                static_cast<std::uint64_t>(imageRows) * cellPixels.height)
            : BoundPixelViewport(columns,
                                 static_cast<std::uint64_t>(imageRows) * 2);
        previewRenderer.SetOptions({
            .image_backend = backend,
            .max_image_columns = columns,
            .max_image_rows = imageRows,
            .cell_pixel_width = cellPixels.width,
            .cell_pixel_height = cellPixels.height,
        });
        previewService.Request(
            fullPathEntries[static_cast<size_t>(selected)],
            columns, rows, pixelWidth, pixelHeight
        );
    };

    auto updateEntries = [&] {
        if (activeSession) {
            activeSession->stopSource.request_stop();
        }

        dirEntries.clear();
        fullPathEntries.clear();
        sizeEntries.clear();

        std::vector<fs::path> directEntries;

        std::error_code ec;
        fs::directory_iterator it(
            path,
            fs::directory_options::skip_permission_denied,
            ec
        );
        const fs::directory_iterator end;

        while (!ec && it != end) {
            const fs::directory_entry entry = *it;
            const fs::path entryPath = entry.path();

            directEntries.push_back(entryPath);
            fullPathEntries.push_back(entryPath.string());
            dirEntries.push_back(entryPath.filename().string());

            sizeEntries.push_back("...");

            it.increment(ec);
        }

        if (ec) {
            msgOrWarning.SetWarning("Failed to read directory");
        }

        selected = 0;
        requestPreview();

        auto session = std::make_shared<ScanSession>(
            ++nextGeneration,
            std::move(directEntries)
        );
        activeSession = session;

        if (session->directEntries.empty()) {
            return;
        }

        const size_t workerCount = std::min(num_threads, session->directEntries.size());

        auto& pool = FirstInitSingleton<runtime::ThreadPool>::Instance().GetObj();

        for (size_t i = 0; i < workerCount; ++i) {
            pool.Submit([session, &screen, &pendingSizeUpdates, &mutex, &customEventQueued] {
                const std::stop_token token = session->stopSource.get_token();

                while (!token.stop_requested()) {
                    const size_t index = session->nextIndex.fetch_add(1);

                    if (index >= session->directEntries.size()) {
                        return;
                    }

                    DirectoryIterator iter(
                        session->directEntries[index]
                    );
                    ScanResult result = iter.SyncSizeUpdate(token);

                    if (token.stop_requested() || result.IsCancelled()) {
                        return;
                    }

                    session->finishedEntries.fetch_add(1);

                    if (result.IsReady()) {
                        session->totalSize.fetch_add(result.Get());
                        session->readyEntries.fetch_add(1);
                    }

                    bool postEvent = false;
                    {
                        std::lock_guard guard(mutex);

                        pendingSizeUpdates.push_back({
                            .generation = session->generation,
                            .index = index,
                            .result = std::move(result),
                        });

                        if (!customEventQueued) {
                            customEventQueued = true;
                            postEvent = true;
                        }
                    }

                    if (postEvent) {
                        screen.PostEvent(Event::Custom);
                    }
                }
            });
        }
    };

    auto doCd = [&] {
        if (selected < 0 ||
            static_cast<size_t>(selected) >= fullPathEntries.size()) {
            msgOrWarning.SetWarning("Selected index out of range");
            return;
        }
        if (!fs::is_directory(fullPathEntries[selected])) {
            msgOrWarning.SetWarning("Can only cd to directory");
            return;
        }
        getParentPath[fullPathEntries[selected]] = path;
        path = fullPathEntries[selected];

        getPrevSelectedByPath[path] = selected;

        updateEntries();

        loadingStatus = "";
    };

    auto undoCd = [&] {
        std::string next = path;
        path = getParentPath[path];
        updateEntries();

        selected = getPrevSelectedByPath[next];
        if (!fullPathEntries.empty()) {
            selected = std::min<int>(selected,
                                     static_cast<int>(fullPathEntries.size() - 1));
        }
        requestPreview();

        loadingStatus = "";
    };

    auto onSubmit = [&] {
        switch (savedChar) {
            case 'r': {
                std::string& fromPath = fullPathEntries[selected];
                std::string toPath = path.string() + "/" + inputMsg;

                auto& cacher = Singleton<Cacher>::Instance().GetObj();
                if (cacher.IsCached(fromPath)) {
                    size_t size = cacher.GetCachedSize(fromPath);

                    fs::rename(fromPath, toPath);

                    cacher.Invalidate(fromPath);
                    cacher.Update(toPath, size);

                    dirEntries[selected] = inputMsg;
                    fullPathEntries[selected] = toPath;
                    requestPreview();
                } else {
                    assert(false && "Shouldn't happen (entry should be cached)");
                }
                msgOrWarning.SetMsg("Successfully renamed");

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
    menuOption.on_change = requestPreview;
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
                sizeMenu->Render() | size(WIDTH, GREATER_THAN, MIN_SIZE_ENTRY_LEN) | align_right,
                separator(),
                previewRenderer.Render(previewService.Snapshot()) | xflex
            }) | frame | border,
            hbox({
                text("$ " + path.string()) | bold | color(Color::LightSlateGrey),
                filler(),
                text(loadingStatus) | color(Color::LightSlateGrey)
            }),
            msgOrWarning.Render()
        });
    });

    auto inputField = Input(&inputMsg);

    auto promptRenderer = Renderer(inputField, [&] {
        return vbox({
            text("Input name:") | bold | color(Color::Yellow),
            separator(),
            inputField->Render() | border,
        }) | border | size(WIDTH, EQUAL, 40) | size(HEIGHT, EQUAL, 8);
    });

    auto popupRenderer = Renderer([&] {
        return vbox({
            text("Are you sure you want to delete this entry?") | bold | color(Color::Yellow) | center,
            separator(),
            hbox({
                filler() | size(WIDTH, EQUAL, 5),
                text("[Y]es") | color(Color::LightSlateGrey),
                filler(),
                text("[N]o") | color(Color::LightSlateGrey),
                filler() | size(WIDTH, EQUAL, 5)
            })
        }) | border | size(WIDTH, EQUAL, 50) | size(HEIGHT, EQUAL, 6);
    });

    auto container = Container::Tab({mainRenderer, promptRenderer, popupRenderer}, &activeTab);

    auto finalRenderer = Renderer(container, [&] {
        if (activeTab == TabIndex::Prompt) {
            return dbox({
                mainRenderer->Render() | dim,
                promptRenderer->Render() | clear_under | center
            });
        }
        if (activeTab == TabIndex::Popup) {
            return dbox({
                mainRenderer->Render() | dim,
                popupRenderer->Render() | clear_under | center
            });
        }
        if (previewFullscreen) {
            return vbox({
                previewRenderer.Render(previewService.Snapshot()) | flex,
                separator(),
                text("p / Esc: close preview") | color(Color::LightSlateGrey)
            }) | border;
        }
        return mainRenderer->Render();
    });

    auto applySizeUpdates = [&] {
        std::vector<SizeUpdate> updates;

        {
            std::lock_guard guard(mutex);
            updates.swap(pendingSizeUpdates);
            customEventQueued = false;
        }

        if (!activeSession) {
            return;
        }

        for (auto& update : updates) {
            if (update.generation != activeSession->generation || update.index >= sizeEntries.size()) {
                continue;
            }

            if (update.result.IsReady()) {
                sizeEntries[update.index] = formatEntrySize(update.result.Get());
            } else {
                sizeEntries[update.index] = "<error>";
            }
        }
        loadingStatus = formatEntryStatus(
            activeSession->finishedEntries.load(),
            activeSession->directEntries.size(),
            activeSession->totalSize.load()
        );
    };

    auto eventHandler = CatchEvent(finalRenderer, [&](Event event) {
        if (event == Event::Custom) {
            applySizeUpdates();
            return true;
        }

        if (activeTab == TabIndex::Prompt) {
            if (event == Event::Return) {
                onSubmit();
                activeTab = TabIndex::Browser;
                return true;
            }
            if (event == Event::Escape) {
                inputMsg = "";
                activeTab = TabIndex::Browser;
                return true;
            }
        } else if (activeTab == TabIndex::Browser) {
            if (OneOfKeysPressed({'q', 'Q'}, event)) {
                screen.Exit();
                auto& pool = FirstInitSingleton<runtime::ThreadPool>::Instance().GetObj();
                pool.Stop();
                return true;
            }

            if (previewFullscreen &&
                (event == Event::Escape || OneOfKeysPressed({'p', 'P'}, event))) {
                previewFullscreen = false;
                requestPreview();
                return true;
            }

            if (previewFullscreen) {
                return true;
            }

            if (OneOfKeysPressed({'p', 'P'}, event)) {
                previewFullscreen = true;
                requestPreview();
                return true;
            }

            if (OneOfKeysPressed({'o', 'O'}, event)) {
                if (selected < 0 ||
                    static_cast<size_t>(selected) >= fullPathEntries.size()) {
                    msgOrWarning.SetWarning("No file selected");
                    return true;
                }
                const fs::path selectedPath =
                    fullPathEntries[static_cast<size_t>(selected)];
                std::string extension = selectedPath.extension().string();
                std::transform(extension.begin(), extension.end(), extension.begin(),
                               [](unsigned char ch) {
                                   return static_cast<char>(std::tolower(ch));
                               });
                std::error_code statusError;
                if (extension != ".pdf" ||
                    !fs::is_regular_file(selectedPath, statusError)) {
                    msgOrWarning.SetWarning("System viewer is available for PDF files");
                    return true;
                }
                const auto opened = OpenWithSystemViewer(selectedPath);
                if (opened.ok) {
                    msgOrWarning.SetMsg("Opened PDF in the system viewer");
                } else {
                    msgOrWarning.SetWarning(opened.error);
                }
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
                activeTab = TabIndex::Prompt;
                savedChar = 'c';
                inputMsg = "";
                return true;
            }

            if (OneOfKeysPressed({'m', 'M'}, event)) {
                activeTab = TabIndex::Prompt;
                savedChar = 'm';
                inputMsg = "";
                return true;
            }

            if (OneOfKeysPressed({'r', 'R'}, event)) {
                activeTab = TabIndex::Prompt;
                savedChar = 'r';
                inputMsg = "";
                return true;
            }

            if (OneOfKeysPressed({'d', 'D'}, event)) {
                std::string& rmPath = fullPathEntries[selected];
                if (fs::is_directory(rmPath) && !fs::is_empty(rmPath)) {
                    msgOrWarning.SetWarning("Not empty folders deletion is not supported for now");
                    return true;
                }
                activeTab = TabIndex::Popup;
                return true;
            }
        } else {
            if (OneOfKeysPressed({'y', 'Y'}, event)) {
                std::string& rmPath = fullPathEntries[selected];
                if (fs::is_directory(rmPath) && !fs::is_empty(rmPath)) {
                    msgOrWarning.SetWarning("Not empty folders deletion is not supported for now");
                    return true;
                }
                auto& cacher = Singleton<Cacher>::Instance().GetObj();
                size_t size = cacher.GetCachedSize(rmPath);

                if (!fs::remove(fullPathEntries[selected])) {
                    msgOrWarning.SetWarning("Failed to remove entry");
                    return true;
                }

                cacher.Update(path, cacher.GetCachedSize(path) - size);

                fullPathEntries.erase(fullPathEntries.begin() + selected);
                dirEntries.erase(dirEntries.begin() + selected);
                sizeEntries.erase(sizeEntries.begin() + selected);
                if (!fullPathEntries.empty()) {
                    selected = std::min<int>(selected,
                        static_cast<int>(fullPathEntries.size() - 1));
                }
                requestPreview();
                msgOrWarning.SetMsg("Successfully deleted entry");
                activeTab = TabIndex::Browser;
                return true;
            }

            if (OneOfKeysPressed({'n', 'N'}, event)) {
                activeTab = TabIndex::Browser;
                return true;
            }
            return false;
        }

        msgOrWarning.Reset();

        return false;
    });

    screen.Post([&] { updateEntries(); });
    screen.Loop(eventHandler);
    std::cout << previewRenderer.TakeCleanupCommand() << std::flush;
}
